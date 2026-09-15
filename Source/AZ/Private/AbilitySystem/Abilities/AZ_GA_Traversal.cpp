// Copyright Artur. AZ project.

#include "AbilitySystem/Abilities/AZ_GA_Traversal.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/AbilityTasks/AZ_AT_PlayMontageAndWaitForEvent.h"
#include "Animation/AnimMontage.h"
#include "AZ_GameplayTags.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Character/AZ_TraversalComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "MotionWarpingComponent.h"
#include "TimerManager.h"

UAZ_GA_Traversal::UAZ_GA_Traversal()
{
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	ReplicationPolicy  = EGameplayAbilityReplicationPolicy::ReplicateNo;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UAZ_GA_Traversal::DeclareAbilityTags()
{
	Super::DeclareAbilityTags();

	const FAZ_GameplayTags& T = FAZ_GameplayTags::Get();
	ActivationOwnedTags.AddTag(T.State_Traversing);

	ActivationBlockedTags.AddTag(T.State_Traversing);         // one traversal at a time
	ActivationBlockedTags.AddTag(T.State_Grabbed);            // caught: the struggle is the only action
	ActivationBlockedTags.AddTag(T.State_Combat_Staggered);   // a reaction already owns the body
	ActivationBlockedTags.AddTag(T.Character_Dead);
	ActivationBlockedTags.AddTag(T.Character_Dying);
}

bool UAZ_GA_Traversal::BuildTraversalRequest(FAZ_TraversalRequest& OutRequest)
{
	return false;   // a subclass that does not describe an action cannot run one
}

void UAZ_GA_Traversal::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// InstancedPerActor: the SAME instance runs every traversal, so these latches survive the previous one.
	// Without the reset the second action would skip cleanup entirely — leaking its warp targets, its
	// root-motion drive and the Traversing mode.
	bCleanedUp = false;
	bHandedOff = false;
	SuccessMovementMode = FName("Walking");
	RegisteredWarpTargets.Reset();

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

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UAZ_PawnMoverComponent*  Mover   = Avatar ? Avatar->FindComponentByClass<UAZ_PawnMoverComponent>()  : nullptr;
	UMotionWarpingComponent* Warping = Avatar ? Avatar->FindComponentByClass<UMotionWarpingComponent>() : nullptr;
	if (!Avatar || !Mover || !Warping)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FAZ_TraversalRequest Request;
	if (!BuildTraversalRequest(Request) || !Request.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (Request.SuccessMovementMode != TEXT("Walking") && Request.SuccessMovementMode != TEXT("Falling"))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Traversal] rejected unsupported success movement mode %s"),
			*Request.SuccessMovementMode.ToString());
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	SuccessMovementMode = Request.SuccessMovementMode;

	// (1) Warp targets FIRST. A window can open on the montage's very first frame — the Relaxed run clips
	// do exactly that — and a target registered after it opens is a silent no-op for that window.
	for (const FAZ_TraversalWarpTarget& Target : Request.WarpTargets)
	{
		if (Target.Name.IsNone())
		{
			continue;
		}
		Warping->AddOrUpdateWarpTargetFromLocationAndRotation(
			Target.Name, Target.Transform.GetLocation(), Target.Transform.Rotator());
		RegisteredWarpTargets.Add(Target.Name);
	}

	// (2) Let the capsule through the ONE primitive this action crosses. Without it the action cannot work
	// at all: the traversal mode sweeps the capsule and deliberately does NOT slide on a blocking hit
	// (AZ_PawnMovementMode_RMAction.cpp:87-95 — correct for a jump-in-place), while a mantle's or hurdle's
	// first proposed move goes straight into the face it is crossing. The capsule pins there, and warping
	// then amplifies its correction every tick chasing a target it never approaches.
	if (UPrimitiveComponent* Target = Request.CollisionExemption.Get())
	{
		if (UCapsuleComponent* Capsule = Avatar->FindComponentByClass<UCapsuleComponent>())
		{
			Capsule->IgnoreComponentWhenMoving(Target, true);
			IgnoredTargetComponent = Target;
		}
	}

	// (3) Gravity-free, no floor-snap, and — unlike the jump's RMAction instance — no apex handoff, so the
	// action plays out. Walking's TryMoveToAdjustHeightAboveFloor would otherwise eat the whole climb.
	Mover->QueueNextMode(TEXT("Traversing"));

	// (4) Montage at rate 1.0. Warping corrects position spatially; it never rescales time.
	//
	// bStopWhenAbilityEnds is FALSE on purpose: the success path stops the montage itself with the handoff
	// blend, and letting EndAbility fire a second stop would replace that blend with an abrupt one. Every
	// exit path owns its own stop. (The task still force-stops on an explicit external cancel.)
	FGameplayTagContainer EventTags;
	EventTags.AddTag(FAZ_GameplayTags::Get().Event_Traversal_Handoff);

	MontageTask = UAZ_AT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
		this, Request.TaskName, Request.Montage, EventTags,
		/*Rate*/ 1.f, /*StartSection*/ NAME_None, /*bStopWhenAbilityEnds*/ false,
		/*AnimRootMotionTranslationScale*/ 1.f, /*StartTimeSeconds*/ Request.StartTime);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// Three DISTINCT outcomes, deliberately not funnelled through one callback.
	MontageTask->EventReceived.AddDynamic(this, &UAZ_GA_Traversal::OnHandoffEvent);
	MontageTask->OnCompleted.AddDynamic(this, &UAZ_GA_Traversal::OnMontagePlayedOut);
	MontageTask->OnBlendOut.AddDynamic(this, &UAZ_GA_Traversal::OnMontagePlayedOut);
	MontageTask->OnInterrupted.AddDynamic(this, &UAZ_GA_Traversal::OnMontageAborted);
	MontageTask->OnCancelled.AddDynamic(this, &UAZ_GA_Traversal::OnMontageAborted);
	MontageTask->ReadyForActivation();
	if (!IsActive())
	{
		return;   // a callback already ended us synchronously
	}

	// (5) Bridge the montage's root motion to the capsule. Without a live layered move the clip animates
	// the mesh in place — and the warp modifier never runs at all, because SkewWarp executes inside
	// FLayeredMove_RootMotionAttribute::GenerateMove. Sized to what is actually left of the clip, since
	// starting partway in means there is less of it.
	const float RemainingLength = FMath::Max(0.f, Request.Montage->GetPlayLength() - Request.StartTime);
	RootMotionGeneration = Mover->DriveRootMotion(RemainingLength);

