#include "Quests/AZ_QuestReachArea.h"
#include "Quests/AZ_QuestProgressComponent.h"
#include "Components/BoxComponent.h"
#include "Navigation/AZ_NavigationTargetComponent.h"
#include "Navigation/AZ_NavigationTargetSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AAZ_QuestReachArea::AAZ_QuestReachArea()
{
	PrimaryActorTick.bCanEverTick = false;
	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("ObjectiveVolume"));
	SetRootComponent(Volume);
	Volume->SetBoxExtent(FVector(150.f, 150.f, 150.f));
	Volume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Volume->SetCollisionResponseToAllChannels(ECR_Ignore);
	Volume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Volume->SetGenerateOverlapEvents(true);
	NavigationTarget = CreateDefaultSubobject<UAZ_NavigationTargetComponent>(TEXT("NavigationTarget"));
	NavigationTarget->SetupAttachment(Volume);
}
void AAZ_QuestReachArea::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority()) return;
	Volume->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnEntered);
	// Handles a streamed-in area around an already-present pawn. Quest activation
	// and pawn replacement independently reconcile through the progress component.
	for (TActorIterator<APlayerState> It(GetWorld()); It; ++It) ReconcilePlayer(*It);
}
void AAZ_QuestReachArea::OnEntered(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (const auto* Pawn = Cast<APawn>(OtherActor)) ReconcilePlayer(Pawn->GetPlayerState());
}
void AAZ_QuestReachArea::ReconcilePlayer(APlayerState* Player)
{
	if (!HasAuthority() || !IsValid(Player) || Player->GetWorld() != GetWorld() || QuestId.IsNone() || ObjectiveId.IsNone()) return;
	APawn* Pawn = Player->GetPawn();
	auto* Progress = Player->FindComponentByClass<UAZ_QuestProgressComponent>();
	if (!IsValid(Pawn) || !Progress) return;
	FAZ_QuestObjectiveDefinition Objective;
	int32 Remaining;
	if (!Progress->GetActiveObjective(QuestId, ObjectiveId, Objective, Remaining) || Objective.Kind != EAZ_QuestObjectiveKind::ReachArea) return;
	auto* Registry = GetWorld()->GetSubsystem<UAZ_NavigationTargetSubsystem>();
	UAZ_NavigationTargetComponent* Provider = nullptr;
	FVector TargetLocation;
	if (!Registry || Registry->ResolveTarget(Objective.Target, TargetLocation, Provider) != EAZ_NavigationTargetResolveResult::ResolvedProvider
		|| Provider != NavigationTarget) return;
	const FVector Scale = Volume->GetComponentScale();
	if (Scale.ContainsNaN() || FMath::Abs(Scale.X) < SMALL_NUMBER || FMath::Abs(Scale.Y) < SMALL_NUMBER || FMath::Abs(Scale.Z) < SMALL_NUMBER) return;
	const FVector Local = Volume->GetComponentTransform().InverseTransformPosition(Pawn->GetActorLocation()).GetAbs();
	const FVector Extent = Volume->GetUnscaledBoxExtent();
	const bool bCenterInside = Local.X <= Extent.X && Local.Y <= Extent.Y && Local.Z <= Extent.Z;
	if (!bCenterInside && !Volume->IsOverlappingActor(Pawn)) return;
	Progress->ReportObjectiveFact(QuestId, ObjectiveId, EAZ_QuestObjectiveKind::ReachArea, Pawn,
		NavigationTarget->TargetId, FGuid::NewGuid());
}
