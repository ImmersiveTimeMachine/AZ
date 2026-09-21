#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Game/AZ_CampaignSaveGame.h"
#include "AZ_CampaignWorldSubsystem.generated.h"

class UAZ_Inv_CommonUI_ItemComponent;

/** Supported world participants: persistent pickups plus explicitly published campaign facts. */
UCLASS()
class AZ_API UAZ_CampaignWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	void RegisterPickup(UAZ_Inv_CommonUI_ItemComponent* Component);
	void PickupRemoved(UAZ_Inv_CommonUI_ItemComponent* Component);
	bool Capture(FAZ_CampaignWorldSnapshot& OutState, FString& OutError) const;
	bool Validate(const FAZ_CampaignWorldSnapshot& State, const FAZ_InventorySnapshot& Inventory, FString& OutError) const;
	bool PrepareRestore(const FAZ_CampaignWorldSnapshot& State, FString& OutError);
	void CommitRestore();
	void CancelRestore();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="AZ|Campaign") bool SetWorldFact(FName FactId, bool bValue);
	UFUNCTION(BlueprintPure, Category="AZ|Campaign") bool GetWorldFact(FName FactId) const;
private:
	TSet<TWeakObjectPtr<UAZ_Inv_CommonUI_ItemComponent>> LivePickups;
	TMap<FGuid, FAZ_CampaignPickupSnapshot> AuthoredCatalog;
	TSet<FGuid> AmbiguousIds;
	UPROPERTY() TMap<FName, bool> Facts;
	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> StagedActors;
	UPROPERTY(Transient) TMap<FGuid, TObjectPtr<UAZ_Inv_CommonUI_ItemComponent>> RestoreTargets;
	UPROPERTY(Transient) FAZ_CampaignWorldSnapshot PendingState;
	bool bRestoring = false;
	bool bUnidentifiedAuthoredPickupRemoved = false;
};
