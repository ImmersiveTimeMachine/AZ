// Copyright Artur. AZ project.

#include "Character/AZ_FallingMode.h"

#include "Animation/AZ_LocomotionTypes.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverDataModelTypes.h"

void UAZ_FallingMode::GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState,
	const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	// Phase 1 — let the parent handle gravity, air control, terminal-speed clamping, etc.
	Super::GenerateMove_Implementation(SimContext, StartState, TimeStep, OutProposedMove);

	// Phase 2 — read inputs.
	const FCharacterDefaultInputs* DefaultInputs =
		StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FAZ_MoverCustomInputs* CustomInputs =
		StartState.InputCmd.InputCollection.FindDataByType<FAZ_MoverCustomInputs>();

	const FMoverDefaultSyncState* SyncState =
		StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!SyncState)
	{
		return;
	}

	// Phase 3 — compute target yaw = OrientationIntent.Yaw + RotationOffset, then derive
	// the angular velocity needed to reach it within DeltaSeconds, capped at the per-axis limit.
	const FRotator CurrentRotator = SyncState->GetOrientation_WorldSpace();

	double IntentYaw = 0.0;
	if (DefaultInputs)
	{
		const FVector OrientationIntent = DefaultInputs->OrientationIntent;
		if (!OrientationIntent.IsNearlyZero())
		{
			IntentYaw = OrientationIntent.Rotation().Yaw;
		}
		else
		{
			IntentYaw = CurrentRotator.Yaw;
		}
	}
	else
	{
		IntentYaw = CurrentRotator.Yaw;
	}

	const double Offset = CustomInputs ? CustomInputs->RotationOffset : 0.0;
	const double TargetYaw = IntentYaw + Offset;

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;
	if (DeltaSeconds > UE_KINDA_SMALL_NUMBER)
	{
		OutProposedMove.AngularVelocityDegrees = UMovementUtils::ComputeAngularVelocityDegrees(
			CurrentRotator,
			FRotator(0.0, TargetYaw, 0.0),
			DeltaSeconds,
			AirTurningRateLimit);
	}
}
