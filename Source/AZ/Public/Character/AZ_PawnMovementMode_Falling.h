// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/Modes/FallingMode.h"
#include "AZ_PawnMovementMode_Falling.generated.h"

UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "AZ Pawn Movement Mode - Falling"))
class AZ_API UAZ_PawnMovementMode_Falling : public UFallingMode
{
	GENERATED_BODY()

public:
	virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState,
		const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

	/** Per-axis turning-rate cap (deg/s) applied to the air angular-velocity override.
	 *  GASP uses 300; lower values make air turns lazier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Falling", meta = (ClampMin = "0", ForceUnits = "deg/s"))
	float AirTurningRateLimit = 300.f;
};
