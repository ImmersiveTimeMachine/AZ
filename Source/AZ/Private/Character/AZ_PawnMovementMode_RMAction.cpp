// Copyright Artur. AZ project.

#include "Character/AZ_PawnMovementMode_RMAction.h"

#include "MoverComponent.h"
#include "MoverDataModelTypes.h"          // FMoverDefaultSyncState
#include "MoverSimulationTypes.h"         // FSimulationTickParams, FMoverTickEndData, CommonBlackboard
#include "MoverTypes.h"                   // Mover_IsInAir, FMoverOnImpactParams
#include "MoveLibrary/MoverBlackboard.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/MovementRecord.h"

UAZ_PawnMovementMode_RMAction::UAZ_PawnMovementMode_RMAction()
{
	// Air state for any system that queries it (perception, FX, gameplay). DELIBERATELY does NOT add
	// Mover.SkipAnimRootMotion (would zero the whole RM contribution — RootMotionAttributeLayeredMove.cpp:138)
	// nor Mover.SkipVerticalAnimRootMotion (would strip the vertical lift — MovementModeStateMachine.cpp:327).
	// Keeping both off is the entire point of this mode: full OverrideAll root motion, vertical Z included.
	GameplayTags.AddTag(Mover_IsInAir);

	// No SharedSettingsClasses: this mode reads nothing from CommonLegacyMovementSettings (no floor sweep,
	// no gravity, no speed clamps). Stays dependency-free so registering it can't perturb shared settings.
}

void UAZ_PawnMovementMode_RMAction::GenerateMove_Implementation(const FMoverSimContext& SimContext,
	const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	// No self-generated motion: zero velocity, AdditiveVelocity mix (FProposedMove default) so it contributes
	// nothing. NO gravity (the difference from Falling) — on ticks the root-motion attribute isn't driving,
	// the capsule HOLDS. NO angular velocity — the clip's own root-motion yaw turns the body; we don't
	// air-steer toward input during an RM action. When the layered move is OverrideAll-active it fully
	// replaces this anyway (and MovementModeStateMachine skips this GenerateMove entirely under
	// SkipGenerateMoveIfOverridden).
	OutProposedMove = FProposedMove();
}

void UAZ_PawnMovementMode_RMAction::SimulationTick_Implementation(const FSimulationTickParams& Params,
	FMoverTickEndData& OutputState)
{
	UMoverComponent* MoverComp = GetMoverComponent();
	USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
	if (!UpdatedComponent || !MoverComp)
	{
		return;
	}

	const FMoverDefaultSyncState* StartingSyncState =
		Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	FMoverDefaultSyncState& OutputSyncState =
		OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
	const FProposedMove& ProposedMove = Params.ProposedMove;

	FMovementRecord MoveRecord;
	MoveRecord.SetDeltaSeconds(DeltaSeconds);

	// Orientation: fold in any angular velocity the move proposed (the clip's root-motion yaw, when present).
	const FRotator StartingOrient = StartingSyncState->GetOrientation_WorldSpace();
	const FRotator TargetOrient = UMovementUtils::ApplyAngularVelocityToRotator(
		StartingOrient, ProposedMove.AngularVelocityDegrees, DeltaSeconds);
	const FQuat TargetOrientQuat = TargetOrient.Quaternion();

	// Position: the full root-motion translation INCLUDING the vertical Z. Swept (bSweep=true) so the capsule
	// still resolves penetration and stops at blocking geometry (walls/ceilings) instead of tunnelling — but
	// crucially there is NO FindFloor / TryMoveToAdjustHeightAboveFloor afterwards, so the lift is NOT eaten.
	const FVector MoveDelta = ProposedMove.LinearVelocity * DeltaSeconds;
	FHitResult MoveHit(1.f);
	UMovementUtils::TrySafeMoveUpdatedComponent(
		Params.MovingComps, MoveDelta, TargetOrientQuat, /*bSweep*/ true, MoveHit, ETeleportType::None, MoveRecord);

	if (MoveHit.IsValidBlockingHit())
	{
		// Hit a wall/ceiling mid-arc. Report the impact (registered handlers may react); we intentionally do
		// NOT slide along the surface — a jump-in-place rarely needs it and the AnimInstance ends the action by
		// clip length regardless. The capsule simply stops against the surface; remaining root motion is absorbed.
		// NOTE: non-const — UMoverComponent::HandleImpact takes a mutable FMoverOnImpactParams& (do not add const).
		FMoverOnImpactParams ImpactParams(MoverComp->GetMovementModeName(), MoveHit, MoveDelta);
		MoverComp->HandleImpact(ImpactParams);
	}

	// We are airborne, so the floor we left is stale. Invalidate it (and any dynamic base) so the Walking mode
	// re-queries a fresh floor when the AnimInstance hands control back to it at the end of the action.
	if (UMoverBlackboard* SimBlackboard = MoverComp->GetSimBlackboard_Mutable())
	{
		SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);
		SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);
	}

	const FVector FinalLocation = UpdatedComponent->GetComponentLocation();
	const FVector EffectiveVelocity = MoveRecord.GetRelevantVelocity();

	OutputSyncState.MoveDirectionIntent =
		(ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector);
	OutputSyncState.SetTransforms_WorldSpace(
		FinalLocation,
		UpdatedComponent->GetComponentRotation(),
		EffectiveVelocity,
		ProposedMove.AngularVelocityDegrees,
		nullptr); // no movement base while airborne

	UpdatedComponent->ComponentVelocity = EffectiveVelocity;
}
