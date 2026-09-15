// Copyright Artur. AZ project.

#include "Character/AZ_TraversalSurfaceData.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"

namespace
{
	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	bool IsFiniteRotation(const FQuat& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y)
			&& FMath::IsFinite(Value.Z) && FMath::IsFinite(Value.W) && Value.IsNormalized();
	}

	const TCHAR* ResolveResultName(EAZ_TraversalSurfaceResolveResult Result)
	{
		switch (Result)
		{
		case EAZ_TraversalSurfaceResolveResult::NotConfigured: return TEXT("NotConfigured");
		case EAZ_TraversalSurfaceResolveResult::Resolved:      return TEXT("Resolved");
		case EAZ_TraversalSurfaceResolveResult::Disabled:      return TEXT("Disabled");
		default:                                             return TEXT("Invalid");
		}
	}

	void AddBoxEdges(const FVector& Center, const FVector& Extent, TArray<FAZ_TraversalSurfaceLocalEdge>& OutEdges)
	{
		const FVector Min = Center - Extent;
		const FVector Max = Center + Extent;
		auto Add = [&OutEdges](const FVector& Start, const FVector& End, const FVector& Normal, int32 Opposite)
		{
			FAZ_TraversalSurfaceLocalEdge& Edge = OutEdges.AddDefaulted_GetRef();
			Edge.Start = Start;
			Edge.End = End;
			Edge.OutwardNormal = Normal;
			Edge.OppositeIndex = Opposite;
		};
		// Explicit reciprocal pairs. Normals, not winding, define outward even under mirrored XY scale.
		Add(FVector(Min.X, Min.Y, Max.Z), FVector(Min.X, Max.Y, Max.Z), FVector(-1.f, 0.f, 0.f), 1);
		Add(FVector(Max.X, Max.Y, Max.Z), FVector(Max.X, Min.Y, Max.Z), FVector(1.f, 0.f, 0.f), 0);
		Add(FVector(Max.X, Min.Y, Max.Z), FVector(Min.X, Min.Y, Max.Z), FVector(0.f, -1.f, 0.f), 3);
		Add(FVector(Min.X, Max.Y, Max.Z), FVector(Max.X, Max.Y, Max.Z), FVector(0.f, 1.f, 0.f), 2);
	}
}

