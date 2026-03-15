// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/Widgets/SlottedItems/AZ_Inv_CommonUI_EquippedSlottedItem.h"

void UAZ_Inv_CommonUI_EquippedSlottedItem::NativeOnClicked()
{
	Super::NativeOnClicked();
	OnEquippedSlottedItemClicked.Broadcast(this);
}
