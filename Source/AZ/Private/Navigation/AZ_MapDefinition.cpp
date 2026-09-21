#include "Navigation/AZ_MapDefinition.h"

bool UAZ_MapDefinition::ValidateDefinition(FText& OutError) const
{
	OutError = FText::GetEmpty();
	if (MapId.IsNone() || LayerId.IsNone())
	{
		OutError = NSLOCTEXT("AZNavigation", "MissingMapIdentity", "The map and layer must have stable identifiers.");
		return false;
	}
	if (WorldOrigin.ContainsNaN() || WorldSizeCm.ContainsNaN() || !FMath::IsFinite(RotationDegrees))
	{
		OutError = NSLOCTEXT("AZNavigation", "NonfiniteCalibration", "Map calibration values must be finite.");
		return false;
	}
	if (WorldSizeCm.X <= UE_SMALL_NUMBER || WorldSizeCm.Y <= UE_SMALL_NUMBER)
	{
		OutError = NSLOCTEXT("AZNavigation", "InvalidMapSpan", "Both map world spans must be positive and nonzero.");
		return false;
	}
	return true;
}

bool UAZ_MapDefinition::WorldToMapNormalized(const FVector& WorldLocation, FVector2D& OutMapNormalized) const
{
	OutMapNormalized = FVector2D::ZeroVector;
	FText Error;
	if (WorldLocation.ContainsNaN() || !ValidateDefinition(Error))
	{
		return false;
	}
	double SinAngle = 0.0;
	double CosAngle = 1.0;
	FMath::SinCos(&SinAngle, &CosAngle, FMath::DegreesToRadians(FMath::Fmod(RotationDegrees, 360.0)));
	const double DeltaX = WorldLocation.X - WorldOrigin.X;
	const double DeltaY = WorldLocation.Y - WorldOrigin.Y;
	// Inverse yaw moves the world point into the rectangle's two orthogonal local axes.
	const double LocalX = CosAngle * DeltaX + SinAngle * DeltaY;
	const double LocalY = -SinAngle * DeltaX + CosAngle * DeltaY;
	FVector2D Result(0.5 + LocalX / WorldSizeCm.X, 0.5 + LocalY / WorldSizeCm.Y);
	if (bFlipU) { Result.X = 1.0 - Result.X; }
	if (bFlipV) { Result.Y = 1.0 - Result.Y; }
	if (Result.ContainsNaN())
	{
		return false;
	}
	OutMapNormalized = Result;
	return true;
}

bool UAZ_MapDefinition::MapNormalizedToWorld(const FVector2D& MapNormalized, FVector& OutWorldLocation) const
{
	OutWorldLocation = FVector::ZeroVector;
	FText Error;
	if (MapNormalized.ContainsNaN() || !ValidateDefinition(Error))
	{
		return false;
	}
	const double U = bFlipU ? 1.0 - MapNormalized.X : MapNormalized.X;
	const double V = bFlipV ? 1.0 - MapNormalized.Y : MapNormalized.Y;
	const double LocalX = (U - 0.5) * WorldSizeCm.X;
	const double LocalY = (V - 0.5) * WorldSizeCm.Y;
	double SinAngle = 0.0;
	double CosAngle = 1.0;
	FMath::SinCos(&SinAngle, &CosAngle, FMath::DegreesToRadians(FMath::Fmod(RotationDegrees, 360.0)));
	const FVector Result(WorldOrigin.X + CosAngle * LocalX - SinAngle * LocalY,
		WorldOrigin.Y + SinAngle * LocalX + CosAngle * LocalY, WorldOrigin.Z);
	if (Result.ContainsNaN())
	{
		return false;
	}
	OutWorldLocation = Result;
	return true;
}

bool UAZ_MapDefinition::ContainsMapNormalized(const FVector2D& MapNormalized, double Inset) const
{
	return !MapNormalized.ContainsNaN() && FMath::IsFinite(Inset) && Inset >= 0.0 && Inset <= 0.5
		&& MapNormalized.X >= Inset && MapNormalized.X <= 1.0 - Inset
		&& MapNormalized.Y >= Inset && MapNormalized.Y <= 1.0 - Inset;
}

FPrimaryAssetId UAZ_MapDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("AZMap")), GetFName());
}
