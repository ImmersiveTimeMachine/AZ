// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/Widgets/SlottedItems/AZ_Inv_CommonUI_SlottedItem.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h"
#include "InventoryUI/Items/Manifest/AZ_Inv_CommonUI_ItemManifest.h"

void UAZ_Inv_CommonUI_SlottedItem::SetInventoryItem(TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> InInventoryItem)
{
	InventoryItem = InInventoryItem;

	if (!InventoryItem.IsValid())
	{
		if (Image_Icon)
		{
			Image_Icon->SetVisibility(ESlateVisibility::Hidden);
		}
		UpdateStackCount(0);
		return;
	}

	if (Image_Icon)
	{
		Image_Icon->SetVisibility(ESlateVisibility::Visible);

		const FAZ_Inv_CommonUI_ItemManifest& Manifest = InventoryItem->GetItemManifest();

		// Try to find an image fragment to display the icon
		if (const FAZ_Inv_CommonUI_ImageFragment* ImageFragment = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_ImageFragment>())
		{
			if (UTexture2D* Icon = ImageFragment->GetIcon())
			{
				Image_Icon->SetBrushFromTexture(Icon);
			}
		}
	}

	UpdateStackCount(InventoryItem->GetTotalStackCount());
}

void UAZ_Inv_CommonUI_SlottedItem::SetImageBrush(const FSlateBrush& Brush)
{
	if (Image_Icon)
	{
		Image_Icon->SetBrush(Brush);
	}
}

FReply UAZ_Inv_CommonUI_SlottedItem::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FKey PressedKey = InMouseEvent.GetEffectingButton();

	// Left-click is handled by CommonUI button system (OnItemClicked).
	// For any other mouse button, broadcast the key so the grid can check the IMC mapping.
	if (PressedKey != EKeys::LeftMouseButton)
	{
		OnMouseButtonDownDelegate.Broadcast(this, PressedKey);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UAZ_Inv_CommonUI_SlottedItem::UpdateStackCount(int32 InStackCount)
{
	if (!Text_StackCount)
	{
		return;
	}

	if (InStackCount > 0)
	{
		Text_StackCount->SetText(FText::AsNumber(InStackCount));
		Text_StackCount->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		Text_StackCount->SetVisibility(ESlateVisibility::Collapsed);
	}
}
