// Copyright Artur. AZ project.

#include "Character/AZ_PawnMovementMode_Falling.h"

#include "Animation/AZ_LocomotionTypes.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverDataModelTypes.h"

void UAZ_PawnMovementMode_Falling::GenerateMove_Implementation(const FMoverSimContext& SimContext,
	const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	// Phase 1: parent supplies linear physics (gravity, air control, terminal velocity).
	Super::GenerateMove_Implementation(SimContext, StartState, TimeStep, OutProposedMove);

	// Cache the current input structs from the data collection for easy retrieval throughout this function.
	const FCharacterDefaultInputs* DefaultInputs =
		StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FAZ_MoverCustomInputs* CustomInputs =
		StartState.InputCmd.InputCollection.FindDataByType<FAZ_MoverCustomInputs>();

	// Phase 3: write OutProposedMove.AngularVelocityDegrees so the capsule yaw still steers
	// toward (OrientationIntent + RotationOffset) while airborne. Without this, the engine
	// UFallingMode does linear physics only and the capsule keeps whatever yaw it had at
	// liftoff — visibly broken when the player turns the camera mid-jump.
	const FMoverDefaultSyncState* SyncState =
		StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	if (!SyncState || !DefaultInputs)
	{
		return;
	}

	const FRotator CurrentRotator = SyncState->GetOrientation_WorldSpace();
	const FVector OrientationIntent = DefaultInputs->OrientationIntent;
	const float YawFromIntent = static_cast<float>(OrientationIntent.Rotation().Yaw);
	const float RotationOffsetDeg = CustomInputs ? static_cast<float>(CustomInputs->RotationOffset) : 0.f;
	const FRotator TargetRotator(0.f, YawFromIntent + RotationOffsetDeg, 0.f);

	// Deterministic sim time — NOT GetWorldDeltaSeconds (would diverge under NetworkPrediction replay).
	const float DeltaSeconds = TimeStep.StepMs * 0.001f;

	OutProposedMove.AngularVelocityDegrees = UMovementUtils::ComputeAngularVelocityDegrees(
		CurrentRotator, TargetRotator, DeltaSeconds, AirTurningRateLimit);
}
