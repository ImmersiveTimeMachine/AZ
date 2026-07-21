// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "AZ_GA_Death.generated.h"

class UAZ_AT_PlayMontageAndWaitForEvent;

/**
 * The death ability — GAS-proper death per the rail doctrine: triggered by the Event.Death gameplay
 * event the vitals set fires at 0 HP (payload carries the killer), cancels every other ability
 * (a mid-swing death interrupts the attack correctly), plays the directional death montage through
 * the SAME montage task as the punch (so it REPLICATES to clients), then hands the avatar its
 * corpse-ification (shutdown + ragdoll at the fall's impact beat).
 *
 * Montage source = the avatar's anim-set DA (DeathMontage_F/B/L/R — per-variant deaths). Direction
 * convention: property = the side the killing blow arrived at (hit from the front -> falls backward).
 *
 * NOTE trigger wiring: native tags register in the AssetManager AFTER CDOs construct, so AbilityTriggers
 * cannot be set in the constructor — grant sites must call ConfigureTriggerOnCDO() first (runtime,
 * registry ready, idempotent).
 */
UCLASS()
class AZ_API UAZ_GA_Death : public UAZ_GameplayAbility
{
	GENERATED_BODY()

public:
	UAZ_GA_Death();

	/** Idempotent CDO patch: adds the Event.Death GameplayEvent trigger. Call before GiveAbility. */
	static void ConfigureTriggerOnCDO();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** Montage progress fraction at which the corpse hands off to ragdoll (the fall's impact beat). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Death", meta = (ClampMin = "0", ClampMax = "1"))
	float RagdollAtMontageFraction = 0.6f;

	UPROPERTY() UAZ_AT_PlayMontageAndWaitForEvent* MontageTask = nullptr;
};
