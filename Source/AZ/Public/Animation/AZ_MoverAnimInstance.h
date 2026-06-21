// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AZ_LocomotionTypes.h"
#include "Animation/TrajectoryTypes.h"
#include "AZ_MoverAnimInstance.generated.h"

class AAZ_PawnMoverHeroCharacter;
class UAZ_PawnMoverComponent;
class UCharacterMoverComponent;
class UPoseSearchDatabase;
class UAnimSequence;
class UAZ_LocomotionStateMachine;

/**
 * UAZ_MoverAnimInstance — v2 AnimInstance for the Mover-driven hero pawn.
 *
 * Single source of state for the v2 anim pipeline. Every tick:
 *   1. NativeUpdateAnimation reads Mover sync state + ASC tags from the owning pawn.
 *   2. Derives SMState (locomotion phase enum) from velocity + last-frame state + Mover events.
 *   3. Populates FAZ_v2_ChooserContext — a single struct the AnimGraph chooser node uses
 *      as its input property to select which anim plays.
 *
 * The ABP (AZ_ABP_MoverAnimInstance) has NO AnimGraph State Machine node — phase tracking
 * lives in this C++ class (the SMState field). See project_v2_architecture.md for the
 * "SM = derived enum, not AnimGraph SM" decision.
 */
UCLASS()
class AZ_API UAZ_MoverAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Single struct exposed to the AnimGraph — the chooser node binds to this as its
	 *  Input Property. Refreshed every tick in NativeUpdateAnimation. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|V2|Anim")
	FAZ_v2_ChooserContext ChooserContext;

	/** BlendStack inputs — written by SetBlendStackAnimFromChooser, read by the BlendStack
	 *  node's internal SequencePlayer via property bindings (BlendStackInputs.Anim, .bLoop, etc.). */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|V2|Anim")
	FAZ_BlendStackInputs BlendStackInputs;

	/** Cached output from the last chooser evaluation. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|V2|Anim")
	FAZ_ChooserOutputs ChooserOutputs;

	/** Shared PoseSearch database of locomotion LOOPS (walk + run). When set, the LocomotionLoop state
	 *  motion-matches across this whole DB instead of the single chooser-picked clip, so MM selects the
	 *  walk-vs-run pose by the ACTUAL trajectory/speed — a seamless, speed-continuous walk<->run gear change.
	 *  Assign in the AZ_ABP_MoverAnimInstance CDO; that creates the asset reference so the DB cooks into a
	 *  packaged build (a raw LoadObject would leave it orphaned). Null = fall back to the chooser's direct clip. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|V2|Anim|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> LocomotionLoopDatabase = nullptr;

	/** Strafe (combat-ready) directional RUN loops — the full 8-way set (fwd / 45 / 90 / 135 / back, L+R).
	 *  When bStrafe && LocomotionLoop && Gait==Run, SetBlendStackAnimFromChooser searches THIS DB so MM picks
	 *  the directional pose (incl. the 45/135 diagonals) by trajectory — the 4-way MovementDirection enum can't
	 *  address 8 directions. Assign PSD_v2_StrafeRun in the AZ_ABP_MoverAnimInstance CDO so it cooks. Walk
	 *  strafe stays direct-play (no full diagonal set exists for walk). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|V2|Anim|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> StrafeRunDatabase = nullptr;

	/** Strafe WALK directional loops — the 8-way walk-speed set (AnimPro_Strafe* lateral/diagonals + Walk
	 *  Fwd/Bwd). Used when bStrafe && LocomotionLoop && Gait==Walk. Gait-gated alongside StrafeRunDatabase so
	 *  the chosen clip's speed matches the Mover's gait-driven move speed. Assign PSD_v2_StrafeWalk in the CDO. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|V2|Anim|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> StrafeWalkDatabase = nullptr;

	/** Strafe CROUCH directional loops — the 8-way crouch set (AnimPro_Crouch_Walk* directional). Used when
	 *  bStrafe && LocomotionLoop && Stance==Crouching (gait-agnostic — crouch is one speed). Takes priority
	 *  over the walk/run DBs. Assign PSD_v2_StrafeCrouch in the AZ_ABP_MoverAnimInstance CDO. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|V2|Anim|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> StrafeCrouchDatabase = nullptr;

	/** EXPLORE forward-loop set per gait — Fwd + LeanL/R (cornering lean). When NOT strafe && LocomotionLoop &&
	 *  Standing, SetBlendStackAnimFromChooser searches the gait's loco DB so MM picks the lean variant on a
	 *  curving trajectory. Assign PSD_v2_WalkLoco / PSD_v2_RunLoco in the CDO. (Crouch explore = single clip;
	 *  strafe forward-lean is handled by the lean clips living inside the strafe DBs.) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|V2|Anim|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> WalkLocoDatabase = nullptr;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|V2|Anim|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> RunLocoDatabase = nullptr;

	/** PoseSearch database of JUMP clips (start + air + land). When set, the air states (TransitionToInAir /
	 *  InAirLoop) motion-match across this whole DB instead of the chooser's single clip, so MM walks the jump
	 *  start->air->land by the physics trajectory (incl. vertical). Unlike the loco loops, jump clips are
	 *  one-shots, so we feed MM the continuing pose (below) — the DB's continuing_pose_cost_bias then keeps the
	 *  search advancing through the clip instead of snapping back to frame 0 (the mid-air "restart" bug).
	 *  Assign PSD_v2_Jump in the AZ_ABP_MoverAnimInstance CDO. Null = fall back to the chooser's direct clip. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|V2|Anim|MotionMatching")
	TObjectPtr<UPoseSearchDatabase> JumpDatabase = nullptr;
	// ^ FINAL JUMP DESIGN (2026-06-07, hybrid 2-clip): no direct C++ use anymore. The air-MM machinery
	// (single-clip air search + continuing-pose tracking) was RETIRED — the jump is Start clip (RM rise,
	// tail carries the physics fall) → Land clip (single-clip MM entry frame via the clips' BranchIn
	// notifies, which link to PSD_v2_Jump). Kept as the CDO-assigned cook/reference anchor for the DB.

	/** Foot + move-intent latched at takeoff and held through the air. bLeftFootDown is curve-driven (contact_l)
	 *  and goes false/unstable while airborne (jump clips carry no foot-contact curve). Captured from the last
	 *  GROUNDED frame and applied to ChooserContext during TransitionToInAir / InAirLoop so the LAND rows pick
	 *  the LU/RU land clip matching the takeoff foot chain at touchdown (the air itself pushes nothing — final
	 *  2-clip jump design). bIsMoving latched too, to separate the standing land (JumpIdleLand) from Land2X. */
	bool LastGroundedLeftFootDown = false;
	bool LastGroundedIsMoving     = false;

	/** Hybrid jump (RM rise → physics fall). bHybridJumpActive mirrors UAZ_PawnMoverComponent::bUseHybridJump,
	 *  cached each NativeUpdateAnimation (read by the thread-safe RM-bridge gate in SetBlendStackAnimFromChooser).
	 *  LastRawMoverModeName tracks the raw mode name so the RMAction→Falling apex edge can cancel the rise's
	 *  OverrideAll root-motion move on the game thread. */
	bool  bHybridJumpActive = false;
	FName LastRawMoverModeName;

	// ---- Trajectory (option A: PoseSearch FTransformTrajectory via the Mover predictor) ----
	/** Per-tick PoseSearch trajectory (history + prediction). SINGLE source: the AnimGraph PoseHistory
	 *  node binds its TransformTrajectory pin to this member (named "Trajectory" by convention, matching v1),
	 *  AND the intent-based IsMoving below derives future velocity from it. Built in NativeUpdateAnimation. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "AZ|V2|Anim|Trajectory")
	FTransformTrajectory Trajectory;

	/** World-space velocity TrajectoryFutureLookahead seconds ahead, finite-differenced from Trajectory.
	 *  Leading indicator driving intent-based IsMoving / MovementDirection (replaces lagging Speed2D). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "AZ|V2|Anim|Trajectory")
	FVector PredictedFutureVelocity = FVector::ZeroVector;

	/** How far ahead (seconds) to sample Trajectory for PredictedFutureVelocity. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|V2|Anim|Trajectory", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float TrajectoryFutureLookahead = 0.2f;

	/** Persisted desired controller yaw for PoseSearchGenerateTransformTrajectoryWithPredictor (smooth facing). */
	UPROPERTY(Transient)
	float PredictionYawLast = 0.f;

	/** When true, draws the predicted future velocity on screen (dev only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|V2|Anim|Trajectory")
	bool bDebugTrajectory = false;

	/** Vehicle/driver-pose hook: when true the locomotion SM is pinned to IdleLoop (timers frozen, no idle
	 *  breaks) so an external pose system (e.g. State.InVehicle.Driver.*) can own the skeleton without the
	 *  SM fighting it with phases derived from Mover input it shouldn't trust. Set by gameplay code on
	 *  vehicle enter/exit; passed into the SM every tick. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|V2|Anim|StateMachine")
	bool bSuppressLocomotion = false;

	/** Called from the ABP each tick (after EvaluateChooser2). Populates BlendStackInputs from
	 *  the chooser result + optionally runs a single-frame MotionMatch over ValidAnims when
	 *  ChooserOut.bUseMM is true. Pushes a fresh blend via ForceBlendNextUpdate if bForceBlend
	 *  is set AND the new anim is non-looping (matches v1 behaviour).
	 *  The SM phase is deliberately NOT a parameter: ChooserContext.SMState (written by the
	 *  StateMachine in NativeUpdateAnimation, read by the chooser through context [0]) is the
	 *  single source of truth — taking it as a param invited a stale EventGraph wire to clobber
	 *  the derived phase (the removed `State` param was advisory-only for exactly that reason). */
	UFUNCTION(BlueprintCallable, Category = "AZ|V2|Anim|StateMachine", meta = (BlueprintThreadSafe, AutoCreateRefTerm = "Candidates"))
	void SetBlendStackAnimFromChooser(
		bool bForceBlend,
		FAnimNodeReference BlendStackNode,
		FAZ_ChooserOutputs ChooserOut,
		UAnimationAsset* ChosenAnim,
		const TArray<UObject*>& Candidates);

