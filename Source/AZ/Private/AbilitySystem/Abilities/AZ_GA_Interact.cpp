// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/AZ_GA_Interact.h"

#include "Player/AZ_PlayerController.h"


UAZ_GA_Interact::UAZ_GA_Interact()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UAZ_GA_Interact::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AAZ_PlayerController* PC = Cast<AAZ_PlayerController>(ActorInfo->PlayerController.Get());
	if (!PC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PC->PrimaryInteract();

	// Delay before ending (mirrors the BP's 2s delay)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CooldownTimerHandle,
			this,
			&UAZ_GA_Interact::OnCooldownFinished,
			InteractCooldownDuration,
			false
		);
	}
}

void UAZ_GA_Interact::OnCooldownFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
