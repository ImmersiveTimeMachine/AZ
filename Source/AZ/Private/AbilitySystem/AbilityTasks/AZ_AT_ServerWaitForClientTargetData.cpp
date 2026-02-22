// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/AbilityTasks/AZ_AT_ServerWaitForClientTargetData.h"

#include "AbilitySystemComponent.h"

UAZ_AT_ServerWaitForClientTargetData::UAZ_AT_ServerWaitForClientTargetData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UAZ_AT_ServerWaitForClientTargetData* UAZ_AT_ServerWaitForClientTargetData::ServerWaitForClientTargetData(UGameplayAbility* OwningAbility, FName TaskInstanceName, bool TriggerOnce)
{
	auto* NewTask = NewAbilityTask<UAZ_AT_ServerWaitForClientTargetData>(OwningAbility, TaskInstanceName);
	NewTask->bTriggerOnce = TriggerOnce;
	return NewTask;
}

void UAZ_AT_ServerWaitForClientTargetData::Activate()
{
	if (!Ability || !Ability->GetCurrentActorInfo()->IsNetAuthority())
	{
		return;
	}

	const FGameplayAbilitySpecHandle SpecHandle = GetAbilitySpecHandle();
	const FPredictionKey ActivationPredictionKey = GetActivationPredictionKey();
	AbilitySystemComponent->AbilityTargetDataSetDelegate(SpecHandle, ActivationPredictionKey).AddUObject(this, &UAZ_AT_ServerWaitForClientTargetData::OnTargetDataReplicatedCallback);
}

void UAZ_AT_ServerWaitForClientTargetData::OnTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ActivationTag)
{
	FGameplayAbilityTargetDataHandle MutableData = Data;
	AbilitySystemComponent->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey());

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		ValidData.Broadcast(MutableData);
	}

	if (bTriggerOnce)
	{
		EndTask();
	}
}

void UAZ_AT_ServerWaitForClientTargetData::OnDestroy(bool AbilityEnded)
{
	if (AbilitySystemComponent.IsValid())
	{
		const FGameplayAbilitySpecHandle SpecHandle = GetAbilitySpecHandle();
		const FPredictionKey ActivationPredictionKey = GetActivationPredictionKey();
		AbilitySystemComponent->AbilityTargetDataSetDelegate(SpecHandle, ActivationPredictionKey).RemoveAll(this);
	}

	Super::OnDestroy(AbilityEnded);
}
