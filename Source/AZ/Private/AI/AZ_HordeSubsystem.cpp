// Copyright Artur. AZ project.

#include "AI/AZ_HordeSubsystem.h"

#include "AI/AZ_InfectedAIController.h"
#include "Engine/World.h"

namespace
{
	// Attack-token ledger: prey -> current attackers. File-static glue (LC can't add members to a live
	// UObject); a Live-Coding patch of this TU resets it — harmless, tokens re-acquire next swing.
	constexpr int32 AZ_MaxAttackersPerPrey = 2;
	TMap<TWeakObjectPtr<const AActor>, TArray<TWeakObjectPtr<AAZ_InfectedAIController>>> GAttackTokens;
}

bool UAZ_HordeSubsystem::RequestAttackToken(AAZ_InfectedAIController* Attacker, const AActor* Prey)
{
	if (!Attacker || !Prey)
	{
		return false;
	}
	TArray<TWeakObjectPtr<AAZ_InfectedAIController>>& Holders = GAttackTokens.FindOrAdd(Prey);
	Holders.RemoveAll([](const TWeakObjectPtr<AAZ_InfectedAIController>& H) { return !H.IsValid(); });
	if (Holders.Contains(Attacker))
	{
		return true;   // already holds one (re-entrant swings)
	}
	if (Holders.Num() >= AZ_MaxAttackersPerPrey)
	{
		return false;  // slots full — hold the ring, MoveTo keeps you close, retry next loop
	}
	Holders.Add(Attacker);
	return true;
}

void UAZ_HordeSubsystem::ReleaseAttackToken(AAZ_InfectedAIController* Attacker)
{
	if (!Attacker)
	{
		return;
	}
	for (auto& Pair : GAttackTokens)
	{
		Pair.Value.RemoveAll([Attacker](const TWeakObjectPtr<AAZ_InfectedAIController>& H)
		{
			return !H.IsValid() || H.Get() == Attacker;
		});
	}
}

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
