// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/AZ_GA_Jump.h"

#include "Character/AZ_JumpRequester.h"


UAZ_GA_Jump::UAZ_GA_Jump()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	ActivationOwnedTags.RemoveTag(FAZ_GameplayTags::Get().Character_Interacting);
}

void UAZ_GA_Jump::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
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

	// Pawn-agnostic dispatch — works on any pawn class that implements
	// IAZ_JumpRequester (legacy ACharacter-based, v2 Mover-based, future vehicles).
	if (IAZ_JumpRequester* Requester = Cast<IAZ_JumpRequester>(ActorInfo->AvatarActor.Get()))
	{
		Requester->SetJumpPressed(true);
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

bool UAZ_GA_Jump::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const IAZ_JumpRequester* Requester = Cast<IAZ_JumpRequester>(Avatar);
	return Requester && Requester->CanRequestJump();
}

void UAZ_GA_Jump::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		CancelAbility(Handle, ActorInfo, ActivationInfo, true);
	}
}

// Epic's comment (preserved from original):
//	Canceling a non-instanced ability is tricky. Right now this works for Jump since
//	there's nothing that can go wrong by calling StopJumping() if you aren't already
//	jumping. If we had a montage playing non-instanced ability, it would need to make
//	sure the Montage that *it* played was still playing, and if so, to cancel it.

void UAZ_GA_Jump::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &UAZ_GA_Jump::CancelAbility, Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility));
		return;
	}

	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);

	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		if (IAZ_JumpRequester* Requester = Cast<IAZ_JumpRequester>(ActorInfo->AvatarActor.Get()))
		{
			Requester->SetJumpPressed(false);
		}
	}
}

