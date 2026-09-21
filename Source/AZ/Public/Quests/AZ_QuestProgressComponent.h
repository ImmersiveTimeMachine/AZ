#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Quests/AZ_QuestTypes.h"
#include "AZ_QuestProgressComponent.generated.h"

class APlayerState;
class APlayerController;
class APawn;
class UAZ_Inv_CommonUI_InventoryComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAZ_QuestProgressChanged);

/** PlayerState-owned gameplay progress. Widgets consume snapshots; no quest is accepted implicitly. */
UCLASS(ClassGroup=(AZ), meta=(BlueprintSpawnableComponent))
class AZ_API UAZ_QuestProgressComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAZ_QuestProgressComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(BlueprintAssignable, Category="Quest") FAZ_QuestProgressChanged OnQuestProgressChanged;
	UFUNCTION(BlueprintPure, Category="Quest") TArray<FAZ_QuestProgressRecord> GetQuestRecords() const { return Records; }
	UFUNCTION(BlueprintPure, Category="Quest") UAZ_QuestDefinition* FindQuestDefinition(FName QuestId) const;
	UFUNCTION(BlueprintPure, Category="Quest") FName GetTrackedQuestId() const { return TrackedQuestId; }
	UFUNCTION(BlueprintPure, Category="Quest") FName GetTrackedObjectiveId() const { return TrackedObjectiveId; }
	UFUNCTION(BlueprintCallable, Category="Quest") bool SetTrackedObjective(FName QuestId, FName ObjectiveId);
	UFUNCTION(BlueprintPure, Category="Quest") bool GetTrackedTarget(FAZ_NavigationTargetDescriptor& OutTarget) const;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest") bool AcceptQuest(UAZ_QuestDefinition* Definition);
	UFUNCTION(BlueprintPure, Category="Quest") bool CanAcceptQuest(UAZ_QuestDefinition* Definition, FString& OutReason) const;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest") bool CancelQuest(FName QuestId);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest") bool FailObjective(FName QuestId, FName ObjectiveId);
	/** Trusted committed world action. No client fact RPC; supplied instigator must belong to this PlayerState. */
	bool ReportObjectiveFact(FName QuestId, FName ObjectiveId, EAZ_QuestObjectiveKind Kind, APawn* InstigatorPawn,
		FName SourceTargetId, FGuid ReceiptId);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest") void RefreshInventoryObjectives();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest") bool TryDeliverObjective(FName QuestId, FName ObjectiveId,
		AActor* Recipient, FGuid ReceiptId, FString& OutError);
	UFUNCTION(BlueprintPure, Category="Quest") TArray<FAZ_QuestProgressRecord> ExportProgress() const { return Records; }
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Quest") bool RestoreProgress(const TArray<FAZ_QuestProgressRecord>& Progress,
		FString& OutError, bool bPublish = true);
	bool ValidateRestoreProgress(const TArray<FAZ_QuestProgressRecord>& Progress, FString& OutError) const;
	bool GetActiveObjective(FName QuestId, FName ObjectiveId, FAZ_QuestObjectiveDefinition& OutDefinition, int32& OutRemaining) const;
	bool IsDeliveryReceiptCommitted(FName QuestId, FName ObjectiveId, FGuid ReceiptId) const;
	bool ValidateDelivery(FName QuestId, FName ObjectiveId, AActor* Recipient, FGuid ReceiptId,
		FGameplayTag& OutItemType, int32& OutRemaining, FString& OutError) const;
	bool CommitDeliverySilent(FName QuestId, FName ObjectiveId, int32 Amount, FGuid ReceiptId);
	/** Call only after the inventory/save transaction is fully committed. */
	void PublishProgressChanged();
	/** Authority coordinator boundary. Rejects nesting and restore during a published callback. */
	bool BeginRestoreTransaction();
	/** Caller must finish teleport/commit or restore the previous checkpoint before unlocking.
	 * bPublish=true reconciles the now-coherent world/inventory and emits the resulting view. */
	void EndRestoreTransaction(bool bPublish);
	bool IsRestoreTransactionActive() const { return bRestoreTransaction; }
	APlayerController* GetQuestController() const;
	static UAZ_QuestProgressComponent* FromPawn(const APawn* Pawn);
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	UPROPERTY(ReplicatedUsing=OnRep_Progress) TArray<FAZ_QuestProgressRecord> Records;
	UPROPERTY(ReplicatedUsing=OnRep_Progress) FName TrackedQuestId;
	UPROPERTY(ReplicatedUsing=OnRep_Progress) FName TrackedObjectiveId;
	UPROPERTY(Transient) TArray<TObjectPtr<UAZ_QuestDefinition>> LoadedDefinitions;
	bool bPublishing = false;
	bool bPublishAgain = false;
	bool bRestoreTransaction = false;
	UPROPERTY(Transient) TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryComponent> BoundInventory;
	FTimerHandle InventoryBindingRetry;
	void BindInventory();
	UFUNCTION() void OnRep_Progress();
	UFUNCTION() void OnPawnChanged(APlayerState* Player, APawn* NewPawn, APawn* OldPawn);
	UFUNCTION() void OnTargetsChanged(FName TargetId);
	UFUNCTION(Server, Reliable) void ServerSetTrackedObjective(FName QuestId, FName ObjectiveId);
	FAZ_QuestProgressRecord* FindRecord(FName QuestId);
	const FAZ_QuestProgressRecord* FindRecord(FName QuestId) const;
	bool HasQuestAuthority() const;
	bool Reevaluate(FAZ_QuestProgressRecord& Record, const UAZ_QuestDefinition& Definition);
	void NormalizeTracking();
	bool ApplyCount(FName QuestId, FName ObjectiveId, int32 Count, FGuid ReceiptId, bool bAbsolute);
	void ReconcileWorldAreas();
};
