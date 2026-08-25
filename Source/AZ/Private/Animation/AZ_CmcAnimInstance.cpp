// Copyright Artur. AZ project.

#include "Animation/AZ_CmcAnimInstance.h"

#include "Character/Cmc/AZ_CmcCharacterBase.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"   // EDrawDebugTrace
#include "BlendStack/BlendStackAnimNodeLibrary.h"
#include "PoseSearch/MotionMatchingAnimNodeLibrary.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "BoneControllers/AnimNode_OffsetRootBone.h"
#include "Animation/AnimClassInterface.h"

namespace AZ::CmcAnim
{
	/** Signed yaw (deg, + = right) from a base yaw to a world direction. 0 for a degenerate direction —
	 *  callers decide whether to hold their previous value instead. */
	static float SignedYawTo(const FVector& WorldDir, float BaseYawDeg)
	{
		const FVector Dir = WorldDir.GetSafeNormal2D();
		if (Dir.IsNearlyZero())
		{
			return 0.f;
		}
		const FRotationMatrix Basis(FRotator(0.f, BaseYawDeg, 0.f));
		return FMath::RadiansToDegrees(FMath::Atan2(
			FVector::DotProduct(Dir, Basis.GetUnitAxis(EAxis::Y)),
			FVector::DotProduct(Dir, Basis.GetUnitAxis(EAxis::X))));
	}
}

UAZ_CmcAnimInstance::UAZ_CmcAnimInstance()
{
	// CMC applies montage root motion to the capsule ITSELF; locomotion here is kinematic. Montages-only
	// is the correct contract, and the CONSTRUCTOR is the right place to say so — it becomes the CDO
	// default the ABP inherits while leaving the asset free to override.
	//
	// Deliberately NOT set in NativeInitializeAnimation: doing it there overwrites whatever the asset
	// says on every load, which is exactly the bug that made the Chalkie run at double speed
	// (AZ_MoverAnimInstance.cpp:94 forces RootMotionFromEverything unconditionally). Init only WARNS.
	RootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
}

void UAZ_CmcAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	Cached_Character = Cast<AAZ_CmcCharacterBase>(TryGetPawnOwner());

	if (RootMotionMode != ERootMotionMode::RootMotionFromMontagesOnly)
	{
		// Not corrected on purpose — a silent fix hides the authoring mistake, and this one is expensive.
		UE_LOG(LogTemp, Warning,
			TEXT("[CmcAnim] %s has RootMotionMode=%d, expected RootMotionFromMontagesOnly(%d). ")
			TEXT("Locomotion root motion will be applied twice on CMC. Fix it in the ABP Class Defaults."),
			*GetClass()->GetName(), static_cast<int32>(RootMotionMode),
			static_cast<int32>(ERootMotionMode::RootMotionFromMontagesOnly));
	}

	if (!bLoggedInit)
	{
		bLoggedInit = true;
		const USkeletalMeshComponent* MeshComp = GetSkelMeshComponent();
		UE_LOG(LogTemp, Display,
			TEXT("[CmcAnim] %s init | pawn=%s | mesh=%s | collisions=%s"),
			*GetClass()->GetName(), *GetNameSafe(Cached_Character),
			*GetNameSafe(MeshComp ? MeshComp->GetSkeletalMeshAsset() : nullptr),
			bHandleTrajectoryCollisions ? TEXT("on") : TEXT("off"));
	}
}

namespace
{
	// See the MONTAGE-ENDED EDGE comment at the write site in NativeUpdateAnimation.
	bool GCmcMontageWasActive = false;
	bool GCmcMontageJustEnded = false;
}

void UAZ_CmcAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!Cached_Character)
	{
		// Lazy re-resolve. Returning here HOLDS the previous contract rather than zeroing it — a zeroed
		// contract reads as a sudden stop and would fire a spurious stop transition on the frame the
		// character finally comes alive.
		Cached_Character = Cast<AAZ_CmcCharacterBase>(TryGetPawnOwner());
		if (!Cached_Character)
		{
			return;
		}
	}

	// The ENTIRE pawn -> anim seam, once per frame, on the game thread.
	Cached_Character->FillAnimContract(CharacterProperties);

	// ---- ANIM -> MOVEMENT snapshot: what the selected stop clip depicts RIGHT NOW. ----
	// The movement layer makes the capsule track this instead of a tuned deceleration, so stopping
	// distance becomes a property of the CONTENT rather than a number fitted to it. Snapshotted here,
	// on the game thread, rather than read directly by the character: CurrentDatabaseTags and the curve
	// are touched by the anim worker, and the character's Tick is a different thread context.
	//
	// The character ticks BEFORE the mesh (PostInitializeComponents adds the prerequisite), so it reads
	// last frame's snapshot — one frame, ~16ms of latency on the braking value. Acceptable because the
	// clip's speed curve is smooth; a discontinuous curve would need the read moved into the movement tick.
	{
		static const FName StopsTag(TEXT("Stops"));
		bStopClipSelected_GT = CurrentDatabaseTags.Contains(StopsTag);

		// Sample the SELECTED SEQUENCE'S OWN curve at its own playback time.
		//
		// NOT GetCurveValue(). That returns the BLEND-WEIGHTED value across everything still active in
		// the BlendStack, so for the whole blend-in of a stop it is dominated by the outgoing loop.
		// Measured 2026-08-23: it read 167-172 cm/s at the start of every walk stop — the walk LOOP's
		// 172.6, not the stop clip's 147 peak — so the movement layer concluded the clip wanted MORE
		// speed than the body had and coasted with braking=0 through the first part of every stop.
		//
		// EvaluateCurveData is immune to that: it asks one asset what it authored at one time.
		StopClipSpeed_GT = 0.f;
		StopClipSampleTime_GT = 0.f;
		if (bStopClipSelected_GT)
		{
			if (const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(CurrentSelectedAnim.Get()))
			{
				StopClipSampleTime_GT = CurrentSelectedTime;
				StopClipSpeed_GT = Sequence->EvaluateCurveData(MoveDataSpeedCurve, CurrentSelectedTime);
			}
		}

		// TIP reverse edge, same discipline. Root yaw is snapshotted unconditionally: the hero's release
		// snap wants it even on the frame the selection has already moved on.
		static const FName TurnInPlaceSnapshotTag(TEXT("TurnInPlace"));
		bTipClipSelected_GT = CurrentDatabaseTags.Contains(TurnInPlaceSnapshotTag);
		TipClipFraction_GT = 0.f;
		if (bTipClipSelected_GT)
		{
			if (const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(CurrentSelectedAnim.Get()))
			{
				const float Len = Sequence->GetPlayLength();
				TipClipFraction_GT = (Len > KINDA_SMALL_NUMBER) ? (CurrentSelectedTime / Len) : 0.f;
			}
		}
		TipRootYaw_GT = static_cast<float>(RootTransform.Rotator().Yaw);
	}

	if (bDebugAnim)
	{
		DrawDebugAnimOverlay();
		LogMovementFeelOncePerSecond(DeltaSeconds);
	}
}

/**
 * Prints the CMC values ApplyMovementFeelParams actually wrote this frame.
 *
 * GAME THREAD ONLY (called from NativeUpdateAnimation) — these are read straight off the live component,
 * which the thread-safe update must not touch.
 *
 * Exists because "is this tuning value actually reaching the component?" was guessed at twice on
 * 2026-08-22 and answered wrong once: a GroundFriction change read as having no effect, and the reason
 * turned out to be that friction is only ~1/3 of the direction authority (MaxAcceleration owns the rest)
 * — not that the value failed to apply. Reading the applied numbers is cheaper than reasoning about them.
 *
 * Function-local statics rather than members: a new member changes the UCLASS layout and Live Coding
 * cannot patch that.
 */
void UAZ_CmcAnimInstance::LogMovementFeelOncePerSecond(float DeltaSeconds) const
{
	// GAME WORLDS ONLY. Without this an unpossessed or ABP-preview instance logs its UNTOUCHED CMC CDO
	// (friction 8 / maxAccel 800 / braking 2048 / rotYaw 360) at spd=0 forever, which buried the real
	// samples 81-to-17 the first time this ran. ApplyMovementFeelParams never touches those instances.
	const UWorld* FeelWorld = GetWorld();
	if (!FeelWorld || !FeelWorld->IsGameWorld())
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(Cached_Character);
	const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Move)
	{
		return;
	}

	static float FeelLogAccumulator = 0.f;
	FeelLogAccumulator += DeltaSeconds;
	if (FeelLogAccumulator < 1.f)
	{
		return;
	}
	FeelLogAccumulator = 0.f;

	// Named to avoid hiding the member Speed2D, which the WORKER thread owns — this one is read live
	// off the component on the game thread and the two must not be confused.
	const float LiveSpeed2D = Move->Velocity.Size2D();
	// Time to reach the CURRENT speed at the CURRENT acceleration — the honest scalar for "inertia".
	const float TimeToSpeed = (Move->MaxAcceleration > 0.f) ? (LiveSpeed2D / Move->MaxAcceleration) : 0.f;

	UE_LOG(LogTemp, Display,
		TEXT("[CmcFeel] spd=%.0f | friction=%.2f maxAccel=%.0f braking=%.0f rotYaw=%.0f | ")
		TEXT("timeToSpeed=%.2fs dirTau=%.2fs | orientToMove=%d ctrlDesired=%d"),
		LiveSpeed2D, Move->GroundFriction, Move->MaxAcceleration, Move->BrakingDecelerationWalking,
		Move->RotationRate.Yaw, TimeToSpeed,
		(Move->GroundFriction > 0.f) ? (1.f / Move->GroundFriction) : 0.f,
		Move->bOrientRotationToMovement ? 1 : 0, Move->bUseControllerDesiredRotation ? 1 : 0);
}