EAZ_TraversalSurfaceResolveResult UAZ_TraversalSurfaceData::Resolve(
	const FHitResult& Hit, FAZ_TraversalSurfaceGeometry& OutGeometry, FString& OutReason)
{
	OutGeometry = FAZ_TraversalSurfaceGeometry();
	OutReason.Reset();
	UPrimitiveComponent* Component = Hit.GetComponent();
	if (!IsValid(Component))
	{
		OutReason = TEXT("No valid hit primitive component.");
		return EAZ_TraversalSurfaceResolveResult::NotConfigured;
	}

	const UAZ_TraversalSurfaceData* Data = Cast<UAZ_TraversalSurfaceData>(
		Component->GetAssetUserDataOfClass(StaticClass()));
	const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component);
	UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
	FString Provider = FString::Printf(TEXT("Component %s"), *Component->GetPathName());
	if (!Data && IsValid(Mesh))
	{
		Data = Cast<UAZ_TraversalSurfaceData>(Mesh->GetAssetUserDataOfClass(StaticClass()));
		Provider = FString::Printf(TEXT("StaticMesh %s on %s"), *Mesh->GetPathName(), *Component->GetPathName());
	}
	if (!Data)
	{
		OutReason = TEXT("No traversal surface data on the hit component or its static mesh.");
		return EAZ_TraversalSurfaceResolveResult::NotConfigured;
	}

	OutGeometry.SourceDescription = FString::Printf(TEXT("%s; profile=%s"), *Provider, *GetPathNameSafe(Data->Profile.Get()));
	if (!Data->bEnabled)
	{
		OutReason = TEXT("Traversal explicitly disabled by this surface override.");
		return EAZ_TraversalSurfaceResolveResult::Disabled;
	}
	const UAZ_TraversalSurfaceProfile* Profile = Data->Profile.Get();
	if (!IsValid(Profile))
	{
		OutReason = TEXT("Configured traversal surface has no valid profile.");
		return EAZ_TraversalSurfaceResolveResult::Invalid;
	}
	// HISM derives from ISM. Mesh and component user data can opt in, but the component transform alone
	// cannot describe a hit instance; refusing here is preferable to generating ledges at the wrong place.
	if (Component->IsA<UInstancedStaticMeshComponent>())
	{
		OutReason = TEXT("Configured ISM/HISM traversal is unsupported; per-instance hit transforms are required.");
		return EAZ_TraversalSurfaceResolveResult::Invalid;
	}

	const FTransform Transform = Component->GetComponentTransform();
	const FVector Scale = Transform.GetScale3D();
	if (!IsFiniteVector(Transform.GetTranslation()) || !IsFiniteVector(Scale)
		|| !IsFiniteRotation(Transform.GetRotation())
		|| FMath::Abs(Scale.X) <= UE_SMALL_NUMBER || FMath::Abs(Scale.Y) <= UE_SMALL_NUMBER
		|| FMath::Abs(Scale.Z) <= UE_SMALL_NUMBER)
	{
		OutReason = TEXT("Surface transform must be finite with a normalized rotation and nonzero scale.");
		return EAZ_TraversalSurfaceResolveResult::Invalid;
	}
	const FVector WorldUp = Transform.TransformVector(FVector::UpVector).GetSafeNormal();
	if (Scale.Z < 0.f || !WorldUp.Equals(FVector::UpVector, UE_KINDA_SMALL_NUMBER))
	{
		OutReason = TEXT("Traversal surfaces must be upright; tilted or inverted-Z transforms are unsupported.");
		return EAZ_TraversalSurfaceResolveResult::Invalid;
	}
	if (!FMath::IsFinite(Profile->EdgeEndMargin) || Profile->EdgeEndMargin < 0.f)
	{
		OutReason = TEXT("EdgeEndMargin must be a finite, nonnegative world-space distance.");
		return EAZ_TraversalSurfaceResolveResult::Invalid;
	}

	TArray<FAZ_TraversalSurfaceLocalEdge> LocalEdges;
	if (Profile->GeometrySource == EAZ_TraversalSurfaceGeometrySource::AuthoredEdges)
	{
		LocalEdges = Profile->AuthoredEdges;
	}
	else
	{
		FVector Center;
		FVector Extent;
		switch (Profile->GeometrySource)
		{
		case EAZ_TraversalSurfaceGeometrySource::MeshBounds:
			if (!IsValid(Mesh))
			{
				OutReason = TEXT("MeshBounds geometry requires a valid static mesh on the hit component.");
				return EAZ_TraversalSurfaceResolveResult::Invalid;
			}
			{
				const FBox Bounds = Mesh->GetBoundingBox();
				if (!Bounds.IsValid)
				{
					OutReason = TEXT("Static mesh has no valid local bounds.");
					return EAZ_TraversalSurfaceResolveResult::Invalid;
				}
				Center = Bounds.GetCenter();
				Extent = Bounds.GetExtent();
			}
			break;
		case EAZ_TraversalSurfaceGeometrySource::CustomBounds:
			Center = Profile->BoundsCenter;
			Extent = Profile->BoundsExtent;
			break;
		default:
			OutReason = TEXT("Unrecognized traversal geometry source.");
			return EAZ_TraversalSurfaceResolveResult::Invalid;
		}
		if (!IsFiniteVector(Center) || !IsFiniteVector(Extent)
			|| Extent.X <= UE_SMALL_NUMBER || Extent.Y <= UE_SMALL_NUMBER || Extent.Z <= UE_SMALL_NUMBER)
		{
			OutReason = TEXT("Surface bounds need a finite center and finite, positive half extents on all axes.");
			return EAZ_TraversalSurfaceResolveResult::Invalid;
		}
		AddBoxEdges(Center, Extent, LocalEdges);
	}
	if (LocalEdges.IsEmpty())
	{
		OutReason = TEXT("Configured traversal surface has no authored edges.");
		return EAZ_TraversalSurfaceResolveResult::Invalid;
	}

	TArray<FAZ_TraversalSurfaceWorldEdge> WorldEdges;
	WorldEdges.Reserve(LocalEdges.Num());
	for (int32 Index = 0; Index < LocalEdges.Num(); ++Index)
	{
		const FAZ_TraversalSurfaceLocalEdge& Local = LocalEdges[Index];
		if (!IsFiniteVector(Local.Start) || !IsFiniteVector(Local.End) || !IsFiniteVector(Local.OutwardNormal)
			|| Local.Start.Equals(Local.End, UE_SMALL_NUMBER) || Local.OutwardNormal.IsNearlyZero())
		{
			OutReason = FString::Printf(TEXT("Edge %d needs finite endpoints, nonzero length and a finite nonzero outward normal."), Index);
			return EAZ_TraversalSurfaceResolveResult::Invalid;
		}
		if (Local.OppositeIndex != INDEX_NONE
			&& (!LocalEdges.IsValidIndex(Local.OppositeIndex) || Local.OppositeIndex == Index
				|| LocalEdges[Local.OppositeIndex].OppositeIndex != Index))
		{
			OutReason = FString::Printf(TEXT("Edge %d has invalid or non-reciprocal OppositeIndex %d."), Index, Local.OppositeIndex);
			return EAZ_TraversalSurfaceResolveResult::Invalid;
		}

		FAZ_TraversalSurfaceWorldEdge Edge;
		Edge.Start = Transform.TransformPosition(Local.Start);
		Edge.End = Transform.TransformPosition(Local.End);
		Edge.OppositeIndex = Local.OppositeIndex;
		// A normal transforms by inverse-transpose, not like a point or tangent. Division by signed
		// scale preserves the correct outward side for nonuniform and mirrored XY component scales.
		const FVector InverseScaledNormal(Local.OutwardNormal.X / Scale.X,
			Local.OutwardNormal.Y / Scale.Y, Local.OutwardNormal.Z / Scale.Z);
		Edge.OutwardNormal = Transform.GetRotation().RotateVector(InverseScaledNormal);
		if (!IsFiniteVector(Edge.Start) || !IsFiniteVector(Edge.End) || !IsFiniteVector(Edge.OutwardNormal)
			|| !FMath::IsFinite((Edge.End - Edge.Start).SizeSquared())
			|| Edge.Start.Equals(Edge.End, UE_SMALL_NUMBER) || !Edge.OutwardNormal.Normalize()
			|| !Edge.OutwardNormal.IsNormalized()
			|| FMath::Abs(Edge.OutwardNormal.Z) > UE_KINDA_SMALL_NUMBER)
		{
			OutReason = FString::Printf(TEXT("Edge %d must have finite world geometry, nonzero length and a horizontal outward normal."), Index);
			return EAZ_TraversalSurfaceResolveResult::Invalid;
		}
		const FVector EdgeDelta = Edge.End - Edge.Start;
		if (EdgeDelta.SizeSquared2D() <= UE_SMALL_NUMBER
			|| FMath::Abs(FVector::DotProduct(EdgeDelta.GetSafeNormal(), Edge.OutwardNormal)) > 0.01f)
		{
			OutReason = FString::Printf(TEXT("Edge %d needs nonzero planar length and an outward normal perpendicular to its tangent."), Index);
			return EAZ_TraversalSurfaceResolveResult::Invalid;
		}
		WorldEdges.Add(Edge);
	}

	OutGeometry.Edges = MoveTemp(WorldEdges);
	OutGeometry.SurfaceTags = Profile->SurfaceTags;
	OutGeometry.bAllowMantle = Profile->bAllowMantle;
	OutGeometry.bAllowHurdle = Profile->bAllowHurdle;
	OutGeometry.bAllowClimb = Profile->bAllowClimb;
	OutGeometry.bAllowClimbAndDrop = Profile->bAllowClimbAndDrop;
	OutGeometry.bSupportsStanding = Profile->bSupportsStanding;
	OutGeometry.bSupportsCrossing = Profile->bSupportsCrossing;
	OutGeometry.EdgeEndMargin = Profile->EdgeEndMargin;
	OutReason = TEXT("Explicit traversal surface resolved; physical support and collision checks remain required.");
	return EAZ_TraversalSurfaceResolveResult::Resolved;
}

