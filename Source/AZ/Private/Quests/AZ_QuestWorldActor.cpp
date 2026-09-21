#include "Quests/AZ_QuestWorldActor.h"

#include "AZ/AZ.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Navigation/AZ_NavigationTargetComponent.h"
#include "Navigation/AZ_NavigationTargetSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Player/AZ_PlayerController.h"
#include "Quests/AZ_QuestDefinition.h"
#include "Quests/AZ_QuestInteractionComponent.h"
#include "Quests/AZ_QuestProgressComponent.h"

AAZ_QuestWorldActor::AAZ_QuestWorldActor()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	InteractionVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionVolume"));
	SetRootComponent(InteractionVolume);
	InteractionVolume->InitSphereRadius(InteractionRadius);
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionVolume->SetCollisionResponseToChannel(COLLISION_INTERACTABLE, ECR_Overlap);
	InteractionVolume->SetGenerateOverlapEvents(true);
	DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
	DisplayMesh->SetupAttachment(InteractionVolume);
	DisplayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NavigationTarget = CreateDefaultSubobject<UAZ_NavigationTargetComponent>(TEXT("NavigationTarget"));
	NavigationTarget->SetupAttachment(InteractionVolume);
	InteractionCommit = CreateDefaultSubobject<UAZ_QuestInteractionComponent>(TEXT("InteractionCommit"));
}

void AAZ_QuestWorldActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	InteractionVolume->SetSphereRadius(FMath::IsFinite(InteractionRadius) ? FMath::Clamp(InteractionRadius, 25.0f, 1000.0f) : 250.0f);
	InteractionCommit->QuestId = QuestId;
	InteractionCommit->ObjectiveId = ObjectiveId;
}

void AAZ_QuestWorldActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAZ_QuestWorldActor, bEnabled);
	DOREPLIFETIME(AAZ_QuestWorldActor, CommittedUseCount);
}

FText AAZ_QuestWorldActor::GetInteractionPrompt() const
{
	if (!InteractionPrompt.IsEmpty()) return InteractionPrompt;
	switch (Action)
	{
	case EAZ_QuestWorldAction::OfferQuest: return NSLOCTEXT("CHALK", "OfferQuestPrompt", "Press E to accept task");
	case EAZ_QuestWorldAction::DeliverItems: return NSLOCTEXT("CHALK", "DeliverQuestPrompt", "Press E to deliver items");
	default: return NSLOCTEXT("CHALK", "UseQuestPrompt", "Press E to use");
	}
}

FText AAZ_QuestWorldActor::GetInteractionCaption() const
{
	if (!InteractionCaption.IsEmpty()) return InteractionCaption;
	if (!InteractionPrompt.IsEmpty()) return FText::GetEmpty();
	switch (Action)
	{
	case EAZ_QuestWorldAction::OfferQuest: return NSLOCTEXT("CHALK", "OfferQuestCaption", "Accept task");
	case EAZ_QuestWorldAction::DeliverItems: return NSLOCTEXT("CHALK", "DeliverQuestCaption", "Deliver items");
	default: return NSLOCTEXT("CHALK", "UseQuestCaption", "Use");
	}
}

bool AAZ_QuestWorldActor::IsAvailableForInteraction_Implementation(UPrimitiveComponent* Component) const
{
	return bEnabled && !bCommitting && !IsActorBeingDestroyed() && (!Component || Component == InteractionVolume);
}