protected:
	/** Cached on NativeInitializeAnimation — saves a TryGetPawnOwner cast per tick. */
	UPROPERTY(Transient)
	TObjectPtr<AAZ_PawnMoverHeroCharacter> Cached_Pawn;

	UPROPERTY(Transient)
	TObjectPtr<UAZ_PawnMoverComponent> Cached_MoverComponent;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMoverComponent> Cached_CharacterMoverComponent;

	/** The locomotion phase machine (extracted from the old DeriveSMState). Pure C++ decision function — this
	 *  AnimInstance feeds it inputs each tick and applies its outputs; it owns the phase + the transition /
	 *  idle-break timers + the turn/land/pivot latches. Created in NativeInitializeAnimation. */
	UPROPERTY(Transient)
	TObjectPtr<UAZ_LocomotionStateMachine> StateMachine;

	// ---- Idle break scheduling ----
	/** Min seconds in IdleLoop before transitioning to a random IdleBreak. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|V2|Anim|IdleBreak", meta = (ClampMin = "0", ForceUnits = "s"))
	float IdleBreakMinTime = 5.f;

	/** Max seconds in IdleLoop before transitioning to a random IdleBreak. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|V2|Anim|IdleBreak", meta = (ClampMin = "0", ForceUnits = "s"))
	float IdleBreakMaxTime = 15.f;

	/** When the current break anim has this many seconds remaining, transition back to IdleLoop —
	 *  gives the BlendStack cross-fade time to start before the break anim's last frame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|V2|Anim|IdleBreak", meta = (ClampMin = "0", ForceUnits = "s"))
	float IdleBreakAlmostCompleteThreshold = 0.25f;

	// ---- Transition (start/stop) scheduling. The transition / idle-break end-time timers + the turn/land/
	// pivot latches now live in UAZ_LocomotionStateMachine; these thresholds (below + IdleBreakMinTime/MaxTime
	// above) are passed into it each tick / on the Notify* calls.
	/** When the transition clip has this many seconds left, fall through to the target loop so the
	 *  BlendStack cross-fade starts before the clip's last frame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|V2|Anim|Transition", meta = (ClampMin = "0", ForceUnits = "s"))
	float TransitionAlmostCompleteThreshold = 0.15f;

	// A′ RM-move handoff: SetBlendStackAnimFromChooser (BlueprintThreadSafe — may run on the anim worker
	// thread) flags a pending move; NativeUpdateAnimation (game thread) queues it. Plain members (no GC ref).
	bool bPendingTransitionRMMove = false;
	float PendingTransitionRMMoveDurationMs = 0.f;

	// Outgoing-wins blend-out: one-shot stash of the currently-playing clip's authored FAZ_ChooserOutputs::BlendOut.
	// On the NEXT push (which replaces that clip) it overrides the incoming BlendTime for the crossfade, then is
	// refreshed to the new clip's BlendOut. <= 0 = no override. Plain member (worker-thread store, single word).
	float PendingBlendOut = 0.f;

	// ---- Turn-start (90/135/180) selection — see project_v2_locomotion_progress / project_v2_architecture.
	/** Signed yaw (deg, +right) from current facing to desired move direction, recomputed every tick in
	 *  NativeUpdateAnimation while moving. Passed into UAZ_LocomotionStateMachine, which buckets + latches it
	 *  at the start-transition edge. Game-thread only handoff (plain member, no GC ref). */
	float PendingStartAngleDeg = 0.f;

	// ---- Per-push state cache — gates BlendStack pushes so RandomizeColumn rows
	// don't churn the stack every tick within the same logical chooser context.
	// COMMITTED only after a push actually lands (every abort path returns first) — committing
	// earlier wedged transitions: an aborted entry push poisoned the cache and the transition
	// lock then suppressed every retry for the whole transition (audit P0-2).
	UPROPERTY(Transient) EAZ_StateMachineState LastPushedSMState = EAZ_StateMachineState::IdleLoop;
	UPROPERTY(Transient) EAZ_Stance              LastPushedStance = EAZ_Stance::Standing;
	UPROPERTY(Transient) EAZ_Gait                LastPushedGait   = EAZ_Gait::Walk;
	UPROPERTY(Transient) EAZ_MovementDirection   LastPushedDir    = EAZ_MovementDirection::F;
	UPROPERTY(Transient) bool                    LastPushedLeftFootDown = false;

	/** Additive lean — X = left/right lean while moving FORWARD (cornering). Lateral-acceleration driven
	 *  (port of AZ_AnimInstance::Update_AdditiveLean), forward-gated; consumed by the lean BlendSpace via an
	 *  Apply-Additive node in the ABP (mirror AZ_ABP_Mover). BlueprintReadOnly so the AnimGraph can bind it. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|V2|Anim|Lean")
	FVector2D LeanAmount = FVector2D::ZeroVector;
	/** Layer alpha for the additive lean — 0 at idle / non-forward (base pose passes through clean), ramps to 1
	 *  while walking/running forward. Bind to the Apply-Mesh-Space-Additive node's Alpha pin (AlphaInputType=Float).
	 *  This is what keeps the walk-derived additive OFF the idle base pose. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|V2|Anim|Lean")
	float LeanAlpha = 0.f;
	UPROPERTY(Transient) FVector PrevVelocity = FVector::ZeroVector;   // for the VelocityAcceleration derivative (lean)

	// Transition-entry token: incremented on the game thread whenever SMState changes; stamped into
	// LastPushedTransitionSerial on a COMMITTED push. The transition lock compares serials, not raw
	// SMState — raw-state equality let a stale LastPushedSMState from a bailed push suppress a LATER
	// same-state transition (stop → bailed jump push → land-to-stop locked out; audit P0-3).
	// Plain members: game-thread write / worker read, fenced by the anim-task boundary like ChooserContext.
	uint32 TransitionSerial = 0;
	uint32 LastPushedTransitionSerial = 0;
};
