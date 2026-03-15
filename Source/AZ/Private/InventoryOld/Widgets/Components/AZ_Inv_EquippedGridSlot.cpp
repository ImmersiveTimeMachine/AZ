// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryOld/Widgets/Components/AZ_Inv_EquippedGridSlot.h"

#include "AZ_GameplayTags.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "InventoryOld/Items/Fragments/AZ_Inv_ItemFragment.h"
#include "InventoryOld/Items/HoverItem/AZ_Inv_HoverItem.h"
#include "InventoryOld/Widgets/SlottedItems/AZ_Inv_EquippedSlottedItem.h"
#include "InventoryUI/Utils/AZ_Inv_InventoryStatics.h"

void UAZ_Inv_EquippedGridSlot::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!IsAvailable()) return;
	UAZ_Inv_HoverItem* HoverItem = UAZ_Inv_InventoryStatics::GetHoverItem(GetOwningPlayer());
	if (!IsValid(HoverItem)) return;

	if (HoverItem->GetItemType().MatchesTag(EquipmentTypeTag))
	{
		SetOccupiedTexture();
		Image_GrayedOutIcon->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAZ_Inv_EquippedGridSlot::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	if (!IsAvailable()) return;
	UAZ_Inv_HoverItem* HoverItem = UAZ_Inv_InventoryStatics::GetHoverItem(GetOwningPlayer());
	if (!IsValid(HoverItem)) return;

	if (IsValid(EquippedSlottedItem)) return;

	if (HoverItem->GetItemType().MatchesTag(EquipmentTypeTag))
	{
		SetUnoccupiedTexture();
		Image_GrayedOutIcon->SetVisibility(ESlateVisibility::Visible);
	}
}

FReply UAZ_Inv_EquippedGridSlot::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	EquippedGridSlotClicked.Broadcast(this, EquipmentTypeTag);
	return FReply::Handled();
}

UAZ_Inv_EquippedSlottedItem* UAZ_Inv_EquippedGridSlot::OnItemEquipped(UAZ_Inv_InventoryItem* Item, const FGameplayTag& EquipmentTag, float TileSize)
{
	// Check the Equipment Type Tag
	if (!EquipmentTag.MatchesTagExact(EquipmentTypeTag)) return nullptr;
	
	// Get Grid Dimensions
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	const FAZ_Inv_GridFragment* GridFragment = GetFragment<FAZ_Inv_GridFragment>(Item, Tags.Item_Fragment_Grid);
	if (!GridFragment) return nullptr;
	const FIntPoint GridDimensions = GridFragment->GetGridSize();

	// Calculate the Draw Size for the Equipped Slotted Item
	const float IconTileWidth = TileSize - GridFragment->GetGridPadding() * 2;
	const FVector2D DrawSize = GridDimensions * IconTileWidth;
	
	// Create the Equipped Slotted Item widget
	EquippedSlottedItem = CreateWidget<UAZ_Inv_EquippedSlottedItem>(GetOwningPlayer(), EquippedSlottedItemClass);
	
	// Set the Slotted Item's Inventory Item
	EquippedSlottedItem->SetInventoryItem(Item);
	
	// Set the Slotted Item's Equipment Type Tag
	EquippedSlottedItem->SetEquipmentTypeTag(EquipmentTag);
	
	// Hide the Stack Count widget on the Slotted Item
	EquippedSlottedItem->UpdateStackCount(0);
	
	// Set Inventory Item on this class (the Equipped Grid Slot)
	SetInventoryItem(Item);
	
	// Set the Image Brush on the Equipped Slotted Item
	const FAZ_Inv_ImageFragment* ImageFragment = GetFragment<FAZ_Inv_ImageFragment>(Item, Tags.Item_Fragment_Icon);
	if (!ImageFragment) return nullptr;

	FSlateBrush Brush;
	Brush.SetResourceObject(ImageFragment->GetIcon());
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = DrawSize;
	
	EquippedSlottedItem->SetImageBrush(Brush);
	
	// Add the Slotted Item as a child to this widget's Overlay
	Overlay_Root->AddChildToOverlay(EquippedSlottedItem);
	FGeometry OverlayGeometry = Overlay_Root->GetCachedGeometry();
	auto OverlayPos = OverlayGeometry.Position;
	auto OverlaySize = OverlayGeometry.Size;

	const float LeftPadding = OverlaySize.X / 2.f - DrawSize.X / 2.f;
	const float TopPadding = OverlaySize.Y / 2.f - DrawSize.Y / 2.f;

	UOverlaySlot* OverlaySlot = UWidgetLayoutLibrary::SlotAsOverlaySlot(EquippedSlottedItem);
	OverlaySlot->SetPadding(FMargin(LeftPadding, TopPadding));
	
	// Return the Equipped Slotted Item widget
	return EquippedSlottedItem;
}
