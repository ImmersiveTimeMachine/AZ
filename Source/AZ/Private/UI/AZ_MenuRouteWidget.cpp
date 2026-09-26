#include "UI/AZ_MenuRouteWidget.h"
#include "UI/AZ_MenuCommandButton.h"
#include "UI/AZ_ActionPrompt.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Input/CommonUIInputTypes.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"

#define LOCTEXT_NAMESPACE "CHALKMenus"

UAZ_MenuRouteWidget::UAZ_MenuRouteWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
	SetIsFocusable(true);
	bIsBackHandler = false; // The explicit BackAction below is this page's sole CommonUI Back binding.
	PaperButtonStyle = GetDefault<UButton>()->GetStyle();
	OverlayButtonStyle = PaperButtonStyle;
	PaperComboStyle = GetDefault<UComboBoxString>()->GetWidgetStyle();
	PaperComboRowStyle = GetDefault<UComboBoxString>()->GetItemStyle();
}
void UAZ_MenuRouteWidget::SetRouteOwner(UAZ_MenuRoutesComponent* Owner) { RouteOwner = Owner; }

TSharedRef<SWidget> UAZ_MenuRouteWidget::RebuildWidget()
{
	if (WidgetTree && (!WidgetTree->RootWidget ||
		(WidgetTree->RootWidget->IsA<UCanvasPanel>() && CastChecked<UCanvasPanel>(WidgetTree->RootWidget)->GetChildrenCount() == 0)))
	{
		Background = WidgetTree->ConstructWidget<UBorder>();
		Background->SetPadding(FMargin(0));
		WidgetTree->RootWidget = Background;
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		Background->AddChild(Canvas);
		auto Place = [&](UWidget* Child, const FAnchors& Anchors, const FMargin& Offsets)
		{
			auto* CanvasSlot = Canvas->AddChildToCanvas(Child); CanvasSlot->SetAnchors(Anchors); CanvasSlot->SetOffsets(Offsets); return CanvasSlot;
		};
		Brand = WidgetTree->ConstructWidget<UTextBlock>();
		Brand->SetText(LOCTEXT("Brand", "CHALK"));
		BrandSlot = Place(Brand, FAnchors(0, 0), PaperBrandPlacement);
		Heading = WidgetTree->ConstructWidget<UTextBlock>();
		HeadingSlot = Place(Heading, FAnchors(0, 0), PaperHeadingPlacement);
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
		Scroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);
		BodySlot = Place(Scroll, FAnchors(0, 0, 1, 1), PaperBodyInsets);
		ContentWidth = WidgetTree->ConstructWidget<USizeBox>();
		ContentWidth->SetWidthOverride(MenuColumnWidth);
		if (auto* ContentSlot = Cast<UScrollBoxSlot>(Scroll->AddChild(ContentWidth))) ContentSlot->SetHorizontalAlignment(HAlign_Left);
		Body = WidgetTree->ConstructWidget<UVerticalBox>();
		ContentWidth->AddChild(Body);
		Status = WidgetTree->ConstructWidget<UTextBlock>();
		Status->SetAutoWrapText(true);
		Place(Status, FAnchors(0, 1, 1, 1), FMargin(80, -100, 80, 72));
		if (GetOwningPlayer())
		{
			auto MakePrompt = [&](TSubclassOf<UAZ_ActionPrompt> PromptClass) -> UAZ_ActionPrompt*
			{
				if (!PromptClass) return nullptr;
				UAZ_ActionPrompt* Prompt = CreateWidget<UAZ_ActionPrompt>(GetOwningPlayer(), PromptClass);
				if (Prompt)
				{
					Place(Prompt, FAnchors(0, 1), FMargin(56, -52, 380, 36));
					Prompt->SetVisibility(ESlateVisibility::Collapsed);
				}
				return Prompt;
			};
			BackPrompt = MakePrompt(BackPromptClass);
			OverlayBackPrompt = MakePrompt(OverlayBackPromptClass);
		}
	}
	return Super::RebuildWidget();
}

