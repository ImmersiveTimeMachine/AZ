// Fill out your copyright notice in the Description page of Project Settings.
#include "InventoryUI/Items/HoverItem/AZ_Inv_CommonUI_HoverItem.h"

#include "InventoryUI/Items/HoverItem/AZ_Inv_CommonUI_HoverItem.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"

void UAZ_Inv_CommonUI_HoverItem::SetImageBrush(const FSlateBrush& Brush) const
{
	if (Image_Icon)
	{
		Image_Icon->SetBrush(Brush);
	}
}

void UAZ_Inv_CommonUI_HoverItem::UpdateStackCount(const int32 Count)
{
	StackCount = Count;
	if (Text_StackCount)
	{
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
}

FGameplayTag UAZ_Inv_CommonUI_HoverItem::GetItemType() const
{
	if (InventoryItem.IsValid())
	{
		return InventoryItem->GetItemManifest().GetItemTypeTag();
	}
	return FGameplayTag();
}

void UAZ_Inv_CommonUI_HoverItem::SetIsStackable(const bool bStacks)
{
	bIsStackable = bStacks;
	if (!bStacks && Text_StackCount)
	{
		Text_StackCount->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UAZ_Inv_CommonUI_InventoryItem* UAZ_Inv_CommonUI_HoverItem::GetInventoryItem() const
{
	return InventoryItem.Get();
}

void UAZ_Inv_CommonUI_HoverItem::SetInventoryItem(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	InventoryItem = Item;
}
