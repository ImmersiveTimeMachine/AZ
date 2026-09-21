#include "Quests/AZ_QuestInteractionComponent.h"
#include "Quests/AZ_QuestProgressComponent.h"
#include "Navigation/AZ_NavigationTargetComponent.h"
#include "Navigation/AZ_NavigationTargetSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"

UAZ_QuestInteractionComponent::UAZ_QuestInteractionComponent() { PrimaryComponentTick.bCanEverTick = false; }
bool UAZ_QuestInteractionComponent::ReportSuccessfulInteraction(APawn* InstigatorPawn, FGuid ReceiptId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(InstigatorPawn) || !ReceiptId.IsValid() || InstigatorPawn->GetWorld() != GetWorld()) return false;
	auto* Progress = UAZ_QuestProgressComponent::FromPawn(InstigatorPawn);
	FAZ_QuestObjectiveDefinition Objective;
	int32 Remaining;
	if (!Progress || !Progress->GetActiveObjective(QuestId, ObjectiveId, Objective, Remaining) || Objective.Kind != EAZ_QuestObjectiveKind::Interact) return false;
	auto* Registry = GetWorld()->GetSubsystem<UAZ_NavigationTargetSubsystem>();
	UAZ_NavigationTargetComponent* Provider = nullptr;
	FVector Location;
	if (!Registry || Registry->ResolveTarget(Objective.Target, Location, Provider) != EAZ_NavigationTargetResolveResult::ResolvedProvider
		|| !Provider || Provider->GetOwner() != GetOwner()
		|| FVector::DistSquared(Location, InstigatorPawn->GetActorLocation()) > FMath::Square(Objective.InteractionRadius)) return false;
	return Progress->ReportObjectiveFact(QuestId, ObjectiveId, EAZ_QuestObjectiveKind::Interact, InstigatorPawn, Provider->TargetId, ReceiptId);
}
