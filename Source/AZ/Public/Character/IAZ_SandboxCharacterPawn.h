#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Animation/AZ_LocomotionTypes.h"
#include "IAZ_SandboxCharacterPawn.generated.h"

/** Mirrors GASP BPI_SandboxCharacter_Pawn — query interface used by AnimInstance,
 *  camera director, and traversal system to read pawn state without hard-typing
 *  to AAZ_HeroPawn. SetCharacterInputState lets external systems (e.g. cinematics,
 *  AI controllers, GAS abilities) override the player's raw input flags.
 */
UINTERFACE(BlueprintType, MinimalAPI)
class UAZ_SandboxCharacterPawn : public UInterface
{
	GENERATED_BODY()
};

class AZ_API IAZ_SandboxCharacterPawn
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AZ|SandboxPawn")
	FAZ_CharacterPropertiesForAnimation GetPropertiesForAnimation() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AZ|SandboxPawn")
	FAZ_CharacterPropertiesForCamera GetPropertiesForCamera() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AZ|SandboxPawn")
	FAZ_CharacterPropertiesForTraversal GetPropertiesForTraversal() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AZ|SandboxPawn")
	void SetCharacterInputState(const FAZ_PlayerInputState& NewState);
};
