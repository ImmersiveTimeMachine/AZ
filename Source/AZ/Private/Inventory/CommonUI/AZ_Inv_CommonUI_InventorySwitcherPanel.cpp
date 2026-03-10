// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/CommonUI/AZ_Inv_CommonUI_InventorySwitcherPanel.h"

#include "CommonActivatableWidgetSwitcher.h"
#include "CommonButtonBase.h"
#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"

#include "AZ/AZ.h"
#include "AZ_GameplayTags.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Inventory/CommonUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "Inventory/CommonUI/AZ_Inv_CommonUI_InventoryGrid.h"
#include "Inventory/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h"
#include "Inventory/CommonUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "Inventory/CommonUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "Inventory/Items/HoverItem/AZ_Inv_CommonUI_HoverItem.h"
#include "Inventory/Widgets/AZ_Inv_ItemDescription/AZ_Inv_CommonUI_ItemDescription.h"
#include "Inventory/Widgets/Components/AZ_Inv_CommonUI_EquippedGridSlot.h"
#include "Inventory/Widgets/SlottedItems/AZ_Inv_CommonUI_EquippedSlottedItem.h"
#include "Inventory/Widgets/Utils/AZ_Inv_InventoryStatics.h"

void UAZ_Inv_CommonUI_InventorySwitcherPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Bind tab button click events
	if (Button_Equippable) Button_Equippable->OnClicked().AddUObject(this, &ThisClass::ShowEquippables);
	if (Button_Consumable) Button_Consumable->OnClicked().AddUObject(this, &ThisClass::ShowConsumables);
	if (Button_Craftable) Button_Craftable->OnClicked().AddUObject(this, &ThisClass::ShowCraftables);

	if (InventoryGridSwitcher)
	{
		InventoryGridSwitcher->OnActiveWidgetIndexChanged.AddUObject(this, &ThisClass::HandleGridSwitcherIndexChanged);
	}

	// Set initial active grid (Equippables at index 0)
	ShowEquippables();

	// Collect all EquippedGridSlots in the widget tree and bind their click delegates
	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		UAZ_Inv_CommonUI_EquippedGridSlot* EquippedGridSlot = Cast<UAZ_Inv_CommonUI_EquippedGridSlot>(Widget);
		if (IsValid(EquippedGridSlot))
		{
			EquippedGridSlots.Add(EquippedGridSlot);
			EquippedGridSlot->EquippedGridSlotClicked.AddDynamic(this, &ThisClass::HandleEquippedGridSlotClicked);
		}
	});
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Description widgets are now docked in ItemDescriptionHBox — no tick repositioning needed.
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::NativeDestruct()
{
	if (Button_Equippable) Button_Equippable->OnClicked().RemoveAll(this);
	if (Button_Consumable) Button_Consumable->OnClicked().RemoveAll(this);
	if (Button_Craftable) Button_Craftable->OnClicked().RemoveAll(this);

	if (InventoryGridSwitcher)
	{
		InventoryGridSwitcher->OnActiveWidgetIndexChanged.RemoveAll(this);
	}

	for (UAZ_Inv_CommonUI_EquippedGridSlot* GridSlot : EquippedGridSlots)
	{
		if (IsValid(GridSlot))
		{
			GridSlot->EquippedGridSlotClicked.RemoveAll(this);
		}
	}
	EquippedGridSlots.Empty();

	Super::NativeDestruct();
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::HandleGridSwitcherIndexChanged(UWidget* ActiveWidget, int32 ActiveIndex)
{
	UAZ_Inv_CommonUI_InventoryGrid* Grid = Cast<UAZ_Inv_CommonUI_InventoryGrid>(ActiveWidget);
	if (!Grid)
	{
		Grid = GetGridAtIndex(ActiveIndex);
	}

	SetActiveGrid(Grid, GetGridLabel(ActiveIndex));

	// Sync tab button selection (e.g. when switched via TabLeft/TabRight)
	TMap<UAZ_Inv_CommonUI_InventoryGrid*, UCommonButtonBase*> Map = GetGridButtonMap();
	if (UCommonButtonBase** FoundButton = Map.Find(Grid))
	{
		SelectTabButton(*FoundButton);
	}
}

UAZ_Inv_CommonUI_InventoryGrid* UAZ_Inv_CommonUI_InventorySwitcherPanel::GetGridAtIndex(int32 Index) const
{
	switch (Index)
	{
		case 0: return Grid_Equippables;
		case 1: return Grid_Consumables;
		case 2: return Grid_Craftables;
		default: return nullptr;
	}
}

FText UAZ_Inv_CommonUI_InventorySwitcherPanel::GetGridLabel(int32 Index) const
{
	switch (Index)
	{
		case 0: return NSLOCTEXT("AZ_Inventory", "Equippables", "Equippables");
		case 1: return NSLOCTEXT("AZ_Inventory", "Consumables", "Consumables");
		case 2: return NSLOCTEXT("AZ_Inventory", "Craftables", "Craftables");
		default: return FText::GetEmpty();
	}
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::ShowEquippables()
{
	SetActiveGrid(Grid_Equippables, GetGridLabel(0));
	SelectTabButton(Button_Equippable);
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::ShowConsumables()
{
	SetActiveGrid(Grid_Consumables, GetGridLabel(1));
	SelectTabButton(Button_Consumable);
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::ShowCraftables()
{
	SetActiveGrid(Grid_Craftables, GetGridLabel(2));
	SelectTabButton(Button_Craftable);
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::SelectTabButton(UCommonButtonBase* Button)
{
	if (Button_Equippable) Button_Equippable->SetIsSelected(Button == Button_Equippable);
	if (Button_Consumable) Button_Consumable->SetIsSelected(Button == Button_Consumable);
	if (Button_Craftable) Button_Craftable->SetIsSelected(Button == Button_Craftable);
}

TMap<UAZ_Inv_CommonUI_InventoryGrid*, UCommonButtonBase*> UAZ_Inv_CommonUI_InventorySwitcherPanel::GetGridButtonMap() const
{
	TMap<UAZ_Inv_CommonUI_InventoryGrid*, UCommonButtonBase*> Map;

	if (Grid_Equippables && Button_Equippable) Map.Add(Grid_Equippables, Button_Equippable);
	if (Grid_Consumables && Button_Consumable) Map.Add(Grid_Consumables, Button_Consumable);
	if (Grid_Craftables && Button_Craftable) Map.Add(Grid_Craftables, Button_Craftable);

	return Map;
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::SetItemName(const FText& InText)
{
	if (ItemNameText)
	{
		ItemNameText->SetText(InText);
	}
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::SetItemDescription(const FText& InText)
{
	if (ItemDescriptionText)
	{
		ItemDescriptionText->SetText(InText);
	}

	if (ItemDescriptionHBox)
	{
		ItemDescriptionHBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::ClearItemDescription()
{
	if (ItemNameText)
	{
		ItemNameText->SetText(FText::GetEmpty());
	}

	if (ItemDescriptionText)
	{
		ItemDescriptionText->SetText(FText::GetEmpty());
	}

	if (ItemDescriptionHBox)
	{
		ItemDescriptionHBox->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::SetCurrencyAmount(const FText& InText)
{
	if (CurrencyAmountText)
	{
		CurrencyAmountText->SetText(InText);
	}
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::SetCurrencyName(const FText& InText)
{
	if (CurrencyNameText)
	{
		CurrencyNameText->SetText(InText);
	}
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::SetSectionLabel(const FText& InText)
{
	if (SectionLabelText)
	{
		SectionLabelText->SetText(InText);
	}
}

UCommonActivatableWidgetSwitcher* UAZ_Inv_CommonUI_InventorySwitcherPanel::GetWidgetSwitcher() const
{
	return InventoryGridSwitcher;
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::SetActiveGrid(UAZ_Inv_CommonUI_InventoryGrid* Grid, const FText& GridLabel)
{
	if (ActiveGrid == Grid) return;

	if (ActiveGrid.IsValid())
	{
		ActiveGrid->HideCursor();
		ActiveGrid->OnHide();
	}

	ActiveGrid = Grid;

	if (ActiveGrid.IsValid())
	{
		ActiveGrid->ShowCursor();
	}

	if (InventoryGridSwitcher)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetActiveGrid: Switcher valid, NumChildren=%d, Grid=%s"),
			InventoryGridSwitcher->GetNumWidgets(), *GetNameSafe(Grid));
		InventoryGridSwitcher->SetActiveWidget(Grid);
		UE_LOG(LogTemp, Warning, TEXT("SetActiveGrid: ActiveIndex after switch = %d"),
			InventoryGridSwitcher->GetActiveWidgetIndex());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("SetActiveGrid: InventoryGridSwitcher is NULL!"));
	}

	if (ActiveSelectionLabelText && !GridLabel.IsEmpty())
	{
		ActiveSelectionLabelText->SetText(GridLabel);
	}
}

UAZ_Inv_CommonUI_InventoryGrid* UAZ_Inv_CommonUI_InventorySwitcherPanel::GetActiveGrid() const
{
	return ActiveGrid.Get();
}

FAZ_Inv_CommonUI_SlotAvailabilityResult UAZ_Inv_CommonUI_InventorySwitcherPanel::HasRoomForItem(UAZ_Inv_CommonUI_ItemComponent* ItemComponent) const
{
	switch (ItemComponent->GetItemManifest().GetItemCategory())
	{
		case EInv_ItemCategory::Equippable:
			return Grid_Equippables->HasRoomForItem(ItemComponent);

		case EInv_ItemCategory::Consumable:
			return Grid_Consumables->HasRoomForItem(ItemComponent);

		case EInv_ItemCategory::Craftable:
			return Grid_Craftables->HasRoomForItem(ItemComponent);

		default:
			UE_LOG(Log_AZ, Warning, TEXT("HasRoomForItem: invalid item category for %s"), *GetNameSafe(ItemComponent));
			return FAZ_Inv_CommonUI_SlotAvailabilityResult();
	}
}

bool UAZ_Inv_CommonUI_InventorySwitcherPanel::HasHoverItem() const
{
	if (Grid_Equippables && Grid_Equippables->HasHoverItem()) return true;
	if (Grid_Consumables && Grid_Consumables->HasHoverItem()) return true;
	if (Grid_Craftables && Grid_Craftables->HasHoverItem()) return true;
	return false;
}

UAZ_Inv_CommonUI_HoverItem* UAZ_Inv_CommonUI_InventorySwitcherPanel::GetHoverItem() const
{
	if (!ActiveGrid.IsValid()) return nullptr;
	return ActiveGrid->GetHoverItem();
}

float UAZ_Inv_CommonUI_InventorySwitcherPanel::GetTileSize() const
{
	if (Grid_Equippables) return Grid_Equippables->GetTileSize();
	return 0.f;
}

bool UAZ_Inv_CommonUI_InventorySwitcherPanel::IsItemEquipped(UAZ_Inv_CommonUI_InventoryItem* Item) const
{
	auto* Found = EquippedGridSlots.FindByPredicate([Item](const UAZ_Inv_CommonUI_EquippedGridSlot* GridSlot)
	{
		return GridSlot->GetInventoryItem() == Item;
	});
	return Found != nullptr;
}

UAZ_Inv_CommonUI_InventoryItem* UAZ_Inv_CommonUI_InventorySwitcherPanel::GetEquippedItemByEquipmentType(const FGameplayTag& EquipmentType) const
{
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();

	auto* Found = EquippedGridSlots.FindByPredicate([&EquipmentType, &Tags](const UAZ_Inv_CommonUI_EquippedGridSlot* GridSlot)
	{
		UAZ_Inv_CommonUI_InventoryItem* InventoryItem = GridSlot->GetInventoryItem().Get();
		if (!IsValid(InventoryItem)) return false;
		const FAZ_Inv_CommonUI_EquipmentFragment* EquipFrag = GetFragment<FAZ_Inv_CommonUI_EquipmentFragment>(InventoryItem, Tags.Item_Fragment_Equipment);
		return EquipFrag && EquipFrag->GetEquipmentType() == EquipmentType;
	});
	return Found ? (*Found)->GetInventoryItem().Get() : nullptr;
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::SetOwningCanvas(UCanvasPanel* OwningCanvas)
{
	OwningCanvasPanel = OwningCanvas;
	if (Grid_Equippables) Grid_Equippables->SetOwningCanvas(OwningCanvas);
	if (Grid_Consumables) Grid_Consumables->SetOwningCanvas(OwningCanvas);
	if (Grid_Craftables) Grid_Craftables->SetOwningCanvas(OwningCanvas);
}

// =============================================================================
// Equipped Grid Slot System
// =============================================================================

void UAZ_Inv_CommonUI_InventorySwitcherPanel::HandleEquippedGridSlotClicked(UAZ_Inv_CommonUI_EquippedGridSlot* EquippedGridSlot, const FGameplayTag& EquipmentTypeTag)
{
	if (!CanEquipHoverItem(EquippedGridSlot, EquipmentTypeTag)) return;

	UAZ_Inv_CommonUI_HoverItem* HoverItem = GetHoverItem();

	const float TileSize = GetTileSize();
	UAZ_Inv_CommonUI_EquippedSlottedItem* EquippedSlottedItem = EquippedGridSlot->OnItemEquipped(
		HoverItem->GetInventoryItem(),
		EquipmentTypeTag,
		TileSize
	);
	if (IsValid(EquippedSlottedItem))
	{
		EquippedSlottedItem->OnEquippedSlottedItemClicked.AddDynamic(this, &ThisClass::HandleEquippedSlottedItemClicked);
	}

	UAZ_Inv_CommonUI_InventoryComponent* InventoryComponent = UAZ_Inv_InventoryStatics::Get_CommonUI_InventoryComponent(GetOwningPlayer());
	check(IsValid(InventoryComponent));
	InventoryComponent->Server_EquipSlotClicked(HoverItem->GetInventoryItem(), nullptr);

	Grid_Equippables->ClearHoverItem();
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::HandleEquippedSlottedItemClicked(UAZ_Inv_CommonUI_EquippedSlottedItem* EquippedSlottedItem)
{
	UAZ_Inv_InventoryStatics::CommonUI_ItemUnhovered(GetOwningPlayer());

	if (IsValid(GetHoverItem()) && GetHoverItem()->IsStackable()) return;

	UAZ_Inv_CommonUI_InventoryItem* ItemToEquip = IsValid(GetHoverItem()) ? GetHoverItem()->GetInventoryItem() : nullptr;
	UAZ_Inv_CommonUI_InventoryItem* ItemToUnequip = EquippedSlottedItem->GetInventoryItem().Get();

	UAZ_Inv_CommonUI_EquippedGridSlot* EquippedGridSlot = FindSlotWithEquippedItem(ItemToUnequip);

	ClearSlotOfItem(EquippedGridSlot);

	Grid_Equippables->AssignHoverItem(ItemToUnequip);

	RemoveEquippedSlottedItem(EquippedSlottedItem);

	MakeEquippedSlottedItem(EquippedSlottedItem, EquippedGridSlot, ItemToEquip);

	BroadcastSlotClickedDelegates(ItemToEquip, ItemToUnequip);
}

bool UAZ_Inv_CommonUI_InventorySwitcherPanel::CanEquipHoverItem(UAZ_Inv_CommonUI_EquippedGridSlot* EquippedGridSlot, const FGameplayTag& EquipmentTypeTag) const
{
	if (!IsValid(EquippedGridSlot) || EquippedGridSlot->GetInventoryItem().IsValid()) return false;

	UAZ_Inv_CommonUI_HoverItem* HoverItem = GetHoverItem();
	if (!IsValid(HoverItem)) return false;

	UAZ_Inv_CommonUI_InventoryItem* HeldItem = HoverItem->GetInventoryItem();

	const bool bHasHoverItem = HasHoverItem();
	const bool bHeldItemValid = IsValid(HeldItem);
	const bool bNotStackable = !HoverItem->IsStackable();
	const bool bIsEquippable = HeldItem && HeldItem->GetItemManifest().GetItemCategory() == EInv_ItemCategory::Equippable;
	const bool bMatchesType = HeldItem && HeldItem->GetItemManifest().GetItemTypeTag().MatchesTag(EquipmentTypeTag);

	return bHasHoverItem && bHeldItemValid && bNotStackable && bIsEquippable && bMatchesType;
}

UAZ_Inv_CommonUI_EquippedGridSlot* UAZ_Inv_CommonUI_InventorySwitcherPanel::FindSlotWithEquippedItem(UAZ_Inv_CommonUI_InventoryItem* EquippedItem) const
{
	auto* FoundSlot = EquippedGridSlots.FindByPredicate([EquippedItem](const UAZ_Inv_CommonUI_EquippedGridSlot* GridSlot)
	{
		return GridSlot->GetInventoryItem() == EquippedItem;
	});
	return FoundSlot ? *FoundSlot : nullptr;
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::ClearSlotOfItem(UAZ_Inv_CommonUI_EquippedGridSlot* EquippedGridSlot)
{
	if (IsValid(EquippedGridSlot))
	{
		EquippedGridSlot->SetEquippedSlottedItem(nullptr);
		EquippedGridSlot->SetInventoryItem(nullptr);
	}
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::RemoveEquippedSlottedItem(UAZ_Inv_CommonUI_EquippedSlottedItem* EquippedSlottedItem)
{
	if (!IsValid(EquippedSlottedItem)) return;

	if (EquippedSlottedItem->OnEquippedSlottedItemClicked.IsAlreadyBound(this, &ThisClass::HandleEquippedSlottedItemClicked))
	{
		EquippedSlottedItem->OnEquippedSlottedItemClicked.RemoveDynamic(this, &ThisClass::HandleEquippedSlottedItemClicked);
	}
	EquippedSlottedItem->RemoveFromParent();
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::MakeEquippedSlottedItem(UAZ_Inv_CommonUI_EquippedSlottedItem* OldSlottedItem, UAZ_Inv_CommonUI_EquippedGridSlot* EquippedGridSlot, UAZ_Inv_CommonUI_InventoryItem* ItemToEquip)
{
	if (!IsValid(EquippedGridSlot)) return;

	UAZ_Inv_CommonUI_EquippedSlottedItem* SlottedItem = EquippedGridSlot->OnItemEquipped(
		ItemToEquip,
		OldSlottedItem->GetEquipmentTypeTag(),
		GetTileSize());

	if (IsValid(SlottedItem))
	{
		SlottedItem->OnEquippedSlottedItemClicked.AddDynamic(this, &ThisClass::HandleEquippedSlottedItemClicked);
	}

	EquippedGridSlot->SetEquippedSlottedItem(SlottedItem);
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::BroadcastSlotClickedDelegates(UAZ_Inv_CommonUI_InventoryItem* ItemToEquip, UAZ_Inv_CommonUI_InventoryItem* ItemToUnequip) const
{
	UAZ_Inv_CommonUI_InventoryComponent* InventoryComponent = UAZ_Inv_InventoryStatics::Get_CommonUI_InventoryComponent(GetOwningPlayer());
	check(IsValid(InventoryComponent));
	InventoryComponent->Server_EquipSlotClicked(ItemToEquip, ItemToUnequip);
}

// =============================================================================
// Item Description System
// =============================================================================

void UAZ_Inv_CommonUI_InventorySwitcherPanel::OnItemHovered(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	if (!IsValid(ItemDescription)) return;
	ItemDescription->SetVisibility(ESlateVisibility::Collapsed);

	GetOwningPlayer()->GetWorldTimerManager().ClearTimer(DescriptionTimer);
	GetOwningPlayer()->GetWorldTimerManager().ClearTimer(EquippedDescriptionTimer);

	FTimerDelegate DescriptionTimerDelegate;
	DescriptionTimerDelegate.BindLambda([this, Item]()
	{
		if (!IsValid(Item) || !IsValid(ItemDescription)) return;
		const auto& Manifest = Item->GetItemManifest();
		ItemDescription->SetVisibility(ESlateVisibility::HitTestInvisible);
		Manifest.AssimilateInventoryFragments(ItemDescription);

		FTimerDelegate EquippedDescriptionTimerDelegate;
		EquippedDescriptionTimerDelegate.BindUObject(this, &ThisClass::ShowEquippedItemDescription, Item);
		GetOwningPlayer()->GetWorldTimerManager().SetTimer(EquippedDescriptionTimer, EquippedDescriptionTimerDelegate, EquippedDescriptionTimerDelay, false);
	});

	GetOwningPlayer()->GetWorldTimerManager().SetTimer(DescriptionTimer, DescriptionTimerDelegate, DescriptionTimerDelay, false);
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::OnItemUnHovered()
{
	if (IsValid(ItemDescription))
	{
		ItemDescription->SetVisibility(ESlateVisibility::Collapsed);
	}
	GetOwningPlayer()->GetWorldTimerManager().ClearTimer(DescriptionTimer);

	if (IsValid(EquippedItemDescription))
	{
		EquippedItemDescription->SetVisibility(ESlateVisibility::Collapsed);
	}
	GetOwningPlayer()->GetWorldTimerManager().ClearTimer(EquippedDescriptionTimer);
}

UAZ_Inv_CommonUI_ItemDescription* UAZ_Inv_CommonUI_InventorySwitcherPanel::GetEquippedItemDescription()
{
	if (!IsValid(EquippedItemDescription) && IsValid(ItemDescriptionHBox))
	{
		EquippedItemDescription = CreateWidget<UAZ_Inv_CommonUI_ItemDescription>(GetOwningPlayer(), EquippedItemDescriptionClass);
		ItemDescriptionHBox->AddChild(EquippedItemDescription);
	}
	return EquippedItemDescription;
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::ShowEquippedItemDescription(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	const FAZ_Inv_CommonUI_EquipmentFragment* EquipmentFragment = GetFragment<FAZ_Inv_CommonUI_EquipmentFragment>(Item, Tags.Item_Fragment_Equipment);
	if (!EquipmentFragment) return;

	const FGameplayTag HoveredEquipmentType = EquipmentFragment->GetEquipmentType();

	if (IsItemEquipped(Item)) return;

	UAZ_Inv_CommonUI_InventoryItem* EquippedItem = GetEquippedItemByEquipmentType(HoveredEquipmentType);
	if (!IsValid(EquippedItem)) return;

	const auto& EquippedItemManifest = EquippedItem->GetItemManifest();
	UAZ_Inv_CommonUI_ItemDescription* EquippedDescriptionWidget = GetEquippedItemDescription();

	EquippedDescriptionWidget->Collapse();
	EquippedDescriptionWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	EquippedItemManifest.AssimilateInventoryFragments(EquippedDescriptionWidget);
}