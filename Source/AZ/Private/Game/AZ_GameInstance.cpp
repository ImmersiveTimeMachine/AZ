// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/AZ_GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Player/AZ_PlayerController.h"
#include "UI/AZ_MenuRoutesComponent.h"

void UAZ_GameInstance::Init()
{
	Super::Init();
	if (GEngine) CampaignTravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleCampaignTravelFailure);
}

void UAZ_GameInstance::Shutdown()
{
	if (GEngine && CampaignTravelFailureHandle.IsValid()) GEngine->OnTravelFailure().Remove(CampaignTravelFailureHandle);
	CampaignTravelFailureHandle.Reset();
	Super::Shutdown();
}

void UAZ_GameInstance::ClearPendingCampaignLoad()
{
	PendingCampaignLoad = nullptr;
	bCampaignRestorePending = false;
}

void UAZ_GameInstance::HandleCampaignTravelFailure(UWorld* World, ETravelFailure::Type, const FString& Error)
{
	if (!bCampaignTravelPending || !World || World->GetGameInstance() != this) return;
	bCampaignTravelPending = false;
	PendingTravelError = Error;
	if (auto* PC = Cast<AAZ_PlayerController>(World->GetFirstPlayerController()); PC && PC->MenuRoutes)
		PC->MenuRoutes->ReportCampaignTravelFailure(Error);
}



