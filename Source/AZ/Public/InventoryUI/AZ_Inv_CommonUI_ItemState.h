#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_CommonUI_ItemState.generated.h"

/** Supported trigger behavior for inventory-backed firearms. */
UENUM(BlueprintType)
enum class EAZ_FirearmFireMode : uint8
{
	Single,
	Automatic
};

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
	/** Magazine mutation revision. Preserved with its identity through world transfers. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Inventory")
	int64 AmmoRevision = 0;
	/** Per-weapon selection, preserved with its identity through equipment/world transfers. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Inventory|Firearm")
	EAZ_FirearmFireMode SelectedFireMode = EAZ_FirearmFireMode::Single;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Inventory|Firearm")
	int64 FireModeRevision = 0;
};

/** Unavailable also covers a linked magazine whose replicated item has not arrived yet. */
UENUM(BlueprintType)
enum class EAZ_WeaponMagazineState : uint8
{
	Unavailable,
	NoMagazine,
	Empty,
	Loaded
};

/** Derived from owned items; this is a readout/shot receipt, never another ammo store. */
USTRUCT(BlueprintType)
struct AZ_API FAZ_WeaponAmmoSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Inventory|Ammo")
	FGuid WeaponItemId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Inventory|Ammo")
	FGuid MagazineItemId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Inventory|Ammo")
	EAZ_WeaponMagazineState MagazineState = EAZ_WeaponMagazineState::Unavailable;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Inventory|Ammo")
	int32 Rounds = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Inventory|Ammo")
	int32 Capacity = 0;
    /** Compatible backpack magazines containing rounds; excludes inserted and empty magazines. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Inventory|Ammo")
	int32 SpareMagazineCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Inventory|Ammo")
	int64 AmmoRevision = -1;
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
