// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"   // our base, NOT UGameplayAbility
#include "AZ_GA_MeleeAttack.generated.h"

class UAZ_AT_PlayMontageAndWaitForEvent;

// Which hand threw it — latched at activation from the InputTag.
UENUM(BlueprintType)
enum class EAZ_MeleeHand : uint8 { Left, Right };

UCLASS()
class AZ_API UAZ_GA_MeleeAttack : public UAZ_GameplayAbility
{
	GENERATED_BODY()

public:
	UAZ_GA_MeleeAttack();
	
protected:
	
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;
	
	virtual void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	
	// The selector. First pass: hardcoded switch over the 4 fist clips. CHT_Melee later.
	UAnimMontage* SelectMontage() const;

	// Montage finished / interrupted / cancelled -> end the ability.
	UFUNCTION()
	void OnMontageFinished(FGameplayTag EventTag, FGameplayEventData EventData);

	// Hit-window (and later combo-window) GameplayEvents from notifies. Stub for now.
	UFUNCTION()
	void OnMontageEvent(FGameplayTag EventTag, FGameplayEventData EventData);

	// --- Tunables: assign the 4 fist montages in a BP child (like GA_Shoot's FireMontage*) ---
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Animation") UAnimMontage* PunchIdle_L = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Animation") UAnimMontage* PunchIdle_R = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Animation") UAnimMontage* PunchMove_L = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Animation") UAnimMontage* PunchMove_R = nullptr;

	// The tag the hit-window notify will send (matched in OnMontageEvent). Wire later.
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee") FGameplayTag HitWindowEventTag;

	// --- Runtime state, latched at activation ---
	UPROPERTY() UAZ_AT_PlayMontageAndWaitForEvent* MontageTask = nullptr;
	UPROPERTY(EditDefaultsOnly) EAZ_MeleeHand Hand = EAZ_MeleeHand::Left;
	bool bIsMovingLatched = false;
	FGameplayTag ProfileTag;          // current Weapon.* — used by SelectMontage when CHT lands
	int32 ComboIndex = 0;             // later
	
};
