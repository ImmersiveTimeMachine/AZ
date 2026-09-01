// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/AZ_GA_MeleeAttack.h"
#include "AbilitySystem/AbilityTasks/AZ_AT_MeleeSweep.h"
#include "AbilitySystem/AbilityTasks/AZ_AT_PlayMontageAndWaitForEvent.h"
#include "AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h"
#include "AbilitySystem/GameplayEffects/AZ_GE_Damage.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"   // ApplyHitStop
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/AZ_AnimNotify_SendGameplayEvent.h"      // BeatEnd notify scan
#include "Animation/AZ_CombatMontage.h"
#include "Animation/AZ_InfectedAnimInstance.h"
#include "Animation/AZ_MoverAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "AZ_GameplayTags.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "DrawDebugHelpers.h"
#include "GenericTeamAgentInterface.h"
#include "MotionWarpingComponent.h"
#include "AnimNotifyState_MotionWarping.h"   // reading each warp window's own modifier off the notify
#include "RootMotionModifier.h"              // URootMotionModifier_Warp: WarpTargetName + bWarpTranslation
#include "Perception/AISense_Hearing.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Character/AZ_PawnMoverInfectedCharacter.h"
#include "Character/AZ_PawnMoverComponent.h"

UAZ_GA_MeleeAttack::UAZ_GA_MeleeAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	DamageEffect = UAZ_GE_Damage::StaticClass();
	// Same-hand chaining: without this the engine refuses to reactivate an active InstancedPerActor spec
	// outright, and CanActivateAbility's recovery gate never gets a say. Note this is NOT sufficient on
	// its own — the input rail only calls TryActivateAbility while the spec is INACTIVE, so today the
	// recovery gate governs cross-hand chains only. Same-hand chains arrive with the input buffer, which
	// must attempt activation on an active spec.
	bRetriggerInstancedAbility = true;
}

void UAZ_GA_MeleeAttack::DeclareAbilityTags()
{
	Super::DeclareAbilityTags();

	// Runs per INSTANCE (see UAZ_GameplayAbility::DeclareAbilityTags for why not the ctor or the CDO).
	// Adds, so a BP tuning child's own entries survive.
	const FAZ_GameplayTags& T = FAZ_GameplayTags::Get();
	ActivationBlockedTags.AddTag(T.State_Combat_Staggered);   // reeling — the reaction owns the body
	ActivationBlockedTags.AddTag(T.State_Grabbed);            // caught: the struggle mash is the only action
	ActivationBlockedTags.AddTag(T.State_Combat_Grabbing);    // holding someone: the grab is the attack
	ActivationBlockedTags.AddTag(T.Character_Dead);
	ActivationBlockedTags.AddTag(T.Character_Dying);

	// A punch cancels a punch — explicitly, rather than by letting the new montage displace the old one
	// on the slot and waiting for the loser's OnInterrupted. Ordering matters and works in our favour:
	// GAS applies this in PreActivate, so the outgoing swing's EndAbility (and its generation-scoped
	// ReleaseRootMotion) runs BEFORE the incoming one calls DriveRootMotion, and cannot cancel the new
	// drive. Which swing is ALLOWED to start is decided separately, by the recovery gate in
	// CanActivateAbility — this only makes the handover clean once permission is granted.
	CancelAbilitiesWithTag.AddTag(T.Ability_Combat_Melee);
}

USkeletalMeshComponent* UAZ_GA_MeleeAttack::GetAvatarMesh() const
{
	// Explicit casts, not FindComponentByClass — the hero carries extra skeletal meshes (equipment
	// proxies); each pawn's GetMesh() is the authoritative body.
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (const AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(Avatar))
	{
		return Hero->GetMesh();
	}
	if (const AAZ_PawnMoverInfectedCharacter* Infected = Cast<AAZ_PawnMoverInfectedCharacter>(Avatar))
	{
		return Infected->GetMesh();
	}
	return nullptr;
}

bool UAZ_GA_MeleeAttack::ResolveAvatarIsMoving() const
{
	const USkeletalMeshComponent* Mesh = GetAvatarMesh();
	const UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (const UAZ_MoverAnimInstance* HeroAnim = Cast<UAZ_MoverAnimInstance>(Anim))
	{
		return HeroAnim->ChooserContext.bIsMoving;   // hero: INTENT (trajectory), per v2 doctrine
	}
	if (const UAZ_InfectedAnimInstance* InfectedAnim = Cast<UAZ_InfectedAnimInstance>(Anim))
	{
		return InfectedAnim->IsMoving();             // infected: commanded-motion ground speed
	}
	return false;
}

bool UAZ_GA_MeleeAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	// CONTROLLED CHAINING. A swing may only be started (or restarted) while no other melee instance is
	// mid-commitment. Checked on EVERY attempt, and — verified in engine source — this runs on the
	// primary instance BEFORE the retrigger EndAbility block, so a refused mash does NOT end the swing
	// already in flight. Without it, L and R being separate specs means a second fist can displace the
	// first on the slot at frame 0, mid-warp and mid-strike.
	if (IsOtherMeleeCommitted(ActorInfo))
	{
		return false;
	}
	if (IsActive() && !bCancellable)
	{
		return false;   // same spec, still committed — mashing does nothing until recovery
	}

	return true; //ToDo: HasAmmo();
}

