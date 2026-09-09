#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "UI/AZ_PlayerUITypes.h"
#include "AZ_Inv_CommonUI_InventoryHudWidget.generated.h"

class UAZ_PlayerUIComponent;
class UAZ_Inv_CommonUI_InfoMessage;
class UImage;
class UTextBlock;
class USizeBox;
class UScaleBox;
class UAZ_HUDReticleWidget;
class UAZ_HUDReticleDefinition;

/** Passive CommonUI HUD. Gameplay data comes from the owning player's UI component. */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_InventoryHudWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="AZ|Inventory")
	void ShowPickupMessage(const FString& Message);
	virtual void ShowPickupMessage_Implementation(const FString& Message);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="AZ|Inventory")
	void HidePickupMessage();
	virtual void HidePickupMessage_Implementation();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** BP forwards to the HQUI child's supported interface; it is not a UProgressBar. */
	UFUNCTION(BlueprintImplementableEvent, Category="AZ|HUD")
	void ApplyHealthPresentation(double Percent, FLinearColor FillColor, bool bImmediate);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|HUD|Style")
	FLinearColor HealthFillColor = FLinearColor::FromSRGBColor(FColor(238, 234, 224));
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|HUD|Style")
	FLinearColor CriticalHealthFillColor = FLinearColor::FromSRGBColor(FColor(224, 119, 104));
	UPROPERTY(EditDefaultsOnly, Category="AZ|HUD|Feedback", meta=(ClampMin="0.01"))
	float HitFeedbackDuration = 0.15f;
	UPROPERTY(EditDefaultsOnly, Category="AZ|HUD|Feedback", meta=(ClampMin="0.1"))
	float InfoMessageDuration = 3.0f;

	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UWidget> HealthContainer;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UUserWidget> HealthProgressBar;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> HealthIcon;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> LowHealthText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UWidget> WeaponContainer;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> WeaponIcon;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> WeaponNameText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> AmmoRoundsText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> AmmoCapacityText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SpareMagazinesText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UWidget> PickupContainer;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> PickupText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UWidget> InfoContainer;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> InfoText;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UWidget> HitMarker;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<USizeBox> ReticleHost;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UAZ_Inv_CommonUI_InfoMessage> InfoMessage;

private:
	TWeakObjectPtr<UAZ_PlayerUIComponent> PlayerUI;
	FTimerHandle HitFeedbackTimer;
	FTimerHandle InfoTimer;
	bool bPresentedHealth = false;
	bool bInventoryOpen = false;
	FGuid PresentedWeaponId;
	int32 DefaultAmmoFontSize = 0;
	UPROPERTY(Transient) TObjectPtr<UAZ_HUDReticleWidget> ReticleWidget;
	UPROPERTY(Transient) TObjectPtr<UAZ_HUDReticleDefinition> ActiveReticleDefinition;
	UPROPERTY(Transient) TObjectPtr<UScaleBox> ReticleScaleBox;

	UFUNCTION() void HandleVitalsChanged(const FAZ_PlayerVitalsView& View);
	UFUNCTION() void HandleWeaponChanged(const FAZ_PlayerWeaponView& View);
	UFUNCTION() void HandleHitConfirmed();
	UFUNCTION() void HandleInventoryFull();
	UFUNCTION() void HandleInventoryVisibilityChanged(bool bOpen);
	UFUNCTION() void HandleReticleChanged(const FAZ_PlayerReticleView& View);
	void ClearReticle();
	void ClearHitFeedback();
	void ClearInfoMessage();
};
