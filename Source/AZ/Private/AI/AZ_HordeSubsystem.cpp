// Copyright Artur. AZ project.

#include "AI/AZ_HordeSubsystem.h"

#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AI/AZ_InfectedAIController.h"
#include "AZ_GameplayTags.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/World.h"

namespace
{
	const TCHAR* RoleName(EAZ_CombatRole Role)
	{
		switch (Role)
		{
		case EAZ_CombatRole::Active:  return TEXT("Active");
		case EAZ_CombatRole::Passive: return TEXT("Passive");
		default:                      return TEXT("None");
		}
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

void UAZ_HordeSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// The brain beat: cheap (N zombies, tiny N) but there's no reason to re-rank every frame — combat
	// distances change on the 100ms scale, and the event paths keep the ledger correct between beats.
	RoleBeatAccumulator += DeltaTime;
	if (RoleBeatAccumulator >= RoleRecomputeIntervalSeconds)
	{
		RoleBeatAccumulator = 0.f;
		AssignCombatRoles();
	}
}

TStatId UAZ_HordeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAZ_HordeSubsystem, STATGROUP_Tickables);
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
	if (Controller)
	{
		// Corpses and unpossessed pawns hold no slot and wear no role tag (rule 7 + corpse doctrine).
		ApplyRole(Controller, EAZ_CombatRole::None, nullptr);
		Roles.Remove(Controller);
	}
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

bool UAZ_HordeSubsystem::RequestAttackToken(AAZ_InfectedAIController* Attacker, const AActor* Prey)
{
	if (!Attacker || !Prey)
	{
		return false;
	}
	if (const FCombatRoleState* State = Roles.Find(Attacker))
	{
		if (State->Role == EAZ_CombatRole::Active && State->Prey == Prey)
		{
			return true;   // already a designated fighter on this prey (re-entrant swings)
		}
	}
	// Inline promotion — don't make a lone fresh attacker wait out the beat. The BEAT balances by
	// distance; the GATE only enforces the cap.
	if (CountActiveOnPrey(Prey) < MaxAttackersPerPrey)
	{
		ApplyRole(Attacker, EAZ_CombatRole::Active, Prey);
		return true;
	}
	// Slots full: publish the ring role right away so the BT forks onto the ring hold this very tick.
	ApplyRole(Attacker, EAZ_CombatRole::Passive, Prey);
	return false;
}

void UAZ_HordeSubsystem::ReleaseAttackToken(AAZ_InfectedAIController* Attacker)
{
	if (!Attacker)
	{
		return;
	}
	// NOT a demotion — the pause between swings is still Active (rule 7: role = published state, not a
	// per-swing permit). Only clear when the engagement itself is gone (break-off, escape, unpossess);
	// the next beat promotes a Passive packmate if this one really left the fight.
	const FCombatRoleState* State = Roles.Find(Attacker);
	if (State && State->Role != EAZ_CombatRole::None && Attacker->GetFreshPerceivedTarget() != State->Prey.Get())
	{
		ApplyRole(Attacker, EAZ_CombatRole::None, nullptr);
	}
}

EAZ_CombatRole UAZ_HordeSubsystem::GetCombatRole(const AAZ_InfectedAIController* Controller) const
{
	const FCombatRoleState* State = Roles.Find(const_cast<AAZ_InfectedAIController*>(Controller));
	return State ? State->Role : EAZ_CombatRole::None;
}

int32 UAZ_HordeSubsystem::CountActiveOnPrey(const AActor* Prey) const
{
	int32 Count = 0;
	for (const auto& Pair : Roles)
	{
		if (Pair.Key.IsValid() && Pair.Value.Role == EAZ_CombatRole::Active && Pair.Value.Prey == Prey)
		{
			++Count;
		}
	}
	return Count;
}

