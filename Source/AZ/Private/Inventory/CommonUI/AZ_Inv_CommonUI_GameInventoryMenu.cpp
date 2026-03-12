// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/CommonUI/AZ_Inv_CommonUI_GameInventoryMenu.h"

#include "Input/CommonUIInputTypes.h"
#include "CommonActivatableWidgetSwitcher.h"
#include "Inventory/CommonUI/AZ_Inv_CommonUI_InventorySwitcherPanel.h"
#include "Inventory/CommonUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "Inventory/Items/HoverItem/AZ_Inv_CommonUI_HoverItem.h"

void UAZ_Inv_CommonUI_GameInventoryMenu::NativeConstruct()
{
	Super::NativeConstruct();

	if (InventorySwitcherPanel)
	{
		InventorySwitcherPanel->SetOwningCanvas(MainCanvas);
		InventorySwitcherPanel->SetContextMenuAction(ContextMenuAction);
	}
}

void UAZ_Inv_CommonUI_GameInventoryMenu::NativeOnActivated()
{
	Super::NativeOnActivated();

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("NativeOnActivated called"));

	// IMC is pushed automatically by base class UCommonActivatableWidget::ActivateMappingContext()
	// via the InputMapping property set in Blueprint (IMC_AZ_InventoryMenu).

	// Register input action bindings — active only while the widget is activated.

	if (TabLeftAction)
	{
		RegisterUIActionBinding(FBindUIActionArgs(TabLeftAction, false, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleTabLeft)));
	}

	if (TabRightAction)
	{
		RegisterUIActionBinding(FBindUIActionArgs(TabRightAction, false, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleTabRight)));
	}

	if (BackAction)
	{
		RegisterUIActionBinding(FBindUIActionArgs(BackAction, false, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleBack)));
	}

	if (ContextMenuAction)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("Registering ContextMenuAction: %s"), *ContextMenuAction->GetName()));
		RegisterUIActionBinding(FBindUIActionArgs(ContextMenuAction, false, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleContextMenu)));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("ContextMenuAction is NULL!"));
	}
}

void UAZ_Inv_CommonUI_GameInventoryMenu::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
	// Base class handles IMC removal and clears registered action bindings.
}

void UAZ_Inv_CommonUI_GameInventoryMenu::HandleTabLeft()
{
	if (MenuSwitcher)
	{
		MenuSwitcher->ActivatePreviousWidget(true);
	}
}

void UAZ_Inv_CommonUI_GameInventoryMenu::HandleTabRight()
{
	if (MenuSwitcher)
	{
		MenuSwitcher->ActivateNextWidget(true);
	}
}

void UAZ_Inv_CommonUI_GameInventoryMenu::HandleBack()
{
	// Skip back if a popup is currently active (right-click was on an item)
	if (InventorySwitcherPanel && InventorySwitcherPanel->HasActivePopUp())
	{
		return;
	}
	OnBackAction.ExecuteIfBound();
}

FAZ_Inv_CommonUI_SlotAvailabilityResult UAZ_Inv_CommonUI_GameInventoryMenu::HasRoomForItem(UAZ_Inv_CommonUI_ItemComponent* ItemComponent) const
{
	if (InventorySwitcherPanel) return InventorySwitcherPanel->HasRoomForItem(ItemComponent);
	return FAZ_Inv_CommonUI_SlotAvailabilityResult();
}

void UAZ_Inv_CommonUI_GameInventoryMenu::OnItemHovered(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	if (InventorySwitcherPanel) InventorySwitcherPanel->OnItemHovered(Item);
}

void UAZ_Inv_CommonUI_GameInventoryMenu::OnItemUnHovered()
{
	if (InventorySwitcherPanel) InventorySwitcherPanel->OnItemUnHovered();
}

bool UAZ_Inv_CommonUI_GameInventoryMenu::HasHoverItem() const
{
	if (InventorySwitcherPanel) return InventorySwitcherPanel->HasHoverItem();
	return false;
}

UAZ_Inv_CommonUI_HoverItem* UAZ_Inv_CommonUI_GameInventoryMenu::GetHoverItem() const
{
	if (InventorySwitcherPanel) return InventorySwitcherPanel->GetHoverItem();
	return nullptr;
}

void UAZ_Inv_CommonUI_GameInventoryMenu::HandleContextMenu()
{
	GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, TEXT("HandleContextMenu triggered"));
	if (InventorySwitcherPanel) InventorySwitcherPanel->TryShowContextMenu();
}

float UAZ_Inv_CommonUI_GameInventoryMenu::GetTileSize() const
{
	if (InventorySwitcherPanel) return InventorySwitcherPanel->GetTileSize();
	return 0.f;
}
