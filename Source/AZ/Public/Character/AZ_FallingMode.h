// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/FallingMode.h"
#include "AZ_FallingMode.generated.h"

/**
 * GASP-parity Falling mode for AZ. Mirrors BP_MovementMode_Falling — adds nothing
 * to the parent UFallingMode physics, but writes OutProposedMove.AngularVelocityDegrees
 * each tick so the capsule yaw still steers toward (OrientationIntent + RotationOffset)
 * while airborne, capped at AirTurningRateLimit deg/s. Without this override, the
 * pawn does not rotate while in the air.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "AZ Falling Mode"))
class AZ_API UAZ_FallingMode : public UFallingMode
{
	GENERATED_BODY()

public:
	virtual void GenerateMove_Implementation(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep,
		FProposedMove& OutProposedMove) const override;

	/** Per-axis turning-rate cap (deg/s) applied to the air angular-velocity override.
	 *  GASP uses 300; lower values make air turns lazier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Falling", meta = (ClampMin = "0", ForceUnits = "deg/s"))
	float AirTurningRateLimit = 300.f;
};
