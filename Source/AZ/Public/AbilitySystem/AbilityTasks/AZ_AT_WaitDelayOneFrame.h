// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AZ_AT_WaitDelayOneFrame.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitDelayOneFrameDelegate);

UCLASS()
class AZ_API UAZ_AT_WaitDelayOneFrame : public UAbilityTask
{
	GENERATED_UCLASS_BODY()
	
	UPROPERTY(BlueprintAssignable)
	FWaitDelayOneFrameDelegate OnFinish;

	virtual void Activate() override;

	// Wait one frame.
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAZ_AT_WaitDelayOneFrame* WaitDelayOneFrame(UGameplayAbility* OwningAbility);

private:
	void OnDelayFinish();
};
