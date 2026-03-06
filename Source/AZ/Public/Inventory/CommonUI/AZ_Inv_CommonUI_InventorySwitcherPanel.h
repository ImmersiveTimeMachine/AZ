// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Inventory/Types/AZ_Inv_GridTypes.h"
#include "AZ_Inv_CommonUI_InventorySwitcherPanel.generated.h"

class UAZ_Inv_CommonUI_HoverItem;
class UAZ_Inv_CommonUI_InventoryGrid;
class UAZ_Inv_CommonUI_ItemComponent;
// Forward Declarations
class UCanvasPanel;
class UCommonActivatableWidgetSwitcher;
class UCommonButtonBase;
class UCommonTextBlock;
class UCommonRichTextBlock;
class UHorizontalBox;
class UImage;
class UVerticalBox;

/**
 * C++ Base Class for AZ_WBP_GameInventorySwitcher
 * Tab-based menu container with Inventory/Crafting/Map tabs,
 * currency display, content switcher, and item description area.
 */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_InventorySwitcherPanel : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:

	void SetItemName(const FText& InText);
	void SetItemDescription(const FText& InText);
	void ClearItemDescription();
	void SetCurrencyAmount(const FText& InText);
	void SetCurrencyName(const FText& InText);
	void SetSectionLabel(const FText& InText);

	UCommonActivatableWidgetSwitcher* GetWidgetSwitcher() const;

	/** Set the active inventory grid and update the switcher. */
	void SetActiveGrid(UAZ_Inv_CommonUI_InventoryGrid* Grid, const FText& GridLabel = FText::GetEmpty());

	UAZ_Inv_CommonUI_InventoryGrid* GetActiveGrid() const;

	FAZ_Inv_CommonUI_SlotAvailabilityResult HasRoomForItem(UAZ_Inv_CommonUI_ItemComponent* ItemComponent) const;
	bool HasHoverItem() const;
	UAZ_Inv_CommonUI_HoverItem* GetHoverItem() const;
	float GetTileSize() const;

	/** Pass the owning canvas to all inventory grids. */
	void SetOwningCanvas(UCanvasPanel* OwningCanvas);

protected:

	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonActivatableWidgetSwitcher* InventoryGridSwitcher;
	
	/** Inventory grids */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UAZ_Inv_CommonUI_InventoryGrid> Grid_Equippables;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UAZ_Inv_CommonUI_InventoryGrid> Grid_Consumables;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UAZ_Inv_CommonUI_InventoryGrid> Grid_Craftables;

	/** Buttons to switch between grids */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Button_Equippable;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Button_Consumable;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCommonButtonBase> Button_Craftable;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UHorizontalBox* HorizontalBoxMenuTabs;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* SectionLabelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* CurrencyAmountText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UCommonTextBlock* CurrencyNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* CurrencyIcon;
	
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* ActiveSelectionLabelText;	

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UCommonTextBlock* ItemNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UCommonRichTextBlock* ItemDescriptionText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UVerticalBox* ItemDescriptionVBox;
	
private:

	void HandleGridSwitcherIndexChanged(UWidget* ActiveWidget, int32 ActiveIndex);

	void ShowEquippables();
	void ShowConsumables();
	void ShowCraftables();

	/** Select the active tab button and deselect the others. */
	void SelectTabButton(UCommonButtonBase* Button);

	/** Maps switcher index to the corresponding grid. */
	UAZ_Inv_CommonUI_InventoryGrid* GetGridAtIndex(int32 Index) const;

	/** Returns a display label for the given grid index. */
	FText GetGridLabel(int32 Index) const;

	/** Helper to get all grids mapped to their buttons for iteration. */
	TMap<UAZ_Inv_CommonUI_InventoryGrid*, UCommonButtonBase*> GetGridButtonMap() const;

	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryGrid> ActiveGrid;
};