void UAZ_CmcAnimInstance::DrawDebugAnimOverlay() const
{
	if (!GEngine)
	{
		return;
	}

	// Stable keys so each line REPLACES itself instead of scrolling. Keys are arbitrary but must not
	// collide with anything else drawing an overlay; the 0x415A ("AZ") prefix keeps us out of the way.
	static constexpr int32 KeyBase = 0x415A00;

	const UEnum* DirEnum   = StaticEnum<EAZ_MovementDirection>();
	const UEnum* GaitEnum  = StaticEnum<EAZ_Gait>();
	const UEnum* StateEnum = StaticEnum<EAZ_MovementState>();
	auto EnumName = [](const UEnum* E, int64 V) -> FString
	{
		return E ? E->GetNameStringByValue(V) : TEXT("?");
	};

	// THE line: what is on screen right now, and out of which pool.
	const FString AnimName = GetNameSafe(CurrentSelectedAnim);
	const FString DbName   = GetNameSafe(CurrentSelectedDatabase);

	// Capsule vs mesh root. Zero until an OffsetRootBone node exists; once it does, this IS the turn
	// lag, and the single most useful number for judging whether the offset is behaving.
	const float RootOffsetYaw = static_cast<float>(
		(CharacterTransform.Rotator() - RootTransform.Rotator()).GetNormalized().Yaw);

	FString GateNames;
	for (const FName& GateLabel : MatchedGateLabels)
	{
		if (!GateNames.IsEmpty()) { GateNames += TEXT(","); }
		GateNames += GateLabel.ToString();
	}

	const FVector2D Lean = Get_LeanAmount();

	GEngine->AddOnScreenDebugMessage(KeyBase + 0, 0.f, FColor::Yellow,
		FString::Printf(TEXT("ANIM  %s"), *AnimName));
	GEngine->AddOnScreenDebugMessage(KeyBase + 1, 0.f, FColor::Orange,
		FString::Printf(TEXT("DB    %s   cost %.1f   loop %d   tags %d"),
			*DbName, SearchCost, bCurrentAssetLooping ? 1 : 0, CurrentDatabaseTags.Num()));
	GEngine->AddOnScreenDebugMessage(KeyBase + 2, 0.f, FColor::Silver,
		FString::Printf(TEXT("gates [%s]"), *GateNames));
	GEngine->AddOnScreenDebugMessage(KeyBase + 3, 0.f, FColor::Green,
		FString::Printf(TEXT("dir   %s  ang %+.0f    gait %s   state %s"),
			*EnumName(DirEnum,  static_cast<int64>(MovementDirection)), MovementDirectionAngle,
			*EnumName(GaitEnum, static_cast<int64>(Gait)),
			*EnumName(StateEnum, static_cast<int64>(MovementState))));
	GEngine->AddOnScreenDebugMessage(KeyBase + 4, 0.f, FColor::Cyan,
		FString::Printf(TEXT("spd   %.0f / %.0f    trj fut %.0f   turnAng %+.0f"),
			Speed2D, CharacterProperties.CurrentMaxSpeed,
			Trj_FutureVelocity.Size2D(), Get_TrajectoryTurnAngle()));
	// Capsule yaw, mesh-root yaw, and the lag between them. If ROOTOFF stays 0 while turning, the
	// OffsetRootBone node is not reaching Update_EssentialValues.
	GEngine->AddOnScreenDebugMessage(KeyBase + 5, 0.f,
		FMath::Abs(RootOffsetYaw) > 60.f ? FColor::Red : FColor::White,
		FString::Printf(TEXT("yaw   capsule %+.0f   root %+.0f   ROOTOFF %+.0f"),
			CharacterTransform.Rotator().Yaw, RootTransform.Rotator().Yaw, RootOffsetYaw));
	GEngine->AddOnScreenDebugMessage(KeyBase + 6, 0.f, FColor::Magenta,
		FString::Printf(TEXT("lean  %+.2f    AOyaw %+.0f    pivot %d  tip %d  starting %d"),
			Lean.X, Get_AO_Yaw(), IsPivoting() ? 1 : 0,
			ShouldTurnInPlace() ? 1 : 0, IsStarting() ? 1 : 0));

	// The lean and the root offset were ON SCREEN ONLY, so every "did it change?" question this session
	// had to be answered by eye instead of by grep. Mirror the two numbers that matter into the log.
	// relAccY is the RAW normalised lateral term the lean is built from: if that stays pinned at +-1.00
	// the new split-budget normalisation is not taking; if it varies but the character looks the same,
	// the lean is simply too small to see and the blendspace range is the thing to change.
	// rootOff is the capsule-vs-mesh-root yaw lag. If it stays ~0 through a turn, OffsetRootBone is NOT
	// absorbing the rotation and the mesh is rigidly welded to the capsule — a different fault entirely.
	if (Speed2D > 40.f)
	{
		UE_LOG(LogTemp, Display,
			TEXT("[CmcLean] lean=%+.2f relAccY=%+.2f spd=%.0f rootOff=%+.0f capsuleYaw=%+.0f"),
			Lean.X, CalculateRelativeAccelerationAmount().Y, Speed2D, RootOffsetYaw,
			CharacterTransform.Rotator().Yaw);
	}
}

void UAZ_CmcAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	if (!Cached_Character)
	{
		return;
	}

	Update_Logic(DeltaSeconds);
}

void UAZ_CmcAnimInstance::SetOffsetRootTransform(const FTransform& InOffsetRootTransform)
{
	// DEAD — see the declaration. Update_EssentialValues pulls from the node directly and ignores this.
	OffsetRootTransform = InOffsetRootTransform;
	bHasOffsetRootTransform = true;
}

FAnimNode_OffsetRootBone* UAZ_CmcAnimInstance::FindOffsetRootBoneNode()
{
	// GetClass() is the GENERATED class at runtime, so a native parent can reach a node its own
	// compilation never saw. This is the whole reason the BP hand-off is unnecessary.
	const IAnimClassInterface* AnimClass = IAnimClassInterface::GetFromClass(GetClass());
	if (!AnimClass)
	{
		return nullptr;
	}

	for (const FStructProperty* NodeProp : AnimClass->GetAnimNodeProperties())
	{
		if (NodeProp && NodeProp->Struct == FAnimNode_OffsetRootBone::StaticStruct())
		{
			return NodeProp->ContainerPtrToValuePtr<FAnimNode_OffsetRootBone>(this);
		}
	}
	return nullptr;
}

// ======================================================================================
// Update_Logic
// ======================================================================================

void UAZ_CmcAnimInstance::Update_Logic(float DeltaSeconds)
{
	Update_Trajectory(DeltaSeconds);
	Update_EssentialValues(DeltaSeconds);
	Update_States();

	// GASP gates the next two on UseExperimentalStateMachine, which is FALSE on its shipped CDO — so on
	// the MM path neither runs. We keep direction because our CHT rows are addressed by it; we do not
	// port Update_TargetRotation at all, because nothing here would ever call it.
	Update_MovementDirection();

	if (bDebugAnim)
	{
		// ------------------------------------------------------------------------------------------
		// SELECTION-CHANGE log.
		//
		// The 1 Hz line below CANNOT see churn. Two adjacent rows in it are a whole second apart, so
		// N alternating rows are indistinguishable from N separate, perfectly healthy reversals - a
		// transition table built from it is a sampling artifact, not evidence. (Learned the hard way:
		// a "pivot flip-flop" diagnosis was built on exactly that mistake.) This fires on the frame the
		// selection actually changes, which is the only signal that separates "the search is
		// flip-flopping" from "the clip simply looks start-like".
		//
		// READING IT: >=3 changes inside ONE reversal, each dt<200ms, is real churn. One change that
		// then holds for the clip's length is not a selection bug at all - it is the content ceiling.
		// cost<0 means the CONTINUING pose won (our -1.0 OverrideContinuingPoseCostBias applied);
		// cost>0 is a fresh selection, and a LARGE positive cost means the pool had nothing better.
		//
		// Function-local statics, NOT members: a new member changes the UCLASS layout and Live Coding
		// cannot patch that, which would cost an editor restart for a diagnostic. One player character
		// per game world, and the IsGameWorld guard keeps ABP-editor preview instances out of the
		// sample, so the sharing is harmless.
		// ------------------------------------------------------------------------------------------
		if (const UWorld* DebugWorld = GetWorld())
		{
			if (DebugWorld->IsGameWorld())
			{
				static TWeakObjectPtr<UObject> LastSelectedAnim;
				static float SecondsSinceSelectionChange = 0.f;
				static int32 SelectionChangeIndex = 0;

				SecondsSinceSelectionChange += DeltaSeconds;

				// ----------------------------------------------------------------------------------
				// MISMATCH RATIO — ActualSpeed / DepictedSpeed, accumulated per selected clip.
				//
				// This is the number that decides WHICH correction layer a clip needs, and it must be
				// measured rather than guessed. Play rate alone can absorb roughly 0.85-1.15; stride
				// warping extends that; beyond about 0.75-1.25 sustained, the honest answer is that the
				// content and the gameplay speed disagree and no procedural layer hides it.
				//
				// Sampled from the SELECTED sequence at its own time (not GetCurveValue, which returns
				// the blend-weighted value and reads the outgoing clip during blend-in).
				// ----------------------------------------------------------------------------------
				static float RatioSum = 0.f;
				static int32 RatioCount = 0;
				static float RatioMin = TNumericLimits<float>::Max();
				static float RatioMax = 0.f;

				// Below this the ratio is dominated by noise: near-zero ground speed against a clip
				// still depicting motion produces an arbitrarily small number that means nothing.
				static constexpr float RatioMinSpeed = 20.f;

				float DepictedSpeed = 0.f;
				if (const UAnimSequenceBase* SelectedSeq = Cast<UAnimSequenceBase>(CurrentSelectedAnim.Get()))
				{
					DepictedSpeed = SelectedSeq->EvaluateCurveData(MoveDataSpeedCurve, CurrentSelectedTime);
				}
				if (DepictedSpeed > UE_KINDA_SMALL_NUMBER && Speed2D > RatioMinSpeed)
				{
					const float Ratio = Speed2D / DepictedSpeed;
					RatioSum += Ratio;
					++RatioCount;
					RatioMin = FMath::Min(RatioMin, Ratio);
					RatioMax = FMath::Max(RatioMax, Ratio);
				}

				if (CurrentSelectedAnim.Get() != LastSelectedAnim.Get())
				{
					// Report the clip that is ENDING, classified by how much correction it needed.
					// GREEN survives on play rate alone; YELLOW needs stride warping too; RED is a
					// content/gameplay disagreement (the crouch loop at 172.5 against a 90 gait speed
					// is the known example) and should be re-authored or the gait speed changed.
					if (RatioCount > 0)
					{
						const float Mean = RatioSum / RatioCount;
						const TCHAR* Band =
							(Mean >= 0.85f && Mean <= 1.15f) ? TEXT("GREEN") :
							(Mean >= 0.75f && Mean <= 1.25f) ? TEXT("YELLOW") : TEXT("RED");
						UE_LOG(LogTemp, Display,
							TEXT("[CmcRatio] %-32s n=%3d mean=%.2f min=%.2f max=%.2f  %s"),
							*GetNameSafe(LastSelectedAnim.Get()), RatioCount, Mean, RatioMin, RatioMax, Band);
					}
					RatioSum = 0.f;
					RatioCount = 0;
					RatioMin = TNumericLimits<float>::Max();
					RatioMax = 0.f;

					FString ChangeGates;
					for (const FName& GateLabel : MatchedGateLabels)
					{
						if (!ChangeGates.IsEmpty()) ChangeGates += TEXT(",");
						ChangeGates += GateLabel.ToString();
					}
					// COMMANDED vs SELECTION gait, and the raw wants-flags behind them. The gate row is
					// addressed by the SELECTION gait, so when a run start plays a WALK start clip for
					// its first third (measured 2026-08-23: WalkFwdStart for 366ms, then RunFwdStart)
					// the question is whether the Movement.Running tag arrived late or the run input did.
					// Only the commanded gait separates them: it is tag-derived, so cmd=Walk while the
					// player is holding run means the ABILITY has not granted the tag yet.
					const UEnum* GaitEnum = StaticEnum<EAZ_Gait>();
					const FString CmdGait = GaitEnum
						? GaitEnum->GetNameStringByValue(static_cast<int64>(CharacterProperties.Gait))
						: FString::FromInt(static_cast<int32>(CharacterProperties.Gait));
					const FString SelGait = GaitEnum
						? GaitEnum->GetNameStringByValue(static_cast<int64>(CharacterProperties.SelectionGait))
						: FString::FromInt(static_cast<int32>(CharacterProperties.SelectionGait));

					UE_LOG(LogTemp, Display,
						TEXT("[CmcSel:%s|mtg=%d] #%d dt=%.0fms %s -> %s | cost=%+.2f spd=%.0f turn=%.0f accel=%.2f ")
						TEXT("moving=%d pivot=%d tip=%d | cmd=%s sel=%s wantSprint=%d wantWalk=%d ")
						TEXT("| db=%s gates=[%s]"),
						*GetNameSafe(TryGetPawnOwner()), GCmcMontageWasActive ? 1 : 0,
						++SelectionChangeIndex,
						SecondsSinceSelectionChange * 1000.f,
						*GetNameSafe(LastSelectedAnim.Get()), *GetNameSafe(CurrentSelectedAnim),
						SearchCost, Speed2D, Get_TrajectoryTurnAngle(), AccelerationAmount,
						IsMoving(), IsPivoting(), ShouldTurnInPlace(),
						*CmdGait, *SelGait,
						CharacterProperties.InputState.bWantsToSprint ? 1 : 0,
						CharacterProperties.InputState.bWantsToWalk ? 1 : 0,
						*GetNameSafe(CurrentSelectedDatabase), *ChangeGates);

					LastSelectedAnim = CurrentSelectedAnim.Get();
					SecondsSinceSelectionChange = 0.f;
				}
			}
		}

		DebugAccumulator += DeltaSeconds;
		if (DebugAccumulator >= 1.f)
		{
			DebugAccumulator = 0.f;
			const UEnum* DirEnum = StaticEnum<EAZ_MovementDirection>();
			FString GateNames;
			for (const FName& GateLabel : MatchedGateLabels)
			{
				if (!GateNames.IsEmpty()) GateNames += TEXT(",");
				GateNames += GateLabel.ToString();
			}
			UE_LOG(LogTemp, Display,
				TEXT("[CmcAnim] spd=%.0f/%.0f moving=%d pivot=%d tip=%d | accel=%.2f | dir=%s ang=%.0f Lfoot=%d ")
				TEXT("| trj past=%.0f cur=%.0f fut=%.0f turn=%.0f | land=%.2fs @ %.0f | samples=%d ")
				TEXT("| gates=[%s] | yaw cap=%.0f root=%.0f ROOTOFF=%.0f ")
				TEXT("| MM db=%s cost=%.1f loop=%d tags=%d anim=%s"),
				Speed2D, CharacterProperties.CurrentMaxSpeed, IsMoving(), IsPivoting(), ShouldTurnInPlace(),
				AccelerationAmount,
				DirEnum ? *DirEnum->GetNameStringByValue(static_cast<int64>(MovementDirection)) : TEXT("?"),
				MovementDirectionAngle, bLeftFootDown,
				Trj_PastVelocity.Size2D(), Trj_CurrentVelocity.Size2D(), Trj_FutureVelocity.Size2D(),
				Get_TrajectoryTurnAngle(),
				TrajectoryCollision.TimeToLand, TrajectoryCollision.LandSpeed, Trajectory.Samples.Num(),
				*GateNames,
				CharacterTransform.Rotator().Yaw, RootTransform.Rotator().Yaw,
				FRotator::NormalizeAxis(RootTransform.Rotator().Yaw - CharacterTransform.Rotator().Yaw),
				*GetNameSafe(CurrentSelectedDatabase), SearchCost, bCurrentAssetLooping,
				CurrentDatabaseTags.Num(), *GetNameSafe(CurrentSelectedAnim));
		}
	}
}

