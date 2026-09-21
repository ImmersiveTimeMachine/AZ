// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/AZ_Inv_CommonUI_GameInventoryMenu.h"

#include "Input/CommonUIInputTypes.h"
#include "CommonActivatableWidgetSwitcher.h"
#include "Components/CanvasPanel.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventorySwitcherPanel.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "InventoryUI/Items/HoverItem/AZ_Inv_CommonUI_HoverItem.h"
#include "Player/AZ_PlayerController.h"
#include "UI/AZ_QuestMapPage.h"

void UAZ_Inv_CommonUI_GameInventoryMenu::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	if (InventorySwitcherPanel)
	{
		InventorySwitcherPanel->SetOwningCanvas(MainCanvas);
		InventorySwitcherPanel->SetContextMenuAction(ContextMenuAction);
		InventorySwitcherPanel->OnMapRequested.RemoveAll(this);
		InventorySwitcherPanel->OnMapRequested.AddUObject(this, &ThisClass::OpenMapPage);
	}
	if (MenuSwitcher)
	{
		MenuSwitcher->OnActiveWidgetIndexChanged.RemoveAll(this);
		MenuSwitcher->OnActiveWidgetIndexChanged.AddUObject(this, &ThisClass::HandleMenuPageChanged);
		if (!MapPage && MapPageClass)
		{
			MapPage = CreateWidget<UAZ_QuestMapPage>(GetOwningPlayer(), MapPageClass);
		}
		if (MapPage && MapPage->GetParent() != MenuSwitcher)
		{
			MapPage->RemoveFromParent();
			MenuSwitcher->AddChild(MapPage);
		}
		if (MapPage) MapPage->OnBackToInventory.AddUniqueDynamic(this, &ThisClass::OpenInventoryPage);
	}
}

void UAZ_Inv_CommonUI_GameInventoryMenu::NativeDestruct()
{
	if (InventorySwitcherPanel) InventorySwitcherPanel->OnMapRequested.RemoveAll(this);
	if (MenuSwitcher) MenuSwitcher->OnActiveWidgetIndexChanged.RemoveAll(this);
	if (MapPage) MapPage->OnBackToInventory.RemoveDynamic(this, &ThisClass::OpenInventoryPage);
	Super::NativeDestruct();
}

bool UAZ_Inv_CommonUI_GameInventoryMenu::IsInventoryPageActive() const
{
	return !MenuSwitcher || MenuSwitcher->GetActiveWidget() == InventoryCanvas;
}

void UAZ_Inv_CommonUI_GameInventoryMenu::OpenMapPage()
{
	if (!MenuSwitcher || !MapPage || (InventorySwitcherPanel && InventorySwitcherPanel->HasHoverItem())) return;
	if (InventorySwitcherPanel) InventorySwitcherPanel->OnHide();
	MenuSwitcher->SetActiveWidget(MapPage);
	MapPage->ActivateWidget();
}

void UAZ_Inv_CommonUI_GameInventoryMenu::OpenInventoryPage()
{
	if (MapPage) MapPage->DeactivateWidget();
	if (MenuSwitcher && InventoryCanvas) MenuSwitcher->SetActiveWidget(InventoryCanvas);
	if (InventorySwitcherPanel) InventorySwitcherPanel->RefreshFromInventory();
}

void UAZ_Inv_CommonUI_GameInventoryMenu::HandleMenuPageChanged(UWidget* ActiveWidget, int32 ActiveIndex)
{
	if (ActiveWidget == MapPage)
	{
		if (InventorySwitcherPanel) InventorySwitcherPanel->OnHide();
		if (MapPage) MapPage->ActivateWidget();
	}
	else if (MapPage)
	{
		MapPage->DeactivateWidget();
	}
}

void UAZ_Inv_CommonUI_GameInventoryMenu::NativeOnActivated()
{
	Super::NativeOnActivated();
	OpenInventoryPage();
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
	if (MapPage) MapPage->DeactivateWidget();
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
	if (InventorySwitcherPanel && InventorySwitcherPanel->HasHoverItem()) return;
	if (MenuSwitcher)
	{
		MenuSwitcher->ActivatePreviousWidget(true);
	}
}

void UAZ_Inv_CommonUI_GameInventoryMenu::HandleTabRight()
{
	if (InventorySwitcherPanel && InventorySwitcherPanel->HasHoverItem()) return;
	if (MenuSwitcher)
	{
		MenuSwitcher->ActivateNextWidget(true);
	}
}

void UAZ_Inv_CommonUI_GameInventoryMenu::HandleBack()
{
	if (!IsInventoryPageActive()) { OpenInventoryPage(); return; }
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
	if (IsInventoryPageActive() && InventorySwitcherPanel) InventorySwitcherPanel->OnItemHovered(Item);
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
	if (IsInventoryPageActive() && InventorySwitcherPanel) InventorySwitcherPanel->TryShowContextMenu();
}

float UAZ_Inv_CommonUI_GameInventoryMenu::GetTileSize() const
{
	if (InventorySwitcherPanel) return InventorySwitcherPanel->GetTileSize();
	return 0.f;
}
