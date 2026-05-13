// Copyright Artur. AZ project.

#include "AbilitySystem/Abilities/AZ_GA_PawnJump.h"

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

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UAZ_GA_PawnJump::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	// End the ability on release; EndAbility clears the pawn's press flag.
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
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
