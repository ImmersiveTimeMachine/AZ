#include "UI/AZ_MenuRoutesComponent.h"
#include "UI/AZ_MenuRouteWidget.h"
#include "UI/AZ_InputPresentationSubsystem.h"
#include "Player/AZ_PlayerController.h"
#include "Game/AZ_CampaignSaveCoordinator.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_GameInventoryMenu.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/PackageName.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "TimerManager.h"

namespace
{
	EWindowMode::Type ToWindowMode(EAZ_MenuDisplayMode Mode)
	{
		return Mode == EAZ_MenuDisplayMode::Fullscreen ? EWindowMode::Fullscreen :
			Mode == EAZ_MenuDisplayMode::Windowed ? EWindowMode::Windowed : EWindowMode::WindowedFullscreen;
	}
	EAZ_MenuDisplayMode FromWindowMode(EWindowMode::Type Mode)
	{
		return Mode == EWindowMode::Fullscreen ? EAZ_MenuDisplayMode::Fullscreen :
			Mode == EWindowMode::Windowed ? EAZ_MenuDisplayMode::Windowed : EAZ_MenuDisplayMode::Borderless;
	}
}

UAZ_MenuRoutesComponent::UAZ_MenuRoutesComponent() { PrimaryComponentTick.bCanEverTick = false; }
AAZ_PlayerController* UAZ_MenuRoutesComponent::Controller() const { return Cast<AAZ_PlayerController>(GetOwner()); }
UGameUserSettings* UAZ_MenuRoutesComponent::Settings() const { return GEngine ? GEngine->GetGameUserSettings() : nullptr; }
UAZ_InputPresentationSubsystem* UAZ_MenuRoutesComponent::Preferences() const
{
	const auto* PC = Controller();
	return PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UAZ_InputPresentationSubsystem>() : nullptr;
}
bool UAZ_MenuRoutesComponent::CanOwnMenus() const
{
	const auto* PC = Controller();
	return !bEndingPlay && PC && PC->IsLocalController() && PC->HasAuthority() && GetWorld()
		&& GetWorld()->IsGameWorld() && GetWorld()->GetNetMode() == NM_Standalone && GetWorld()->GetNumPlayerControllers() == 1;
}

void UAZ_MenuRoutesComponent::InitializeForLocalPlayer()
{
	if (!CanOwnMenus()) return;
	if (auto* P = Preferences())
	{
		P->OnMasterVolumeChanged.AddUniqueDynamic(this, &ThisClass::HandleMasterVolumeChanged);
		ApplyMasterVolume(P->GetMasterVolume());
	}
	if (bShowTitleOnStartup && !GetWorld()->URL.HasOption(TEXT("AZSkipTitle="))) Show(EAZ_MenuRoute::Title);
}

void UAZ_MenuRoutesComponent::HandlePauseAction()
{
	if (!CanOwnMenus() || LastNavigationFrame == GFrameCounter) return;
	if (IsOpen()) { Back(); return; }
	// Inventory/Map and Quick Select retain their own Back/cancel ordering.
	if (Controller()->IsInventoryInputCaptured()) return;
	LastNavigationFrame = GFrameCounter;
	Message = FText::GetEmpty();
	Show(EAZ_MenuRoute::Pause);
}

void UAZ_MenuRoutesComponent::AcquirePause()
{
	if (!bOwnsPause && Controller() && GetWorld() && !GetWorld()->IsPaused()) bOwnsPause = Controller()->SetPause(true);
}
bool UAZ_MenuRoutesComponent::ReleasePause()
{
	if (bOwnsPause && Controller())
	{
		if (!Controller()->SetPause(false)) return false;
		bOwnsPause = false;
	}
	return true;
}

