#include "InventoryUI/Widgets/HUD/AZ_Inv_CommonUI_InventoryHudWidget.h"

#include "AZ_GameplayTags.h"
#include "CommonInputSubsystem.h"
#include "CommonUITypes.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Input/AZ_InputConfig.h"
#include "Player/AZ_PlayerController.h"
#include "UI/AZ_ActionPrompt.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/CanvasPanelSlot.h"
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
	void FitWeaponHUD(UWidgetTree* Tree, UWidget* Container, UTextBlock* Name, bool bThrowable)
	{
		if (!Tree || !Container) return;
		if (UWidget* Backplate = Tree->FindWidget(TEXT("CoreContrast"))) Backplate->SetVisibility(ESlateVisibility::Collapsed);
		Container->SetClipping(EWidgetClipping::ClipToBounds);
		const auto Place = [Tree](const TCHAR* WidgetName, FVector2D Position, FVector2D Size)
		{
			if (UWidget* Widget = Tree->FindWidget(WidgetName))
				if (auto* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
				{
					CanvasSlot->SetAutoSize(false);
					CanvasSlot->SetAnchors(FAnchors(0, 0));
					CanvasSlot->SetAlignment(FVector2D::ZeroVector);
					CanvasSlot->SetPosition(Position); CanvasSlot->SetSize(Size);
				}
		};
		// Square item art needs more height than a wide gun silhouette. Keep its
		// name and quantity together beside it, within the same transparent HUD host.
		Place(TEXT("WeaponIconAspectFit"), bThrowable ? FVector2D(0,8) : FVector2D(0,0),
			bThrowable ? FVector2D(100,100) : FVector2D(128,60));
		Place(TEXT("WeaponNameText"), bThrowable ? FVector2D(110,12) : FVector2D(140,4),
			bThrowable ? FVector2D(170,50) : FVector2D(140,54));
		Place(TEXT("AmmoRoundsText"), bThrowable ? FVector2D(110,66) : FVector2D(6,62),
			bThrowable ? FVector2D(170,44) : FVector2D(66,68));
		Place(TEXT("AmmoCapacityText"), FVector2D(73,90), FVector2D(100,38));
		Place(TEXT("MagazineCountRow"), FVector2D(178,68), FVector2D(102,32));
		Place(TEXT("FireModeText"), FVector2D(180,109), FVector2D(100,24));
		if (auto* Fit = Cast<UScaleBox>(Tree->FindWidget(TEXT("WeaponIconAspectFit"))))
		{ Fit->SetStretch(EStretch::ScaleToFit); Fit->SetStretchDirection(EStretchDirection::Both); }
		if (Name)
		{
			Name->SetAutoWrapText(true); Name->SetWrapTextAt(bThrowable ? 170.f : 140.f);
			Name->SetClipping(EWidgetClipping::ClipToBounds);
			Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		}
	}

	void ShowElement(UWidget* Widget, bool bShow)
	{
		if (Widget) Widget->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	void ApplyModePresentation(UWidgetTree* Tree, const FAZ_PlayerWeaponView& View, bool bShowFirearm)
	{
		if (!Tree) return;
		const FGameplayTag NoneProfile = FAZ_GameplayTags::Get().Weapon_None;
		// Same committed profile contract as QuickBar::IsFightMode. The HUD
		// describes the current state; quick-select slot 0 offers its opposite.
		const bool bKnownMode = View.Profile.IsValid() && View.Profile.MatchesTag(NoneProfile.RequestDirectParent());
		const bool bShowMode = bKnownMode && !bShowFirearm;
		const bool bFightMode = bKnownMode && View.Profile != NoneProfile;
		ShowElement(Tree->FindWidget(TEXT("ModeContainer")), bShowMode);
		ShowElement(Tree->FindWidget(TEXT("FightModeIcon")), bShowMode && bFightMode);
		ShowElement(Tree->FindWidget(TEXT("ExploreModeIcon")), bShowMode && !bFightMode);
		if (UTextBlock* ModeName = Cast<UTextBlock>(Tree->FindWidget(TEXT("ModeNameText"))))
		{
			ModeName->SetText(!bShowMode ? FText::GetEmpty()
				: bFightMode ? LOCTEXT("CurrentFightMode", "FIGHT") : LOCTEXT("CurrentExploreMode", "EXPLORE"));
		}
	}
}

void UAZ_Inv_CommonUI_InventoryHudWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bPresentedHealth = false;
	PresentedWeaponId.Invalidate();
	if (AmmoRoundsText && DefaultAmmoFontSize == 0) DefaultAmmoFontSize = AmmoRoundsText->GetFont().Size;
	if (WeaponNameText && DefaultWeaponNameFontSize == 0) DefaultWeaponNameFontSize = WeaponNameText->GetFont().Size;
	ShowElement(HealthContainer, false);
	ShowElement(WeaponContainer, false);
	// Hide the authored magazine row until the first supported weapon view arrives.
	ShowElement(SpareMagazinesText, false);
	if (WidgetTree)
	{
		ShowElement(WidgetTree->FindWidget(TEXT("ModeContainer")), false);
		ShowElement(WidgetTree->FindWidget(TEXT("MagazineCountRow")), false);
		ShowElement(WidgetTree->FindWidget(TEXT("MagazineIcon")), false);
	}
	ShowElement(LowHealthText, false);
	HidePickupMessage();
	UnbindInteractionPresentation();
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		InteractionInput = UCommonInputSubsystem::Get(LocalPlayer);
		InteractionMappings = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		if (InteractionInput.IsValid())
			InteractionInputChangedHandle = InteractionInput->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInteractionInputChanged);
		if (InteractionMappings.IsValid())
			InteractionMappings->ControlMappingsRebuiltDelegate.AddUniqueDynamic(this, &ThisClass::HandleInteractionMappingsRebuilt);
	}
	ClearHitFeedback();
	ClearInfoMessage();
	ClearReticle();

	APlayerController* PC = GetOwningPlayer();
	PlayerUI = PC ? PC->FindComponentByClass<UAZ_PlayerUIComponent>() : nullptr;
	if (UAZ_PlayerUIComponent* UI = PlayerUI.Get())
	{
		UI->OnVitalsChanged.AddUniqueDynamic(this, &ThisClass::HandleVitalsChanged);
		UI->OnWeaponChanged.AddUniqueDynamic(this, &ThisClass::HandleWeaponChanged);
		UI->OnThrowableChanged.AddUniqueDynamic(this, &ThisClass::HandleThrowableChanged);
		UI->OnHitConfirmed.AddUniqueDynamic(this, &ThisClass::HandleHitConfirmed);
		UI->OnInventoryFull.AddUniqueDynamic(this, &ThisClass::HandleInventoryFull);
		UI->OnInventoryVisibilityChanged.AddUniqueDynamic(this, &ThisClass::HandleInventoryVisibilityChanged);
		UI->OnReticleChanged.AddUniqueDynamic(this, &ThisClass::HandleReticleChanged);
		HandleVitalsChanged(UI->GetVitalsView());
		HandleWeaponChanged(UI->GetWeaponView());
		// After the weapon view, so the row is built once against both and settles on the throwable when one
		// is already readied at the moment the HUD is constructed.
		HandleThrowableChanged(UI->GetThrowableView());
		HandleReticleChanged(UI->GetReticleView());
		HandleInventoryVisibilityChanged(UI->IsInventoryOpen());
	}
}

