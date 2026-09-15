// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "AZ_GA_Traversal.generated.h"

class UAnimMontage;
class UAZ_AT_PlayMontageAndWaitForEvent;
class UPrimitiveComponent;

/** One warp target to register for the duration of a traversal action, by the name its montage windows
 *  rendezvous with. Mantle needs a single FrontLedge; hurdle needs FrontLedge + BackLedge + BackFloor. */
USTRUCT()
struct FAZ_TraversalWarpTarget
{
	GENERATED_BODY()

	UPROPERTY()
	FName Name = NAME_None;

	UPROPERTY()
	FTransform Transform;
};

/**
 * Everything the shared executor needs to run one traversal. The ACTION decides all of it; the executor
 * decides none of it. That split is the point: adding hurdle must not mean a second copy of the
 * root-motion, collision and cleanup code that took several PIE sessions to get right.
 */
USTRUCT()
struct FAZ_TraversalRequest
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UAnimMontage> Montage = nullptr;

	/** Montage time to begin from — chosen so the authored approach still ahead matches the real distance
	 *  to the obstacle, instead of making warping compress it. */
	UPROPERTY()
	float StartTime = 0.f;

	/** Successful release only: Walking for a supported traversal, Falling for an explicitly opted-in
	 *  climb-and-drop. Failure recovery does not inherit this choice. Only these two modes are accepted. */
	UPROPERTY()
	FName SuccessMovementMode = FName("Walking");

	UPROPERTY()
	TArray<FAZ_TraversalWarpTarget> WarpTargets;

	/** The ONE primitive the capsule may pass through for the duration. Scoped deliberately: the traversal
	 *  movement mode does not slide on a blocking hit, so without this the capsule pins against the very
	 *  face it is crossing — but disabling collision wholesale would let it through everything else too. */
	UPROPERTY()
	TWeakObjectPtr<UPrimitiveComponent> CollisionExemption;

	/** Names the montage task instance, purely for debugging. */
	UPROPERTY()
	FName TaskName = FName("Traversal");

	bool IsValid() const { return Montage != nullptr; }
};

/**
 * UAZ_GA_Traversal — the SHARED execution path for contextual traversal (mantle today, hurdle next,
 * vault/climb later). Subclasses describe the action; this class runs it.
 *
 * It owns, and subclasses must not duplicate: warp-target registration and teardown, the Traversing
 * movement mode, the generation-scoped root-motion drive, the scoped collision exemption, the montage
 * task and its four distinct outcomes, the watchdog, and one idempotent release.
 *
 * ORDER IS LOAD-BEARING and lives here precisely so it cannot be got wrong per-action:
 *   - warp targets are registered BEFORE the montage, because a window can open on frame 0 and a target
 *     registered afterwards is a silent no-op for that window;
 *   - the movement mode changes before the drive, because Walking's floor-snap eats vertical motion;
 *   - on release the root-motion drive is dropped FIRST, so a montage still blending out is cosmetic and
 *     cannot keep moving the capsule;
 *   - the collision exemption is restored BEFORE anything queues Walking, because the surface the capsule
 *     must now stand on is the component it was passing through.
 */
UCLASS(Abstract)
class AZ_API UAZ_GA_Traversal : public UAZ_GameplayAbility
{
	GENERATED_BODY()

public:
	UAZ_GA_Traversal();

protected:
	/** Blocks on itself (one traversal at a time) and on the states that own the body outright. */
	virtual void DeclareAbilityTags() override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/**
	 * THE action-specific hook. Fill in which montage, from what time, which warp targets and which
	 * primitive to pass through. Return false to abort cleanly (the press is already consumed by then).
	 * Geometry validation belongs to the action, not here — hurdle needs a far-side landing, mantle needs
	 * support on top, and this class is deliberately ignorant of both.
	 */
	virtual bool BuildTraversalRequest(FAZ_TraversalRequest& OutRequest);

	/** ANIM-LED SUCCESS: the clip reached its release point. Cuts the authored tail and uses the request's exit mode. */
	UFUNCTION()
	void OnHandoffEvent(FGameplayTag EventTag, FGameplayEventData EventData);

	/** The montage reached its own end with no handoff marker — a standing mantle's normal completion,
	 *  and the safety net for a moving clip whose marker is missing. */
	UFUNCTION()
	void OnMontagePlayedOut(FGameplayTag EventTag, FGameplayEventData EventData);

	/** FAILURE, kept separate: another montage displaced ours, or we were cancelled (death, grab). */
	UFUNCTION()
	void OnMontageAborted(FGameplayTag EventTag, FGameplayEventData EventData);

	/** Blend used when the handoff cuts the authored tail. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Traversal", meta = (ClampMin = "0", ForceUnits = "s"))
	float HandoffBlendOutTime = 0.25f;

	/** Events drive, timers guard. Added to the remaining montage length. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Traversal", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float WatchdogPadding = 1.f;

private:
	void OnWatchdogExpired();

	/** One idempotent, owner-aware release. Never touches velocity: the next mode inherits the clip's
	 *  realized speed. Success uses the validated request mode; failure keeps Walking's physical recovery. */
	void ReleaseTraversal(bool bSuccessfulHandoff = false);

	UPROPERTY()
	TObjectPtr<UAZ_AT_PlayMontageAndWaitForEvent> MontageTask = nullptr;

	/** Ours, not "the" drive: release is a no-op once a newer owner has superseded us, so a late callback
	 *  here can never cancel a successor's motion. */
	uint64 RootMotionGeneration = 0;

	/** Copied from the request before movement starts, reset on every activation. */
	FName SuccessMovementMode = FName("Walking");

	/** Every warp target this action registered, so teardown removes exactly those and no others. */
	TArray<FName> RegisteredWarpTargets;

	TWeakObjectPtr<UPrimitiveComponent> IgnoredTargetComponent;

	FTimerHandle WatchdogTimer;

	bool bCleanedUp = false;
	bool bHandedOff = false;
};
