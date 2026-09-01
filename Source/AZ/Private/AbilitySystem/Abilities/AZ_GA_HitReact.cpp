// Copyright Artur. AZ project.

#include "AbilitySystem/Abilities/AZ_GA_HitReact.h"

#include "AbilitySystem/Abilities/AZ_GA_MeleeAttack.h"   // FindAnimSetCombatMontage / FindAnimSetMontage / ReadConfigFloat / FindBeatEndNotifyTime
#include "AbilitySystemComponent.h"   // SetLooseGameplayTagCount (the stagger gate is applied explicitly)
#include "AbilitySystemGlobals.h"     // reading the GRANTED reaction class's escape array off its CDO
#include "AbilitySystem/AbilityTasks/AZ_AT_PlayMontageAndWaitForEvent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AZ_GameplayTags.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

// Pose-selected reaction (TrySelectReactionByPose ONLY — Experimental API quarantine)
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchLibrary.h"

UAZ_GA_HitReact::UAZ_GA_HitReact()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// Reactions originate on the authority (vitals + horde both run server-side); montage replicates
	// through the ASC via the task, same as GA_Death.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	// A second punch mid-stagger RESTARTS the reaction (EndAbility + fresh activation) instead of being
	// swallowed. Stun-lock on one Chalkie is accepted; difficulty comes from the pack (fight rulebook).
	bRetriggerInstancedAbility = true;
}

void UAZ_GA_HitReact::ConfigureOnCDO(UClass* GrantClass)
{
	// Native tags register after CDO ctors, so triggers/tags cannot be set in the constructor — this is
	// the GA_Death grant-time pattern. Each patch is guarded by its SPECIFIC tag (audit #10): a future
	// addition here must not be silently skipped on a CDO an earlier session already patched.
	UClass* TargetClass = GrantClass ? GrantClass : UAZ_GA_HitReact::StaticClass();
	UAZ_GA_HitReact* CDO = Cast<UAZ_GA_HitReact>(TargetClass->GetDefaultObject());
	if (!CDO)
	{
		return;
	}
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();

	for (const FGameplayTag& TriggerTag : { Tags.Event_Combat_HitReact, Tags.Event_Combat_StepBack, Tags.Event_Combat_GrabShoved })
	{
		if (!CDO->AbilityTriggers.ContainsByPredicate(
			[&TriggerTag](const FAbilityTriggerData& T) { return T.TriggerTag == TriggerTag; }))
		{
			FAbilityTriggerData Trigger;
			Trigger.TriggerTag = TriggerTag;
			Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
			CDO->AbilityTriggers.Add(Trigger);
		}
	}

	// Kept so the tag is visible on the CDO (editor inspection, and anything evaluated against
	// Spec.Ability sees it) — but do NOT rely on it to actually apply the tag. PreActivate reads
	// ActivationOwnedTags off the ABILITY INSTANCE, and an instance does not inherit a CDO patched at
	// runtime: measured, CDO = {State.Combat.Staggered} while a fresh instance = {}. The tag therefore
	// never reached the ASC, no BT gate ever saw it, and the Chalkie swung again 0.5s into its own
	// knockback. ApplyStaggerTag/ClearStaggerTag below are the real owners.
	// (ActivationBlockedTags does NOT escape this either: InternalTryActivateAbility picks
	// `AbilitySource = InstancedAbility ? InstancedAbility : Ability`, so once a primary instance exists
	// — always, for InstancedPerActor — CanActivateAbility runs on the INSTANCE too. Both containers are
	// declared per-instance in DeclareAbilityTags; only AbilityTriggers and asset tags are CDO-read.)
	if (!CDO->ActivationOwnedTags.HasTagExact(Tags.State_Combat_Staggered))
	{
		CDO->ActivationOwnedTags.AddTag(Tags.State_Combat_Staggered);
	}
	// GRAB ARMOR (rule 8): a mid-grab Chalkie plays no flinch MOTION. Only the motion — damage-lock,
	// scream and melee-cancel live in HandleDamaged, which runs before the event is even sent.
	if (!CDO->ActivationBlockedTags.HasTagExact(Tags.State_Combat_Grabbing))
	{
		CDO->ActivationBlockedTags.AddTag(Tags.State_Combat_Grabbing);
	}
}

