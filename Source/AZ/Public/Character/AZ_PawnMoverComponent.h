// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "AZ_PawnMoverComponent.generated.h"

/**
 * UAZ_PawnMoverComponent — thin UCharacterMoverComponent subclass.
 *
 * Sole reason for existing right now: call RefreshSharedSettings() in the ctor so
 * the CDO's SharedSettings array is populated with every UCommonLegacyMovementSettings
 * instance the default modes (Walking / Falling / Flying) need. Without this,
 * a fresh Blueprint child of the pawn class is born with an empty SharedSettings
 * array baked into its serialized state — at PIE the modes' OnRegistered ensure-fails:
 *   "Failed to find instance of CommonLegacyMovementSettings on …DefaultWalkingMode"
 *
 * UMoverComponent::RefreshSharedSettings is protected, so we expose it here only by
 * subclassing and invoking it. PostInitProperties on instances also calls it, but
 * an instanced subobject duplicated from a BP CDO does not re-run PostInitProperties
 * — duplicates inherit whatever the parent CDO has serialized.
 *
 * Future v2 work may add real overrides here. For now this is intentionally minimal.
 */
UCLASS(ClassGroup=(Movement), meta=(BlueprintSpawnableComponent))
class AZ_API UAZ_PawnMoverComponent : public UCharacterMoverComponent
{
	GENERATED_BODY()

public:
	UAZ_PawnMoverComponent();

protected:
	// Override OnRegister so we can populate SharedSettings BEFORE the parent calls
	// FindAndRegisterDefaults, which triggers each mode's OnRegistered → lookup of
	// CommonLegacyMovementSettings. NewObject is forbidden inside ctors, allowed here.
	virtual void OnRegister() override;
};
