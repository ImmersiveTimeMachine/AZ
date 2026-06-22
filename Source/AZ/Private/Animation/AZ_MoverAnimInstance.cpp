// Copyright Artur. AZ project.

#include "Animation/AZ_MoverAnimInstance.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AZ_LocomotionStateMachine.h"
#include "AZ_GameplayTags.h"
#include "BlendStack/BlendStackAnimNodeLibrary.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Character/AZ_PawnMovementMode_RMAction.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/LayeredMoves/RootMotionAttributeLayeredMove.h"
#include "MoverDataModelTypes.h"   // FCharacterDefaultInputs
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "MoverComponent.h"
#include "MoverPoseSearchTrajectoryPredictor.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "PoseSearch/PoseSearchTrajectoryPredictor.h"

void UAZ_MoverAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// The locomotion phase machine (extracted from the old DeriveSMState). NewObject with `this` as outer so
	// the StateMachine UPROPERTY keeps it alive; tunables stay on this AnimInstance and are passed into Tick.
	StateMachine = NewObject<UAZ_LocomotionStateMachine>(this);

	// RM bridge (anim side). Mover does NOT consume UAnimInstance::RootMotionMode directly, but
	// RootMotionFromEverything makes the engine extract the per-frame root delta from ALL playing
	// anims into the "RootMotionDelta" mesh attribute. FLayeredMove_RootMotionAttribute (queued on
	// the pawn's MoverComp in BeginPlay) consumes that attribute to move the capsule. Loops carry
	// ~zero authored RM, so this only drives stop/start/transition clips. Mirrors v1 UAZ_AnimInstance.
	// See project_root_motion_mode.
	RootMotionMode = ERootMotionMode::RootMotionFromEverything;

	// Cache pawn + Mover refs — pawn class is fixed at spawn, no need to re-cast every tick.
	if (APawn* PawnOwner = TryGetPawnOwner())
	{
		Cached_Pawn = Cast<AAZ_PawnMoverHeroCharacter>(PawnOwner);
		if (Cached_Pawn)
		{
			Cached_MoverComponent = Cached_Pawn->GetMoverComponent();
			Cached_CharacterMoverComponent = Cast<UCharacterMoverComponent>(Cached_MoverComponent);
		}
	}

	// Re-init can run on a re-used / Live-Coding-re-instanced object: clear every one-shot stash and
	// push-cache so the first real push can't inherit state from a clip that no longer exists.
	PendingBlendOut                   = 0.f;
	bPendingTransitionRMMove          = false;
	PendingTransitionRMMoveDurationMs = 0.f;
	TransitionSerial                  = 0;
	LastPushedTransitionSerial        = 0;
	LastPushedSMState                 = EAZ_StateMachineState::IdleLoop;
}

