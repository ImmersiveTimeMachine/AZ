// Copyright Artur. AZ project.

#include "Character/AZ_PawnMoverComponent.h"

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
	RefreshSharedSettings();
	Super::OnRegister();
}