void UAZ_MenuRouteWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (BackAction) BackBinding = RegisterUIActionBinding(FBindUIActionArgs(BackAction, false, FSimpleDelegate::CreateUObject(this, &ThisClass::HandleBack)));
	RefreshRoute();
}
void UAZ_MenuRouteWidget::NativeOnDeactivated()
{
	if (BackPrompt) BackPrompt->ClearPrompt();
	if (OverlayBackPrompt) OverlayBackPrompt->ClearPrompt();
	if (BackBinding.IsValid()) { BackBinding.Unregister(); RemoveActionBinding(BackBinding); BackBinding = FUIActionBindingHandle(); }
	if (RouteOwner) RouteOwner->HandleWidgetRemoved();
	Super::NativeOnDeactivated();
}
void UAZ_MenuRouteWidget::NativeDestruct()
{
	if (BackPrompt) BackPrompt->ClearPrompt();
	if (OverlayBackPrompt) OverlayBackPrompt->ClearPrompt();
	if (BackBinding.IsValid()) { BackBinding.Unregister(); RemoveActionBinding(BackBinding); BackBinding = FUIActionBindingHandle(); }
	if (RouteOwner) RouteOwner->HandleWidgetRemoved();
	Super::NativeDestruct();
}
UWidget* UAZ_MenuRouteWidget::NativeGetDesiredFocusTarget() const { return DefaultFocus ? DefaultFocus.Get() : Super::NativeGetDesiredFocusTarget(); }
void UAZ_MenuRouteWidget::FocusDefaultControl() { if (DefaultFocus && IsActivated()) DefaultFocus->SetUserFocus(GetOwningPlayer()); }
TOptional<FUIInputConfig> UAZ_MenuRouteWidget::GetDesiredInputConfig() const
{
	// PC capture owns move/look suppression; retain the normal held/release dispatch lane.
	return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture, EMouseLockMode::DoNotLock, false);
}
void UAZ_MenuRouteWidget::HandleBack() { if (RouteOwner) RouteOwner->Back(); }
void UAZ_MenuRouteWidget::HandleCommand(EAZ_MenuCommand Command) { if (RouteOwner) RouteOwner->Execute(Command); }

void UAZ_MenuRouteWidget::AddText(const FText& Text, bool bMuted)
{
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
	Label->SetText(Text); Label->SetAutoWrapText(true);
	Label->SetFont(BodyFont.FontObject ? BodyFont : FCoreStyle::GetDefaultFontStyle("Regular", 18));
	Label->SetColorAndOpacity(bOverlay ? OverlayTextColor : bMuted ? MutedColor : TextColor);
	Body->AddChildToVerticalBox(Label)->SetPadding(FMargin(12, 0, 12, 26));
}
UAZ_MenuCommandButton* UAZ_MenuRouteWidget::AddCommand(const FText& Text, EAZ_MenuCommand Command, bool bEnabled, bool bDefault)
{
	UAZ_MenuCommandButton* Button = WidgetTree->ConstructWidget<UAZ_MenuCommandButton>();
	Button->Configure(Command); Button->SetIsEnabled(bEnabled);
	Button->SetStyle(bOverlay ? OverlayButtonStyle : PaperButtonStyle);
	Button->OnCommand.AddUniqueDynamic(this, &ThisClass::HandleCommand);
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
	Label->SetText(Text); Label->SetFont(HeadingFont.FontObject ? HeadingFont : FCoreStyle::GetDefaultFontStyle("Bold", 20));
	Label->SetColorAndOpacity(bOverlay ? OverlayTextColor : TextColor);
	Button->AddChild(Label);
	if (auto* LabelSlot = Cast<UButtonSlot>(Label->Slot)) { LabelSlot->SetPadding(FMargin(24, 14)); LabelSlot->SetHorizontalAlignment(HAlign_Left); }
	Body->AddChildToVerticalBox(Button)->SetPadding(FMargin(0, 0, 0, 12));
	if (bEnabled && (!DefaultFocus || bDefault)) DefaultFocus = Button;
	return Button;
}
UWidget* UAZ_MenuRouteWidget::MakeChoiceLabel(FString Item)
{
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
	Label->SetText(FText::FromString(Item));
	Label->SetFont(BodyFont.FontObject ? BodyFont : FCoreStyle::GetDefaultFontStyle("Regular", 18));
	Label->SetColorAndOpacity(TextColor);
	return Label;
}
UComboBoxString* UAZ_MenuRouteWidget::AddChoice(const FText& LabelText, const TArray<FString>& Choices, int32 Selected)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	UTextBlock* Label = CastChecked<UTextBlock>(MakeChoiceLabel(LabelText.ToString()));
	auto* LabelSlot = Row->AddChildToHorizontalBox(Label); LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LabelSlot->SetVerticalAlignment(VAlign_Center);
	UComboBoxString* Box = WidgetTree->ConstructWidget<UComboBoxString>();
	// Popup brushes must be opaque: translucent paper lets the settings below bleed through.
	FComboBoxStyle ChoiceStyle = PaperComboStyle;
	FLinearColor PopupColor = PaperColor;
	PopupColor.A = 1.0f;
	ChoiceStyle.ComboButtonStyle.SetMenuBorderBrush(FSlateColorBrush(PopupColor));
	ChoiceStyle.ComboButtonStyle.SetMenuBorderPadding(FMargin(6));
	ChoiceStyle.SetMenuRowPadding(FMargin(12, 8));
	Box->SetWidgetStyle(ChoiceStyle); Box->SetItemStyle(PaperComboRowStyle);
	// Keep long resolution lists compact; Slate scrolls the remaining options into view.
	Box->SetMaxListHeight(240.0f);
	Box->SetContentPadding(FMargin(14, 10)); Box->SetEnableGamepadNavigationMode(true);
	Box->OnGenerateWidgetEvent.BindDynamic(this, &ThisClass::MakeChoiceLabel);
	for (const FString& Value : Choices) Box->AddOption(Value);
	if (Choices.IsValidIndex(Selected)) Box->SetSelectedOption(Choices[Selected]);
	Box->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleSettingChanged);
	Row->AddChildToHorizontalBox(Box)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Body->AddChildToVerticalBox(Row)->SetPadding(FMargin(12, 0, 12, 18));
	if (!DefaultFocus) DefaultFocus = Box;
	return Box;
}