bool UAZ_GA_HitReact::ResolveShoveDescriptor(const AActor* Avatar, FAZ_CombatMontage& Out) const
{
	// RANDOM PICK, filtered to entries that actually have a montage — an array with an empty slot is the
	// normal state while someone is still authoring it, and rolling onto that slot would silently drop
	// the whole escape reaction.
	TArray<const FAZ_CombatMontage*> Valid;
	for (const FAZ_CombatMontage& Candidate : GrabEscapeReactions)
	{
		if (Candidate.IsSet())
		{
			Valid.Add(&Candidate);
		}
	}
	if (Valid.Num() > 0)
	{
		Out = *Valid[FMath::RandRange(0, Valid.Num() - 1)];
		return true;
	}
	return ResolveShoveFallback(Avatar, Out);
}

float UAZ_GA_HitReact::GetShoveHoldSeconds(const AActor* Avatar)
{
	// Read the array off the GRANTED class, not the native default: the Chalkie is granted a BP tuning
	// child, and its CDO carries the authored entries. Spec.Ability IS that class's default object.
	if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Avatar))
	{
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			const UAZ_GA_HitReact* Reaction = Cast<UAZ_GA_HitReact>(Spec.Ability);
			if (!Reaction)
			{
				continue;
			}
			float Longest = 0.f;
			for (const FAZ_CombatMontage& Candidate : Reaction->GrabEscapeReactions)
			{
				if (Candidate.IsSet())
				{
					Longest = FMath::Max(Longest, Candidate.ResolveGate());
				}
			}
			if (Longest > 0.f)
			{
				return Longest;
			}
			break;
		}
	}
	FAZ_CombatMontage Single;
	return ResolveShoveFallback(Avatar, Single) ? Single.ResolveGate() : 1.5f;
}

bool UAZ_GA_HitReact::ResolveShoveFallback(const AActor* Avatar, FAZ_CombatMontage& Out)
{
	if (UAZ_GA_MeleeAttack::FindAnimSetCombatMontage(Avatar, TEXT("GrabEscapeReact"), Out))
	{
		return true;   // authored per variant — the author's numbers win outright
	}
	if (!UAZ_GA_MeleeAttack::FindAnimSetCombatMontage(Avatar, TEXT("HitReact"), Out))
	{
		return false;
	}
	// FALLBACK: same clip, DIFFERENT beat. The flinch descriptor cuts KB_Chase_2 at 1.81s because a punch
	// reaction wants the recoil peak and nothing after it — driving that clip further turns a knockback
	// into a stroll back at you (see FAZ_CombatMontage's header). A grab escape is the opposite: the
	// player earned the whole beat, so it plays the clip out, root motion and all, and the Chalkie's walk
	// back in IS the authored consequence of letting it recover.
	//
	// Author GrabEscapeReact on the anim set to override this per variant.
	Out.ActiveSeconds = 0.f;       // 0 = the whole montage
	Out.RootMotionSeconds = 0.f;   // 0 = match the beat, i.e. also the whole montage
	return true;
}

void UAZ_GA_HitReact::DeclareAbilityTags()
{
	Super::DeclareAbilityTags();

	const FAZ_GameplayTags& T = FAZ_GameplayTags::Get();
	ActivationBlockedTags.AddTag(T.State_Combat_Grabbing);   // grab armor (rule 8) — motion only
	// VICTIM-side grab armor, now that the hero runs this ability too: a third Chalkie clawing a grabbed
	// hero must not stomp the paired grab montage (or the self-driven escape, which holds State.Grabbed
	// to its own end) with a FullBody flinch. Motion only — the damage GE has already run. Chalkies never
	// carry State.Grabbed, so this is inert on their side.
	ActivationBlockedTags.AddTag(T.State_Grabbed);
	ActivationBlockedTags.AddTag(T.Character_Dead);
	ActivationBlockedTags.AddTag(T.Character_Dying);
	// Getting hit interrupts your swing. This replaces HandleDamaged's hand-rolled loop over every spec
	// of class UAZ_GA_ZombieMelee — declared once, and it covers the hero side and any future attack on
	// the same rail for free.
	CancelAbilitiesWithTag.AddTag(T.Ability_Combat_Melee);
}

