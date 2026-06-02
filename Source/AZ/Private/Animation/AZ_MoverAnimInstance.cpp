// Copyright Artur. AZ project.

#include "Animation/AZ_MoverAnimInstance.h"

#include "Animation/AnimSequenceBase.h"
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
		// The RM-action mode (jump/vault/etc.) is airborne — report InAir so trajectory/other systems agree.
		// The jump SM phase is DECOUPLED from this (DeriveSMState holds TransitionToInAir by clip length, not
		// by MovementMode), so this is purely informational for non-jump consumers.
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

	// ---- Phase derivation (the "C++ SM") — see project_v2_architecture.md ----
	ChooserContext.SMState = DeriveSMState(ChooserContext, PreviousSMState);

	// Turn-start bucket: hold the latched bucket for the whole start transition, Fwd everywhere else.
	// DeriveSMState set LatchedStartDirection at the TransitionToLocomotion entry edge (and only there).
	const bool bInStartTransition = (ChooserContext.SMState == EAZ_StateMachineState::TransitionToLocomotion);
	ChooserContext.StartDirection = bInStartTransition ? LatchedStartDirection : EAZ_StartDirection::Fwd;
	// Moving-pivot vs from-rest start: same latch lifetime. Lets the chooser pick RTG_RM_RunFwdTurn180_*
	// (momentum-preserving) for a moving reversal vs the from-rest RunFwdStart* for an idle start.
	ChooserContext.bMovingTransition = bInStartTransition ? bLatchedMovingTransition : false;
	// Land flag: true only while playing the touchdown land clip — a TransitionToIdle entered from the air.
	// Distinguishes a jump-land from a locomotion stop under the shared TransitionToIdle phase.
	ChooserContext.bJustLanded = (ChooserContext.SMState == EAZ_StateMachineState::TransitionToIdle)
		? bLatchedJustLanded : false;

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

	// ---- RM-action mode handoff (jump) ----
	// The RM jump clip (RTG_RM_Jump_place_ALL) carries vertical root motion. In Walking mode the per-tick
	// floor-snap (UWalkingMode::TryMoveToAdjustHeightAboveFloor) eats that lift, so for the air phase we hand
	// the Mover to UAZ_PawnMovementMode_RMAction — gravity-free, floor-snap-free — which follows the clip's
	// full XYZ root motion (incl. Z). Switch in on the TransitionToInAir ENTER edge; hand back to Walking on
	// the EXIT edge — by then the clip's own root motion has set the capsule back on the ground, and Walking
	// re-finds the floor on Activate. QueueNextMode is the same game-thread Mover API the RM-move queue above
	// uses. The phase length is bounded by the clip (DeriveSMState), so there is no way to get stuck airborne.
	// Only the machine that SIMULATES this pawn drives the mode (authority + locally-predicted autonomous
	// proxy). A simulated proxy doesn't run the sim — it RECEIVES the mode via replication and mirrors it in
	// DeriveSMState — so it must NOT queue mode changes (meaningless locally, and could perturb interpolation).
	if (CachedPawn->GetLocalRole() != ROLE_SimulatedProxy)
	{
		const bool bWasInAir = (PreviousSMState      == EAZ_StateMachineState::TransitionToInAir);
		const bool bNowInAir = (ChooserContext.SMState == EAZ_StateMachineState::TransitionToInAir);
		if (bNowInAir && !bWasInAir)
		{
			CachedMover->QueueNextMode(TEXT("RMAction"));
		}
		else if (bWasInAir && !bNowInAir)
		{
			CachedMover->QueueNextMode(TEXT("Walking"));
		}
	}
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
			IdleBreakEndTime = Now + Seq->GetPlayLength() - IdleBreakAlmostCompleteThreshold;
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
			TransitionEndTime = Now + Remaining - TransitionAlmostCompleteThreshold;
			// RM bridge for ALL transitions — including the jump. TransitionToInAir now plays the RM jump clip
			// RTG_RM_Jump_place_ALL, whose root motion (incl. the vertical lift) drives the whole arc via the
			// same per-transition FLayeredMove_RootMotionAttribute (OverrideAll) that drives starts/stops. The
			// earlier "jumps are physics-only, skip RM" gate is removed now that the jump is RM-driven.
			PendingTransitionRMMoveDurationMs = Remaining * 1000.f;
			bPendingTransitionRMMove = true;
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

