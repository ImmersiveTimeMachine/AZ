// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AZ_WeaponGripField.generated.h"

/**
 * Baked signed-distance lattice of a weapon mesh: for every node of a regular 3D grid in the weapon mesh's space, the
 * distance (cm) to the weapon's surface, negative inside. The AZ Weapon Grip anim node samples it every frame to close
 * each finger until its phalanx touches the weapon (real-time grasp, no per-finger sockets).
 *
 * Baked offline from the weapon's triangles (Tools/wgs/bake_grip_field.py: exact nearest-triangle distance per part,
 * inside test by the generalised winding number, so open / overlapping parts are handled).
 */
UCLASS(BlueprintType)
class AZ_API UAZ_WeaponGripField : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Weapon-mesh-space position (cm) of lattice node (0, 0, 0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip Field")
	FVector Origin = FVector::ZeroVector;

	/** Distance between neighbouring nodes (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip Field", meta = (ClampMin = "0.01"))
	float Spacing = 0.5f;

	/** Node count per axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip Field")
	FIntVector Dims = FIntVector::ZeroValue;

	/** Signed distances (cm, < 0 inside), index = (X * Dims.Y + Y) * Dims.Z + Z. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip Field")
	TArray<float> Distances;

	/** Returned for points outside the lattice box (treated as free space). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grip Field")
	float OutsideDistance = 10.f;

	bool IsValidField() const
	{
		return Spacing > 0.f && Dims.X > 1 && Dims.Y > 1 && Dims.Z > 1
			&& Distances.Num() == Dims.X * Dims.Y * Dims.Z;
	}

	/** Trilinear signed distance at a weapon-mesh-space point. Thread-safe (read only). */
	float Sample(const FVector& Point) const;

	/** Like Sample, but also valid OUTSIDE the lattice box: the value at the nearest point of the box plus the distance to
	 *  the box (the field is 1-Lipschitz, so this is the natural continuation; exact when the nearest surface lies
	 *  straight across the box face). The lattice is only a few cm wider than the weapon, which is enough for fingers
	 *  but not for an arm lying beside the stock. Thread-safe (read only). */
	float SampleUnbounded(const FVector& Point) const;
};
