// Copyright Artur. AZ project.

#include "Character/AZ_PawnMovementMode_Walking.h"

#include "Animation/AZ_LocomotionTypes.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/World.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "TimerManager.h"

UAZ_PawnMovementMode_Walking::UAZ_PawnMovementMode_Walking()
{
	// GASP CDO overrides — engine defaults are bSmoothFacingWithDoubleSpring=true, FacingSmoothingTime=0.25.
	// Double-spring adds an S-curve lead-out (cinematic feel); GASP turns it off for crisp locomotion
	// response. FacingSmoothingTime baseline is overridden per-tick by the gait map, but the CDO value
	// is used on the first tick before the override runs.
	bSmoothFacingWithDoubleSpring = false;
	FacingSmoothingTime = 0.5f;
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

void UAZ_PawnMovementMode_Walking::OnRegistered(const FName ModeName, const FMoverSimContext& SimContext)
{
	Super::OnRegistered(ModeName, SimContext);
	MyModeName = ModeName;

	if (UMoverComponent* Mover = GetMoverComponent<UMoverComponent>())
	{
		Mover->OnMovementModeChanged.AddDynamic(this, &UAZ_PawnMovementMode_Walking::HandleMovementModeChanged);
	}
}

void UAZ_PawnMovementMode_Walking::OnUnregistered(const FMoverSimContext& SimContext)
{
	if (UMoverComponent* Mover = GetMoverComponent<UMoverComponent>())
	{
		Mover->OnMovementModeChanged.RemoveDynamic(this, &UAZ_PawnMovementMode_Walking::HandleMovementModeChanged);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(JustLandedTimerHandle);
	}
	bJustLanded = false;
	Super::OnUnregistered(SimContext);
}

void UAZ_PawnMovementMode_Walking::HandleMovementModeChanged(const FName& Previous, const FName& Next)
{
	// Sticky-landing: latch JustLanded for JustLandedDuration when entering this mode from the Falling mode.
	if (Next == MyModeName && Previous == FallingModeName)
	{
		bJustLanded = true;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(JustLandedTimerHandle, this,
				&UAZ_PawnMovementMode_Walking::OnJustLandedTimerExpired, JustLandedDuration, /*bLoop*/ false);
		}
	}
}

void UAZ_PawnMovementMode_Walking::OnJustLandedTimerExpired()
{
	bJustLanded = false;
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
	const UCharacterMoverComponent* CMC = Cast<UCharacterMoverComponent>(GetMoverComponent<UMoverComponent>());
	const bool bIsCrouching = CMC && CMC->IsCrouching();
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
	// desired facing direction.
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
	const float FacingDeltaAbs = static_cast<float>(FMath::Abs(
		FRotator::NormalizeAxis((CurrentFacing.Inverse() * DesiredFacing).Rotator().Yaw)));
	const float SnapShorten = static_cast<float>(FMath::GetMappedRangeValueClamped(
		FVector2D(CameraSnapShortenStartAngle, CameraSnapShortenFullAngle),
		FVector2D(0.f, CameraSnapShortenMaxSeconds),
		FacingDeltaAbs));
	FacingSmoothingTime = FMath::Max(0.f, FacingSmoothingTime - SnapShorten);

	// Phase 4: pass OverridenDesiredFacing (not raw DesiredFacing) to the parent spring-damper.
	Super::GenerateWalkMove_Implementation(StartState, DeltaSeconds, SimContext, DesiredVelocity,
		OverridenDesiredFacing, CurrentFacing, InOutAngularVelocityDegrees, InOutVelocity);
}
