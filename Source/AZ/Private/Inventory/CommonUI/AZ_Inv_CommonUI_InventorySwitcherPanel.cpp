// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/CommonUI/AZ_Inv_CommonUI_InventorySwitcherPanel.h"

#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"

#include "Components/Image.h"
#include "Components/VerticalBox.h"

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
	return MenuSwitcher;
}