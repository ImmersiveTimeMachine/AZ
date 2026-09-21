#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AZ_MapDefinition.generated.h"

class UTexture2D;

/** Invertible rectangular XY calibration. UV(0,0) is the image top-left, independent of texture aspect. */
UCLASS(BlueprintType)
class AZ_API UAZ_MapDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	FName MapId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	FName LayerId = TEXT("Outdoor");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	TObjectPtr<UTexture2D> MapTexture;

	/** Centre of the calibrated rectangle. Z is the explicit inverse-projection plane; no ground is inferred. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map|Calibration", meta=(Units="cm"))
	FVector WorldOrigin = FVector::ZeroVector;

	/** Full positive rectangle spans along its rotated local X/Y axes, in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map|Calibration", meta=(Units="cm"))
	FVector2D WorldSizeCm = FVector2D(100000.0, 100000.0);

	/** World yaw of the rectangle local X axis. North/art orientation is authored, never guessed from world axes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map|Calibration", meta=(Units="deg"))
	double RotationDegrees = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map|Calibration")
	bool bFlipU = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map|Calibration")
	bool bFlipV = false;

	/** Does not clamp: outside-world points produce outside UVs. */
	UFUNCTION(BlueprintPure, Category="Map")
	bool WorldToMapNormalized(const FVector& WorldLocation, FVector2D& OutMapNormalized) const;

	/** Does not clamp. Returns an XY location at WorldOrigin.Z; callers decide whether to trace for terrain. */
	UFUNCTION(BlueprintPure, Category="Map")
	bool MapNormalizedToWorld(const FVector2D& MapNormalized, FVector& OutWorldLocation) const;

	UFUNCTION(BlueprintPure, Category="Map")
	bool ContainsMapNormalized(const FVector2D& MapNormalized, double Inset = 0.0) const;

	/** Validates identity and numeric calibration. Artwork can remain unassigned while authoring. */
	UFUNCTION(BlueprintPure, Category="Map")
	bool ValidateDefinition(FText& OutError) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
