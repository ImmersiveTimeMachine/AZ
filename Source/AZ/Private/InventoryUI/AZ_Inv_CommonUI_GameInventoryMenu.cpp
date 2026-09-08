// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/AZ_Inv_CommonUI_GameInventoryMenu.h"

#include "Input/CommonUIInputTypes.h"
#include "CommonActivatableWidgetSwitcher.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventorySwitcherPanel.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "InventoryUI/Items/HoverItem/AZ_Inv_CommonUI_HoverItem.h"
#include "Player/AZ_PlayerController.h"

void UAZ_Inv_CommonUI_GameInventoryMenu::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	if (InventorySwitcherPanel)
	{
		InventorySwitcherPanel->SetOwningCanvas(MainCanvas);
		InventorySwitcherPanel->SetContextMenuAction(ContextMenuAction);
	}
}

void UAZ_Inv_CommonUI_GameInventoryMenu::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (InventorySwitcherPanel) InventorySwitcherPanel->RefreshFromInventory();
	if (const AAZ_PlayerController* PC = Cast<AAZ_PlayerController>(GetOwningPlayer()); PC && PC->OpenInventoryAction && PC->OpenInventoryAction != BackAction)
	{
		MenuActionBindings.Add(RegisterUIActionBinding(FBindUIActionArgs(PC->OpenInventoryAction, false, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleBack))));
	}

	// IMC is pushed automatically by base class UCommonActivatableWidget::ActivateMappingContext()
	// via the InputMapping property set in Blueprint (IMC_AZ_InventoryMenu).

	// Register input action bindings — active only while the widget is activated.

	if (TabLeftAction)
	{
		MenuActionBindings.Add(RegisterUIActionBinding(FBindUIActionArgs(TabLeftAction, false, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleTabLeft))));
	}

	if (TabRightAction)
	{
		MenuActionBindings.Add(RegisterUIActionBinding(FBindUIActionArgs(TabRightAction, false, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleTabRight))));
	}

	if (BackAction)
	{
		MenuActionBindings.Add(RegisterUIActionBinding(FBindUIActionArgs(BackAction, false, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleBack))));
	}

	if (ContextMenuAction)
	{
		MenuActionBindings.Add(RegisterUIActionBinding(FBindUIActionArgs(ContextMenuAction, false, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleContextMenu))));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AZ_Inv_CommonUI_GameInventoryMenu: ContextMenuAction is not set — context menu unavailable."));
	}
}

void UAZ_Inv_CommonUI_GameInventoryMenu::NativeOnDeactivated()
{
	if (InventorySwitcherPanel) InventorySwitcherPanel->OnHide();
	for (FUIActionBindingHandle& Binding : MenuActionBindings)
	{
		Binding.Unregister();
		RemoveActionBinding(Binding);
	}
	MenuActionBindings.Reset();
	Super::NativeOnDeactivated();
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
	if (InventorySwitcherPanel && InventorySwitcherPanel->CancelInteraction())
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
	if (InventorySwitcherPanel) InventorySwitcherPanel->TryShowContextMenu();
}

float UAZ_Inv_CommonUI_GameInventoryMenu::GetTileSize() const
{
	if (InventorySwitcherPanel) return InventorySwitcherPanel->GetTileSize();
	return 0.f;
}
