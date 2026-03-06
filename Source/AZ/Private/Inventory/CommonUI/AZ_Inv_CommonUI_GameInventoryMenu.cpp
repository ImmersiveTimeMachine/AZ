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
	}
}

void UAZ_Inv_CommonUI_GameInventoryMenu::NativeOnActivated()
{
	Super::NativeOnActivated();

	// Register input action bindings — these are active only while the widget is activated.
	// CommonUI automatically pushes the Input Mapping Context assigned in Blueprint (IMC_AZ_InventoryMenu).

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
}

void UAZ_Inv_CommonUI_GameInventoryMenu::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
	// CommonUI automatically clears registered action bindings on deactivation.
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
	OnBackAction.ExecuteIfBound();
}

FAZ_Inv_CommonUI_SlotAvailabilityResult UAZ_Inv_CommonUI_GameInventoryMenu::HasRoomForItem(UAZ_Inv_CommonUI_ItemComponent* ItemComponent) const
{
	return FAZ_Inv_CommonUI_SlotAvailabilityResult();
}

void UAZ_Inv_CommonUI_GameInventoryMenu::OnItemHovered(UAZ_Inv_CommonUI_InventoryItem* Item)
{
}

void UAZ_Inv_CommonUI_GameInventoryMenu::OnItemUnHovered()
{
}

bool UAZ_Inv_CommonUI_GameInventoryMenu::HasHoverItem() const
{
	return false;
}

UAZ_Inv_CommonUI_HoverItem* UAZ_Inv_CommonUI_GameInventoryMenu::GetHoverItem() const
{
	return nullptr;
}

float UAZ_Inv_CommonUI_GameInventoryMenu::GetTileSize() const
{
	return 0.f;
}
