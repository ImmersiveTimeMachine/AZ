// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AZ_LocomotionTypes.h"   // EAZ_StateMachineState, EAZ_StartDirection
#include "AZ_LocomotionStateMachine.generated.h"

/**
 * Inputs the SM reads each tick. Snapshotted by the AnimInstance and passed by const ref so the SM body
 * has ZERO engine/Mover API surface — it is a pure decision function (deterministic, unit-testable,
 * AI-reusable). The air phase is now driven entirely by bInAirMode (the replicated MovementMode), so the
 * SM derivation is IDENTICAL on the authority and on simulated proxies — no one-shot jump edge to miss, no
 * proxy-only mirror branch. (Physics jump: the engine Falling mode is persistent replicated STATE; proxies
 * read it via the synced sim state exactly like the authority.)
 */
USTRUCT()
struct FAZ_LocoSMInputs
{
	GENERATED_BODY()

	/** World time this tick (GetTimeSeconds), used for the transition / idle-break / takeoff timers. */
	float WorldNow = 0.f;

	/** Intent-based moving flag (ChooserContext.bIsMoving). */
	bool bIsMoving = false;

	/** Pawn is airborne — MovementMode == InAir (engine Falling for physics jumps; RMAction for future
	 *  vault/mantle). Persistent replicated state, so it drives the air phase identically on proxy + authority. */
	bool bInAirMode = false;

	/** Signed facing→desired-heading yaw (deg, +right), recomputed each moving frame; buckets the turn-start. */
	float PendingStartAngleDeg = 0.f;

	// ---- Tunables (kept CDO-editable on the AnimInstance, passed in rather than duplicated here) ----
	float IdleBreakMinTime = 5.f;
	float IdleBreakMaxTime = 15.f;
};

/** Outputs the AnimInstance applies after Tick — the chooser-selection state + the lifetime-gated latches. */
USTRUCT()
struct FAZ_LocoSMOutputs
{
	GENERATED_BODY()

	EAZ_StateMachineState State          = EAZ_StateMachineState::IdleLoop;
	EAZ_StartDirection    StartDirection = EAZ_StartDirection::Fwd;   // gated: only meaningful in TransitionToLocomotion
	bool                  bMovingTransition = false;                  // gated: only in TransitionToLocomotion
	bool                  bJustLanded       = false;                  // gated: only in TransitionToIdle
};

/**
 * UAZ_LocomotionStateMachine — the v2 locomotion phase machine, extracted from
 * UAZ_MoverAnimInstance::DeriveSMState. Pure C++ (NOT an AnimGraph State Machine node — that would run
 * per-machine on the anim worker thread, the network non-determinism that caused the proxy-jump bug).
 *
 * Owns only RUNTIME state (previous phase + the transition/idle-break timers + the turn/land/pivot latches).
 * Tunables stay on the AnimInstance (CDO-editable) and arrive via FAZ_LocoSMInputs. The AnimInstance retains
 * all Mover side-effects (QueueNextMode, the RM-move queue/teardown) — the SM never touches the Mover, which
 * keeps the "simulated proxies must not drive their own capsule" invariant in one place.
 *
 * Created via NewObject in the AnimInstance's NativeInitializeAnimation. See
 * project_locomotion_sm_refactor_plan for the migration steps and the latent AI-jump gap.
 *
 * NOTE (refactor Step 1): this class is currently DORMANT — built and verified to compile, but the
 * AnimInstance still runs its own DeriveSMState until the switch-over increment (which first checks the ABP
 * for references to the BlueprintReadOnly timer fields before they are removed).
 */
UCLASS()
class AZ_API UAZ_LocomotionStateMachine : public UObject
{
	GENERATED_BODY()

public:
	/** The single per-tick entry. Pure w.r.t. engine state — everything it needs is in `In`. Advances the
	 *  internal PreviousState and returns the new phase + the lifetime-gated selection latches. */
	FAZ_LocoSMOutputs Tick(const FAZ_LocoSMInputs& In);

	/** Called by the AnimInstance's SetBlendStackAnimFromChooser when a transition clip is pushed, to hand the
	 *  SM the chosen clip's real remaining length. Replaces the old direct TransitionEndTime write. May run on
	 *  the anim worker thread (SetBlendStackAnimFromChooser is BlueprintThreadSafe) — a plain single-word float
	 *  store, same benign race as before; do not read-modify-write across the boundary. */
	void NotifyTransitionClipPushed(float WorldNow, float ClipRemainingSeconds, float AlmostCompleteThreshold);

	/** As above, for the idle-break clip → replaces the old IdleBreakEndTime write. */
	void NotifyIdleBreakClipPushed(float WorldNow, float ClipPlayLength, float AlmostCompleteThreshold);

	/** Read-only accessors (the AnimInstance exposed these as BlueprintReadOnly; provide parity for the ABP). */
	float GetTransitionEndTime() const { return TransitionEndTime; }
	float GetNextIdleBreakTime() const { return NextIdleBreakTime; }
	float GetIdleBreakEndTime()  const { return IdleBreakEndTime; }

private:
	/** The faithful port of DeriveSMState's body — returns the next phase, mutating the timers/latches. */
	EAZ_StateMachineState ComputeNextState(const FAZ_LocoSMInputs& In);

	/** Bucket a signed facing→desired yaw (deg, +right) into a turn-start clip selector. Moved here from the
	 *  file-static in AZ_MoverAnimInstance.cpp. */
	static EAZ_StartDirection BucketStartDirection(float SignedAngleDeg);

	// ---- runtime state (owned here; was on the AnimInstance) ----
	EAZ_StateMachineState PreviousState = EAZ_StateMachineState::IdleLoop;

	float NextIdleBreakTime = -1.f;
	float IdleBreakEndTime  = -1.f;
	float TransitionEndTime = -1.f;

	/** Brief takeoff window: while airborne, hold TransitionToInAir until Now >= this, then fall (InAirLoop).
	 *  Physics jumps are variable-length, so the air phase is mode-driven; only the TAKEOFF is timed. */
	float TakeoffEndTime    = -1.f;

	EAZ_StartDirection LatchedStartDirection = EAZ_StartDirection::Fwd;
	bool  bLatchedMovingTransition = false;
	bool  bLatchedJustLanded = false;
};