bool UAZ_MenuRoutesComponent::Show(EAZ_MenuRoute Next)
{
	if (!CanOwnMenus() || Next == EAZ_MenuRoute::Closed) return false;
	if (!IsOpen() && Controller()->IsInventoryInputCaptured()) return false;
	if (!Widget)
	{
		const TSubclassOf<UAZ_MenuRouteWidget> Class = MenuWidgetClass ? MenuWidgetClass.Get() : UAZ_MenuRouteWidget::StaticClass();
		Widget = CreateWidget<UAZ_MenuRouteWidget>(Controller(), Class);
		if (!Widget) return false;
		Widget->SetRouteOwner(this);
	}
	Controller()->SetMenuRouteInputCaptured(true);
	Route = Next;
	if (Next != EAZ_MenuRoute::Loading && !(Controller()->CampaignSave && Controller()->CampaignSave->IsBusy())) AcquirePause();
	if (!Widget->IsInViewport()) Widget->AddToViewport(200);
	Widget->ActivateWidget();
	Widget->RefreshRoute();
	RestoreMenuInput();
	return true;
}
void UAZ_MenuRoutesComponent::RestoreMenuInput()
{
	if (!Widget || !Controller()) return;
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(Widget->TakeWidget());
	Mode.SetHideCursorDuringCapture(false);
	Controller()->SetInputMode(Mode);
	Controller()->SetShowMouseCursor(true);
	Widget->FocusDefaultControl();
}

void UAZ_MenuRoutesComponent::Close()
{
	if (bWaitingForLoad || (Controller() && Controller()->CampaignSave && Controller()->CampaignSave->IsBusy())) return;
	if (!ReleasePause()) { Message = NSLOCTEXT("CHALK", "MenuUnpauseFailed", "The campaign could not resume."); if (Widget) Widget->RefreshRoute(); return; }
	bClosingWidget = true;
	CancelSettings();
	Route = EAZ_MenuRoute::Closed;
	if (Widget) { Widget->DeactivateWidget(); Widget->RemoveFromParent(); Widget = nullptr; }
	if (Controller())
	{
		Controller()->SetMenuRouteInputCaptured(false);
		if (!Controller()->IsGameplayInputCaptured())
		{
			Controller()->SetInputMode(FInputModeGameOnly());
			Controller()->SetShowMouseCursor(false);
		}
	}
	bClosingWidget = false;
}

void UAZ_MenuRoutesComponent::Back()
{
	if (!IsOpen() || LastNavigationFrame == GFrameCounter || bWaitingForLoad || Route == EAZ_MenuRoute::Loading) return;
	LastNavigationFrame = GFrameCounter;
	if (Controller()->CampaignSave && Controller()->CampaignSave->IsBusy()) return;
	if (Route == EAZ_MenuRoute::ConfirmDisplay) { RevertDisplay(); return; }
	if (Route == EAZ_MenuRoute::Settings) { CancelSettings(); Message = FText::GetEmpty(); Show(ReturnRoute); return; }
	if (Route == EAZ_MenuRoute::Pause) { Close(); return; }
	if (Route == EAZ_MenuRoute::Title)
	{
		if (bTitleEnteredFromPause) { bTitleEnteredFromPause = false; Message = FText::GetEmpty(); Show(EAZ_MenuRoute::Pause); }
		return;
	}
	Message = FText::GetEmpty();
	Show(ReturnRoute);
}

bool UAZ_MenuRoutesComponent::CanLoadCheckpoint() const
{
	const auto* PC = Controller();
	return CanOwnMenus() && PC->CampaignSave && !PC->CampaignSave->IsBusy() && PC->CampaignSave->HasCampaignSave();
}
bool UAZ_MenuRoutesComponent::CanOpenInventory() const
{
	return Controller() && Controller()->CanOpenInventoryFromMenuRoute();
}

