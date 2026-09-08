// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/Widgets/Components/AZ_Inv_CommonUI_EquippedGridSlot.h"

#include "AZ_GameplayTags.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h"
#include "InventoryUI/Items/HoverItem/AZ_Inv_CommonUI_HoverItem.h"
#include "InventoryUI/Widgets/SlottedItems/AZ_Inv_CommonUI_EquippedSlottedItem.h"
#include "InventoryUI/Utils/AZ_Inv_InventoryStatics.h"

void UAZ_Inv_CommonUI_EquippedGridSlot::NativeOnHovered()
{
	Super::NativeOnHovered();

	if (!IsAvailable()) return;
	UAZ_Inv_CommonUI_HoverItem* HoverItem = UAZ_Inv_InventoryStatics::CommonUI_GetHoverItem(GetOwningPlayer());
	if (!IsValid(HoverItem)) return;

	if (HoverItem->GetItemType().MatchesTag(EquipmentTypeTag))
	{
		SetOccupiedTexture();
		Image_GrayedOutIcon->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAZ_Inv_CommonUI_EquippedGridSlot::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();

	if (!IsAvailable()) return;
	UAZ_Inv_CommonUI_HoverItem* HoverItem = UAZ_Inv_InventoryStatics::CommonUI_GetHoverItem(GetOwningPlayer());
	if (!IsValid(HoverItem)) return;

	if (IsValid(EquippedSlottedItem)) return;

	if (HoverItem->GetItemType().MatchesTag(EquipmentTypeTag))
	{
		SetUnoccupiedTexture();
		Image_GrayedOutIcon->SetVisibility(ESlateVisibility::Visible);
	}
}

void UAZ_Inv_CommonUI_EquippedGridSlot::NativeOnClicked()
{
	Super::NativeOnClicked();
	EquippedGridSlotClicked.Broadcast(this, EquipmentTypeTag);
}

void UAZ_Inv_CommonUI_EquippedGridSlot::ClearEquippedItem()
{
	if (IsValid(EquippedSlottedItem)) EquippedSlottedItem->RemoveFromParent();
	EquippedSlottedItem = nullptr;
	SetInventoryItem(nullptr);
	SetAvailable(true);
	SetState(EInv_CommonUI_GridSlotState::Unoccupied);
	SetUnoccupiedTexture();
	if (Image_GrayedOutIcon) Image_GrayedOutIcon->SetVisibility(ESlateVisibility::Visible);
}

UAZ_Inv_CommonUI_EquippedSlottedItem* UAZ_Inv_CommonUI_EquippedGridSlot::OnItemEquipped(UAZ_Inv_CommonUI_InventoryItem* Item, const FGameplayTag& EquipmentTag, float TileSize)
{
	if (!IsValid(Item) || !EquipmentTag.MatchesTagExact(EquipmentTypeTag) || !EquippedSlottedItemClass || !Overlay_Root) return nullptr;

	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	const FAZ_Inv_CommonUI_GridFragment* GridFragment = GetFragment<FAZ_Inv_CommonUI_GridFragment>(Item, Tags.Item_Fragment_Grid);
	if (!GridFragment) return nullptr;
	const FAZ_Inv_CommonUI_ImageFragment* ImageFragment = GetFragment<FAZ_Inv_CommonUI_ImageFragment>(Item, Tags.Item_Fragment_Icon);
	if (!ImageFragment) return nullptr;
	const FIntPoint GridDimensions = GridFragment->GetGridSize();

	const float IconTileWidth = TileSize - GridFragment->GetGridPadding() * 2;
	const FVector2D DrawSize = FVector2D(GridDimensions) * IconTileWidth;

	EquippedSlottedItem = CreateWidget<UAZ_Inv_CommonUI_EquippedSlottedItem>(GetOwningPlayer(), EquippedSlottedItemClass);
	if (!IsValid(EquippedSlottedItem)) return nullptr;

	EquippedSlottedItem->SetInventoryItem(Item);
	EquippedSlottedItem->SetEquipmentTypeTag(EquipmentTag);
	EquippedSlottedItem->UpdateStackCount(0);

	SetInventoryItem(Item);
	SetAvailable(false);
	SetState(EInv_CommonUI_GridSlotState::Occupied);
	if (Image_GrayedOutIcon) Image_GrayedOutIcon->SetVisibility(ESlateVisibility::Collapsed);

	FSlateBrush Brush;
	Brush.SetResourceObject(ImageFragment->GetIcon());
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = DrawSize;

	EquippedSlottedItem->SetImageBrush(Brush);

	Overlay_Root->AddChildToOverlay(EquippedSlottedItem);
	FGeometry OverlayGeometry = Overlay_Root->GetCachedGeometry();
	auto OverlaySize = OverlayGeometry.Size;

	const float LeftPadding = OverlaySize.X / 2.f - DrawSize.X / 2.f;
	const float TopPadding = OverlaySize.Y / 2.f - DrawSize.Y / 2.f;

	UOverlaySlot* OverlaySlot = UWidgetLayoutLibrary::SlotAsOverlaySlot(EquippedSlottedItem);
	OverlaySlot->SetPadding(FMargin(LeftPadding, TopPadding));

	return EquippedSlottedItem;
}
