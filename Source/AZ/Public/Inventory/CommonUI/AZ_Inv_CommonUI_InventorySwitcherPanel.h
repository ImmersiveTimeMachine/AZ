// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "AZ_Inv_CommonUI_InventorySwitcherPanel.generated.h"

class UAZ_Inv_CommonUI_InventoryGrid;
// Forward Declarations
class UCommonActivatableWidgetSwitcher;
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

protected:

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UCommonActivatableWidgetSwitcher* MenuSwitcher;
	
	/** Inventory grids */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UAZ_Inv_CommonUI_InventoryGrid> Grid_Equippables;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UAZ_Inv_CommonUI_InventoryGrid> Grid_Consumables;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UAZ_Inv_CommonUI_InventoryGrid> Grid_Craftables;

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
};