void UAZ_MenuRoutesComponent::Execute(EAZ_MenuCommand Command)
{
	if (!CanOwnMenus() || !IsOpen()) return;
	if (bWaitingForLoad || bTravelRequested || Route == EAZ_MenuRoute::Loading || (Controller()->CampaignSave && Controller()->CampaignSave->IsBusy())) return;
	if (Command == EAZ_MenuCommand::Cancel) { Back(); return; }
	if (Route == EAZ_MenuRoute::ConfirmDisplay)
	{
		if (Command == EAZ_MenuCommand::Confirm)
		{
			if (FPlatformTime::Seconds() >= DisplayDeadline) { RevertDisplay(); return; }
			StopDisplayRevert(); bDisplayPreview = false; if (Settings()) Settings()->ConfirmVideoMode(); CommitSettings();
		}
		return;
	}
	if (Command == EAZ_MenuCommand::Confirm)
	{
		if (Route == EAZ_MenuRoute::ConfirmLoad) StartLoad();
		else if (Route == EAZ_MenuRoute::ConfirmNewGame) StartNewGame();
		else if (Route == EAZ_MenuRoute::ConfirmTitle) { bTitleEnteredFromPause = true; Message = FText::GetEmpty(); Show(EAZ_MenuRoute::Title); }
		else if (Route == EAZ_MenuRoute::ConfirmQuit) UKismetSystemLibrary::QuitGame(this, Controller(), EQuitPreference::Quit, false);
		return;
	}
	if (Route == EAZ_MenuRoute::Settings)
	{
		if (Command == EAZ_MenuCommand::Apply) ApplySettings();
		else if (Command == EAZ_MenuCommand::Reset) ResetSettings();
		return;
	}
	Message = FText::GetEmpty();
	switch (Command)
	{
	case EAZ_MenuCommand::Resume: if (Route == EAZ_MenuRoute::Pause) Close(); break;
	case EAZ_MenuCommand::Inventory: if (Route == EAZ_MenuRoute::Pause) OpenInventory(false); break;
	case EAZ_MenuCommand::Map: if (Route == EAZ_MenuRoute::Pause) OpenInventory(true); break;
	case EAZ_MenuCommand::Settings: ReturnRoute = Route; BeginSettings(); break;
	case EAZ_MenuCommand::LoadCheckpoint: ReturnRoute = Route; Show(EAZ_MenuRoute::LoadCheckpoint); break;
	case EAZ_MenuCommand::Continue: if (CanLoadCheckpoint()) { if (Route != EAZ_MenuRoute::LoadCheckpoint) ReturnRoute = Route; Show(EAZ_MenuRoute::ConfirmLoad); } break;
	case EAZ_MenuCommand::NewGame: ReturnRoute = Route; Show(EAZ_MenuRoute::ConfirmNewGame); break;
	case EAZ_MenuCommand::Title: ReturnRoute = Route; Show(EAZ_MenuRoute::ConfirmTitle); break;
	case EAZ_MenuCommand::Quit: ReturnRoute = Route; Show(EAZ_MenuRoute::ConfirmQuit); break;
	default: break;
	}
}

void UAZ_MenuRoutesComponent::OpenInventory(bool bMap)
{
	auto* Inventory = Controller()->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>();
	if (!CanOpenInventory() || !Inventory || !Inventory->GetInventoryMenu() || !ReleasePause())
	{ Message = NSLOCTEXT("CHALK", "MenuInventoryUnavailable", "Inventory is not ready."); Widget->RefreshRoute(); return; }
	// Transfer capture before releasing this owner; no combat/input gap.
	if (!Inventory->IsMenuOpen()) Inventory->ToggleInventoryMenu();
	if (!Inventory->IsMenuOpen()) { AcquirePause(); return; }
	Close();
	UAZ_Inv_CommonUI_GameInventoryMenu* Menu = Inventory->GetInventoryMenu();
	if (bMap)
	{
		Menu->OpenMapPage();
		// Preserve the activated Map page's desired MapCanvas focus; do not steal it for the parent.
		if (UCommonUIActionRouterBase* Router = UCommonUIActionRouterBase::Get(*Menu)) Router->RefreshActiveRootFocus();
	}
	else Menu->SetUserFocus(Controller());
}

void UAZ_MenuRoutesComponent::StartLoad()
{
	auto* Save = Controller()->CampaignSave.Get();
	if (!Save || !CanLoadCheckpoint()) return;
	if (!ReleasePause() || GetWorld()->IsPaused())
	{ Message = NSLOCTEXT("CHALK", "MenuOtherPause", "Another pause owner must resume before a checkpoint can load."); Show(EAZ_MenuRoute::LoadFailure); return; }
	Save->OnLoadCompleted.AddUniqueDynamic(this, &ThisClass::HandleLoadCompleted);
	bWaitingForLoad = true;
	Message = NSLOCTEXT("CHALK", "RestoringCampaign", "Restoring checkpoint…");
	Show(EAZ_MenuRoute::Loading);
	// Permit one running-world frame after startup/menu pause. This is a one-shot
	// request boundary, not a progress timer or repeated disk-load attempt.
	DeferredLoadRequest = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RequestLoadAfterResume);
}

