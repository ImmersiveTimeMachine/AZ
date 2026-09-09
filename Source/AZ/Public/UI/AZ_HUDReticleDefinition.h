#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AZ_HUDReticleDefinition.generated.h"

class UAZ_HUDReticleWidget;

/** Weapon-authored UI presentation. Null on a weapon means no aiming reticle. */
UCLASS(BlueprintType)
class AZ_API UAZ_HUDReticleDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|HUD|Reticle")
	TSubclassOf<UAZ_HUDReticleWidget> WidgetClass;

	/** Logical viewport units; the host scales the widget's authored geometry uniformly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|HUD|Reticle", meta=(ClampMin="1"))
	FVector2D Size = FVector2D(40.f, 40.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|HUD|Reticle")
	FLinearColor Tint = FLinearColor::FromSRGBColor(FColor(238, 234, 224));

	/** False permits an equipped/hip reticle; action, menu and health suppression still apply. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AZ|HUD|Reticle")
	bool bAimOnly = true;
};
