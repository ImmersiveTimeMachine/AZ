// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GA_MeleeAttack.h"
#include "AZ_GA_ZombieMelee.generated.h"

/**
 * The Chalkie's claw swipe — the SAME melee rail as the hero's punch (weapon parity by construction):
 * identical hit window, sweep, GE_Damage flow. Differences are pure data/selection:
 *  - montages come from the pawn's anim-set DA (AttackMontage_L/R fields — per-variant swings),
 *  - the pack's attack clips are LONG sustained-clawing cycles, so the ability plays a timed BITE
 *    (BiteSeconds) and ends itself; blend-out smooths the exit, the BT re-triggers for the next swing,
 *  - hand is randomized per activation (visual variety, no input to derive it from).
 * Granted natively on the infected ASC at possess; activated by UAZ_BTTask_ZombieAttack.
 */
UCLASS()
class AZ_API UAZ_GA_ZombieMelee : public UAZ_GA_MeleeAttack
{
	GENERATED_BODY()

public:
	UAZ_GA_ZombieMelee();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual UAnimMontage* SelectMontage() const override;

	/** How much of the long clawing cycle one activation plays before self-ending (one swing-ish). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee", meta = (ClampMin = "0.5"))
	float BiteSeconds = 2.2f;
};
