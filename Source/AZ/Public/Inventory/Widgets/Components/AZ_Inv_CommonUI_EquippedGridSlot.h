// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/CommonUI/AZ_Inv_CommonUI_GridSlot.h"
#include "GameplayTagContainer.h"
#include "AZ_Inv_CommonUI_EquippedGridSlot.generated.h"

class UOverlay;
class UImage;
class UAZ_Inv_CommonUI_EquippedSlottedItem;
class UAZ_Inv_CommonUI_InventoryItem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCommonUI_EquippedGridSlotClicked, UAZ_Inv_CommonUI_EquippedGridSlot*, GridSlot, const FGameplayTag&, EquipmentTypeTag);

UCLASS()
class AZ_API UAZ_Inv_CommonUI_EquippedGridSlot : public UAZ_Inv_CommonUI_GridSlot
{
	GENERATED_BODY()

public:

	UAZ_Inv_CommonUI_EquippedSlottedItem* OnItemEquipped(UAZ_Inv_CommonUI_InventoryItem* Item, const FGameplayTag& EquipmentTag, float TileSize);
	void SetEquippedSlottedItem(UAZ_Inv_CommonUI_EquippedSlottedItem* Item) { EquippedSlottedItem = Item; }

	FCommonUI_EquippedGridSlotClicked EquippedGridSlotClicked;

private:

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory", meta = (Categories = "GameItems.Equipment"))
	FGameplayTag EquipmentTypeTag;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_GrayedOutIcon;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TSubclassOf<UAZ_Inv_CommonUI_EquippedSlottedItem> EquippedSlottedItemClass;

	UPROPERTY()
	TObjectPtr<UAZ_Inv_CommonUI_EquippedSlottedItem> EquippedSlottedItem;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> Overlay_Root;

	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void NativeOnClicked() override;
};
