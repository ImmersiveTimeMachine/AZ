#include "InventoryUI/Widgets/HUD/AZ_Inv_CommonUI_InventoryHudWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "InventoryUI/Widgets/HUD/AZ_Inv_CommonUI_InfoMessage.h"
#include "TimerManager.h"
#include "UI/AZ_PlayerUIComponent.h"
#include "UI/AZ_HUDReticleDefinition.h"
#include "UI/AZ_HUDReticleWidget.h"

#define LOCTEXT_NAMESPACE "AZGameHUD"

namespace
{
	void ShowElement(UWidget* Widget, bool bShow)
	{
		if (Widget) Widget->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UAZ_Inv_CommonUI_InventoryHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bPresentedHealth = false;
	PresentedWeaponId.Invalidate();
	if (AmmoRoundsText && DefaultAmmoFontSize == 0) DefaultAmmoFontSize = AmmoRoundsText->GetFont().Size;
	ShowElement(HealthContainer, false);
	ShowElement(WeaponContainer, false);
	ShowElement(LowHealthText, false);
	HidePickupMessage();
	ClearHitFeedback();
	ClearInfoMessage();
	ClearReticle();

	APlayerController* PC = GetOwningPlayer();
	PlayerUI = PC ? PC->FindComponentByClass<UAZ_PlayerUIComponent>() : nullptr;
	if (UAZ_PlayerUIComponent* UI = PlayerUI.Get())
	{
		UI->OnVitalsChanged.AddUniqueDynamic(this, &ThisClass::HandleVitalsChanged);
		UI->OnWeaponChanged.AddUniqueDynamic(this, &ThisClass::HandleWeaponChanged);
		UI->OnHitConfirmed.AddUniqueDynamic(this, &ThisClass::HandleHitConfirmed);
		UI->OnInventoryFull.AddUniqueDynamic(this, &ThisClass::HandleInventoryFull);
		UI->OnInventoryVisibilityChanged.AddUniqueDynamic(this, &ThisClass::HandleInventoryVisibilityChanged);
		UI->OnReticleChanged.AddUniqueDynamic(this, &ThisClass::HandleReticleChanged);
		HandleVitalsChanged(UI->GetVitalsView());
		HandleWeaponChanged(UI->GetWeaponView());
		HandleReticleChanged(UI->GetReticleView());
		HandleInventoryVisibilityChanged(UI->IsInventoryOpen());
	}
}

void UAZ_Inv_CommonUI_InventoryHudWidget::NativeDestruct()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(HitFeedbackTimer);
		GetWorld()->GetTimerManager().ClearTimer(InfoTimer);
	}
	if (UAZ_PlayerUIComponent* UI = PlayerUI.Get())
	{
		UI->OnVitalsChanged.RemoveDynamic(this, &ThisClass::HandleVitalsChanged);
		UI->OnWeaponChanged.RemoveDynamic(this, &ThisClass::HandleWeaponChanged);
		UI->OnHitConfirmed.RemoveDynamic(this, &ThisClass::HandleHitConfirmed);
		UI->OnInventoryFull.RemoveDynamic(this, &ThisClass::HandleInventoryFull);
		UI->OnInventoryVisibilityChanged.RemoveDynamic(this, &ThisClass::HandleInventoryVisibilityChanged);
		UI->OnReticleChanged.RemoveDynamic(this, &ThisClass::HandleReticleChanged);
	}
	PlayerUI.Reset();
	ClearReticle();
	bPresentedHealth = false;
	Super::NativeDestruct();
}

