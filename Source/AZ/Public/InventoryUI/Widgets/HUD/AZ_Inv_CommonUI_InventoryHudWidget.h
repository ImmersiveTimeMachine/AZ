// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "AZ_Inv_CommonUI_InventoryHudWidget.generated.h"

class UAZ_Inv_CommonUI_InfoMessage;

/**
 * CommonUI version of the inventory HUD widget.
 */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_InventoryHudWidget : public UCommonUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

public:
	UFUNCTION(BlueprintImplementableEvent, Category = "AZ|Inventory")
	void ShowPickupMessage(const FString& Message);

	UFUNCTION(BlueprintImplementableEvent, Category = "AZ|Inventory")
	void HidePickupMessage();

private:

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UAZ_Inv_CommonUI_InfoMessage> InfoMessage;

	UFUNCTION()
	void OnNoRoom();
};
