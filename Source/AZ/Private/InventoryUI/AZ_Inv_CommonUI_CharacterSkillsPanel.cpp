// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/AZ_Inv_CommonUI_CharacterSkillsPanel.h"

#include "CommonTextBlock.h"

#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

void UAZ_Inv_CommonUI_CharacterSkillsPanel::SetUpgradeNotificationText(const FText& InText)
{
	if (UpgradeNotificationText)
	{
		UpgradeNotificationText->SetText(InText);
	}
}

void UAZ_Inv_CommonUI_CharacterSkillsPanel::SetSkillValue(int32 SkillIndex, const FText& InValue)
{
	if (UTextBlock* Text = GetSkillValueText(SkillIndex))
	{
		Text->SetText(InValue);
	}
}

void UAZ_Inv_CommonUI_CharacterSkillsPanel::SetSkillName(int32 SkillIndex, const FText& InName)
{
	if (UTextBlock* Text = GetSkillNameText(SkillIndex))
	{
		Text->SetText(InName);
	}
}

UTextBlock* UAZ_Inv_CommonUI_CharacterSkillsPanel::GetSkillNameText(int32 Index) const
{
	switch (Index)
	{
	case 0: return StrengthNameText;
	case 1: return AgilityNameText;
	case 2: return ResilienceNameText;
	case 3: return ExpertiseNameText;
	default: return nullptr;
	}
}

UTextBlock* UAZ_Inv_CommonUI_CharacterSkillsPanel::GetSkillValueText(int32 Index) const
{
	switch (Index)
	{
	case 0: return StrengthValueText;
	case 1: return AgilityValueText;
	case 2: return ResilienceValueText;
	case 3: return ExpertiseValueText;
	default: return nullptr;
	}
}