// ======================================================================================
// Update_Trajectory
// ======================================================================================

void UAZ_CmcAnimInstance::Update_Trajectory(float DeltaSeconds)
{
	// GASP selects between two tuning sets on Speed2D > 0.0 — literally any motion at all, not a
	// threshold. The prediction shaping that suits a standstill is not what suits a sprint.
	const FPoseSearchTrajectoryData& TrajectoryData =
		(Speed2D > 0.f) ? TrajectoryGenerationData_Moving : TrajectoryGenerationData_Idle;

	FTransformTrajectory Generated;
	UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectory(
		this, TrajectoryData, DeltaSeconds,
		Trajectory, PreviousDesiredControllerYaw, Generated,
		TrajectoryHistorySamplingInterval, TrajectoryHistoryCount,
		TrajectoryPredictionSamplingInterval, TrajectoryPredictionCount);

	if (bHandleTrajectoryCollisions)
	{
		// Floats the predicted samples over real geometry, so the search is not asked to match a path
		// that runs through the staircase we are about to climb.
		const TArray<AActor*> ActorsToIgnore;
		FTransformTrajectory Collided;
		UPoseSearchTrajectoryLibrary::HandleTransformTrajectoryWorldCollisions(
			this, this, Generated,
			bTrajectoryApplyGravity, FloorCollisionsOffset,
			Collided, TrajectoryCollision,
			TraceTypeQuery1, /*bTraceComplex*/ false, ActorsToIgnore,
			EDrawDebugTrace::None, /*bIgnoreSelf*/ true, MaxObstacleHeight);
		Trajectory = MoveTemp(Collided);
	}
	else
	{
		Trajectory = MoveTemp(Generated);
	}

	// TIP LOCK: bend the PREDICTED facings toward the latched target. With input zeroed the predictor
	// sees zero acceleration and a static camera, so the raw prediction depicts nothing turning — the MM
	// query would keep choosing Idle, and the TIP Steering node (whose target is Get_DesiredFacing, i.e.
	// a sample of THIS trajectory) would actively fight the clip's authored rotation back to the old
	// facing. One edit fixes both consumers, which is why it lives here and not in either of them.
	// Composed as a WORLD-Z DELTA on each sample's own facing rather than an absolute quat, so the mesh
	// component's -90 yaw convention never enters into it.
	if (CharacterProperties.bTurnInPlaceActive)
	{
		const float DeltaYaw = static_cast<float>(FRotator::NormalizeAxis(
			CharacterProperties.TurnInPlaceTargetYaw
			- CharacterProperties.ActorTransform.Rotator().Yaw));
		const float ConvergeTime = FMath::Max(TurnInPlaceFacingConvergeTime, KINDA_SMALL_NUMBER);
		for (FTransformTrajectorySample& Sample : Trajectory.Samples)
		{
			if (Sample.TimeInSeconds > 0.f)
			{
				const float Alpha = FMath::Clamp(Sample.TimeInSeconds / ConvergeTime, 0.f, 1.f);
				const FQuat Delta(FVector::UpVector, FMath::DegreesToRadians(DeltaYaw * Alpha));
				Sample.Facing = Delta * Sample.Facing;
			}
		}
	}

	// THREE samples, not one. Past feeds deceleration reads, current is the honest "now", and future is
	// 0.4-0.5s out — far enough ahead to choose a clip before the motion it describes has begun.
	UPoseSearchTrajectoryLibrary::GetTransformTrajectoryVelocity(
		Trajectory, static_cast<float>(PastVelocityWindow.X), static_cast<float>(PastVelocityWindow.Y),
		Trj_PastVelocity, /*bExtrapolate*/ false);
	UPoseSearchTrajectoryLibrary::GetTransformTrajectoryVelocity(
		Trajectory, static_cast<float>(CurrentVelocityWindow.X), static_cast<float>(CurrentVelocityWindow.Y),
		Trj_CurrentVelocity, false);
	UPoseSearchTrajectoryLibrary::GetTransformTrajectoryVelocity(
		Trajectory, static_cast<float>(FutureVelocityWindow.X), static_cast<float>(FutureVelocityWindow.Y),
		Trj_FutureVelocity, false);
}

// ======================================================================================
// Update_EssentialValues — GASP's 4-way Sequence, in order.
// ======================================================================================

void UAZ_CmcAnimInstance::Update_EssentialValues(float DeltaSeconds)
{
	// --- then_0: character transform ---
	CharacterTransform_LastFrame = CharacterTransform;
	CharacterTransform = CharacterProperties.ActorTransform;

	// --- then_1: root transform ---
	// The OFFSET root, not the capsule — the mesh lags the capsule through a turn and every
	// actor-relative value below has to be measured against what is actually on screen.
	// GASP: RootTransform = MakeTransform(GetOffsetRootTransform(NodeReference: OffsetRoot)) with
	// Yaw + 90, which converts the offset node's space into the mesh's (-90 yaw) convention.
	FAnimNode_OffsetRootBone* OffsetRootNode = bOffsetRootBoneEnabled ? FindOffsetRootBoneNode() : nullptr;
	if (OffsetRootNode)
	{
		FTransform OffsetRoot;
		OffsetRootNode->GetOffsetRootTransform(OffsetRoot);

		const FRotator OffsetRotation = OffsetRoot.Rotator();
		RootTransform = FTransform(
			FRotator(OffsetRotation.Pitch, OffsetRotation.Yaw + 90.f, OffsetRotation.Roll),
			OffsetRoot.GetTranslation(),
			FVector::OneVector);
	}
	else
	{
		// GASP's else-branch, and where we land whenever the AnimGraph carries no OffsetRootBone node.
		// Degraded, not broken: every direction below is then measured against the capsule instead.
		RootTransform = CharacterTransform;
	}

	// --- then_2: acceleration ---
	Acceleration_LastFrame = Acceleration;
	Acceleration = CharacterProperties.InputAcceleration;
	// NORMALISED against what CMC will actually allow this frame, not a raw magnitude — that is what
	// makes it comparable across gaits while the feel pass retunes MaxAcceleration every tick.
	AccelerationAmount = (CharacterProperties.CurrentMaxAcceleration > UE_KINDA_SMALL_NUMBER)
		? Acceleration.Size() / CharacterProperties.CurrentMaxAcceleration
		: 0.f;
	bHasAcceleration = AccelerationAmount > 0.f;

	// --- then_3: velocity ---
	Velocity_LastFrame = Velocity;
	Velocity = CharacterProperties.Velocity;
	Speed2D = Velocity.Size2D();
	bHasVelocity = Speed2D > HasVelocityThreshold;

	// SMOOTHED, not raw. A single-frame derivative of velocity is noise: variable frame time, collision
	// resolution and CalcVelocity's friction bend all perturb it, and on keyboard the input goes 0 -> full
	// in one frame so the derivative spikes. That value feeds CalculateRelativeAccelerationAmount ->
	// Get_LeanAmount -> the lean blendspace, so an unfiltered read makes the character SNAP into a bank
	// instead of rolling into it. The harshness is created here, after the capsule — no amount of movement
	// tuning reaches it.
	//
	// Smoothed at the source so RelativeAcceleration below inherits it: both are the same signal in
	// different spaces, and both feed additive layers that want a rolled response rather than a spike.
	//
	// ~8 gives a visible roll in and out without lag. Lower = smoother and mushier, higher = sharper.
	// TODO: promote to an EditDefaultsOnly UPROPERTY at the next editor-closed build so it is tunable
	// without a recompile. Kept a constant for now because a new UPROPERTY cannot be Live Coding patched.

	const FVector RawVelocityAcceleration =
		(Velocity - Velocity_LastFrame) / FMath::Max(DeltaSeconds, 0.001f);
	VelocityAcceleration =
		FMath::VInterpTo(VelocityAcceleration, RawVelocityAcceleration, DeltaSeconds, LeanInterpSpeed);
	RelativeAcceleration = RootTransform.GetRotation().UnrotateVector(VelocityAcceleration);

	if (bHasVelocity)
	{
		LastNonZeroVelocity = Velocity;
	}
}

