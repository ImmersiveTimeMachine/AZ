// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "AZ_GrabCameraShakes.generated.h"

/**
 * Continuous low struggle rumble while held in a grab (perlin — organic, non-repeating). Infinite
 * duration: started by GA_PlayerGrabbed on catch, stopped when the grab resolves. bSingleInstance so
 * the per-press restart RE-SCALES the running shake (curve-driven intensity ramp) instead of stacking
 * a new instance per press. All numbers are EditDefaultsOnly-equivalent via a BP child if tuning wants
 * to leave C++.
 */
UCLASS()
class AZ_API UAZ_CameraShake_GrabRumble : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UAZ_CameraShake_GrabRumble(const FObjectInitializer& ObjInit);
};

/**
 * Short sharp jolt fired on each E-press of the struggle mash — the physical "wrench" feedback.
 * Finite (0.3s), self-terminating, allowed to overlap the rumble.
 */
UCLASS()
class AZ_API UAZ_CameraShake_GrabJolt : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UAZ_CameraShake_GrabJolt(const FObjectInitializer& ObjInit);
};
