// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "AZ_Inv_CommonUI_CharacterSkillsPanel.generated.h"

// Forward Declarations
class UButton;
class UCommonActivatableWidgetSwitcher;
class UCommonTextBlock;
class UHorizontalBox;
class UTextBlock;
class UUserWidget;
class UVerticalBox;

/**
 * C++ Base Class for the Character Skills Panel (extracted from AZ_WBP_GameInventoryMenu).
 * Displays 4 primary skill bars (Strength, Agility, Resilience, Expertise),
 * an upgrade notification, and a details section with stat bonuses.
 */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_CharacterSkillsPanel : public UCommonUserWidget
{
	GENERATED_BODY()

public:

	void SetUpgradeNotificationText(const FText& InText);
	void SetSkillValue(int32 SkillIndex, const FText& InValue);
	void SetSkillName(int32 SkillIndex, const FText& InName);

	UCommonActivatableWidgetSwitcher* GetWidgetSwitcher() const { return MenuSwitcher; }

protected:

	// -- Menu Switcher (Skills / Details tabs) --

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UCommonActivatableWidgetSwitcher* MenuSwitcher;

	// -- Upgrade Notification --

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UCommonTextBlock* UpgradeNotificationText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* UpgradeButton;

	// -- Skill Bars: Strength (index 0) --

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* StrengthNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* StrengthValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UUserWidget* StrengthProgress;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* StrengthAdd;

	// -- Skill Bars: Agility (index 1) --

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* AgilityNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* AgilityValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UUserWidget* AgilityProgress;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* AgilityAdd;

	// -- Skill Bars: Resilience (index 2) --

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* ResilienceNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* ResilienceValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UUserWidget* ResilienceProgress;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* ResilienceAdd;

	// -- Skill Bars: Expertise (index 3) --

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* ExpertiseNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* ExpertiseValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UUserWidget* ExpertiseProgress;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* ExpertiseAdd;

	// -- Details Section --

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UCommonTextBlock* DetailsHeaderText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* CarryWeightText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* CarryWeightBonusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* MeleeDmgText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* MeleeDmgBonusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UHorizontalBox* PrimarySkills;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UVerticalBox* DetailsVBox;

private:

	UTextBlock* GetSkillNameText(int32 Index) const;
	UTextBlock* GetSkillValueText(int32 Index) const;
};