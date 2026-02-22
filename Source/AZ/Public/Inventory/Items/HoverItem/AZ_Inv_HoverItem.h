// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "AZ_Inv_HoverItem.generated.h"

/**
 * The HoverItem is the item that will appear and follow the mouse
 * when an inventory item on the grid has been clicked.
 */

class UAZ_Inv_InventoryItem;
class UTextBlock;
class UImage;

UCLASS()
class AZ_API UAZ_Inv_HoverItem : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetImageBrush(const FSlateBrush& Brush) const;
	void UpdateStackCount(const int32 Count);

	FGameplayTag GetItemType() const;
	int32 GetStackCount() const { return StackCount; }
	bool IsStackable() const { return bIsStackable; }
	void SetIsStackable(bool bStacks);
	int32 GetPreviousGridIndex() const { return PreviousGridIndex; }
	void SetPreviousGridIndex(int32 Index) { PreviousGridIndex = Index; }
	FIntPoint GetGridDimensions() const { return GridDimensions; }
	void SetGridDimensions(const FIntPoint& Dimensions) { GridDimensions = Dimensions; }
	UAZ_Inv_InventoryItem* GetInventoryItem() const;
	void SetInventoryItem(UAZ_Inv_InventoryItem* Item);
	
private:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Icon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_StackCount;

	int32 PreviousGridIndex;
	FIntPoint GridDimensions;
	TWeakObjectPtr<UAZ_Inv_InventoryItem> InventoryItem;
	bool bIsStackable{false};
	int32 StackCount{0};
};
