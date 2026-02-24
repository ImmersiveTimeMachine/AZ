// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Inventory/Types/AZ_Inv_GridTypes.h"
#include "AZ_Inv_CommonUI_GameInventoryMenu.generated.h"

class UAZ_Inv_CommonUI_ItemComponent;
class UAZ_Inv_CommonUI_InventoryItem;
class UAZ_Inv_CommonUI_HoverItem;

/**
 * CommonUI inventory menu widget.
 * Widget layout and references are configured in the Blueprint (AZ_WBP_GameInventoryMenu).
 */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_GameInventoryMenu : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:

	FAZ_Inv_CommonUI_SlotAvailabilityResult HasRoomForItem(UAZ_Inv_CommonUI_ItemComponent* ItemComponent) const;
	void OnItemHovered(UAZ_Inv_CommonUI_InventoryItem* Item);
	void OnItemUnHovered();
	bool HasHoverItem() const;
	UAZ_Inv_CommonUI_HoverItem* GetHoverItem() const;
	float GetTileSize() const;
};
