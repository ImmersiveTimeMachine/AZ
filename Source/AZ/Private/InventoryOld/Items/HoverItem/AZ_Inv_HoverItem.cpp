// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryOld/Items/HoverItem/AZ_Inv_HoverItem.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Items/AZ_Inv_InventoryItem.h"

void UAZ_Inv_HoverItem::SetImageBrush(const FSlateBrush& Brush) const
{
	Image_Icon->SetBrush(Brush);
}

void UAZ_Inv_HoverItem::UpdateStackCount(const int32 Count)
{
	StackCount = Count;
	if (Count > 0)
	{
		Text_StackCount->SetText(FText::AsNumber(Count));
		Text_StackCount->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		Text_StackCount->SetVisibility(ESlateVisibility::Collapsed);
	}
}

FGameplayTag UAZ_Inv_HoverItem::GetItemType() const
{
	if (InventoryItem.IsValid())
	{
		return InventoryItem->GetItemManifest().GetItemTypeTag();
	}
	return FGameplayTag();
}

void UAZ_Inv_HoverItem::SetIsStackable(const bool bStacks)
{
	bIsStackable = bStacks;
	if (!bStacks)
	{
		Text_StackCount->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UAZ_Inv_InventoryItem* UAZ_Inv_HoverItem::GetInventoryItem() const
{
	return InventoryItem.Get();
}

void UAZ_Inv_HoverItem::SetInventoryItem(UAZ_Inv_InventoryItem* Item)
{
	InventoryItem = Item;
}
