// Copyright Artur. AZ project.

#include "Character/AZ_PawnMovementMode_Walking.h"

#include "Animation/AZ_LocomotionTypes.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/World.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverTypes.h"   // FMoverEventContext
#include "DefaultMovementSet/Settings/StanceSettings.h"

UAZ_PawnMovementMode_Walking::UAZ_PawnMovementMode_Walking()
{
	// GASP CDO overrides — engine defaults are bSmoothFacingWithDoubleSpring=true, FacingSmoothingTime=0.25.
	// Double-spring adds an S-curve lead-out (cinematic feel); GASP turns it off for crisp locomotion
	// response. FacingSmoothingTime baseline is overridden per-tick by the gait map, but the CDO value
	// is used on the first tick before the override runs.
	bSmoothFacingWithDoubleSpring = false;
	FacingSmoothingTime = 0.5f;
	SharedSettingsClasses.Add(UStanceSettings::StaticClass());
}

FQuat UAZ_PawnMovementMode_Walking::ResolveRotationTarget(const FMoverTickStartData& StartState,
	const FQuat& DesiredFacing, const FQuat& CurrentFacing) const
{
	const FAZ_MoverCustomInputs* CustomInputs =
		StartState.InputCmd.InputCollection.FindDataByType<FAZ_MoverCustomInputs>();

	const double RotationOffsetIn = CustomInputs ? CustomInputs->RotationOffset : 0.0;
	const double ClampedOffset = FMath::Clamp(RotationOffsetIn,
		CachedRotationOffsetDegrees - RotationOffsetClampDegrees,
		CachedRotationOffsetDegrees + RotationOffsetClampDegrees);
	return (FRotator(0.0, ClampedOffset, 0.0).Quaternion() * DesiredFacing);
}

EAZ_Gait UAZ_PawnMovementMode_Walking::ResolveGait(const FMoverTickStartData& StartState) const
{
	const FAZ_MoverCustomInputs* CustomInputs =
		StartState.InputCmd.InputCollection.FindDataByType<FAZ_MoverCustomInputs>();
	return CustomInputs ? CustomInputs->Gait : EAZ_Gait::Walk;
}

void UAZ_PawnMovementMode_Walking::Activate(const FMoverEventContext& Context, FName PrevModeName,
	const FMoverSimContext& SimContext, const FMoverTickStartData& StartState,
	FMoverSyncState* OutSyncState, FMoverAuxStateContext* OutAuxState)
{
	Super::Activate(Context, PrevModeName, SimContext, StartState, OutSyncState, OutAuxState);

	// Sticky-landing: record the landing SIM time when entering from the Falling mode. The window itself
	// is derived in GenerateMove — sim-time math, no wall-clock timer, valid under time dilation and resim.
	if (PrevModeName == FallingModeName)
	{
		LandedSimTimeMs = Context.EventTimeMs;
	}
}

void UAZ_PawnMovementMode_Walking::GenerateMove_Implementation(const FMoverSimContext& SimContext,
	const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	// Derive the sticky-landing window in sim time, BEFORE Super runs the walk-move math (which calls our
	// GenerateWalkMove — the Deceleration pick below reads bJustLanded).
	bJustLanded = (LandedSimTimeMs >= 0.0) &&
		((TimeStep.BaseSimTimeMs - LandedSimTimeMs) < static_cast<double>(JustLandedDuration) * 1000.0);

	Super::GenerateMove_Implementation(SimContext, StartState, TimeStep, OutProposedMove);
}

