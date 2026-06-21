// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/SmoothWalkingMode.h"
#include "AZ_PawnMovementMode_Walking.generated.h"

struct FMoverTickStartData;
struct FMoverEventContext;
struct FMoverSyncState;
struct FMoverAuxStateContext;
struct FMoverTimeStep;
struct FProposedMove;
enum class EAZ_Gait : uint8;

UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "AZ Pawn Movement Mode - Walking"))
class AZ_API UAZ_PawnMovementMode_Walking : public USmoothWalkingMode
{
	GENERATED_BODY()

public:
	UAZ_PawnMovementMode_Walking();

	virtual void GenerateWalkMove_Implementation(FMoverTickStartData& StartState, float DeltaSeconds,
		const FMoverSimContext& SimContext, const FVector& DesiredVelocity, const FQuat& DesiredFacing,
		const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity) override;

	/** Compute the desired facing for this tick. Default implementation pulls FAZ_MoverCustomInputs
	 *  from StartState.InputCmd.InputCollection and applies RotationOffset to DesiredFacing, clamped
	 *  to ±RotationOffsetClampDegrees around the prior frame's cached offset so the spring damper
	 *  always picks the short arc.
	 *
	 *  Override on AI/vehicle subclasses to return a different rotation target (nav heading,
	 *  aim target, steering yaw, etc) by pulling whichever input struct that subclass uses out
	 *  of the same InputCollection — without rewriting the gait/decel/facing tuning math. */
	virtual FQuat ResolveRotationTarget(const FMoverTickStartData& StartState,
		const FQuat& DesiredFacing, const FQuat& CurrentFacing) const;

	/** Compute the gait for this tick (drives Max Speed, Acceleration). Default implementation
	 *  pulls FAZ_MoverCustomInputs from StartState.InputCmd.InputCollection and returns its Gait
	 *  field (defaulting to Walk when absent).
	 *
	 *  Override on AI subclasses to return gait from the AIController's blackboard or behavior
	 *  tree state. Vehicle subclasses should not inherit from this mode — gait is meaningless. */
	virtual EAZ_Gait ResolveGait(const FMoverTickStartData& StartState) const;

	// ---- Speeds (cm/s) — GASP CDO defaults ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Speeds", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float WalkSpeed = 165.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Speeds", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float RunSpeed = 375.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Speeds", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SprintSpeed = 585.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Speeds", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float CrouchSpeed = 200.f;

	// ---- Accelerations (cm/s²) — GASP CDO defaults ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Accel", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float WalkAcceleration = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Accel", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float RunAcceleration = 800.f;

