// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/AZ_GA_MeleeAttack.h"
#include "AbilitySystem/AbilityTasks/AZ_AT_MeleeSweep.h"
#include "AbilitySystem/AbilityTasks/AZ_AT_PlayMontageAndWaitForEvent.h"
#include "AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h"
#include "AbilitySystem/GameplayEffects/AZ_GE_Damage.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/AZ_AnimNotify_SendGameplayEvent.h"   // BeatEnd notify scan
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
#include "Perception/AISense_Hearing.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Character/AZ_PawnMoverInfectedCharacter.h"
#include "Character/AZ_PawnMoverComponent.h"

UAZ_GA_MeleeAttack::UAZ_GA_MeleeAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	DamageEffect = UAZ_GE_Damage::StaticClass();
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

	UAnimMontage* Montage = SelectMontage();
	if (!Montage)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Warp target BEFORE the montage starts: a warp window opening on frame 0 resolves its target the
	// moment it becomes relevant, so registering after would race the first frame of the lunge.
	// Finding nothing is a legitimate outcome — the clip then plays its authored distance unwarped.
	if (bUseMotionWarping && !WarpTargetName.IsNone())
	{
		if (const AActor* Avatar = GetAvatarActorFromActorInfo())
		{
			if (UMotionWarpingComponent* Warping = Avatar->FindComponentByClass<UMotionWarpingComponent>())
			{
				if (const AActor* WarpTarget = FindWarpTarget())
				{
					if (USceneComponent* TargetRoot = WarpTarget->GetRootComponent())
					{
						// VectorFromTargetToOwner keeps the warp point on OUR side of them however they
						// turn; bFollowComponent re-reads it each frame so a target that backs off mid-
						// punch is still tracked. All six trailing args spelled out because the two
						// overloads differ only in the 5th and both default it (ambiguous otherwise).
						Warping->AddOrUpdateWarpTargetFromComponent(WarpTargetName, TargetRoot, NAME_None,
							/*bFollowComponent*/ true,
							EWarpTargetLocationOffsetDirection::VectorFromTargetToOwner,
							FVector(WarpApproachDistance, 0.f, 0.f), FRotator::ZeroRotator);
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

	FGameplayTagContainer EventTags;
	EventTags.AddTag(FAZ_GameplayTags::Get().Event_Montage_Melee_WindowBegin);
	EventTags.AddTag(FAZ_GameplayTags::Get().Event_Montage_Melee_WindowEnd);
	EventTags.AddTag(FAZ_GameplayTags::Get().Event_Combat_BeatEnd);

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
	if (const AActor* Avatar = GetAvatarActorFromActorInfo())   // pawn-class-agnostic: hero AND infected
	{
		if (UAZ_PawnMoverComponent* Mover = Avatar->FindComponentByClass<UAZ_PawnMoverComponent>())
		{
			// RootMotionSeconds 0 = the attack's beat (the BeatEnd notify when authored, else the whole
			// montage — in-place punches). A positive value cuts the capsule loose even earlier so a
			// lunge clip's travelling recovery tail cannot walk the attacker through the victim's
			// knockback — see the property comment for the measured numbers.
			const float RMDefault = (BeatSeconds > 0.f) ? BeatSeconds : Montage->GetPlayLength();
			RootMotionGen = Mover->DriveRootMotion((RootMotionSeconds > 0.f)
				? FMath::Min(RootMotionSeconds, Montage->GetPlayLength())
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
		return Candidate;
	}
	return nullptr;
}

UAnimMontage* UAZ_GA_MeleeAttack::SelectMontage() const
{
	if (bIsMovingLatched)
		return (Hand == EAZ_MeleeHand::Left) ? PunchMove_L : PunchMove_R;
	return (Hand == EAZ_MeleeHand::Left) ? PunchIdle_L : PunchIdle_R;
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
	}
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
	SweepTask = UAZ_AT_MeleeSweep::MeleeSweepWindow(this, { StrikeSocket_L, StrikeSocket_R }, SweepSphereRadius,
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
	UE_LOG(LogTemp, Display, TEXT("[MeleeHit] %s -> %s contact at %s (socket sweep)"),
		*GetName(), *GetNameSafe(Target), *Hit.ImpactPoint.ToCompactString());

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
