#include "UI/AZ_InputPresentationSubsystem.h"

#include "CommonInputBaseTypes.h"
#include "CommonInputSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Kismet/GameplayStatics.h"
#include "Templates/UnrealTemplate.h"
#include "Subsystems/SubsystemCollection.h"

#define LOCTEXT_NAMESPACE "AZInputPresentation"

void UAZ_InputPresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UCommonInputSubsystem>();
	CommonInput = UCommonInputSubsystem::Get(GetLocalPlayer());
	Devices = UInputDeviceSubsystem::Get();
	if (CommonInput.IsValid())
	{
		InputChangedHandle = CommonInput->OnInputMethodChangedNative.AddUObject(this, &ThisClass::HandleInputMethodChanged);
	}
	if (Devices.IsValid())
	{
		HardwareChangedHandle = Devices->OnInputHardwareDeviceChangedNative.AddUObject(this, &ThisClass::HandleHardwareChanged);
	}
	if (ULocalPlayer* Player = GetLocalPlayer())
	{
		UserChangedHandle = Player->OnPlatformUserIdChanged().AddUObject(this, &ThisClass::HandlePlatformUserChanged);
	}
	IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
	DevicePairingHandle = Mapper.GetOnInputDevicePairingChange().AddUObject(this, &ThisClass::HandleDevicePairingChanged);
	DeviceConnectionHandle = Mapper.GetOnInputDeviceConnectionChange().AddUObject(this, &ThisClass::HandleDeviceConnectionChanged);
	LoadForCurrentUser();
}

void UAZ_InputPresentationSubsystem::Deinitialize()
{
	if (CommonInput.IsValid()) CommonInput->OnInputMethodChangedNative.Remove(InputChangedHandle);
	if (Devices.IsValid()) Devices->OnInputHardwareDeviceChangedNative.Remove(HardwareChangedHandle);
	if (ULocalPlayer* Player = GetLocalPlayer()) Player->OnPlatformUserIdChanged().Remove(UserChangedHandle);
	IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
	Mapper.GetOnInputDevicePairingChange().Remove(DevicePairingHandle);
	Mapper.GetOnInputDeviceConnectionChange().Remove(DeviceConnectionHandle);
	InputChangedHandle.Reset(); HardwareChangedHandle.Reset(); UserChangedHandle.Reset();
	DevicePairingHandle.Reset(); DeviceConnectionHandle.Reset();
	CommonInput.Reset(); Devices.Reset();
	Super::Deinitialize();
}

void UAZ_InputPresentationSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	// Controller replacement does not reload preferences or replace UI ownership.
	if (!bLoadedForUser) LoadForCurrentUser();
	else
	{
		ApplyPreference();
		OnMasterVolumeChanged.Broadcast(MasterVolume);
	}
}

FName UAZ_InputPresentationSubsystem::FamilyForPreference(EAZ_GamepadGlyphPreference Candidate)
{
	switch (Candidate)
	{
	case EAZ_GamepadGlyphPreference::XboxSeries: return FName(TEXT("XSX"));
	case EAZ_GamepadGlyphPreference::PlayStation4: return FName(TEXT("PS4"));
	case EAZ_GamepadGlyphPreference::PlayStation5: return FName(TEXT("PS5"));
	default: return NAME_None;
	}
}

bool UAZ_InputPresentationSubsystem::IsGamepadGlyphPreferenceAvailable(EAZ_GamepadGlyphPreference Candidate) const
{
	const UCommonInputPlatformSettings* Settings = UCommonInputPlatformSettings::Get();
	if (!Settings || !CommonInput.IsValid()) return false;
	if (Candidate == EAZ_GamepadGlyphPreference::Auto) return true;
	const FName Family = FamilyForPreference(Candidate);
	return !Family.IsNone() && Settings->CanChangeGamepadType()
		&& !Settings->GetControllerDataForInputType(ECommonInputType::Gamepad, Family).IsEmpty();
}

bool UAZ_InputPresentationSubsystem::IsOwnedConnectedDevice(FInputDeviceId DeviceId) const
{
	const ULocalPlayer* Player = GetLocalPlayer();
	if (!Player || !Player->GetPlatformUserId().IsValid() || !DeviceId.IsValid()) return false;
	const IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
	return Mapper.GetUserForInputDevice(DeviceId) == Player->GetPlatformUserId()
		&& Mapper.GetInputDeviceConnectionState(DeviceId) == EInputDeviceConnectionState::Connected;
}