// ======================================================================================
// Update_States — one-frame history for every discrete state. That history is what lets
// anything downstream detect a TRANSITION rather than just a value.
// ======================================================================================

void UAZ_CmcAnimInstance::Update_States()
{
	MovementMode_LastFrame = MovementMode;
	MovementMode = CharacterProperties.MovementMode;

	RotationMode_LastFrame = RotationMode;
	RotationMode = CharacterProperties.RotationMode;

	MovementState_LastFrame = MovementState;
	MovementState = IsMoving() ? EAZ_MovementState::Moving : EAZ_MovementState::Idle;

	Gait_LastFrame = Gait;

	// The CONTRACT carries two gaits and we deliberately take the selection one. Gait (commanded,
	// tag-derived) drops to Walk the frame the sprint input is released, which narrowed the gate rows to
	// WalkMove while the body was still at 565 cm/s and offered a 162 cm/s stop clip to a sprinting
	// character. SelectionGait is momentum-aware AND latched for the duration of a stop, so a single stop
	// keeps one pool instead of stepping Sprint->Run->Walk and playing three different stop clips.
	// Derived on the character (AAZ_CmcCharacterBase::UpdateSelectionGait) rather than here, so the
	// gait->speed table has exactly one owner.
	Gait = CharacterProperties.SelectionGait;

	Stance_LastFrame = Stance;
	Stance = CharacterProperties.Stance;

	// Mirrored with a _LastFrame twin like the states above, and for the same reason: the graph runs
	// AFTER this block, so Get_MMInterruptMode can see the edge this frame rather than a frame late.
	bStopActive_LastFrame = bStopActive;
	bStopActive = CharacterProperties.bStopActive;

	bTurnInPlaceActive_LastFrame = bTurnInPlaceActive;
	bTurnInPlaceActive = CharacterProperties.bTurnInPlaceActive;

	// MONTAGE-ENDED EDGE — the third invalidation edge, same doctrine as the stop and TIP edges above.
	// While an RM start/stop montage owns the slot, whatever MM elects underneath is invisible and often
	// garbage (measured 2026-08-25: WalkFwdLoop at cost 5.2-6.99 with the body at 8-10 cm/s). If that
	// selection survives as the continuing pose, the montage's blend-out lands INTO an arbitrarily
	// phased loop — which is the hand-back jerk. Invalidating on the ended edge makes the next search
	// honest: the pose history already contains the montage's actual end pose, so MM picks the loop at
	// the matching phase instead of resuming a stale one.
	// File-scope statics, not members (Live Coding cannot add members): single UAZ_CmcAnimInstance in
	// the level in practice — the other logging instance is the Mover ABP, a different class. TODO
	// promote to a member pair at the next editor-closed build.
	{
		const bool bMontageNow = (GetCurrentActiveMontage() != nullptr);
		GCmcMontageJustEnded = (GCmcMontageWasActive && !bMontageNow);
		GCmcMontageWasActive = bMontageNow;
	}
}

// ======================================================================================
// Update_MovementDirection — AZ-only (the MM node selects by trajectory; our CHT rows do not).
// ======================================================================================

void UAZ_CmcAnimInstance::Update_MovementDirection()
{
	// ★ FIXED 2026-08-25. This read FootSpeed_L/R — curves that exist on ZERO clips in the AnimPro set.
	// GetCurveValue returns 0 for a missing curve, so the test was (0 < 1) && (0 <= 0) = TRUE on every
	// frame of every clip: bLeftFootDown was a constant. Measured consequence: 19 consecutive RM stops
	// all logged leftFoot=1 and the _RU stop clip could never be selected, so half of all stops landed
	// on the wrong foot.
	//
	// The clips carry contact_l / contact_r, and those are CONTACT FLAGS with the opposite polarity to a
	// speed: high = planted. Hence the comparison flips.
	//
	// Only updated when at least one foot reports contact. Clips without the curves (the stops, and any
	// one-shot) then HOLD the last known foot rather than snapping to a default — which matters
	// precisely here, because the stop foot is decided at the stop edge while the LOOP is still playing,
	// and the loops are the clips that carry the curves.
	const float ContactL = GetCurveValue(FootSpeedCurveL);
	const float ContactR = GetCurveValue(FootSpeedCurveR);
	if (ContactL > FootPlantedSpeedThreshold || ContactR > FootPlantedSpeedThreshold)
	{
		bLeftFootDown = (ContactL >= ContactR);
	}

	// Below a crawl the velocity direction is numerical noise; HOLD the last angle rather than let the
	// chooser row churn every frame while the character settles.
	if (Speed2D > DirectionHoldSpeed)
	{
		MovementDirectionAngle = AZ::CmcAnim::SignedYawTo(Velocity, static_cast<float>(RootTransform.Rotator().Yaw));
	}

	// The leading foot is the one about to swing — the one NOT planted. bInvertFootPhase flips that
	// reading without a rebuild, because it can only be settled by watching it.
	const bool bLeftFootLeads = bInvertFootPhase ? bLeftFootDown : !bLeftFootDown;
	const float Angle = MovementDirectionAngle;

	// DYNAMIC thresholds — see Get_MovementDirectionThresholds. GASP re-picks the quadrant boundaries
	// every frame; a fixed set makes strafe flicker between adjacent directions mid-transition.
	const FAZ_MovementDirectionThresholds Thresholds = Get_MovementDirectionThresholds();

	if (Angle >= -Thresholds.FL && Angle <= Thresholds.FR)
	{
		MovementDirection = EAZ_MovementDirection::F;
	}
	else if (Angle > Thresholds.BR || Angle < -Thresholds.BL)
	{
		MovementDirection = EAZ_MovementDirection::B;
	}
	else if (Angle > 0.f)
	{
		MovementDirection = bLeftFootLeads ? EAZ_MovementDirection::RL : EAZ_MovementDirection::RR;
	}
	else
	{
		MovementDirection = bLeftFootLeads ? EAZ_MovementDirection::LL : EAZ_MovementDirection::LR;
	}
}

// ======================================================================================
// Predicates + getters
// ======================================================================================

bool UAZ_CmcAnimInstance::IsMoving() const
{
	// VELOCITY-BASED, per the GASP 5.8 audit's measured CMC behaviour: "IsMoving on CMC path =
	// |current velocity| > 0.1". The earlier port ANDed in an acceleration term (read off the Mover
	// graph) and that single term made stop animations UNREACHABLE: releasing the stick zeroes
	// acceleration the same frame, the state flips to Idle instantly, the gate rows swap to the idle
	// pool, and the Stops databases leave the search before the deceleration they exist for has even
	// begun (2026-08-21: a whole session with ZERO stop selections, idle playing while sliding at
	// 280 cm/s). Velocity-based, the state stays Moving through the brake — the audit's exact words:
	// "GASP CMC stop-feel assumes velocity-based IsMoving".
	return !Velocity.Equals(FVector::ZeroVector, IsMovingVelocityTolerance);
}

bool UAZ_CmcAnimInstance::IsPivoting() const
{
	// MM path only. GASP's other branch (a denser stance/gait/speed-window test) belongs to the
	// experimental state machine and is unreachable with UseExperimentalStateMachine false.
	float Threshold = PivotAngleThreshold_OrientToMovement;
	switch (RotationMode)
	{
	case EAZ_RotationMode::Strafe:  Threshold = PivotAngleThreshold_Strafe;  break;
	case EAZ_RotationMode::Aiming:  Threshold = PivotAngleThreshold_Aiming;  break;
	default: break;
	}

	return (FMath::Abs(Get_TrajectoryTurnAngle()) >= Threshold) && IsMoving();
}

bool UAZ_CmcAnimInstance::ShouldTurnInPlace() const
{
	// The character's TIP lock outranks the angle test: the angle was already checked at the latch edge
	// (>= TurnInPlaceEnterAngle) and the capsule does not rotate during the lock, so re-deriving it here
	// against a moving mesh root would CLOSE the gate mid-turn — the exact self-cancelling-signal
	// failure the 2026-08-21 pivot gate died of. One owner: the latch decides, this predicate reports.
	if (CharacterProperties.bTurnInPlaceActive)
	{
		return true;
	}

	const float YawDelta = static_cast<float>(FMath::Abs(
		(CharacterProperties.OrientationIntent - RootTransform.Rotator()).GetNormalized().Yaw));

	// Aiming holds the turn continuously; otherwise it fires on the single frame we stopped — which is
	// the only frame where a turn-in-place can start without fighting locomotion.
	const bool bJustStopped = (MovementState == EAZ_MovementState::Idle)
		&& (MovementState_LastFrame == EAZ_MovementState::Moving);

	return (YawDelta >= TurnInPlaceAngleThreshold)
		&& (CharacterProperties.InputState.bWantsToAim || bJustStopped);
}

float UAZ_CmcAnimInstance::Get_TrajectoryTurnAngle() const
{
	// Yaw between where we are ASKING to go and where we are actually going. Both from directions, so a
	// standstill returns 0 rather than a meaningless angle.
	return static_cast<float>((Acceleration.Rotation() - Velocity.Rotation()).GetNormalized().Yaw);
}

