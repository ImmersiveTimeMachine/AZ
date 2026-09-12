#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/AZ_PlayerUITypes.h"
#include "AZ_HUDReticleWidget.generated.h"

class UImage;

/** Visual leaf shared by rifle, pistol, scope and other weapon reticle styles. */
UCLASS(Abstract)
class AZ_API UAZ_HUDReticleWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetReticleView(const FAZ_PlayerReticleView& View);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Existing four-tick visuals. Other reticle styles can omit these bindings. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> ArmUp;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> ArmDown;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> ArmLeft;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> ArmRight;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> OutlineUp;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> OutlineDown;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> OutlineLeft;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> OutlineRight;

	UPROPERTY(BlueprintReadOnly, Transient, Category="AZ|HUD|Reticle")
	FAZ_PlayerReticleView CurrentReticleView;

	/** Optional custom presentation; no game state or shot accuracy is owned here. */
	UFUNCTION(BlueprintImplementableEvent, Category="AZ|HUD|Reticle")
	void OnReticleViewChanged(const FAZ_PlayerReticleView& View);

private:
	void SetSpreadTranslation(FVector2D ExtraRadius);
};
