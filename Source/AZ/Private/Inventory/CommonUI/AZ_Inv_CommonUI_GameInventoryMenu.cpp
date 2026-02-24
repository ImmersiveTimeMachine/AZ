// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/CommonUI/AZ_Inv_CommonUI_GameInventoryMenu.h"

FAZ_Inv_CommonUI_SlotAvailabilityResult UAZ_Inv_CommonUI_GameInventoryMenu::HasRoomForItem(UAZ_Inv_CommonUI_ItemComponent* ItemComponent) const
{
	return FAZ_Inv_CommonUI_SlotAvailabilityResult();
}

void UAZ_Inv_CommonUI_GameInventoryMenu::OnItemHovered(UAZ_Inv_CommonUI_InventoryItem* Item)
{
}

void UAZ_Inv_CommonUI_GameInventoryMenu::OnItemUnHovered()
{
}

bool UAZ_Inv_CommonUI_GameInventoryMenu::HasHoverItem() const
{
	return false;
}

UAZ_Inv_CommonUI_HoverItem* UAZ_Inv_CommonUI_GameInventoryMenu::GetHoverItem() const
{
	return nullptr;
}

float UAZ_Inv_CommonUI_GameInventoryMenu::GetTileSize() const
{
	return 0.f;
}
