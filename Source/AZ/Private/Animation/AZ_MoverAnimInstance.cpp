// Copyright Artur. AZ project.

#include "Animation/AZ_MoverAnimInstance.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/AZ_LocomotionStateMachine.h"
#include "BlendStack/BlendStackAnimNodeLibrary.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
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
		CachedPawn = Cast<AAZ_PawnMoverHeroCharacter>(PawnOwner);
		if (CachedPawn)
		{
			CachedMover = CachedPawn->GetMoverComponent();
			CachedCMC = Cast<UCharacterMoverComponent>(CachedMover);
		}
	}
}

void UAZ_MoverAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	LastUpdateDeltaSeconds = DeltaSeconds;   // cached for the jump MM continuing-pose advance (thread-safe reader)

	if (!CachedPawn || !CachedMover)
	{
		return;
	}

	// ---- A′ per-transition RM bridge (game thread) ----
	// SetBlendStackAnimFromChooser (possibly on the anim worker thread) flags this when it pushes a
	// start/stop clip. Queue the OverrideAll root-motion-attribute move here, on the game thread, with
	// DurationMs = the transition clip's remaining length so it auto-expires when the transition ends.
	// Loops queue nothing → the RootMotionDelta attribute is written but unconsumed → velocity-driven.
	// See project_root_motion_mode (approach A′).
	if (bPendingTransitionRMMove)
	{
		bPendingTransitionRMMove = false;   // consume the flag on every machine (it was set on the proxy too)
		// ONLY the simulating machine (authority / autonomous proxy) drives the capsule from root motion. A
		// SIMULATED proxy's capsule follows the REPLICATED, interpolated transform — queuing the OverrideAll
		// root-motion move there makes the proxy try to RM-drive a capsule that's also being interpolated, and
		// the two fight every frame → visibly JERKY proxy motion (worst on the fast vertical jump arc). The
		// proxy still plays the clip locally with RootMotionFromEverything, which extracts the root IN PLACE,
		// so the mesh animates correctly while the replicated transform carries the world motion. Smooth.
		if (CachedPawn->GetLocalRole() != ROLE_SimulatedProxy)
		{
			// A reversal chains two transition clips (stop → turn-start) back-to-back with no intervening
			// non-transition frame, so the teardown at the bottom of this function never fires between them.
			// Cancel any still-active prior transition RM move before queuing the new one, else the stop's and
			// the start's FLayeredMove_RootMotionAttribute could both be live and double-drive the capsule.
			// No-op in the normal idle→start / loop→stop case (no prior anim-RM move was queued).
			CachedMover->CancelFeaturesWithTag(Mover_AnimRootMotion, /*bRequireExactMatch*/ false);
			TSharedPtr<FLayeredMove_RootMotionAttribute> RMMove = MakeShared<FLayeredMove_RootMotionAttribute>();
			RMMove->DurationMs = PendingTransitionRMMoveDurationMs;
			CachedMover->QueueLayeredMove(RMMove);
		}
	}

	const EAZ_StateMachineState PreviousSMState = ChooserContext.SMState;

	// ---- Trajectory (option A — PoseSearch FTransformTrajectory via the Mover predictor) ----
	// SINGLE source: feeds the AnimGraph PoseHistory node (MM) via the "Trajectory" property binding AND
	// the intent-based IsMoving below. Mirrors v1 Update_Trajectory. The predictor is Setup on the pawn in
	// BeginPlay. Future velocity is finite-differenced from the predicted samples (the sample struct carries
	// position/facing/time, not velocity), giving a leading indicator that catches start/stop on intent.
	if (UMoverTrajectoryPredictor* Predictor = CachedPawn->GetTrajectoryPredictor())
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
	const FVector Velocity = CachedMover->GetVelocity();
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
		const FMoverInputCmdContext& LastInput = CachedMover->GetLastInputCmd();
		if (const FCharacterDefaultInputs* CharIn = LastInput.InputCollection.FindDataByType<FCharacterDefaultInputs>())
		{
			MoveIntentWS = CharIn->GetMoveInput_WorldSpace();
		}
	}
	ChooserContext.bIsMoving = MoveIntentWS.SizeSquared2D() > FMath::Square(0.1f);   // small input deadzone

	// MovementMode from the active mode's registered name (set on the MoverComponent's MovementModes map).
	const FName ModeName = CachedMover->GetMovementModeName();
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
		// RMAction (FUTURE vault/mantle — contextual RM warped to measured geometry) is airborne; report InAir
		// so the SM air phase + trajectory/other systems treat it like Falling. Jumps no longer use this mode
		// (they are physics/Falling now), so today this branch is dormant until traversal lands.
		ChooserContext.MovementMode = EAZ_MovementMode::InAir;
	}
	// Slide / Swim left as default until those modes exist.

	// Stance — CharacterMoverComponent owns crouch state.
	ChooserContext.Stance = (CachedCMC && CachedCMC->IsCrouching())
		? EAZ_Stance::Crouching
		: EAZ_Stance::Standing;

	// Gait — read the authoritative INTENT from the Mover input cmd (the same FAZ_MoverCustomInputs.Gait
	// the walking mode uses to pick WalkSpeed/RunSpeed/SprintSpeed), NOT re-derived from measured speed.
	// Intent-based so a from-idle run start selects the Run chooser rows on frame 1 (speed is still ~0 then,
	// which would mis-read as Walk), and so a mid-locomotion walk->run flips rows immediately instead of
	// lagging until speed crosses a threshold. Mirrors how bIsMoving above reads the input cmd. The gait is
	// produced in ProduceInput from the player's Movement.* GAS tags. Falls back to Walk when absent (e.g.
	// sim proxies without bSyncInputsForSimProxy — same pending MP fix as bIsMoving).
	const FVector CurrentForward = CachedPawn->GetActorForwardVector();
	{
		const FMoverInputCmdContext& GaitInput = CachedMover->GetLastInputCmd();
		if (const FAZ_MoverCustomInputs* Custom = GaitInput.InputCollection.FindDataByType<FAZ_MoverCustomInputs>())
		{
			ChooserContext.Gait = Custom->Gait;
		}
		else
		{
			ChooserContext.Gait = EAZ_Gait::Walk;
		}
	}

	// MovementDirection — relative to actor forward. Cheap dot/sign decision for F/B/L/R.
	if (ChooserContext.bIsMoving)
	{
		const FVector VelDir2D = PredictedFutureVelocity.GetSafeNormal2D();   // intent direction (future, not lagging current)
		const float Forward = static_cast<float>(FVector::DotProduct(VelDir2D, CurrentForward));
		const float Right   = static_cast<float>(FVector::DotProduct(VelDir2D, CachedPawn->GetActorRightVector()));
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
	CachedPawn->GetOwnedGameplayTags(ChooserContext.OwnedTags);

	// ---- Camera/facing — for rotation-aware chooser rows (TIP, AO chains) ----
	if (const AController* Controller = CachedPawn->GetController())
	{
		ChooserContext.AimingRotation = Controller->GetControlRotation();
	}
	// RotationOffset is signed delta from actor yaw to camera yaw (radians or degrees?
	// Project convention: degrees. Matches FAZ_MoverCustomInputs::RotationOffset units.)
	ChooserContext.RotationOffset = FRotator::NormalizeAxis(
		ChooserContext.AimingRotation.Yaw - CachedPawn->GetActorRotation().Yaw);

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
		// Airborne = MovementMode InAir (engine Falling for physics jumps; RMAction for future vault/mantle).
		// This is persistent replicated STATE, so the SM derives the whole air phase identically on simulated
		// proxies and the authority — there is no jump-press edge to read and no proxy-only mirror branch (that
		// was the old one-shot-edge RM jump, which proxies routinely missed).
		SMIn.bInAirMode           = (ChooserContext.MovementMode == EAZ_MovementMode::InAir);
		SMIn.PendingStartAngleDeg = PendingStartAngleDeg;
		SMIn.IdleBreakMinTime     = IdleBreakMinTime;
		SMIn.IdleBreakMaxTime     = IdleBreakMaxTime;

		const FAZ_LocoSMOutputs SMOut = StateMachine->Tick(SMIn);
		ChooserContext.SMState           = SMOut.State;
		ChooserContext.StartDirection    = SMOut.StartDirection;
		ChooserContext.bMovingTransition = SMOut.bMovingTransition;
		ChooserContext.bJustLanded       = SMOut.bJustLanded;
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
	if (CachedPawn->GetLocalRole() != ROLE_SimulatedProxy &&
		IsTransitionPhase(PreviousSMState) && !IsTransitionPhase(ChooserContext.SMState))
	{
		CachedMover->CancelFeaturesWithTag(Mover_AnimRootMotion, /*bRequireExactMatch*/ false);
	}

	// ---- Jump mode handoff: NONE (physics jump) ----
	// Jumps are PHYSICS-driven now: the GA jump packs bIsJumpJustPressed → the engine Walking mode (bHandleJump,
	// SetHandleJump(true) on the pawn) applies the impulse and transitions Walking → Falling; the engine Falling
	// mode does gravity / air-control and plants back into Walking on REAL floor contact. The AnimInstance no
	// longer drives the Mover mode for jumps (the old QueueNextMode("RMAction")/("Walking") handoff is removed),
	// so the descent adapts to actual terrain height — the float-then-drop divergence fix.
	// UAZ_PawnMovementMode_RMAction is retained for the FUTURE vault/mantle path (contextual RM warped to
	// measured geometry), where flat-ground baking is not an issue — see project_traversal_system.
}

