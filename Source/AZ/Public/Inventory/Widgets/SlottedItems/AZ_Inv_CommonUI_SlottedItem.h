// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
// Assuming the path to your project's common button base class
#include "Inventory/CommonUI/AZ_Inv_CommonUI_Button.h"
#include "AZ_Inv_CommonUI_SlottedItem.generated.h"

class UImage;
class UTextBlock;
class UAZ_Inv_CommonUI_InventoryItem;

/**
 * A widget representing an item placed in the inventory grid.
 * It's a button to allow for clicking, dragging, etc. and is managed by the Inventory Grid.
 */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_SlottedItem : public UAZ_Inv_CommonUI_Button
{
	GENERATED_BODY()

public:
	// Called by the inventory grid to set up the item's visual representation.
	void SetInventoryItem(TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> InInventoryItem);

	void SetImageBrush(const FSlateBrush& Brush);
	
	void SetIsStackable(bool bInIsStackable) { bIsStackable = bInIsStackable; }

	// Updates the stack count text. Hides it if count <= 1.
	void UpdateStackCount(int32 InStackCount);

	// Getters
	int32 GetGridIndex() const
	{
		return GridIndex;
	}

	FIntPoint GetGridDimensions() const
	{
		return GridDimensions;
	}

	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> GetInventoryItem() const
	{
		return InventoryItem;
	}

	// Setters
	void SetGridIndex(int32 InGridIndex)
	{
		GridIndex = InGridIndex;
	}

	void SetGridDimensions(const FIntPoint& InGridDimensions)
	{
		GridDimensions = InGridDimensions;
	}

	/** Expose the protected base-class delegates for the grid to bind to */
	FCommonButtonBaseClicked& OnItemClicked()
	{
		return OnButtonBaseClicked;
	}

	FCommonButtonBaseClicked& OnItemHovered()
	{
		return OnButtonBaseHovered;
	}

	FCommonButtonBaseClicked& OnItemUnhovered()
	{
		return OnButtonBaseUnhovered;
	}

	FCommonButtonBaseClicked& OnItemRightClicked()
	{
		return OnButtonBaseRightClicked;
	}

protected:
	// Bindings to widgets in the UMG Designer, ported from the old UAZ_Inv_SlottedItem
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> Image_Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_StackCount;

private:
	// Data about the item this widget represents
	int32 GridIndex = INDEX_NONE;
	FIntPoint GridDimensions = FIntPoint(1, 1);
	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> InventoryItem;
	bool bIsStackable{false};
};
