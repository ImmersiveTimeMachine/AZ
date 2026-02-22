// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Widgets/SlottedItems/AZ_Inv_CommonUI_SlottedItem.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Inventory/CommonUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "Inventory/Items/Fragments/AZ_Inv_CommonUI_Item_Fragment.h"
#include "Inventory/Items/Manifest/AZ_Inv_CommonUI_ItemManifest.h"

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
		if (const FAZ_Inv_CommonUI_Image_Fragment* ImageFragment = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_Image_Fragment>())
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
		Image_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UAZ_Inv_CommonUI_SlottedItem::UpdateStackCount(int32 InStackCount)
{
	if (!Text_StackCount)
	{
		return;
	}

	if (InStackCount > 1)
	{
		Text_StackCount->SetText(FText::AsNumber(InStackCount));
		Text_StackCount->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		Text_StackCount->SetVisibility(ESlateVisibility::Collapsed);
	}
}
