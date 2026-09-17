// Copyright Artur. AZ project.

#include "Throwables/AZ_ThrowableDefinition.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"

namespace
{
	/** Uniform scale bringing a mesh's longest axis to DesiredCm. 0 or a degenerate mesh means "as authored". */
	FVector ScaleToLongestAxis(const FBoxSphereBounds& Bounds, const float DesiredCm)
	{
		const double Longest = Bounds.BoxExtent.GetAbsMax() * 2.0;
		return (DesiredCm > 0.f && Longest > KINDA_SMALL_NUMBER)
			? FVector(DesiredCm / Longest) : FVector::OneVector;
	}
}

FVector UAZ_ThrowableDefinition::GetHeldSkeletalMeshScale() const
{
	return HeldSkeletalMesh ? ScaleToLongestAxis(HeldSkeletalMesh->GetBounds(), HeldMeshSize) : FVector::OneVector;
}

FVector UAZ_ThrowableDefinition::GetHeldMeshScale() const
{
	// A scale is meaningless without the mesh it scales. Converting through the bounds is what lets the
	// size be authored in centimetres and stay right when the art is swapped — and it is what keeps the
	// visible object the same size as the CollisionRadius the world actually hits.
	return HeldMesh ? ScaleToLongestAxis(HeldMesh->GetBounds(), HeldMeshSize) : FVector::OneVector;
}