void UAZ_MenuRouteWidget::BuildSettings()
{
	const FAZ_MenuSettingsDraft& Draft = RouteOwner->PendingSettings;
	AddText(LOCTEXT("SettingsPolicy", "Changes remain pending until Apply. Display changes need confirmation within 15 seconds."), true);
	ModeBox = AddChoice(LOCTEXT("Mode", "Display mode"), {TEXT("Windowed"), TEXT("Borderless"), TEXT("Fullscreen")}, static_cast<int32>(Draft.DisplayMode));
	ResolutionValues = RouteOwner->GetResolutions();
	TArray<FString> Resolutions;
	for (const FIntPoint Size : ResolutionValues) Resolutions.Add(FString::Printf(TEXT("%d × %d"), Size.X, Size.Y));
	ResolutionBox = AddChoice(LOCTEXT("Resolution", "Resolution"), Resolutions, ResolutionValues.IndexOfByKey(Draft.Resolution));
	VSyncBox = AddChoice(LOCTEXT("VSync", "Vertical sync"), {TEXT("Off"), TEXT("On")}, Draft.bVSync ? 1 : 0);
	QualityBox = AddChoice(LOCTEXT("Quality", "Quality preset"), {TEXT("Custom"), TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic")}, Draft.Quality + 1);
	FrameRateValues = {0, 30, 60, 120, 144}; FrameRateValues.AddUnique(Draft.FrameRateLimit);
	TArray<FString> FrameRates;
	for (const float Rate : FrameRateValues) FrameRates.Add(Rate <= 0 ? TEXT("Unlimited") : FString::Printf(TEXT("%g FPS"), Rate));
	FrameRateBox = AddChoice(LOCTEXT("FrameRate", "Frame limit"), FrameRates, FrameRateValues.IndexOfByKey(Draft.FrameRateLimit));
	if (RouteOwner->CanEditMasterVolume())
	{
		MasterVolumeValues.Reset(); TArray<FString> Volumes;
		for (int32 Percent = 0; Percent <= 100; Percent += 5) MasterVolumeValues.Add(Percent / 100.f);
		MasterVolumeValues.AddUnique(Draft.MasterVolume); MasterVolumeValues.Sort();
		for (float Value : MasterVolumeValues) Volumes.Add(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.f)));
		MasterVolumeBox = AddChoice(LOCTEXT("MasterVolume", "Master volume"), Volumes, MasterVolumeValues.IndexOfByKey(Draft.MasterVolume));
	}
	else AddText(LOCTEXT("AudioUnavailable", "Master audio is unavailable."), true);
	FamilyValues.Reset(); TArray<FString> Families;
	const TCHAR* Names[] = {TEXT("Automatic"), TEXT("Xbox"), TEXT("PlayStation 4"), TEXT("PlayStation 5")};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const auto Family = static_cast<EAZ_GamepadGlyphPreference>(Index);
		if (RouteOwner->IsGlyphFamilyAvailable(Family)) { FamilyValues.Add(Family); Families.Add(Names[Index]); }
	}
	FamilyBox = AddChoice(LOCTEXT("Family", "Controller button appearance"), Families, FamilyValues.IndexOfByKey(Draft.GlyphFamily));
	AddCommand(LOCTEXT("Apply", "Apply"), EAZ_MenuCommand::Apply);
	AddCommand(LOCTEXT("Reset", "Reset to defaults"), EAZ_MenuCommand::Reset);
	AddCommand(LOCTEXT("CancelSettings", "Cancel / Back"), EAZ_MenuCommand::Cancel);
}
void UAZ_MenuRouteWidget::HandleSettingChanged(FString, ESelectInfo::Type)
{
	if (bRefreshing || !RouteOwner || !ModeBox || !ResolutionBox || !VSyncBox || !QualityBox || !FamilyBox || !FrameRateBox) return;
	FAZ_MenuSettingsDraft Draft = RouteOwner->PendingSettings;
	Draft.DisplayMode = static_cast<EAZ_MenuDisplayMode>(ModeBox->GetSelectedIndex());
	if (ResolutionValues.IsValidIndex(ResolutionBox->GetSelectedIndex())) Draft.Resolution = ResolutionValues[ResolutionBox->GetSelectedIndex()];
	Draft.bVSync = VSyncBox->GetSelectedIndex() == 1;
	Draft.Quality = QualityBox->GetSelectedIndex() - 1;
	if (FrameRateValues.IsValidIndex(FrameRateBox->GetSelectedIndex())) Draft.FrameRateLimit = FrameRateValues[FrameRateBox->GetSelectedIndex()];
	if (MasterVolumeBox && MasterVolumeValues.IsValidIndex(MasterVolumeBox->GetSelectedIndex())) Draft.MasterVolume = MasterVolumeValues[MasterVolumeBox->GetSelectedIndex()];
	if (FamilyValues.IsValidIndex(FamilyBox->GetSelectedIndex())) Draft.GlyphFamily = FamilyValues[FamilyBox->GetSelectedIndex()];
	RouteOwner->SetSettingsDraft(Draft);
}