void UAZ_GA_MeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	if (!HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Data-driven GEs on activation (e.g. GE_CombatReady -> refresh the fists-up stance every punch). The GE owns
	// its duration + refresh-on-reapply stacking; ApplyGameplayEffectSpecToOwner routes prediction/authority.
	for (const TSubclassOf<UGameplayEffect>& EffectClass : EffectsOnActivate)
	{
		if (!*EffectClass) continue;
		const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(EffectClass, GetAbilityLevel());
		if (Spec.IsValid()) ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	}

	bIsMovingLatched = ResolveAvatarIsMoving();

	// Resolve the lunge candidate BEFORE picking the clip: the gap is now a selection input, not just a
	// warp input. Latched so selection and warp registration cannot disagree about which target this
	// swing is for (a second sweep could return a different actor a frame later).
	//
	// Skipped entirely when neither consumer exists — the Chalkie warps from its BT task and has no lunge
	// clips, so it would otherwise run a forward sweep per swing to produce a value nothing reads.
	WarpTargetLatched = nullptr;
	TargetGapLatched = 0.f;
	if (bUseMotionWarping || PunchLunge_L || PunchLunge_R)
	{
		WarpTargetLatched = FindWarpTarget();
		if (const AActor* Avatar = GetAvatarActorFromActorInfo())
		{
			if (const AActor* Latched = WarpTargetLatched.Get())
			{
				TargetGapLatched = FVector::Dist2D(Latched->GetActorLocation(), Avatar->GetActorLocation());
			}
		}
	}

	UAnimMontage* Montage = SelectMontage();
	if (!Montage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Does this clip TRAVEL? Read once here — the warp clamp below needs it (an in-place jab may only
	// correct its distance) and the root-motion drive further down needs it (driving a clip that goes
	// nowhere pins the pawn). The flag says nothing; the measured displacement does — the jabs move
	// 0.0-0.3cm, the moving punches 70cm, the heavy 202cm.
	const FTransform ClipRootMotion = Montage->ExtractRootMotionFromTrackRange(
		0.f, Montage->GetPlayLength(), FAnimExtractContext());
	const bool bClipTravels = ClipRootMotion.GetTranslation().Size2D() >= MinRootMotionTravel;

	// Warp target BEFORE the montage starts: a warp window opening on frame 0 resolves its target the
	// moment it becomes relevant, so registering after would race the first frame of the lunge.
	// Finding nothing is a legitimate outcome — the clip then plays its authored distance unwarped.
	if (bUseMotionWarping && !WarpTargetName.IsNone())
	{
		if (const AActor* Avatar = GetAvatarActorFromActorInfo())
		{
			if (UMotionWarpingComponent* Warping = Avatar->FindComponentByClass<UMotionWarpingComponent>())
			{
				if (const AActor* WarpTarget = WarpTargetLatched.Get())
				{
					if (USceneComponent* TargetRoot = WarpTarget->GetRootComponent())
					{
						// ★ CLAMPED OFFSET — and it is a MIN, not a max.
						//
						// LocationOffset.X is the DESTINATION GAP, not a distance to travel: the engine
						// builds the point as Target + normalize(Owner - Target) * X
						// (RootMotionModifier.cpp:194-196), so it lands X cm from the TARGET on our side.
						// The attacker therefore moves from its current gap TO X. Larger X = further from
						// the target = backwards.
						//
						// Max() was wrong in both directions and shipped that way: at gap 200 with
						// stand-off 100 it asked for 200 — the hero's own position — so the heavy punch's
						// 202cm of travel scaled to nothing and the lunge silently never happened; at gap
						// 60 it asked for 100, i.e. 40cm BEHIND the attacker, the moonwalk it was meant to
						// prevent. Min approaches when there is room (200 -> 100) and holds position when
						// there is not (60 -> 60), which is the whole intent: never retreat to reach a
						// stand-off we are already inside.
						//
						// ★ BOUNDED BACK-STEP (2026-09-01): "never retreat" left the everyday case unfixed —
						// a standing jab from 60cm buries the knuckle 18cm past the victim's centre, and no
						// stand-off value changes that because the clamp holds at 60. So: retreat IS
						// allowed, but only by MaxBackstepDistance (60 -> 75 with 15), which is what makes
						// it a weight-shift instead of the unbounded moonwalk. With the property at 0 this
						// is exactly the old Min(). And an IN-PLACE clip may only CORRECT its distance
						// (±MaxInPlaceApproachDistance): SkewWarp's lerp branch has no speed clamp, so an
						// unbounded approach would turn a standing jab at 250cm into a 120cm dash.
						float ClampedApproach = FMath::Min(WarpApproachDistance, TargetGapLatched + MaxBackstepDistance);
						if (!bClipTravels)
						{
							ClampedApproach = FMath::Max(ClampedApproach, TargetGapLatched - MaxInPlaceApproachDistance);
						}
						UE_LOG(LogTemp, Display, TEXT("[MeleeWarp] %s clip=%s gap=%.0f standoff=%.0f -> dest=%.0f (%s %.0fcm, inPlace=%d)"),
							*GetNameSafe(Avatar), *Montage->GetName(), TargetGapLatched, WarpApproachDistance, ClampedApproach,
							(ClampedApproach > TargetGapLatched) ? TEXT("back") : TEXT("fwd"),
							FMath::Abs(ClampedApproach - TargetGapLatched), !bClipTravels);
						Warping->AddOrUpdateWarpTargetFromComponent(WarpTargetName, TargetRoot, NAME_None,
							/*bFollowComponent*/ true,
							EWarpTargetLocationOffsetDirection::VectorFromTargetToOwner,
							FVector(ClampedApproach, 0.f, 0.f), FRotator::ZeroRotator);
					}
				}
				else
				{
					// Nothing to lunge at — clear any stale target from a previous swing so this punch
					// cannot warp toward whoever the LAST one was aimed at.
					Warping->RemoveWarpTarget(WarpTargetName);
				}
			}
		}
	}

	// BEAT CLOCK (arch step A, "events drive, timers guard"): if the montage carries an authored
	// Event.Combat.BeatEnd notify, THAT ends this attack — the animation timeline is the clock, so rate
	// scale and interrupts propagate for free. This is how the zombie bite ends now (notify at the bite
	// beat on the 8-10s clawing cycles — replaces GA_ZombieMelee's timer + generation-map hack). Hero
	// punch clips carry no notify → BeatSeconds = clip length → behaviour unchanged.
	const float BeatSeconds = FindBeatEndNotifyTime(Montage);

	// Retrigger reuses this instance, so every per-swing latch resets HERE, not just on end.
	//
	// ★ COMMITTED UNTIL THE CLIP SAYS OTHERWISE. This started out inverted — "no authored window means
	// cancellable throughout" — on the reasoning that missing data should never make a swing MORE
	// committal. That was right for the chain gate and catastrophic once movement could cancel: an attack
	// thrown while a movement key is HELD published its window on frame 0 and was cancelled on the next
	// tick, before a single frame drew. Punching while moving became impossible, which also made
	// AM_Fists_Punch_Move_L/R — clips whose entire purpose is punching on the move — unplayable.
	//
	// So the baseline is commitment, and an authored CancelOpen is what BUYS the freedom. Missing data
	// now costs a swing its cancel tail, which is a small, visible loss — not a swing that never plays.
	bCancellable = false;
	SetCancelWindowTag(false);
	ActiveMontage = Montage;

	// ★ Every tag the clip can send must be in THIS container: the montage task subscribes with it, and
	// anything unlisted is silently dropped — the notify fires, nothing receives it, and the feature
	// looks unimplemented. Adding a notify without adding its tag here is the single easiest way to lose
	// an afternoon on this rail.
	FGameplayTagContainer EventTags;
	EventTags.AddTag(FAZ_GameplayTags::Get().Event_Montage_Melee_WindowBegin);
	EventTags.AddTag(FAZ_GameplayTags::Get().Event_Montage_Melee_WindowEnd);
	EventTags.AddTag(FAZ_GameplayTags::Get().Event_Combat_BeatEnd);
	EventTags.AddTag(FAZ_GameplayTags::Get().Event_Combat_CancelOpen);
	EventTags.AddTag(FAZ_GameplayTags::Get().Event_Combat_CancelClose);

	MontageTask = UAZ_AT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
		this, FName("MeleeMontage"), Montage, EventTags,
		/*Rate*/ 1.f, /*StartSection*/ NAME_None, /*bStopWhenAbilityEnds*/ true,
		/*AnimRootMotionTranslationScale*/ 1.f);

	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UAZ_GA_MeleeAttack::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &UAZ_GA_MeleeAttack::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UAZ_GA_MeleeAttack::OnMontageFinished);
	MontageTask->EventReceived.AddDynamic(this, &UAZ_GA_MeleeAttack::OnMontageEvent);
	MontageTask->ReadyForActivation();   // C++ must call this manually (see the task's header comment)

	// Stage 2: drive the capsule with the punch montage's root motion. The FullBody slot overrides
	// the pose, so RootMotionFromEverything extracts the MONTAGE delta into the attribute; a live
	// FLayeredMove_RootMotionAttribute makes the Mover follow it (OverrideAll). Mirrors the
	// transition-clip bridge in AZ_MoverAnimInstance.cpp:112-116. Scoped to the montage length;
	// cancelled in EndAbility so an interrupted punch doesn't overrun the capsule.
	// ★ ONLY CLIPS THAT ACTUALLY TRAVEL GET A DRIVE. Every fist clip in the kit sets bEnableRootMotion, so
	// the flag says nothing; the measured displacement does — the jabs move 0.0-0.3cm. Driving one queues
	// an OverrideAll layered move that transports nothing for the clip's whole length, and OverrideAll
	// applying a ~zero delta PINS the pawn. That is felt as "I still have to wait after punching", for no
	// transport in return.
	// (ClipRootMotion / bClipTravels are read once, right after SelectMontage — the warp clamp uses them too.)

	// ...BUT "does the clip travel" is the wrong question on its own, because WARPING SYNTHESISES TRAVEL
	// A CLIP DOES NOT CONTAIN. The Chalkie's claws are in-place by design and get their entire approach
	// from SkewWarp's add-translation branch — and that branch lives inside
	// FLayeredMove_RootMotionAttribute::GenerateMove, so with no live layered move the warp modifier never
	// runs at all. Gating on travel alone therefore didn't just cancel the lunge: it silently disabled the
	// stand-off, and a Chalkie that stops at point-blank via nav swings an 87cm claw through a 30cm
	// capsule. That is the "the claw goes inside the hero" report.
	//
	// The name is read off the NOTIFY's own modifier rather than this ability's WarpTargetName, so the
	// hero ("MeleeTarget") and the BT-driven Chalkie ("AttackTarget") both work with no per-class config,
	// and a notify/registration name mismatch stays inert instead of silently driving.
	//
	// TRANSLATION WINDOWS ONLY. A rotation-only window would also need the layered move, but driving one
	// re-queues an OverrideAll move with a ~zero delta — the documented pin that cost the hero 0.8s per
	// jab. Requiring translation keeps that fix intact; a rooted-but-rotating swing stays un-driven.
	float WarpWindowEnd = 0.f;
	bool bLiveWarpWindow = false;
	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		if (const UMotionWarpingComponent* Warping = Avatar->FindComponentByClass<UMotionWarpingComponent>())
		{
			TArray<FMotionWarpingWindowData> Windows;
			UMotionWarpingUtilities::GetMotionWarpingWindowsFromAnimation(Montage, Windows);
			for (const FMotionWarpingWindowData& Window : Windows)
			{
				const URootMotionModifier_Warp* Modifier = Window.AnimNotify
					? Cast<URootMotionModifier_Warp>(Window.AnimNotify->RootMotionModifier) : nullptr;
				if (Modifier && Modifier->bWarpTranslation && Warping->FindWarpTarget(Modifier->WarpTargetName))
				{
					bLiveWarpWindow = true;
					WarpWindowEnd = FMath::Max(WarpWindowEnd, Window.EndTime);
				}
			}
		}
	}

	const bool bDrive = bClipTravels || bLiveWarpWindow;
	UE_LOG(LogTemp, Display,
		TEXT("[MeleeRM] %s clip=%s travel=%.1fcm warpWindow=%s drive=%s"),
		*GetNameSafe(GetAvatarActorFromActorInfo()), *GetNameSafe(Montage),
		ClipRootMotion.GetTranslation().Size2D(),
		bLiveWarpWindow ? *FString::Printf(TEXT("live->%.2fs"), WarpWindowEnd) : TEXT("none"),
		bDrive ? TEXT("YES") : TEXT("no"));

	if (const AActor* Avatar = bDrive ? GetAvatarActorFromActorInfo() : nullptr)   // hero AND infected
	{
		if (UAZ_PawnMoverComponent* Mover = Avatar->FindComponentByClass<UAZ_PawnMoverComponent>())
		{
			// RootMotionSeconds 0 = the attack's beat (the BeatEnd notify when authored, else the whole
			// montage — in-place punches). A positive value cuts the capsule loose even earlier so a
			// lunge clip's travelling recovery tail cannot walk the attacker through the victim's
			// knockback — see the property comment for the measured numbers.
			//
			// The drive must also outlast the WARP WINDOW, or a beat that lands before the window closes
			// truncates the approach the warp was in the middle of synthesising. Never extends past the
			// window's end: beyond it the modifier is inactive and an in-place clip would just pin.
			float RMDefault = (BeatSeconds > 0.f) ? BeatSeconds : Montage->GetPlayLength();
			RMDefault = FMath::Max(RMDefault, WarpWindowEnd);
			RootMotionGen = Mover->DriveRootMotion((RootMotionSeconds > 0.f)
				? FMath::Max(FMath::Min(RootMotionSeconds, Montage->GetPlayLength()), WarpWindowEnd)
				: RMDefault);
		}
	}

	// WATCHDOG for the notify-driven beat: if the BeatEnd event never arrives (montage killed the same
	// frame it would fire), end anyway — a zombie must not claw for the full 8-10s cycle on a missed
	// notify. Guard, never the mechanism.
	if (BeatSeconds > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(BeatWatchdog,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (IsActive())
				{
					EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
				}
			}), BeatSeconds + 0.5f, false);
	}
}