bool UAZ_GA_HitReact::TrySelectReactionByPose(const AActor* Avatar, FAZ_CombatMontage& InOutDesc, float& OutStartPosition) const
{
	OutStartPosition = 0.f;
	if (!ReactionDatabase || !Avatar)
	{
		return false;   // legacy single-montage path by configuration
	}
	// The pose history lives on whichever mesh runs the Chalkie's ABP.
	UAnimInstance* Anim = nullptr;
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(Avatar);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->GetAnimInstance())
		{
			Anim = Mesh->GetAnimInstance();
			break;
		}
	}
	if (!Anim)
	{
		UE_LOG(LogTemp, Warning, TEXT("[React MM] FALLBACK reason=no anim instance"));
		return false;
	}

	TArray<UObject*> AssetsToSearch;
	AssetsToSearch.Add(ReactionDatabase);
	FPoseSearchBlueprintResult Result;
	UPoseSearchLibrary::MotionMatch(Anim, AssetsToSearch, FName(TEXT("PoseHistory")),
		FPoseSearchContinuingProperties(), FPoseSearchFutureProperties(), Result);

	UAnimMontage* Picked = Cast<UAnimMontage>(Result.SelectedAnim);
	if (!Picked || Result.SearchCost >= BIG_NUMBER)
	{
		// Same silent-failure shape as everywhere else: no pose history, or an unbuilt index (R12/R14).
		UE_LOG(LogTemp, Warning, TEXT("[React MM] FALLBACK reason=empty search (anim=%s cost=%.1f) — pose history or index"),
			*GetNameSafe(Result.SelectedAnim), Result.SearchCost);
		return false;
	}
	if (Result.SelectedTime > ReactionEntryMaxTime)
	{
		UE_LOG(LogTemp, Warning, TEXT("[React MM] FALLBACK reason=entry t=%.2f past the impact window (%.2f) — SamplingRange lost?"),
			Result.SelectedTime, ReactionEntryMaxTime);
		return false;
	}

	UE_LOG(LogTemp, Display, TEXT("[React MM] %s: %s -> %s t=%.2f cost=%.1f (beat %.2fs kept)"),
		*GetNameSafe(Avatar), *GetNameSafe(InOutDesc.Montage), *Picked->GetName(),
		Result.SelectedTime, Result.SearchCost, InOutDesc.ActiveSeconds);

	// ONE OWNER PER FACT — refined once the pool became heterogeneous (3.1s..7.5s clips): the search owns
	// WHICH clip, and the CLIP owns its own beat. The variant descriptor's ActiveSeconds was authored for
	// one 3.10s montage; left in place it arms a cut timer that fires before a longer clip's authored
	// BeatEnd (bNotifyOwnsBeat needs Beat >= NotifyBeat - 0.05), truncating every long knockback. Adopt
	// the picked clip's measured beat so the notify owns the cut — and with RootMotionSeconds at 0
	// ("match the beat") the capsule follows the recoil for exactly that window too.
	// staggerRecoverSeconds stays the descriptor's: how long THIS variant is dazed AFTER the animation is
	// characterisation, and it is the one duration with no clip to read it from.
	if (const float ClipBeat = UAZ_GA_MeleeAttack::FindBeatEndNotifyTime(Picked); ClipBeat > 0.f)
	{
		InOutDesc.ActiveSeconds = ClipBeat;
	}
	InOutDesc.Montage = Picked;
	OutStartPosition = Result.SelectedTime;
	return true;
}