void UAZ_MenuRoutesComponent::RequestLoadAfterResume()
{
	if (!bWaitingForLoad || bEndingPlay) return;
	auto* Save = Controller() ? Controller()->CampaignSave.Get() : nullptr;
	FString Error;
	if (!Save || !Save->LoadCampaign(Error))
	{
		if (Save) Save->OnLoadCompleted.RemoveDynamic(this, &ThisClass::HandleLoadCompleted);
		bWaitingForLoad = false;
		Message = Error.IsEmpty() ? NSLOCTEXT("CHALK", "CampaignUnavailable", "The campaign is not ready to load.") : FText::FromString(Error);
		Show(EAZ_MenuRoute::LoadFailure);
	}
}
void UAZ_MenuRoutesComponent::HandleLoadCompleted(bool bSuccess, const FString& Error)
{
	if (!bWaitingForLoad || bEndingPlay) return;
	auto* Save = Controller() ? Controller()->CampaignSave.Get() : nullptr;
	if (Save && Save->IsBusy())
	{
		// Rollback-blocked failure is deliberately still busy: retain input and callback.
		Message = FText::FromString(Error); Show(EAZ_MenuRoute::LoadFailure); return;
	}
	if (Save) Save->OnLoadCompleted.RemoveDynamic(this, &ThisClass::HandleLoadCompleted);
	bWaitingForLoad = false;
	if (bSuccess) Close();
	else { Message = FText::FromString(Error); Show(EAZ_MenuRoute::LoadFailure); }
}

void UAZ_MenuRoutesComponent::StartNewGame()
{
	FString Map;
	if (GConfig) GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameDefaultMap"), Map, GEngineIni);
	Map = FSoftObjectPath(Map).GetLongPackageName();
	if (!FPackageName::IsValidLongPackageName(Map) || !FPackageName::DoesPackageExist(Map))
	{ Message = NSLOCTEXT("CHALK", "StartupMapUnavailable", "The configured starting area is unavailable."); Show(EAZ_MenuRoute::LoadFailure); return; }
	if (!ReleasePause() || GetWorld()->IsPaused())
	{ Message = NSLOCTEXT("CHALK", "MenuOtherPause", "Another pause owner must resume before a checkpoint can load."); Show(EAZ_MenuRoute::LoadFailure); return; }
	Message = NSLOCTEXT("CHALK", "StartingCampaign", "Starting a new campaign…");
	Show(EAZ_MenuRoute::Loading);
	bTravelRequested = true;
	if (GEngine) TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
	// No save deletion. The next successful checkpoint/autosave replaces the previous generation.
	UGameplayStatics::OpenLevel(this, FName(*Map), true, TEXT("AZSkipTitle=1"));
}

void UAZ_MenuRoutesComponent::HandleTravelFailure(UWorld* World, ETravelFailure::Type, const FString& Error)
{
	if (!bTravelRequested || bEndingPlay || World != GetWorld()) return;
	bTravelRequested = false;
	if (GEngine && TravelFailureHandle.IsValid()) GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	TravelFailureHandle.Reset();
	Message = FText::FromString(Error); Show(EAZ_MenuRoute::LoadFailure);
}

void UAZ_MenuRoutesComponent::ReadSettings()
{
	if (const auto* S = Settings())
	{
		OriginalSettings.Resolution = S->GetScreenResolution();
		OriginalSettings.DisplayMode = FromWindowMode(S->GetFullscreenMode());
		OriginalSettings.bVSync = S->IsVSyncEnabled();
		OriginalSettings.Quality = S->GetOverallScalabilityLevel();
		OriginalSettings.FrameRateLimit = S->GetFrameRateLimit();
	}
	if (const auto* P = Preferences())
	{
		OriginalSettings.GlyphFamily = P->GetGamepadGlyphPreference();
		OriginalSettings.MasterVolume = P->GetMasterVolume();
	}
	PendingSettings = OriginalSettings;
	if (!IsGlyphFamilyAvailable(PendingSettings.GlyphFamily)) PendingSettings.GlyphFamily = EAZ_GamepadGlyphPreference::Auto;
}
void UAZ_MenuRoutesComponent::BeginSettings()
{
	if (!Settings()) { Message = NSLOCTEXT("CHALK", "SettingsUnavailable", "Settings are unavailable."); if (Widget) Widget->RefreshRoute(); return; }
	ReadSettings(); bSettingsSession = true;
	if (auto* P = Preferences()) P->OnPreferenceChanged.AddUniqueDynamic(this, &ThisClass::HandlePreferenceChanged);
	ApplyMasterVolume(OriginalSettings.MasterVolume); // Also handles an audio device that became ready after PC BeginPlay.
	Show(EAZ_MenuRoute::Settings);
}