FString UAZ_TraversalSurfaceData::DescribeSurface(UPrimitiveComponent* Component)
{
	FHitResult Hit;
	Hit.Component = Component;
	FAZ_TraversalSurfaceGeometry Geometry;
	FString Reason;
	const EAZ_TraversalSurfaceResolveResult Result = Resolve(Hit, Geometry, Reason);
	FString Description = FString::Printf(TEXT("result=%s\nsource=%s\nreason=%s"),
		ResolveResultName(Result), *Geometry.SourceDescription, *Reason);
	if (Result != EAZ_TraversalSurfaceResolveResult::Resolved)
	{
		return Description;
	}
	Description += FString::Printf(TEXT("\nactions: mantle=%d hurdle=%d climb=%d climbAndDrop=%d; standing=%d crossing=%d\nmargin=%.3fcm; tags=%s; edges=%d"),
		Geometry.bAllowMantle, Geometry.bAllowHurdle, Geometry.bAllowClimb,
		Geometry.bAllowClimbAndDrop,
		Geometry.bSupportsStanding, Geometry.bSupportsCrossing, Geometry.EdgeEndMargin,
		*Geometry.SurfaceTags.ToStringSimple(), Geometry.Edges.Num());
	for (int32 Index = 0; Index < Geometry.Edges.Num(); ++Index)
	{
		const FAZ_TraversalSurfaceWorldEdge& Edge = Geometry.Edges[Index];
		Description += FString::Printf(TEXT("\nedge[%d]: start=%s end=%s outward=%s opposite=%d"),
			Index, *Edge.Start.ToString(), *Edge.End.ToString(), *Edge.OutwardNormal.ToString(), Edge.OppositeIndex);
	}
	return Description;
}
