// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineBaseTypes.h"
#include "AZ_GameInstance.generated.h"

class UAZ_CampaignSaveGame;

/**
 * 
 */
UCLASS(Config=Game)
class AZ_API UAZ_GameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:

	UPROPERTY(Config, EditDefaultsOnly, Category="Campaign|Maps")
	TSoftObjectPtr<UWorld> MainMenuMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/AZ/Maps/L_MainMenu.L_MainMenu")));
	UPROPERTY(Config, EditDefaultsOnly, Category="Campaign|Maps")
	TSoftObjectPtr<UWorld> FirstCampaignMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/AZ/Maps/L_001.L_001")));

	// Travel survives the source controller/world. Save objects contain no runtime actor references.
	UPROPERTY(Transient) TObjectPtr<UAZ_CampaignSaveGame> PendingCampaignLoad;
	UPROPERTY(Transient) bool bCampaignRestorePending = false;
	FString PendingTravelMap;
	FString PendingTravelSourceMap;
	FString PendingTravelError;
	bool bCampaignTravelPending = false;
	bool bRestoreAfterTravel = false;
	void ClearPendingCampaignLoad();
	virtual void Init() override;
	virtual void Shutdown() override;

private:
	FDelegateHandle CampaignTravelFailureHandle;
	void HandleCampaignTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error);

public:

	UPROPERTY()
	FName PlayerStartTag = FName();

	UPROPERTY()
	FString LoadSlotName = FString();

	UPROPERTY()
	int32 LoadSlotIndex = 0;	
};
