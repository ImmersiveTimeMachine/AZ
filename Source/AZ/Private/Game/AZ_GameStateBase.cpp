#include "Game/AZ_GameStateBase.h"

#include "Net/UnrealNetwork.h"

AAZ_GameStateBase::AAZ_GameStateBase()
	:
	DifficultyLevel(EDifficultyLevel::Normal)
{
	// Set the default difficulty level here
}

void AAZ_GameStateBase::SetDifficultyLevel(EDifficultyLevel NewDifficultyLevel)
{
	if (GetLocalRole() == ROLE_Authority) // Only set on the server
	{
		DifficultyLevel = NewDifficultyLevel;
		OnRep_DifficultyLevel(DifficultyLevel); // Manually call OnRep for single-player or dedicated server
	}
}

void AAZ_GameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AAZ_GameStateBase, DifficultyLevel);
}

void AAZ_GameStateBase::OnRep_DifficultyLevel(EDifficultyLevel OldDifficultyLevel)
{
}