void UAZ_GA_MeleeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BeatWatchdog);
	}
	// An interrupted swing must not leave a hit detector ticking past the ability.
	StopHitWindow();
	// Nor a recovery latch outliving the swing it belonged to — the instance is reused under retrigger,
	// and the ASC tag would otherwise advertise a cancel window on a pawn that is no longer attacking.
	bCancellable = false;
	SetCancelWindowTag(false);
	// Don't let the next swing's selection read this swing's gap.
	WarpTargetLatched = nullptr;
	TargetGapLatched = 0.f;

	// Stop driving the capsule with the punch RM. No-op if none was queued; on an interrupted
	// punch this cancels the layered move so it doesn't overrun its DurationMs. Self-heals for
	// locomotion (the AnimInstance re-queues transition RM moves each frame as needed).
	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		if (UAZ_PawnMoverComponent* Mover = Avatar->FindComponentByClass<UAZ_PawnMoverComponent>())
		{
			// Generation-scoped: only cancels if OUR drive is still the live one. A raw
			// CancelFeaturesWithTag here killed whoever's move was live — including a flinch that had
			// already taken over when this end fired late (correct before only by call-ordering luck).
			Mover->ReleaseRootMotion(RootMotionGen);
			RootMotionGen = 0;
		}
		// Drop the warp target on EVERY exit, cancelled or not. A stale one would silently steer the next
		// punch toward this swing's victim.
		if (!WarpTargetName.IsNone())
		{
			if (UMotionWarpingComponent* Warping = Avatar->FindComponentByClass<UMotionWarpingComponent>())
			{
				Warping->RemoveWarpTarget(WarpTargetName);
			}
		}
	}

	// (later) remove Ability.State.MeleeAttacking loose tag here.
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAZ_GA_MeleeAttack::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

