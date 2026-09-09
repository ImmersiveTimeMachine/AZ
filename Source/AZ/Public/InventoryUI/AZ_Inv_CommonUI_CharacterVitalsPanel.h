// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/AZ_PlayerUITypes.h"
#include "AZ_Inv_CommonUI_CharacterVitalsPanel.generated.h"

// Forward Declarations
class UImage;
class UTextBlock;
class UUserWidget;
class UAZ_PlayerUIComponent;

/**
 * C++ Base Class for the Character Vitals Panel (extracted from AZ_WBP_GameInventoryMenu).
 * Displays the portrait and canonical player health shared with the HUD.
 * Infection and mortality presentation stays hidden until gameplay supplies it.
 */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_CharacterVitalsPanel : public UCommonUserWidget
{
	GENERATED_BODY()

protected:

	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Blueprint owns the vendor bar call; apply the first available value immediately. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|Vitals")
	void ApplyHealthPresentation(double Percent, FLinearColor FillColor, bool bImmediate);

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Inventory|Vitals")
	FAZ_PlayerVitalsView CurrentVitals;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Vitals")
	FLinearColor HealthFillColor = FLinearColor::FromSRGBColor(FColor(238, 234, 224));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Vitals")
	FLinearColor CriticalHealthFillColor = FLinearColor::FromSRGBColor(FColor(224, 119, 104));

	/** Designer opt-in for future supported meters; no gameplay value is implied. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Vitals")
	bool bShowUnsupportedVitals = false;

	// -- Hero Portrait --

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UImage* HeroPortraitImage;

	// -- Health Orb --

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UImage* HealthIconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UUserWidget* HealthProgressBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* HealthValueText;

	// -- Infection Orb --

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UImage* InfectionIconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UUserWidget* InfectionProgressBar;

	/** Optional dedicated container; it must not contain health or portrait elements. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UWidget* InfectionContainer;

	// -- Mortality Orb --

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UImage* MortalityIconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UUserWidget* MortalityProgressBar;

	/** Keep heartbeat decorations outside this optional unsupported-meter group. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UWidget* MortalityContainer;

	// -- Heartbeat EKG Decorations --

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* HeartbeatImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* HeartbeatShadowImage;

private:
	UFUNCTION()
	void HandleVitalsChanged(const FAZ_PlayerVitalsView& View);
	UFUNCTION()
	void HandleInventoryVisibilityChanged(bool bOpen);

	void HideUnsupportedVitals();
	void UnbindPlayerUI();

	TWeakObjectPtr<UAZ_PlayerUIComponent> BoundPlayerUI;
};
