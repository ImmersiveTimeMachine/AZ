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
	bool bDebugTrajectory = true;

	/** Called from the ABP each tick (after EvaluateChooser2). Populates BlendStackInputs from
	 *  the chooser result + optionally runs a single-frame MotionMatch over ValidAnims when
	 *  ChooserOut.bUseMM is true. Pushes a fresh blend via ForceBlendNextUpdate if bForceBlend
	 *  is set AND the new anim is non-looping (matches v1 behaviour). */
	UFUNCTION(BlueprintCallable, Category = "AZ|V2|Anim|StateMachine", meta = (BlueprintThreadSafe, AutoCreateRefTerm = "Candidates"))
	void SetBlendStackAnimFromChooser(
		EAZ_StateMachineState State,
		bool bForceBlend,
		FAnimNodeReference BlendStackNode,
		FAZ_ChooserOutputs ChooserOut,
		UAnimationAsset* ChosenAnim,
		const TArray<UObject*>& Candidates);

protected:
	/** Cached on NativeInitializeAnimation — saves a TryGetPawnOwner cast per tick. */
	UPROPERTY(Transient)
	TObjectPtr<AAZ_PawnMoverHeroCharacter> CachedPawn;

	UPROPERTY(Transient)
	TObjectPtr<UAZ_PawnMoverComponent> CachedMover;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMoverComponent> CachedCMC;

	// ---- Phase derivation thresholds (tunable in BP CDO) ----
	/** Below this 2D speed the pawn is considered "not moving" — drives Idle vs Locomotion phase. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|V2|Anim|Phase", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float IdleSpeedThreshold = 10.f;

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

	/** Absolute world time when SMState should switch IdleLoop → IdleBreak. -1 = not scheduled.
	 *  Re-rolled every time we (re-)enter IdleLoop via RandRange(IdleBreakMinTime, IdleBreakMaxTime). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "AZ|V2|Anim|IdleBreak")
	float NextIdleBreakTime = -1.f;

	/** Absolute world time when the current break should end. Set by SetBlendStackAnimFromChooser
	 *  when the chosen break anim is pushed; -1 = no break in progress. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "AZ|V2|Anim|IdleBreak")
	float IdleBreakEndTime = -1.f;

	// ---- Transition (start/stop) scheduling — generalizes the idle-break hold to TransitionToIdle /
	// TransitionToLocomotion. During a transition the chosen RM clip plays uninterrupted (clip locked in
	// SetBlendStackAnimFromChooser) and a per-transition FLayeredMove_RootMotionAttribute drives the capsule
	// (approach A′ — see project_root_motion_mode / project_v2_locomotion_progress).
	/** Absolute world time when the active transition should fall through to its target loop. Set by
	 *  SetBlendStackAnimFromChooser when the transition clip is pushed (Now + remaining - threshold);
	 *  -1 = no transition in progress. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "AZ|V2|Anim|Transition")
	float TransitionEndTime = -1.f;

	/** When the transition clip has this many seconds left, fall through to the target loop so the
	 *  BlendStack cross-fade starts before the clip's last frame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|V2|Anim|Transition", meta = (ClampMin = "0", ForceUnits = "s"))
	float TransitionAlmostCompleteThreshold = 0.15f;

	// A′ RM-move handoff: SetBlendStackAnimFromChooser (BlueprintThreadSafe — may run on the anim worker
	// thread) flags a pending move; NativeUpdateAnimation (game thread) queues it. Plain members (no GC ref).
	bool bPendingTransitionRMMove = false;
	float PendingTransitionRMMoveDurationMs = 0.f;

	// ---- Turn-start (90/135/180) selection — see project_v2_locomotion_progress / project_v2_architecture.
	/** Signed yaw (deg, +right) from current facing to desired move direction, recomputed every tick in
	 *  NativeUpdateAnimation while moving. Consumed by DeriveSMState at the start-transition edge to latch
	 *  LatchedStartDirection. Game-thread only handoff (plain member, no GC ref). */
	float PendingStartAngleDeg = 0.f;

	/** Turn bucket latched when SMState enters TransitionToLocomotion and held for the whole start
	 *  transition — the RM turn clip rotates the capsule as it plays, collapsing the live angle, so we
	 *  must NOT re-bucket mid-turn or it would flip side/magnitude and restart the clip. Fed to
	 *  ChooserContext.StartDirection while in the start transition; Fwd otherwise. */
	UPROPERTY(Transient)
	EAZ_StartDirection LatchedStartDirection = EAZ_StartDirection::Fwd;

	/** Latched alongside LatchedStartDirection at the TransitionToLocomotion entry edge: true if entered
	 *  from LocomotionLoop (a moving pivot / reversal), false if from idle (from-rest start). Fed to
	 *  ChooserContext.bMovingTransition while in the start transition so the chooser can pick the dedicated
	 *  moving-pivot clip vs the from-rest turn-start. */
	UPROPERTY(Transient)
	bool bLatchedMovingTransition = false;

	/** Latched true at the air->ground edge (a landing) and held through the touchdown land transition,
	 *  cleared when it settles to idle. Fed to ChooserContext.bJustLanded so the shared TransitionToIdle
	 *  phase can pick a jump-land clip instead of a locomotion stop. */
	UPROPERTY(Transient)
	bool bLatchedJustLanded = false;

	// ---- Per-push state cache — gates BlendStack pushes so RandomizeColumn rows
	// don't churn the stack every tick within the same logical chooser context.
	UPROPERTY(Transient) EAZ_StateMachineState LastPushedSMState = EAZ_StateMachineState::IdleLoop;
	UPROPERTY(Transient) EAZ_Stance              LastPushedStance = EAZ_Stance::Standing;
	UPROPERTY(Transient) EAZ_Gait                LastPushedGait   = EAZ_Gait::Walk;
	UPROPERTY(Transient) EAZ_MovementDirection   LastPushedDir    = EAZ_MovementDirection::F;
	UPROPERTY(Transient) bool                    LastPushedLeftFootDown = false;

	/** Derive ChooserContext.SMState from current Mover state + previous SMState.
	 *  Non-const: mutates NextIdleBreakTime / IdleBreakEndTime for break scheduling. */
	EAZ_StateMachineState DeriveSMState(const FAZ_v2_ChooserContext& Current, EAZ_StateMachineState Previous);
};