void UAZ_GA_HitReact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* Avatar = GetAvatarActorFromActorInfo();
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();

	// Resolve the clip + timing by trigger. ONE resolve site — HandleDamaged no longer knows montages.
	Desc = FAZ_CombatMontage();
	bRecovering = false;   // retrigger: a fresh reaction is not a continuation of the last one's recover
	const bool bStepBack = TriggerEventData && TriggerEventData->EventTag == Tags.Event_Combat_StepBack;
	const bool bGrabShoved = TriggerEventData && TriggerEventData->EventTag == Tags.Event_Combat_GrabShoved;
	if (bGrabShoved)
	{
		ResolveShoveDescriptor(Avatar, Desc);
	}
	else if (bStepBack)
	{
		// Payload-supplied clip. Montage tasks want a mutable montage; the payload field is const by
		// signature only — these are shared assets we do not mutate.
		Desc.Montage = const_cast<UAnimMontage*>(Cast<UAnimMontage>(TriggerEventData->OptionalObject.Get()));
		// Caller beat (crowd pacing) — may SHORTEN the clip's own beat, never lengthen it: the BeatEnd
		// notify still fires at the authored time and acts as the ceiling.
		Desc.ActiveSeconds = TriggerEventData->EventMagnitude;
	}
	else
	{
		// Per-clip descriptor from the anim set (arch step B); legacy fallback = bare montage field +
		// the pawn's FlinchRootMotionSeconds, so un-authored variants keep pre-B behaviour exactly.
		if (!UAZ_GA_MeleeAttack::FindAnimSetCombatMontage(Avatar, TEXT("HitReact"), Desc))
		{
			Desc.Montage = UAZ_GA_MeleeAttack::FindAnimSetMontage(Avatar, TEXT("HitReactMontage"));
			Desc.ActiveSeconds = UAZ_GA_MeleeAttack::ReadConfigFloat(Avatar, TEXT("FlinchRootMotionSeconds"), 0.f);
		}
		// DIRECTIONAL FALLBACK — the hero path (no AnimSet on the pawn resolves above). Side = which
		// side of the VICTIM'S body the sweep struck, read off the hit result the attacker stamped into
		// the effect context (AZ_GA_MeleeAttack.cpp AddHitResult). Local +Y is the actor's right. NOT
		// keyed on the attacker's hand: a right hook landing on your right side must pick the right-side
		// react no matter whose fist it was, and the geometric read survives both bodies turning.
		if (!Desc.IsSet() && Avatar)
		{
			const FHitResult* Hit = TriggerEventData ? TriggerEventData->ContextHandle.GetHitResult() : nullptr;
			float LocalY = 0.f;   // no hit result (environment damage) -> Left, the documented default
			if (Hit)
			{
				LocalY = Avatar->GetActorTransform().InverseTransformPosition(Hit->ImpactPoint).Y;
			}
			Desc = (LocalY > 0.f) ? DefaultReactionRight : DefaultReactionLeft;
			UE_LOG(LogTemp, Display, TEXT("[HitReact] %s fallback react: struck %s (localY=%.1f, hit=%s)"),
				*GetNameSafe(Avatar), (LocalY > 0.f) ? TEXT("RIGHT") : TEXT("LEFT"), LocalY,
				Hit ? TEXT("yes") : TEXT("no"));
		}
	}

	// POSE-SELECTED CLIP (tier 1) — only on the plain hit-react path: StepBack carries its clip in the
	// payload and the shove's escape clip is chosen by the grab, so neither is the search's business.
	// Runs AFTER the descriptor resolve so the beat/recover/root-motion timing is already in hand and
	// only the montage is swapped; a failed search leaves the hard-picked clip exactly as before.
	float ReactionStartPosition = 0.f;
	if (!bStepBack && !bGrabShoved && Desc.IsSet())
	{
		TrySelectReactionByPose(Avatar, Desc, ReactionStartPosition);
	}

	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	if (!Avatar || !AnimInstance || !Desc.IsSet())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Montage task: BeatEnd is the only event we wait for; completion callbacks are the lifecycle.
	//
	// ...EXCEPT on the shove. KB_Chase_2 carries an authored Event.Combat.BeatEnd at 1.810s — the recoil
	// peak, correct for a flinch — and subscribing to it here would stop the montage there no matter what
	// the descriptor says, silently cutting a 3.10s knockback down to 1.81s. The escape wants the clip
	// whole, so it does not listen. An authored GrabEscapeReact that DOES want an early cut still gets
	// one, through the cut timer below.
	const bool bUseBeatNotify = !bGrabShoved;
	FGameplayTagContainer EventTags;
	if (bUseBeatNotify)
	{
		EventTags.AddTag(Tags.Event_Combat_BeatEnd);
	}
	MontageTask = UAZ_AT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
		this, FName("HitReactMontage"), Desc.Montage, EventTags, 1.f, NAME_None,
		/*bStopWhenAbilityEnds*/ true, 1.f);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// NOT bound: OnBlendOut. Ending there would drop the Staggered gate at blend START; waiting for
	// OnCompleted makes the gate naturally span beat + blend — what a viewer reads as "still staggering".
	MontageTask->OnCompleted.AddDynamic(this, &UAZ_GA_HitReact::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UAZ_GA_HitReact::OnMontageFinished);
	MontageTask->OnCancelled.AddDynamic(this, &UAZ_GA_HitReact::OnMontageFinished);
	MontageTask->EventReceived.AddDynamic(this, &UAZ_GA_HitReact::OnMontageEvent);
	MontageTask->ReadyForActivation();

	// Entry frame from the search (the task has no start-position parameter, so set it on the montage the
	// task just started — same pattern as the PSI catch). Bounded by ReactionEntryMaxTime, so this only
	// ever skips a few frames into the impact; the clip's own BeatEnd notify still owns the cut.
	if (ReactionStartPosition > KINDA_SMALL_NUMBER)
	{
		AnimInstance->Montage_SetPosition(Desc.Montage, ReactionStartPosition);
	}

	// Capsule follows the recoil for the descriptor's RM window (KnockBack_Chase clips walk back in
	// during their second half — driving past the peak turns a knockback into a stroll-back-at-you).
	UAZ_PawnMoverComponent* Mover = Avatar->FindComponentByClass<UAZ_PawnMoverComponent>();
	if (Mover)
	{
		RootMotionGen = Mover->DriveRootMotion(Desc.ResolveRootMotion());
	}

	// TEMP diagnostic ([HitReact]): the montage playing is not proof the CAPSULE moves — that needs the
	// anim instance in RootMotionFromEverything, the clip's bEnableRootMotion, a live RM layered move and
	// a slot that reaches the output pose. Report the inputs here and the travelled distance at EndAbility
	// so "the knockback doesn't move him" is answered with a number instead of a theory.
	// THE STAGGER GATE, applied explicitly. SetLooseGameplayTagCount, not Add/Remove: loose tags are
	// COUNTED, and a retrigger (punched again mid-reaction) would otherwise leave the count at 2 and the
	// gate stuck on permanently after the first clear. Set to an absolute 1 — idempotent under any
	// activation order.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->SetLooseGameplayTagCount(Tags.State_Combat_Staggered, 1);
	}

	ReactStartLocation = Avatar->GetActorLocation();
	UE_LOG(LogTemp, Display, TEXT("[HitReact] %s clip=%s beat=%.2f rm=%.2f gen=%llu mover=%s"),
		*GetNameSafe(Avatar), *GetNameSafe(Desc.Montage), Desc.ResolveBeat(), Desc.ResolveRootMotion(),
		RootMotionGen, Mover ? TEXT("yes") : TEXT("NO MOVER"));

	// CLOCKS. Primary: the BeatEnd notify authored on the montage (the timeline itself). The cut timer
	// is armed ONLY when that notify doesn't own the beat — un-authored variant, or a caller-shortened
	// step-back — and does the identical stop. The watchdog ends the ability if every event path failed.
	const float NotifyBeat = bUseBeatNotify ? UAZ_GA_MeleeAttack::FindBeatEndNotifyTime(Desc.Montage) : 0.f;
	const float Beat = Desc.ResolveBeat();
	const bool bNotifyOwnsBeat = NotifyBeat > 0.f && Beat >= NotifyBeat - 0.05f;
	if (!bNotifyOwnsBeat && Desc.IsCutEarly())
	{
		GetWorld()->GetTimerManager().SetTimer(BeatCutTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]() { StopMontageAtBeat(); }), Beat, false);
	}
	GetWorld()->GetTimerManager().SetTimer(Watchdog,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (IsActive())
			{
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			}
		}), Desc.ResolveStaggerHold() + 0.5f, false);   // must outlast the recover hold, or it would cut it short
}