bool AAZ_QuestWorldActor::CanInteractForPlayer(APlayerController* Controller, FString& OutError) const
{
	OutError.Reset();
	const AAZ_PlayerController* AZController = Cast<AAZ_PlayerController>(Controller);
	APawn* Pawn = IsValid(Controller) ? Controller->GetPawn() : nullptr;
	APlayerState* Player = IsValid(Controller) ? Controller->PlayerState : nullptr;
	if (!bEnabled || bCommitting || IsActorBeingDestroyed() || !AZController || !AZController->CanUseImmediateWorldInteraction()
		|| !IsValid(Pawn) || !IsValid(Player) || Player->GetPawn() != Pawn || Pawn->GetController() != Controller
		|| Controller->GetWorld() != GetWorld() || Pawn->GetWorld() != GetWorld())
	{
		OutError = TEXT("This interaction is unavailable for the current player.");
		return false;
	}
	FVector UseLocation;
	if (!FMath::IsFinite(InteractionRadius) || InteractionRadius <= 0.0f || InteractionRadius > 1000.0f
		|| !NavigationTarget->GetNavigationLocation(UseLocation) || Pawn->GetActorLocation().ContainsNaN()
		|| FVector::DistSquared(Pawn->GetActorLocation(), UseLocation) > FMath::Square(InteractionRadius))
	{
		OutError = TEXT("The interaction is out of reach.");
		return false;
	}
	FVector EyeLocation; FRotator EyeRotation;
	Pawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	if (EyeLocation.ContainsNaN()) return false;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(QuestWorldUse), false, Pawn);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, EyeLocation, UseLocation, ECC_Visibility, Query) && Hit.GetActor() != this)
	{
		OutError = TEXT("The interaction is obstructed.");
		return false;
	}
	UAZ_QuestProgressComponent* Progress = UAZ_QuestProgressComponent::FromPawn(Pawn);
	if (!Progress) { OutError = TEXT("Quest progress is unavailable."); return false; }
	if (Action == EAZ_QuestWorldAction::OfferQuest)
	{
		return Progress->CanAcceptQuest(OfferDefinition, OutError);
	}
	FAZ_QuestObjectiveDefinition Objective;
	int32 Remaining = 0;
	const EAZ_QuestObjectiveKind Kind = Action == EAZ_QuestWorldAction::DeliverItems
		? EAZ_QuestObjectiveKind::DeliverItem : EAZ_QuestObjectiveKind::Interact;
	if (!Progress->GetActiveObjective(QuestId, ObjectiveId, Objective, Remaining) || Objective.Kind != Kind)
	{
		OutError = TEXT("This objective is not active.");
		return false;
	}
	UAZ_NavigationTargetSubsystem* Registry = GetWorld()->GetSubsystem<UAZ_NavigationTargetSubsystem>();
	UAZ_NavigationTargetComponent* Provider = nullptr;
	FVector ObjectiveLocation;
	if (!Registry || Registry->ResolveTarget(Objective.Target, ObjectiveLocation, Provider) != EAZ_NavigationTargetResolveResult::ResolvedProvider
		|| Provider != NavigationTarget || !FMath::IsFinite(Objective.InteractionRadius) || Objective.InteractionRadius <= 0.0f
		|| FVector::DistSquared(Pawn->GetActorLocation(), ObjectiveLocation) > FMath::Square(Objective.InteractionRadius))
	{
		OutError = TEXT("The objective target is unresolved, ambiguous or out of reach.");
		return false;
	}
	return true;
}

bool AAZ_QuestWorldActor::TryInteractForPlayer(APlayerController* Controller, FGuid ReceiptId, FString& OutError)
{
	OutError.Reset();
	if (!HasAuthority() || !ReceiptId.IsValid() || CommittedReceipts.Contains(ReceiptId) || !CanInteractForPlayer(Controller, OutError)) return false;
	UAZ_QuestProgressComponent* Progress = UAZ_QuestProgressComponent::FromPawn(Controller->GetPawn());
	if (!Progress) return false;
	TGuardValue<bool> CommitGuard(bCommitting, true);
	CommittedReceipts.Add(ReceiptId);
	bool bCommitted = false;
	switch (Action)
	{
	case EAZ_QuestWorldAction::OfferQuest:
		bCommitted = Progress->AcceptQuest(OfferDefinition);
		break;
	case EAZ_QuestWorldAction::DeliverItems:
		bCommitted = Progress->TryDeliverObjective(QuestId, ObjectiveId, this, ReceiptId, OutError);
		break;
	case EAZ_QuestWorldAction::InteractObjective:
		// This class's world action is an immediate use-state change, not an animation/ability-start callback.
		if (CommittedUseCount == MAX_int32) break;
		++CommittedUseCount;
		InteractionCommit->QuestId = QuestId;
		InteractionCommit->ObjectiveId = ObjectiveId;
		bCommitted = InteractionCommit->ReportSuccessfulInteraction(Controller->GetPawn(), ReceiptId);
		if (!bCommitted) --CommittedUseCount;
		break;
	default: break;
	}
	if (!bCommitted)
	{
		CommittedReceipts.Remove(ReceiptId);
		if (OutError.IsEmpty()) OutError = TEXT("The world action could not be committed.");
		return false;
	}
	ForceNetUpdate();
	OnWorldActionCommitted(Controller, ReceiptId);
	return true;
}

void AAZ_QuestWorldActor::PostInteract_Implementation(AActor* InteractingActor, UPrimitiveComponent* Component)
{
	if (Component && Component != InteractionVolume) return;
	if (APawn* Pawn = Cast<APawn>(InteractingActor))
	{
		if (AAZ_PlayerController* Controller = Cast<AAZ_PlayerController>(Pawn->GetController()))
		{
			Controller->RequestImmediateWorldInteraction(this);
		}
	}
}
