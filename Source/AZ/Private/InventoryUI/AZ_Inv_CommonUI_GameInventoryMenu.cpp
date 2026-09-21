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
		InventorySwitcherPanel->OnTabNavigationRequested.RemoveAll(this);
		InventorySwitcherPanel->OnTabNavigationRequested.AddUObject(this, &ThisClass::HandleTabNavigationRequested);
		InventorySwitcherPanel->SetTabNavigationActions(TabLeftAction, TabRightAction);
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
	if (InventorySwitcherPanel)
	{
		InventorySwitcherPanel->OnMapRequested.RemoveAll(this);
		InventorySwitcherPanel->OnTabNavigationRequested.RemoveAll(this);
	}
	if (MenuSwitcher) MenuSwitcher->OnActiveWidgetIndexChanged.RemoveAll(this);
	if (MapPage) MapPage->OnBackToInventory.RemoveDynamic(this, &ThisClass::OpenInventoryPage);
	Super::NativeDestruct();
}

UWidget* UAZ_Inv_CommonUI_GameInventoryMenu::GetLogicalMenuPage() const
{
	if (!MenuSwitcher) return InventoryCanvas;
	if (UWidget* Pending = MenuSwitcher->GetPendingActiveWidget()) return Pending;
	return MenuSwitcher->GetActiveWidget();
}

bool UAZ_Inv_CommonUI_GameInventoryMenu::IsInventoryPageActive() const
{
	return GetLogicalMenuPage() == InventoryCanvas;
}

void UAZ_Inv_CommonUI_GameInventoryMenu::OpenMapPage()
{
	if (!IsActivated() || !MenuSwitcher || !MapPage || (InventorySwitcherPanel && !InventorySwitcherPanel->CanChangeInventoryTab())) return;
	if (InventorySwitcherPanel) InventorySwitcherPanel->OnHide();
	MenuSwitcher->SetActiveWidget(MapPage);
	if (InventorySwitcherPanel) InventorySwitcherPanel->SetMapTabActive(true);
	// A fade's pending child is not yet in Slate's visible child tree. Activate
	// and focus at arrival; the immediate path also handles already-visible Map.
	if (MenuSwitcher->GetActiveWidget() == MapPage)
	{
		MapPage->ActivateWidget();
		MapPage->RequestRefreshFocus();
	}
}

void UAZ_Inv_CommonUI_GameInventoryMenu::OpenInventoryPage()
{
	if (MenuSwitcher && InventoryCanvas) MenuSwitcher->SetActiveWidget(InventoryCanvas);
	else if (MapPage) MapPage->DeactivateWidget();
	if (InventorySwitcherPanel)
	{
		InventorySwitcherPanel->RefreshFromInventory();
		InventorySwitcherPanel->SetMapTabActive(false);
		if (IsActivated() && (!MenuSwitcher || MenuSwitcher->GetActiveWidget() == InventoryCanvas))
			InventorySwitcherPanel->FocusActiveInventoryTab();
	}
}

void UAZ_Inv_CommonUI_GameInventoryMenu::HandleMenuPageChanged(UWidget* ActiveWidget, int32 ActiveIndex)
{
	// The engine broadcasts after the visible Slate index changes. Its pending
	// target remains the source of logical navigation, but focus follows arrival.
	if (!IsActivated())
	{
		if (MapPage) MapPage->DeactivateWidget();
		return;
	}
	UWidget* Arrived = MenuSwitcher ? MenuSwitcher->GetActiveWidget() : ActiveWidget;
	if (Arrived == MapPage && GetLogicalMenuPage() == MapPage)
	{
		if (InventorySwitcherPanel) InventorySwitcherPanel->OnHide();
		if (MapPage)
		{
			MapPage->ActivateWidget();
			MapPage->RequestRefreshFocus();
		}
		if (InventorySwitcherPanel) InventorySwitcherPanel->SetMapTabActive(true);
	}
	else
	{
		if (MapPage) MapPage->DeactivateWidget();
		if (InventorySwitcherPanel)
		{
			InventorySwitcherPanel->SetMapTabActive(GetLogicalMenuPage() == MapPage);
			if (Arrived == InventoryCanvas && IsInventoryPageActive()) InventorySwitcherPanel->FocusActiveInventoryTab();
		}
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
	NavigateInventoryTab(-1);
}

void UAZ_Inv_CommonUI_GameInventoryMenu::HandleTabRight()
{
	NavigateInventoryTab(1);
}

void UAZ_Inv_CommonUI_GameInventoryMenu::HandleTabNavigationRequested(int32 Direction)
{
	NavigateInventoryTab(Direction);
}

bool UAZ_Inv_CommonUI_GameInventoryMenu::NavigateInventoryTab(int32 Direction)
{
	if (!Direction || !IsActivated() || !GetOwningLocalPlayer() || !InventorySwitcherPanel
		|| !InventorySwitcherPanel->CanChangeInventoryTab()) return false;
	const bool bMapActive = MapPage && GetLogicalMenuPage() == MapPage;
	if (!bMapActive && !IsInventoryPageActive()) return false;
	const int32 Current = bMapActive ? 3 : InventorySwitcherPanel->GetActiveInventoryCategoryIndex();
	if (Current == INDEX_NONE) return false;
	const int32 Next = (Current + (Direction > 0 ? 1 : -1) + 4) % 4;
	if (Next == 3)
	{
		OpenMapPage();
		return MapPage && GetLogicalMenuPage() == MapPage;
	}
	if (!InventorySwitcherPanel->ShowInventoryCategory(Next)) return false;
	if (bMapActive) OpenInventoryPage();
	return true;
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