void UAZ_GA_HitReact::OnMontageEvent(FGameplayTag EventTag, FGameplayEventData EventData)
{
	if (EventTag == FAZ_GameplayTags::Get().Event_Combat_BeatEnd)
	{
		StopMontageAtBeat();
	}
}

void UAZ_GA_HitReact::OnMontageFinished(FGameplayTag EventTag, FGameplayEventData EventData)
{
	// RECOVER HOLD: the animation is done but the victim is still reeling. Keeping the ABILITY alive is
	// what keeps State.Combat.Staggered up, which is the one fact the BT reads — so "the Chalkie pauses
	// before resuming the chase" needs no BT Wait node, no second duration, and nothing to keep in sync.
	// A timer is correct here precisely because nothing is playing: there is no timeline to anchor to.
	const float Recover = FMath::Max(0.f, Desc.StaggerRecoverSeconds);
	if (Recover > 0.f && !bRecovering && IsActive())
	{
		bRecovering = true;
		GetWorld()->GetTimerManager().SetTimer(RecoverTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (IsActive())
				{
					EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
				}
			}), Recover, false);
		return;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAZ_GA_HitReact::StopMontageAtBeat()
{
	UAnimInstance* AnimInstance = CurrentActorInfo ? CurrentActorInfo->GetAnimInstance() : nullptr;
	if (AnimInstance && Desc.Montage && AnimInstance->Montage_IsPlaying(Desc.Montage))
	{
		// Blend-out starts here; the task's OnCompleted fires when it finishes → EndAbility → tag drops.
		AnimInstance->Montage_Stop(Desc.BlendOutTime, Desc.Montage);
	}
}

