#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "InputMappingContext.h"
#include "UI/AZ_UIPreferencesSaveGame.h"
#include "AZ_MenuRoutesComponent.generated.h"

class AAZ_PlayerController;
class UAZ_MenuRouteWidget;
class UGameUserSettings;
class UAZ_InputPresentationSubsystem;
class USoundMix;
class USoundClass;

/** Synchronous resume barrier only: never an asset, saved remap, or retained context. */
UCLASS(Transient, NotBlueprintable)
class AZ_API UAZ_MenuResumeInputContext : public UInputMappingContext
{
	GENERATED_BODY()
public:
	UAZ_MenuResumeInputContext()
	{
		InputModeFilterOptions = EMappingContextInputModeFilterOptions::DoNotFilter;
		RegistrationTrackingMode = EMappingContextRegistrationTrackingMode::Untracked;
	}
};

UENUM(BlueprintType)
enum class EAZ_MenuRoute : uint8
{
	Closed, Pause, Title, Settings, LoadCheckpoint, ConfirmLoad, ConfirmNewGame, ConfirmTitle, ConfirmQuit, ConfirmDisplay, Loading, LoadFailure
};

UENUM(BlueprintType)
enum class EAZ_MenuCommand : uint8
{
	Resume, Inventory, Map, Settings, LoadCheckpoint, Continue, NewGame, Title, Quit, Confirm, Cancel, Apply, Reset
};

UENUM(BlueprintType)
enum class EAZ_MenuDisplayMode : uint8 { Windowed, Borderless, Fullscreen };

USTRUCT(BlueprintType)
struct AZ_API FAZ_MenuSettingsDraft
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) FIntPoint Resolution = FIntPoint(1920, 1080);
	UPROPERTY(BlueprintReadWrite) EAZ_MenuDisplayMode DisplayMode = EAZ_MenuDisplayMode::Borderless;
	UPROPERTY(BlueprintReadWrite) bool bVSync = false;
	/** -1 displays Custom without replacing the existing per-quality values. */
	UPROPERTY(BlueprintReadWrite) int32 Quality = -1;
	UPROPERTY(BlueprintReadWrite) float FrameRateLimit = 0;
	UPROPERTY(BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) float MasterVolume = 1;
	UPROPERTY(BlueprintReadWrite) EAZ_GamepadGlyphPreference GlyphFamily = EAZ_GamepadGlyphPreference::Auto;
};

/** One local-SP route owner. Campaign transactions remain in CampaignSaveCoordinator. */
UCLASS(ClassGroup=(AZ), meta=(BlueprintSpawnableComponent))
class AZ_API UAZ_MenuRoutesComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAZ_MenuRoutesComponent();
	UPROPERTY(EditDefaultsOnly, Category="Menus") TSubclassOf<UAZ_MenuRouteWidget> MenuWidgetClass;
	/** Defaults off to preserve direct editor/PIE startup. New Game adds a one-travel skip option. */
	UPROPERTY(EditDefaultsOnly, Category="Menus") bool bShowTitleOnStartup = false;
	UPROPERTY(BlueprintReadOnly, Category="Menus") EAZ_MenuRoute Route = EAZ_MenuRoute::Closed;
	UPROPERTY(BlueprintReadOnly, Category="Menus") FText Message;
	UPROPERTY(BlueprintReadOnly, Category="Menus|Settings") FAZ_MenuSettingsDraft PendingSettings;
	void InitializeForLocalPlayer();
	void HandlePauseAction();
	UFUNCTION(BlueprintCallable, Category="Menus") void Execute(EAZ_MenuCommand Command);
	UFUNCTION(BlueprintCallable, Category="Menus") void Back();
	UFUNCTION(BlueprintCallable, Category="Menus|Settings") void SetSettingsDraft(const FAZ_MenuSettingsDraft& Draft);
	UFUNCTION(BlueprintPure, Category="Menus") bool IsOpen() const { return Route != EAZ_MenuRoute::Closed; }
	UFUNCTION(BlueprintPure, Category="Menus") bool CanLoadCheckpoint() const;
	UFUNCTION(BlueprintPure, Category="Menus") bool CanOpenInventory() const;
	UFUNCTION(BlueprintPure, Category="Menus") bool CanReturnFromTitle() const { return bTitleEnteredFromPause; }
	UFUNCTION(BlueprintPure, Category="Menus") bool IsOperationPending() const { return bWaitingForLoad || Route == EAZ_MenuRoute::Loading; }
	UFUNCTION(BlueprintPure, Category="Menus|Settings") bool IsGlyphFamilyAvailable(EAZ_GamepadGlyphPreference Family) const;
	UFUNCTION(BlueprintPure, Category="Menus|Settings") bool CanEditMasterVolume() const;
	UFUNCTION(BlueprintPure, Category="Menus|Settings") TArray<FIntPoint> GetResolutions() const;
	UFUNCTION(BlueprintPure, Category="Menus|Settings") int32 GetDisplayConfirmSeconds() const;
	void HandleWidgetRemoved();
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY(Transient) TObjectPtr<UAZ_MenuRouteWidget> Widget;
	UPROPERTY(Transient) TObjectPtr<USoundMix> MasterMix;
	UPROPERTY(Transient) TObjectPtr<USoundClass> MasterClass;
	EAZ_MenuRoute ReturnRoute = EAZ_MenuRoute::Pause;
	FAZ_MenuSettingsDraft OriginalSettings;
	bool bOwnsPause = false;
	bool bClosingWidget = false;
	bool bEndingPlay = false;
	bool bWaitingForLoad = false;
	bool bDisplayPreview = false;
	bool bSettingsSession = false;
	bool bApplyingDisplayChange = false;
	bool bTravelRequested = false;
	bool bTitleEnteredFromPause = false;
	bool bMasterMixPushed = false;
	double DisplayDeadline = 0;
	uint64 LastNavigationFrame = MAX_uint64;
	FTSTicker::FDelegateHandle RevertTicker;
	FDelegateHandle DeactivateHandle;
	FDelegateHandle TravelFailureHandle;
	FTimerHandle DeferredLoadRequest;
	AAZ_PlayerController* Controller() const;
	UGameUserSettings* Settings() const;
	UAZ_InputPresentationSubsystem* Preferences() const;
	bool CanOwnMenus() const;
	bool Show(EAZ_MenuRoute Next);
	void Close();
	void AcquirePause();
	bool ReleasePause();
	void RestoreMenuInput();
	void StartLoad();
	void StartNewGame();
	void OpenInventory(bool bMap);
	void BeginSettings();
	void ReadSettings();
	void ApplySettings();
	void CommitSettings();
	void ResetSettings();
	void RevertDisplay();
	void CancelSettings();
	void ApplyMasterVolume(float Volume);
	void ReleaseMasterMix();
	void StopDisplayRevert();
	void HandleApplicationDeactivated();
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);
	bool TickDisplayRevert(float DeltaTime);
	UFUNCTION() void HandleLoadCompleted(bool bSuccess, const FString& Error);
	UFUNCTION() void RequestLoadAfterResume();
	UFUNCTION() void HandlePreferenceChanged(EAZ_GamepadGlyphPreference Preference, bool bSaved, FText Error);
	UFUNCTION() void HandleMasterVolumeChanged(float Volume);
};