FName UAZ_InputPresentationSubsystem::ResolveAutomaticGamepadFamily() const
{
	UCommonInputPlatformSettings* Settings = UCommonInputPlatformSettings::Get();
	if (!Settings) return NAME_None;
	// Seed with the configured default, NEVER the sticky current/manual family.
	const FName DefaultFamily = Settings->GetDefaultGamepadName();
	const ULocalPlayer* Player = GetLocalPlayer();
	if (!Player || !Devices.IsValid()) return DefaultFamily;
	// This engine's typed LatestDevices map uses FindOrAdd, so it can retain
	// the first gamepad. An actual own-user device event is fresher after KBM.
	const FInputDeviceId Candidates[] = {
		Devices->GetMostRecentlyUsedInputDeviceId(Player->GetPlatformUserId()),
		LastObservedGamepadDevice,
		Devices->GetMostRecentlyUsedInputDeviceId(Player->GetPlatformUserId(), EHardwareDevicePrimaryType::Gamepad)
	};
	for (FInputDeviceId DeviceId : Candidates)
	{
		// Hardware metadata is global; cached IDs may have been disconnected or
		// paired to another user since their last hardware notification.
		if (!IsOwnedConnectedDevice(DeviceId)) continue;
		const FHardwareDeviceIdentifier Hardware = Devices->GetInputDeviceHardwareIdentifier(DeviceId);
		if (!Hardware.IsValid() || Hardware.PrimaryDeviceType != EHardwareDevicePrimaryType::Gamepad) continue;
		return Settings->GetBestGamepadNameForHardware(DefaultFamily, Hardware.InputClassName,
			Hardware.HardwareDeviceIdentifier.ToString());
	}
	return DefaultFamily;
}

void UAZ_InputPresentationSubsystem::LoadForCurrentUser()
{
	if (bLoadedForUser) return;
	const ULocalPlayer* Player = GetLocalPlayer();
	if (!Player || !Player->GetPlatformUserId().IsValid() || Player->GetPlatformUserIndex() == INDEX_NONE) return;
	SaveUserIndex = Player->GetPlatformUserIndex();
	// The name is user-specific even on platforms whose SaveGame implementation ignores UserIndex.
	PreferenceSlot = FString::Printf(TEXT("AZ_UI_Preferences_User_%d"), SaveUserIndex);
	bLoadedForUser = true;
	Preference = EAZ_GamepadGlyphPreference::Auto;
	MasterVolume = 1.0f;
	bPreferenceSaved = false;
	bCanWritePreferences = true;
	bNeedsFormatUpgrade = false;
	PersistenceBlockReason = FText::GetEmpty();
	LastPreferenceError = FText::GetEmpty();
	if (UGameplayStatics::DoesSaveGameExist(PreferenceSlot, SaveUserIndex))
	{
		const UAZ_UIPreferencesSaveGame* Saved = Cast<UAZ_UIPreferencesSaveGame>(
			UGameplayStatics::LoadGameFromSlot(PreferenceSlot, SaveUserIndex));
		if (!Saved)
		{
			PersistenceBlockReason = LOCTEXT("ReadFailedProtected", "UI preferences could not be loaded. The existing file has been preserved.");
		}
		else if (Saved->FormatVersion < 1 || Saved->FormatVersion > 2)
		{
			PersistenceBlockReason = LOCTEXT("UnsupportedVersionProtected", "This UI preferences format is unsupported. The existing file has been preserved.");
		}
		else
		{
			// Version 1 has no audio field. Migrate in memory only; explicit Apply writes version 2.
			const float LoadedVolume = Saved->FormatVersion == 1 ? 1.0f : Saved->MasterVolume;
			const bool bKnownFamily = Saved->GamepadGlyphPreference == EAZ_GamepadGlyphPreference::Auto
				|| !FamilyForPreference(Saved->GamepadGlyphPreference).IsNone();
			if (!bKnownFamily || !FMath::IsFinite(LoadedVolume) || LoadedVolume < 0.0f || LoadedVolume > 1.0f)
			{
				PersistenceBlockReason = LOCTEXT("InvalidRecordProtected", "UI preferences contain invalid values. The existing file has been preserved.");
			}
			else
			{
				MasterVolume = LoadedVolume;
				bNeedsFormatUpgrade = Saved->FormatVersion == 1;
				if (IsGamepadGlyphPreferenceAvailable(Saved->GamepadGlyphPreference))
				{
					Preference = Saved->GamepadGlyphPreference;
					bPreferenceSaved = true;
				}
				else LastPreferenceError = LOCTEXT("UnavailableSavedFamily", "The saved controller glyph family is unavailable. Automatic glyphs are active.");
			}
		}
	}
	if (!PersistenceBlockReason.IsEmpty())
	{
		bCanWritePreferences = false;
		LastPreferenceError = FText::Format(LOCTEXT("ProtectedDefaults", "UI defaults are active. {0}"), PersistenceBlockReason);
	}
	if (!ApplyPreference() && LastPreferenceError.IsEmpty())
	{
		LastPreferenceError = LOCTEXT("ApplyFailed", "Controller glyph preferences are unavailable on this platform.");
	}
	OnMasterVolumeChanged.Broadcast(MasterVolume);
	BroadcastPreference();
}

