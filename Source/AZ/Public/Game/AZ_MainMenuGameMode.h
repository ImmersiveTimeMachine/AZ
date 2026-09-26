#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AZ_MainMenuGameMode.generated.h"

/** Lightweight frontend world: the player owns menu UI but never a gameplay or spectator pawn. */
UCLASS()
class AZ_API AAZ_MainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AAZ_MainMenuGameMode();
protected:
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
};