void UAZ_GA_MeleeAttack::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
}

AActor* UAZ_GA_MeleeAttack::FindWarpTarget() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || WarpSearchDistance <= 0.f)
	{
		return nullptr;
	}

	// Same shape as the hit window's sweep (OnMontageEvent) so the thing we lunge at is the thing the
	// damage sweep would hit — just reaching further, because closing the gap is the point.
	const FVector Forward = Avatar->GetActorForwardVector().GetSafeNormal2D();
	const FVector Start = Avatar->GetActorLocation();
	const FVector End = Start + Forward * WarpSearchDistance;

	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AZMeleeWarpSearch), /*bTraceComplex*/ false, Avatar);
	Avatar->GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(MeleeRadius), Params);

	// SweepMulti returns hits ordered by distance, so the first survivor is the nearest.
	for (const FHitResult& Hit : Hits)
	{
		AActor* Candidate = Hit.GetActor();
		if (!Candidate || Candidate == Avatar)
		{
			continue;
		}
		// Must be genuinely in front — same cos(55 deg) cone the damage filter uses. Without this we
		// would lunge at something beside us that the punch could never reach.
		const FVector DirTo = (Candidate->GetActorLocation() - Avatar->GetActorLocation()).GetSafeNormal2D();
		if (FVector::DotProduct(Forward, DirTo) < 0.574f)
		{
			continue;
		}
		if (FGenericTeamId::GetAttitude(Avatar, Candidate) != ETeamAttitude::Hostile)
		{
			continue;
		}
		// Don't lunge at corpses (mirrors the hit window's rule): permanent corpses keep a hostile team
		// and a live ASC, so without a vitals check the punch would commit to a body on the floor.
		UAbilitySystemComponent* TargetASC =
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Candidate);
		if (!TargetASC)
		{
			continue;
		}
		bool bHasVitals = false;
		const float TargetHealth =
			TargetASC->GetGameplayAttributeValue(UAZ_VitalsAttributeSet::GetHealthAttribute(), bHasVitals);
		if (bHasVitals && TargetHealth <= 0.f)
		{
			continue;
		}
		// Close range is handled by CLAMPING the warp offset at the registration site, not by rejecting
		// the target here — see ActivateAbility. Rejecting would leave a travelling clip to play its
		// full authored distance (the heavy punch's 202cm) into a blocking capsule, and would also throw
		// away the ROTATION warp, which is still wanted at point-blank.
		return Candidate;
	}
	return nullptr;
}