bool UAZ_InputPresentationSubsystem::ApplyPreference()
{
	if (bApplyingPreference) return true;
	UCommonInputPlatformSettings* Settings = UCommonInputPlatformSettings::Get();
	if (!Settings || !CommonInput.IsValid()) return false;
	const FName Desired = Preference == EAZ_GamepadGlyphPreference::Auto
		? ResolveAutomaticGamepadFamily() : FamilyForPreference(Preference);
	if (Desired.IsNone()) return false;
	if (CommonInput->GetCurrentGamepadName() == Desired) return true;
	if (!Settings->CanChangeGamepadType()) return false;
	TGuardValue<bool> Guard(bApplyingPreference, true);
	CommonInput->SetGamepadInputType(Desired);
	return CommonInput->GetCurrentGamepadName() == Desired;
}

bool UAZ_InputPresentationSubsystem::SetGamepadGlyphPreference(EAZ_GamepadGlyphPreference NewPreference, FText& OutError)
{
	return SetUIPreferences(NewPreference, MasterVolume, OutError);
}

bool UAZ_InputPresentationSubsystem::SetMasterVolume(float NewMasterVolume, FText& OutError)
{
	return SetUIPreferences(Preference, NewMasterVolume, OutError);
}

bool UAZ_InputPresentationSubsystem::SetUIPreferences(EAZ_GamepadGlyphPreference NewPreference, float NewMasterVolume, FText& OutError)
{
	OutError = FText::GetEmpty();
	if (!FMath::IsFinite(NewMasterVolume) || NewMasterVolume < 0.0f || NewMasterVolume > 1.0f)
	{
		OutError = LOCTEXT("InvalidMasterVolume", "Master volume must be a finite value from 0 to 1.");
		LastPreferenceError = OutError;
		BroadcastPreference();
		return false;
	}
	if (!bLoadedForUser || !IsGamepadGlyphPreferenceAvailable(NewPreference))
	{
		OutError = LOCTEXT("UnavailableSelection", "This controller glyph preference is not available.");
		LastPreferenceError = OutError;
		BroadcastPreference();
		return false;
	}
	const EAZ_GamepadGlyphPreference Previous = Preference;
	const float PreviousVolume = MasterVolume;
	Preference = NewPreference;
	if (!ApplyPreference())
	{
		Preference = Previous;
		OutError = LOCTEXT("ApplyFailed", "Controller glyph preferences are unavailable on this platform.");
		LastPreferenceError = OutError;
		BroadcastPreference();
		return false;
	}
	MasterVolume = NewMasterVolume;
	if (PreviousVolume != MasterVolume) OnMasterVolumeChanged.Broadcast(MasterVolume);
	if (Previous == NewPreference && PreviousVolume == MasterVolume && bPreferenceSaved && !bNeedsFormatUpgrade)
	{
		LastPreferenceError = FText::GetEmpty();
		BroadcastPreference();
		return true;
	}
	if (!bCanWritePreferences)
	{
		bPreferenceSaved = false;
		LastPreferenceError = FText::Format(LOCTEXT("ProtectedSessionOnly", "Preferences changed for this session only. {0}"), PersistenceBlockReason);
		OutError = LastPreferenceError;
		BroadcastPreference();
		return false;
	}
	// This is the ONLY write site. Startup, reconnect, mappings and travel never save.
	UAZ_UIPreferencesSaveGame* Record = Cast<UAZ_UIPreferencesSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UAZ_UIPreferencesSaveGame::StaticClass()));
	if (Record)
	{
		Record->GamepadGlyphPreference = Preference;
		Record->MasterVolume = MasterVolume;
	}
	bPreferenceSaved = Record && UGameplayStatics::SaveGameToSlot(Record, PreferenceSlot, SaveUserIndex);
	if (bPreferenceSaved) bNeedsFormatUpgrade = false;
	LastPreferenceError = bPreferenceSaved ? FText::GetEmpty()
		: LOCTEXT("SaveFailed", "Preferences changed for this session, but the UI preferences could not be saved.");
	OutError = LastPreferenceError;
	BroadcastPreference();
	return bPreferenceSaved;
}

