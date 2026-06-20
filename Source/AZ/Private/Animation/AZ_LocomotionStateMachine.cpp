// Copyright Artur. AZ project.

#include "Animation/AZ_LocomotionStateMachine.h"

// ---- Turn-angle thresholds (degrees) — SINGLE SOURCE OF TRUTH for every turn magic number in this file ----
// Turn-start bucketing: the named turn-start clip magnitudes are 0 / 90 / 135 / 180. Each threshold is the
// MIDPOINT between two adjacent magnitudes, so |yaw| <= threshold picks the lower magnitude. Edit these
// together if the set of turn-start clips changes.
// Reversal: a stop that runs WHILE input is held with |yaw| >= the reversal angle pivots through a turn-start
// instead of steering the loop around (feel-tunable — CHALK wants a weighty plant; promote to an
// EditDefaultsOnly UPROPERTY if designers need to tune it live).
namespace
{
	constexpr float TurnBucketFwd_90Deg  = 45.0f;    // 0   | 90
	constexpr float TurnBucket90_135Deg  = 112.5f;   // 90  | 135
	constexpr float TurnBucket135_180Deg = 157.5f;   // 135 | 180
	constexpr float ReversalAngleDeg     = 135.0f;   // hard-turn pivot threshold

	// Strafe idle turn-in-place: enter the turn when the camera is off the body by more than Enter; leave it
	// when back under Exit (hysteresis so it doesn't flicker idle<->turn at the boundary). Feel-tunable.
	constexpr float IdleTurnEnterDeg     = 10.0f;
	constexpr float IdleTurnExitDeg      = 4.0f;

	// Physics-jump takeoff window (seconds): how long TransitionToInAir (the launch pose) holds before the
	// SM advances to InAirLoop (the fall loop). The total air phase is variable (gravity + floor contact),
	// so only the takeoff is timed; the rest is governed by MovementMode == InAir. Feel-tunable — promote to
	// an EditDefaultsOnly UPROPERTY on the AnimInstance if designers need to tune it live.
	constexpr float TakeoffDurationSeconds = 0.20f;
}

// Bucket a signed facing->desired yaw (deg, +right) into a turn-start clip selector. Thresholds above; side
// from the sign; at exactly +-180 the sign is a deterministic tiebreak.
EAZ_StartDirection UAZ_LocomotionStateMachine::BucketStartDirection(float SignedAngleDeg)
{
	const float A = FMath::Abs(SignedAngleDeg);
	const bool  bRight = SignedAngleDeg >= 0.f;
	if (A <= TurnBucketFwd_90Deg)  { return EAZ_StartDirection::Fwd; }
	if (A <= TurnBucket90_135Deg)  { return bRight ? EAZ_StartDirection::R90  : EAZ_StartDirection::L90;  }
	if (A <= TurnBucket135_180Deg) { return bRight ? EAZ_StartDirection::R135 : EAZ_StartDirection::L135; }
	return                           bRight ? EAZ_StartDirection::R180 : EAZ_StartDirection::L180;
}

void UAZ_LocomotionStateMachine::NotifyTransitionClipPushed(float WorldNow, float ClipRemainingSeconds, float AlmostCompleteThreshold)
{
	// Mirrors the old SetBlendStackAnimFromChooser write: TransitionEndTime = Now + Remaining - threshold.
	TransitionEndTime = WorldNow + ClipRemainingSeconds - AlmostCompleteThreshold;
}

void UAZ_LocomotionStateMachine::NotifyIdleBreakClipPushed(float WorldNow, float ClipPlayLength, float AlmostCompleteThreshold)
{
	IdleBreakEndTime = WorldNow + ClipPlayLength - AlmostCompleteThreshold;
}

FAZ_LocoSMOutputs UAZ_LocomotionStateMachine::Tick(const FAZ_LocoSMInputs& In)
{
	const EAZ_StateMachineState NewState = ComputeNextState(In);

	FAZ_LocoSMOutputs Out;
	Out.State = NewState;

	// Latch lifetime-gating (was in NativeUpdateAnimation): hold the latched bucket/flag only for the phase it
	// belongs to, defaults everywhere else. DeriveSMState set the latches at the entry edge; we gate them here.
	const bool bInStartTransition = (NewState == EAZ_StateMachineState::TransitionToLocomotion);
	Out.StartDirection    = bInStartTransition ? LatchedStartDirection : EAZ_StartDirection::Fwd;
	Out.bMovingTransition = bInStartTransition ? bLatchedMovingTransition : false;
	// bJustLanded is surfaced for BOTH land transitions: TransitionToIdle (standing land → JumpIdleLand) AND
	// TransitionToLocomotion (moving land → Land2Walk/Land2Run). A normal moving START is also
	// TransitionToLocomotion but clears bLatchedJustLanded on entry, so the flag disambiguates land vs start.
	Out.bJustLanded       = (NewState == EAZ_StateMachineState::TransitionToIdle
	                         || NewState == EAZ_StateMachineState::TransitionToLocomotion) ? bLatchedJustLanded : false;

	PreviousState = NewState;
	return Out;
}

