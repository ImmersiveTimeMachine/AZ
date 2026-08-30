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

	/** WaitInputRelease task callback. Releases the PRESS only — it does NOT end the ability. Holding the
	 *  press past the release would keep CMC applying jump velocity for JumpMaxHoldTime and re-jump the
	 *  moment the character lands (bPressedJump is still set), so the press must drop on release; but the
	 *  ability lives until the landing finishes (see OnLandComplete).
	 *  The task, not the raw InputReleased virtual, is what replicates the release to the server instance:
	 *  the ASC invokes the virtual only on the owning client, so a LocalPredicted server instance stayed
	 *  Active forever and rejected every later jump from a remote client (audit P1-8). */
	UFUNCTION()
	void OnJumpInputReleased(float TimeHeld);

	/** The jump animation cycle (takeoff -> air -> land) handed the body back — the landing has finished
	 *  and the ability may end, releasing Movement.Jumping so the next jump can activate. The anim
	 *  instance owns this fact; the ability never guesses a recovery duration. */
	UFUNCTION()
	void OnLandComplete(FGameplayEventData Payload);

	/** Watchdog: events drive, timers guard. If the completion event never arrives — no jump animation
	 *  played, the pawn was teleported, movement mode changed under us — the ability must not stay Active
	 *  forever, because Movement.Jumping would then block every future jump. Generous by design: this is a
	 *  failsafe, not the normal end path. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Jump", meta = (ClampMin = "0.5", ForceUnits = "s"))
	float LandWatchdogSeconds = 5.f;

private:
	void OnLandWatchdogExpired();

	UPROPERTY()
	class UAbilityTask_WaitGameplayEvent* WaitLandTask = nullptr;

	FTimerHandle LandWatchdogTimer;

protected:

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
};