void UAZ_PawnMovementMode_Walking::GenerateWalkMove_Implementation(FMoverTickStartData& StartState, float DeltaSeconds,
	const FMoverSimContext& SimContext, const FVector& DesiredVelocity, const FQuat& DesiredFacing,
	const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity)
{
	// Cache the engine default inputs for the MoveInput read below. Custom-input reads go through
	// the ResolveRotationTarget / ResolveGait virtuals so subclasses can swap rotation/gait policy
	// without touching this function.
	const FCharacterDefaultInputs* DefaultInputs =
		StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();

	// Apply the Rotation Offset input to the desired facing rotation, and save as the OverridenDesiredFacing.
	// The offset is clamped to not be greater than the current offset + 179, or less than the current offset - 179.
	// This means the pawn will always rotate TOWARD the Desired Facing direction, even if the shortest path
	// would've been away from it.
	const FQuat OverridenDesiredFacing = ResolveRotationTarget(StartState, DesiredFacing, CurrentFacing);

	// Cache the current offset between the current pawn orientation and desired facing orientation.
	{
		const FRotator Delta = (CurrentFacing.Inverse() * OverridenDesiredFacing).Rotator();
		CachedRotationOffsetDegrees = FRotator::NormalizeAxis(Delta.Yaw);
	}

	// Set the Max Speed depending on the Gait Input or Crouching state.
	const UCharacterMoverComponent* CharacterMover = Cast<UCharacterMoverComponent>(GetMoverComponent<UMoverComponent>());
	const bool bIsCrouching = CharacterMover && CharacterMover->IsCrouching();
	const EAZ_Gait Gait = ResolveGait(StartState);

	if (bIsCrouching)
	{
		MaxSpeedOverride = CrouchSpeed;
	}
	else
	{
		switch (Gait)
		{
		case EAZ_Gait::Walk:   MaxSpeedOverride = WalkSpeed;   break;
		case EAZ_Gait::Run:    MaxSpeedOverride = RunSpeed;    break;
		case EAZ_Gait::Sprint: MaxSpeedOverride = SprintSpeed; break;
		default:               MaxSpeedOverride = RunSpeed;    break;
		}
	}

	// Set the Acceleration depending on the Gait Input. The Sprint Acceleration is only applied
	// when moving faster than the run speed, effectively allowing us to tune how fast you can get
	// from stationary to the max run speed separately from how fast we can get from the run to
	// max sprint speed.
	const float CurrentSpeedXY = static_cast<float>(InOutVelocity.Size2D());
	float BaseAccel;
	switch (Gait)
	{
	case EAZ_Gait::Walk:   BaseAccel = WalkAcceleration; break;
	case EAZ_Gait::Run:    BaseAccel = RunAcceleration;  break;
	case EAZ_Gait::Sprint: BaseAccel = RunAcceleration;  break;
	default:               BaseAccel = RunAcceleration;  break;
	}
	const float SprintAdd = (CurrentSpeedXY > RunSpeed) ? SprintAcceleration : 0.f;
	Acceleration = BaseAccel + SprintAdd;

	// Set the Deceleration based on if there is Movement Input or not. This allows us to tune
	// stopping deceleration separately from Gait change deceleration, like when going from
	// sprint to run or run to walk. Also, we greatly increase the deceleration if we've just
	// landed which makes the landings feel sticky and responsive.
	const FVector WorldMoveInput = DefaultInputs ? DefaultInputs->GetMoveInput_WorldSpace() : FVector::ZeroVector;
	const bool bMoveInputZero = WorldMoveInput.IsNearlyZero();

	if (bJustLanded)
	{
		Deceleration = JustLandedDeceleration;
	}
	else
	{
		Deceleration = bMoveInputZero ? StoppingDeceleration : GaitChangeDeceleration;
	}

	// Set the Turning Strength, which controls how quickly the pawn can change its velocity
	// direction (not its rotation). This essentially controls how responsive the pawn is when
	// cornering. A low turning strength will make the pawn feel sluggish, and a high turning
	// strength will make the pawn feel snappy.
	TurningStrength = static_cast<float>(FMath::GetMappedRangeValueClamped(
		FVector2D(RunSpeed, SprintSpeed),
		FVector2D(WalkRunTurnStrength, SprintTurnStrength),
		CurrentSpeedXY));

	// Set the Facing Smoothing Time, which controls how fast the pawn can rotate to get to the
	// desired facing direction. Strafe (combat-ready) overrides it with a tight aim-lock.
	const FAZ_MoverCustomInputs* FacingInputs =
		StartState.InputCmd.InputCollection.FindDataByType<FAZ_MoverCustomInputs>();
	const bool bStrafeFacing = FacingInputs && FacingInputs->RotationMode == EAZ_RotationMode::Strafe;

	if (bStrafeFacing)
	{
		// Strafe (combat-ready): aim-lock spring at StrafeFacingTime (BP-tunable). The idle HOLD (don't follow the
		// camera) and the latched move-start align are driven by OrientationIntent in ProduceInput, NOT here — so
		// this just springs the body toward whatever target that produced (current facing at idle, camera when
		// moving/aligning).
		FacingSmoothingTime = StrafeFacingTime;
	}
	else
	{
		if (bMoveInputZero)
		{
			FacingSmoothingTime = IdleFacingTime;
		}
		else
		{
			FacingSmoothingTime = static_cast<float>(FMath::GetMappedRangeValueClamped(
				FVector2D(RunSpeed, SprintSpeed),
				FVector2D(WalkRunFacingTime, SprintFacingTime),
				CurrentSpeedXY));
		}

		// GASP BP_MovementMode_Walking parity: when |Δfacing| > 90°, subtract up to 0.2s
		// from FacingSmoothingTime so the capsule's angular velocity ramps up to at least
		// the control rotation's rate (which is high when the camera is turning quickly).
		// Without this, the spring damper under-rotates past ~135° and the mesh lags.
		// EXPLORE ONLY — in strafe the body already tracks the camera tightly, and this snap is exactly
		// what produced the catch-up spin when the camera was whipped around.
		const float FacingDeltaAbs = static_cast<float>(FMath::Abs(
			FRotator::NormalizeAxis((CurrentFacing.Inverse() * DesiredFacing).Rotator().Yaw)));
		const float SnapShorten = static_cast<float>(FMath::GetMappedRangeValueClamped(
			FVector2D(CameraSnapShortenStartAngle, CameraSnapShortenFullAngle),
			FVector2D(0.f, CameraSnapShortenMaxSeconds),
			FacingDeltaAbs));
		FacingSmoothingTime = FMath::Max(0.f, FacingSmoothingTime - SnapShorten);
	}

	// Phase 4: pass OverridenDesiredFacing (raw DesiredFacing + aim offset) to the parent spring-damper.
	Super::GenerateWalkMove_Implementation(StartState, DeltaSeconds, SimContext, DesiredVelocity,
		OverridenDesiredFacing, CurrentFacing, InOutAngularVelocityDegrees, InOutVelocity);
}