void UAZ_HordeSubsystem::ApplyRole(AAZ_InfectedAIController* Controller, EAZ_CombatRole NewRole, const AActor* Prey)
{
	if (!Controller)
	{
		return;
	}
	FCombatRoleState& State = Roles.FindOrAdd(Controller);
	if (State.Role == NewRole && State.Prey.Get() == Prey)
	{
		return;   // no transition — no tag churn, no BB writes, no log spam
	}
	const EAZ_CombatRole OldRole = State.Role;
	State.Role = NewRole;
	State.Prey = Prey;

	// ASC role tags — replicated menace state for the AnimInstance (server-authoritative writer).
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	if (const APawn* Pawn = Controller->GetPawn())
	{
		if (const IAbilitySystemInterface* AbilityInterface = Cast<IAbilitySystemInterface>(Pawn))
		{
			if (UAZ_AbilitySystemComponent* ASC = Cast<UAZ_AbilitySystemComponent>(AbilityInterface->GetAbilitySystemComponent()))
			{
				ASC->RemoveStateTag(Tags.State_Combat_Engaged_Active);
				ASC->RemoveStateTag(Tags.State_Combat_Engaged_Passive);
				if (NewRole == EAZ_CombatRole::Active)
				{
					ASC->AddStateTag(Tags.State_Combat_Engaged_Active);
				}
				else if (NewRole == EAZ_CombatRole::Passive)
				{
					ASC->AddStateTag(Tags.State_Combat_Engaged_Passive);
				}
			}
		}
	}

	// BB mirror — the ONE key the BT Chase fork observes, plus the ring radius its MoveTo binds.
	if (UBlackboardComponent* BB = Controller->GetBlackboardComponent())
	{
		BB->SetValueAsBool(AZ_ChalkieBBKeys::bActiveFighter, NewRole == EAZ_CombatRole::Active);
		BB->SetValueAsFloat(AZ_ChalkieBBKeys::RingDistance, RingDistanceCm);
	}

	UE_LOG(LogTemp, Display, TEXT("[CrowdBrain] %s role %s -> %s (prey=%s)"),
		*GetNameSafe(Controller->GetPawn()), RoleName(OldRole), RoleName(NewRole), *GetNameSafe(Prey));
}

void UAZ_HordeSubsystem::AssignCombatRoles()
{
	// 1) Facts: who is engaged, on whom, from how far. (Perception owns freshness — rule 9.)
	struct FCandidate
	{
		AAZ_InfectedAIController* Controller = nullptr;
		float RankDistance = 0.f;
	};
	TMap<const AActor*, TArray<FCandidate>> ByPrey;

	for (int32 i = Infected.Num() - 1; i >= 0; --i)
	{
		AAZ_InfectedAIController* Chalkie = Infected[i].Get();
		if (!Chalkie)
		{
			Infected.RemoveAtSwap(i);
			continue;
		}
		const APawn* ChalkiePawn = Chalkie->GetPawn();
		APawn* Prey = Chalkie->GetFreshPerceivedTarget();
		if (!ChalkiePawn || !Prey)
		{
			ApplyRole(Chalkie, EAZ_CombatRole::None, nullptr);
			continue;
		}
		const FCombatRoleState* Current = Roles.Find(Chalkie);
		const bool bWasActiveOnPrey = Current && Current->Role == EAZ_CombatRole::Active && Current->Prey.Get() == Prey;
		FCandidate Candidate;
		Candidate.Controller = Chalkie;
		// Stickiness: an incumbent Active only loses its slot to a challenger that is MEANINGFULLY
		// closer — otherwise the pair flickers on every strafe step and the tags/BT churn with it.
		Candidate.RankDistance = FVector::Dist2D(ChalkiePawn->GetActorLocation(), Prey->GetActorLocation())
			- (bWasActiveOnPrey ? ActiveStickinessCm : 0.f);
		ByPrey.FindOrAdd(Prey).Add(Candidate);
	}

	// 2) Decision per prey: closest MaxAttackersPerPrey press, everyone else rings.
	for (auto& Pair : ByPrey)
	{
		Pair.Value.Sort([](const FCandidate& A, const FCandidate& B) { return A.RankDistance < B.RankDistance; });
		for (int32 i = 0; i < Pair.Value.Num(); ++i)
		{
			ApplyRole(Pair.Value[i].Controller,
				i < MaxAttackersPerPrey ? EAZ_CombatRole::Active : EAZ_CombatRole::Passive,
				Pair.Key);
		}
	}
}