void UAZ_MoverAnimInstance::SetBlendStackAnimFromChooser(
	EAZ_StateMachineState State,
	bool bForceBlend,
	FAnimNodeReference BlendStackNode,
	FAZ_ChooserOutputs ChooserOut,
	UAnimationAsset* ChosenAnim,
	const TArray<UObject*>& Candidates)
{
	// NOTE: do NOT assign ChooserContext.SMState from `State`. DeriveSMState (called from
	// NativeUpdateAnimation) is the single source of truth for the SM phase. The `State`
	// parameter is kept for BP-signature stability but is advisory only — assigning it back
	// here would let an EventGraph wiring (e.g. literal IdleLoop on the pin) clobber the
	// derived phase, causing IdleBreak to flash for one tick then revert to IdleLoop.
	ChooserOutputs = ChooserOut;

	// Nothing to play: first-match mode gives ChosenAnim, return-all mode gives Candidates.
	// Bail only when both are empty.
	if (!ChosenAnim && Candidates.Num() == 0)
	{
		return;
	}

	// Transition clip lock: once a start/stop clip is pushed, let it play to completion (DeriveSMState's
	// TransitionEndTime governs the fall-through to the target loop). Without this, the stop clip's own
	// contact_l curve would keep flipping bLeftFootDown, re-trigger the chooser, and restart the stop on
	// the other foot every few frames. The entry frame (SMState just changed from the loop) is NOT locked
	// — only subsequent frames where we're still in the same transition.
	const bool bInTransition =
		ChooserContext.SMState == EAZ_StateMachineState::TransitionToIdle ||
		ChooserContext.SMState == EAZ_StateMachineState::TransitionToLocomotion ||
		ChooserContext.SMState == EAZ_StateMachineState::TransitionToInAir;
	if (bInTransition && ChooserContext.SMState == LastPushedSMState && !bForceBlend)
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

	LastPushedSMState        = ChooserContext.SMState;
	LastPushedStance         = ChooserContext.Stance;
	LastPushedGait           = ChooserContext.Gait;
	LastPushedDir            = ChooserContext.MovementDirection;
	LastPushedLeftFootDown   = ChooserContext.bLeftFootDown;

	bool bAssetLooping = false;

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

		// Jump MM: in the air states, search the WHOLE JumpDatabase (start->air->land clips) instead of the
		// chooser's single clip, so MM walks the jump arc by the physics trajectory (incl. vertical). Jump clips
		// are one-shots (NOT self-stabilizing loops), so we also feed the continuing pose — the currently-playing
		// jump clip + its accumulated time — so the DB's continuing_pose_cost_bias keeps the search ADVANCING
		// through the clip instead of snapping back to frame 0 (the mid-air "restart"). Loco loops keep the empty
		// continuing props: a loop self-stabilizes (every frame matches the cyclic pose and advances naturally).
		const bool bJumpAirMM =
			JumpDatabase != nullptr &&
			(ChooserContext.SMState == EAZ_StateMachineState::TransitionToInAir ||
			 ChooserContext.SMState == EAZ_StateMachineState::InAirLoop);

		FPoseSearchContinuingProperties Continuing;
		if (bJumpAirMM)
		{
			AssetsToSearch.Reset();
			AssetsToSearch.Add(JumpDatabase);
			Continuing.PlayingAsset = JumpMMContinuingAnim;
			Continuing.PlayingAssetAccumulatedTime = JumpMMContinuingTime + LastUpdateDeltaSeconds;
			Continuing.PlayingAssetDatabase = JumpDatabase;   // helps MM locate the continuing pose in the DB
		}

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
			Continuing, FPoseSearchFutureProperties(),
			MMResult);

		// Track the continuing pose for the NEXT jump MM tick (advanced by LastUpdateDeltaSeconds above); reset
		// outside the air so a fresh jump starts clean.
		if (bJumpAirMM)
		{
			JumpMMContinuingAnim = Cast<UAnimationAsset>(MMResult.SelectedAnim);
			JumpMMContinuingTime = static_cast<float>(MMResult.SelectedTime);
		}
		else
		{
			JumpMMContinuingAnim = nullptr;
			JumpMMContinuingTime = 0.f;
		}

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
		BlendStackInputs.BlendTime    = ChooserOut.BlendTime;
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
		BlendStackInputs.BlendTime    = ChooserOut.BlendTime;
		BlendStackInputs.BlendProfile = const_cast<UBlendProfile*>(GetBlendProfileByName(ChooserOut.BlendProfile));
		BlendStackInputs.Tags         = ChooserOut.Tags;
	}

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
				StateMachine->NotifyIdleBreakClipPushed(Now, Seq->GetPlayLength(), IdleBreakAlmostCompleteThreshold);
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
		UE_LOG(LogTemp, Warning, TEXT("[v2 TRANS] entry: SM=%d ctx.bLeftFootDown=%d Dir=%d Gait=%d -> chosen=%s"),
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
			// RM bridge for GROUND start/stop/turn transitions ONLY. The jump takeoff (TransitionToInAir) and the
			// landings (bJustLanded) are PHYSICS-driven — the engine Falling mode + floor contact own the capsule
			// — so queuing an OverrideAll root-motion move there would fight gravity/velocity and re-introduce the
			// float-then-drop height divergence. The jump/land clips still play COSMETICALLY (RootMotionFromEvery-
			// thing extracts the root in place); we simply don't drive the capsule from them.
			const bool bPhysicsDrivenTransition =
				ChooserContext.SMState == EAZ_StateMachineState::TransitionToInAir || ChooserContext.bJustLanded;
			if (!bPhysicsDrivenTransition)
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
