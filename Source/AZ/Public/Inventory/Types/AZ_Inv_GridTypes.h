// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "AZ_Inv_GridTypes.generated.h"

class UAZ_Inv_CommonUI_InventoryItem;
class UAZ_Inv_InventoryItem;

UENUM(BlueprintType)
enum class EInv_ItemCategory : uint8
{
	Equippable,
	Consumable,
	Craftable,
	None,
	Max
};

USTRUCT()
struct FInv_SlotAvailability
{
	GENERATED_BODY()

	FInv_SlotAvailability() {};

	FInv_SlotAvailability(const int32 InIndex, const int32 InAmountToFill, const bool bInItemAtIndex)
		:	Index(InIndex)
		  , AmountToFill(InAmountToFill)
		  , bItemAtIndex(bInItemAtIndex)
	{
	}

	int32 Index{INDEX_NONE};
	int32 AmountToFill{0};
	bool bItemAtIndex{false};
};

USTRUCT()
struct FAZ_Inv_SlotAvailabilityResult
{
	GENERATED_BODY()

	FAZ_Inv_SlotAvailabilityResult() {};

	TWeakObjectPtr<UAZ_Inv_InventoryItem> Item;
	int32 TotalRoomToFill{0};
	int32 RemainingRooms{0};
	bool bIsStackable{false};
	TArray<FInv_SlotAvailability> AvailableSlots;
};

USTRUCT()
struct FAZ_Inv_CommonUI_SlotAvailabilityResult
{
	GENERATED_BODY()

	FAZ_Inv_CommonUI_SlotAvailabilityResult() {};

	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> Item;
	int32 TotalRoomToFill{0};
	int32 RemainingRooms{0};
	bool bIsStackable{false};
	TArray<FInv_SlotAvailability> AvailableSlots;
};

UENUM(BlueprintType)
enum class EAZ_Inv_TileQuadrant : uint8
{
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight,
	None
};

USTRUCT(BlueprintType)
struct FAZ_Inv_TileParameters
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AZ|Inventory")
	FIntPoint TileCoordinats{};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AZ|Inventory")
	int32 TileIndex{INDEX_NONE};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AZ|Inventory")
	EAZ_Inv_TileQuadrant TileQuadrant{EAZ_Inv_TileQuadrant::None};
};

inline bool operator==(const FAZ_Inv_TileParameters& A, const FAZ_Inv_TileParameters& B)
{
	return A.TileCoordinats == B.TileCoordinats && A.TileIndex == B.TileIndex && A.TileQuadrant == B.TileQuadrant;
}

USTRUCT()
struct FAZ_Inv_SpaceQueryResult
{
	GENERATED_BODY()

	// True if the space queried has no items in it
	bool bHasSpace{false};

	// Valid if there's a single item we can swap with
	TWeakObjectPtr<UAZ_Inv_InventoryItem> ValidItem = nullptr;

	// Upper left index of the valid item, if there is one
	int32 UpperLeftIndex{INDEX_NONE};
};

USTRUCT()
struct FAZ_Inv_CommonUI_SpaceQueryResult
{
	GENERATED_BODY()

	bool bHasSpace{false};
	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> ValidItem = nullptr;
	int32 UpperLeftIndex{INDEX_NONE};
};