// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryOld/Widgets/Components/AZ_Inv_GridSlot.h"

#include "Items/AZ_Inv_InventoryItem.h"
#include "InventoryOld/Widgets/ItemPopUp/AZ_Inv_ItemPopUp.h"

TWeakObjectPtr<UAZ_Inv_InventoryItem> UAZ_Inv_GridSlot::GetInventoryItem() const
{
	return InventoryItem;
}

void UAZ_Inv_GridSlot::SetInventoryItem(TWeakObjectPtr<UAZ_Inv_InventoryItem> InInventoryItem)
{
	InventoryItem = InInventoryItem;
}

void UAZ_Inv_GridSlot::NativeOnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	Super::NativeOnMouseEnter(MyGeometry, MouseEvent);
	GridSlotHovered.Broadcast(TileIndex, MouseEvent);
}

void UAZ_Inv_GridSlot::NativeOnMouseLeave(const FPointerEvent& MouseEvent)
{
	Super::NativeOnMouseLeave(MouseEvent);
	GridSlotUnhovered.Broadcast(TileIndex, MouseEvent);
}

FReply UAZ_Inv_GridSlot::NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	GridSlotClicked.Broadcast(TileIndex, MouseEvent);
	return FReply::Handled();
}

void UAZ_Inv_GridSlot::SetOccupiedTexture()
{
	GridSlotState = EInv_GridSlotState::Occupied;
	Image_GridSlot->SetBrush(OccupiedBrush);
}

void UAZ_Inv_GridSlot::SetUnoccupiedTexture()
{
	GridSlotState = EInv_GridSlotState::Unoccupied;
	Image_GridSlot->SetBrush(UnoccupiedBrush);
}

void UAZ_Inv_GridSlot::SetSelectedTexture()
{
	GridSlotState = EInv_GridSlotState::Selected;
	Image_GridSlot->SetBrush(SelectedBrush);
}

void UAZ_Inv_GridSlot::SetGrayedOutTexture()
{
	GridSlotState = EInv_GridSlotState::GrayedOut;
	Image_GridSlot->SetBrush(GrayedOutBrush);
}

UAZ_Inv_ItemPopUp* UAZ_Inv_GridSlot::GetItemPopUp() const
{
	return ItemPopUp.Get();
}

void UAZ_Inv_GridSlot::SetItemPopUp(UAZ_Inv_ItemPopUp* PopUp)
{
	ItemPopUp = PopUp;
	ItemPopUp->SetGridIndex(GetIndex());
	ItemPopUp->OnNativeDestruct.AddUObject(this, &ThisClass::OnItemPopUpDestruct);
}

void UAZ_Inv_GridSlot::OnItemPopUpDestruct(UUserWidget* Menu)
{
	ItemPopUp.Reset();
}
