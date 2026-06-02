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
	Out.bJustLanded       = (NewState == EAZ_StateMachineState::TransitionToIdle) ? bLatchedJustLanded : false;

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

	// ---- Simulated-proxy air mirror ----
	// A simulated proxy doesn't run the Mover sim and routinely MISSES the one-shot jump edge (true for a
	// single frame, falls between interpolated snapshots). The MovementMode is persistent replicated STATE, so
	// mirror it: in the air phase for EXACTLY as long as mode == RMAction. Stateless ⇒ no re-entry.
	if (In.bIsSimProxy)
	{
		if (In.bInRMActionMode)
		{
			NextIdleBreakTime = -1.f;
			IdleBreakEndTime  = -1.f;
			bLatchedJustLanded = false;
			return EAZ_StateMachineState::TransitionToInAir;   // enter or stay
		}
		if (Previous == EAZ_StateMachineState::TransitionToInAir)
		{
			// Mode just dropped out of RMAction → air phase ended on the authority. Settle by synced intent.
			TransitionEndTime = -1.f;
			return In.bIsMoving ? EAZ_StateMachineState::LocomotionLoop : EAZ_StateMachineState::IdleLoop;
		}
		// Not in the RM jump → fall through to the normal grounded derivation below.
	}

	// --- RM jump hold: once a jump has started, HOLD TransitionToInAir for the clip's full length, decoupled
	// from MovementMode (the clip's own RM flips the mode mid-clip when it plants). ---
	if (Previous == EAZ_StateMachineState::TransitionToInAir)
	{
		NextIdleBreakTime = -1.f;
		IdleBreakEndTime  = -1.f;
		bLatchedJustLanded = false;
		if (TransitionEndTime > 0.f && Now >= TransitionEndTime)
		{
			TransitionEndTime = -1.f;
			return In.bIsMoving ? EAZ_StateMachineState::LocomotionLoop : EAZ_StateMachineState::IdleLoop;
		}
		return EAZ_StateMachineState::TransitionToInAir;
	}

	// --- Jump START off the input edge (simulating machine only; In.bJumpJustPressed is false on proxies). ---
	if (In.bJumpJustPressed)
	{
		NextIdleBreakTime = -1.f;
		IdleBreakEndTime  = -1.f;
		bLatchedJustLanded = false;
		TransitionEndTime = Now + 1.0f;   // overridden by the RTG_RM_Jump_place_ALL clip length
		return EAZ_StateMachineState::TransitionToInAir;
	}

	// ---- Grounded dispatch. The movement-intent split must precede the switch: the SAME previous phase routes
	// differently by intent (e.g. LocomotionLoop → pivot when moving, → stop when not). Within each branch the
	// switch dispatches on the previous phase; a NEW EAZ_StateMachineState should be added as an explicit case
	// (the default is the "any other prior phase" catch-all the if-chain used). ----
	if (In.bIsMoving)
	{
		NextIdleBreakTime = -1.f;
		IdleBreakEndTime  = -1.f;
		bLatchedJustLanded = false;

		// Hard-reversal / sharp-turn detection while MOVING: a stop that runs WHILE input is held is a
		// committed reversal — pivot through a turn-start instead of smearing the loop to a backward clip.
		const bool bHardTurn = FMath::Abs(In.PendingStartAngleDeg) >= ReversalAngleDeg;

		switch (Previous)
		{
		// Start transition: idle → moving routes through TransitionToLocomotion (from-rest turn-start clips).
		case EAZ_StateMachineState::IdleLoop:
		case EAZ_StateMachineState::IdleBreak:
			LatchedStartDirection = BucketStartDirection(In.PendingStartAngleDeg);
			bLatchedMovingTransition = false;   // from rest → from-rest turn-start clips
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

	// Not moving — stop / idle / idle-break dispatch.
	switch (Previous)
	{
	// Stop transition: moving → idle routes through TransitionToIdle.
	case EAZ_StateMachineState::LocomotionLoop:
		bLatchedJustLanded = false;   // this TransitionToIdle is a STOP, not a land
		TransitionEndTime = Now + 1.0f;
		return EAZ_StateMachineState::TransitionToIdle;

	case EAZ_StateMachineState::TransitionToIdle:
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
		if (IdleBreakEndTime > 0.f && Now >= IdleBreakEndTime)
		{
			IdleBreakEndTime = -1.f;
			NextIdleBreakTime = Now + FMath::FRandRange(In.IdleBreakMinTime, In.IdleBreakMaxTime);
			return EAZ_StateMachineState::IdleLoop;
		}
		return EAZ_StateMachineState::IdleBreak;

	// IdleLoop or fresh entry into idle. Schedule the next break if none pending, else fire when due.
	default:
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