FVector UAZ_CmcAnimInstance::CalculateRelativeAccelerationAmount() const
{
	const float MaxAcceleration = CharacterProperties.CurrentMaxAcceleration;
	const float MaxDeceleration = CharacterProperties.CurrentMaxDeceleration;
	if (MaxAcceleration <= 0.f || MaxDeceleration <= 0.f)
	{
		return FVector::ZeroVector;
	}

	// ★★ REWRITTEN 2026-08-24. The old form clamped the WHOLE velocity derivative against the linear
	// acceleration budget and normalised by it. That is correct for speeding up and slowing down and
	// badly wrong for turning, because in a steady carve the derivative is CENTRIPETAL: its magnitude is
	// v*omega, which has nothing to do with how hard the character can push off the ground.
	//
	// Measured 2026-08-24 at RunSpeed 375.7 against the 400 budget:
	//     omega 400 deg/s -> 2618 cm/s^2 -> 6.5x budget
	//     omega 165 deg/s -> 1080 cm/s^2 -> 2.7x budget
	//     omega 115 deg/s ->  753 cm/s^2 -> 1.9x budget
	// Over budget at EVERY rotation rate the project has ever shipped, so the clamp pinned the lateral
	// term at +-1 and the lean became a binary left/right at full deflection. It could not express how
	// hard the turn was, which is why four separately verified parameter changes (rotation rate 180 ->
	// 400 -> 165, friction 3.5 -> 8) all looked identical: the lean is the ONLY node in this AnimGraph
	// that responds to turning at all, and it was saturated flat through every one of them.
	//
	// The fix is to give each component the budget that actually bounds it:
	//   LONGITUDINAL (along velocity) -> MaxAcceleration / MaxDeceleration, exactly as before.
	//   LATERAL      (across velocity) -> v * omega_ref, the centripetal acceleration of a reference
	//                                     turn at the CURRENT speed. Speed cancels, so the term becomes
	//                                     a clean "how much of a full-rate turn is this", 0..1.
	const FVector VelDir = Velocity.GetSafeNormal2D();
	if (VelDir.IsNearlyZero())
	{
		// No heading to decompose against; fall back to the old whole-vector form.
		const float Budget = FMath::Max(MaxAcceleration, 1.f);
		return CharacterTransform.GetRotation().UnrotateVector(
			VelocityAcceleration.GetClampedToMaxSize(Budget) / Budget);
	}

	// Now an EditDefaultsOnly property (AZ|Cmc|Anim|Lean). Still a REFERENCE rate rather than the
	// capsule's live RotationRate.Yaw — feeding the real rate through the contract would make the lean
	// max out exactly when the turn does, and remains the better answer.

	const float   LongMag  = static_cast<float>(FVector::DotProduct(VelocityAcceleration, VelDir));
	const FVector LatVec   = FVector(VelocityAcceleration.X, VelocityAcceleration.Y, 0.f) - VelDir * LongMag;

	// Sign convention preserved: positive longitudinal = speeding up, so it takes the accel budget.
	const float LongBudget = FMath::Max(LongMag >= 0.f ? MaxAcceleration : MaxDeceleration, 1.f);
	const float TurnBudget = FMath::Max(
		Speed2D * FMath::DegreesToRadians(LeanTurnRateReference), 1.f);

	const FVector LongPart = VelDir * FMath::Clamp(LongMag / LongBudget, -1.f, 1.f);
	const FVector LatPart  = LatVec.GetSafeNormal2D()
		* FMath::Clamp(static_cast<float>(LatVec.Size2D()) / TurnBudget, 0.f, 1.f);

	// Clamp the COMBINED vector, not just each component. Measured 2026-08-24: with the two parts
	// clamped separately the sum reaches sqrt(2) = 1.414 when both saturate, and the log showed exactly
	// that (relAccY -1.35, -1.41, -1.41 through the hardest third of a turn). Everything past 1.0 pins
	// at the blendspace axis, so the strongest part of the turn was STILL flat — the same saturation
	// bug this function was rewritten to remove, just one step further out.
	const FVector Combined = (LongPart + LatPart).GetClampedToMaxSize(1.f);

	// CharacterTransform here, NOT RootTransform — GASP measures this one against the capsule.
	return CharacterTransform.GetRotation().UnrotateVector(Combined);
}

FVector2D UAZ_CmcAnimInstance::Get_LeanAmount() const
{
	// Written out rather than via FMath::GetMappedRangeValueClamped: that overload set is FVector2D/
	// double under LWC and picking between its candidates here costs more clarity than the two lines buy.
	const float RangeSpan = FMath::Max(static_cast<float>(LeanSpeedRangeIn.Y - LeanSpeedRangeIn.X), UE_KINDA_SMALL_NUMBER);
	const float Alpha = FMath::Clamp((Speed2D - static_cast<float>(LeanSpeedRangeIn.X)) / RangeSpan, 0.f, 1.f);
	const float SpeedScale = FMath::Lerp(static_cast<float>(LeanSpeedRangeOut.X),
	                                     static_cast<float>(LeanSpeedRangeOut.Y), Alpha);

	// X only. GASP returns 0 on Y — forward/back lean is carried by the clips themselves.
	return FVector2D(CalculateRelativeAccelerationAmount().Y * SpeedScale, 0.f);
}

FVector2D UAZ_CmcAnimInstance::Get_AOValue() const
{
	const FRotator Delta =
		(CharacterProperties.AimingRotation - RootTransform.Rotator()).GetNormalized();

	// Faded to zero by the clip's own Disable_AO curve, so an animation can switch the aim offset off
	// for its duration without anything else needing to know.
	const float DisableAlpha = FMath::Clamp(GetCurveValue(DisableAOCurve), 0.f, 1.f);
	return FMath::Lerp(FVector2D(Delta.Yaw, Delta.Pitch), FVector2D::ZeroVector, DisableAlpha);
}

// ======================================================================================
// Motion matching — the AnimGraph hands us the node; C++ does the rest.
// ======================================================================================

TArray<UPoseSearchDatabase*> UAZ_CmcAnimInstance::Get_DatabasesToSearch() const
{
	// Union of every matching gate row — EvaluateChooserMulti semantics without the chooser (see
	// FAZ_DatabaseGate). Order preserved: earlier rows land earlier in the array, like earlier chooser
	// rows. The engine node AddUniques on ingest, so overlap between rows is harmless.
	MatchedGateLabels.Reset();

	TArray<UPoseSearchDatabase*> Result;
	for (const FAZ_DatabaseGate& Gate : DatabaseGates)
	{
		if (!Gate.Matches(MovementMode, Stance, MovementState, Gait))
		{
			continue;
		}
		MatchedGateLabels.Add(Gate.Label);
		for (const TObjectPtr<UPoseSearchDatabase>& Database : Gate.Databases)
		{
			if (Database)
			{
				Result.Add(Database);
			}
		}
	}

	// DELIBERATE GASP DEVIATION (user call 2026-08-21): Starts pools are searchable only while
	// actually STARTING. GASP searches starts at any speed and relies on its 130+ pivot clips to
	// outbid them during moving turns; our pools have no such content, so mid-turn queries kept
	// electing Start heads ("when we turn we have a start anim playing"). Below the cap covers both
	// legitimate cases: the idle launch, and the near-stop plant of a reversal — where a 180-start
	// IS the right answer. The CurrentDatabaseTags guard keeps the pool searchable while a start is
	// already the selection, so one is never cut mid-play by its own filter.
	// LOWERED 100 -> 50 on 2026-08-24, and the number is read off a log rather than chosen. A turn
	// during the run loop swings Acceleration off Velocity, CalcVelocity takes the friction branch, and
	// speed craters 375 -> 67-89 BEFORE any animation is involved. At a cap of 100 that dip made the
	// Starts pool legal mid-turn, and the trajectory genuinely looks like "near-rest with a heading
	// change", so RunFwdStart180/135/90 won on cost — seven times in one session. That is the user's
	// "when i turn during the run loop, MM takes turn anims": the deceleration was making the wrong
	// clip legal. The separation is clean and empirical: genuine starts entered at spd=5 and 23, every
	// turn leak entered at 67-89, and nothing landed in between. 50 splits them with margin on both
	// sides. Raising it back above ~60 reopens the leak.
	static const FName StartsTag(TEXT("Starts"));
	if (Speed2D > StartsSearchMaxSpeed2D && !CurrentDatabaseTags.Contains(StartsTag))
	{
		Result.RemoveAll([](const UPoseSearchDatabase* Db)
		{
			return Db && Db->Tags.Contains(StartsTag);
		});
	}

	// Second class under the same doctrine (user call, same day): Stops are only for STOPPING, and
	// stopping is an INPUT fact, not a velocity fact. A held stick mid-turn brakes exactly like a
	// stop — the trajectory cannot tell them apart, which is how "turning at some speed takes the
	// stop anim" — but input can: released stick = stop legitimate, held stick = never. Uses the
	// same tolerance IsMoving() uses for its acceleration term, so "input held" means the same thing
	// everywhere. Same currently-playing guard as Starts: a selected stop is never cut by its own
	// filter (re-pressing input mid-stop interrupts it through normal cost competition instead).
	static const FName StopsTag(TEXT("Stops"));
	const bool bInputHeld = !Acceleration.Equals(FVector::ZeroVector, IsMovingAccelerationTolerance);

	// THE CONTENT FLOOR. A release below StopAnimEnterSpeed has no stop clip within reach — the slowest
	// stop depicts 147 cm/s — so offering the pool only invites a badly-costed match on a body moving at
	// a third of that, which is what "no stop animation plays, it just slides" actually was on a tap of W.
	// The movement side already decided this ONCE at the stop edge and holds it, so the pool cannot
	// flicker as the body decays through the threshold. Below the floor the honest answer is to blend to
	// idle, and no amount of selection work can manufacture the missing content.
	const bool bSubFloorStop = CharacterProperties.bStopActive && !CharacterProperties.bStopIsAnimated;

	if ((bInputHeld || bSubFloorStop) && !CurrentDatabaseTags.Contains(StopsTag))
	{
		Result.RemoveAll([](const UPoseSearchDatabase* Db)
		{
			return Db && Db->Tags.Contains(StopsTag);
		});
	}

	// Third class — PIVOTS, restored 2026-08-24 (user report: "when i turn during the run loop, MM
	// tries to take turn anims, but i need them only if i pass some conditions").
	//
	// A gate like this lived here for a few hours on 2026-08-21 and was removed the same day because a
	// whole session logged ZERO pivot selections. That failure is worth stating precisely, because the
	// fix is one line different: the old gate ALSO required a speed that the deceleration itself was
	// destroying, so it shut mid-event. Two things make this version survive:
	//   1. The window is evaluated BEFORE the plant exists. With input held against velocity, CMC takes
	//      the friction branch (CalcVelocity :3923) and speed decays only AFTER the reversal begins —
	//      so at the flip frame, which is the frame that matters, Speed2D is still full loop speed.
	//   2. The standard currently-playing guard. Once a pivot IS the selection its pool cannot be cut
	//      by its own filter, so the collapse that killed the old gate is unreachable by construction.
	//
	// The content justifies the 135 floor rather than taste: every pivot clip we own rotates a full
	// 180 (measured 2026-08-24: 179.8-180.0 deg on all six), and the MM path has no RotationMethod::
	// Scale, so a 180 asset serving a 90 request over-rotates by 90. Below this angle the honest answer
	// is the loop plus rotation rate, which is what the arc/lean setup already does well.
	//
	// NOTE the tag also covers AnimPro_RunArchLoop_L/R, which share PSD_AZ_Stand_Run_Pivots. They are
	// currently DISABLED at the database-entry level and kept there deliberately as an experiment
	// toggle (user, 2026-08-24), so this gate does not affect them either way. Leave them in place.

	// ★ INPUT MUST BE HELD. Get_TrajectoryTurnAngle is (Acceleration.Rotation() - Velocity.Rotation()),
	// and on a released stick Acceleration is ZERO, so Acceleration.Rotation() collapses to the zero
	// rotator and the "angle" degenerates into the negated world heading of the velocity. Measured
	// 2026-08-24: a plain run-and-release logged a rock-steady turn=144 for 12+ consecutive frames with
	// accel=0.00 — not a reversal at all, just a stop whose heading happened to sit past the threshold.
	// That opened the pivot window through the entire stop and let pivots compete with the Stops pool.
	// A reversal is an INPUT fact, exactly as the Stops gate above already establishes.
	const bool bPivotInputHeld = !Acceleration.Equals(FVector::ZeroVector, IsMovingAccelerationTolerance);
	const float ReversalAngle = bPivotInputHeld ? FMath::Abs(Get_TrajectoryTurnAngle()) : 0.f;
	const bool bReversalCommitted = bPivotInputHeld
		&& (ReversalAngle >= PivotSearchMinTurnAngle)
		&& (Speed2D >= PivotSearchMinSpeed2D);

	if (!bReversalCommitted && !CurrentDatabaseTags.Contains(PivotDatabaseTag))
	{
		Result.RemoveAll([this](const UPoseSearchDatabase* Db)
		{
			return Db && Db->Tags.Contains(PivotDatabaseTag);
		});
	}

	// Logged only WHILE OPEN, deliberately. The first version edge-logged through a function-local
	// static and the trace was unreadable: a second UAZ_CmcAnimInstance in the level (idle, spd=0)
	// shares that static and flipped it every frame, so every real line came paired with a phantom
	// "shut | turn=0 spd=0". Statics in an AnimInstance method are per-CLASS, not per-instance — the
	// same scar as the stop contract's function-local latches. An open window is a condition no idle
	// instance can satisfy, so keying the log on it removes the confound without any shared state.
	// Silence here still means the window never opened, which was the question this log exists for.
	if (bReversalCommitted)
	{
		UE_LOG(LogTemp, Display, TEXT("[CmcPivot] window OPEN | turn=%.0f spd=%.0f poolSize=%d pivotsInPool=%d"),
			ReversalAngle, Speed2D, Result.Num(),
			Result.ContainsByPredicate([this](const UPoseSearchDatabase* Db)
				{ return Db && Db->Tags.Contains(PivotDatabaseTag); }) ? 1 : 0);
	}

	// Fourth class — the leak the doctrine table had marked "watch": with every other wrong
	// candidate gated, mid-turn queries fell through to the crouch<->stand clips (measured 2026-08-21:
	// Idle2Crouch/Crouch2Idle selected at cost 3.2-4.6 during walking turns, and as rm-on transitions
	// their near-zero root motion braked the capsule too). Stance transitions have exactly one
	// legitimate trigger: the stance actually changing. Gate on that edge, with the standard
	// currently-playing guard so the clip finishes once chosen. After this, every class in the pools
	// has an explicit competition condition — there is no ungated fallback left.
	// RM MONTAGE OWNS THE POSE: drop the one-shot pools while a start/stop montage is playing. The
	// montage sits on DefaultSlot above the graph, so whatever MM picks underneath is invisible — but it
	// still runs, still churns, and still pollutes the trace (measured 2026-08-25: Start135_L -> _R ->
	// _L in 19ms steps beneath an RM start). Nothing downstream needs a one-shot while the slot drives,
	// so the honest answer is not to search for one.
	// THE GAME-THREAD SNAPSHOT, not GetCurrentActiveMontage(). This function runs on the anim WORKER
	// thread, and calling GetCurrentActiveMontage() from it read game-thread montage state cross-thread
	// — which failed INTERMITTENTLY: measured 2026-08-25, Walk_Starts was searched and elected from at
	// +101ms into a playing montage (db=PSD_AZ_Stand_Walk_Starts under an active RM start), while other
	// frames gated correctly. The snapshot is written once per frame in NativeUpdateAnimation (game
	// thread, same discipline as every _GT member in this file) and covers the montage's whole life
	// including both blend phases.
	if (GCmcMontageWasActive)
	{
		static const FName RmStartsTag(TEXT("Starts"));
		static const FName RmStopsTag(TEXT("Stops"));
		Result.RemoveAll([](const UPoseSearchDatabase* Db)
		{
			return Db && (Db->Tags.Contains(RmStartsTag) || Db->Tags.Contains(RmStopsTag));
		});
	}

	static const FName StanceTransTag(TEXT("StanceTrans"));
	if ((Stance == Stance_LastFrame) && !CurrentDatabaseTags.Contains(StanceTransTag))
	{
		Result.RemoveAll([](const UPoseSearchDatabase* Db)
		{
			return Db && Db->Tags.Contains(StanceTransTag);
		});
	}

	// Fifth class — TURN IN PLACE, wired 2026-08-24. PSD_AZ_Stand_TurnInPlace (TurnLt/Rt90_Loop,
	// TurnLt/Rt180) and PSD_AZ_Crouch_TurnInPlace existed, were tagged, and were referenced by NO gate
	// row, so ShouldTurnInPlace() was computed every frame with nothing able to act on it.
	//
	// The condition REUSES ShouldTurnInPlace() rather than restating it. That predicate already owns
	// the question and encodes the right doctrine: a turn-in-place is legal while AIMING (the body
	// catching up to the camera, which is the whole point of the feature) or on the single frame we
	// stopped. Duplicating the test here would be a second owner of one fact, and the handedness bugs
	// in this file all trace to exactly that.
	//
	// This is also the one turn case that works WITHOUT capsule root motion, which is why it is safe
	// to serve from MM while moving pivots are not: at idle with no input,
	// ComputeOrientToMovementRotation returns CurrentRotation, so the capsule is stationary BY DESIGN
	// and the visible rotation is carried entirely by the clip through OffsetRootBone. There is no
	// capsule-vs-clip mismatch to resolve because the capsule is not a participant.
	//
	// ⚠ KNOWN FRAGILITY: ShouldTurnInPlace()'s non-aiming path is bJustStopped, a ONE-FRAME edge
	// (Idle this frame, Moving last). The search has to land on that exact frame; if it does, the
	// currently-playing guard below keeps the pool alive for the rest of the clip, but if it misses,
	// the turn is simply lost. Aiming holds continuously and does not have this problem. Widening the
	// stopped window needs a latch, and a latch needs a member, which needs an editor-closed build.
	static const FName TurnInPlaceTag(TEXT("TurnInPlace"));
	if (!ShouldTurnInPlace() && !CurrentDatabaseTags.Contains(TurnInPlaceTag))
	{
		Result.RemoveAll([](const UPoseSearchDatabase* Db)
		{
			return Db && Db->Tags.Contains(TurnInPlaceTag);
		});
	}

	return Result;
}