void UAZ_MenuRoutesComponent::HandlePreferenceChanged(EAZ_GamepadGlyphPreference Preference, bool bSaved, FText Error)
{
	if (!bSettingsSession || bEndingPlay) return;
	const bool bPendingUnchanged = PendingSettings.GlyphFamily == OriginalSettings.GlyphFamily;
	const bool bPreferenceChanged = OriginalSettings.GlyphFamily != Preference;
	OriginalSettings.GlyphFamily = Preference;
	if (bPendingUnchanged) PendingSettings.GlyphFamily = IsGlyphFamilyAvailable(Preference) ? Preference : EAZ_GamepadGlyphPreference::Auto;
	if (!bSaved && !Error.IsEmpty()) Message = Error;
	if (bPreferenceChanged && Route == EAZ_MenuRoute::Settings && Widget) Widget->RefreshRoute();
}
void UAZ_MenuRoutesComponent::SetSettingsDraft(const FAZ_MenuSettingsDraft& Draft)
{
	if (Route != EAZ_MenuRoute::Settings || !bSettingsSession || bDisplayPreview) return;
	if (Draft.Resolution.X < 320 || Draft.Resolution.Y < 200 || static_cast<uint8>(Draft.DisplayMode) > 2 ||
		Draft.Quality < -1 || Draft.Quality > 4 || !FMath::IsFinite(Draft.FrameRateLimit) || Draft.FrameRateLimit < 0 || Draft.FrameRateLimit > 1000 ||
		!FMath::IsFinite(Draft.MasterVolume) || Draft.MasterVolume < 0 || Draft.MasterVolume > 1) return;
	if (!IsGlyphFamilyAvailable(Draft.GlyphFamily)) return;
	PendingSettings = Draft;
}
bool UAZ_MenuRoutesComponent::IsGlyphFamilyAvailable(EAZ_GamepadGlyphPreference Family) const
{
	const auto* P = Preferences();
	return P ? P->IsGamepadGlyphPreferenceAvailable(Family) : Family == EAZ_GamepadGlyphPreference::Auto;
}
bool UAZ_MenuRoutesComponent::CanEditMasterVolume() const
{
	return Preferences() && GEngine && GEngine->UseSound() && GetWorld() && GetWorld()->bAllowAudioPlayback &&
		GetWorld()->GetAudioDeviceRaw() && GetDefault<UAudioSettings>()->GetDefaultSoundClass();
}
void UAZ_MenuRoutesComponent::ApplyMasterVolume(float Volume)
{
	if (!CanEditMasterVolume() || !FMath::IsFinite(Volume)) return;
	USoundClass* ConfiguredMaster = GetDefault<UAudioSettings>()->GetDefaultSoundClass();
	if (MasterClass && MasterClass != ConfiguredMaster) ReleaseMasterMix();
	MasterClass = ConfiguredMaster;
	if (!MasterMix)
	{
		MasterMix = NewObject<USoundMix>(this, NAME_None, RF_Transient);
		MasterMix->bApplyEQ = false;
		MasterMix->InitialDelay = MasterMix->FadeInTime = MasterMix->FadeOutTime = 0.f;
		MasterMix->Duration = -1.f;
		FSoundClassAdjuster Adjuster;
		Adjuster.SoundClassObject = MasterClass;
		Adjuster.bApplyToChildren = true;
		MasterMix->SoundClassEffects.Add(Adjuster);
	}
	UGameplayStatics::SetSoundMixClassOverride(this, MasterMix, MasterClass, FMath::Clamp(Volume, 0.f, 1.f), 1.f, 0.05f, true);
	if (!bMasterMixPushed) { UGameplayStatics::PushSoundMixModifier(this, MasterMix); bMasterMixPushed = true; }
}
void UAZ_MenuRoutesComponent::ReleaseMasterMix()
{
	if (MasterMix)
	{
		if (MasterClass) UGameplayStatics::ClearSoundMixClassOverride(this, MasterMix, MasterClass, 0.f);
		if (bMasterMixPushed) UGameplayStatics::PopSoundMixModifier(this, MasterMix);
	}
	bMasterMixPushed = false; MasterMix = nullptr; MasterClass = nullptr;
}
void UAZ_MenuRoutesComponent::HandleMasterVolumeChanged(float Volume)
{
	if (bEndingPlay) return;
	ApplyMasterVolume(Volume);
	const bool bPendingUnchanged = FMath::IsNearlyEqual(PendingSettings.MasterVolume, OriginalSettings.MasterVolume);
	OriginalSettings.MasterVolume = Volume;
	if (bPendingUnchanged) PendingSettings.MasterVolume = Volume;
	if (bSettingsSession && Route == EAZ_MenuRoute::Settings && Widget) Widget->RefreshRoute();
}
TArray<FIntPoint> UAZ_MenuRoutesComponent::GetResolutions() const
{
	TArray<FIntPoint> Values, Windowed;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Values);
	UKismetSystemLibrary::GetConvenientWindowedResolutions(Windowed);
	for (const FIntPoint Value : Windowed) Values.AddUnique(Value);
	Values.AddUnique(PendingSettings.Resolution);
	Values.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
	return Values;
}

