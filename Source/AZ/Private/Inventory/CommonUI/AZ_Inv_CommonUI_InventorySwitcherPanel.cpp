// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/CommonUI/AZ_Inv_CommonUI_InventorySwitcherPanel.h"

#include "CommonActivatableWidgetSwitcher.h"
#include "CommonButtonBase.h"
#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"

#include "AZ/AZ.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Inventory/CommonUI/AZ_Inv_CommonUI_InventoryGrid.h"
#include "Inventory/CommonUI/AZ_Inv_CommonUI_ItemComponent.h"
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

	if (ItemDescriptionVBox)
	{
		ItemDescriptionVBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
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

	if (ItemDescriptionVBox)
	{
		ItemDescriptionVBox->SetVisibility(ESlateVisibility::Collapsed);
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

void UAZ_Inv_CommonUI_InventorySwitcherPanel::SetOwningCanvas(UCanvasPanel* OwningCanvas)
{
	if (Grid_Equippables) Grid_Equippables->SetOwningCanvas(OwningCanvas);
	if (Grid_Consumables) Grid_Consumables->SetOwningCanvas(OwningCanvas);
	if (Grid_Craftables) Grid_Craftables->SetOwningCanvas(OwningCanvas);
}