namespace AZ::CmcAnim
{
	/** Database tag marking the deceleration one-shots. Matches the literal used by Get_DatabasesToSearch. */
	static const FName StopsTagName(TEXT("Stops"));

	/** Below this the character counts as stopped: the stop clip's remaining frames are settle, not travel. */
	static constexpr float StoppedSpeedTolerance = 2.f;

	/** Play-rate floor while a Stops clip is selected. Far below the loop floor (0.8) on purpose — see
	 *  Get_DynamicPlayRate. 0.2 lets a stop settle into its plant instead of striding in place. */
	static constexpr float StopsMinPlayRate = 0.2f;

	/** Tag on the idle pools. Suppressed while a stop is still playing — see KeepPlayingOneShotSearchable. */
	static const FName IdlesTagName(TEXT("Idles"));


}

/**
 * Keep the CURRENTLY PLAYING one-shot's database in the searchable set until its clip is mostly done.
 *
 * WHY. The gate table is addressed by MovementState, so the frame Speed2D reaches 0 the matching row
 * flips WalkMove -> StandIdle and the Stops database leaves the union entirely. InterruptOnDatabaseChange
 * then invalidates the continuing pose *because its database is no longer listed* — so the stop clip is
 * evicted by SET MEMBERSHIP, not outbid on cost. Measured 2026-08-22: stop clips survived a median of
 * 82ms against 0.93-1.53s of content, and an OverrideContinuingPoseCostBias of -1.0 changed nothing,
 * because a cost bias cannot help a candidate that was never in the search.
 *
 * This does NOT suppress anything. The pool still carries every database the new state wants, so a
 * re-press mid-stop still puts Starts/Pivots in the search and they win on cost exactly as before. The
 * only thing prevented is a clip being yanked out from under itself while it is still playing.
 *
 * Looping assets are excluded: they have no natural end, so holding one would pin the search forever.
 */
void UAZ_CmcAnimInstance::KeepPlayingOneShotSearchable(
	const FMotionMatchingAnimNodeReference& MotionMatchingNode, TArray<UPoseSearchDatabase*>& Pool) const
{
	FPoseSearchBlueprintResult Current;
	bool bIsResultValid = false;
	UMotionMatchingAnimNodeLibrary::GetMotionMatchingSearchResult(MotionMatchingNode, Current, bIsResultValid);
	if (!bIsResultValid || !Current.SelectedDatabase || Current.bLoop)
	{
		return;
	}

	const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(Current.SelectedAnim);
	if (!Sequence)
	{
		return;
	}

	const float PlayLength = Sequence->GetPlayLength();
	if (PlayLength <= 0.f)
	{
		return;
	}

	const bool bIsStop = CurrentDatabaseTags.Contains(AZ::CmcAnim::StopsTagName);

	// INPUT-RELEASE ESCAPE. One blanket fraction cannot express per-state interruption rules. A Start held
	// to 70% of a ~1.2s clip OUTLIVES the entire tap that spawned it: release W after 0.2s and the start
	// kept its database in the pool — and with it the continuing pose — while the body decelerated to a
	// halt. That is precisely the reported "we slide but no animation plays". A start is interrupted by
	// the stick being released; a stop is not, because the release is what ASKED for it.
	if (!bIsStop && CharacterProperties.bStopActive)
	{
		return;
	}

	const float KeepAliveFraction =
		bIsStop ? StopKeepAliveFractionTunable : OneShotKeepAliveFractionTunable;

	if (Current.SelectedTime >= KeepAliveFraction * PlayLength)
	{
		return;
	}

	Pool.AddUnique(const_cast<UPoseSearchDatabase*>(Current.SelectedDatabase.Get()));

	// While a stop is still playing, IDLE IS NOT A CANDIDATE.
	//
	// Keeping the stop's database in the pool only prevents EVICTION; it never stopped Idle out-bidding
	// the stop's continuing pose on cost once the character was stationary. Measured 2026-08-23: most
	// stops were cut at 50-63% of the clip — BELOW the keep-alive threshold — so membership was not the
	// binding constraint, cost was. The same measurement cleared the movement of blame: the capsule
	// travelled 174cm against a clip depicting 167cm, i.e. it arrives correctly and only the POSE was
	// being replaced early. Removing the idle pools for the duration is the deterministic fix.
	//
	// Deliberately narrow: Starts, Loops and Pivots all stay in the search, so re-pressing input still
	// interrupts a stop on cost exactly as before. The only thing suppressed is idling out of a stop that
	// has not finished.
	// Suppress the idle pools ONLY WHILE STILL DECELERATING.
	//
	// The point of the suppression is to stop Idle out-bidding a stop clip mid-deceleration. Once the
	// character is actually stationary that job is done, and keeping it on is unsafe: the release
	// condition was a fraction of the CURRENT clip, but a stop handing off to its opposite-foot variant
	// RESETS SelectedTime to the new clip's entry frame, so the release point was never reached and the
	// two stop clips traded indefinitely with Idle locked out (measured 2026-08-23: stuck in
	// RunFwdStop_LU at spd=0, no transition to Idle at all). A speed bound cannot loop, because speed
	// only decreases while stopping.
	//
	// The cost is that a stop may still be cut once the body halts. That is a lesser failure than being
	// trapped, and the real fix for the handoff is a BlockTransition window on the stop clips' tails so a
	// second stop cannot be ENTERED late — content, not gating.
	if (bIsStop && Speed2D > AZ::CmcAnim::StoppedSpeedTolerance)
	{
		// NEVER empty the pool. At Speed2D 0 the only matching gate row is StandIdle, whose only database
		// is Idles-tagged — removing it left the search with NOTHING and the character with no animation
		// at all for ~480ms (measured 2026-08-23: "Stop_LU -> None, db=None gates=[]"). Suppression is
		// only ever legitimate when something else remains playable.
		const bool bHasNonIdleAlternative = Pool.ContainsByPredicate([](const UPoseSearchDatabase* Db)
		{
			return Db && !Db->Tags.Contains(AZ::CmcAnim::IdlesTagName);
		});

		if (bHasNonIdleAlternative)
		{
			Pool.RemoveAll([](const UPoseSearchDatabase* Db)
			{
				return Db && Db->Tags.Contains(AZ::CmcAnim::IdlesTagName);
			});
		}
	}
}

