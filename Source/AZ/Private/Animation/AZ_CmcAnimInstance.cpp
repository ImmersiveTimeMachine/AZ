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

				if (CurrentSelectedAnim.Get() != LastSelectedAnim.Get())
				{
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
						TEXT("[CmcSel] #%d dt=%.0fms %s -> %s | cost=%+.2f spd=%.0f turn=%.0f accel=%.2f ")
						TEXT("moving=%d pivot=%d tip=%d | cmd=%s sel=%s wantSprint=%d wantWalk=%d ")
						TEXT("| db=%s gates=[%s]"),
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

	VelocityAcceleration = (Velocity - Velocity_LastFrame) / FMath::Max(DeltaSeconds, 0.001f);
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
}

// ======================================================================================
// Update_MovementDirection — AZ-only (the MM node selects by trajectory; our CHT rows do not).
// ======================================================================================

void UAZ_CmcAnimInstance::Update_MovementDirection()
{
	// FootSpeed_L/R, not contact_l/r. "Planted" = slow AND slower than the other foot, so idle (both
	// feet near zero) still resolves to a definite foot instead of flickering between them.
	const float FootSpeedL = GetCurveValue(FootSpeedCurveL);
	const float FootSpeedR = GetCurveValue(FootSpeedCurveR);
	bLeftFootDown = (FootSpeedL < FootPlantedSpeedThreshold) && (FootSpeedL <= FootSpeedR);

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

	// Which budget to normalise against depends on whether we are speeding up or slowing down, and the
	// dot of acceleration against velocity is what says which.
	const bool bSpeedingUp = FVector::DotProduct(Acceleration, Velocity) > 0.0;
	const float Budget = bSpeedingUp ? MaxAcceleration : MaxDeceleration;

	const FVector Clamped = VelocityAcceleration.GetClampedToMaxSize(Budget) / Budget;

	// CharacterTransform here, NOT RootTransform — GASP measures this one against the capsule.
	return CharacterTransform.GetRotation().UnrotateVector(Clamped);
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
	// TODO: promote the cap to an EditDefaultsOnly UPROPERTY at the next editor-closed build.
	static constexpr float StartsSearchMaxSpeed2D = 100.f;
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

	// NOTE 2026-08-21: a pivot hard-turn gate (v2's bHardTurn>=135 analog) lived here for a few
	// hours and was REMOVED the same day: Get_TrajectoryTurnAngle collapses the moment velocity flips
	// through the plant and Speed2D dips during the decel, so the gate read false at exactly the frames
	// a reversal selects its pivot — a whole session logged ZERO pivot selections. Its justification
	// (a mis-fired 180 must not rotate the capsule) belonged to the reverted RootMotionFromEverything
	// experiment. Pivots compete freely; commitment bounds the cost of a shallow-turn over-serve.

	// Fourth and FINAL class — the leak the doctrine table had marked "watch": with every other wrong
	// candidate gated, mid-turn queries fell through to the crouch<->stand clips (measured 2026-08-21:
	// Idle2Crouch/Crouch2Idle selected at cost 3.2-4.6 during walking turns, and as rm-on transitions
	// their near-zero root motion braked the capsule too). Stance transitions have exactly one
	// legitimate trigger: the stance actually changing. Gate on that edge, with the standard
	// currently-playing guard so the clip finishes once chosen. After this, every class in the pools
	// has an explicit competition condition — there is no ungated fallback left.
	static const FName StanceTransTag(TEXT("StanceTrans"));
	if ((Stance == Stance_LastFrame) && !CurrentDatabaseTags.Contains(StanceTransTag))
	{
		Result.RemoveAll([](const UPoseSearchDatabase* Db)
		{
			return Db && Db->Tags.Contains(StanceTransTag);
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

	/** Stops are held LONGER than other one-shots: their last third is the settle and plant, and cutting
	 *  it is what made a stop read as unfinished. Measured 2026-08-23: stops played only 50-76% of the
	 *  clip while the CAPSULE arrived correctly (run travelled 174cm against the clip's 167cm), so the
	 *  fault was entirely the pose being replaced early, not the movement. */
	static constexpr float StopKeepAliveFraction = 0.9f;

	/** Hold a one-shot's database in the pool until this fraction of the clip has played. A FRACTION and
	 *  not a fixed remaining-time: our stop clips run 0.933-1.533s, and any fixed "N seconds left" cut
	 *  releases the short ones almost immediately and the long ones far too late. */
	static constexpr float OneShotKeepAliveFraction = 0.7f;
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
		bIsStop ? AZ::CmcAnim::StopKeepAliveFraction : AZ::CmcAnim::OneShotKeepAliveFraction;

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
		// No gate matched. Not calling SetDatabasesToSearch leaves the node on the last pushed pool
		// (the override array persists on the node), which degrades to slightly-stale selection instead
		// of a frozen pose from searching nothing. Warn once per dead spot — a hole in the gate table
		// is authoring debt, not a per-frame event.
		if (!bWarnedEmptyGateUnion)
		{
			bWarnedEmptyGateUnion = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[CmcAnim] DatabaseGates union is EMPTY for mode=%d stance=%d state=%d gait=%d — ")
				TEXT("MM holds the previous pool. Add a matching gate row."),
				static_cast<int32>(MovementMode), static_cast<int32>(Stance),
				static_cast<int32>(MovementState), static_cast<int32>(Gait));
		}
		return;
	}
	bWarnedEmptyGateUnion = false;

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
