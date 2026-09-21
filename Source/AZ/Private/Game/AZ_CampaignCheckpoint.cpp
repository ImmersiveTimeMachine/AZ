#include "Game/AZ_CampaignCheckpoint.h"
#include "Game/AZ_CampaignSaveCoordinator.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Player/AZ_PlayerController.h"
#include "AZ/AZ.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

AAZ_CampaignCheckpoint::AAZ_CampaignCheckpoint()
{
	PrimaryActorTick.bCanEverTick = false;
	CheckpointRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CheckpointRoot"));
	SetRootComponent(CheckpointRoot);
	bReplicates = true;
	InteractionVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(CheckpointRoot);
	InteractionVolume->SetSphereRadius(SaveRadius);
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionVolume->SetCollisionResponseToChannel(COLLISION_INTERACTABLE, ECR_Overlap);
	InteractionVolume->SetGenerateOverlapEvents(true);
}

void AAZ_CampaignCheckpoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	InteractionVolume->SetSphereRadius(FMath::Max(1.f, SaveRadius));
}

bool AAZ_CampaignCheckpoint::CanSaveForPlayer(APlayerController* Controller, FString& Error) const
{
	const APawn* Pawn = IsValid(Controller) ? Controller->GetPawn() : nullptr;
	if (!HasAuthority() || !bEnabled || CheckpointId.IsNone() || !IsValid(Pawn) || Controller->GetWorld() != GetWorld() ||
		!FMath::IsFinite(SaveRadius) || SaveRadius <= 0.f || FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) > FMath::Square(SaveRadius))
	{ Error = TEXT("Move to an enabled authored save point first."); return false; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(CampaignCheckpoint), false, Pawn);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetPawnViewLocation(), GetActorLocation(), ECC_Visibility, Query) && Hit.GetActor() != this)
	{ Error = TEXT("The save point is obstructed."); return false; }
	return true;
}

bool AAZ_CampaignCheckpoint::SaveForPlayer(APlayerController* Controller, FString& Error)
{
	if (!CanSaveForPlayer(Controller, Error)) return false;
	auto* Coordinator = UAZ_CampaignSaveCoordinator::GetOrCreateForController(Controller);
	if (!Coordinator) { Error = TEXT("Campaign save coordinator is unavailable."); return false; }
	return Coordinator->RequestCheckpointSave(this, Error);
}

bool AAZ_CampaignCheckpoint::IsAvailableForInteraction_Implementation(UPrimitiveComponent*) const { return bEnabled && !CheckpointId.IsNone(); }
void AAZ_CampaignCheckpoint::PostInteract_Implementation(AActor* Actor, UPrimitiveComponent*)
{
	APawn* Pawn = Cast<APawn>(Actor);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : Cast<APlayerController>(Actor);
	if (auto* AZPC = Cast<AAZ_PlayerController>(PC)) AZPC->RequestImmediateWorldInteraction(this);
}
