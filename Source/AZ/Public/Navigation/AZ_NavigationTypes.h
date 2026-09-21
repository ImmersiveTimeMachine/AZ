#pragma once

#include "CoreMinimal.h"
#include "AZ_NavigationTypes.generated.h"

/** Persistent navigation identity/data. Loaded actor/component references belong to the world registry. */
USTRUCT(BlueprintType)
struct AZ_API FAZ_NavigationTargetDescriptor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Navigation")
	FName TargetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Navigation")
	FName MapId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Navigation")
	FName LayerId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Navigation")
	bool bHasWorldLocation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Navigation")
	FVector WorldLocation = FVector::ZeroVector;

	/** Centimetres. Zero denotes an exact destination, positive values an authored search area. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Navigation", meta=(ClampMin="0.0", Units="cm"))
	float SearchRadius = 0.0f;

	bool IsWellFormed() const;
};

/** One personal waypoint, independent of quest tracking. World Z is explicitly supplied by its map/layer policy. */
USTRUCT(BlueprintType)
struct AZ_API FAZ_MapWaypoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Navigation")
	bool bActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Navigation")
	FName MapId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Navigation")
	FName LayerId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Navigation")
	FVector WorldLocation = FVector::ZeroVector;

	bool IsWellFormed() const;
};

UENUM(BlueprintType)
enum class EAZ_NavigationTargetResolveResult : uint8
{
	ResolvedProvider,
	ResolvedLocation,
	Missing,
	Ambiguous,
	Invalid,
	ContextMismatch
};
