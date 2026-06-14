// Copyright Artur. AZ project.

#include "AbilitySystem/Abilities/AZ_GA_PawnJump.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Character/AZ_JumpRequester.h"

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

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UAZ_GA_PawnJump::OnJumpInputReleased(float TimeHeld)
{
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