UAnimMontage* UAZ_GA_MeleeAttack::SelectMontage() const
{
	const bool bLeft = (Hand == EAZ_MeleeHand::Left);

	// DISTANCE FIRST. Warping is meant to CORRECT an attack's authored travel, not invent it: picking a
	// 202cm lunge against a target 90cm away asked SkewWarp to scale that away to nothing and spent 1.70s
	// of committed root motion pressed against a blocking capsule. Choose the clip whose reach matches the
	// gap, and the warp is left trimming a residual — which also means close-quarters combat runs at the
	// jab's 0.15s contact instead of the lunge's 1.65s.
	//
	// Requires a real candidate: with nothing in front there is nothing to close on, so an unset gap must
	// never select a lunge. Unset PunchLunge slots disable the axis entirely (pre-existing behaviour).
	if (WarpTargetLatched.IsValid() && TargetGapLatched >= LungeMinDistance)
	{
		if (UAnimMontage* Lunge = bLeft ? PunchLunge_L : PunchLunge_R)
		{
			return Lunge;
		}
	}
	if (bIsMovingLatched)
	{
		return bLeft ? PunchMove_L : PunchMove_R;
	}
	return bLeft ? PunchIdle_L : PunchIdle_R;
}

float UAZ_GA_MeleeAttack::ReadConfigFloat(const UObject* Object, FName PropertyName, float Default)
{
	if (Object)
	{
		if (const FProperty* Property = Object->GetClass()->FindPropertyByName(PropertyName))
		{
			if (const FFloatProperty* AsFloat = CastField<FFloatProperty>(Property))
			{
				return AsFloat->GetPropertyValue_InContainer(Object);
			}
			if (const FDoubleProperty* AsDouble = CastField<FDoubleProperty>(Property))
			{
				return static_cast<float>(AsDouble->GetPropertyValue_InContainer(Object));
			}
		}
	}
	return Default;
}

