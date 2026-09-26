#pragma once

#include "CoreMinimal.h"
#include "Player/AZ_PlayerController.h"
#include "AZ_MainMenuPlayerController.generated.h"

/** Menu-only owner for the existing CommonUI routes; deliberately has no gameplay Blueprint parent. */
UCLASS()
class AZ_API AAZ_MainMenuPlayerController : public AAZ_PlayerController
{
	GENERATED_BODY()
public:
	AAZ_MainMenuPlayerController();
};
