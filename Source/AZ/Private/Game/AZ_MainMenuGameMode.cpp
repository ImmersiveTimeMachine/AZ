#include "Game/AZ_MainMenuGameMode.h"

#include "GameFramework/PlayerState.h"
#include "Player/AZ_MainMenuPlayerController.h"

AAZ_MainMenuGameMode::AAZ_MainMenuGameMode()
{
	PlayerControllerClass = AAZ_MainMenuPlayerController::StaticClass();
	PlayerStateClass = APlayerState::StaticClass();
	DefaultPawnClass = nullptr;
	SpectatorClass = nullptr;
	HUDClass = nullptr;
}

void AAZ_MainMenuGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Intentionally omit RestartPlayer: an empty frontend world needs only its local controller.
}