void UAZ_Inv_CommonUI_InventoryHudWidget::NativeDestruct()
{
	UnbindInteractionPresentation();
	bInteractionPromptRequested = false;
	if (InteractionActionPrompt) InteractionActionPrompt->ClearPrompt();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(HitFeedbackTimer);
		GetWorld()->GetTimerManager().ClearTimer(InfoTimer);
	}
	if (UAZ_PlayerUIComponent* UI = PlayerUI.Get())
	{
		UI->OnVitalsChanged.RemoveDynamic(this, &ThisClass::HandleVitalsChanged);
		UI->OnWeaponChanged.RemoveDynamic(this, &ThisClass::HandleWeaponChanged);
		UI->OnThrowableChanged.RemoveDynamic(this, &ThisClass::HandleThrowableChanged);
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

void UAZ_Inv_CommonUI_InventoryHudWidget::HandleThrowableChanged(const FAZ_PlayerThrowableView& View)
{
	PresentedThrowable = View;
	// One row, two sources. Re-run it against the equipment view we already have rather than duplicating the
	// presentation here, so the two can never disagree about what is being shown.
	HandleWeaponChanged(PresentedWeapon);
}

void UAZ_Inv_CommonUI_InventoryHudWidget::HandleWeaponChanged(const FAZ_PlayerWeaponView& View)
{
	FitWeaponHUD(WidgetTree, WeaponContainer, WeaponNameText, PresentedThrowable.bHasThrowable);
	if (WeaponNameText && DefaultWeaponNameFontSize > 0)
	{
		FSlateFontInfo Font = WeaponNameText->GetFont();
		Font.Size = PresentedThrowable.bHasThrowable ? FMath::Min(DefaultWeaponNameFontSize, 18) : DefaultWeaponNameFontSize;
		WeaponNameText->SetFont(Font);
	}
	PresentedWeapon = View;
	if (PresentedWeaponId != View.Ammo.WeaponItemId)
	{
		ClearHitFeedback();
		PresentedWeaponId = View.Ammo.WeaponItemId;
	}

	// ★ A READIED THROWABLE IS WHAT IS IN THE HANDS, and the row has to say so — same row, same place, same
	// reading as a pistol or a rifle (user call 2026-09-21). Equipment is committed to NOTHING while a
	// grenade is out, which the mode indicator resolved to EXPLORE: the HUD claimed the player was carrying
	// nothing while they were holding a live grenade. It takes priority over the mode indicator for exactly
	// that reason, and over a firearm row because a readied throwable means the firearm is already stowed.
	if (PresentedThrowable.bHasThrowable)
	{
		ApplyModePresentation(WidgetTree, View, /*bShowFirearm*/ true);
		ShowElement(WeaponContainer, true);
		ShowElement(SpareMagazinesText, false);
		ShowElement(FireModeText, false);
		if (WidgetTree)
		{
			ShowElement(WidgetTree->FindWidget(TEXT("MagazineCountRow")), false);
			ShowElement(WidgetTree->FindWidget(TEXT("MagazineIcon")), false);
		}
		if (WeaponIcon)
		{
			WeaponIcon->SetBrushFromTexture(PresentedThrowable.Icon, true);
			ShowElement(WeaponIcon, IsValid(PresentedThrowable.Icon));
		}
		if (WeaponNameText) WeaponNameText->SetText(PresentedThrowable.DisplayName);
		// The stack count IS the ammunition here, and there is no magazine behind it to give a capacity for
		// — so the capacity half of the row is cleared rather than showing a misleading "/ --".
		if (AmmoRoundsText)
		{
			if (DefaultAmmoFontSize > 0)
			{
				FSlateFontInfo Font = AmmoRoundsText->GetFont();
				Font.Size = FMath::Min(DefaultAmmoFontSize, 30);
				AmmoRoundsText->SetFont(Font);
			}
			AmmoRoundsText->SetText(FText::Format(LOCTEXT("ThrowableQuantity", "×{0}"),
				FText::AsNumber(FMath::Max(0, PresentedThrowable.Count))));
		}
		if (AmmoCapacityText) AmmoCapacityText->SetText(FText::GetEmpty());
		return;
	}

	// Firearms retain their weapon/ammo row. Other committed equipment modes
	// show the approved Fight/Explore indicator independently of health.
	const bool bShow = View.bHasWeapon && View.bUsesMagazines;
	ApplyModePresentation(WidgetTree, View, bShow);
	ShowElement(WeaponContainer, bShow);
	ShowElement(SpareMagazinesText, bShow);
	if (WidgetTree)
	{
		ShowElement(WidgetTree->FindWidget(TEXT("MagazineCountRow")), bShow);
		ShowElement(WidgetTree->FindWidget(TEXT("MagazineIcon")), bShow);
	}
	if (SpareMagazinesText)
	{
		SpareMagazinesText->SetText(bShow
			? FText::Format(LOCTEXT("SpareMagazines", "{0} MAGS"), FText::AsNumber(FMath::Max(0, View.Ammo.SpareMagazineCount)))
			: FText::GetEmpty());
	}
	ShowElement(FireModeText, bShow && View.bHasFireMode);
	if (FireModeText)
	{
		FireModeText->SetText(!bShow || !View.bHasFireMode ? FText::GetEmpty()
			: View.SelectedFireMode == EAZ_FirearmFireMode::Automatic ? LOCTEXT("AutomaticFireMode", "AUTO") : LOCTEXT("SingleFireMode", "SINGLE"));
	}
	if (!bShow)
	{
		ClearHitFeedback();
		if (WeaponIcon) WeaponIcon->SetBrushFromTexture(nullptr);
		return;
	}
	if (WeaponIcon)
	{
		// The authored ScaleBox fits the texture's natural ratio inside the HUD host.
		WeaponIcon->SetBrushFromTexture(View.Icon, true);
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
}

void UAZ_Inv_CommonUI_InventoryHudWidget::ShowPickupMessage_Implementation(const FString& Message)
{
	// Compatibility for existing Blueprint callers. Do not parse localized text.
	ShowInteractionPrompt(FText::GetEmpty(), Message);
}

void UAZ_Inv_CommonUI_InventoryHudWidget::ShowInteractionPrompt(const FText& Caption, const FString& LegacyEHint)
{
	InteractionCaption = Caption;
	LegacyInteractionEHint = LegacyEHint;
	bInteractionPromptRequested = !Caption.IsEmpty() || !LegacyEHint.IsEmpty();
	RefreshInteractionPrompt();
}

void UAZ_Inv_CommonUI_InventoryHudWidget::RefreshInteractionPrompt()
{
	const AAZ_PlayerController* Player = Cast<AAZ_PlayerController>(GetOwningPlayer());
	const UInputAction* Action = Player && Player->InputConfig
		? Player->InputConfig->FindAbilityInputActionForTag(FAZ_GameplayTags::Get().Input_Action_Interact)
		: nullptr;
	if (!bInteractionPromptRequested || !Player || !Player->IsLocalController())
	{
		if (InteractionActionPrompt) InteractionActionPrompt->ClearPrompt();
		ShowElement(PickupContainer, false);
		return;
	}
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	if (Input && !Input->ShouldShowInputKeys())
	{
		if (InteractionActionPrompt) InteractionActionPrompt->ClearPrompt();
		ShowElement(PickupContainer, false);
		return;
	}
	const ECommonInputType Type = Input ? Input->GetCurrentInputType() : ECommonInputType::MouseAndKeyboard;
	const FKey CurrentKey = Action && Input ? CommonUI::GetFirstKeyForInputType(GetOwningLocalPlayer(), Type, Action) : FKey();
	TArray<FKey> Keys;
	if (Action) CommonUI::GetEnhancedInputActionKeys(GetOwningLocalPlayer(), Action, Keys);
	// Legacy full strings are explicit E hints, never captions to be parsed.
	// Preserve their complete item details only while E is a real current KBM
	// binding. A remap or gamepad must never display the old hardcoded key.
	const bool bSafeLegacyEHint = InteractionCaption.IsEmpty() && !LegacyInteractionEHint.IsEmpty()
		&& Input && Input->ShouldShowInputKeys() && Type == ECommonInputType::MouseAndKeyboard && Keys.Contains(EKeys::E);
	if (bSafeLegacyEHint)
	{
		if (InteractionActionPrompt) InteractionActionPrompt->ClearPrompt();
		if (PickupText) PickupText->SetText(FText::FromString(LegacyInteractionEHint));
		ShowElement(PickupText, true);
	}
	else
	{
		const FText Caption = InteractionCaption.IsEmpty() ? LOCTEXT("InteractCaption", "Interact") : InteractionCaption;
		if (InteractionActionPrompt && Action && CurrentKey.IsValid() && CommonUI::IsEnhancedInputSupportEnabled())
		{
			// Display only. Do not register CommonUI or EnhancedInput commands for
			// this gameplay ability; its existing InputConfig/GAS route remains owner.
			InteractionActionPrompt->ConfigureAction(const_cast<UInputAction*>(Action), Caption, TEXT("Overlay"));
			ShowElement(PickupText, false);
		}
		else
		{
			if (InteractionActionPrompt) InteractionActionPrompt->ClearPrompt();
			if (PickupText) PickupText->SetText(FText::Format(LOCTEXT("InteractionTextFallback", "{0}  {1}"),
				CurrentKey.IsValid() ? CurrentKey.GetDisplayName() : LOCTEXT("InteractionUnbound", "Unbound"), Caption));
			ShowElement(PickupText, true);
		}
	}
	// Keep the existing outer-HUD visibility owner; callback order is unchanged.
	ShowElement(PickupContainer, true);
}

void UAZ_Inv_CommonUI_InventoryHudWidget::HidePickupMessage_Implementation()
{
	bInteractionPromptRequested = false;
	InteractionCaption = FText::GetEmpty();
	LegacyInteractionEHint.Reset();
	if (InteractionActionPrompt) InteractionActionPrompt->ClearPrompt();
	ShowElement(PickupContainer, false);
}

void UAZ_Inv_CommonUI_InventoryHudWidget::HandleInteractionInputChanged(ECommonInputType InputType)
{
	RefreshInteractionPrompt();
}

void UAZ_Inv_CommonUI_InventoryHudWidget::HandleInteractionMappingsRebuilt()
{
	RefreshInteractionPrompt();
}

void UAZ_Inv_CommonUI_InventoryHudWidget::UnbindInteractionPresentation()
{
	if (InteractionInput.IsValid()) InteractionInput->OnInputMethodChangedNative.Remove(InteractionInputChangedHandle);
	if (InteractionMappings.IsValid()) InteractionMappings->ControlMappingsRebuiltDelegate.RemoveDynamic(this, &ThisClass::HandleInteractionMappingsRebuilt);
	InteractionInputChangedHandle.Reset();
	InteractionInput.Reset();
	InteractionMappings.Reset();
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
	ShowTransientInfo(LOCTEXT("InventoryFull", "INVENTORY FULL"));
}

void UAZ_Inv_CommonUI_InventoryHudWidget::ShowTransientInfo(const FText& Message)
{
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
