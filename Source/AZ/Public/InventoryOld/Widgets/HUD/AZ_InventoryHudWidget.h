// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_InfoMessage.h"
#include "Blueprint/UserWidget.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemState.h"
#include "AZ_InventoryHudWidget.generated.h"

/**
 * 
 */
UCLASS()
class AZ_API UAZ_InventoryHudWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

public:
	UFUNCTION(BlueprintImplementableEvent, Category = "AZ|Inventory")
	void ShowPickupMessage(const FString& Message);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "AZ|Inventory")
	void HidePickupMessage();

private:
	TWeakObjectPtr<class UAZ_Inv_CommonUI_InventoryComponent> AmmoInventory;
	TWeakObjectPtr<class UAZ_Inv_CommonUI_EquipmentComponent> AmmoEquipment;
	TWeakObjectPtr<class AAZ_Weapon> HitFeedbackWeapon;
	FAZ_WeaponAmmoSnapshot AmmoSnapshot;
	bool bShowWeaponAmmo = false;
	double HitFeedbackUntil = 0.0;
	FTimerHandle HitFeedbackTimer;
	UFUNCTION() void RefreshWeaponAmmo();
	UFUNCTION() void OnFirearmHitConfirmed(const FHitResult& Hit);
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UAZ_Inv_InfoMessage> InfoMessage;

	UFUNCTION()
	void OnNoRoom();
};