void UAZ_CmcAnimInstance::Update_MotionMatching(const FAnimNodeReference& Node)
{
	EAnimNodeReferenceConversionResult Conversion = EAnimNodeReferenceConversionResult::Failed;
	const FMotionMatchingAnimNodeReference MotionMatchingNode =
		UMotionMatchingAnimNodeLibrary::ConvertToMotionMatchingNode(Node, Conversion);
	if (Conversion != EAnimNodeReferenceConversionResult::Succeeded)
	{
		return;
	}

	TArray<UPoseSearchDatabase*> Pool = Get_DatabasesToSearch();
	KeepPlayingOneShotSearchable(MotionMatchingNode, Pool);
	if (Pool.IsEmpty())
	{
		// ================== EMPTY UNION — FAIL SAFE, DO NOT JUST RETURN ==================
		// The old behaviour (return, leave the node on its last pushed pool) was reasoned to "degrade to
		// slightly-stale selection". MEASURED 2026-08-23: it degrades to NO ANIMATION. The last pushed
		// pool can itself have been narrowed by KeepPlayingOneShotSearchable (idles suppressed during a
		// stop), so when the held clip ends the node has nothing left that matches and the search returns
		// a NULL anim — 300-400ms with no pose, twelve times in one session.
		//
		// The trigger is almost always mode=InAir: there is no airborne row in DatabaseGates at all, so
		// every frame off the ground empties the union. It also flips GetMaxBrakingDeceleration to the
		// falling value, which breaks stop prediction for as long as it lasts.
		//
		// Retrying the match as if GROUNDED is the fail-safe: airborne locomotion is unauthored, so the
		// grounded row for the same stance/state/gait is the nearest legal content rather than an
		// arbitrary fallback. This does NOT excuse the missing rows — authoring them is the real fix and
		// the warning stays — but the character must never have zero animation.
		for (const FAZ_DatabaseGate& Gate : DatabaseGates)
		{
			if (!Gate.Matches(EAZ_MovementMode::OnGround, Stance, MovementState, Gait))
			{
				continue;
			}
			for (const TObjectPtr<UPoseSearchDatabase>& Database : Gate.Databases)
			{
				if (Database)
				{
					Pool.AddUnique(Database);
				}
			}
		}

		if (!bWarnedEmptyGateUnion)
		{
			bWarnedEmptyGateUnion = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[CmcAnim] DatabaseGates union is EMPTY for mode=%d stance=%d state=%d gait=%d — ")
				TEXT("fell back to the GROUNDED rows (%d db). Add a matching gate row."),
				static_cast<int32>(MovementMode), static_cast<int32>(Stance),
				static_cast<int32>(MovementState), static_cast<int32>(Gait), Pool.Num());
		}

		// Only if even the grounded retry found nothing is holding the previous pool the best available
		// answer — at that point the gate table is empty for this stance/state/gait and there is nothing
		// legal to offer.
		if (Pool.IsEmpty())
		{
			return;
		}
	}
	else
	{
		bWarnedEmptyGateUnion = false;
	}

	// Every update, like GASP: the node only stores the array + NextUpdateInterruptMode, and
	// InterruptOnDatabaseChange does its own is-the-continuing-pose-still-in-the-set check downstream.
	// Gating this call on "did the pool change" would starve that logic of the interrupt mode.
	UMotionMatchingAnimNodeLibrary::SetDatabasesToSearch(
		MotionMatchingNode, Pool, Get_MMInterruptMode());
}

void UAZ_CmcAnimInstance::Update_MotionMatching_PostSelection(const FAnimNodeReference& Node)
{
	EAnimNodeReferenceConversionResult Conversion = EAnimNodeReferenceConversionResult::Failed;
	const FMotionMatchingAnimNodeReference MotionMatchingNode =
		UMotionMatchingAnimNodeLibrary::ConvertToMotionMatchingNode(Node, Conversion);
	if (Conversion != EAnimNodeReferenceConversionResult::Succeeded)
	{
		return;
	}

	FPoseSearchBlueprintResult Result;
	bool bIsResultValid = false;
	UMotionMatchingAnimNodeLibrary::GetMotionMatchingSearchResult(MotionMatchingNode, Result, bIsResultValid);

	// DIVERGENCE: GASP writes these unconditionally. We guard on validity — an invalid result carries a
	// null database, and GetDatabaseTags would then WIPE CurrentDatabaseTags for that frame, which flips
	// IsStarting and ShouldSpinTransition on mid-pivot. Holding the previous frame's selection is the
	// stabler read of "the search told us nothing new".
	if (bIsResultValid)
	{
		CurrentSelectedAnim = Result.SelectedAnim;
		CurrentSelectedDatabase = Result.SelectedDatabase;
		SearchCost = Result.SearchCost;

		// The clip's OWN playback time. Needed because the movement layer samples MoveData_Speed straight
		// off the selected sequence: UAnimInstance::GetCurveValue returns the BLEND-WEIGHTED value across
		// everything still active in the BlendStack, so during a stop's blend-in it is dominated by the
		// outgoing LOOP. Measured 2026-08-23: it reported 167-172 cm/s at the start of every walk stop —
		// the walk loop's 172.6, not the stop clip's 147 peak — which made the capsule coast instead of brake.
		CurrentSelectedTime = Result.SelectedTime;

		// Straight off the search, so we never need the state machine's BlendStack to know this.
		bCurrentAssetLooping = Result.bLoop;

		CurrentDatabaseTags.Reset();
		if (CurrentSelectedDatabase)
		{
			UPoseSearchLibrary::GetDatabaseTags(CurrentSelectedDatabase, CurrentDatabaseTags);
		}
	}

	// DIVERGENCE: GASP additionally calls OverrideMotionMatchingBlendSettings here with a FLAT 0.2s
	// HermiteCubic blend. We do not, for two reasons and the second is the real one:
	//
	//  1. FMotionMatchingBlueprintBlendSettings declares a default constructor but its struct carries no
	//     export macro, so the symbol is not linkable from a game module.
	//  2. It would be a SECOND owner of the blend time. Get_MMBlendTime is bound to the node's BlendTime
	//     pin and is state-aware (0.5 steady / 0.2 on touchdown / 0.15 rising); a flat post-selection
	//     override runs after it every frame and silently wins, which makes the state-aware version
	//     dead code. One owner per fact — we keep the one that reacts to what the character is doing.
}

bool UAZ_CmcAnimInstance::EnableSteering(const FAnimNodeReference& Node) const
{
	// DIVERGENCE: UAZ_AnimInstance added a BlendStackInputs.bLoop fallback and included Sliding. 5.8 is
	// just "going somewhere, and something is playing".
	const bool bGoingSomewhere = (MovementState == EAZ_MovementState::Moving)
		|| (MovementMode == EAZ_MovementMode::InAir);

	return bGoingSomewhere && UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimIsActive(Node);
}

// ======================================================================================
// Derived predicates — GASP 5.8. Every divergence from the older UAZ_AnimInstance port is
// called out, because in each case ours was the stale behaviour.
// ======================================================================================

bool UAZ_CmcAnimInstance::IsStarting() const
{
	// DIVERGENCE: UAZ_AnimInstance additionally required Speed2D < 100 and keyed off bHasVelocity.
	// 5.8 has no such clamp and uses IsMoving(). The clamp suppressed start detection for anything
	// already moving at a walk, which is exactly when a gear-change start should fire.
	return IsMoving()
		&& (Trj_FutureVelocity.Size2D() >= Velocity.Size2D() + StartingFutureSpeedMargin)
		&& !CurrentDatabaseTags.Contains(PivotDatabaseTag);
}

bool UAZ_CmcAnimInstance::ShouldSpinTransition() const
{
	// DIVERGENCE, and the significant one: 5.8 measures the CAPSULE-vs-OFFSET-ROOT yaw — how far the
	// mesh root has fallen behind the capsule — where UAZ_AnimInstance used a predicted future facing
	// delta. Those are unrelated quantities. 5.8's is also the better signal: a spin is precisely what
	// you need once accumulated offset grows too large to interpolate away.
	const float OffsetYaw = FMath::Abs(static_cast<float>(
		(CharacterTransform.Rotator() - RootTransform.Rotator()).GetNormalized().Yaw));

	return (OffsetYaw >= SpinTransitionAngleThreshold)
		&& (Speed2D >= SpinTransitionMinSpeed)
		&& !CurrentDatabaseTags.Contains(PivotDatabaseTag);
}

FQuat UAZ_CmcAnimInstance::Get_DesiredFacing() const
{
	// 5.8 DROPPED the SteeringTargetTime curve lookup entirely and just samples the trajectory at a
	// fixed time. That is load-bearing for us: our library carries ZERO clips with that curve, so the
	// older implementation would have returned a constant.
	FTransformTrajectorySample Sample;
	UPoseSearchTrajectoryLibrary::GetTransformTrajectorySampleAtTime(
		Trajectory, DesiredFacingSampleTime, Sample, /*bExtrapolate*/ false);
	return Sample.Facing;
}

float UAZ_CmcAnimInstance::Get_AO_Yaw() const
{
	// Strafe only. Orienting to movement, the body already follows the stick; while aiming, the body is
	// turned to the aim. In both the residual yaw is zero by construction, so only strafe has an offset.
	return (RotationMode == EAZ_RotationMode::Strafe) ? static_cast<float>(Get_AOValue().X) : 0.f;
}

FAZ_MovementDirectionThresholds UAZ_CmcAnimInstance::Get_MovementDirectionThresholds() const
{
	// Travelling forward or backward, the quadrants are symmetric and there is nothing to disambiguate.
	if (MovementDirection == EAZ_MovementDirection::F || MovementDirection == EAZ_MovementDirection::B)
	{
		return DirectionThresholds_Cardinal;
	}

	// Mid-pivot the direction is changing fast; the narrow back quadrant stops it latching sideways.
	if (IsPivoting())
	{
		return DirectionThresholds_Cardinal;
	}

	// Settled into a loop and not aiming: widen the back quadrant so shallow rearward strafes keep
	// their sideways clip instead of flipping to backward every few frames.
	if (bCurrentAssetLooping && !CharacterProperties.InputState.bWantsToAim)
	{
		return DirectionThresholds_SideLoop;
	}

	return DirectionThresholds_SideTight;
}