void UAZ_MoverAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!Cached_Pawn || !Cached_MoverComponent)
	{
		return;
	}

	// ============================== ROOT-MOTION BRIDGE ==============================
	// How a chooser-picked clip gets to move the CAPSULE (not just the mesh).
	//
	// The model: LOOP clips (idle/walk/run) are velocity-driven — their root motion is
	// extracted but never consumed, the capsule follows Mover's own velocity/input.
	// TRANSITION clips (start / stop / turn, and the hybrid-jump RISE) are authored
	// root-motion — for those, and only those, the capsule must follow the clip exactly.
	//
	// Mover's mechanism for that is a layered move: while an FLayeredMove_RootMotionAttribute
	// is live, the simulation reads the mesh's root-motion attribute (written every frame
	// because this AnimInstance has RootMotionFromEverything) and OVERRIDES the capsule
	// motion with it. No move queued = attribute ignored = velocity-driven. That single
	// bit — "is the move live" — is the whole loop-vs-transition switch.
	//
	// The bridge has two halves, because Mover may only be written from the game thread:
	//   1. SetBlendStackAnimFromChooser (anim WORKER thread) pushes a transition clip and
	//      sets bPendingTransitionRMMove + the clip's remaining play length.
	//   2. THIS block (GAME thread, the next NativeUpdateAnimation) consumes the flag and
	//      queues the move with DurationMs = that remaining length, so it expires exactly
	//      when the clip ends. Early exits (transition abandoned, jump apex) are handled
	//      by the cancel/teardown blocks further down in this function.
	// See project_root_motion_mode (approach A′).
	if (bPendingTransitionRMMove)
	{
		// Consume the flag on EVERY machine — it gets set wherever the chooser runs (including
		// machines that must not queue, gated below); a stale flag would queue on a later frame.
		bPendingTransitionRMMove = false;

		// Queue only where this machine actually SIMULATES the capsule:
		//  - IsGameWorld(): BP-editor preview worlds (SCS / AnimBP viewport) tick this AnimInstance
		//    too, but their MoverComponent has no simulation behind it — queuing there trips the
		//    "null backend liaison" ensure in UMoverComponent::QueueLayeredMove.
		//  - Not ROLE_SimulatedProxy: a sim proxy's capsule is driven by the replicated transform;
		//    RM-driving it as well makes the two sources fight (jerky proxy motion). The proxy still
		//    plays the clip and extracts root motion in place, so the mesh animates correctly while
		//    replication carries the world movement.
		const UWorld* World = GetWorld();
		if (World && World->IsGameWorld() && Cached_Pawn->GetLocalRole() != ROLE_SimulatedProxy)
		{
			// REPLACE, don't stack: a direction reversal chains stop → turn-start with no
			// non-transition frame between them, so the teardown below never runs in the gap —
			// without this cancel, both clips' moves would be live and double-drive the capsule.
			// No-op in the normal case (no prior transition move active).
			Cached_MoverComponent->CancelFeaturesWithTag(Mover_AnimRootMotion, /*bRequireExactMatch*/ false);

			TSharedPtr<FLayeredMove_RootMotionAttribute> RMMove = MakeShared<FLayeredMove_RootMotionAttribute>();
			RMMove->DurationMs = PendingTransitionRMMoveDurationMs;
			Cached_MoverComponent->QueueLayeredMove(RMMove);
		}
	}

	const EAZ_StateMachineState PreviousSMState = ChooserContext.SMState;

	// ---- Trajectory (option A — PoseSearch FTransformTrajectory via the Mover predictor) ----
	// SINGLE source: feeds the AnimGraph PoseHistory node (MM) via the "Trajectory" property binding AND
	// the intent-based IsMoving below. Mirrors v1 Update_Trajectory. The predictor is Setup on the pawn in
	// BeginPlay. Future velocity is finite-differenced from the predicted samples (the sample struct carries
	// position/facing/time, not velocity), giving a leading indicator that catches start/stop on intent.
	if (UMoverTrajectoryPredictor* Predictor = Cached_Pawn->GetTrajectoryPredictor())
	{
		TScriptInterface<IPoseSearchTrajectoryPredictorInterface> PredictorInterface;
		PredictorInterface.SetObject(Predictor);
		PredictorInterface.SetInterface(Cast<IPoseSearchTrajectoryPredictorInterface>(Predictor));

		UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectoryWithPredictor(
			PredictorInterface, DeltaSeconds, Trajectory, PredictionYawLast, Trajectory,
			/*HistoryInterval*/ 0.04f, /*HistoryCount*/ 10,
			/*PredictionInterval*/ 0.1f, /*PredictionCount*/ 10);

		const FVector PNow = Trajectory.GetSampleAtTime(0.f).Position;
		const FVector PFut = Trajectory.GetSampleAtTime(TrajectoryFutureLookahead).Position;
		PredictedFutureVelocity = (PFut - PNow) / FMath::Max(0.01f, TrajectoryFutureLookahead);
	}

	// ---- Mover-derived fields ----
	const FVector Velocity = Cached_MoverComponent->GetVelocity();
	ChooserContext.Speed2D = static_cast<float>(Velocity.Size2D());   // measured speed (gait thresholds, leaning)

	// Intent-based "is moving" — keyed on the player's ACTUAL movement INPUT, not predicted/measured
	// velocity. During an RM-driven stop (and the velocity-driven decel of the walk loop) the capsule is
	// still moving forward, so a velocity-based test keeps IsMoving true → the SM never leaves
	// LocomotionLoop / oscillates (the WalkFwdLoop "stuck loop" bug). Reading the last Mover input cmd makes
	// release→stop fire on intent (the frame W is released), with the RM stop clip then driving the decel.
	// (GASP: IsMoving = future velocity + accel; input intent is the cleanest accel/intent proxy and works
	// for start AND stop.) PredictedFutureVelocity is still used below for MovementDirection + MM trajectory.
	FVector MoveIntentWS = FVector::ZeroVector;
	{
		const FMoverInputCmdContext& LastInput = Cached_MoverComponent->GetLastInputCmd();
		if (const FCharacterDefaultInputs* CharIn = LastInput.InputCollection.FindDataByType<FCharacterDefaultInputs>())
		{
			MoveIntentWS = CharIn->GetMoveInput_WorldSpace();
		}
	}
	ChooserContext.bIsMoving = MoveIntentWS.SizeSquared2D() > FMath::Square(0.1f);   // small input deadzone

	// MovementMode from the active mode's registered name (set on the MoverComponent's MovementModes map).
	const FName ModeName = Cached_MoverComponent->GetMovementModeName();
	if (ModeName == TEXT("Walking"))
	{
		ChooserContext.MovementMode = EAZ_MovementMode::OnGround;
	}
	else if (ModeName == TEXT("Falling"))
	{
		ChooserContext.MovementMode = EAZ_MovementMode::InAir;
	}
	else if (ModeName == TEXT("RMAction"))
	{
		// RMAction is airborne; report InAir so the SM air phase + trajectory treat it like Falling.
		// Used by (a) the HYBRID JUMP rise (bUseHybridJump on the mover comp: jump enters RMAction, the Start
		// clip's RM lifts the capsule, the mode hands itself to Falling at the apex) and (b) the FUTURE
		// vault/mantle traversal (bHandOffToFallingAtApex=false there — plays to completion).
		ChooserContext.MovementMode = EAZ_MovementMode::InAir;
	}
	// Slide / Swim left as default until those modes exist.

	// ---- Hybrid jump: flag cache + apex-handoff RM teardown ----
	// bRMActionIsJumpRise: TYPED RMAction-kind read (audit pre-work) — the mode's own
	// bHandOffToFallingAtApex distinguishes a jump RISE (true: hands to Falling at apex, the SM must hold
	// the takeoff phase) from a self-contained traversal action (false: vault/mantle plays to completion
	// and must NOT be classified as a jump takeoff; it gets its own Traversal SM phase when built).
	bool bRMActionIsJumpRise = false;
	{
		const UAZ_PawnMoverComponent* AZMover = Cast<UAZ_PawnMoverComponent>(Cached_MoverComponent);
		bHybridJumpActive = AZMover && AZMover->bUseHybridJump;

		if (ModeName == TEXT("RMAction"))
		{
			if (const UAZ_PawnMovementMode_RMAction* RMMode = Cast<UAZ_PawnMovementMode_RMAction>(
					Cached_MoverComponent->FindMovementModeByName(TEXT("RMAction"))))
			{
				bRMActionIsJumpRise = RMMode->bHandOffToFallingAtApex;
			}
		}

		// At the apex, RMAction switches itself to Falling. The rise's OverrideAll root-motion move must die
		// WITH it: under Falling the engine merely downgrades it (SkipVerticalAnimRootMotion strips the Z) while
		// the HORIZONTAL would stay clip-driven and fight air control/gravity pathing. Cancel on ANY observed
		// RMAction-exit edge, not just →Falling: a low-ceiling abort or instant ground re-contact exits straight
		// to Walking, and the move (whose DurationMs is the FULL remaining clip incl. the fall tail) would keep
		// clip-driving the capsule on the ground until it expired (audit P1-15). Simulating machines only —
		// proxies never queued the move.
		if (Cached_Pawn->GetLocalRole() != ROLE_SimulatedProxy &&
			LastRawMoverModeName == TEXT("RMAction") && ModeName != TEXT("RMAction"))
		{
			Cached_MoverComponent->CancelFeaturesWithTag(Mover_AnimRootMotion, /*bRequireExactMatch*/ false);
		}
		LastRawMoverModeName = ModeName;
	}

	// Stance — CharacterMoverComponent owns crouch state.
	ChooserContext.Stance = (Cached_CharacterMoverComponent && Cached_CharacterMoverComponent->IsCrouching())
		? EAZ_Stance::Crouching
		: EAZ_Stance::Standing;

	// Gait — read the authoritative INTENT from the Mover input cmd (the same FAZ_MoverCustomInputs.Gait
	// the walking mode uses to pick WalkSpeed/RunSpeed/SprintSpeed), NOT re-derived from measured speed.
	// Intent-based so a from-idle run start selects the Run chooser rows on frame 1 (speed is still ~0 then,
	// which would mis-read as Walk), and so a mid-locomotion walk->run flips rows immediately instead of
	// lagging until speed crosses a threshold. Mirrors how bIsMoving above reads the input cmd. The gait is
	// produced in ProduceInput from the player's Movement.* GAS tags. Falls back to Walk when absent (e.g.
	// sim proxies without bSyncInputsForSimProxy — same pending MP fix as bIsMoving).
	const FVector CurrentForward = Cached_Pawn->GetActorForwardVector();
	{
		const FMoverInputCmdContext& GaitInput = Cached_MoverComponent->GetLastInputCmd();
		if (const FAZ_MoverCustomInputs* Custom = GaitInput.InputCollection.FindDataByType<FAZ_MoverCustomInputs>())
		{
			ChooserContext.Gait = Custom->Gait;
		}
		else
		{
			ChooserContext.Gait = EAZ_Gait::Walk;
		}
	}

	// MovementDirection — the directional clip selector. Cheap dot/sign decision for F/B/L/R.
	if (ChooserContext.bIsMoving)
	{
		// Reference frame:
		//   EXPLORE — velocity vs the BODY (orient-to-movement; the body faces where it goes).
		//   STRAFE  — the KEYBOARD INTENT vs the CAMERA. In strafe the body faces the camera, so "W = forward"
		//     is true the instant the key is pressed, regardless of where the body currently points. Classifying
		//     by velocity-vs-body instead would read a side/back clip on move-start (body still rotating to the
		//     camera, velocity lagging) and only correct once the body caught up — the "walks sideways then snaps
		//     forward" bug. Using intent-vs-camera makes the FIRST start clip correct, no align stall needed.
		//     Falls back to velocity for the STOP (input released → MoveIntentWS is zero) so the directional stop
		//     clip still matches the last travel direction.
		FVector RefForward, RefRight, Dir2D;
		if (ChooserContext.bStrafe)
		{
			const FRotator CamYaw(0.f, ChooserContext.AimingRotation.Yaw, 0.f);
			RefForward = CamYaw.Vector();
			RefRight   = FRotationMatrix(CamYaw).GetScaledAxis(EAxis::Y);
			Dir2D      = MoveIntentWS.IsNearlyZero()
				? PredictedFutureVelocity.GetSafeNormal2D()
				: MoveIntentWS.GetSafeNormal2D();
		}
		else
		{
			RefForward = CurrentForward;
			RefRight   = Cached_Pawn->GetActorRightVector();
			Dir2D      = PredictedFutureVelocity.GetSafeNormal2D();   // intent direction (future, not lagging current)
		}
		const float Forward = static_cast<float>(FVector::DotProduct(Dir2D, RefForward));
		const float Right   = static_cast<float>(FVector::DotProduct(Dir2D, RefRight));
		if (FMath::Abs(Forward) > FMath::Abs(Right))
		{
			ChooserContext.MovementDirection = Forward >= 0.f ? EAZ_MovementDirection::F : EAZ_MovementDirection::B;
		}
		else
		{
			// L/R reported without foot-lead specificity (LL / RR) until we wire the
			// foot-leading classifier — the GASP-parity 4-way split (LL/LR/RL/RR) needs
			// a foot-phase signal we don't have yet. Chooser rows for first-pass
			// locomotion can collapse LL+LR / RL+RR with the MultiEnum column.
			ChooserContext.MovementDirection = Right >= 0.f ? EAZ_MovementDirection::RR : EAZ_MovementDirection::LL;
		}
	}
	// When idle, leave MovementDirection as last computed — chooser rows for IdleLoop use Any.

	// Turn-start angle — signed yaw from current facing to the desired (world-space input) heading.
	// This is the selector for the 90/135/180 L/R turn-start clips: at the idle->moving edge the body
	// can face far from where the stick points, and the RM turn-start clip pivots it there. Recomputed
	// every moving frame here; DeriveSMState LATCHES it on the TransitionToLocomotion entry edge (the clip
	// rotates the capsule as it plays, collapsing the live angle — re-bucketing mid-turn would restart it).
	// +ve = turn right (UE yaw: +90 yaw rotates ForwardVector toward RightVector). VERIFY sign in PIE.
	if (ChooserContext.bIsMoving)
	{
		const float FacingYaw  = static_cast<float>(CurrentForward.Rotation().Yaw);
		const float DesiredYaw = static_cast<float>(MoveIntentWS.Rotation().Yaw);
		PendingStartAngleDeg   = FMath::FindDeltaAngleDegrees(FacingYaw, DesiredYaw);
	}

	// Planted-foot — read the contact_l curve from the playing clip (baked 0/1 step, threshold 0.5).
	// Same convention as v1 UAZ_AnimInstance. Drives the BoolColumn that selects L/R stop/start
	// variants; false when the current clip carries no contact curve (idle, break, jump).
	ChooserContext.bLeftFootDown = GetCurveValue(FName(TEXT("contact_l"))) > 0.5f;

	// ---- GAS tag snapshot from the pawn (routes through IGameplayTagAssetInterface → ASC). ----
	ChooserContext.OwnedTags.Reset();
	Cached_Pawn->GetOwnedGameplayTags(ChooserContext.OwnedTags);
	// Strafe / combat-ready — derived from the replicated Movement.Strafe tag (set on equip).
	// Gates the chooser's directional strafe rows. Replicated loose tag -> present on sim proxies too.
	ChooserContext.bStrafe = ChooserContext.OwnedTags.HasTag(FAZ_GameplayTags::Get().Movement_Strafe);
	// Upper-body fists-up combat stance — timed Combat.Ready tag (set on fist equip / refreshed on attack).
	// Drives the spine_02 layered fists-up overlay in the AnimGraph; independent of bStrafe.
	bCombatReady = ChooserContext.OwnedTags.HasTag(FAZ_GameplayTags::Get().Combat_Ready);
	// Ease the fists-up overlay weight here — Layered Blend Per Bone has no built-in alpha interp (raw BlendWeights).
	// Rises to 1 on enable, falls to 0 on disable, with separate in/out speeds. Bind CombatReadyAlpha -> BlendWeights[0].
	CombatReadyAlpha = FMath::FInterpTo(CombatReadyAlpha, bCombatReady ? 1.f : 0.f, DeltaSeconds,
		bCombatReady ? CombatReadyBlendInSpeed : CombatReadyBlendOutSpeed);

	// ---- Camera/facing — for rotation-aware chooser rows (TIP, AO chains) ----
	if (const AController* Controller = Cached_Pawn->GetController())
	{
		ChooserContext.AimingRotation = Controller->GetControlRotation();
	}
	// RotationOffset is signed delta from actor yaw to camera yaw (radians or degrees?
	// Project convention: degrees. Matches FAZ_MoverCustomInputs::RotationOffset units.)
	ChooserContext.RotationOffset = FRotator::NormalizeAxis(
		ChooserContext.AimingRotation.Yaw - Cached_Pawn->GetActorRotation().Yaw);

	// ---- Additive lean (port of AZ_AnimInstance::Update_AdditiveLean) — FORWARD ONLY ----
	// Lateral acceleration (velocity derivative, rotated into velocity-local space) -> a [-1..1] lean signal.
	// Gated to forward movement (both explore and strafe); falls smoothly to 0 otherwise. Consumed by the lean
	// BlendSpace through an Apply-Additive node in the ABP. (Lean is a continuous overlay, NOT an MM pick — the
	// lean clips are pose-tilts with no curving root motion, so MM can't select them; see project_strafe_system.)
	{
		const FVector VelAccel = (Velocity - PrevVelocity) / FMath::Max(0.0001f, DeltaSeconds);
		PrevVelocity = Velocity;
		const FRotator VelRot = Velocity.IsNearlyZero(1.0) ? FRotator::ZeroRotator : Velocity.Rotation();
		const FVector LocalAccel = VelRot.UnrotateVector(VelAccel);
		const float Divisor = static_cast<float>(FMath::GetMappedRangeValueClamped(
			FVector2D(200.f, 320.f), FVector2D(500.f, 800.f), ChooserContext.Speed2D));
		const float Lat = FMath::Clamp(static_cast<float>(LocalAccel.Y) / FMath::Max(1.f, Divisor), -1.f, 1.f);
		// Forward-only gate (both explore + strafe). bForwardLean drives BOTH the BS coordinate AND the layer
		// Alpha: the coordinate alone isn't enough — the additive node would still apply the walk-derived delta
		// at full strength over the IDLE base pose (the "takes a root anim / weird at idle" bug). Gating Alpha to
		// 0 lets the base pose pass through untouched; it ramps to 1 only while walking/running forward.
		// Only in the steady LocomotionLoop — NOT during any transition (start / stop / turn / pivot / air). Those
		// phases play an RM clip that already owns the body; the additive lean stacked on top of root motion reads
		// wrong (it tilts over the planted turn-start). Loops carry ~zero RM, so this == "no lean while RM plays".
		const bool bForwardLean = ChooserContext.bIsMoving
			&& ChooserContext.MovementDirection == EAZ_MovementDirection::F
			&& ChooserContext.SMState == EAZ_StateMachineState::LocomotionLoop;
		const FVector2D Target = bForwardLean ? FVector2D(Lat, 0.f) : FVector2D::ZeroVector;
		LeanAmount = FMath::Vector2DInterpTo(LeanAmount, Target, DeltaSeconds, 10.f);
		LeanAlpha  = FMath::FInterpTo(LeanAlpha, bForwardLean ? 1.f : 0.f, DeltaSeconds, 10.f);
	}

