// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/Items/AZ_Inv_InventoryItem.h"
#include "AZ_Inv_SlottedItem.generated.h"

class UTextBlock;
class UImage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAZ_SlottedItemClicked, int32, GridIndex, const FPointerEvent&, MouseEvent);

UCLASS()
class AZ_API UAZ_Inv_SlottedItem : public UUserWidget
{
	GENERATED_BODY()
	
public:	
	
	FAZ_SlottedItemClicked OnSlottedItemClickedEvent;
	
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	

	// Getters and Setters
	UImage* GetImageIcon() const { return Image_Icon; }
	void SetImageIcon(UImage* InImage) { Image_Icon = InImage; }
	int32 GetGridIndex() const { return GridIndex; }
	void SetGridIndex(int32 InGridIndex) { GridIndex = InGridIndex; }
	FIntPoint GetGridDimensions() const { return GridDimensions; }
	void SetGridDimensions(const FIntPoint& InGridDimensions) { GridDimensions = InGridDimensions; }
	UAZ_Inv_InventoryItem* GetInventoryItem() const { return InventoryItem.Get(); }
	void SetInventoryItem(UAZ_Inv_InventoryItem* InInventoryItem) { InventoryItem = InInventoryItem; }
	bool IsStackable() const { return bIsStackable; }
	void SetIsStackable(bool bInIsStackable) { bIsStackable = bInIsStackable; }
	void SetImageBrush(const FSlateBrush& Brush) const;
// END Getters and Setters

	void UpdateStackCount(int32 InStackCount);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Icon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_StackCount;

	int32 GridIndex;
	FIntPoint GridDimensions;
	TWeakObjectPtr<UAZ_Inv_InventoryItem> InventoryItem;
	bool bIsStackable{false};


	
};
