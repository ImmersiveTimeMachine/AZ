// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_GridSlot.h"
#include "GameplayTagContainer.h"
#include "AZ_Inv_EquippedGridSlot.generated.h"


class UOverlay;
class UAZ_Inv_EquippedSlottedItem;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEquippedGridSlotClicked, UAZ_Inv_EquippedGridSlot*, GridSlot, const FGameplayTag&, EquipmentTypeTag);


UCLASS()
class AZ_API UAZ_Inv_EquippedGridSlot : public UAZ_Inv_GridSlot
{
	GENERATED_BODY()
	
public:
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UAZ_Inv_EquippedSlottedItem* OnItemEquipped(UAZ_Inv_InventoryItem* Item, const FGameplayTag& EquipmentTag, float TileSize);
	void SetEquippedSlottedItem(UAZ_Inv_EquippedSlottedItem* Item) { EquippedSlottedItem = Item; }

	FEquippedGridSlotClicked EquippedGridSlotClicked;

private:
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory", meta = (Categories = "GameItems.Equipment"))
	FGameplayTag EquipmentTypeTag;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_GrayedOutIcon;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TSubclassOf<UAZ_Inv_EquippedSlottedItem> EquippedSlottedItemClass;

	UPROPERTY()
	TObjectPtr<UAZ_Inv_EquippedSlottedItem> EquippedSlottedItem;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> Overlay_Root;
};
