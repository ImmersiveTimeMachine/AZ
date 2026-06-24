// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AZ_LocomotionTypes.h"   // EAZ_StateMachineState, EAZ_StartDirection
#include "AZ_LocomotionStateMachine.generated.h"

/**
 * Inputs the SM reads each tick. Snapshotted by the AnimInstance and passed by const ref so the SM body
 * has ZERO engine/Mover API surface — it is a pure decision function (deterministic, unit-testable,
 * AI-reusable). The air phase is driven entirely by MovementMode (replicated state), so the SM derivation
 * is IDENTICAL on the authority and on simulated proxies — no one-shot jump edge to miss, no proxy-only
 * mirror branch. (Physics jump: the engine Falling mode is persistent replicated STATE; proxies read it
 * via the synced sim state exactly like the authority.)
 * KNOWN EXCEPTION to the determinism claim: idle-break scheduling uses FMath::FRandRange (global RNG), so
 * authority and proxies pick different break moments — ACCEPTED nondeterminism, cosmetic only (the break
 * is an idle flourish; every gameplay-relevant phase stays identical). Seed deterministically if it ever
 * matters.
 */
USTRUCT()
struct FAZ_LocoSMInputs
{
	GENERATED_BODY()

	/** World time this tick (GetTimeSeconds), used for the transition / idle-break / takeoff timers. */
	float WorldNow = 0.f;

	/** Intent-based moving flag (ChooserContext.bIsMoving). */
	bool bIsMoving = false;

	/** The pawn's movement mode mapped to the anim-side enum (OnGround / InAir / future Slide / Traversing).
	 *  Replaces the old bInAirMode bool: dispatching on the full enum is what lets the reserved Slide states
	 *  (TransitionToSlide=7 / SlideLoop=8) become a case instead of another flag (audit scalability pre-work).
	 *  Persistent replicated state, so it drives the air phase identically on proxy + authority. */
	EAZ_MovementMode MovementMode = EAZ_MovementMode::OnGround;

	/** Vehicle/driver-pose hook: pins the SM to IdleLoop with timers cleared — an external pose system owns
	 *  the skeleton, and the SM must not derive phases from Mover input it shouldn't trust (audit pre-work). */
	bool bSuppressLocomotion = false;

	/** Current stance (from IsCrouching()). A change cancels an in-progress idle
	break — same tier as bIsMoving. */
	EAZ_Stance Stance = EAZ_Stance::Standing;

	/** HYBRID JUMP: true while the RM rise owns the capsule (raw Mover mode == RMAction). Holds
	 *  TransitionToInAir PAST the takeoff timer so the rising Start clip keeps driving the capsule
	 *  until the apex handoff — without this, the timer (0.2s) expires mid-rise (apex 0.33-0.47s),
	 *  the InAirLoop row replaces the rising clip with a FALL-frame InAir clip, and its negative
	 *  root-motion Z yanks the capsule down early (the "very quick and low jump" bug). Goes false
	 *  the moment RMAction hands off to Falling → the SM advances to InAirLoop immediately. */
	bool bHoldTakeoffPhase = false;

	/** Signed facing→desired-heading yaw (deg, +right), recomputed each moving frame; buckets the turn-start. */
	float PendingStartAngleDeg = 0.f;

	/** Strafe / combat-ready rotation mode active (ChooserContext.bStrafe). The body faces the camera; direction
	 *  changes just switch the directional loop (no body-turning reversal pivots). */
	bool bStrafe = false;

	/** Camera-relative movement direction (ChooserContext.MovementDirection). Used at a strafe move-start to pick
	 *  a cosmetic turn-start clip ONLY for a FORWARD move (W): there movement == camera, so the explore turn-start
	 *  bucket (PendingStartAngleDeg = body→movement = body→camera) also aligns the body to the camera. Sideways/
	 *  back strafe starts force a plain directional step-off (those forward-turn clips don't fit them). */
	EAZ_MovementDirection MovementDirection = EAZ_MovementDirection::F;

	/** True while the obstacle sensor reports an active reaction (Brace/Blocked). The SM HOLDS LocomotionLoop and
	 *  skips start/stop/turn transitions so a wall reaction can't be interrupted by turning / stick-flicker into
	 *  the wall (those would fire pivots/stops that out-match the reaction row). Grounded-only; clears the instant
	 *  the reaction does (you turn off the wall) -> normal dispatch resumes. See project_obstacle_reaction_system. */
	bool bObstacleReacting = false;

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
 * LIVE since the v2 switch-over — this class owns the phase derivation; DeriveSMState is gone.
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

	/** Impact-reaction clip (Brace/Stumble/HeadHit) played in the held LocomotionLoop. Hands the SM the chosen
	 *  clip's real remaining length so the loop holds until the flinch is almost done — the reaction plays in FULL
	 *  then releases to idle, the hold tracking whatever clip the CHT picked (no per-clip magic hold number). */
	void NotifyReactionClipPushed(float WorldNow, float ClipRemainingSeconds, float AlmostCompleteThreshold);

	/** The last SETTLED stance — what the body currently SHOWS, not last tick's input. During a stance
	 *  transition this still holds the OLD stance, which is exactly the "FromStance" the chooser needs
	 *  the day a third stance (prone) makes the transition pair ambiguous (audit scalability pre-work). */
	EAZ_Stance GetSettledStance() const { return PreviousStance; }

	/** True while an impact-reaction clip is still being held (window set by NotifyReactionClipPushed from the clip
	 *  length). The AnimInstance reads this to keep ChooserContext.Reaction LATCHED past the sensor's brief trigger
	 *  window, so the chooser keeps selecting the flinch clip for its full length instead of dropping back to the
	 *  loop mid-flinch. Benign single-word read (same race class as the Notify* writes). */
	bool IsReactionActive(float WorldNow) const { return ReactionEndTime > 0.f && WorldNow < ReactionEndTime; }

private:
	/** The faithful port of DeriveSMState's body — returns the next phase, mutating the timers/latches. */
	EAZ_StateMachineState ComputeNextState(const FAZ_LocoSMInputs& In);

	/** Bucket a signed facing→desired yaw (deg, +right) into a turn-start clip selector. Moved here from the
	 *  file-static in AZ_MoverAnimInstance.cpp. */
	static EAZ_StartDirection BucketStartDirection(float SignedAngleDeg);

	// ---- runtime state (owned here; was on the AnimInstance) ----
	EAZ_StateMachineState PreviousState = EAZ_StateMachineState::IdleLoop;
	EAZ_Stance PreviousStance = EAZ_Stance::Standing;

	float NextIdleBreakTime = -1.f;
	float IdleBreakEndTime  = -1.f;
	float TransitionEndTime = -1.f;
	/** World time the active impact-reaction clip is ~done; holds LocomotionLoop past the sensor's brief trigger
	 *  window so the flinch plays in full. <0 = no reaction active. Set by NotifyReactionClipPushed. */
	float ReactionEndTime   = -1.f;

	/** Brief takeoff window: while airborne, hold TransitionToInAir until Now >= this, then fall (InAirLoop).
	 *  Physics jumps are variable-length, so the air phase is mode-driven; only the TAKEOFF is timed. */
	float TakeoffEndTime    = -1.f;

	EAZ_StartDirection LatchedStartDirection = EAZ_StartDirection::Fwd;
	bool  bLatchedMovingTransition = false;
	bool  bLatchedJustLanded = false;
};
