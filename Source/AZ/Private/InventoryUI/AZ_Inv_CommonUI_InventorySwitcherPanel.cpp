// Fill out your copyright notice in the Description page of Project Settings.

#include "InventoryUI/AZ_Inv_CommonUI_InventorySwitcherPanel.h"

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
#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryGrid.h"
#include "InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "InventoryUI/Items/HoverItem/AZ_Inv_CommonUI_HoverItem.h"
#include "InventoryUI/Widgets/AZ_Inv_CommonUI_ItemDescription.h"
#include "InventoryUI/Widgets/Components/AZ_Inv_CommonUI_EquippedGridSlot.h"
#include "InventoryUI/Widgets/SlottedItems/AZ_Inv_CommonUI_EquippedSlottedItem.h"
#include "InventoryUI/Utils/AZ_Inv_InventoryStatics.h"

void UAZ_Inv_CommonUI_InventorySwitcherPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::NativeConstruct()
{
	Super::NativeConstruct();
	InventoryComponent = UAZ_Inv_InventoryStatics::Get_CommonUI_InventoryComponent(GetOwningPlayer());
	EquipmentComponent = GetOwningPlayer() ? GetOwningPlayer()->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>() : nullptr;
	if (InventoryComponent.IsValid()) InventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandleInventoryChanged);
	if (EquipmentComponent.IsValid()) EquipmentComponent->OnEquipmentChanged.AddUniqueDynamic(this, &ThisClass::RefreshEquipment);

	// Bind tab button click events
	if (Button_Equippable) Button_Equippable->OnClicked().AddUObject(this, &ThisClass::ShowEquippables);
	if (Button_Consumable) Button_Consumable->OnClicked().AddUObject(this, &ThisClass::ShowConsumables);
	if (Button_Craftable) Button_Craftable->OnClicked().AddUObject(this, &ThisClass::ShowCraftables);

	if (InventoryGridSwitcher)
	{
		InventoryGridSwitcher->OnActiveWidgetIndexChanged.AddUObject(this, &ThisClass::HandleGridSwitcherIndexChanged);
	}

	// Set initial active grid (Equippables at index 0)
	ActiveGrid.Reset();
	ShowEquippables();

	// Collect all EquippedGridSlots in the widget tree and bind their click delegates
	EquippedGridSlots.Reset();
	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		UAZ_Inv_CommonUI_EquippedGridSlot* EquippedGridSlot = Cast<UAZ_Inv_CommonUI_EquippedGridSlot>(Widget);
		if (IsValid(EquippedGridSlot))
		{
			EquippedGridSlots.Add(EquippedGridSlot);
			EquippedGridSlot->EquippedGridSlotClicked.AddDynamic(this, &ThisClass::HandleEquippedGridSlotClicked);
		}
	});
	RefreshEquipment();
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Description widgets are now docked in ItemDescriptionHBox — no tick repositioning needed.
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::NativeDestruct()
{
	OnHide();
	if (InventoryComponent.IsValid()) InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
	if (EquipmentComponent.IsValid()) EquipmentComponent->OnEquipmentChanged.RemoveDynamic(this, &ThisClass::RefreshEquipment);
	InventoryComponent.Reset();
	EquipmentComponent.Reset();
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
		ActiveGrid->OnHide();
	}

	ActiveGrid = Grid;

	if (ActiveGrid.IsValid())
	{
		ActiveGrid->ShowCursor();
	}

	if (InventoryGridSwitcher)
	{
		InventoryGridSwitcher->SetActiveWidget(Grid);
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
	return IsValid(ItemComponent) && InventoryComponent.IsValid()
		? InventoryComponent->GetRoomForItem(ItemComponent->GetItemManifest())
		: FAZ_Inv_CommonUI_SlotAvailabilityResult();
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

bool UAZ_Inv_CommonUI_InventorySwitcherPanel::HasActivePopUp() const
{
	if (ActiveGrid.IsValid()) return ActiveGrid->HasActivePopUp();
	return false;
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::TryShowContextMenu()
{
	if (ActiveGrid.IsValid()) ActiveGrid->TryShowContextMenu();
}

bool UAZ_Inv_CommonUI_InventorySwitcherPanel::CancelInteraction()
{
	return ActiveGrid.IsValid() && ActiveGrid->CancelInteraction();
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::OnHide()
{
	if (Grid_Equippables) Grid_Equippables->OnHide();
	if (Grid_Consumables) Grid_Consumables->OnHide();
	if (Grid_Craftables) Grid_Craftables->OnHide();
	OnItemUnHovered();
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::RefreshFromInventory()
{
	if (Grid_Equippables) Grid_Equippables->RefreshFromInventory();
	if (Grid_Consumables) Grid_Consumables->RefreshFromInventory();
	if (Grid_Craftables) Grid_Craftables->RefreshFromInventory();
	RefreshEquipment();
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::HandleInventoryChanged()
{
	RefreshEquipment();
	if (DescribedItem.IsValid() && InventoryComponent.IsValid() && InventoryComponent->ContainsItem(DescribedItem.Get()))
	{
		RefreshDescription();
	}
	else
	{
		OnItemUnHovered();
	}
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::RefreshEquipment()
{
	UAZ_Inv_CommonUI_InventoryItem* ActiveItem = EquipmentComponent.IsValid() ? EquipmentComponent->GetActiveItem() : nullptr;
	for (UAZ_Inv_CommonUI_EquippedGridSlot* GridSlot : EquippedGridSlots)
	{
		if (!IsValid(GridSlot)) continue;
		GridSlot->ClearEquippedItem();
		if (!IsValid(ActiveItem) || !ActiveItem->GetItemManifest().GetItemTypeTag().MatchesTag(GridSlot->GetEquipmentTypeTag())) continue;
		UAZ_Inv_CommonUI_EquippedSlottedItem* SlottedItem = GridSlot->OnItemEquipped(ActiveItem, GridSlot->GetEquipmentTypeTag(), GetTileSize());
		if (IsValid(SlottedItem)) SlottedItem->OnEquippedSlottedItemClicked.AddUniqueDynamic(this, &ThisClass::HandleEquippedSlottedItemClicked);
	}
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::SetContextMenuAction(UInputAction* InAction)
{
	if (Grid_Equippables) Grid_Equippables->SetContextMenuAction(InAction);
	if (Grid_Consumables) Grid_Consumables->SetContextMenuAction(InAction);
	if (Grid_Craftables) Grid_Craftables->SetContextMenuAction(InAction);
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
	if (!CanEquipHoverItem(EquippedGridSlot, EquipmentTypeTag) || !InventoryComponent.IsValid()) return;
	UAZ_Inv_CommonUI_InventoryItem* Item = GetHoverItem()->GetInventoryItem();
	// End the drag preview while the item remains in its canonical backpack cells.
	if (Grid_Equippables) Grid_Equippables->OnHide();
	InventoryComponent->Server_EquipSlotClicked(Item, nullptr);
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::HandleEquippedSlottedItemClicked(UAZ_Inv_CommonUI_EquippedSlottedItem* EquippedSlottedItem)
{
	if (!IsValid(EquippedSlottedItem) || !InventoryComponent.IsValid()) return;
	UAZ_Inv_InventoryStatics::CommonUI_ItemUnhovered(GetOwningPlayer());
	if (IsValid(GetHoverItem()) && GetHoverItem()->IsStackable()) return;
	UAZ_Inv_CommonUI_InventoryItem* ItemToEquip = IsValid(GetHoverItem()) ? GetHoverItem()->GetInventoryItem() : nullptr;
	UAZ_Inv_CommonUI_InventoryItem* ItemToUnequip = EquippedSlottedItem->GetInventoryItem().Get();
	if (Grid_Equippables) Grid_Equippables->OnHide();
	InventoryComponent->Server_EquipSlotClicked(ItemToEquip, ItemToUnequip);
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
	const bool bHasEquipment = HeldItem && HeldItem->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_EquipmentFragment>();
	const bool bMatchesType = HeldItem && HeldItem->GetItemManifest().GetItemTypeTag().MatchesTag(EquipmentTypeTag);

	return bHasHoverItem && bHeldItemValid && bNotStackable && bIsEquippable && bHasEquipment && bMatchesType;
}

// =============================================================================
// Item Description System
// =============================================================================

void UAZ_Inv_CommonUI_InventorySwitcherPanel::OnItemHovered(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	OnItemUnHovered();
	if (!IsValid(Item) || !IsValid(ItemDescription) || !GetWorld()) return;
	DescribedItem = Item;
	GetWorld()->GetTimerManager().SetTimer(DescriptionTimer, this, &ThisClass::RefreshDescription, DescriptionTimerDelay, false);
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::OnItemUnHovered()
{
	DescribedItem.Reset();
	if (IsValid(ItemDescription)) ItemDescription->SetVisibility(ESlateVisibility::Collapsed);
	if (IsValid(EquippedItemDescription)) EquippedItemDescription->SetVisibility(ESlateVisibility::Collapsed);
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(DescriptionTimer);
		GetWorld()->GetTimerManager().ClearTimer(EquippedDescriptionTimer);
	}
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::RefreshDescription()
{
	if (!DescribedItem.IsValid() || !IsValid(ItemDescription) || !InventoryComponent.IsValid() || !InventoryComponent->ContainsItem(DescribedItem.Get()))
	{
		OnItemUnHovered();
		return;
	}
	ItemDescription->ShowItem(DescribedItem.Get(), InventoryComponent.Get());
	FTimerDelegate Delegate;
	Delegate.BindUObject(this, &ThisClass::ShowEquippedItemDescription, DescribedItem.Get());
	GetWorld()->GetTimerManager().SetTimer(EquippedDescriptionTimer, Delegate, EquippedDescriptionTimerDelay, false);
}

UAZ_Inv_CommonUI_ItemDescription* UAZ_Inv_CommonUI_InventorySwitcherPanel::GetEquippedItemDescription()
{
	if (!IsValid(EquippedItemDescription) && IsValid(ItemDescriptionHBox) && EquippedItemDescriptionClass)
	{
		EquippedItemDescription = CreateWidget<UAZ_Inv_CommonUI_ItemDescription>(GetOwningPlayer(), EquippedItemDescriptionClass);
		if (IsValid(EquippedItemDescription)) ItemDescriptionHBox->AddChild(EquippedItemDescription);
	}
	return EquippedItemDescription;
}

void UAZ_Inv_CommonUI_InventorySwitcherPanel::ShowEquippedItemDescription(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	if (!IsValid(Item) || IsItemEquipped(Item)) return;
	const FAZ_Inv_CommonUI_EquipmentFragment* EquipmentFragment = GetFragment<FAZ_Inv_CommonUI_EquipmentFragment>(Item, FAZ_GameplayTags::Get().Item_Fragment_Equipment);
	if (!EquipmentFragment) return;
	UAZ_Inv_CommonUI_InventoryItem* EquippedItem = GetEquippedItemByEquipmentType(EquipmentFragment->GetEquipmentType());
	if (!IsValid(EquippedItem)) return;
	if (UAZ_Inv_CommonUI_ItemDescription* Widget = GetEquippedItemDescription())
	{
		Widget->ShowItem(EquippedItem, InventoryComponent.Get());
	}
}