void UAZ_MenuRouteWidget::RefreshRoute()
{
	if (!RouteOwner || !Body || !Heading || !Background) return;
	TGuardValue<bool> Guard(bRefreshing, true);
	DefaultFocus = nullptr;
	ModeBox = ResolutionBox = VSyncBox = QualityBox = FamilyBox = FrameRateBox = MasterVolumeBox = nullptr;
	Body->ClearChildren();
	const EAZ_MenuRoute Route = RouteOwner->Route;
	bOverlay = Route == EAZ_MenuRoute::Title || Route == EAZ_MenuRoute::Loading;
	const bool bTitle = Route == EAZ_MenuRoute::Title;
	const bool bConfirmation = Route == EAZ_MenuRoute::ConfirmLoad || Route == EAZ_MenuRoute::ConfirmNewGame ||
		Route == EAZ_MenuRoute::ConfirmTitle || Route == EAZ_MenuRoute::ConfirmQuit || Route == EAZ_MenuRoute::ConfirmDisplay;
	BrandSlot->SetOffsets(bTitle ? TitleBrandPlacement : PaperBrandPlacement);
	HeadingSlot->SetOffsets(bTitle ? TitleHeadingPlacement : PaperHeadingPlacement);
	BodySlot->SetOffsets(bTitle ? TitleBodyInsets : PaperBodyInsets);
	ContentWidth->SetWidthOverride(Route == EAZ_MenuRoute::Settings ? SettingsColumnWidth : bConfirmation ? ConfirmationColumnWidth : MenuColumnWidth);
	UAZ_ActionPrompt* ActivePrompt = bOverlay ? OverlayBackPrompt.Get() : BackPrompt.Get();
	for (UAZ_ActionPrompt* Prompt : {BackPrompt.Get(), OverlayBackPrompt.Get()})
	{
		if (!Prompt) continue;
		const bool bCanBack = (Route != EAZ_MenuRoute::Title || RouteOwner->CanReturnFromTitle()) && Route != EAZ_MenuRoute::Loading && !RouteOwner->IsOperationPending();
		const FText Description = Route == EAZ_MenuRoute::Pause ? LOCTEXT("Resume", "Resume") : Route == EAZ_MenuRoute::ConfirmDisplay ? LOCTEXT("Revert", "Revert") : LOCTEXT("Back", "Back");
		const bool bShowPrompt = bCanBack && Prompt == ActivePrompt;
		Prompt->SetVisibility(bShowPrompt ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bShowPrompt) Prompt->ConfigureBinding(BackBinding, Description, bOverlay ? TEXT("Overlay") : TEXT("Paper"));
		else Prompt->ClearPrompt();
	}
	Background->SetBrushColor(bOverlay ? OverlayColor : PaperColor);
	const FSlateFontInfo& AppliedBrandFont = bTitle ? TitleBrandFont : BrandFont;
	Brand->SetFont(AppliedBrandFont.FontObject ? AppliedBrandFont : FCoreStyle::GetDefaultFontStyle("Bold", bTitle ? 105 : 34));
	Brand->SetColorAndOpacity(bOverlay ? OverlayTextColor : TextColor);
	Heading->SetFont(HeadingFont.FontObject ? HeadingFont : FCoreStyle::GetDefaultFontStyle("Bold", 22));
	Heading->SetColorAndOpacity(bOverlay ? OverlayTextColor : TextColor);
	Status->SetFont(BodyFont.FontObject ? BodyFont : FCoreStyle::GetDefaultFontStyle("Regular", 16));
	Status->SetColorAndOpacity(bOverlay ? OverlayTextColor : MutedColor);
	Status->SetText(RouteOwner->Message);
	LastDisplaySeconds = INDEX_NONE;
	if (Route == EAZ_MenuRoute::Pause)
	{
		Heading->SetText(LOCTEXT("Paused", "PAUSED"));
		AddText(LOCTEXT("Moment", "Take a moment."));
		AddCommand(LOCTEXT("Resume", "Resume"), EAZ_MenuCommand::Resume);
		AddCommand(LOCTEXT("Inventory", "Inventory"), EAZ_MenuCommand::Inventory, RouteOwner->CanOpenInventory());
		AddCommand(LOCTEXT("Map", "Map & journal"), EAZ_MenuCommand::Map, RouteOwner->CanOpenInventory());
		AddCommand(LOCTEXT("Load", "Load checkpoint"), EAZ_MenuCommand::LoadCheckpoint);
		AddCommand(LOCTEXT("Settings", "Settings"), EAZ_MenuCommand::Settings);
		AddCommand(LOCTEXT("MainMenu", "Main menu"), EAZ_MenuCommand::Title);
	}
	else if (Route == EAZ_MenuRoute::Title)
	{
		Heading->SetText(LOCTEXT("LocalCampaign", "LOCAL CAMPAIGN"));
		AddCommand(LOCTEXT("Continue", "Continue"), EAZ_MenuCommand::Continue, RouteOwner->CanLoadCheckpoint());
		AddCommand(LOCTEXT("NewGame", "New game"), EAZ_MenuCommand::NewGame);
		AddCommand(LOCTEXT("Load", "Load checkpoint"), EAZ_MenuCommand::LoadCheckpoint);
		AddCommand(LOCTEXT("Settings", "Settings"), EAZ_MenuCommand::Settings);
		AddCommand(LOCTEXT("Exit", "Exit"), EAZ_MenuCommand::Quit);
		if (RouteOwner->CanReturnFromTitle()) AddCommand(LOCTEXT("BackToPause", "Back to paused campaign"), EAZ_MenuCommand::Cancel);
	}
	else if (Route == EAZ_MenuRoute::Settings) { Heading->SetText(LOCTEXT("SettingsTitle", "SETTINGS")); BuildSettings(); }
	else if (Route == EAZ_MenuRoute::LoadCheckpoint)
	{
		Heading->SetText(LOCTEXT("LoadTitle", "LOAD CHECKPOINT"));
		AddText(LOCTEXT("LoadPolicy", "Return to the latest checkpoint available in the current local campaign. Manual saving requires a save point."));
		AddCommand(LOCTEXT("LatestCheckpoint", "Load latest checkpoint"), EAZ_MenuCommand::Continue, RouteOwner->CanLoadCheckpoint());
		AddCommand(LOCTEXT("Back", "Back"), EAZ_MenuCommand::Cancel);
	}
	else if (Route == EAZ_MenuRoute::Loading)
	{
		Heading->SetText(LOCTEXT("Loading", "LOADING"));
		AddText(RouteOwner->Message);
		Status->SetText(FText::GetEmpty()); // No invented progress percentage.
	}
	else if (Route == EAZ_MenuRoute::LoadFailure)
	{
		Heading->SetText(LOCTEXT("LoadFailed", "LOAD UNAVAILABLE"));
		AddText(RouteOwner->Message);
		Status->SetText(FText::GetEmpty());
		AddCommand(LOCTEXT("Back", "Back"), EAZ_MenuCommand::Cancel, !RouteOwner->IsOperationPending());
	}
	else
	{
		Heading->SetText(LOCTEXT("Confirm", "CONFIRMATION"));
		if (Route == EAZ_MenuRoute::ConfirmLoad) AddText(LOCTEXT("LoadWarning", "Load this checkpoint? Progress since the checkpoint will be replaced."));
		else if (Route == EAZ_MenuRoute::ConfirmNewGame) AddText(LOCTEXT("NewWarning", "Start a new campaign? This discards the current unsaved run and returns to the starting area. Your previous checkpoint stays available until the new campaign successfully saves or autosaves."));
		else if (Route == EAZ_MenuRoute::ConfirmTitle) AddText(LOCTEXT("TitleWarning", "Return to the main menu? This does not save progress. Continue will restore your last checkpoint."));
		else if (Route == EAZ_MenuRoute::ConfirmQuit) AddText(LOCTEXT("QuitWarning", "Exit the game? Progress since the last successful checkpoint is not saved."));
		else if (Route == EAZ_MenuRoute::ConfirmDisplay) AddText(LOCTEXT("DisplayWarning", "Keep these display settings? They revert automatically after 15 seconds, or when the application loses focus."));
		AddCommand(Route == EAZ_MenuRoute::ConfirmDisplay ? LOCTEXT("Revert", "Revert") : LOCTEXT("Cancel", "Cancel"), EAZ_MenuCommand::Cancel, true, true);
		AddCommand(Route == EAZ_MenuRoute::ConfirmDisplay ? LOCTEXT("Keep", "Keep settings") : LOCTEXT("ConfirmButton", "Confirm"), EAZ_MenuCommand::Confirm);
		RefreshDisplayCountdown();
	}
	if (DefaultFocus && IsActivated()) DefaultFocus->SetUserFocus(GetOwningPlayer());
}
void UAZ_MenuRouteWidget::RefreshDisplayCountdown()
{
	if (!Status || !RouteOwner || RouteOwner->Route != EAZ_MenuRoute::ConfirmDisplay) return;
	const int32 Seconds = RouteOwner->GetDisplayConfirmSeconds();
	if (Seconds == LastDisplaySeconds) return;
	LastDisplaySeconds = Seconds;
	Status->SetText(FText::Format(LOCTEXT("RevertSeconds", "Reverting in {0} seconds"), FText::AsNumber(Seconds)));
}

#undef LOCTEXT_NAMESPACE
