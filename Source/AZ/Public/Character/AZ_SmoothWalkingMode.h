// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/SmoothWalkingMode.h"
#include "AZ_SmoothWalkingMode.generated.h"

/**
 * GASP-parity Walking mode for AZ. Mirrors BP_MovementMode_Walking from
 * GameAnimationSample but reads FAZ_MoverCustomInputs (AZ's USTRUCT) instead of
 * GASP's S_MoverCustomInputs UDS, which AZ's pawn never writes.
 *
 * Per-tick behavior (driven by Gait + RotationOffset from FAZ_MoverCustomInputs):
 *   - MaxSpeedOverride: Walk/Run/Sprint/Crouch
 *   - Acceleration:    Walk/Run base, +Sprint when speed > RunSpeed
 *   - Deceleration:    Stopping vs gait-change vs sticky-landing (20000 for 0.2s after Falling)
 *   - TurningStrength: speed-mapped Walk/Run -> Sprint
 *   - FacingSmoothingTime: gait-mapped + camera-snap shorten when |Δfacing| > 90°
 *   - DesiredFacing: rotated by RotationOffset, clamped to ±179° around prior offset
 *
 * CDO defaults match BP_MovementMode_Walking: bSmoothFacingWithDoubleSpring=false,
 * FacingSmoothingTime=0.5s baseline. Per-frame BP overrides take effect inside
 * GenerateWalkMove_Implementation before calling Super.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "AZ Smooth Walking Mode"))
class AZ_API UAZ_SmoothWalkingMode : public USmoothWalkingMode
{
	GENERATED_BODY()

public:
	UAZ_SmoothWalkingMode();

	virtual void GenerateWalkMove_Implementation(FMoverTickStartData& StartState, float DeltaSeconds,
		const FMoverSimContext& SimContext, const FVector& DesiredVelocity, const FQuat& DesiredFacing,
		const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity) override;

	virtual void OnRegistered(const FName ModeName, const FMoverSimContext& SimContext) override;
	virtual void OnUnregistered(const FMoverSimContext& SimContext) override;

	// ---- Speeds (cm/s) — GASP CDO defaults from gasp_movement_modes.md ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Speeds", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float WalkSpeed = 165.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Speeds", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float RunSpeed = 375.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Speeds", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SprintSpeed = 585.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Speeds", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float CrouchSpeed = 200.f;

	// ---- Accelerations (cm/s²) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Accel", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float WalkAcceleration = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Accel", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float RunAcceleration = 800.f;

	/** Layered ON TOP of RunAcceleration when current speed already exceeds RunSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Accel", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float SprintAcceleration = 300.f;

	// ---- Decelerations (cm/s²) ----
	// 6000 chosen so a Run-entry stop (~400 cm/s) decays in ~65 ms — capsule plants
	// well before the foot plant of the RTG_RM_*Stop_* clips (~400-500 ms), letting
	// OffsetRootBone-Accumulate hide the residual visual offset. Tune in BP CDO if
	// the catch-up swing reads as too snappy.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Decel", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float StoppingDeceleration = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Decel", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float GaitChangeDeceleration = 300.f;

	/** Sticky-landing brake — applied for JustLandedDuration after Falling -> Walking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Decel", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float JustLandedDeceleration = 20000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Decel", meta = (ClampMin = "0", ForceUnits = "s"))
	float JustLandedDuration = 0.2f;

	// ---- Turning (lateral velocity steer) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Turning", meta = (ClampMin = "0"))
	float WalkRunTurnStrength = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Turning", meta = (ClampMin = "0"))
	float SprintTurnStrength = 4.0f;

	// ---- Facing smoothing (capsule yaw) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing", meta = (ClampMin = "0", ForceUnits = "s"))
	float WalkRunFacingTime = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing", meta = (ClampMin = "0", ForceUnits = "s"))
	float SprintFacingTime = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing", meta = (ClampMin = "0", ForceUnits = "s"))
	float IdleFacingTime = 0.2f;

protected:
	UFUNCTION()
	void HandleMovementModeChanged(const FName& Previous, const FName& Next);

	void OnJustLandedTimerExpired();

	/** Set true when previous mode was Falling and this mode just activated; cleared after JustLandedDuration. */
	UPROPERTY(Transient)
	bool bJustLanded = false;

	/** Cached at OnRegistered — used to detect "this mode just became active" in the delegate. */
	UPROPERTY(Transient)
	FName MyModeName = NAME_None;

	FTimerHandle JustLandedTimerHandle;

	/** Persists across ticks — used to clamp RotationOffset to ±179° around the prior frame's offset
	 *  so the spring-damper always picks the short arc toward DesiredFacing. */
	UPROPERTY(Transient)
	double CachedRotationOffsetDegrees = 0.0;
};