void UAZ_InputPresentationSubsystem::HandleInputMethodChanged(ECommonInputType InputType)
{
	if (!bApplyingPreference) ApplyPreference();
}

void UAZ_InputPresentationSubsystem::HandleHardwareChanged(FPlatformUserId UserId, FInputDeviceId DeviceId)
{
	if (!GetLocalPlayer() || UserId != GetLocalPlayer()->GetPlatformUserId()) return;
	if (Devices.IsValid() && IsOwnedConnectedDevice(DeviceId))
	{
		const FHardwareDeviceIdentifier Hardware = Devices->GetInputDeviceHardwareIdentifier(DeviceId);
		if (Hardware.IsValid() && Hardware.PrimaryDeviceType == EHardwareDevicePrimaryType::Gamepad)
		{
			LastObservedGamepadDevice = DeviceId;
			// The family is resolved below; this is the separate question of whose turn it is to be shown.
			// Reached from the connection handler too, and from here when a device that could not be
			// classified at connection time finally reports itself as a gamepad.
			AnnounceGamepadArrival(DeviceId);
		}
	}
	ApplyPreference();
}

void UAZ_InputPresentationSubsystem::AnnounceGamepadArrival(FInputDeviceId DeviceId)
{
	if (AnnouncedGamepadDevice == DeviceId || !CommonInput.IsValid())
	{
		return;   // already spoken for by this very pad; a repeat notification is not a new arrival
	}
	const UCommonInputPlatformSettings* Settings = UCommonInputPlatformSettings::Get();
	if (!Settings || !Settings->SupportsInputType(ECommonInputType::Gamepad))
	{
		return;   // a platform that cannot present gamepad hints must not be told to
	}
	AnnouncedGamepadDevice = DeviceId;
	if (CommonInput->GetCurrentInputType() == ECommonInputType::Gamepad)
	{
		return;   // already there; claiming the transition again would only fight the thrash guard
	}
	CommonInput->SetCurrentInputType(ECommonInputType::Gamepad);
	UE_LOG(LogTemp, Display, TEXT("[InputPresentation] gamepad arrived - hints switched to gamepad (device %d)"),
		DeviceId.GetId());
}

bool UAZ_InputPresentationSubsystem::HasOwnedConnectedGamepad(FInputDeviceId Excluded) const
{
	const ULocalPlayer* Player = GetLocalPlayer();
	if (!Player || !Devices.IsValid())
	{
		return false;
	}
	// Asked of the device subsystem rather than tracked in a list of our own: pairing can move a pad to
	// another user without any event we would have counted, and a stale list would keep the presentation on
	// a controller this player no longer holds.
	const FInputDeviceId Candidates[] = {
		Devices->GetMostRecentlyUsedInputDeviceId(Player->GetPlatformUserId(), EHardwareDevicePrimaryType::Gamepad),
		Devices->GetMostRecentlyUsedInputDeviceId(Player->GetPlatformUserId())
	};
	for (FInputDeviceId Candidate : Candidates)
	{
		if (Candidate == Excluded || !IsOwnedConnectedDevice(Candidate))
		{
			continue;
		}
		const FHardwareDeviceIdentifier Hardware = Devices->GetInputDeviceHardwareIdentifier(Candidate);
		if (Hardware.IsValid() && Hardware.PrimaryDeviceType == EHardwareDevicePrimaryType::Gamepad)
		{
			return true;
		}
	}
	return false;
}

