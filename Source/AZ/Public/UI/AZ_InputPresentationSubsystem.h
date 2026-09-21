#pragma once

#include "CoreMinimal.h"
#include "CommonInputTypeEnum.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "UI/AZ_UIPreferencesSaveGame.h"
#include "AZ_InputPresentationSubsystem.generated.h"

class UCommonInputSubsystem;
class UInputDeviceSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAZ_UIPreferenceChanged, EAZ_GamepadGlyphPreference, Preference,
	bool, bSaved, FText, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAZ_MasterVolumeChanged, float, MasterVolume);

/** One UI presentation owner per existing local player. No input routing, polling or campaign state. */
UCLASS()
class AZ_API UAZ_InputPresentationSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	/** Call only after the user explicitly commits a selection. False includes save failure. */
	UFUNCTION(BlueprintCallable, Category="AZ|UI|Input Presentation")
	bool SetGamepadGlyphPreference(EAZ_GamepadGlyphPreference NewPreference, FText& OutError);

	/** Commit the complete UI preference record once from Settings Apply. Invalid values change nothing.
	 * A persistence failure retains both valid session choices and reports false/error. */
	UFUNCTION(BlueprintCallable, Category="AZ|UI|Preferences")
	bool SetUIPreferences(EAZ_GamepadGlyphPreference NewPreference, float NewMasterVolume, FText& OutError);

	/** Compatibility single-field commit; preserves the current glyph choice. Runtime mix belongs to MenuRoutes. */
	UFUNCTION(BlueprintCallable, Category="AZ|UI|Preferences")
	bool SetMasterVolume(float NewMasterVolume, FText& OutError);

	UFUNCTION(BlueprintPure, Category="AZ|UI|Preferences")
	float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintPure, Category="AZ|UI|Input Presentation")
	EAZ_GamepadGlyphPreference GetGamepadGlyphPreference() const { return Preference; }

	UFUNCTION(BlueprintPure, Category="AZ|UI|Input Presentation")
	bool IsGamepadGlyphPreferenceAvailable(EAZ_GamepadGlyphPreference Candidate) const;

	/** Queries fresh device metadata and uses the engine hardware matcher with the configured default seed. */
	UFUNCTION(BlueprintPure, Category="AZ|UI|Input Presentation")
	FName ResolveAutomaticGamepadFamily() const;

	UFUNCTION(BlueprintPure, Category="AZ|UI|Input Presentation")
	bool IsPreferenceSaved() const { return bPreferenceSaved; }

	UFUNCTION(BlueprintPure, Category="AZ|UI|Input Presentation")
	FText GetLastPreferenceError() const { return LastPreferenceError; }

	UPROPERTY(BlueprintAssignable, Category="AZ|UI|Input Presentation")
	FAZ_UIPreferenceChanged OnPreferenceChanged;

	/** Fires after user-identity load, controller-ready refresh and committed session volume changes. */
	UPROPERTY(BlueprintAssignable, Category="AZ|UI|Preferences")
	FAZ_MasterVolumeChanged OnMasterVolumeChanged;

private:
	static FName FamilyForPreference(EAZ_GamepadGlyphPreference Candidate);
	void LoadForCurrentUser();
	bool ApplyPreference();
	void HandleInputMethodChanged(ECommonInputType InputType);
	void HandleHardwareChanged(FPlatformUserId UserId, FInputDeviceId DeviceId);
	void HandleDevicePairingChanged(FInputDeviceId DeviceId, FPlatformUserId NewUserId, FPlatformUserId OldUserId);
	void HandleDeviceConnectionChanged(EInputDeviceConnectionState State, FPlatformUserId UserId, FInputDeviceId DeviceId);
	bool IsOwnedConnectedDevice(FInputDeviceId DeviceId) const;

	/**
	 * A gamepad this player owns just arrived: show gamepad hints without waiting for a button press.
	 *
	 * ★ GLYPH FAMILY AND ACTIVE INPUT METHOD ARE DIFFERENT FACTS. Everything else here answers "which
	 * artwork" (SetGamepadInputType); this answers "which device is the player using right now", which is
	 * what every prompt reads. Without it, plugging a controller in changes nothing on screen until the
	 * player presses something — the hints keep advertising keyboard keys for a device that is no longer
	 * in their hands.
	 *
	 * ONE-TIME, per arriving device, and never on a timer: the next real keyboard or mouse input is allowed
	 * to take the presentation straight back. It is a nudge at the moment of connection, not a mode lock,
	 * and it is deliberately not written to the saved preference.
	 */
	void AnnounceGamepadArrival(FInputDeviceId DeviceId);

	/**
	 * The device that was presenting has gone. Fall back only when nothing else this player owns can
	 * present, and only to a method the platform actually supports.
	 */
	void AnnounceGamepadDeparture();

	/** True when this local player still owns a connected gamepad, ignoring  Excluded. */
	bool HasOwnedConnectedGamepad(FInputDeviceId Excluded) const;
	void HandlePlatformUserChanged(FPlatformUserId NewUserId, FPlatformUserId OldUserId);
	void BroadcastPreference();

	TWeakObjectPtr<UCommonInputSubsystem> CommonInput;
	TWeakObjectPtr<UInputDeviceSubsystem> Devices;
	FDelegateHandle InputChangedHandle;
	FDelegateHandle HardwareChangedHandle;
	FDelegateHandle UserChangedHandle;
	FDelegateHandle DevicePairingHandle;
	FDelegateHandle DeviceConnectionHandle;
	FInputDeviceId LastObservedGamepadDevice;

	/**
	 * The device whose arrival already moved the presentation to gamepad.
	 *
	 * Keyed on the DEVICE, not on a bool, for two reasons: a second controller arriving is a fresh event
	 * worth reacting to, and re-notification about the same pad (hardware metadata often lands after the
	 * connection itself) must not keep yanking the presentation back off keyboard while the player types.
	 */
	FInputDeviceId AnnouncedGamepadDevice;
	EAZ_GamepadGlyphPreference Preference = EAZ_GamepadGlyphPreference::Auto;
	float MasterVolume = 1.0f;
	FText LastPreferenceError;
	FText PersistenceBlockReason;
	FString PreferenceSlot;
	int32 SaveUserIndex = INDEX_NONE;
	bool bLoadedForUser = false;
	bool bPreferenceSaved = false;
	bool bCanWritePreferences = true;
	bool bNeedsFormatUpgrade = false;
	bool bApplyingPreference = false;
};