UAnimMontage* UAZ_GA_MeleeAttack::FindAnimSetMontage(const AActor* Avatar, FName MontageProperty)
{
	if (!Avatar)
	{
		return nullptr;
	}
	const FObjectProperty* SetProperty = CastField<FObjectProperty>(Avatar->GetClass()->FindPropertyByName(TEXT("AnimSet")));
	const UObject* AnimSet = SetProperty ? SetProperty->GetObjectPropertyValue_InContainer(Avatar) : nullptr;
	if (!AnimSet)
	{
		return nullptr;
	}
	const FObjectProperty* Field = CastField<FObjectProperty>(AnimSet->GetClass()->FindPropertyByName(MontageProperty));
	return Field ? Cast<UAnimMontage>(Field->GetObjectPropertyValue_InContainer(AnimSet)) : nullptr;
}

bool UAZ_GA_MeleeAttack::FindAnimSetCombatMontage(const AActor* Avatar, FName StructPropertyName, FAZ_CombatMontage& Out)
{
	Out = FAZ_CombatMontage();
	if (!Avatar)
	{
		return false;
	}
	const FObjectProperty* SetProperty = CastField<FObjectProperty>(Avatar->GetClass()->FindPropertyByName(TEXT("AnimSet")));
	const UObject* AnimSet = SetProperty ? SetProperty->GetObjectPropertyValue_InContainer(Avatar) : nullptr;
	if (!AnimSet)
	{
		return false;
	}
	// Struct-typed BP variable on the anim set (e.g. "HitReact"). The exact-struct check matters: a BP
	// variable with the right NAME but a different type must read as "not authored", not garbage.
	const FStructProperty* Field = CastField<FStructProperty>(AnimSet->GetClass()->FindPropertyByName(StructPropertyName));
	if (!Field || Field->Struct != FAZ_CombatMontage::StaticStruct())
	{
		return false;
	}
	Out = *Field->ContainerPtrToValuePtr<FAZ_CombatMontage>(AnimSet);
	// An authored row with no montage assigned is still "not set" — callers fall back to the legacy field.
	return Out.IsSet();
}

float UAZ_GA_MeleeAttack::FindBeatEndNotifyTime(const UAnimMontage* Montage)
{
	if (!Montage)
	{
		return 0.f;
	}
	const FGameplayTag& BeatEnd = FAZ_GameplayTags::Get().Event_Combat_BeatEnd;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		const UAZ_AnimNotify_SendGameplayEvent* Sender = Cast<UAZ_AnimNotify_SendGameplayEvent>(Event.Notify);
		if (Sender && Sender->EventTag == BeatEnd)
		{
			return Event.GetTriggerTime();
		}
	}
	return 0.f;
}

