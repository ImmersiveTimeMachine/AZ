// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_CommonUI_ItemComponent.h"
#include "CommonActivatableWidget.h"
#include "InventoryUI/Items/HoverItem/AZ_Inv_CommonUI_HoverItem.h"
#include "Inventory/Types/AZ_Inv_GridTypes.h"
#include "AZ_Inv_CommonUI_GameInventoryMenu.generated.h"

// Forward Declarations
class UAZ_Inv_CommonUI_InventorySwitcherPanel;
class UBorder;
class UCanvasPanel;
class UHorizontalBox;
class UInputAction;
class UCommonActivatableWidgetSwitcher;

DECLARE_DELEGATE(FOnInventoryMenuBackAction);

/**
 * C++ Base Class for AZ_WBP_GameInventoryMenu
 */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_GameInventoryMenu : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:

	FOnInventoryMenuBackAction OnBackAction;

	FAZ_Inv_CommonUI_SlotAvailabilityResult HasRoomForItem(UAZ_Inv_CommonUI_ItemComponent* ItemComponent) const;
	void OnItemHovered(UAZ_Inv_CommonUI_InventoryItem* Item);
	void OnItemUnHovered();
	bool HasHoverItem() const;
	UAZ_Inv_CommonUI_HoverItem* GetHoverItem() const;
	float GetTileSize() const;

protected:

	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	// -- Input Actions (bound via base class InputMapping = IMC_AZ_InventoryMenu) --

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputAction> TabLeftAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputAction> TabRightAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputAction> BackAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputAction> ContextMenuAction;

	// -- Bound Widgets --

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCanvasPanel* MainCanvas;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UBorder* BackgroundBorder;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UCommonActivatableWidgetSwitcher* MenuSwitcher;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCanvasPanel* InventoryCanvas;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UAZ_Inv_CommonUI_InventorySwitcherPanel> InventorySwitcherPanel;

private:

	void HandleTabLeft();
	void HandleTabRight();
	void HandleBack();
	void HandleContextMenu();
};