// ======================================================================================
// AnimGraph node settings
// ======================================================================================

float UAZ_CmcAnimInstance::Get_MMBlendTime() const
{
	if (MovementMode == EAZ_MovementMode::InAir)
	{
		return (Velocity.Z > MMRisingVelocityZ) ? MMBlendTime_Rising : MMBlendTime_Falling;
	}

	// DIVERGENCE: UAZ_AnimInstance had these inverted — 0.2 in steady ground and 0.5 on landing. 5.8
	// blends FAST on the touchdown frame (so the land reads as an impact) and slow once settled.
	return (MovementMode_LastFrame == EAZ_MovementMode::InAir) ? MMBlendTime_JustLanded : MMBlendTime_Ground;
}

float UAZ_CmcAnimInstance::Get_MMNotifyRecencyTimeOut() const
{
	switch (Gait)
	{
	case EAZ_Gait::Sprint: return MMNotifyRecency_Sprint;
	case EAZ_Gait::Run:    return MMNotifyRecency_Run;
	default:               return MMNotifyRecency_Walk;
	}
}

EPoseSearchInterruptMode UAZ_CmcAnimInstance::Get_MMInterruptMode() const
{
	// A movement-MODE change always interrupts. Everything else only interrupts on the ground, because
	// mid-air the continuing pose is the only thing keeping the jump coherent.
	const bool bModeChanged = (MovementMode != MovementMode_LastFrame);

	const bool bGroundedStateChanged =
		((MovementState != MovementState_LastFrame)
			|| ((Gait != Gait_LastFrame) && (MovementState == EAZ_MovementState::Moving))
			|| (Stance != Stance_LastFrame))
		&& (MovementMode == EAZ_MovementMode::OnGround);

	// THE STOP EDGE INVALIDATES THE CONTINUING POSE.
	//
	// BlockTransition cannot cover this case: it is applied through FSearchFilters, built only in the
	// CANDIDATE search paths (PoseSearchDatabase.cpp:2371/2490/2693). SearchContinuingPose (:1874) never
	// builds them, so a continuing pose is exempt from every filter — including any tail block we author.
	//
	// The consequence, measured 2026-08-23: releasing the stick again while a previous stop clip was
	// still playing INHERITED that clip wholesale, so the new stop began at clipTime 0.5-0.99s with the
	// clip depicting 24-50 cm/s against a body at 155. Curve-driven braking then saw a 130 cm/s
	// disagreement and pinned at its clamp — twenty hard snaps in five seconds.
	//
	// Invalidating on the edge forces the search to find a pose that genuinely matches, rather than
	// keeping one that only happens to be playing. Deliberately the EDGE and not the whole stop: holding
	// it for the duration would re-search every frame and reintroduce the churn the continuing pose exists
	// to damp.
	if (bStopActive && !bStopActive_LastFrame)
	{
		return EPoseSearchInterruptMode::InterruptOnDatabaseChangeAndInvalidateContinuingPose;
	}

	// The TIP-latch edge invalidates for the same reason the stop edge does: the continuing pose at that
	// moment is Idle (or an idle break), it is exempt from every filter, and with the body stationary it
	// costs almost nothing — left alone it would out-compete the turn clip and the lock would sit in its
	// no-selection grace until the watchdog released it. Edge only, not the whole lock, as above.
	if (bTurnInPlaceActive && !bTurnInPlaceActive_LastFrame)
	{
		return EPoseSearchInterruptMode::InterruptOnDatabaseChangeAndInvalidateContinuingPose;
	}

	// The montage-ended edge (see NativeUpdateAnimation). Consumed on read so it fires exactly once.
	if (GCmcMontageJustEnded)
	{
		GCmcMontageJustEnded = false;
		return EPoseSearchInterruptMode::InterruptOnDatabaseChangeAndInvalidateContinuingPose;
	}

	// DIVERGENCE: UAZ_AnimInstance did not gate the state change on being grounded and added a
	// direction-changed term. Both made mid-air searches restart, which is what the continuing pose exists
	// to prevent.
	return (bModeChanged || bGroundedStateChanged)
		? EPoseSearchInterruptMode::InterruptOnDatabaseChange
		: EPoseSearchInterruptMode::DoNotInterrupt;
}

EOffsetRootBoneMode UAZ_CmcAnimInstance::Get_OffsetRootRotationMode() const
{
	// Montage playing: release the accumulated rotation offset so the montage owns facing.
	// NOTE this is GASP's behaviour and it is also the case Epic's own known-issues list flags as
	// problematic when combined with motion warping. 5.8 added LockOffsetIncreaseAndConsumeAnimation,
	// which would let a warped montage close the offset without widening it. Left as Release for parity;
	// revisit here first if warped melee reads wrong on v3.
	// REVERTED to Accumulate on user request (2026-08-24 night), restoring GASP parity.
	//
	// FOR THE RECORD, because the measurement was unambiguous and will be relevant if the symptom
	// returns: the engine defines Accumulate as "the offset will COUNTER the motion, and the root will
	// STAY IN PLACE", so every degree the capsule turns is actively held out of the mesh and never
	// converges. With Interpolate ("the root will stay behind, but will attempt to catch up") the mesh
	// lag beyond 30 degrees went from 52%% of frames to 0%%, and the "character ends a turn facing the
	// wrong direction" symptom disappeared. Accumulate also makes RotationHalfLife inert, since that
	// property is documented as "how fast the rotation offset is BLENDED OUT".
	//
	// GASP ships Accumulate because its capsule rotates INSTANTLY (RotationRate -1), so the offset IS
	// the visible turn. Ours rotates at a finite rate, which is the condition that makes the two modes
	// behave differently. If the wrong-facing ending comes back, this line is the first suspect.
	return IsSlotActive(MontageSlotName) ? EOffsetRootBoneMode::Release : EOffsetRootBoneMode::Accumulate;
}

EOffsetRootBoneMode UAZ_CmcAnimInstance::Get_OffsetRootTranslationMode() const
{
	if (IsSlotActive(MontageSlotName))
	{
		return EOffsetRootBoneMode::Release;
	}
	if (MovementMode != EAZ_MovementMode::OnGround)
	{
		return EOffsetRootBoneMode::Release;
	}
	// Standing still, release: an interpolating translation offset at zero speed is visible drift.
	return IsMoving() ? EOffsetRootBoneMode::Interpolate : EOffsetRootBoneMode::Release;
}

float UAZ_CmcAnimInstance::Get_OffsetRootTranslationHalfLife() const
{
	// DIVERGENCE: UAZ_AnimInstance added a third, sprint-specific value. 5.8 has two.
	return (MovementState == EAZ_MovementState::Idle) ? OffsetRootHalfLife_Idle : OffsetRootHalfLife_Moving;
}

EOrientationWarpingSpace UAZ_CmcAnimInstance::Get_OrientationWarpingWarpingSpace() const
{
	return bOffsetRootBoneEnabled
		? EOrientationWarpingSpace::RootBoneTransform
		: EOrientationWarpingSpace::ComponentTransform;
}

bool UAZ_CmcAnimInstance::AllowFootPinning() const
{
	// DIVERGENCE: UAZ_AnimInstance gated on a config bool where 5.8 gates on IsMoving(). Pinning a foot
	// while standing still is what makes an idle look glued to the floor during a turn.
	return (MovementMode == EAZ_MovementMode::OnGround) && IsMoving();
}

float UAZ_CmcAnimInstance::Get_DynamicPlayRate(float MinPlayRate, float MaxPlayRate) const
{
	// No authored reference speed means the ratio is meaningless, so return the honest no-op rather than
	// a number derived from a zero. 745 of our clips carry MoveData_Speed; the LM_RM_* set does not.
	const float MoveDataSpeed = GetCurveValue(MoveDataSpeedCurve);
	if (MoveDataSpeed <= UE_KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}

	// A DECELERATING one-shot needs a far wider floor than a loop does. On a stop the ground speed reaches
	// zero while the clip still depicts a stride, so the honest ratio dives toward 0 — clamping that at a
	// loop's 0.8 keeps the feet striding on a stationary character, which is precisely the slide the
	// MoveData_Speed curves were authored to remove. Letting the rate fall to StopsMinPlayRate lets the
	// clip settle into its plant instead. Measured 2026-08-23: the stop clips depict a ~0.95s glide
	// (147-388 cm/s decaying linearly to 0), while the capsule stops in Speed/BrakingDecel — so without
	// this the two only agree at exactly one braking value, and braking stops being a free gameplay dial.
	const bool bStopSelected = CurrentDatabaseTags.Contains(AZ::CmcAnim::StopsTagName);

	// Once the character has actually STOPPED, the rest of the clip is the settle and plant — it carries
	// no ground motion, so matching stride speed to ground speed is meaningless there and the ratio would
	// pin at the floor. Crawling at 0.2x meant SelectedTime never reached the keep-alive release point,
	// which held the idle pools suppressed on a clip that was effectively finished (measured 2026-08-23).
	// Play the settle at authored speed and let it end.
	if (bStopSelected && Speed2D <= AZ::CmcAnim::StoppedSpeedTolerance)
	{
		return 1.f;
	}

	// ONE CONTROLLER OWNS STOP PHASE. With the stop contract active the MOVEMENT already guarantees the
	// stop's duration (braking is solved from the clip's own ~0.93s), so a second controller warping the
	// clip's clock toward the speed ratio is not a safety net — it fights the guarantee. Both terms decay
	// linearly, so the ratio does not settle: integrating dy/dx = k(1-x)/(1-y) with k = 269/147 finishes
	// a 0.92s clip in 0.30s of a 0.93s stop. MaxPlayRate caps that at 0.77s, which is milder but still
	// early, and the cap means the warp cannot fix a large mismatch anyway.
	//
	// What is LEFT over is a DISTANCE error, not a timing one — the capsule covers EntrySpeed*T/2 while
	// the clip depicts its own authored travel, and they agree only when EntrySpeed matches the clip's
	// peak. Play rate cannot close that gap: it changes cadence, not stride length. Stage 2 owns it
	// (distance-matched phase and/or stride warping); [CmcStop] logs the error so stage 2 has a target.
	if (bStopSelected && CharacterProperties.bStopActive && CharacterProperties.bStopIsAnimated)
	{
		return 1.f;
	}

	const float EffectiveMin = bStopSelected ? AZ::CmcAnim::StopsMinPlayRate : MinPlayRate;

	const float Ratio = FMath::Clamp(Speed2D / MoveDataSpeed, EffectiveMin, MaxPlayRate);
	const float WarpAlpha = FMath::Clamp(GetCurveValue(PlayRateWarpingCurve), 0.f, 1.f);
	return FMath::Lerp(1.f, Ratio, WarpAlpha);
}
