// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Widgets/SlottedItems/AZ_Inv_EquippedSlottedItem.h"

FReply UAZ_Inv_EquippedSlottedItem::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	OnEquippedSlottedItemClicked.Broadcast(this);
	return FReply::Handled();
}