void UAZ_MenuRoutesComponent::ApplySettings()
{
	auto* S = Settings();
	if (!S || !bSettingsSession || bDisplayPreview) return;
	if (!IsGlyphFamilyAvailable(PendingSettings.GlyphFamily)) { Message = NSLOCTEXT("CHALK", "GlyphUnavailable", "That controller appearance is unavailable."); Widget->RefreshRoute(); return; }
	const bool bDisplayChanged = PendingSettings.Resolution != OriginalSettings.Resolution || PendingSettings.DisplayMode != OriginalSettings.DisplayMode;
	if (!bDisplayChanged) { CommitSettings(); return; }
	S->ConfirmVideoMode();
	S->SetScreenResolution(PendingSettings.Resolution);
	S->SetFullscreenMode(ToWindowMode(PendingSettings.DisplayMode));
	bDisplayPreview = true;
	DisplayDeadline = FPlatformTime::Seconds() + 15.0;
	DeactivateHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &ThisClass::HandleApplicationDeactivated);
	RevertTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickDisplayRevert), 0.1f);
	{
		TGuardValue<bool> ApplyingDisplay(bApplyingDisplayChange, true);
		S->ApplyResolutionSettings(false);
	}
	if (bDisplayPreview) Show(EAZ_MenuRoute::ConfirmDisplay);
}