#if !UE_BUILD_SHIPPING
	// rmGen=0 would mean DriveRootMotion refused (sim proxy or zero length) — the capsule would then be
	// frozen by the gravity-free mode rather than moving.
	FString TargetList;
	for (const FAZ_TraversalWarpTarget& Target : Request.WarpTargets)
	{
		TargetList += FString::Printf(TEXT("%s(%.0f,%.0f,%.0f) "),
			*Target.Name.ToString(), Target.Transform.GetLocation().X,
			Target.Transform.GetLocation().Y, Target.Transform.GetLocation().Z);
	}
	UE_LOG(LogTemp, Warning, TEXT("[Traversal] clip=%s startT=%.2f play=%.2f rmGen=%llu ignoring=%s successMode=%s targets=%s"),
		*GetNameSafe(Request.Montage), Request.StartTime, RemainingLength, RootMotionGeneration,
		*GetNameSafe(IgnoredTargetComponent.Get()), *SuccessMovementMode.ToString(), *TargetList);
#endif

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			WatchdogTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]() { OnWatchdogExpired(); }),
			RemainingLength + WatchdogPadding, /*bLoop*/ false);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UAZ_GA_Traversal::OnHandoffEvent(FGameplayTag EventTag, FGameplayEventData EventData)
{
	if (bHandedOff)
	{
		return;   // one handoff per action; a duplicate marker must not re-run the release
	}
	bHandedOff = true;

	// ORDER IS THE WHOLE POINT. Release the capsule FIRST: from this instant locomotion owns movement and
	// inherits the clip's real velocity out of the sync state, so the blend-out below is purely cosmetic
	// and cannot drag the capsule further along the authored tail.
	ReleaseTraversal(/*bSuccessfulHandoff*/ true);

	// THEN cut the tail with our own blend, under which the incoming locomotion pose comes up. Done here
	// rather than by letting EndAbility stop the montage, which would swap this blend for an abrupt stop.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->CurrentMontageStop(HandoffBlendOutTime);
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, /*bWasCancelled*/ false);
}