void UAZ_InputPresentationSubsystem::AnnounceGamepadDeparture()
{
	const FInputDeviceId Departed = AnnouncedGamepadDevice;
	AnnouncedGamepadDevice = INPUTDEVICEID_NONE;
	if (!CommonInput.IsValid() || CommonInput->GetCurrentInputType() != ECommonInputType::Gamepad)
	{
		return;   // the player had already moved back to keyboard; nothing of ours is on screen
	}
	if (HasOwnedConnectedGamepad(Departed))
	{
		return;   // another pad of theirs can carry the presentation; leave it on gamepad
	}
	const UCommonInputPlatformSettings* Settings = UCommonInputPlatformSettings::Get();
	if (!Settings || !Settings->SupportsInputType(ECommonInputType::MouseAndKeyboard))
	{
		return;   // nowhere truthful to fall back to
	}
	// Presentation only. Releasing whatever the pad was holding is the input stack's job, not the icon
	// layer's, and doing it from here would cancel actions the player never let go of.
	CommonInput->SetCurrentInputType(ECommonInputType::MouseAndKeyboard);
	UE_LOG(LogTemp, Display, TEXT("[InputPresentation] gamepad left - hints fell back to keyboard/mouse"));
}

void UAZ_InputPresentationSubsystem::HandleDevicePairingChanged(FInputDeviceId DeviceId, FPlatformUserId NewUserId, FPlatformUserId OldUserId)
{
	const ULocalPlayer* Player = GetLocalPlayer();
	if (!Player || (NewUserId != Player->GetPlatformUserId() && OldUserId != Player->GetPlatformUserId())) return;
	if (LastObservedGamepadDevice == DeviceId) LastObservedGamepadDevice = INPUTDEVICEID_NONE;
	if (NewUserId == Player->GetPlatformUserId())
	{
		HandleHardwareChanged(NewUserId, DeviceId);
	}
	else
	{
		// It went to somebody else. For this player that is indistinguishable from unplugging it, and only
		// this player's presentation is touched.
		if (AnnouncedGamepadDevice == DeviceId)
		{
			AnnounceGamepadDeparture();
		}
		ApplyPreference();
	}
}

void UAZ_InputPresentationSubsystem::HandleDeviceConnectionChanged(EInputDeviceConnectionState State, FPlatformUserId UserId, FInputDeviceId DeviceId)
{
	if (!GetLocalPlayer() || UserId != GetLocalPlayer()->GetPlatformUserId()) return;
	if (State != EInputDeviceConnectionState::Connected)
	{
		if (LastObservedGamepadDevice == DeviceId)
		{
			LastObservedGamepadDevice = INPUTDEVICEID_NONE;
		}
		if (AnnouncedGamepadDevice == DeviceId)
		{
			AnnounceGamepadDeparture();
		}
	}
	HandleHardwareChanged(UserId, DeviceId);
}

void UAZ_InputPresentationSubsystem::HandlePlatformUserChanged(FPlatformUserId NewUserId, FPlatformUserId OldUserId)
{
	// Identity changes are a new user's one-time load, not a reload on controller travel.
	bLoadedForUser = false;
	bPreferenceSaved = false;
	SaveUserIndex = INDEX_NONE;
	PreferenceSlot.Reset();
	LastObservedGamepadDevice = INPUTDEVICEID_NONE;
	AnnouncedGamepadDevice = INPUTDEVICEID_NONE;
	Preference = EAZ_GamepadGlyphPreference::Auto;
	MasterVolume = 1.0f;
	LoadForCurrentUser();
	if (!bLoadedForUser)
	{
		ApplyPreference();
		OnMasterVolumeChanged.Broadcast(MasterVolume);
		BroadcastPreference();
	}
}

void UAZ_InputPresentationSubsystem::BroadcastPreference()
{
	OnPreferenceChanged.Broadcast(Preference, bPreferenceSaved, LastPreferenceError);
}

#undef LOCTEXT_NAMESPACE
