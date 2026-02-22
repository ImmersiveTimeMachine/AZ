// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/AbilityTasks/AZ_AT_WaitDelayOneFrame.h"

UAZ_AT_WaitDelayOneFrame::UAZ_AT_WaitDelayOneFrame(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAZ_AT_WaitDelayOneFrame::Activate()
{
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UAZ_AT_WaitDelayOneFrame::OnDelayFinish);
}

UAZ_AT_WaitDelayOneFrame* UAZ_AT_WaitDelayOneFrame::WaitDelayOneFrame(UGameplayAbility* OwningAbility)
{
	auto* MyObj = NewAbilityTask<UAZ_AT_WaitDelayOneFrame>(OwningAbility);
	return MyObj;
}

void UAZ_AT_WaitDelayOneFrame::OnDelayFinish()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFinish.Broadcast();
	}
	EndTask();
}
