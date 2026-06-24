// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "AZ_MovementDirectionCapabilityComponent.generated.h"

class UMoverComponent;
class UCapsuleComponent;
struct FHitResult;

/**
 * UAZ_MovementDirectionCapabilityComponent — the "where can I move" query.
 *
 * Answers, for a desired WORLD-space move INTENT, which directions the pawn is actually capable of moving — it
 * clamps the intent to the free direction when a vertical wall is ahead, or CANCELS it (returns ~zero) when the
 * pawn faces straight into a wall. It does this by reusing MOVER'S OWN slide math (UMovementUtils::ComputeSlideDelta)
 * on the normal of a short forward capsule sweep, so the clamped intent matches exactly how the real walking move
 * would resolve against that same wall (no hand-rolled projection that could disagree and jitter).
 *
 * WHY a query (not a tick, not velocity): v2 locomotion is INTENT / trajectory / motion-matching driven
 * (predictive). The pawn calls ConstrainIntent() from ProduceInput (the deterministic Mover input producer) and
 * ships the CLAMPED intent, so BOTH Mover and the AnimInstance consume one reality-aware intent and the anim stays
 * predictive — it never reads resolved velocity, which would regress to reactive old-school locomotion. Running
 * inside ProduceInput makes it replay-identically under NetworkPrediction -> co-op-safe by construction.
 *
 * Distinct from UAZ_ObstacleSensorComponent: the sensor answers "what did I hit and how do I flinch" (cosmetic,
 * anim-facing, local tick); this answers "where can I go" (movement input, deterministic). Same forward geometry,
 * different layer. This query is the reusable foundation for cover / AI steering / ledge-edge later.
 */
UCLASS(ClassGroup = (AZ), meta = (BlueprintSpawnableComponent))
class AZ_API UAZ_MovementDirectionCapabilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAZ_MovementDirectionCapabilityComponent();

	/**
	 * Clamp a desired WORLD-space move intent to the direction the pawn can actually move.
	 * @return the slid tangent along a blocking wall (Mover's ComputeSlideDelta), the intent unchanged if clear,
	 *         or ~zero if facing into the wall (movement impossible -> the pawn idles). Call from ProduceInput.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Movement|Capability")
	FVector ConstrainIntent(const FVector& WorldIntent);

	/** True if moving in WorldDir (XY) for the probe distance is unobstructed by a vertical wall. Pure query. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Movement|Capability")
	bool IsDirectionClear(const FVector& WorldDir) const;

	// ---- Outputs (last ConstrainIntent; for debug + future cover / AI consumers) ----
	/** The last ConstrainIntent hit a wall (intent was clamped or cancelled). */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Movement|Capability|Out")
	bool bBlockedThisQuery = false;
	/** Horizontal normal of the wall the last ConstrainIntent clamped against (zero if none). */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Movement|Capability|Out")
	FVector LastBlockingNormal = FVector::ZeroVector;

	// ---- Params ----
	/** Forward look-ahead of the clamp sweep (cm). Keep SHORTER than the obstacle sensor's trigger distance so the
	 *  sensor fires its impact flinch on the rising edge BEFORE this clamp zeroes the intent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Movement|Capability", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ProbeDistance = 30.f;
	/** Shrink the sweep capsule radius (cm) so resting contact / corners don't trip a false block. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Movement|Capability", meta = (ClampMin = "0", ForceUnits = "cm"))
	float CapsuleInset = 2.f;
	/** Ignore obstacles whose bottom is below this height above the feet (cm) — the pawn STEPS over them. Set to the
	 *  walking mode's MaxStepHeight so walkable steps / curbs aren't clamped (which would block stairs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Movement|Capability", meta = (ClampMin = "0", ForceUnits = "cm"))
	float StepUpClearance = 40.f;
	/** Max |normal.Z| accepted as a vertical wall (rejects floors / ramps so they aren't clamped). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Movement|Capability", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxWallNormalZ = 0.4f;
	/** Cancel the intent (return zero -> idle) when the slid direction agrees with the desired direction by less
	 *  than this (near head-on). Above it, slide along the wall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Movement|Capability", meta = (ClampMin = "0", ClampMax = "1"))
	float MinSlideAlignment = 0.15f;
	/** Sweep with the capsule's own collision profile (matches what actually blocks the pawn). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Movement|Capability")
	bool bUseCapsuleProfile = true;
	/** Fallback channel when bUseCapsuleProfile is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Movement|Capability")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_WorldStatic;
	/** Draw the clamp sweep + the resulting slid intent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Movement|Capability|Debug")
	bool bDrawDebug = false;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient) TWeakObjectPtr<UMoverComponent> MoverComp;
	UPROPERTY(Transient) TWeakObjectPtr<UCapsuleComponent> Capsule;

	/** Forward capsule sweep (raised above StepUpClearance). True + fills OutHit if a vertical, opposing wall blocks. */
	bool SweepForWall(const FVector& WorldDir, FHitResult& OutHit) const;
};
