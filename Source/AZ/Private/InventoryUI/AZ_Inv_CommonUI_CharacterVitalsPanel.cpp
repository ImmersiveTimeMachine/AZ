// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/AZ_Inv_CommonUI_CharacterVitalsPanel.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "UI/AZ_PlayerUIComponent.h"

void UAZ_Inv_CommonUI_CharacterVitalsPanel::NativePreConstruct()
{
	Super::NativePreConstruct();

	HideUnsupportedVitals();
	// Vendor templates contain sample percentages. Do not show one before binding.
	if (HealthProgressBar)
	{
		HealthProgressBar->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (HealthValueText)
	{
		HealthValueText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAZ_Inv_CommonUI_CharacterVitalsPanel::NativeConstruct()
{
	Super::NativeConstruct();

	UnbindPlayerUI();
	CurrentVitals = FAZ_PlayerVitalsView{};
	HideUnsupportedVitals();
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		BoundPlayerUI = PlayerController->FindComponentByClass<UAZ_PlayerUIComponent>();
	}

	if (UAZ_PlayerUIComponent* PlayerUI = BoundPlayerUI.Get())
	{
		// Subscribe before the read so both initial construction and reopening are current.
		PlayerUI->OnVitalsChanged.AddUniqueDynamic(this, &ThisClass::HandleVitalsChanged);
		PlayerUI->OnInventoryVisibilityChanged.AddUniqueDynamic(this, &ThisClass::HandleInventoryVisibilityChanged);
		HandleVitalsChanged(PlayerUI->GetVitalsView());
	}
	else
	{
		HandleVitalsChanged(FAZ_PlayerVitalsView{});
	}
}

void UAZ_Inv_CommonUI_CharacterVitalsPanel::NativeDestruct()
{
	UnbindPlayerUI();
	CurrentVitals = FAZ_PlayerVitalsView{};
	Super::NativeDestruct();
}

void UAZ_Inv_CommonUI_CharacterVitalsPanel::HandleVitalsChanged(const FAZ_PlayerVitalsView& View)
{
	const bool bImmediate = !CurrentVitals.bAvailable;
	CurrentVitals = View;
	// The Blueprint calls HQUI's PB_SetPercent API before the bar becomes visible.
	if (View.bAvailable)
	{
		ApplyHealthPresentation(View.Fraction,
			View.bCritical ? CriticalHealthFillColor : HealthFillColor, bImmediate);
	}
	const ESlateVisibility HealthVisibility = View.bAvailable
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (HealthProgressBar)
	{
		HealthProgressBar->SetVisibility(HealthVisibility);
	}
	if (HealthIconImage)
	{
		HealthIconImage->SetVisibility(HealthVisibility);
	}
	if (HealthValueText)
	{
		FNumberFormattingOptions Format;
		Format.MinimumFractionalDigits = 0;
		Format.MaximumFractionalDigits = 1;
		HealthValueText->SetText(View.bAvailable
			? FText::Format(NSLOCTEXT("InventoryVitals", "HealthValue", "{0} / {1}"),
				FText::AsNumber(View.Health, &Format), FText::AsNumber(View.MaxHealth, &Format))
			: FText::GetEmpty());
		HealthValueText->SetVisibility(HealthVisibility);
	}
}

void UAZ_Inv_CommonUI_CharacterVitalsPanel::HandleInventoryVisibilityChanged(bool bOpen)
{
	if (bOpen && BoundPlayerUI.IsValid())
	{
		// Inventory stays constructed while collapsed. Snap the renderer before
		// the first visible frame even if its interpolation was suspended offscreen.
		CurrentVitals = FAZ_PlayerVitalsView{};
		HandleVitalsChanged(BoundPlayerUI->GetVitalsView());
	}
}

void UAZ_Inv_CommonUI_CharacterVitalsPanel::HideUnsupportedVitals()
{
	if (bShowUnsupportedVitals)
	{
		return;
	}

	UWidget* UnsupportedWidgets[] = { InfectionIconImage, InfectionProgressBar,
		MortalityIconImage, MortalityProgressBar, InfectionContainer, MortalityContainer };
	for (UWidget* Widget : UnsupportedWidgets)
	{
		if (Widget)
		{
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UAZ_Inv_CommonUI_CharacterVitalsPanel::UnbindPlayerUI()
{
	if (UAZ_PlayerUIComponent* PlayerUI = BoundPlayerUI.Get())
	{
		PlayerUI->OnVitalsChanged.RemoveDynamic(this, &ThisClass::HandleVitalsChanged);
		PlayerUI->OnInventoryVisibilityChanged.RemoveDynamic(this, &ThisClass::HandleInventoryVisibilityChanged);
	}
	BoundPlayerUI.Reset();
}
