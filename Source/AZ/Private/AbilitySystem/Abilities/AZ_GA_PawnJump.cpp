// Copyright Artur. AZ project.

#include "AbilitySystem/Abilities/AZ_GA_PawnJump.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AZ_GameplayTags.h"
#include "Character/AZ_JumpRequester.h"
#include "Engine/World.h"
#include "TimerManager.h"

UAZ_GA_PawnJump::UAZ_GA_PawnJump()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

bool UAZ_GA_PawnJump::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// Pawn must opt in by implementing IAZ_JumpRequester. Vehicles don't implement
	// it; same GA class drops gracefully across the pawn taxonomy.
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	return Avatar && Avatar->Implements<UAZ_JumpRequester>();
}

void UAZ_GA_PawnJump::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
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
	if (IAZ_JumpRequester* Requester = Cast<IAZ_JumpRequester>(Avatar))
	{
		Requester->SetJumpPressed(true);
	}

	// Release via the WaitInputRelease TASK, not the raw InputReleased virtual: the task replicates the
	// release to the server instance, which otherwise NEVER ends under LocalPredicted (the ASC invokes the
	// virtual only on the owning client) — a remote co-op client could jump exactly once (audit P1-8).
	// bTestAlreadyReleased=true: the server activation arrives via replication and a fast tap may already
	// have released — fire immediately instead of waiting forever. Same pattern as GA_Crouch.
	UAbilityTask_WaitInputRelease* WaitRelease =
		UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased*/ true);
	WaitRelease->OnRelease.AddDynamic(this, &UAZ_GA_PawnJump::OnJumpInputReleased);
	WaitRelease->ReadyForActivation();

	// The ability spans the WHOLE jump, not the button hold: takeoff, air, and the landing recovery.
	// Its ActivationOwnedTags carry Movement.Jumping, which its own ActivationBlockedTags reject, so
	// staying Active is what stops a second jump from cutting the landing animation short.
	//
	// EVENTS DRIVE. The anim instance owns "the jump animation still has the body" and fires
	// Event.Movement.LandComplete on the frame the last jump database hands back — whether the land
	// played out or a walk start interrupted it. The ability never guesses a recovery duration, and
	// this can never be an end-of-clip anim notify: the blend stack updates blending-out players with
	// an inactive context (AnimNode_BlendStack.cpp:893-898), so a notify past the hand-back never fires.
	WaitLandTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, FAZ_GameplayTags::Get().Event_Movement_LandComplete);
	WaitLandTask->EventReceived.AddDynamic(this, &UAZ_GA_PawnJump::OnLandComplete);
	WaitLandTask->ReadyForActivation();

	// TIMERS GUARD. Without this, a jump whose animation cycle never completes would hold
	// Movement.Jumping forever and wedge every future jump.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			LandWatchdogTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]() { OnLandWatchdogExpired(); }),
			LandWatchdogSeconds, /*bLoop*/ false);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UAZ_GA_PawnJump::OnJumpInputReleased(float TimeHeld)
{
	// Drop the press so CMC stops adding hold-height and does not auto re-jump on touchdown.
	// The ability itself keeps running until the landing completes.
	if (const FGameplayAbilityActorInfo* Info = CurrentActorInfo)
	{
		if (Info->AvatarActor.IsValid())
		{
			if (IAZ_JumpRequester* Requester = Cast<IAZ_JumpRequester>(Info->AvatarActor.Get()))
			{
				Requester->SetJumpPressed(false);
			}
		}
	}
}

void UAZ_GA_PawnJump::OnLandComplete(FGameplayEventData Payload)
{
	// The ONLY proof that the authored notify reached the ASC. Without it, a silent log is ambiguous
	// between "the notify never fired" and "nothing was listening" — which need opposite fixes.
	UE_LOG(LogTemp, Warning, TEXT("[CmcJump] land complete (notify) -> ending jump ability"));

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
		/*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}

void UAZ_GA_PawnJump::OnLandWatchdogExpired()
{
	UE_LOG(LogTemp, Warning,
		TEXT("[CmcJump] land-complete event never arrived within %.1fs — ending jump ability on the "
		     "watchdog. The jump animation cycle did not hand back."),
		LandWatchdogSeconds);

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
		/*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}

void UAZ_GA_PawnJump::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// The watchdog must die with the ability — a cancel (death, grab, pawn switch) ends the ability
	// without the timer firing, and a stale handle would end the NEXT jump prematurely.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LandWatchdogTimer);
	}

	// Single place to clear the press flag — covers natural release, cancel,
	// pawn-switch (avatar change), and GE-driven termination.
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		if (IAZ_JumpRequester* Requester = Cast<IAZ_JumpRequester>(ActorInfo->AvatarActor.Get()))
		{
			Requester->SetJumpPressed(false);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
