// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/CommonUI/AZ_Inv_CommonUI_GridSlot.h"

#include "Inventory/Widgets/ItemPopUp/AZ_Inv_CommonUI_ItemPopUp.h"


void UAZ_Inv_CommonUI_GridSlot::SetOccupiedTexture()
{
}

void UAZ_Inv_CommonUI_GridSlot::SetUnoccupiedTexture()
{
}

void UAZ_Inv_CommonUI_GridSlot::SetSelectedTexture()
{
}

void UAZ_Inv_CommonUI_GridSlot::SetGrayedOutTexture()
{
}

UAZ_Inv_CommonUI_ItemPopUp* UAZ_Inv_CommonUI_GridSlot::GetItemPopUp() const
{
	return ItemPopUp.Get();
}

void UAZ_Inv_CommonUI_GridSlot::SetItemPopUp(UAZ_Inv_CommonUI_ItemPopUp* PopUp)
{
	ItemPopUp = PopUp;
	//ItemPopUp->OnNativeDestruct.AddUObject(this, &ThisClass::OnItemPopUpDestruct);
}

void UAZ_Inv_CommonUI_GridSlot::OnItemPopUpDestruct(UCommonUserWidget* Menu)
{
	ItemPopUp.Reset();
}

TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> UAZ_Inv_CommonUI_GridSlot::GetInventoryItem() const
{
	return InventoryItem;
}

void UAZ_Inv_CommonUI_GridSlot::SetInventoryItem(const TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> InInventoryItem)
{
	InventoryItem = InInventoryItem;
}
