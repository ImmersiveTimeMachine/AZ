// Copyright Artur. AZ project.

#include "AbilitySystem/Abilities/AZ_GA_Mantle.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimMontage.h"
#include "AZ_GameplayTags.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Character/AZ_TraversalComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "MotionWarpingComponent.h"
#include "TimerManager.h"

UAZ_GA_Mantle::UAZ_GA_Mantle()
{
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	ReplicationPolicy  = EGameplayAbilityReplicationPolicy::ReplicateNo;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UAZ_GA_Mantle::DeclareAbilityTags()
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

void UAZ_GA_Mantle::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// InstancedPerActor: the SAME instance runs every mantle, so the cleanup latch survives the previous
	// one. Without this reset the second mantle would skip cleanup entirely — leaking its warp target,
	// its root-motion drive and the Traversing mode.
	bCleanedUp = false;

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
	UAZ_TraversalComponent*  Traversal = Avatar ? Avatar->FindComponentByClass<UAZ_TraversalComponent>()  : nullptr;
	UAZ_PawnMoverComponent*  Mover     = Avatar ? Avatar->FindComponentByClass<UAZ_PawnMoverComponent>()  : nullptr;
	UMotionWarpingComponent* Warping   = Avatar ? Avatar->FindComponentByClass<UMotionWarpingComponent>() : nullptr;

	if (!Traversal || !Mover || !Warping)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Copied, not referenced: cleanup clears the component's pending candidate, and EndAbility can run
	// re-entrantly from inside this function's own failure paths.
	const FAZ_MantleCandidate Candidate = Traversal->GetPendingCandidate();
	if (!Candidate.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// (1) Warp target first. The montages' warp point is authored as a Static transform at the animation's
	// contact anchor, so this target is the PHYSICAL LIP — not the destination capsule. The modifier
	// composes the two into the root's end transform itself.
	Warping->AddOrUpdateWarpTargetFromLocationAndRotation(
		WarpTargetName,
		Candidate.LedgeLocation + FVector(0.f, 0.f, WarpTargetZOffset),
		FRotationMatrix::MakeFromX(-Candidate.LedgeNormal).Rotator());

	// (2) Let the capsule through the ONE face it is climbing. Without this the action cannot work at all:
	// the Traversing mode sweeps the capsule and deliberately does NOT slide on a blocking hit
	// (AZ_PawnMovementMode_RMAction.cpp:87-95 — correct for a jump-in-place), and a mantle's first proposed
	// move is diagonally INTO the wall. The capsule stops dead against the face while the montage plays on,
	// and motion warping then amplifies its correction every tick chasing a target the pinned capsule never
	// approaches. Scoped to the single validated primitive, so every other piece of geometry still blocks.
	if (UPrimitiveComponent* Target = Candidate.TargetComponent.Get())
	{
		if (UCapsuleComponent* Capsule = Avatar->FindComponentByClass<UCapsuleComponent>())
		{
			Capsule->IgnoreComponentWhenMoving(Target, true);
			IgnoredTargetComponent = Target;
		}
	}

	// (3) Gravity-free, no floor-snap, and — unlike the jump's RMAction instance — no apex handoff, so it
	// plays to completion. Walking's TryMoveToAdjustHeightAboveFloor would otherwise eat the entire climb.
	Mover->QueueNextMode(TEXT("Traversing"));

	// (3) Montage at rate 1.0. Warping corrects position spatially; it never rescales time.
	MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, FName("Mantle"), Candidate.Montage, /*Rate*/ 1.f, /*StartSection*/ NAME_None,
		/*bStopWhenAbilityEnds*/ true);
	if (!MontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	MontageTask->OnCompleted.AddDynamic(this, &UAZ_GA_Mantle::OnMontageEnded);
	MontageTask->OnBlendOut.AddDynamic(this, &UAZ_GA_Mantle::OnMontageEnded);
	MontageTask->OnInterrupted.AddDynamic(this, &UAZ_GA_Mantle::OnMontageEnded);
	MontageTask->OnCancelled.AddDynamic(this, &UAZ_GA_Mantle::OnMontageEnded);
	MontageTask->ReadyForActivation();
	if (!IsActive())
	{
		return;   // a callback already ended us synchronously
	}

	// (4) Bridge the montage's root motion to the capsule. Without a live layered move the clip animates
	// the mesh in place — and the warp modifier never runs at all, because SkewWarp executes inside
	// FLayeredMove_RootMotionAttribute::GenerateMove.
	const float MontageLength = Candidate.Montage->GetPlayLength();
	RootMotionGeneration = Mover->DriveRootMotion(MontageLength);

#if !UE_BUILD_SHIPPING
	// One line per attempt, carrying everything needed to read the next PIE run without guessing: which
	// clip was picked, the geometry it was aimed at, and whether the drive and the exemption took.
	// rmGen=0 would mean DriveRootMotion refused (sim proxy or zero length) — the capsule would then be
	// frozen by the gravity-free mode rather than climbing.
	const FVector LandLoc = Candidate.LandingTransform.GetLocation();
	UE_LOG(LogTemp, Warning,
		TEXT("[Mantle] approach=%d spd=%.0f style=%d foot=%d clip=%s lip=(%.0f,%.0f,%.0f) n=(%.2f,%.2f) hAboveFeet=%.1f land=(%.0f,%.0f,%.0f) len=%.2f rmGen=%llu ignoring=%s"),
		static_cast<int32>(Candidate.Approach), Candidate.ApproachSpeed,
		static_cast<int32>(Candidate.Style), static_cast<int32>(Candidate.PlantedFoot),
		*GetNameSafe(Candidate.Montage),
		Candidate.LedgeLocation.X, Candidate.LedgeLocation.Y, Candidate.LedgeLocation.Z,
		Candidate.LedgeNormal.X, Candidate.LedgeNormal.Y, Candidate.HeightAboveFeet,
		LandLoc.X, LandLoc.Y, LandLoc.Z,
		MontageLength, RootMotionGeneration,
		*GetNameSafe(IgnoredTargetComponent.Get()));
#endif

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			WatchdogTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]() { OnWatchdogExpired(); }),
			MontageLength + WatchdogPadding, /*bLoop*/ false);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UAZ_GA_Mantle::OnMontageEnded()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAZ_GA_Mantle::OnWatchdogExpired()
{
	// No montage callback ever arrived. Treat as cancelled so the capsule and tags are recovered rather
	// than left holding State.Traversing, which would block every future mantle.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAZ_GA_Mantle::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (!bCleanedUp)
	{
		bCleanedUp = true;

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(WatchdogTimer);
		}

		if (AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr)
		{
			// FIRST, before anything queues Walking: the surface the capsule now has to stand on is the very
			// component it was allowed to pass through. Restoring the exemption late would hand Walking a
			// floor query that cannot see the floor.
			if (UPrimitiveComponent* Ignored = IgnoredTargetComponent.Get())
			{
				if (UCapsuleComponent* Capsule = Avatar->FindComponentByClass<UCapsuleComponent>())
				{
					Capsule->IgnoreComponentWhenMoving(Ignored, false);
				}
			}
			IgnoredTargetComponent = nullptr;

			if (UMotionWarpingComponent* Warping = Avatar->FindComponentByClass<UMotionWarpingComponent>())
			{
				Warping->RemoveWarpTarget(WarpTargetName);
			}

			if (UAZ_PawnMoverComponent* Mover = Avatar->FindComponentByClass<UAZ_PawnMoverComponent>())
			{
				// Generation-scoped: a no-op once someone newer owns the drive.
				Mover->ReleaseRootMotion(RootMotionGeneration);

				// Only hand the capsule back if we still hold it. Death, a grab, or a knockback may have
				// already taken the mode, and stamping Walking over them would undo their control.
				if (Mover->GetMovementModeName() == TEXT("Traversing"))
				{
					// Walking re-runs its own floor query on activation, so an unsupported landing falls
					// through to Falling without us predicting it here.
					Mover->QueueNextMode(TEXT("Walking"));
				}
			}

			if (UAZ_TraversalComponent* Traversal = Avatar->FindComponentByClass<UAZ_TraversalComponent>())
			{
				Traversal->ClearPendingCandidate();
			}
		}

		RootMotionGeneration = 0;
		MontageTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
