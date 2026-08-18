// Copyright Artur. AZ project.

#include "Animation/AZ_CmcAnimInstance.h"

#include "Character/Cmc/AZ_CmcCharacterBase.h"
#include "Engine/EngineTypes.h"
#include "Kismet/KismetSystemLibrary.h"   // EDrawDebugTrace

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
	OffsetRootTransform = InOffsetRootTransform;
	bHasOffsetRootTransform = true;
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
		DebugAccumulator += DeltaSeconds;
		if (DebugAccumulator >= 1.f)
		{
			DebugAccumulator = 0.f;
			const UEnum* DirEnum = StaticEnum<EAZ_MovementDirection>();
			UE_LOG(LogTemp, Display,
				TEXT("[CmcAnim] spd=%.0f/%.0f moving=%d pivot=%d tip=%d | accel=%.2f | dir=%s ang=%.0f Lfoot=%d ")
				TEXT("| trj past=%.0f cur=%.0f fut=%.0f turn=%.0f | land=%.2fs @ %.0f | samples=%d"),
				Speed2D, CharacterProperties.CurrentMaxSpeed, IsMoving(), IsPivoting(), ShouldTurnInPlace(),
				AccelerationAmount,
				DirEnum ? *DirEnum->GetNameStringByValue(static_cast<int64>(MovementDirection)) : TEXT("?"),
				MovementDirectionAngle, bLeftFootDown,
				Trj_PastVelocity.Size2D(), Trj_CurrentVelocity.Size2D(), Trj_FutureVelocity.Size2D(),
				Get_TrajectoryTurnAngle(),
				TrajectoryCollision.TimeToLand, TrajectoryCollision.LandSpeed, Trajectory.Samples.Num());
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
	// The OFFSET root, not the capsule. Yaw + 90 converts the offset node's space into the mesh's
	// (-90 yaw) convention, which is the frame every actor-relative value below is measured in.
	if (bHasOffsetRootTransform)
	{
		const FRotator OffsetRotation = OffsetRootTransform.Rotator();
		RootTransform = FTransform(
			FRotator(OffsetRotation.Pitch, OffsetRotation.Yaw + 90.f, OffsetRotation.Roll),
			OffsetRootTransform.GetTranslation(),
			FVector::OneVector);
	}
	else
	{
		// GASP's else-branch. Also where we land if the AnimGraph never calls SetOffsetRootTransform.
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
	Gait = CharacterProperties.Gait;

	Stance_LastFrame = Stance;
	Stance = CharacterProperties.Stance;
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
	// GASP: (Velocity != 0, tol 0.1) AND <literal true> AND (Acceleration != 0).
	// The middle term is a Trj_FutureVelocity test whose node is present but WIRED TO NOTHING — Epic
	// left it disconnected and the AND pin sits at its `true` default. Ported as it runs.
	const bool bVelocityNonZero = !Velocity.Equals(FVector::ZeroVector, IsMovingVelocityTolerance);
	const bool bAccelerationNonZero = !Acceleration.Equals(FVector::ZeroVector, IsMovingAccelerationTolerance);
	return bVelocityNonZero && bAccelerationNonZero;
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

	const float Ratio = FMath::Clamp(Speed2D / MoveDataSpeed, MinPlayRate, MaxPlayRate);
	const float WarpAlpha = FMath::Clamp(GetCurveValue(PlayRateWarpingCurve), 0.f, 1.f);
	return FMath::Lerp(1.f, Ratio, WarpAlpha);
}
