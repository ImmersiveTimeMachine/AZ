// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/Widgets/SlottedItems/AZ_Inv_CommonUI_SlottedItem.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/Utils/AZ_Inv_InventoryStatics.h"
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
				Image_Icon->SetBrushFromTexture(Icon, true);
			}
		}
	}

	UpdateStackCount(InventoryItem->GetTotalStackCount());
}

void UAZ_Inv_CommonUI_SlottedItem::SetImageBrush(const FSlateBrush& Brush)
{
	if (Image_Icon)
	{
		FSlateBrush FittedBrush = Brush;
		if (const UTexture2D* Texture = Cast<UTexture2D>(Brush.GetResourceObject()))
		{
			const FVector2D Bounds(Brush.ImageSize.X, Brush.ImageSize.Y);
			const FVector2D TextureSize(Texture->GetSizeX(), Texture->GetSizeY());
			if (FMath::IsFinite(Bounds.X) && FMath::IsFinite(Bounds.Y) && Bounds.X > 0.0 && Bounds.Y > 0.0
				&& TextureSize.X > 0.0 && TextureSize.Y > 0.0)
			{
				// The grid owns the item footprint. The image's ScaleBox keeps its authored
				// proportions inside that space, including when grid cells resize unequally.
				const double Scale = FMath::Min(Bounds.X / TextureSize.X, Bounds.Y / TextureSize.Y);
				FittedBrush.ImageSize = TextureSize * Scale;
			}
		}
		Image_Icon->SetBrush(FittedBrush);
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
	if (InventoryItem.IsValid())
	{
		const auto* Weapon = InventoryItem->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
		if (InventoryItem->IsMagazine() || (Weapon && Weapon->bUsesDetachableMagazines))
		{
			const UAZ_Inv_CommonUI_InventoryComponent* Inventory = UAZ_Inv_InventoryStatics::Get_CommonUI_InventoryComponent(GetOwningPlayer());
			const UAZ_Inv_CommonUI_InventoryItem* Magazine = InventoryItem->IsMagazine() ? InventoryItem.Get()
				: (Inventory ? Inventory->FindItemById(InventoryItem->GetInsertedMagazineId()) : nullptr);
			Text_StackCount->SetText(IsValid(Magazine)
				? FText::Format(NSLOCTEXT("AZ_Inventory", "MagazineCountBadge", "{0}/{1}"), FText::AsNumber(Magazine->GetMagazineRounds()), FText::AsNumber(Magazine->GetMagazineCapacity()))
				: NSLOCTEXT("AZ_Inventory", "NoMagazineBadge", "NO MAG"));
			Text_StackCount->SetVisibility(ESlateVisibility::HitTestInvisible);
			return;
		}
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
