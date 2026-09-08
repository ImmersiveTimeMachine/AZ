#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_CommonUI_ItemState.generated.h"

/** Storage and selection are independent: equipping does not release backpack cells. */
UENUM(BlueprintType)
enum class EAZ_InventoryItemLocation : uint8
{
	World,
	Backpack,
	WeaponMagazine
};

USTRUCT(BlueprintType)
struct AZ_API FAZ_InventoryItemState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Inventory")
	FGuid InstanceId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Inventory")
	EAZ_InventoryItemLocation Location = EAZ_InventoryItemLocation::World;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Inventory")
	FGuid ParentItemId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Inventory")
	FGuid InsertedMagazineId;
	/** -1 distinguishes non-magazine items from an empty magazine. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Inventory")
	int32 CurrentRounds = -1;
};

/** A stackable item may occupy several placements; each rifle/magazine occupies one. */
USTRUCT(BlueprintType)
struct AZ_API FAZ_InventoryGridPlacement
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Inventory")
	FGuid ItemId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Inventory")
	int32 GridIndex = INDEX_NONE;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Inventory")
	int32 StackCount = 1;
};
