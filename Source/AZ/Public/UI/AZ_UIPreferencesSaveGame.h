#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AZ_UIPreferencesSaveGame.generated.h"

UENUM(BlueprintType)
enum class EAZ_GamepadGlyphPreference : uint8
{
	Auto,
	XboxSeries UMETA(DisplayName="Xbox"),
	PlayStation4 UMETA(DisplayName="PlayStation 4"),
	PlayStation5 UMETA(DisplayName="PlayStation 5")
};

/** Presentation only. Never included in a campaign snapshot or vendor settings slot. */
UCLASS()
class AZ_API UAZ_UIPreferencesSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) int32 FormatVersion = 2;
	UPROPERTY(SaveGame) EAZ_GamepadGlyphPreference GamepadGlyphPreference = EAZ_GamepadGlyphPreference::Auto;
	UPROPERTY(SaveGame) float MasterVolume = 1.0f;
};