void UAZ_Inv_CommonUI_InventoryHudWidget::HandleVitalsChanged(const FAZ_PlayerVitalsView& View)
{
	if (!View.bAvailable)
	{
		ShowElement(HealthContainer, false);
		ShowElement(LowHealthText, false);
		bPresentedHealth = false;
		return;
	}
	const FLinearColor Tint = View.bCritical ? CriticalHealthFillColor : HealthFillColor;
	ApplyHealthPresentation(View.Fraction, Tint, !bPresentedHealth);
	bPresentedHealth = true;
	if (HealthIcon) HealthIcon->SetColorAndOpacity(Tint);
	if (LowHealthText)
	{
		LowHealthText->SetText(View.Health <= 0.f ? LOCTEXT("DepletedHealth", "NO HEALTH") : LOCTEXT("LowHealth", "LOW HEALTH"));
		LowHealthText->SetColorAndOpacity(FSlateColor(CriticalHealthFillColor));
	}
	ShowElement(HealthContainer, true);
	ShowElement(LowHealthText, View.bCritical);
}

void UAZ_Inv_CommonUI_InventoryHudWidget::HandleWeaponChanged(const FAZ_PlayerWeaponView& View)
{
	if (PresentedWeaponId != View.Ammo.WeaponItemId)
	{
		ClearHitFeedback();
		PresentedWeaponId = View.Ammo.WeaponItemId;
	}
	// Phase 1 displays supported physical-magazine firearms. Unarmed and other
	// equipment keep health visible without a misleading ammunition display.
	const bool bShow = View.bHasWeapon && View.bUsesMagazines;
	ShowElement(WeaponContainer, bShow);
	if (!bShow)
	{
		ClearHitFeedback();
		if (WeaponIcon) WeaponIcon->SetBrushFromTexture(nullptr);
		return;
	}
	if (WeaponIcon)
	{
		WeaponIcon->SetBrushFromTexture(View.Icon, false);
		ShowElement(WeaponIcon, IsValid(View.Icon));
	}
	if (WeaponNameText) WeaponNameText->SetText(View.DisplayName);

	FText Rounds = LOCTEXT("AmmoUnavailable", "--");
	FText Capacity = LOCTEXT("CapacityUnavailable", "/ --");
	if (View.Ammo.MagazineState == EAZ_WeaponMagazineState::NoMagazine)
	{
		Rounds = LOCTEXT("NoMagazine", "NO MAG");
		Capacity = FText::GetEmpty();
	}
	else if (View.Ammo.MagazineState == EAZ_WeaponMagazineState::Loaded || View.Ammo.MagazineState == EAZ_WeaponMagazineState::Empty)
	{
		Rounds = FText::AsNumber(View.Ammo.Rounds);
		Capacity = FText::Format(LOCTEXT("MagazineCapacity", "/ {0}"), FText::AsNumber(View.Ammo.Capacity));
	}
	if (AmmoRoundsText)
	{
		FSlateFontInfo Font = AmmoRoundsText->GetFont();
		Font.Size = View.Ammo.MagazineState == EAZ_WeaponMagazineState::NoMagazine
			? FMath::Min(DefaultAmmoFontSize, 15) : DefaultAmmoFontSize;
		AmmoRoundsText->SetFont(Font);
		AmmoRoundsText->SetText(Rounds);
	}
	if (AmmoCapacityText) AmmoCapacityText->SetText(Capacity);
	if (SpareMagazinesText)
	{
		SpareMagazinesText->SetText(View.Ammo.MagazineState == EAZ_WeaponMagazineState::Unavailable
			? LOCTEXT("SpareMagazinesUnavailable", "-- MAGS")
			: FText::Format(LOCTEXT("SpareMagazines", "{0} MAGS"), FText::AsNumber(View.Ammo.SpareMagazineCount)));
	}
}

void UAZ_Inv_CommonUI_InventoryHudWidget::ShowPickupMessage_Implementation(const FString& Message)
{
	if (PickupText) PickupText->SetText(FText::FromString(Message));
	// The outer HUD owns inventory visibility. Do not gate this update on our
	// cached menu flag: controller and UI delegate callbacks can arrive in either order.
	ShowElement(PickupContainer, !Message.IsEmpty());
}

void UAZ_Inv_CommonUI_InventoryHudWidget::HidePickupMessage_Implementation()
{
	ShowElement(PickupContainer, false);
}

