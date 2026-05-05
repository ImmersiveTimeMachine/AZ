// Copyright Artur. AZ project.

#include "Character/AZ_SmoothWalkingMode.h"

#include "Animation/AZ_LocomotionTypes.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "TimerManager.h"
#include "Engine/World.h"

UAZ_SmoothWalkingMode::UAZ_SmoothWalkingMode()
{
	// GASP CDO overrides — engine defaults are bSmoothFacingWithDoubleSpring=true, FacingTime=0.25.
	bSmoothFacingWithDoubleSpring = false;
	FacingSmoothingTime = 0.5f;
}

void UAZ_SmoothWalkingMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);
	MyModeName = ModeName;

	if (UMoverComponent* Mover = GetMoverComponent<UMoverComponent>())
	{
		Mover->OnMovementModeChanged.AddDynamic(this, &UAZ_SmoothWalkingMode::HandleMovementModeChanged);
	}
}

void UAZ_SmoothWalkingMode::OnUnregistered()
{
	if (UMoverComponent* Mover = GetMoverComponent<UMoverComponent>())
	{
		Mover->OnMovementModeChanged.RemoveDynamic(this, &UAZ_SmoothWalkingMode::HandleMovementModeChanged);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(JustLandedTimerHandle);
	}
	bJustLanded = false;
	Super::OnUnregistered();
}

void UAZ_SmoothWalkingMode::HandleMovementModeChanged(const FName& Previous, const FName& Next)
{
	// GASP parity: latch JustLanded for 0.2s when entering Walking from Falling.
	if (Next == MyModeName && Previous == FName(TEXT("Falling")))
	{
		bJustLanded = true;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(JustLandedTimerHandle, this,
				&UAZ_SmoothWalkingMode::OnJustLandedTimerExpired, JustLandedDuration, /*bLoop*/ false);
		}
	}
}

void UAZ_SmoothWalkingMode::OnJustLandedTimerExpired()
{
	bJustLanded = false;
}

void UAZ_SmoothWalkingMode::GenerateWalkMove_Implementation(FMoverTickStartData& StartState, float DeltaSeconds,
	const FVector& DesiredVelocity, const FQuat& DesiredFacing, const FQuat& CurrentFacing,
	FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity)
{
	if (DeltaSeconds <= UE_KINDA_SMALL_NUMBER)
	{
		Super::GenerateWalkMove_Implementation(StartState, DeltaSeconds, DesiredVelocity, DesiredFacing,
			CurrentFacing, InOutAngularVelocityDegrees, InOutVelocity);
		return;
	}

	// === Phase 1 — input cache ===
	const FCharacterDefaultInputs* DefaultInputs =
		StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FAZ_MoverCustomInputs* CustomInputs =
		StartState.InputCmd.InputCollection.FindDataByType<FAZ_MoverCustomInputs>();

	// AZ inverted GASP's gait logic: Walk is the default, Shift = Sprint.
	// Fallback (no input collection entry) defaults to Walk to match.
	const EAZ_Gait Gait = CustomInputs ? CustomInputs->Gait : EAZ_Gait::Walk;
	const double RotationOffsetIn = CustomInputs ? CustomInputs->RotationOffset : 0.0;

	const FVector WorldMoveInput = DefaultInputs ? DefaultInputs->GetMoveInput_WorldSpace() : FVector::ZeroVector;
	const bool bMoveInputZero = WorldMoveInput.IsNearlyZero();

	UCharacterMoverComponent* CMC = Cast<UCharacterMoverComponent>(GetMoverComponent<UMoverComponent>());
	const bool bIsCrouching = CMC && CMC->IsCrouching();

	// === Phase 2 — DesiredFacing override (clamp RotationOffset to ±179° around prior offset) ===
	const double ClampedOffset = FMath::Clamp(RotationOffsetIn,
		CachedRotationOffsetDegrees - 179.0, CachedRotationOffsetDegrees + 179.0);

	const FQuat OverridenDesiredFacing = (FRotator(0.0, ClampedOffset, 0.0).Quaternion() * DesiredFacing);

	// Cache the new offset = signed yaw delta from CurrentFacing to OverridenDesiredFacing.
	{
		const FRotator Delta = (CurrentFacing.Inverse() * OverridenDesiredFacing).Rotator();
		CachedRotationOffsetDegrees = FRotator::NormalizeAxis(Delta.Yaw);
	}

	// === Phase 3 — per-tick parent property writes ===
	const float CurrentSpeedXY = static_cast<float>(InOutVelocity.Size2D());

	// 3-1: MaxSpeedOverride
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

	// 3-2: Acceleration — Walk/Run base, +Sprint additive when speed > RunSpeed
	float BaseAccel;
	switch (Gait)
	{
	case EAZ_Gait::Walk:   BaseAccel = WalkAcceleration; break;
	case EAZ_Gait::Run:    BaseAccel = RunAcceleration;  break;
	case EAZ_Gait::Sprint: BaseAccel = RunAcceleration;  break; // GASP: Run as base; Sprint adds on top
	default:               BaseAccel = RunAcceleration;  break;
	}
	const float SprintAdd = (CurrentSpeedXY > RunSpeed) ? SprintAcceleration : 0.f;
	Acceleration = BaseAccel + SprintAdd;

	// 3-3: Deceleration — sticky landing wins, else stopping vs gait-change
	if (bJustLanded)
	{
		Deceleration = JustLandedDeceleration;
	}
	else
	{
		Deceleration = bMoveInputZero ? StoppingDeceleration : GaitChangeDeceleration;
	}

	// 3-4: TurningStrength — speed-mapped Walk/Run -> Sprint
	TurningStrength = static_cast<float>(FMath::GetMappedRangeValueClamped(
		FVector2D(RunSpeed, SprintSpeed),
		FVector2D(WalkRunTurnStrength, SprintTurnStrength),
		CurrentSpeedXY));

	// 3-5: FacingSmoothingTime — gait-mapped + camera-snap shorten when |Δfacing| > 90°
	float BaseFacingTime;
	if (bMoveInputZero)
	{
		BaseFacingTime = IdleFacingTime;
	}
	else
	{
		BaseFacingTime = static_cast<float>(FMath::GetMappedRangeValueClamped(
			FVector2D(RunSpeed, SprintSpeed),
			FVector2D(WalkRunFacingTime, SprintFacingTime),
			CurrentSpeedXY));
	}

	// Camera-snap protection: when |Δfacing| > 90°, subtract up to 0.2s from facing time
	// so the capsule starts catching up immediately (matches BP_MovementMode_Walking comment).
	const float FacingDeltaAbs = static_cast<float>(FMath::Abs(
		FRotator::NormalizeAxis((CurrentFacing.Inverse() * OverridenDesiredFacing).Rotator().Yaw)));
	const float SnapShorten = static_cast<float>(FMath::GetMappedRangeValueClamped(
		FVector2D(90.f, 135.f), FVector2D(0.f, 0.2f), FacingDeltaAbs));
	FacingSmoothingTime = FMath::Max(0.f, BaseFacingTime - SnapShorten);

	// === Phase 4 — call parent with OverridenDesiredFacing instead of input DesiredFacing ===
	// The Mover/Anim pipeline consumes root motion from the AnimInstance (RootMotionMode is set
	// to RootMotionFromEverything in UAZ_AnimInstance::NativeInitializeAnimation), so authored
	// stop-clip RM drives the capsule directly — no manual extraction needed here.
	Super::GenerateWalkMove_Implementation(StartState, DeltaSeconds, DesiredVelocity,
		OverridenDesiredFacing, CurrentFacing, InOutAngularVelocityDegrees, InOutVelocity);
}
