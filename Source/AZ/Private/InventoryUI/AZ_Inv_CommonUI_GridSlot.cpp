// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/AZ_Inv_CommonUI_GridSlot.h"

#include "InventoryUI/Widgets/ItemPopUp/AZ_Inv_CommonUI_ItemPopUp.h"

void UAZ_Inv_CommonUI_GridSlot::NativePreConstruct()
{
	Super::NativePreConstruct();
	// Occupancy and placement feedback own this visual state. CommonUI must not
	// toggle it again after the inventory's click callback has rebuilt the grid.
	SetIsSelectable(false);
	SetIsToggleable(false);
	SetIsInteractableWhenSelected(true);
	SetShouldSelectUponReceivingFocus(false);
}

void UAZ_Inv_CommonUI_GridSlot::NativeOnClicked()
{
	Super::NativeOnClicked();
}

void UAZ_Inv_CommonUI_GridSlot::SetOccupiedTexture()
{
	if (!GetSelected()) SetSelectedInternal(true, false);
}

void UAZ_Inv_CommonUI_GridSlot::SetUnoccupiedTexture()
{
	if (GetSelected()) SetSelectedInternal(false, false);
}

void UAZ_Inv_CommonUI_GridSlot::SetSelectedTexture()
{
	if (!GetSelected()) SetSelectedInternal(true, false);
}

void UAZ_Inv_CommonUI_GridSlot::SetGrayedOutTexture()
{
	if (!GetSelected()) SetSelectedInternal(true, false);
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