void UAZ_Inv_CommonUI_InventoryHudWidget::HandleHitConfirmed()
{
	if (bInventoryOpen || !GetWorld()) return;
	ShowElement(HitMarker, true);
	GetWorld()->GetTimerManager().SetTimer(HitFeedbackTimer, this, &ThisClass::ClearHitFeedback, HitFeedbackDuration, false);
}

void UAZ_Inv_CommonUI_InventoryHudWidget::ClearHitFeedback()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(HitFeedbackTimer);
	ShowElement(HitMarker, false);
}

void UAZ_Inv_CommonUI_InventoryHudWidget::HandleInventoryFull()
{
	const FText Message = LOCTEXT("InventoryFull", "INVENTORY FULL");
	if (InfoText && InfoContainer)
	{
		InfoText->SetText(Message);
		ShowElement(InfoContainer, true);
		if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(InfoTimer, this, &ThisClass::ClearInfoMessage, InfoMessageDuration, false);
	}
	else if (InfoMessage)
	{
		InfoMessage->SetMessage(Message);
	}
}

void UAZ_Inv_CommonUI_InventoryHudWidget::ClearInfoMessage()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(InfoTimer);
	ShowElement(InfoContainer, false);
}

void UAZ_Inv_CommonUI_InventoryHudWidget::HandleInventoryVisibilityChanged(bool bOpen)
{
	const bool bWasOpen = bInventoryOpen;
	bInventoryOpen = bOpen;
	if (bOpen)
	{
		HidePickupMessage();
		ClearHitFeedback();
		ClearInfoMessage();
	}
	else if (bWasOpen && PlayerUI.IsValid())
	{
		// A collapsed renderer may not tick interpolation. Restore the latest
		// snapshot before revealing the HUD instead of showing an old value.
		bPresentedHealth = false;
		HandleVitalsChanged(PlayerUI->GetVitalsView());
		HandleWeaponChanged(PlayerUI->GetWeaponView());
	}
	SetVisibility(bOpen ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

void UAZ_Inv_CommonUI_InventoryHudWidget::ClearReticle()
{
	ShowElement(ReticleHost, false);
	if (ReticleHost) ReticleHost->ClearChildren();
	ReticleWidget = nullptr;
	ReticleScaleBox = nullptr;
	ActiveReticleDefinition = nullptr;
}

void UAZ_Inv_CommonUI_InventoryHudWidget::HandleReticleChanged(const FAZ_PlayerReticleView& View)
{
	if (!ReticleHost) return;
	UAZ_HUDReticleDefinition* Definition = View.Definition;
	if (!IsValid(Definition) || !Definition->WidgetClass || !GetOwningPlayer() || !WidgetTree)
	{
		ClearReticle();
		return;
	}
	if (ActiveReticleDefinition != Definition || !ReticleWidget
		|| ReticleWidget->GetClass() != Definition->WidgetClass.Get())
	{
		ClearReticle();
		ReticleWidget = CreateWidget<UAZ_HUDReticleWidget>(GetOwningPlayer(), Definition->WidgetClass);
		if (!ReticleWidget) return;
		ReticleScaleBox = WidgetTree->ConstructWidget<UScaleBox>();
		ReticleScaleBox->SetStretch(EStretch::ScaleToFit);
		ReticleScaleBox->SetContent(ReticleWidget);
		ReticleHost->SetContent(ReticleScaleBox);
		ActiveReticleDefinition = Definition;
	}
	const FVector2D Size = Definition->Size;
	if (!FMath::IsFinite(Size.X) || !FMath::IsFinite(Size.Y) || Size.X <= 0 || Size.Y <= 0)
	{
		ShowElement(ReticleHost, false);
		return;
	}
	ReticleHost->SetWidthOverride(Size.X);
	ReticleHost->SetHeightOverride(Size.Y);
	ReticleWidget->SetReticleView(View);
	// The view already reads the live inventory state; the outer HUD owns menu
	// visibility. Rechecking a cached menu flag here makes callback order matter.
	ShowElement(ReticleHost, View.bVisible);
}

#undef LOCTEXT_NAMESPACE