#if !UE_BUILD_SHIPPING
	if (bDebugTrajectory && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this), 0.f, FColor::Cyan,
			FString::Printf(TEXT("[v2 Trajectory] FutureVel2D=%.0f  (curVel2D=%.0f)  samples=%d"),
				PredictedFutureVelocity.Size2D(), ChooserContext.Speed2D, Trajectory.Samples.Num()));
		// Foot diagnostic: are the walk loop's contact curves reaching GetCurveValue at runtime?
		// If cL/cR stay 0.00 while walking, the BlendStack isn't propagating curves (→ bLeftFootDown
		// always False → always _LU). If they swing 0↔1, the signal works and we look elsewhere.
		GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this) + 1, 0.f, FColor::Yellow,
			FString::Printf(TEXT("[v2 Foot] contact_l=%.2f contact_r=%.2f  bLeftFootDown=%d  SM=%d  bIsMoving=%d"),
				GetCurveValue(FName(TEXT("contact_l"))), GetCurveValue(FName(TEXT("contact_r"))),
				ChooserContext.bLeftFootDown ? 1 : 0,
				static_cast<int32>(ChooserContext.SMState), ChooserContext.bIsMoving ? 1 : 0));
	}
#endif

	// ---- Phase derivation (the "C++ SM") — now owned by UAZ_LocomotionStateMachine. We resolve all role /
	// mode / jump-edge awareness HERE (at the boundary) and hand it in; the SM stays a pure decision function,
	// and applies the latch lifetime-gating (StartDirection / bMovingTransition / bJustLanded) inside Tick. ----
	{
		const UWorld* SMWorld = GetWorld();
		FAZ_LocoSMInputs SMIn;
		SMIn.WorldNow             = SMWorld ? SMWorld->GetTimeSeconds() : 0.f;
		SMIn.bIsMoving            = ChooserContext.bIsMoving;
		// Full movement-mode enum (engine Falling/RMAction map to InAir above; Slide/Traversing become SM
		// cases when those modes land). Persistent replicated STATE, so the SM derives the whole air phase
		// identically on simulated proxies and the authority — there is no jump-press edge to read and no
		// proxy-only mirror branch (that was the old one-shot-edge RM jump, which proxies routinely missed).
		SMIn.MovementMode         = ChooserContext.MovementMode;
		// Vehicle/driver-pose pin — set by gameplay code on vehicle enter/exit; SM holds IdleLoop while true.
		SMIn.bSuppressLocomotion  = bSuppressLocomotion;
		// HYBRID JUMP: hold TransitionToInAir while the RM rise (RMAction) owns the capsule, so the
		// rising Start clip keeps driving until the apex handoff to Falling (then InAirLoop + air MM).
		// Typed: keyed off the mode's bHandOffToFallingAtApex, not the bare mode name — a vault/mantle
		// RMAction (plays to completion, never hands to Falling) must not be held as a jump takeoff.
		SMIn.bHoldTakeoffPhase    = bHybridJumpActive && bRMActionIsJumpRise;
		SMIn.PendingStartAngleDeg = PendingStartAngleDeg;
		SMIn.bStrafe              = ChooserContext.bStrafe;   // strafe: directional starts/stops, no body-turning
		SMIn.MovementDirection    = ChooserContext.MovementDirection;   // strafe forward move-start → cosmetic turn-start
		SMIn.IdleBreakMinTime     = IdleBreakMinTime;
		SMIn.IdleBreakMaxTime     = IdleBreakMaxTime;
		SMIn.Stance				  = ChooserContext.Stance;

		// Defensive: StateMachine is created in NativeInitializeAnimation, but Live Coding re-instancing (and any
		// path that ticks an AnimInstance whose Init didn't run) can leave this Transient pointer null — calling
		// ->Tick() on null then faults inside ComputeNextState reading this->PreviousState. Lazily re-create it.
		if (!StateMachine)
		{
			StateMachine = NewObject<UAZ_LocomotionStateMachine>(this);
		}
		const FAZ_LocoSMOutputs SMOut = StateMachine->Tick(SMIn);
		ChooserContext.SMState           = SMOut.State;
		ChooserContext.StartDirection    = SMOut.StartDirection;
		ChooserContext.bMovingTransition = SMOut.bMovingTransition;
		ChooserContext.bJustLanded       = SMOut.bJustLanded;

		// FromStance — the last SETTLED stance (pre-transition). With two stances the chooser's "from" is
		// implied by the Stance column; a third stance (prone) makes Stand2Prone vs Crouch2Prone undecidable
		// without it, so surface it now (audit scalability pre-work). During TransitionStance the SM's settled
		// stance still holds the OLD stance — exactly the "from" the chooser row wants.
		ChooserContext.FromStance = StateMachine->GetSettledStance();

		// Transition-entry token for the push lock in SetBlendStackAnimFromChooser (see header).
		if (ChooserContext.SMState != PreviousSMState)
		{
			++TransitionSerial;
		}
	}

	// ---- Foot/intent latch: hold the takeoff foot + move-intent stable through the air ----
	// bLeftFootDown is curve-driven (contact_l) and jump clips carry NO foot-contact curve, so it goes
	// false/oscillates airborne. While GROUNDED, remember the planted foot + move-intent; while AIRBORNE, hold
	// those values so the LAND rows pick the LU/RU land clip matching the takeoff foot chain at touchdown, and
	// bIsMoving distinguishes the standing land (JumpIdleLand) from Land2Walk/Land2Run. (Final 2-clip jump
	// design: the air pushes nothing — the Start clip's tail carries the fall — so the latch's only consumer
	// is land-row selection.)
	{
		const bool bAirState =
			ChooserContext.SMState == EAZ_StateMachineState::TransitionToInAir ||
			ChooserContext.SMState == EAZ_StateMachineState::InAirLoop;
		if (!bAirState)
		{
			LastGroundedLeftFootDown = ChooserContext.bLeftFootDown;
			LastGroundedIsMoving     = ChooserContext.bIsMoving;
		}
		else
		{
			ChooserContext.bLeftFootDown = LastGroundedLeftFootDown;
			ChooserContext.bIsMoving     = LastGroundedIsMoving;
		}
	}

	// A′ RM-move teardown: whenever we leave a transition phase — whether it completed normally or was
	// abandoned early (e.g. input re-pressed mid-stop) — cancel the per-transition RM move so it can't keep
	// OverrideAll-driving the resumed loop until its DurationMs auto-expires. Our move is tagged
	// Mover.AnimRootMotion.MeshAttribute (child of Mover_AnimRootMotion); a non-exact cancel matches it.
	// No-op when no such move is active. (CancelFeaturesWithTag only touches layered moves/modifiers.)
	auto IsTransitionPhase = [](EAZ_StateMachineState S)
	{
		return S == EAZ_StateMachineState::TransitionToIdle || S == EAZ_StateMachineState::TransitionToLocomotion;
	};
	// Sim proxies never queued an RM move (gated above), so there's nothing to cancel — and they must not
	// touch their Mover (the capsule is replication-driven). Gate the teardown to the simulating machine too.
	if (Cached_Pawn->GetLocalRole() != ROLE_SimulatedProxy &&
		IsTransitionPhase(PreviousSMState) && !IsTransitionPhase(ChooserContext.SMState))
	{
		Cached_MoverComponent->CancelFeaturesWithTag(Mover_AnimRootMotion, /*bRequireExactMatch*/ false);
	}

	// ---- Jump mode flow (HYBRID: RM rise → physics fall; toggleable) ----
	// bUseHybridJump=true (UAZ_PawnMoverComponent): jump press → Jump() queues RMAction → the takeoff Start
	// clip's root motion lifts the capsule through anticipation+rise (the RM bridge below queues the
	// OverrideAll move for TransitionToInAir) → RMAction switches to Falling at the clip apex → physics
	// descent (terrain-adaptive) → land on real floor contact. The teardown above kills the RM move on the
	// apex edge. bUseHybridJump=false: pure physics — gait-scaled impulse, Walking→Falling, no RM move
	// (the old float-then-drop fix path, kept for A/B).
}