// Faithful port of UAZ_MoverAnimInstance::DeriveSMState. The `Previous` parameter is now the owned
// PreviousState; `Current.bIsMoving` is In.bIsMoving; the role/mode/edge reads are passed in via In; world
// time is In.WorldNow; the idle-break range is In.IdleBreakMin/MaxTime. Behaviour is intended to be identical.
EAZ_StateMachineState UAZ_LocomotionStateMachine::ComputeNextState(const FAZ_LocoSMInputs& In)
{
	const EAZ_StateMachineState Previous = PreviousState;
	const float Now = In.WorldNow;

	// ---- Suppressed (vehicle / external pose system owns the skeleton): pin IdleLoop, clear every timer
	// and latch, and CONSUME stance flips (so exiting the vehicle crouched doesn't fire a stale
	// TransitionStance from a flip that happened while suppressed). One early-return = zero interaction
	// with the phase logic below. ----
	if (In.bSuppressLocomotion)
	{
		PreviousStance     = In.Stance;
		NextIdleBreakTime  = -1.f;
		IdleBreakEndTime   = -1.f;
		TransitionEndTime  = -1.f;
		TakeoffEndTime     = -1.f;
		bLatchedJustLanded = false;
		return EAZ_StateMachineState::IdleLoop;
	}

	// ---- Airborne (physics jump / RM action): MODE-driven, identical on proxy + authority ----
	// MovementMode == InAir (engine Falling for jumps; RMAction for the hybrid rise / future vault/mantle) is
	// persistent replicated STATE — proxies derive the air phase from it exactly like the authority, so there
	// is no one-shot jump edge to miss and no proxy-only mirror branch.
	// SIMPLIFIED JUMP (2026-06-14): the jump has only TWO anim phases — START (this) and LAND (the touchdown
	// block below). There is NO separate in-air loop: TransitionToInAir (the "start jump" / launch phase)
	// PERSISTS for the ENTIRE airborne duration. The Start clip plays through takeoff -> rise -> fall
	// cosmetically while the capsule arc is owned by the RM rise then the engine Falling fall, until REAL
	// floor contact hands us to the land transition below. Collapsing the air to one phase is purely an
	// animation-selection change: the capsule handoff (RMAction->Falling) and the RM-rise-move cancel are raw
	// Mover-mode edges in UAZ_MoverAnimInstance (independent of this phase), so the physics is untouched.
	// EAZ_StateMachineState::InAirLoop is now a RESERVED/unused enum value — never produced; kept only to
	// preserve the chooser-asset integer ABI. bHoldTakeoffPhase / TakeoffEndTime / TakeoffDurationSeconds are
	// likewise vestigial (the in-air loop they gated is gone) — remove them at the next editor-closed build.
	// (Slide/Traversing: add explicit MovementMode cases here when those modes land — the enum input exists
	// for exactly that; do NOT add more bool flags.)
	if (In.MovementMode == EAZ_MovementMode::InAir)
	{
		PreviousStance     = In.Stance;
		NextIdleBreakTime  = -1.f;
		IdleBreakEndTime   = -1.f;
		bLatchedJustLanded = false;
		return EAZ_StateMachineState::TransitionToInAir;   // start -> (held through the whole air) -> land
	}

	// ---- Touchdown: airborne last frame, grounded now (engine Falling → Walking on REAL floor contact). ----
	// This is the height-divergence fix: the capsule lands on the ACTUAL floor (gravity + floor query did the
	// descent), and the land anim is selected HERE on contact — not when a baked clip happens to end.
	if (Previous == EAZ_StateMachineState::TransitionToInAir || Previous == EAZ_StateMachineState::InAirLoop)
	{
		TakeoffEndTime     = -1.f;
		NextIdleBreakTime  = -1.f;
		IdleBreakEndTime   = -1.f;
		bLatchedJustLanded = true;          // surfaced as bJustLanded for the land transition (chooser land rows)
		TransitionEndTime  = Now + 1.0f;    // overridden by the land clip's real length (NotifyTransitionClipPushed)
		if (In.bIsMoving)
		{
			// Moving land → land-into-locomotion; chooser picks Land2Walk / Land2Run by Gait + bJustLanded.
			LatchedStartDirection    = EAZ_StartDirection::Fwd;
			bLatchedMovingTransition = true;   // momentum-preserving (horizontal velocity carried through the arc)
			return EAZ_StateMachineState::TransitionToLocomotion;
		}
		// Standing land → land-into-idle; chooser picks JumpIdleLand by bJustLanded.
		return EAZ_StateMachineState::TransitionToIdle;
	}

	// ---- Grounded dispatch. The movement-intent split must precede the switch: the SAME previous phase routes
	// differently by intent (e.g. LocomotionLoop → pivot when moving, → stop when not). Within each branch the
	// switch dispatches on the previous phase; a NEW EAZ_StateMachineState should be added as an explicit case
	// (the default is the "any other prior phase" catch-all the if-chain used). ----
	if (In.bIsMoving)
	{
		PreviousStance = In.Stance;
		NextIdleBreakTime = -1.f;
		IdleBreakEndTime  = -1.f;
		// Clear the land flag on every NORMAL moving frame (start / pivot / loop), but PRESERVE it while a
		// land-into-locomotion transition is still playing (Previous == TransitionToLocomotion) so the moving
		// land clip (Land2Walk / Land2Run) stays selected for the whole transition. A fresh moving start also
		// passes through TransitionToLocomotion but enters from IdleLoop (flag already false), so this never
		// leaks a stale land flag onto a normal start.
		if (Previous != EAZ_StateMachineState::TransitionToLocomotion)
		{
			bLatchedJustLanded = false;
		}

		// Hard-reversal / sharp-turn detection while MOVING: a stop that runs WHILE input is held is a
		// committed reversal — pivot through a turn-start instead of smearing the loop to a backward clip.
		// No reversal pivot in strafe — the body never turns to face movement; a direction change just
		// switches the directional loop (MovementDirection picks the new clip).
		const bool bHardTurn = !In.bStrafe && FMath::Abs(In.PendingStartAngleDeg) >= ReversalAngleDeg;

		switch (Previous)
		{
		// Start transition: idle → moving routes through TransitionToLocomotion (from-rest turn-start clips).
		case EAZ_StateMachineState::IdleLoop:
		case EAZ_StateMachineState::IdleBreak:
		case EAZ_StateMachineState::IdleTurnLeft:
		case EAZ_StateMachineState::IdleTurnRight:
			// Strafe uses a DIRECTIONAL step-off start (StrafeLeftStart etc.), selected by MovementDirection,
			// NOT a body-turning turn-start — so force StartDirection=Fwd (no turn bucketing) so the explore
			// turn-start rows don't fire, and let the chooser pick the strafe start by direction. Explore keeps
			// the turn-start bucketing. (No "rotate-first": the strafe start clip steps off without turning.)
			LatchedStartDirection = In.bStrafe ? EAZ_StartDirection::Fwd : BucketStartDirection(In.PendingStartAngleDeg);
			bLatchedMovingTransition = false;   // from rest → from-rest start clips
			TransitionEndTime = Now + 1.0f;     // overridden by the real clip length
			return EAZ_StateMachineState::TransitionToLocomotion;

		case EAZ_StateMachineState::TransitionToLocomotion:
			if (TransitionEndTime > 0.f && Now >= TransitionEndTime)
			{
				TransitionEndTime = -1.f;
				return EAZ_StateMachineState::LocomotionLoop;
			}
			return EAZ_StateMachineState::TransitionToLocomotion;

		// Reversal / sharp turn while moving: go STRAIGHT to the turn-start (moving pivot).
		case EAZ_StateMachineState::LocomotionLoop:
			if (bHardTurn)
			{
				LatchedStartDirection = BucketStartDirection(In.PendingStartAngleDeg);
				bLatchedMovingTransition = true;   // moving pivot → momentum-preserving pivot clip
				TransitionEndTime = Now + 1.0f;
				return EAZ_StateMachineState::TransitionToLocomotion;
			}
			TransitionEndTime = -1.f;
			return EAZ_StateMachineState::LocomotionLoop;

		// Stop re-pressed → resume the loop (residual momentum, no from-rest start).
		case EAZ_StateMachineState::TransitionToIdle:
			TransitionEndTime = -1.f;
			return EAZ_StateMachineState::LocomotionLoop;

		// Landing or any other prior phase → straight to the loop.
		default:
			TransitionEndTime = -1.f;
			return EAZ_StateMachineState::LocomotionLoop;
		}
	}

	// Not moving — stop / idle / idle-break / idle-turn dispatch.

	// Strafe idle turn-in-place PREEMPTS the idle break: while standing in strafe with the camera off the
	// body, turn in place toward it. Idle family only (not mid-stop/stance) and stance settled (a stance flip
	// falls through to TransitionStance in the switch). Hysteresis via Enter/Exit so it doesn't flicker.
	{
		const bool bIdleFamily =
			Previous == EAZ_StateMachineState::IdleLoop ||
			Previous == EAZ_StateMachineState::IdleBreak ||
			Previous == EAZ_StateMachineState::IdleTurnLeft ||
			Previous == EAZ_StateMachineState::IdleTurnRight;
		const bool bWasTurning =
			Previous == EAZ_StateMachineState::IdleTurnLeft || Previous == EAZ_StateMachineState::IdleTurnRight;
		const float TurnThresh = bWasTurning ? IdleTurnExitDeg : IdleTurnEnterDeg;
		if (bIdleFamily && In.bStrafe && In.Stance == PreviousStance
			&& FMath::Abs(In.CameraYawDelta) > TurnThresh)
		{
			NextIdleBreakTime = -1.f;
			IdleBreakEndTime  = -1.f;
			return (In.CameraYawDelta > 0.f)
				? EAZ_StateMachineState::IdleTurnRight
				: EAZ_StateMachineState::IdleTurnLeft;
		}
	}

	switch (Previous)
	{
	// Stop transition: moving → idle routes through TransitionToIdle.
	case EAZ_StateMachineState::LocomotionLoop:
		bLatchedJustLanded = false;   // this stop is a STOP, not a land
		// Strafe now routes through TransitionToIdle like explore — the chooser picks the DIRECTIONAL strafe
		// stop clip (WalkBwdStop / StrafeLeft/RightStop_LU/RU) by MovementDirection + foot; forward reuses the
		// explore fwd stop (pinned Dir==F). The RM bridge drives the plant.
		TransitionEndTime = Now + 1.0f;
		return EAZ_StateMachineState::TransitionToIdle;

	case EAZ_StateMachineState::TransitionToIdle:
		if (In.Stance != PreviousStance)
		{
			TransitionEndTime  = Now + 1.0f;   // overridden by the Idle2Crouch clip length
			bLatchedJustLanded = false;
			return EAZ_StateMachineState::TransitionStance;
		}

		if (TransitionEndTime > 0.f && Now >= TransitionEndTime)
		{
			TransitionEndTime = -1.f;
			bLatchedJustLanded = false;
			NextIdleBreakTime = Now + FMath::FRandRange(In.IdleBreakMinTime, In.IdleBreakMaxTime);
			return EAZ_StateMachineState::IdleLoop;
		}
		return EAZ_StateMachineState::TransitionToIdle;

	// Already mid-break — stay until the break anim's almost-complete window.
	case EAZ_StateMachineState::IdleBreak:
		if (In.Stance != PreviousStance)
		{
			IdleBreakEndTime  = -1.f;
			NextIdleBreakTime = -1.f;
			TransitionEndTime = Now + 1.0f;
			return EAZ_StateMachineState::TransitionStance;
		}
		if (IdleBreakEndTime > 0.f && Now >= IdleBreakEndTime)
		{
			IdleBreakEndTime  = -1.f;
			NextIdleBreakTime = Now + FMath::FRandRange(In.IdleBreakMinTime, In.IdleBreakMaxTime);
			return EAZ_StateMachineState::IdleLoop;
		}
		return EAZ_StateMachineState::IdleBreak;
		
	// Stance transition: in-place lower/rise clip; hold for its length, then settle into IdleLoop.
	case EAZ_StateMachineState::TransitionStance:
		if (TransitionEndTime > 0.f && Now >= TransitionEndTime)
		{
			TransitionEndTime = -1.f;
			PreviousStance = In.Stance;
			NextIdleBreakTime = Now + FMath::FRandRange(In.IdleBreakMinTime, In.IdleBreakMaxTime);
			return EAZ_StateMachineState::IdleLoop;
		}
		return EAZ_StateMachineState::TransitionStance;

	// IdleLoop / IdleTurn (settling back, not turning) / fresh entry into idle. Schedule + fire idle breaks.
	case EAZ_StateMachineState::IdleTurnLeft:
	case EAZ_StateMachineState::IdleTurnRight:
	default:
		// Stance changed while idle -> play the in-place lower/rise clip. The chooser keys on
		// TransitionStance + the (target) Stance column to pick Idle2Crouch vs Crouch2Idle.
		if (In.Stance != PreviousStance)
		{
			NextIdleBreakTime = -1.f;
			TransitionEndTime = Now + 1.0f;   // overridden by the clip's real length
			return EAZ_StateMachineState::TransitionStance;
		}
		if (NextIdleBreakTime < 0.f)
		{
			NextIdleBreakTime = Now + FMath::FRandRange(In.IdleBreakMinTime, In.IdleBreakMaxTime);
		}
		else if (Now >= NextIdleBreakTime)
		{
			NextIdleBreakTime = -1.f;
			return EAZ_StateMachineState::IdleBreak;
		}
		return EAZ_StateMachineState::IdleLoop;
	}
}