// Bucket a signed facing->desired yaw (deg, +right) into a turn-start clip selector.
// Midpoints between the named magnitudes: 45 (0|90), 112.5 (90|135), 157.5 (135|180).
// Side from the sign; at exactly +-180 the sign is a deterministic tiebreak (refine in Phase 4 if
// it ever looks wrong-footed). VERIFY the +right convention in PIE — swap bRight if turns mirror.
static EAZ_StartDirection BucketStartDirection(float SignedAngleDeg)
{
	const float A = FMath::Abs(SignedAngleDeg);
	const bool  bRight = SignedAngleDeg >= 0.f;
	if (A <= 45.0f)   { return EAZ_StartDirection::Fwd; }
	if (A <= 112.5f)  { return bRight ? EAZ_StartDirection::R90  : EAZ_StartDirection::L90;  }
	if (A <= 157.5f)  { return bRight ? EAZ_StartDirection::R135 : EAZ_StartDirection::L135; }
	return              bRight ? EAZ_StartDirection::R180 : EAZ_StartDirection::L180;
}

EAZ_StateMachineState UAZ_MoverAnimInstance::DeriveSMState(
	const FAZ_v2_ChooserContext& Current, EAZ_StateMachineState Previous)
{
	// Locomotion phase, NOT posture/intent. Tags handle the orthogonal "aiming/reloading/etc"
	// dimension via OwnedTags; SMState is purely "where am I in the move cycle?".
	//
	// Transition states (TransitionToIdle, TransitionToLocomotion) deferred until we have
	// the per-anim chooser rows + motion-matching DBs that drive them.

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	// ---- Simulated-proxy air mirror ----
	// A simulated proxy (another player on THIS machine) doesn't run the Mover sim, and routinely MISSES the
	// one-shot jump-press edge: bIsJumpJustPressed is true for a SINGLE sim frame and falls between the
	// interpolated snapshots a sim proxy reconstructs motion from (the "sometimes it jumps" symptom). The
	// MovementMode is persistent replicated STATE (part of FMoverSyncState), not a transient event, so we
	// mirror it: the proxy is in the air phase for EXACTLY as long as the replicated mode is "RMAction".
	// One source of truth (no edge, no clip timer) ⇒ can't desync from the physics and can't re-enter —
	// enter/stay while mode==RMAction, exit the frame it drops. Authority + autonomous proxy (the machines
	// that SIMULATE the pawn) fall through to the edge-driven path below — responsive + predicted, and the
	// server replays the client's input so it still sees the edge for remote pawns.
	if (CachedPawn && CachedPawn->GetLocalRole() == ROLE_SimulatedProxy)
	{
		const bool bProxyInRMAction = CachedMover && CachedMover->GetMovementModeName() == TEXT("RMAction");
		if (bProxyInRMAction)
		{
			NextIdleBreakTime = -1.f;
			IdleBreakEndTime  = -1.f;
			bLatchedJustLanded = false;
			return EAZ_StateMachineState::TransitionToInAir;   // enter or stay
		}
		if (Previous == EAZ_StateMachineState::TransitionToInAir)
		{
			// Mode just dropped out of RMAction → the jump's air phase ended on the authority. Settle by the
			// synced movement intent (bIsMoving rides the synced input, so it is valid on a proxy).
			TransitionEndTime = -1.f;
			return Current.bIsMoving ? EAZ_StateMachineState::LocomotionLoop : EAZ_StateMachineState::IdleLoop;
		}
		// Not in the RM jump → fall through to the normal grounded derivation below (walk/run/idle/stop
		// already replicate correctly on proxies — they key off sustained synced input, not a one-shot edge).
	}

	// --- RM jump: the whole jump is ONE root-motion clip (RTG_RM_Jump_place_ALL) whose root motion both
	// lifts the capsule AND sets it back down. So once a jump has started, HOLD TransitionToInAir for the
	// clip's full length, DECOUPLED from MovementMode: the clip's own RM flips MovementMode InAir->OnGround
	// partway through (when it plants), and letting that exit early would cut the clip short / fire the
	// locomotion land edge. The per-transition RM bridge (un-gated in SetBlendStackAnimFromChooser) then
	// OverrideAll-drives the arc — incl. the vertical lift — from the clip's root-motion attribute.
	// (This block must precede the MovementMode==InAir check so the held jump survives the mid-clip plant.) ---
	if (Previous == EAZ_StateMachineState::TransitionToInAir)
	{
		NextIdleBreakTime = -1.f;
		IdleBreakEndTime  = -1.f;
		bLatchedJustLanded = false;
		// Hold the jump clip for its full length, then settle. Re-triggering the jump before landing is
		// blocked at the GA level (BP_GA_Jump can't re-activate until grounded), so we don't gate on the
		// ground here — keeps this purely "play the clip once, then return to loco/idle".
		if (TransitionEndTime > 0.f && Now >= TransitionEndTime)
		{
			TransitionEndTime = -1.f;
			return Current.bIsMoving ? EAZ_StateMachineState::LocomotionLoop : EAZ_StateMachineState::IdleLoop;
		}
		return EAZ_StateMachineState::TransitionToInAir;
	}

	// Jump START: the physics jump is suppressed (SetHandleJump(false)), so the Mover no longer puts us
	// airborne on press — we trigger the RM jump off the jump-press input EDGE instead. RTG_RM_Jump_place_ALL's
	// own root motion does the takeoff FROM THE GROUND (anticipation + push-off) and lifts the capsule via its
	// vertical Z. The GA still ships bIsJumpJustPressed via SetJumpPressed (the Mover just no longer consumes
	// it). Read it from the last input cmd — same source as the gait/move reads.
	// (Fall-off-ledge has no handler here yet — a later rung; for now only a jump-press starts the air clip.)
	// Edge-driven on the SIMULATING machine only (authority / autonomous). Sim proxies are handled by the
	// mode-mirror above and must NOT also read the edge here — a stray synced edge arriving a frame before
	// the replicated mode would enter air, then the mode-mirror (mode not yet RMAction) would immediately
	// exit, cutting the jump short. (CachedMover && non-sim-proxy guarantees the proxy never sees the edge.)
	bool bJumpJustPressed = false;
	if (CachedMover && CachedPawn && CachedPawn->GetLocalRole() != ROLE_SimulatedProxy)
	{
		const FMoverInputCmdContext& JumpIn = CachedMover->GetLastInputCmd();
		if (const FCharacterDefaultInputs* CharIn = JumpIn.InputCollection.FindDataByType<FCharacterDefaultInputs>())
		{
			bJumpJustPressed = CharIn->bIsJumpJustPressed;
		}
	}
	if (bJumpJustPressed)
	{
		NextIdleBreakTime = -1.f;
		IdleBreakEndTime  = -1.f;
		bLatchedJustLanded = false;
		TransitionEndTime = Now + 1.0f;   // overridden by the RTG_RM_Jump_place_ALL clip length
		return EAZ_StateMachineState::TransitionToInAir;
	}

	if (Current.bIsMoving)
	{
		NextIdleBreakTime = -1.f;
		IdleBreakEndTime  = -1.f;
		bLatchedJustLanded = false;

		// Hard-reversal / sharp-turn detection (while MOVING): if the desired heading is far enough from
		// current facing, we PLANT-STOP then re-accelerate through a turn-start, instead of smearing the
		// loop around to a backward/side clip (the unnatural "run fwd, flick S" reversal). Gentle steering
		// (below threshold) stays in the loop — facing-smoothing handles it. A normal stop only exists while
		// !bIsMoving; a stop that runs WHILE moving (input held) is by definition a committed reversal, so we
		// distinguish the two purely by this angle (no extra state flag). Threshold tunable by feel; CHALK
		// wants a weighty plant on reversals. (Same PendingStartAngleDeg recomputed each moving frame.)
		constexpr float ReversalAngleThreshold = 135.f;
		const bool bHardTurn = FMath::Abs(PendingStartAngleDeg) >= ReversalAngleThreshold;

		// --- Start transition: idle → moving routes through TransitionToLocomotion (plays an RM start
		// clip that accelerates from rest) before settling to LocomotionLoop. Symmetric with the stop
		// path below; TransitionEndTime is populated by SetBlendStackAnimFromChooser from the chosen
		// clip's length (same mechanism as the stop / IdleBreak). ---
		if (Previous == EAZ_StateMachineState::IdleLoop || Previous == EAZ_StateMachineState::IdleBreak)
		{
			// Latch the turn-start bucket ONCE, here at the entry edge, from this frame's desired-vs-facing
			// angle. Held for the whole transition (the RM turn clip collapses the live angle as it plays;
			// re-bucketing would flip the selected clip mid-turn). NativeUpdateAnimation feeds it to
			// ChooserContext.StartDirection while SMState == TransitionToLocomotion.
			LatchedStartDirection = BucketStartDirection(PendingStartAngleDeg);
			bLatchedMovingTransition = false;   // from rest → from-rest turn-start clips

			// Arm a default fall-through, overridden by the real clip length in SetBlendStackAnimFromChooser.
			// Safety net: if no TransitionToLocomotion chooser row matches (so no clip is pushed), this still
			// lets us settle into the loop instead of freezing at rest.
			TransitionEndTime = Now + 1.0f;
			return EAZ_StateMachineState::TransitionToLocomotion;
		}
		if (Previous == EAZ_StateMachineState::TransitionToLocomotion)
		{
			if (TransitionEndTime > 0.f && Now >= TransitionEndTime)
			{
				TransitionEndTime = -1.f;
				return EAZ_StateMachineState::LocomotionLoop;
			}
			return EAZ_StateMachineState::TransitionToLocomotion;
		}
		// --- Reversal / sharp turn while moving: go STRAIGHT to the turn-start (pivot), skipping the plant-
		// stop, so there's no halt-then-go delay. The turn-start's root motion overrides the current velocity
		// (A′ layered move = OverrideAll for the transition), so the body re-aims and re-accelerates the new
		// way directly out of the loop — you still get the clip's own decel-through-the-pivot, but not a
		// separate full stop first. Gentle turns (below threshold) stay in the loop (facing-smoothing steers).
		// NOTE: the turn-start clips are authored from rest; entering them at speed can look slightly light on
		// the plant — if so, the dedicated moving pivots (RTG_RM_RunFwdTurn180_*) are the quality upgrade. ---
		if (Previous == EAZ_StateMachineState::LocomotionLoop)
		{
			if (bHardTurn)
			{
				LatchedStartDirection = BucketStartDirection(PendingStartAngleDeg);
				bLatchedMovingTransition = true;   // moving pivot → dedicated momentum-preserving pivot clip
				TransitionEndTime = Now + 1.0f;   // overridden by the real turn-start clip length
				return EAZ_StateMachineState::TransitionToLocomotion;
			}
			TransitionEndTime = -1.f;
			return EAZ_StateMachineState::LocomotionLoop;
		}
		// TransitionToIdle while still moving = a normal stop whose input was re-pressed → resume the loop
		// (residual momentum, no from-rest start needed). If it's actually a hard turn, the loop re-evaluates
		// bHardTurn next frame and pivots then.
		if (Previous == EAZ_StateMachineState::TransitionToIdle)
		{
			TransitionEndTime = -1.f;
			return EAZ_StateMachineState::LocomotionLoop;
		}
		// Landing or any other prior phase → straight to the loop (MM finds the entry frame).
		TransitionEndTime = -1.f;
		return EAZ_StateMachineState::LocomotionLoop;
	}

	// --- Stop transition: moving → idle routes through TransitionToIdle (plays a foot-aware RM stop
	// clip) before settling to IdleLoop. TransitionEndTime is populated by SetBlendStackAnimFromChooser
	// from the chosen clip's length (same mechanism as IdleBreakEndTime). ---
	if (Previous == EAZ_StateMachineState::LocomotionLoop)
	{
		bLatchedJustLanded = false;   // this TransitionToIdle is a STOP, not a land
		// Arm a default fall-through, overridden by the real clip length in SetBlendStackAnimFromChooser.
		// Safety net: if no TransitionToIdle chooser row matches (so no clip is pushed), this still lets
		// us settle to idle instead of freezing in the stop phase.
		TransitionEndTime = Now + 1.0f;
		return EAZ_StateMachineState::TransitionToIdle;
	}
	if (Previous == EAZ_StateMachineState::TransitionToIdle)
	{
		if (TransitionEndTime > 0.f && Now >= TransitionEndTime)
		{
			TransitionEndTime = -1.f;
			bLatchedJustLanded = false;   // land (or stop) finished → clear
			NextIdleBreakTime = Now + FMath::FRandRange(IdleBreakMinTime, IdleBreakMaxTime);
			return EAZ_StateMachineState::IdleLoop;
		}
		return EAZ_StateMachineState::TransitionToIdle;
	}

	// Already mid-break — stay until the break anim's almost-complete window. IdleBreakEndTime
	// is populated by SetBlendStackAnimFromChooser when the chosen break anim is pushed.
	if (Previous == EAZ_StateMachineState::IdleBreak)
	{
		if (IdleBreakEndTime > 0.f && Now >= IdleBreakEndTime)
		{
			IdleBreakEndTime = -1.f;
			NextIdleBreakTime = Now + FMath::FRandRange(IdleBreakMinTime, IdleBreakMaxTime);
			return EAZ_StateMachineState::IdleLoop;
		}
		return EAZ_StateMachineState::IdleBreak;
	}

	// IdleLoop or fresh entry into idle. Schedule the next break if none pending, otherwise
	// fire when the scheduled time is reached.
	if (NextIdleBreakTime < 0.f)
	{
		NextIdleBreakTime = Now + FMath::FRandRange(IdleBreakMinTime, IdleBreakMaxTime);
	}
	else if (Now >= NextIdleBreakTime)
	{
		NextIdleBreakTime = -1.f;
		return EAZ_StateMachineState::IdleBreak;
	}
	return EAZ_StateMachineState::IdleLoop;
}
