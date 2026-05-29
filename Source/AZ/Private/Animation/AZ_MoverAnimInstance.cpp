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
		bPendingTransitionRMMove = false;
		TSharedPtr<FLayeredMove_RootMotionAttribute> RMMove = MakeShared<FLayeredMove_RootMotionAttribute>();
		RMMove->DurationMs = PendingTransitionRMMoveDurationMs;
		CachedMover->QueueLayeredMove(RMMove);
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
	// Slide / Swim left as default until those modes exist.

	// Stance — CharacterMoverComponent owns crouch state.
	ChooserContext.Stance = (CachedCMC && CachedCMC->IsCrouching())
		? EAZ_Stance::Crouching
		: EAZ_Stance::Standing;

	// Gait — derived from speed for now. When a sprint input + gait threshold layer lands
	// (Step 5 follow-up), replace this with a read from the pawn's cached input intent.
	const FVector CurrentForward = CachedPawn->GetActorForwardVector();
	if (ChooserContext.Speed2D > 500.f)        ChooserContext.Gait = EAZ_Gait::Sprint;
	else if (ChooserContext.Speed2D > 200.f)   ChooserContext.Gait = EAZ_Gait::Run;
	else                                       ChooserContext.Gait = EAZ_Gait::Walk;

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

	// A′ RM-move teardown: whenever we leave a transition phase — whether it completed normally or was
	// abandoned early (e.g. input re-pressed mid-stop) — cancel the per-transition RM move so it can't keep
	// OverrideAll-driving the resumed loop until its DurationMs auto-expires. Our move is tagged
	// Mover.AnimRootMotion.MeshAttribute (child of Mover_AnimRootMotion); a non-exact cancel matches it.
	// No-op when no such move is active. (CancelFeaturesWithTag only touches layered moves/modifiers.)
	auto IsTransitionPhase = [](EAZ_StateMachineState S)
	{
		return S == EAZ_StateMachineState::TransitionToIdle || S == EAZ_StateMachineState::TransitionToLocomotion;
	};
	if (IsTransitionPhase(PreviousSMState) && !IsTransitionPhase(ChooserContext.SMState))
	{
		CachedMover->CancelFeaturesWithTag(Mover_AnimRootMotion, /*bRequireExactMatch*/ false);
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
		ChooserContext.SMState == EAZ_StateMachineState::TransitionToLocomotion;
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

		FPoseSearchBlueprintResult MMResult;
		UPoseSearchLibrary::MotionMatch(
			this, AssetsToSearch, FName("PoseHistory"),
			FPoseSearchContinuingProperties(), FPoseSearchFutureProperties(),
			MMResult);

		UAnimationAsset* MMAnim = Cast<UAnimationAsset>(MMResult.SelectedAnim);
		if (!MMAnim)
		{
			return;
		}

		// MMCostLimit: when > 0, MM result must beat that cost or we keep current anim.
		if (ChooserOut.MMCostLimit > 0.0 && MMResult.SearchCost > ChooserOut.MMCostLimit)
		{
			return;
		}

		UPoseSearchLibrary::IsAnimationAssetLooping(MMAnim, bAssetLooping);

		BlendStackInputs.Anim         = MMAnim;
		BlendStackInputs.bLoop        = bAssetLooping;
		BlendStackInputs.StartTime    = MMResult.SelectedTime;
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
		// DIAGNOSTIC: what foot did the chooser see, and what clip did it pick? (always-_LU debug)
		UE_LOG(LogTemp, Warning, TEXT("[v2 STOP] entry: ctx.bLeftFootDown=%d Dir=%d Gait=%d -> chosen=%s"),
			ChooserContext.bLeftFootDown ? 1 : 0, static_cast<int32>(ChooserContext.MovementDirection),
			static_cast<int32>(ChooserContext.Gait), *GetNameSafe(BlendStackInputs.Anim));
#endif
		if (const UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(BlendStackInputs.Anim))
		{
			const UWorld* World = GetWorld();
			const float Now = World ? World->GetTimeSeconds() : 0.f;
			const float Remaining = FMath::Max(0.05f,
				Seq->GetPlayLength() - static_cast<float>(BlendStackInputs.StartTime));
			TransitionEndTime = Now + Remaining - TransitionAlmostCompleteThreshold;
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

EAZ_StateMachineState UAZ_MoverAnimInstance::DeriveSMState(
	const FAZ_v2_ChooserContext& Current, EAZ_StateMachineState Previous)
{
	// Locomotion phase, NOT posture/intent. Tags handle the orthogonal "aiming/reloading/etc"
	// dimension via OwnedTags; SMState is purely "where am I in the move cycle?".
	//
	// Transition states (TransitionToIdle, TransitionToLocomotion) deferred until we have
	// the per-anim chooser rows + motion-matching DBs that drive them.

	// Non-idle phases — clear any pending idle-break / transition scheduling.
	if (Current.MovementMode == EAZ_MovementMode::InAir)
	{
		NextIdleBreakTime = -1.f;
		IdleBreakEndTime  = -1.f;
		TransitionEndTime = -1.f;
		return EAZ_StateMachineState::InAirLoop;
	}
	if (Current.bIsMoving)
	{
		// Moving (intent). Re-pressing input mid-stop lands here → resume the loop immediately,
		// abandoning the stop transition. (Start transition deferred to a later iteration.)
		NextIdleBreakTime = -1.f;
		IdleBreakEndTime  = -1.f;
		TransitionEndTime = -1.f;
		return EAZ_StateMachineState::LocomotionLoop;
	}

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	// --- Stop transition: moving → idle routes through TransitionToIdle (plays a foot-aware RM stop
	// clip) before settling to IdleLoop. TransitionEndTime is populated by SetBlendStackAnimFromChooser
	// from the chosen clip's length (same mechanism as IdleBreakEndTime). ---
	if (Previous == EAZ_StateMachineState::LocomotionLoop)
	{
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
