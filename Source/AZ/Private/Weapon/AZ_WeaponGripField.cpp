// Copyright Artur. AZ project.

#include "Weapon/AZ_WeaponGripField.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AZ_WeaponGripField)

float UAZ_WeaponGripField::Sample(const FVector& Point) const
{
	if (!IsValidField())
	{
		return OutsideDistance;
	}
	const FVector Grid = (Point - Origin) / Spacing;
	const int32 X = FMath::FloorToInt(Grid.X);
	const int32 Y = FMath::FloorToInt(Grid.Y);
	const int32 Z = FMath::FloorToInt(Grid.Z);
	if (X < 0 || Y < 0 || Z < 0 || X >= Dims.X - 1 || Y >= Dims.Y - 1 || Z >= Dims.Z - 1)
	{
		return OutsideDistance;
	}
	const float TX = static_cast<float>(Grid.X - X);
	const float TY = static_cast<float>(Grid.Y - Y);
	const float TZ = static_cast<float>(Grid.Z - Z);
	const int32 StrideX = Dims.Y * Dims.Z;
	const int32 StrideY = Dims.Z;
	const int32 Base = X * StrideX + Y * StrideY + Z;
	const float* V = Distances.GetData();
	const float C00 = FMath::Lerp(V[Base], V[Base + 1], TZ);
	const float C01 = FMath::Lerp(V[Base + StrideY], V[Base + StrideY + 1], TZ);
	const float C10 = FMath::Lerp(V[Base + StrideX], V[Base + StrideX + 1], TZ);
	const float C11 = FMath::Lerp(V[Base + StrideX + StrideY], V[Base + StrideX + StrideY + 1], TZ);
	return FMath::Lerp(FMath::Lerp(C00, C01, TY), FMath::Lerp(C10, C11, TY), TX);
}

float UAZ_WeaponGripField::SampleUnbounded(const FVector& Point) const
{
	if (!IsValidField())
	{
		return OutsideDistance;
	}
	// Clamp into the last full cell so Sample() stays inside; the leftover is the distance to the box.
	const FVector Min = Origin;
	const FVector Max = Origin + FVector(Dims.X - 1, Dims.Y - 1, Dims.Z - 1) * Spacing - FVector(0.001 * Spacing);
	const FVector Clamped(FMath::Clamp(Point.X, Min.X, Max.X), FMath::Clamp(Point.Y, Min.Y, Max.Y), FMath::Clamp(Point.Z, Min.Z, Max.Z));
	return Sample(Clamped) + static_cast<float>(FVector::Dist(Point, Clamped));
}