void UAZ_GA_MeleeAttack::OnMontageFinished(FGameplayTag EventTag, FGameplayEventData EventData)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAZ_GA_MeleeAttack::OnMontageEvent(FGameplayTag EventTag, FGameplayEventData EventData)
{
	// WHOSE MONTAGE SPOKE? These events are broadcast to the ASC and the task subscribes by tag alone,
	// so a swing that was just cancelled still delivers its states' NotifyEnd — a frame or more after the
	// replacement swing latched its own. Discard anything stamped with a different montage. Events with
	// no stamp (the single-frame BeatEnd notify) are always accepted, since only window states stamp.
	if (const UAnimSequenceBase* Source = Cast<UAnimSequenceBase>(EventData.OptionalObject.Get()))
	{
		if (ActiveMontage.IsValid() && Source != ActiveMontage.Get())
		{
			return;
		}
	}

	// THE BITE END: the montage's authored BeatEnd notify says this attack's beat is over — end the
	// ability (the task stops the montage with its blend). This is the event-driven replacement for
	// GA_ZombieMelee's BiteSeconds timer; hero clips carry no BeatEnd and never reach this branch.
	if (EventTag == FAZ_GameplayTags::Get().Event_Combat_BeatEnd)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// THE HIT WINDOW (socket-swept): the montage's WindowBegin/WindowEnd notifies bound the strike
	// phase; between them UAZ_AT_MeleeSweep traces the fist socket every tick, and the frame the fist
	// actually touches a pawn is the hit. Contact timing is physics now — the old single-frame notify
	// (a per-clip GUESS that was wrong twice on one clip) and its cone/range/radius patches are gone.
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	if (EventTag == Tags.Event_Montage_Melee_WindowBegin)
	{
		StartHitWindow();
		return;
	}
	if (EventTag == Tags.Event_Montage_Melee_WindowEnd)
	{
		StopHitWindow();
		return;
	}

	// RECOVERY. Startup and the strike are committed; from here the swing may be abandoned. Published
	// as a tag so the input layer (buffered press) and the movement layer (cancel on move intent) can
	// both act on it without either of them knowing this ability exists.
	if (EventTag == Tags.Event_Combat_CancelOpen)
	{
		bCancellable = true;
		SetCancelWindowTag(true);
		return;
	}
	if (EventTag == Tags.Event_Combat_CancelClose)
	{
		bCancellable = false;
		SetCancelWindowTag(false);
	}
}

void UAZ_GA_MeleeAttack::SetCancelWindowTag(bool bOpen)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->SetLooseGameplayTagCount(FAZ_GameplayTags::Get().State_Combat_CancelWindow, bOpen ? 1 : 0);
	}
}

bool UAZ_GA_MeleeAttack::IsOtherMeleeCommitted(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		return false;
	}
	const FGameplayTag& MeleeIdentity = FAZ_GameplayTags::Get().Ability_Combat_Melee;
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.IsActive() || !Spec.Ability || !Spec.Ability->GetAssetTags().HasTagExact(MeleeIdentity))
		{
			continue;
		}
		for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
		{
			const UAZ_GA_MeleeAttack* Melee = Cast<UAZ_GA_MeleeAttack>(Instance);
			if (Melee && Melee != this && Melee->IsActive() && !Melee->bCancellable)
			{
				return true;   // someone else is mid-commitment — refuse
			}
		}
	}
	return false;
}

void UAZ_GA_MeleeAttack::StartHitWindow()
{
	// Server-authoritative: the predicted client plays the montage but only the authority detects and
	// deals damage (results replicate back via attributes/tags).
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority() || SweepTask)
	{
		return;
	}
	// BOTH fists, not the one Hand picked. Hand still selects the MONTAGE; it does not get to claim which
	// hand the animator actually swings — measured, the clip names lie (AM_Zombie_Atk_L rakes with the
	// RIGHT claw while its left hand is behind the body; the heavy punch's real strike is a right hook).
	// Sweeping both and letting geometry decide leaves the animation as the single owner of that fact.
	// LATENESS INSTRUMENTATION ([MeleeWin], pairs with the swingT on [MeleeHit]): a window that OPENS
	// with no [MeleeHit] before the swing's end is a WHIFF — and a whiffed swing followed by a connecting
	// one is the main way "the reaction feels 1-2s late" happens without any code being late: the hit the
	// player attributes to swing #1 actually landed on swing #2. The gap says whether the whiff was
	// range (gap > ~130 reach) or timing.
	{
		const UAnimInstance* AnimForLog = GetAvatarMesh() ? GetAvatarMesh()->GetAnimInstance() : nullptr;
		const float SwingT = (AnimForLog && ActiveMontage.IsValid())
			? AnimForLog->Montage_GetPosition(ActiveMontage.Get()) : -1.f;
		const AActor* LatchedTarget = WarpTargetLatched.Get();
		const float Gap = LatchedTarget ? FVector::Dist2D(LatchedTarget->GetActorLocation(), Avatar->GetActorLocation()) : -1.f;
		UE_LOG(LogTemp, Display, TEXT("[MeleeWin] %s OPEN swingT=%.2f clip=%s gap=%.0fcm"),
			*GetNameSafe(Avatar), SwingT, *GetNameSafe(ActiveMontage.Get()), Gap);
	}
	SweepTask = UAZ_AT_MeleeSweep::MeleeSweepWindow(this,
		{ StrikeSocket_L, StrikeSocket_R, KnuckleSocket_L, KnuckleSocket_R }, SweepSphereRadius,
		/*bHostilesOnly*/ true, /*bSingleTarget*/ true);
	if (SweepTask)
	{
		SweepTask->OnHit.AddDynamic(this, &UAZ_GA_MeleeAttack::OnSweepHit);
		SweepTask->ReadyForActivation();
	}
}

void UAZ_GA_MeleeAttack::StopHitWindow()
{
	if (SweepTask)
	{
		SweepTask->EndTask();
		SweepTask = nullptr;
	}
}

