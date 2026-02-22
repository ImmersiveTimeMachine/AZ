// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_InfoMessage.h"
#include "Blueprint/UserWidget.h"
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

public:
	UFUNCTION(BlueprintImplementableEvent, Category = "AZ|Inventory")
	void ShowPickupMessage(const FString& Message);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "AZ|Inventory")
	void HidePickupMessage();

private:
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UAZ_Inv_InfoMessage> InfoMessage;

	UFUNCTION()
	void OnNoRoom();
};
