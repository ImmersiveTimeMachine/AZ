// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "AZ_GA_PawnJump.generated.h"

/**
 * Mover-aware jump ability for v2 pawns.
 *
 * Gates the jump request through GAS (tags, cost, cooldown, conditions) and dispatches
 * the press/release to the pawn via IAZ_JumpRequester. The pawn flips its cached Mover
 * input flag; FCharacterDefaultInputs.bIsJumpPressed rides the deterministic InputCmd
 * into NetworkPrediction. Movement prediction stays in Mover — GAS is only the gate.
 *
 * Distinct from UAZ_GA_Jump, which targets the legacy ACharacter+CMC pawn (calls
 * ACharacter::Jump() / StopJumping()). UAZ_GA_PawnJump targets any pawn implementing
 * IAZ_JumpRequester — hero today, future on-foot pawn classes that opt in.
 *
 * InputTag: Input.Action.Jump (set in the BP child or via DefaultObject).
 */
UCLASS()
class AZ_API UAZ_GA_PawnJump : public UAZ_GameplayAbility
{
	GENERATED_BODY()

public:
	UAZ_GA_PawnJump();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		OUT FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void InputReleased(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
};