void UAZ_GA_HitReact::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BeatCutTimer);
		World->GetTimerManager().ClearTimer(Watchdog);
		World->GetTimerManager().ClearTimer(RecoverTimer);
	}
	bRecovering = false;
	// Drop the gate. Absolute 0 for the same counted-tag reason as the set — and it must happen on EVERY
	// exit (completed, interrupted, cancelled, death), which is why it lives here rather than on any one
	// callback. A retrigger re-sets it to 1 immediately afterwards, so the gate never flickers off.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->SetLooseGameplayTagCount(FAZ_GameplayTags::Get().State_Combat_Staggered, 0);
	}

	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		if (UAZ_PawnMoverComponent* Mover = Avatar->FindComponentByClass<UAZ_PawnMoverComponent>())
		{
			// Generation-scoped: no-ops if something newer (a fresh reaction, an attack) took the bridge.
			Mover->ReleaseRootMotion(RootMotionGen);
		}
		// TEMP diagnostic ([HitReact]): how far the capsule ACTUALLY travelled. The clips recoil 85-146cm,
		// so a reading near 0 means the root motion never reached the Mover, not that the clip is wrong.
		if (!ReactStartLocation.IsZero())
		{
			UE_LOG(LogTemp, Display, TEXT("[HitReact] %s reaction over — capsule moved %.1fcm%s"),
				*GetNameSafe(Avatar), FVector::Dist2D(Avatar->GetActorLocation(), ReactStartLocation),
				bWasCancelled ? TEXT(" (cancelled)") : TEXT(""));
		}
	}
	RootMotionGen = 0;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
