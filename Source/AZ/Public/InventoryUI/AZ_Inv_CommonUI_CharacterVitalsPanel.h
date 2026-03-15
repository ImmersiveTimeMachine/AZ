// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "AZ_Inv_CommonUI_CharacterVitalsPanel.generated.h"

// Forward Declarations
class UImage;
class UUserWidget;

/**
 * C++ Base Class for the Character Vitals Panel (extracted from AZ_WBP_GameInventoryMenu).
 * Displays a hero portrait image and 3 circular progress bars:
 * Health (heart icon), Stamina (mana icon), and Defense (head icon),
 * plus heartbeat EKG decoration images.
 */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_CharacterVitalsPanel : public UCommonUserWidget
{
	GENERATED_BODY()

protected:

	// -- Hero Portrait --

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UImage* HeroPortraitImage;

	// -- Health Orb --

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UImage* HealthIconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
		UUserWidget* HealthProgressBar;

	// -- Infection Orb --

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UImage* InfectionIconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UUserWidget* InfectionProgressBar;

	// -- Mortality Orb --

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UImage* MortalityIconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UUserWidget* MortalityProgressBar;

	// -- Heartbeat EKG Decorations --

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* HeartbeatImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* HeartbeatShadowImage;
};