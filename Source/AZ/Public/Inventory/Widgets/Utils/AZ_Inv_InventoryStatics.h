// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_WidgetUtils.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "Inventory/Items/AZ_Inv_ItemComponent.h"
#include "Inventory/Items/HoverItem/AZ_Inv_HoverItem.h"
#include "Inventory/Types/AZ_Inv_GridTypes.h"
#include "AZ_Inv_InventoryStatics.generated.h"

class UAZ_Inv_CommonUI_InventoryComponent;
class UAZ_Inv_InventoryComponent;
class UAZ_Inv_CommonUI_HoverItem;
class UAZ_Inv_CommonUI_InventoryItem;
class UAZ_Inv_CommonUI_GameInventoryMenu;

/**
 * Static helper library for inventory-related operations.
 * Includes accessors, category ↔ gameplay tag mapping, and conversion utilities.
 */
UCLASS()
class AZ_API UAZ_Inv_InventoryStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Returns the inventory component from a given PlayerController. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Inventory")
	static UAZ_Inv_InventoryComponent* GetInventoryComponent(const APlayerController* PlayerController);
	
	/** Returns the inventory component from a given PlayerController. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Inventory")
	static UAZ_Inv_CommonUI_InventoryComponent* Get_CommonUI_InventoryComponent(const APlayerController* PlayerController);

	// -------------------------------------------------------------------------
	// 🔹 Category ↔ Gameplay Tag Conversion
	// -------------------------------------------------------------------------

	/** Returns the root gameplay tag associated with the given item category. */
	UFUNCTION(BlueprintPure, Category = "AZ|Inventory")
	static FGameplayTag GetCategoryTag(EInv_ItemCategory Category);

	/** Returns the category enum value based on a given item gameplay tag. */
	UFUNCTION(BlueprintPure, Category = "AZ|Inventory|Category")
	static EInv_ItemCategory GetCategoryFromTag(const FGameplayTag& ItemTag);

	UFUNCTION(BlueprintPure, Category = "AZ|Inventory")
	static EInv_ItemCategory GetItemCategoryFromItemComp(const UAZ_Inv_ItemComponent* ItemComponent);
	static void ItemHovered(APlayerController* PC, UAZ_Inv_InventoryItem* Item);
	static void ItemUnhovered(APlayerController* PC);
	static UAZ_Inv_HoverItem* GetHoverItem(APlayerController* PC);

	template<typename T, typename FuncT>
	static void ForEach2D(TArray<T>& Array, int32 Index, const FIntPoint& Range2D, int32 GridColumns, const FuncT& Function);
	static UAZ_Inv_InventoryMenuBase* GetInventoryWidget(const APlayerController* PlayerController);

	static void CommonUI_ItemHovered(APlayerController* PC, UAZ_Inv_CommonUI_InventoryItem* Item);
	static void CommonUI_ItemUnhovered(APlayerController* PC);
	static UAZ_Inv_CommonUI_HoverItem* CommonUI_GetHoverItem(APlayerController* PC);
	static UAZ_Inv_CommonUI_GameInventoryMenu* CommonUI_GetInventoryWidget(const APlayerController* PlayerController);
};

template<typename T, typename FuncT>
void UAZ_Inv_InventoryStatics::ForEach2D(TArray<T>& Array, int32 Index, const FIntPoint& Range2D, int32 GridColumns, const FuncT& Function)
{
	// Validate input parameters
	if (GridColumns <= 0 || Range2D.X < 0 || Range2D.Y < 0 || Index < 0)
	{
		return;
	}

	// Calculate base position once
	const FIntPoint BasePosition = UAZ_Inv_WidgetUtils::GetPositionFromIndex(Index, GridColumns);
	
	for (int32 j = 0; j < Range2D.Y; ++j)
	{
		for (int32 i = 0; i < Range2D.X; ++i)
		{
			const FIntPoint Coordinates = BasePosition + FIntPoint(i, j);
			const int32 TileIndex = UAZ_Inv_WidgetUtils::GetIndexFromPosition(Coordinates, GridColumns);
			if (Array.IsValidIndex(TileIndex))
			{
				Function(Array[TileIndex]);
			}
		}
	}
}
