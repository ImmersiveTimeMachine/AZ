// Copyright Artur. AZ project.

#include "Character/AZ_PawnMoverComponent.h"

#include "Character/AZ_PawnMovementMode_RMAction.h"

UAZ_PawnMoverComponent::UAZ_PawnMoverComponent()
{
	// Intentionally empty. RefreshSharedSettings NewObjects each settings instance,
	// and NewObject is forbidden inside a UObject ctor. We do the work in OnRegister.
}

void UAZ_PawnMoverComponent::OnRegister()
{
	// UMoverComponent::OnRegister calls FindAndRegisterDefaults, which fires every
	// mode's OnRegistered. Those handlers do
	//   FindSharedSettings<UCommonLegacyMovementSettings>()
	// and ensure-fail if the SharedSettings array is empty. PostInitProperties /
	// PostLoad / PostCDOCompiled in the base class populate SharedSettings, but a
	// BP CDO can persistently serialize an empty SharedSettings override that survives
	// those hooks. RefreshSharedSettings here guarantees the array is populated for
	// each running instance before the modes are registered.

	// Register the RM-action mode (gravity-free, no floor-snap; follows the jump clip's root-motion
	// attribute incl. the vertical Z — fixes Walking's floor-snap eating the jump lift). Added here,
	// BEFORE Super::OnRegister → FindAndRegisterDefaults, so it is registered alongside Walking/Falling.
	// Code-managed rather than in the BP CDO mode map: it is never the starting mode, only entered on
	// demand by the AnimInstance (QueueNextMode). Idempotent — FindMovementModeByName guards re-adds.
	static const FName RMActionModeName(TEXT("RMAction"));
	if (!FindMovementModeByName(RMActionModeName))
	{
		AddMovementModeFromClass(RMActionModeName, UAZ_PawnMovementMode_RMAction::StaticClass());
	}

	RefreshSharedSettings();
	Super::OnRegister();
}
