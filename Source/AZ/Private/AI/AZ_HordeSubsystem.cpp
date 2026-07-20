// Copyright Artur. AZ project.

#include "AI/AZ_HordeSubsystem.h"

#include "AI/AZ_InfectedAIController.h"
#include "Engine/World.h"

bool UAZ_HordeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UAZ_HordeSubsystem::RegisterInfected(AAZ_InfectedAIController* Controller)
{
	if (Controller)
	{
		Infected.AddUnique(Controller);
	}
}

void UAZ_HordeSubsystem::UnregisterInfected(AAZ_InfectedAIController* Controller)
{
	Infected.Remove(Controller);
}

void UAZ_HordeSubsystem::NotifyAggro(AAZ_InfectedAIController* Instigator, const FVector& PreyLocation)
{
	const APawn* ScreamerPawn = Instigator ? Instigator->GetPawn() : nullptr;
	if (!ScreamerPawn)
	{
		return;
	}
	const FVector ScreamOrigin = ScreamerPawn->GetActorLocation();
	const float RadiusSq = FMath::Square(AlertRadius);

	for (int32 i = Infected.Num() - 1; i >= 0; --i)
	{
		AAZ_InfectedAIController* Ally = Infected[i].Get();
		if (!Ally)
		{
			Infected.RemoveAtSwap(i);
			continue;
		}
		const APawn* AllyPawn = Ally->GetPawn();
		if (Ally == Instigator || !AllyPawn)
		{
			continue;
		}
		if (FVector::DistSquared(AllyPawn->GetActorLocation(), ScreamOrigin) <= RadiusSq)
		{
			Ally->ArmInvestigation(PreyLocation, /*bUrgent*/ true);
		}
	}
}
