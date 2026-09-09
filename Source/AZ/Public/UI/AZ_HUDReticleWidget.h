#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/AZ_PlayerUITypes.h"
#include "AZ_HUDReticleWidget.generated.h"

/** Visual leaf shared by rifle, pistol, scope and other weapon reticle styles. */
UCLASS(Abstract)
class AZ_API UAZ_HUDReticleWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetReticleView(const FAZ_PlayerReticleView& View);

protected:
	UPROPERTY(BlueprintReadOnly, Transient, Category="AZ|HUD|Reticle")
	FAZ_PlayerReticleView CurrentReticleView;

	/** Optional custom presentation; no game state or shot accuracy is owned here. */
	UFUNCTION(BlueprintImplementableEvent, Category="AZ|HUD|Reticle")
	void OnReticleViewChanged(const FAZ_PlayerReticleView& View);
};
