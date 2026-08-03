// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GA_MeleeAttack.h"
#include "AZ_GA_ZombieMelee.generated.h"

/**
 * The Chalkie's claw swipe — the SAME melee rail as the hero's punch (weapon parity by construction):
 * identical hit window, sweep, GE_Damage flow. Differences are pure data/selection:
 *  - montages come from the pawn's anim-set DA (AttackMontage_L/R fields — per-variant swings),
 *  - the pack's attack clips are LONG sustained-clawing cycles, so one activation plays a BITE of the
 *    cycle, ended by the Event.Combat.BeatEnd notify authored on the montage (the timeline is the
 *    clock — arch step A); blend-out smooths the exit, the BT re-triggers for the next swing,
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
	/** A reeling Chalkie must not be ABLE to swing, and that is a property of the ATTACK, not of the tree
	 *  that happens to call it. The BT gate is a decision taken once in ExecuteTask, so every re-entry of
	 *  the Press branch (the 0.5s chase-breather loop) gets a fresh chance to slip through — and one that
	 *  does starts an attack montage that takes DefaultSlot away from the knockback mid-recoil. This is
	 *  checked on EVERY activation attempt, so the loop cannot race it; the tree gate stays as the cheap
	 *  early-out.
	 *
	 *  Written as an explicit check rather than ActivationBlockedTags because those are read off the
	 *  ability INSTANCE (AbilitySystemComponent_Abilities.cpp:1798 — CanActivateAbility runs on
	 *  InstancedAbility when there is one), and an instance does NOT inherit a CDO patched at runtime.
	 *  Native gameplay tags are not registered when CDO constructors run, so the container cannot be
	 *  filled in the ctor either — which leaves this. */
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual UAnimMontage* SelectMontage() const override;

	// (BiteSeconds deleted, arch step A: the bite beat is the Event.Combat.BeatEnd notify authored on
	//  each attack montage — per-clip, timeline-anchored. Tune it by moving the notify.)
};