void UAZ_GA_MeleeAttack::OnSweepHit(const FHitResult& Hit)
{
	// The task already filtered self / team / corpses / repeats — everything here is a legitimate,
	// physically-touched victim. Apply damage + fight noise.
	AActor* Avatar = GetAvatarActorFromActorInfo();
	AActor* Target = Hit.GetActor();
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!Avatar || !Target || !TargetASC || !DamageEffect)
	{
		return;
	}

	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(DamageEffect, GetAbilityLevel());
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(FAZ_GameplayTags::Get().SetByCaller_Damage, DamageAmount);
	Spec.Data->GetContext().AddHitResult(Hit);   // death/hit-react abilities read direction from this
	if (UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo())
	{
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}
	// swingT: where in the attacker's clip contact registered. Window opens at 0.85 on the claw clips —
	// a swingT near the open means detection is honest and any perceived lateness is the authored
	// wind-up; a swingT pinned late in the window means the sweep is finding the target late.
	const UAnimInstance* AnimForLog = GetAvatarMesh() ? GetAvatarMesh()->GetAnimInstance() : nullptr;
	const float SwingT = (AnimForLog && ActiveMontage.IsValid())
		? AnimForLog->Montage_GetPosition(ActiveMontage.Get()) : -1.f;
	UE_LOG(LogTemp, Display, TEXT("[MeleeHit] %s -> %s contact at %s swingT=%.2f (socket sweep)"),
		*GetName(), *GetNameSafe(Target), *Hit.ImpactPoint.ToCompactString(), SwingT);

	// HIT-STOP, both sides. This is the frame-of-contact acknowledgment the fight was missing: the
	// knockback clips peak at 1.34-1.81s and open at 8-14cm/s, so for ~300ms after a landed hit the
	// victim is visually inert while the follow-through drives through them — which reads as fake far
	// more strongly than the ~18cm of capsule overlap does (and that overlap is roughly what shipped
	// melee authors anyway, because the capsule is not the body).
	//
	// Victim goes LAST and slightly longer: the damage GE above has already run, so the reaction montage
	// is up and has had this frame to start — freezing it now hangs the body in a recoil pose instead of
	// its pre-hit pose. Both are refresh-not-stack, so a pack landing together cannot multiply the hold.
	// TRANSPORT IS DONE — the drive existed to close distance, and the distance is closed. Everything
	// still authored in the clip's travel from here would carry the attacker INTO the victim. Only the
	// capsule stops: the montage plays on, the strike window stays open, and recovery still begins at its
	// authored boundary. Generation-scoped, so this cannot cancel a knockback that has already taken the
	// bridge; zeroing the handle keeps EndAbility's release a no-op rather than a second cancel.
	if (bReleaseRootMotionOnHit && RootMotionGen != 0)
	{
		if (UAZ_PawnMoverComponent* Mover = Avatar->FindComponentByClass<UAZ_PawnMoverComponent>())
		{
			Mover->ReleaseRootMotion(RootMotionGen);
			RootMotionGen = 0;
		}
	}

	if (UAZ_AbilitySystemComponent* SelfASC = Cast<UAZ_AbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		SelfASC->ApplyHitStop(HitStopAttackerSeconds);
	}
	if (UAZ_AbilitySystemComponent* VictimASC = Cast<UAZ_AbilitySystemComponent>(TargetASC))
	{
		VictimASC->ApplyHitStop(HitStopVictimSeconds);
	}

	// FIGHT NOISE: a landed hit is loud — pack hearing (direction-agnostic) pulls nearby Chalkies into
	// a wary investigation of the brawl, and every hit re-pins its CURRENT location. Instigator must be
	// the hero-side party (attacker when the hero punches, victim when a zombie claws) — zombie hearing
	// filters out friendly sources.
	AActor* NoiseInstigator = Cast<AAZ_PawnMoverInfectedCharacter>(Avatar) ? Target : Avatar;
	// Loudness MULTIPLIES the listener's HearingRange: tunable per-ATTACK (BP vars on the ability — a
	// knife stays quiet, a bat is loud; per-weapon noise = weapon parity data).
	const float ImpactLoudness = ReadConfigFloat(this, TEXT("ImpactNoiseLoudness"), 1.4f);
	const float ImpactMaxRange = ReadConfigFloat(this, TEXT("ImpactNoiseMaxRange"), 1000.f);
	UAISense_Hearing::ReportNoiseEvent(Avatar->GetWorld(), Target->GetActorLocation(),
		ImpactLoudness, NoiseInstigator, ImpactMaxRange, FName("Combat"));
	// TEMP noise debug (remove with [ChalkieDiag]): sphere = the engine's ACTUAL carry for a
	// 700-HearingRange listener (AISense_Hearing.cpp:147-152).
	const float CarryRadius = FMath::Min(700.f, ImpactMaxRange) * FMath::Max(0.f, ImpactLoudness);
	DrawDebugSphere(Avatar->GetWorld(), Target->GetActorLocation(), CarryRadius, 24, FColor::Yellow, false, 2.f);
}