void UAZ_GA_Traversal::OnMontagePlayedOut(FGameplayTag EventTag, FGameplayEventData EventData)
{
	// The clip ran out on its own — a standing mantle's normal ending, and the safety net for a moving clip
	// whose handoff marker is missing. Still a clean release, just later than we would have liked.
	ReleaseTraversal(/*bSuccessfulHandoff*/ true);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, /*bWasCancelled*/ false);
}

void UAZ_GA_Traversal::OnMontageAborted(FGameplayTag EventTag, FGameplayEventData EventData)
{
	// Another montage displaced ours, or something cancelled us (death, grab). Recovery, not handoff: no
	// graceful blend, and whoever interrupted owns the pose from here.
	ReleaseTraversal();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, /*bWasCancelled*/ true);
}

void UAZ_GA_Traversal::OnWatchdogExpired()
{
	// No callback ever arrived — not the handoff marker, not the montage's own end. Timers are the
	// backstop, never the mechanism. Recover rather than leave State.Traversing held, which would block
	// every future traversal.
	ReleaseTraversal();
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->CurrentMontageStop(HandoffBlendOutTime);
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, /*bWasCancelled*/ true);
}

void UAZ_GA_Traversal::ReleaseTraversal(bool bSuccessfulHandoff)
{
	if (bCleanedUp)
	{
		return;   // idempotent: handoff, completion, abort, watchdog and EndAbility can all reach here
	}
	bCleanedUp = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WatchdogTimer);
	}

	AActor* Avatar = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	if (!Avatar)
	{
		RootMotionGeneration = 0;
		return;
	}

	if (UAZ_PawnMoverComponent* Mover = Avatar->FindComponentByClass<UAZ_PawnMoverComponent>())
	{
		// (1) ROOT MOTION FIRST, so everything after runs with locomotion already owning movement.
		// Generation-scoped: a late callback from this action can never cancel a NEWER owner's drive.
		const bool bDriveSuperseded = RootMotionGeneration != 0
			&& RootMotionGeneration != Mover->GetRootMotionGeneration();
		Mover->ReleaseRootMotion(RootMotionGeneration);

		// (2) Collision before the mode change: the surface the capsule must now stand on is the exact
		// component it was passing through, so Walking's floor query has to see it again.
		if (UPrimitiveComponent* Ignored = IgnoredTargetComponent.Get())
		{
			if (UCapsuleComponent* Capsule = Avatar->FindComponentByClass<UCapsuleComponent>())
			{
				Capsule->IgnoreComponentWhenMoving(Ignored, false);
			}
		}
		IgnoredTargetComponent = nullptr;

		// (3) Hand the capsule back only while this action still owns it. A newer drive or another current
		// mode belongs to the interrupter. Success may explicitly enter Falling for a climb-and-drop;
		// abort/watchdog recovery retains Walking's floor query to determine actual support. In both cases
		// preserve the velocity the Traversing mode realized instead of inserting a stop or launch impulse.
		if (!bDriveSuperseded && Mover->GetMovementModeName() == TEXT("Traversing"))
		{
			Mover->QueueNextMode(bSuccessfulHandoff ? SuccessMovementMode : FName("Walking"));
		}
	}
	else if (UPrimitiveComponent* Ignored = IgnoredTargetComponent.Get())
	{
		if (UCapsuleComponent* Capsule = Avatar->FindComponentByClass<UCapsuleComponent>())
		{
			Capsule->IgnoreComponentWhenMoving(Ignored, false);
		}
		IgnoredTargetComponent = nullptr;
	}

	if (UMotionWarpingComponent* Warping = Avatar->FindComponentByClass<UMotionWarpingComponent>())
	{
		for (const FName& TargetName : RegisteredWarpTargets)
		{
			Warping->RemoveWarpTarget(TargetName);
		}
	}
	RegisteredWarpTargets.Reset();

	if (UAZ_TraversalComponent* Traversal = Avatar->FindComponentByClass<UAZ_TraversalComponent>())
	{
		Traversal->ClearPendingCandidate();
	}

	RootMotionGeneration = 0;
}

void UAZ_GA_Traversal::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Backstop only: every deliberate exit path has already released. This catches the ones that bypass
	// them entirely — an external CancelAbility, EndPlay, unpossess.
	ReleaseTraversal();

	MontageTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