	/** Layered ON TOP of RunAcceleration when current speed already exceeds RunSpeed.
	 *  This lets Walk→Run accel be tuned separately from Run→Sprint accel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Accel", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float SprintAcceleration = 300.f;

	// ---- Decelerations (cm/s²) ----
	/** Applied when MoveInput is zero (player let go of the stick). 6000 chosen so a Run-entry
	 *  stop (~400 cm/s) decays in ~65 ms — capsule plants well before the foot plant of the
	 *  RTG_RM_*Stop_* clips (~400-500 ms), letting OffsetRootBone-Accumulate hide the residual
	 *  visual offset. Tune in BP CDO if the catch-up swing reads as too snappy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Decel", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float StoppingDeceleration = 6000.f;

	/** Applied when MoveInput is non-zero but current speed exceeds the target gait speed
	 *  (e.g. Sprint→Run, Run→Walk). Low value so gait transitions glide rather than slam. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Decel", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float GaitChangeDeceleration = 300.f;

	/** Sticky-landing brake — applied for JustLandedDuration after Falling → this mode. Kills
	 *  jump skid so locomotion anims don't think we're still mid-stride after landing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Decel", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float JustLandedDeceleration = 20000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Decel", meta = (ClampMin = "0", ForceUnits = "s"))
	float JustLandedDuration = 0.2f;

	// ---- Turning (lateral velocity steer — not capsule rotation) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Turning", meta = (ClampMin = "0"))
	float WalkRunTurnStrength = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Turning", meta = (ClampMin = "0"))
	float SprintTurnStrength = 4.0f;

	// ---- Facing smoothing (capsule yaw spring-damper time) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing", meta = (ClampMin = "0", ForceUnits = "s"))
	float WalkRunFacingTime = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing", meta = (ClampMin = "0", ForceUnits = "s"))
	float SprintFacingTime = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing", meta = (ClampMin = "0", ForceUnits = "s"))
	float IdleFacingTime = 0.2f;

	// ---- Strafe (combat-ready) facing ----
	/** Strafe aim-lock facing time = CMC's RotationRate analog — how tightly the body continuously tracks the
	 *  camera (idle and moving; engine-standard bUseControllerDesiredRotation). Lower = snappier/more rigid (→0
	 *  ≈ instant bUseControllerRotationYaw); higher = a softer, laggier follow. BP-tunable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing|Strafe", meta = (ClampMin = "0", ForceUnits = "s"))
	float StrafeFacingTime = 0.10f;

	// ---- Rotation-offset clamp ----
	/** Max delta (degrees) the new RotationOffset can move per tick away from the prior frame's
	 *  cached offset. 179° guarantees the spring damper always picks the short arc — without
	 *  this, a 179→-179° flip in input would visibly snap the capsule the wrong way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	double RotationOffsetClampDegrees = 179.0;

	// ---- Camera-snap protection ----
	/** Below this |Δfacing| no shortening is applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing|CameraSnap", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float CameraSnapShortenStartAngle = 90.f;

	/** At or above this |Δfacing| the full CameraSnapShortenMaxSeconds is subtracted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing|CameraSnap", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float CameraSnapShortenFullAngle = 135.f;

	/** Max amount subtracted from FacingSmoothingTime when |Δfacing| reaches the full angle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing|CameraSnap", meta = (ClampMin = "0", ForceUnits = "s"))
	float CameraSnapShortenMaxSeconds = 0.2f;

	// ---- Sticky-landing source ----
	/** Movement mode name that triggers the JustLanded brake when transitioning INTO this mode.
	 *  Default "Falling" matches the engine FallingMode / UAZ_FallingMode registration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Decel")
	FName FallingModeName = FName(TEXT("Falling"));

	/** Sticky-landing latch, SIM-TIME based: records the landing sim time when activated from the Falling
	 *  mode. The old version latched a bool via a game-thread delegate + FTimerManager — wall-clock (wrong
	 *  under time dilation), not rollback-aware, and the timer survived unrelated mode churn (audit P1-14). */
	virtual void Activate(const FMoverEventContext& Context, FName PrevModeName, const FMoverSimContext& SimContext,
		const FMoverTickStartData& StartState, FMoverSyncState* OutSyncState, FMoverAuxStateContext* OutAuxState) override;

	/** Derives bJustLanded from sim time (TimeStep.BaseSimTimeMs vs LandedSimTimeMs) before the walk-move
	 *  math runs, then defers to Super (which calls GenerateWalkMove). */
	virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState,
		const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

protected:
	/** True within JustLandedDuration (sim seconds) of a Falling→Walking activation. Derived each
	 *  GenerateMove from LandedSimTimeMs — mutable because GenerateMove is const; plain member (sim
	 *  scratch, no GC ref). */
	mutable bool bJustLanded = false;

	/** Sim time (ms, server timespace) of the last Falling→Walking activation; -1 = never landed. */
	double LandedSimTimeMs = -1.0;

	/** Persists across ticks — used to clamp RotationOffset to ±179° around the prior frame's
	 *  offset so the spring damper always picks the short arc toward DesiredFacing. */
	UPROPERTY(Transient)
	double CachedRotationOffsetDegrees = 0.0;
};
