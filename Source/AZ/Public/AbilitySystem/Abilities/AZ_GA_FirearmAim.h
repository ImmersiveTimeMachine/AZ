#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "AZ_GA_FirearmAim.generated.h"

/** Held aim for the firearm currently selected by the inventory equipment owner. */
UCLASS()
class AZ_API UAZ_GA_FirearmAim : public UAZ_GameplayAbility
{
	GENERATED_BODY()

public:
	UAZ_GA_FirearmAim();
	virtual void DeclareAbilityTags() override;
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	bool bOwnsAimState = false;
	FGuid AimedItemId;
	UFUNCTION() void OnAimInputReleased(float TimeHeld);
};
