// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "AZ_GA_Interact.generated.h"

UCLASS()
class AZ_API UAZ_GA_Interact : public UAZ_GameplayAbility
{
	GENERATED_BODY()

public:

	UAZ_GA_Interact();

protected:

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Interact")
	float InteractCooldownDuration = 2.f;

private:

	FTimerHandle CooldownTimerHandle;

	void OnCooldownFinished();
};
