// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "AZ_GA_Mantle.generated.h"

class UAbilityTask_PlayMontageAndWait;

/**
 * UAZ_GA_Mantle — executes one validated mantle: warp target, Traversing mode, FullBody montage, and a
 * generation-scoped root-motion drive, then hands the capsule back.
 *
 * NOT input-bound. UAZ_GA_PawnJump owns Input.Action.Jump and routes to this via
 * UAZ_TraversalComponent::TryStartMantle. Two abilities sharing one input tag would both try to activate
 * on the same press, and which one won would depend on grant order.
 *
 * ORDER IS LOAD-BEARING in ActivateAbility:
 *   1. warp target BEFORE the montage — the first window opens 0.141s in and a target registered after
 *      it opens is a silent no-op for that window (they rendezvous by FName, and a miss is not an error);
 *   2. Traversing mode before the drive — Walking's floor-snap would eat the vertical lift;
 *   3. the montage, then DriveRootMotion. A montage alone animates the mesh in place on a Mover pawn.
 */
UCLASS()
class AZ_API UAZ_GA_Mantle : public UAZ_GameplayAbility
{
	GENERATED_BODY()

public:
	UAZ_GA_Mantle();

protected:
	/** Blocks on itself (one traversal at a time) and on the states that own the body outright. */
	virtual void DeclareAbilityTags() override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** The single cleanup path — completion, interruption, watchdog, death and cancel all land here. */
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	UFUNCTION()
	void OnMontageEnded();

	/** Must match the warp windows authored on the montages. A mismatch is inert, not an error. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Mantle")
	FName WarpTargetName = FName("FrontLedge");

	/** Lift off the lip so the target is not exactly coplanar with the surface. GASP authored 0.5cm. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Mantle", meta = (ForceUnits = "cm"))
	float WarpTargetZOffset = 0.5f;

	/** Events drive, timers guard. Added to the montage length; only reached if no montage callback ever
	 *  arrives, which would otherwise strand State.Traversing and block every future mantle. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Mantle", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float WatchdogPadding = 1.f;

private:
	void OnWatchdogExpired();

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask = nullptr;

	/** Our drive, not "the" drive: ReleaseRootMotion is a no-op once a newer owner has superseded us, so a
	 *  late callback here can never cancel a successor's motion. */
	uint64 RootMotionGeneration = 0;

	FTimerHandle WatchdogTimer;

	/** The one obstacle component the capsule is allowed to pass through for the duration. Scoped to
	 *  exactly the primitive the detector validated — collision with everything else stays on, and the
	 *  exemption is lifted before the capsule is handed back to Walking to find its floor. */
	TWeakObjectPtr<class UPrimitiveComponent> IgnoredTargetComponent;

	/** Cleanup is idempotent — EndAbility can be re-entered (task callback plus GAS teardown). */
	bool bCleanedUp = false;
};
