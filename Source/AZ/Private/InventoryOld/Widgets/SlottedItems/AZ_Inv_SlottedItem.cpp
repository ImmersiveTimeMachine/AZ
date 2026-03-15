// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryOld/Widgets/SlottedItems/AZ_Inv_SlottedItem.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "InventoryUI/Utils/AZ_Inv_InventoryStatics.h"

FReply UAZ_Inv_SlottedItem::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	
	OnSlottedItemClickedEvent.Broadcast(GridIndex, InMouseEvent);
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UAZ_Inv_SlottedItem::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UAZ_Inv_InventoryStatics::ItemHovered(GetOwningPlayer(), InventoryItem.Get());
}

void UAZ_Inv_SlottedItem::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	UAZ_Inv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
}

void UAZ_Inv_SlottedItem::SetImageBrush(const FSlateBrush& Brush) const
{
	Image_Icon->SetBrush(Brush);
}

void UAZ_Inv_SlottedItem::UpdateStackCount(int32 InStackCount)
{
	if (InStackCount > 0 )
	{
		Text_StackCount->SetVisibility(ESlateVisibility::Visible);
		Text_StackCount->SetText(FText::AsNumber(InStackCount));
	}
	else
	{
		Text_StackCount->SetVisibility(ESlateVisibility::Collapsed);
	}
}
