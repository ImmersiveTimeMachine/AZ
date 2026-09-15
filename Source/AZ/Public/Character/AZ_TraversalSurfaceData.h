// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AZ_TraversalSurfaceData.generated.h"

class UPrimitiveComponent;
struct FHitResult;

/** Where an explicitly enabled surface gets its ledge geometry, in the hit component's local space. */
UENUM(BlueprintType)
enum class EAZ_TraversalSurfaceGeometrySource : uint8
{
	MeshBounds,
	CustomBounds,
	AuthoredEdges
};

/** A top edge and its outward face normal. INDEX_NONE means this edge has no authored opposite side. */
USTRUCT(BlueprintType)
struct AZ_API FAZ_TraversalSurfaceLocalEdge
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	FVector Start = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	FVector End = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	FVector OutwardNormal = FVector::ForwardVector;

	/** When set, the other edge must point back to this edge with its own OppositeIndex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	int32 OppositeIndex = INDEX_NONE;
};

/**
 * Authored permission and geometry shared by traversal surfaces. Permission never replaces the pawn's
 * real height, support, landing or collision checks. Mesh bounds are an opt-in ledge approximation,
 * not a declaration that the mesh's entire render bounding box contains solid support.
 */
UCLASS(BlueprintType)
class AZ_API UAZ_TraversalSurfaceProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Actions")
	bool bAllowMantle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Actions")
	bool bAllowHurdle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Actions")
	bool bAllowClimb = true;

	/** Explicitly allow a high-ledge climb to finish clear of the far face and hand off to Falling.
	 *  Requires climb/crossing permission and a validated far-side floor; it does not imply top support. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Actions")
	bool bAllowClimbAndDrop = false;

	/** Whether the top can support the character or planted-foot contact. False excludes ordinary
	 *  mantle/climb and step-on hurdles. An explicit climb-and-drop can waive terminal standing support. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Semantics")
	bool bSupportsStanding = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Semantics")
	bool bSupportsCrossing = true;

	/** Descriptive surface metadata; these tags are not added to the traversing pawn's ASC. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Semantics")
	FGameplayTagContainer SurfaceTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Geometry")
	EAZ_TraversalSurfaceGeometrySource GeometrySource = EAZ_TraversalSurfaceGeometrySource::MeshBounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Geometry",
		meta = (EditCondition = "GeometrySource == EAZ_TraversalSurfaceGeometrySource::CustomBounds", EditConditionHides))
	FVector BoundsCenter = FVector::ZeroVector;

	/** Positive half extents, in local centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Geometry",
		meta = (EditCondition = "GeometrySource == EAZ_TraversalSurfaceGeometrySource::CustomBounds", EditConditionHides))
	FVector BoundsExtent = FVector(50.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Geometry",
		meta = (EditCondition = "GeometrySource == EAZ_TraversalSurfaceGeometrySource::AuthoredEdges", EditConditionHides))
	TArray<FAZ_TraversalSurfaceLocalEdge> AuthoredEdges;

	/** Extra WORLD-space clearance from an edge's endpoints, added to the pawn's own required clearance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal|Geometry", meta = (ClampMin = "0", ForceUnits = "cm"))
	float EdgeEndMargin = 5.f;
};

/** An explicit disabled or malformed override must never fall back to another surface provider. */
enum class EAZ_TraversalSurfaceResolveResult : uint8
{
	NotConfigured,
	Resolved,
	Disabled,
	Invalid
};

struct AZ_API FAZ_TraversalSurfaceWorldEdge
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	FVector OutwardNormal = FVector::ForwardVector;
	int32 OppositeIndex = INDEX_NONE;
};

/** Snapshot only: no collision exemptions, actor mutations, ability grants or gameplay tags are applied. */
struct AZ_API FAZ_TraversalSurfaceGeometry
{
	TArray<FAZ_TraversalSurfaceWorldEdge> Edges;
	FGameplayTagContainer SurfaceTags;
	bool bAllowMantle = true;
	bool bAllowHurdle = true;
	bool bAllowClimb = true;
	bool bAllowClimbAndDrop = false;
	bool bSupportsStanding = true;
	bool bSupportsCrossing = true;
	float EdgeEndMargin = 5.f;
	FString SourceDescription;
};

/**
 * Runtime asset-user-data opt-in on a primitive component or static-mesh asset. Component data takes
 * precedence, including disabled or invalid data. Existing placed actors and their references stay intact.
 */
UCLASS(BlueprintType, EditInlineNew)
class AZ_API UAZ_TraversalSurfaceData : public UAssetUserData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	TObjectPtr<UAZ_TraversalSurfaceProfile> Profile = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal")
	bool bEnabled = true;

	static EAZ_TraversalSurfaceResolveResult Resolve(
		const FHitResult& Hit, FAZ_TraversalSurfaceGeometry& OutGeometry, FString& OutReason);

	/** Read-only inspection of the exact runtime provider and world-space edges for this component. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Traversal")
	static FString DescribeSurface(UPrimitiveComponent* Component);
};