void UAZ_MoverAnimInstance::SetBlendStackAnimFromChooser(
	bool bForceBlend,
	FAnimNodeReference BlendStackNode,
	FAZ_ChooserOutputs ChooserOut,
	UAnimationAsset* ChosenAnim,
	const TArray<UObject*>& Candidates)
{
	// ChooserContext.SMState (written by the StateMachine in NativeUpdateAnimation) is the single source
	// of truth for the SM phase — never assign it from a caller-supplied value here. The old `State`
	// parameter was removed for exactly that reason: a stale EventGraph wire (e.g. a literal IdleLoop on
	// the pin) could clobber the derived phase, making IdleBreak flash one tick then revert.
	ChooserOutputs = ChooserOut;

	// Nothing to play: first-match mode gives ChosenAnim, return-all mode gives Candidates.
	// Bail only when both are empty.
	if (!ChosenAnim && Candidates.Num() == 0)
	{
		return;
	}

	// NOTE (2026-06-06, jump air RESOLVED): the whole in-air "replay/held" saga was NEVER a push/timing problem.
	// The ABP's Blend Stack node had a second FULL Blend Stack node inside its per-sample inner graph (instead of
	// a Blend Stack Input node): every sample spawned a fresh inner stack that played the bound Anim from its
	// literal AnimationTime=0, so the VISIBLE pose ignored every StartTime / MM SelectedTime written here, while
	// the outer node (the one this function drives) rendered to nobody. Fixed by swapping the inner node for
	// Blend Stack Input — StartTime and MM results now reach the screen. The earlier "mid-air pushes never
	// advance" theory is DISPROVEN; the InAirLoop chooser rows are viable again (direct-play from apex StartTime
	// or single-clip MM). See project_jump_system_status § TRUE ROOT CAUSE 2026-06-06.

	// Transition clip lock: once a start/stop clip is pushed, let it play to completion (DeriveSMState's
	// TransitionEndTime governs the fall-through to the target loop). Without this, the stop clip's own
	// contact_l curve would keep flipping bLeftFootDown, re-trigger the chooser, and restart the stop on
	// the other foot every few frames. The entry frame (SMState just changed from the loop) is NOT locked
	// — only subsequent frames where we're still in the same transition.
	const bool bInTransition =
				ChooserContext.SMState == EAZ_StateMachineState::TransitionToIdle ||
				ChooserContext.SMState == EAZ_StateMachineState::TransitionToLocomotion ||
				ChooserContext.SMState == EAZ_StateMachineState::TransitionToInAir ||
				ChooserContext.SMState == EAZ_StateMachineState::TransitionStance;

	// Serial-keyed: each SMState CHANGE mints a new TransitionSerial (game thread); a committed push stamps
	// it. Equal serials = "this same transition entry already pushed" → locked. Comparing raw SMState here
	// let a bailed push's stale cache suppress a later same-state transition entirely (audit P0-3).
	if (bInTransition && TransitionSerial == LastPushedTransitionSerial && !bForceBlend)
	{
		return;
	}

	// Short-circuit: if the chooser's selection inputs haven't changed since the last
	// successful push, skip re-pushing to BlendStack. Without this, RandomizeColumn rows
	// (e.g. 3 idle-break variants) churn the stack every tick because the random pick
	// re-rolls per evaluation. Bypassed when:
	//   - bForceBlend is true (caller explicitly asked for a fresh blend)
	//   - bUseMM is true (motion matching intentionally re-picks each frame)
	const bool bSelectionChanged =
		ChooserContext.SMState           != LastPushedSMState ||
		ChooserContext.Stance            != LastPushedStance ||
		ChooserContext.Gait              != LastPushedGait ||
		ChooserContext.MovementDirection != LastPushedDir ||
		ChooserContext.bLeftFootDown     != LastPushedLeftFootDown;

	if (!bSelectionChanged && !bForceBlend && !ChooserOut.bUseMM && BlendStackInputs.Anim != nullptr)
	{
		return;
	}

	// NOTE: the LastPushed* cache is committed AFTER the push lands (next to the PendingBlendOut refresh
	// below) — every abort path between here and there must leave the cache untouched so the next frame
	// retries instead of wedging behind the transition lock (audit P0-2).

	bool bAssetLooping = false;

	// Outgoing-wins blend-out: if the clip we're REPLACING authored a BlendOut (one-shot stash from its own
	// push), that clip owns this crossfade; otherwise the incoming clip's BlendTime governs (standard). The
	// stash is refreshed to THIS clip's BlendOut after the push commits, below.
	const float Crossfade = (PendingBlendOut > 0.f)
		? PendingBlendOut
		: static_cast<float>(ChooserOut.BlendTime);

	if (ChooserOut.bUseMM)
	{
		// MotionMatch over the chooser's candidate set — picks both the clip (when several are
		// supplied) and the best entry frame by current pose+trajectory cost. When the chooser
		// runs in first-match mode it hands us one anim via ChosenAnim and an empty Candidates
		// array, so fall back to searching just {ChosenAnim}. Requires a PoseHistory node tagged
		// "PoseHistory" in the AnimGraph.
		TArray<UObject*> AssetsToSearch = Candidates;
		if (AssetsToSearch.Num() == 0)
		{
			AssetsToSearch.Add(ChosenAnim);
		}

		// Strafe directional loop: search the full 8-way strafe DB so MM picks the directional clip (incl. the
		// 45/135 diagonals) by trajectory, instead of refining the entry frame within the single chooser-picked
		// clip. Keyed off CONTEXT, not the matched row — any bUseMM strafe loco row routes here. Gait-gated:
		// walk-speed set for Walk, run-speed set for Run/Sprint, so the clip speed matches the Mover's gait-
		// driven move speed (no foot-slide). Null DB → falls through to the direct single-clip MM below.
		if (ChooserContext.bStrafe && ChooserContext.SMState == EAZ_StateMachineState::LocomotionLoop)
		{
			// Crouch takes priority (gait-agnostic — one crouch speed); else gait-gated walk/run. (Strafe DBs
			// also carry the forward-lean clips, so a forward-curving strafe corner gets the lean too.)
			UPoseSearchDatabase* StrafeDB;
			if (ChooserContext.Stance == EAZ_Stance::Crouching)
				StrafeDB = StrafeCrouchDatabase.Get();
			else
				StrafeDB = (ChooserContext.Gait == EAZ_Gait::Walk) ? StrafeWalkDatabase.Get() : StrafeRunDatabase.Get();
			if (StrafeDB)
			{
				AssetsToSearch.Reset();
				AssetsToSearch.Add(StrafeDB);
			}
		}
		else if (ChooserContext.SMState == EAZ_StateMachineState::LocomotionLoop
			&& ChooserContext.Stance == EAZ_Stance::Standing)
		{
			// EXPLORE forward cornering lean: standing loco searches the per-gait loco DB (Fwd + LeanL/R) so MM
			// picks the lean variant when the trajectory curves. Gait-gated; crouch falls through to single-clip.
			UPoseSearchDatabase* LocoDB =
				(ChooserContext.Gait == EAZ_Gait::Walk) ? WalkLocoDatabase.Get() : RunLocoDatabase.Get();
			if (LocoDB)
			{
				AssetsToSearch.Reset();
				AssetsToSearch.Add(LocoDB);
			}
		}

		// FINAL JUMP DESIGN (2026-06-07): the air NEVER pushes — there are no InAirLoop chooser rows. The
		// jump is two clips following the walk start/stop pattern: the Start clip (RM rise drives the capsule
		// to the apex; its tail plays cosmetically under the physics fall) and the Land clip (bUseMM row —
		// the single-clip search below picks the impact ENTRY FRAME via the clip's BranchIn->PSD_v2_Jump
		// link). So this MM path serves exactly two callers: the gait-gated loco loops and the jump lands —
		// both single-clip entry-frame refinement, no continuing pose needed (loops self-stabilize, lands
		// push once on the touchdown edge). The old air-MM machinery (whole-DB search, single-clip air
		// search, continuing-pose tracking) is RETIRED — see project_jump_system_status.

		// v2 loop MM is GAIT-GATED (decision 2026-06-01): MM searches only the chooser-picked clip for the
		// CURRENT gait (WalkFwdLoop on the walk row, RunFwdLoop on the run row) and picks the best entry FRAME
		// within that one loop — it never crosses walk<->run. Rationale: gait is tag-driven (Movement.Running),
		// so a hard turn that momentarily cuts forward speed must NOT let trajectory-based cross-clip MM
		// downgrade a held Run to the walk cycle. Walk<->run changes now ride the gait tag flipping the chooser
		// row + the BlendStack cross-fade. (The old cross-clip walk+run search over LocomotionLoopDatabase is
		// intentionally disabled — re-enable here, GATED PER-GAIT, only if we want the speed-blended gear-change
		// back; the DB UPROPERTY + ABP CDO assignment are kept so the asset still cooks.)

		FPoseSearchBlueprintResult MMResult;
		UPoseSearchLibrary::MotionMatch(
			this, AssetsToSearch, FName("PoseHistory"),
			FPoseSearchContinuingProperties(), FPoseSearchFutureProperties(),	
			MMResult);

		UAnimationAsset* MMAnim = Cast<UAnimationAsset>(MMResult.SelectedAnim);
		double MMStartTime = MMResult.SelectedTime;
		if (!MMAnim)
		{
			// MM returned nothing (e.g. LocomotionLoopDatabase not assigned yet, or an empty search result) —
			// fall back to the chooser's direct clip from frame 0 so the loop never FREEZES. Without this, a
			// bUseMM=True row with no resolvable search target would push no anim and lock the current pose.
			MMAnim = ChosenAnim ? ChosenAnim : (Candidates.Num() > 0 ? Cast<UAnimationAsset>(Candidates[0]) : nullptr);
			MMStartTime = 0.0;
			if (!MMAnim)
			{
				return;
			}
		}
		// MMCostLimit: when > 0, a real MM result must beat that cost or we keep the current anim (skipped on
		// the fallback path above, where there's no meaningful search cost).
		else if (ChooserOut.MMCostLimit > 0.0 && MMResult.SearchCost > ChooserOut.MMCostLimit)
		{
			return;
		}

		UPoseSearchLibrary::IsAnimationAssetLooping(MMAnim, bAssetLooping);

		BlendStackInputs.Anim         = MMAnim;
		BlendStackInputs.bLoop        = bAssetLooping;
		BlendStackInputs.StartTime    = MMStartTime;
		BlendStackInputs.BlendTime    = Crossfade;
		BlendStackInputs.BlendProfile = const_cast<UBlendProfile*>(GetBlendProfileByName(ChooserOut.BlendProfile));
		BlendStackInputs.Tags         = ChooserOut.Tags;
	}
	else
	{
		// Non-MM path: take the chooser-supplied asset + timing/profile/tags directly.
		// Prefer the single Result pin; fall back to the first candidate if only the array is wired.
		UAnimationAsset* DirectAnim = ChosenAnim ? ChosenAnim : Cast<UAnimationAsset>(Candidates[0]);
		if (!DirectAnim)
		{
			return;
		}
		UPoseSearchLibrary::IsAnimationAssetLooping(DirectAnim, bAssetLooping);

		BlendStackInputs.Anim         = DirectAnim;
		BlendStackInputs.bLoop        = bAssetLooping;
		BlendStackInputs.StartTime    = ChooserOut.StartTime;
		BlendStackInputs.BlendTime    = Crossfade;
		BlendStackInputs.BlendProfile = const_cast<UBlendProfile*>(GetBlendProfileByName(ChooserOut.BlendProfile));
		BlendStackInputs.Tags         = ChooserOut.Tags;
	}

	// ---- COMMITTED-PUSH bookkeeping: everything from here down runs only when an anim actually landed in
	// BlendStackInputs (every no-push path returned above). ----

	// Per-push cache + transition serial stamp — gates re-pushes within the same logical context / the same
	// transition entry. Committed here, NOT before the push: an aborted push must leave them stale so the
	// next frame retries (audit P0-2/P0-3).
	LastPushedSMState          = ChooserContext.SMState;
	LastPushedStance           = ChooserContext.Stance;
	LastPushedGait             = ChooserContext.Gait;
	LastPushedDir              = ChooserContext.MovementDirection;
	LastPushedLeftFootDown     = ChooserContext.bLeftFootDown;
	LastPushedTransitionSerial = TransitionSerial;

	// Refresh the one-shot stash with THIS clip's blend-out, so the NEXT push (which replaces it) honors it.
	// Reached only on a committed push, so PendingBlendOut survives across the transition lock until the clip
	// is actually replaced.
	PendingBlendOut = static_cast<float>(ChooserOut.BlendOut);

	// Idle-break duration tracking — when an IdleBreak anim is pushed, record when DeriveSMState
	// should flip back to IdleLoop. Using world time + (anim length - threshold) lets the
	// BlendStack cross-fade begin before the break anim's last frame, hiding the cut.
	// Reads ChooserContext.SMState (not the parameter) so a misconfigured EventGraph pin
	// can't suppress break tracking.
	if (ChooserContext.SMState == EAZ_StateMachineState::IdleBreak)
	{
		if (const UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(BlendStackInputs.Anim))
		{
			const UWorld* World = GetWorld();
			const float Now = World ? World->GetTimeSeconds() : 0.f;
			if (StateMachine)
			{
				// Remaining = Len - StartTime, same as the transition path below — a break row authored
				// with StartTime > 0 must not hold IdleBreak past the clip's actual end (audit P2-19).
				const float BreakRemaining = FMath::Max(0.05f,
					Seq->GetPlayLength() - static_cast<float>(BlendStackInputs.StartTime));
				StateMachine->NotifyIdleBreakClipPushed(Now, BreakRemaining, IdleBreakAlmostCompleteThreshold);
			}
		}
	}

	// Transition (start/stop) tracking + A′ RM-bridge handoff. Reached only on the transition's ENTRY
	// frame (subsequent frames hit the transition lock above and return early). (1) Schedule the
	// fall-through to the target loop, and (2) flag NativeUpdateAnimation (game thread) to queue the
	// per-transition RM move for the clip's REMAINING length (Len - StartTime — StartTime is non-zero
	// when MM picks an entry frame).
	if (bInTransition)
	{
#if !UE_BUILD_SHIPPING
		// DIAGNOSTIC: which transition phase did we enter, what did the chooser see, and what clip did it pick?
		UE_LOG(LogTemp, Verbose, TEXT("[v2 TRANS] entry: SM=%d ctx.bLeftFootDown=%d Dir=%d Gait=%d -> chosen=%s"),
			static_cast<int32>(ChooserContext.SMState), ChooserContext.bLeftFootDown ? 1 : 0,
			static_cast<int32>(ChooserContext.MovementDirection),
			static_cast<int32>(ChooserContext.Gait), *GetNameSafe(BlendStackInputs.Anim));
#endif
		if (const UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(BlendStackInputs.Anim))
		{
			const UWorld* World = GetWorld();
			const float Now = World ? World->GetTimeSeconds() : 0.f;
			const float Remaining = FMath::Max(0.05f,
				Seq->GetPlayLength() - static_cast<float>(BlendStackInputs.StartTime));
			if (StateMachine)
			{
				StateMachine->NotifyTransitionClipPushed(Now, Remaining, TransitionAlmostCompleteThreshold);
			}
			// RM bridge for ground start/stop/turn transitions — AND, under the HYBRID jump, for the takeoff
			// RISE. Hybrid (bHybridJumpActive): TransitionToInAir queues the OverrideAll RM move so the Start
			// clip's baked arc lifts the capsule through anticipation+rise; duration = the full remaining clip,
			// because the actual end is the APEX — RMAction switches itself to Falling there and the teardown in
			// NativeUpdateAnimation cancels this move on that observed edge. Landings stay physics-driven always.
			// Pure-physics mode (bHybridJumpActive=false): jumps queue nothing (impulse owns the capsule; an RM
			// move would fight gravity and re-introduce the float-then-drop divergence).
			const bool bHybridJumpRise =
				bHybridJumpActive &&
				ChooserContext.SMState == EAZ_StateMachineState::TransitionToInAir &&
				!ChooserContext.bJustLanded;
			
			const bool bPhysicsDrivenTransition =
				(ChooserContext.SMState == EAZ_StateMachineState::TransitionToInAir ||
				ChooserContext.bJustLanded) && 
				!bHybridJumpRise;
			
			const bool bInPlaceStanceTransition =
				ChooserContext.SMState == EAZ_StateMachineState::TransitionStance;

			// RM-drive ONLY the strafe TURN-bucket starts (StartDirection L90/R90/L135/R135/L180/R180): the SM
			// bucketed those to the body->camera angle so the clip's baked turn already aims at the camera, and the
			// OverrideAll RM grounds the turn (planted feet) landing the body ~on camera = explore's "Root-motion
			// turn" start. The Fwd bucket has NO baked turn (small-angle forward AND every sideways/back start, which
			// force StartDirection=Fwd), so RM there would hold a stale facing the whole clip then snap after = the
			// old "double". Keep those cosmetic so the facing spring aligns from the first moving frame.
			const bool bStrafeNoTurnStart =
				ChooserContext.bStrafe
				&& ChooserContext.SMState == EAZ_StateMachineState::TransitionToLocomotion
				&& ChooserContext.StartDirection == EAZ_StartDirection::Fwd;

			if (!bPhysicsDrivenTransition && !bInPlaceStanceTransition && !bStrafeNoTurnStart)
			{
				PendingTransitionRMMoveDurationMs = Remaining * 1000.f;
				bPendingTransitionRMMove = true;
			}
		}
	}

	// Force a fresh blend if the caller requested it AND we're not playing the same loop
	// (re-entering the same loop should not re-blend — matches v1's pivot-failure carve-out).
	if (bForceBlend && !BlendStackInputs.bLoop)
	{
		EAnimNodeReferenceConversionResult ConvResult;
		const FBlendStackAnimNodeReference BSNode =
			UBlendStackAnimNodeLibrary::ConvertToBlendStackNode(BlendStackNode, ConvResult);
		if (ConvResult == EAnimNodeReferenceConversionResult::Succeeded)
		{
			UBlendStackAnimNodeLibrary::ForceBlendNextUpdate(BSNode);
		}
	}
}
