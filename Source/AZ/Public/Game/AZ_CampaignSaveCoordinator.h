#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MoverComponent.h"
#include "AZ_CampaignSaveCoordinator.generated.h"

class APlayerController;
class APawn;
class AAZ_CampaignCheckpoint;
class UAZ_CampaignSaveGame;
class UAZ_QuestProgressComponent;
class UAZ_Inv_CommonUI_InventoryComponent;
class UAZ_Inv_CommonUI_EquipmentComponent;
class UAZ_QuickBarComponent;
struct FAZ_CampaignTeleportToken;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAZ_CampaignSaveResult, bool, bSuccess, const FString&, Message);

/** Local single-protagonist checkpoint coordination. Gameplay records never live in the UI. */
UCLASS(BlueprintType, ClassGroup=(AZ), meta=(BlueprintSpawnableComponent))
class AZ_API UAZ_CampaignSaveCoordinator : public UActorComponent
{
	GENERATED_BODY()
public:
	UAZ_CampaignSaveCoordinator();
	UPROPERTY(BlueprintAssignable, Category="Campaign") FAZ_CampaignSaveResult OnSaveCompleted;
	UPROPERTY(BlueprintAssignable, Category="Campaign") FAZ_CampaignSaveResult OnLoadCompleted;
	UPROPERTY(EditDefaultsOnly, Category="Campaign") bool bAutoSaveImportantQuestChanges = true;
	UPROPERTY(EditDefaultsOnly, Category="Campaign") FString DefaultSlotName = TEXT("CHALK_Campaign");
	UPROPERTY(BlueprintReadOnly, Category="Campaign") FString LastMessage;
	UFUNCTION(BlueprintCallable, Category="Campaign") static UAZ_CampaignSaveCoordinator* GetOrCreateForController(APlayerController* Controller);
	/** Manual saves are restricted to an actual authored, reachable save point. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Campaign") bool RequestCheckpointSave(AAZ_CampaignCheckpoint* Checkpoint, FString& OutError);
	/** True means accepted. OnLoadCompleted confirms Mover teleport and publication. Load the saved map first. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Campaign") bool LoadCampaign(FString& OutError);
	/** Newest header-valid A/B candidate. Caller must keep a strong reference across travel.
	 * Destination-dependent validation still happens in LoadCampaignSnapshot. */
	UAZ_CampaignSaveGame* ResolveCampaignTravelSnapshot(FString& OutError) const;
	/** Restore the exact snapshot chosen before travel; never silently substitute a different checkpoint. */
	bool LoadCampaignSnapshot(UAZ_CampaignSaveGame* Snapshot, FString& OutError);
	/** Refreshes component bindings, then checks the same readiness gates used by checkpoint loading. */
	bool IsReadyForCampaignLoad(FString& OutError);
	UFUNCTION(BlueprintPure, Category="Campaign") bool HasCampaignSave() const;
	UFUNCTION(BlueprintPure, Category="Campaign") bool IsBusy() const { return bLoading; }
	UFUNCTION(BlueprintCallable, Category="Campaign") void RefreshBindings();
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UPROPERTY(Transient) TObjectPtr<UAZ_QuestProgressComponent> QuestProgress;
	UPROPERTY(Transient) TObjectPtr<UAZ_CampaignSaveGame> PendingLoad;
	UPROPERTY(Transient) TObjectPtr<UAZ_Inv_CommonUI_InventoryComponent> RestoreInventory;
	UPROPERTY(Transient) TObjectPtr<UAZ_Inv_CommonUI_EquipmentComponent> RestoreEquipment;
	UPROPERTY(Transient) TObjectPtr<UAZ_QuickBarComponent> RestoreQuickBar;
	UPROPERTY(Transient) TObjectPtr<UMoverComponent> RestoreMover;
	FTimerHandle BindingTimer, AutoSaveTimer, LoadTimeout;
	FString LastProgressFingerprint;
	bool bAutoSavePending = false;
	bool bLoading = false;
	bool bOwnsInputLock = false;
	bool bDataCommitted = false;
	FName LastCheckpointId;
	FTransform RestoreStartTransform;
	FVector RestoreDestination = FVector::ZeroVector;
	FName RestorePreviousTrackedQuest, RestorePreviousTrackedObjective;
	TSharedPtr<FAZ_CampaignTeleportToken, ESPMode::ThreadSafe> TeleportToken;
	bool bReturningToStart = false;
	bool bRollbackBlocked = false;
	FString RollbackMessage;
	bool bRestoreCallInProgress = false;
	bool bContextChangedDuringRestore = false;
	bool bEndingPlay = false;
	bool ResolveSlot(FString& OutSlot, int32& OutIndex) const;
	bool ValidateContext(FString& OutError) const;
	bool Capture(UAZ_CampaignSaveGame& OutSave, FString& OutError) const;
	bool ValidateSave(UAZ_CampaignSaveGame& Save, FString& OutError) const;
	bool StartPendingLoad(FString& OutError);
	bool WriteCheckpoint(FName CheckpointId, FString& OutError);
	FString ProgressFingerprint() const;
	void FinishLoad(bool bSuccess, const FString& Message);
	void CancelRestoreForContextChange(const FString& Message);
	bool CommitPendingLoad(FString& OutError);
	void QueueCheckpointTeleport(const FTransform& Transform);
	UFUNCTION() void HandleLoadTimeout();
	UFUNCTION() void HandleQuestChanged();
	UFUNCTION() void TryAutoSave();
	UFUNCTION() void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);
	UFUNCTION() void HandleTeleportSuccess(const FVector& FromLocation, const FQuat& FromRotation,
		const FVector& ToLocation, const FQuat& ToRotation);
	UFUNCTION() void HandleTeleportFailure(const FVector& FromLocation, const FQuat& FromRotation,
		const FVector& ToLocation, const FQuat& ToRotation, ETeleportFailureReason Reason);
};
