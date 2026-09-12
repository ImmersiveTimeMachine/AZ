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
	 *  RTG_RM_*Stop_* clips (~400-500 ms). Tune in BP CDO if the catch-up swing reads as too snappy.
	 *  NOTE (2026-09-07): an earlier version of this comment credited "OffsetRootBone-Accumulate" with hiding
	 *  the residual offset. That described the v1 UAZ_AnimInstance graph (OFR + Steering); the Mover/MHC hero
	 *  has no working OffsetRootBone and the stop clip is RM-driven through the transition move
	 *  (UAZ_MoverAnimInstance RM bridge), so this value only governs the frames before that move is live and
	 *  after it expires. See docs/design-briefs/offsetrootbone-mover-port-plan.md. */
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

	/** Strafe BIG-TURN facing time — used when the body is far off the camera at a move-start (the angle band that
	 *  triggers the 90/135/180 turn-start CLIPS). The spring time is ramped from StrafeFacingTime (small angle,
	 *  snappy aim-lock) up to this (large angle) so the body turns over roughly the turn-start clip's DURATION
	 *  instead of snapping in ~0.3s and leaving the clip to play out desynced. Higher = the body turn matches a
	 *  longer/heavier clip; set == StrafeFacingTime to disable the ramp (uniform snappy turn). BP-tunable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing|Strafe", meta = (ClampMin = "0", ForceUnits = "s"))
	float StrafeTurnFacingTime = 0.50f;

	// ---- Aiming (firearm ADS) facing ----
	/** Aiming facing time — FLAT, no angle ramp, unlike strafe. Aiming used to share the strafe ramp, so a large
	 *  body→camera angle spent StrafeTurnFacingTime (0.5s) turning. That ramp exists to pace the body against a
	 *  90/135/180 TURN-START CLIP; while aiming there is none — the aim turn-in-place clips
	 *  (Stand_Aim_Turn_In_Place_L/R) exist in the retargeted set but no chooser row plays them, and the SM now
	 *  forces a plain forward start while aiming (FAZ_LocoSMInputs::bIsAiming). So the slow band was pacing
	 *  against an animation that never comes: pure lag, and the source of the 2026-09-11 "bound" feel.
	 *
	 *  Low is the point here: with an aim free-look cone the body only moves once the camera pushes PAST the cone,
	 *  and then it should arrive immediately so the pawn keeps sitting on the cone edge. Still a spring, not a
	 *  snap, so the mesh never teleports. Raise toward StrafeFacingTime if the turn reads too mechanical. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing|Aim", meta = (ClampMin = "0", ForceUnits = "s"))
	float AimFacingTime = 0.07f;

	/** AIM TURN-IN-PLACE pacing rate (deg/s). At REST the aim body still tracks the camera, so without a stepping
	 *  clip the capsule spins under a standing idle and the feet slide. The SM hands a large at-rest delta to
	 *  IdleTurnLeft/Right, whose chooser rows push the aim turn-in-place LOOP; this makes the facing spring turn at
	 *  roughly that clip's angular rate so the feet stay with the turn. 67 deg/s is the authored rate of
	 *  AZ_RTG_MH_W2_Stand_Aim_Turn_In_Place_L/R_Loop (45 deg over 0.67 s) — raising it turns faster but the feet
	 *  start to slip, until the clip's play rate is driven from this too. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing|Aim", meta = (ClampMin = "1", ForceUnits = "deg/s"))
	float AimTurnInPlaceRateDegPerSec = 67.f;

	/** Enter/exit the paced at-rest turn. ★ MUST MATCH the constants of the same name in
	 *  AZ_LocomotionStateMachine.cpp — the mode decides when to PACE, the SM decides when to show the CLIP, and
	 *  both read the same body→camera angle. Drift gives a paced turn with no clip (reads as lag) or a clip with
	 *  a snapping body. Enter > Exit is deliberate hysteresis against flicker at the boundary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing|Aim", meta = (ClampMin = "0", ForceUnits = "degrees"))
	float AimTurnInPlaceEnterDeg = 35.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing|Aim", meta = (ClampMin = "0", ForceUnits = "degrees"))
	float AimTurnInPlaceExitDeg = 6.f;

	/** AIM TURN-IN-PLACE master switch. OFF (default): while aiming the body simply tracks the camera with the flat
	 *  AimFacingTime spring, no stepping clip, no rate limit - the behaviour the user signed off as "the pistol works
	 *  very well" (2026-09-11). ON: past AimTurnInPlaceEnterDeg at rest the body turns at AimTurnInPlaceRateDegPerSec
	 *  under the aim turn-in-place loop (SM IdleTurnLeft/Right, chooser rows 304-307 / 397-400). Left in because the
	 *  machinery is deterministic and complete; the stepping loops in both weapon sets rock the hips at any rate
	 *  much above their authored 67-90 deg/s, so it needs purpose-made fast turn clips before it reads well. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing|Aim")
	bool bAimTurnInPlaceEnabled = false;

	/** Max body yaw rate while aiming (deg/s, 0 = unlimited). The flat AimFacingTime spring (0.05 s) is right for
	 *  tracking the camera, but on aim ENTRY from a large angle it is a teleport: recorded 2026-09-11, aim pressed
	 *  with the character facing the camera (171 deg off) -> 137 deg in the first tick, 2328 deg/s, a one-frame
	 *  motion-blur smear. 540 turns 180 deg in ~0.35 s: still fast, but a turn the eye can follow. Rides the
	 *  InputCmd (FAZ_MoverCustomInputs::AimTurnYawRateLimit) like the turn-in-place limit, so it is deterministic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing|Aim", meta = (ClampMin = "0", ForceUnits = "deg/s"))
	float AimMaxYawRateDegPerSec = 540.f;

	// ---- Grabbed facing ----
	/** Facing time while the body is HELD (FAZ_MoverCustomInputs.bGrabbed). A caught hero must be square to
	 *  the grabber before the paired catch clips' first frame — the PoseSearch Interaction search already
	 *  places the pair along the actors' line, and this is the only thing that decides whether the victim's
	 *  yaw gets there in time. Overrides BOTH the strafe ramp (0.10→0.50s by angle, tuned to match turn-start
	 *  CLIPS, which is exactly wrong here: measured 2026-09-02, a hero caught 60° off the line was still
	 *  turning while the catch section played) and the explore idle time. Angle-independent on purpose:
	 *  the grabber's close-in is ~0.15s, so this must converge inside that from any start angle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Walking|Facing", meta = (ClampMin = "0", ForceUnits = "s"))
	float GrabbedFacingTime = 0.04f;

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

	/** Diagnostic mirror of the InputCmd's AimTurnYawRateLimit > 0 (the limit itself is produced in the pawn's
	 *  ProduceInput and shipped in the InputCmd so rollback re-simulation replays it identically). */
	bool bAimTurningInPlace = false;
};