void UAZ_MenuRoutesComponent::HandleApplicationDeactivated()
{
	// Window recreation may synchronously deactivate the old window during Apply.
	if (!bApplyingDisplayChange) RevertDisplay();
}
void UAZ_MenuRoutesComponent::CommitSettings()
{
	auto* S = Settings(); if (!S) return;
	const bool bAudioUnavailable = !FMath::IsNearlyEqual(PendingSettings.MasterVolume, OriginalSettings.MasterVolume) && !CanEditMasterVolume();
	const float VolumeToCommit = bAudioUnavailable ? OriginalSettings.MasterVolume : PendingSettings.MasterVolume;
	S->SetVSyncEnabled(PendingSettings.bVSync);
	S->SetFrameRateLimit(PendingSettings.FrameRateLimit);
	if (PendingSettings.Quality >= 0) S->SetOverallScalabilityLevel(PendingSettings.Quality);
	S->ApplyNonResolutionSettings(); S->SaveSettings();
	Message = bAudioUnavailable ? NSLOCTEXT("CHALK", "MasterUnavailableAtApply", "Graphics applied. Master audio is unavailable; its volume was not changed.")
		: NSLOCTEXT("CHALK", "SettingsApplied", "Settings applied.");
	if (auto* P = Preferences(); P && (P->GetGamepadGlyphPreference() != PendingSettings.GlyphFamily ||
		!FMath::IsNearlyEqual(P->GetMasterVolume(), VolumeToCommit) || !P->IsPreferenceSaved()))
	{
		FText Error;
		const FText ApplyMessage = Message;
		// One complete local preference record/write for this explicit Apply.
		if (!P->SetUIPreferences(PendingSettings.GlyphFamily, VolumeToCommit, Error))
			Message = FText::Format(NSLOCTEXT("CHALK", "SettingsPreferenceUnsaved", "{0}\nUI/audio preferences could not be saved: {1}"), ApplyMessage, Error);
		ApplyMasterVolume(P->GetMasterVolume());
	}
	ReadSettings(); Show(EAZ_MenuRoute::Settings);
}
void UAZ_MenuRoutesComponent::ResetSettings()
{
	const auto* Current = Settings(); if (!Current) return;
	UGameUserSettings* Defaults = NewObject<UGameUserSettings>(this, Current->GetClass());
	Defaults->SetToDefaults();
	PendingSettings.Resolution = Defaults->GetScreenResolution();
	if (PendingSettings.Resolution.X <= 0 || PendingSettings.Resolution.Y <= 0) PendingSettings.Resolution = Current->GetDesktopResolution();
	PendingSettings.DisplayMode = FromWindowMode(Defaults->GetFullscreenMode());
	PendingSettings.bVSync = Defaults->IsVSyncEnabled();
	PendingSettings.Quality = Defaults->GetOverallScalabilityLevel();
	PendingSettings.FrameRateLimit = Defaults->GetFrameRateLimit();
	if (CanEditMasterVolume()) PendingSettings.MasterVolume = 1.f;
	PendingSettings.GlyphFamily = EAZ_GamepadGlyphPreference::Auto;
	Message = NSLOCTEXT("CHALK", "DefaultsPending", "Defaults are ready. Apply to keep them, or cancel.");
	Widget->RefreshRoute();
}
void UAZ_MenuRoutesComponent::StopDisplayRevert()
{
	if (RevertTicker.IsValid()) { FTSTicker::RemoveTicker(RevertTicker); RevertTicker.Reset(); }
	if (DeactivateHandle.IsValid()) { FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(DeactivateHandle); DeactivateHandle.Reset(); }
}
void UAZ_MenuRoutesComponent::RevertDisplay()
{
	if (!bDisplayPreview) return;
	StopDisplayRevert(); bDisplayPreview = false;
	if (auto* S = Settings())
	{
		S->RevertVideoMode();
		S->SetScreenResolution(OriginalSettings.Resolution);
		S->SetFullscreenMode(ToWindowMode(OriginalSettings.DisplayMode));
		S->ApplyResolutionSettings(false); S->ConfirmVideoMode();
	}
	PendingSettings.Resolution = OriginalSettings.Resolution;
	PendingSettings.DisplayMode = OriginalSettings.DisplayMode;
	Message = NSLOCTEXT("CHALK", "DisplayReverted", "Display changes reverted.");
	if (!bClosingWidget && !bEndingPlay) Show(EAZ_MenuRoute::Settings);
}
int32 UAZ_MenuRoutesComponent::GetDisplayConfirmSeconds() const
{
	return bDisplayPreview ? FMath::Max(0, FMath::CeilToInt(DisplayDeadline - FPlatformTime::Seconds())) : 0;
}
bool UAZ_MenuRoutesComponent::TickDisplayRevert(float)
{
	if (!bDisplayPreview || bEndingPlay) return false;
	if (FPlatformTime::Seconds() >= DisplayDeadline) { RevertDisplay(); return false; }
	if (Widget) Widget->RefreshDisplayCountdown();
	return true;
}
void UAZ_MenuRoutesComponent::CancelSettings()
{
	RevertDisplay(); StopDisplayRevert(); PendingSettings = OriginalSettings; bSettingsSession = false;
	if (auto* P = Preferences()) P->OnPreferenceChanged.RemoveDynamic(this, &ThisClass::HandlePreferenceChanged);
}
void UAZ_MenuRoutesComponent::HandleWidgetRemoved()
{
	if (bClosingWidget || bEndingPlay) return;
	bClosingWidget = true; CancelSettings(); bClosingWidget = false;
	if (!bWaitingForLoad && !(Controller() && Controller()->CampaignSave && Controller()->CampaignSave->IsBusy())) Close();
}
void UAZ_MenuRoutesComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	bEndingPlay = true; bClosingWidget = true;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(DeferredLoadRequest);
	CancelSettings();
	if (auto* P = Preferences()) P->OnMasterVolumeChanged.RemoveDynamic(this, &ThisClass::HandleMasterVolumeChanged);
	ReleaseMasterMix();
	if (GEngine && TravelFailureHandle.IsValid()) GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	TravelFailureHandle.Reset();
	if (Controller() && Controller()->CampaignSave) Controller()->CampaignSave->OnLoadCompleted.RemoveDynamic(this, &ThisClass::HandleLoadCompleted);
	ReleasePause();
	if (Widget) { Widget->DeactivateWidget(); Widget->RemoveFromParent(); Widget = nullptr; }
	// Teardown releases ownership only; do not rebuild mappings on a departing PC/world.
	if (Controller()) Controller()->SetMenuRouteInputCaptured(false, false);
	Super::EndPlay(Reason);
}
