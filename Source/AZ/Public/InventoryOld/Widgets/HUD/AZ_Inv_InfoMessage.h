// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "AZ_Inv_InfoMessage.generated.h"

/**
 * 
 */
UCLASS()
class AZ_API UAZ_Inv_InfoMessage : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;

public:
	UFUNCTION(BlueprintImplementableEvent, Category = "AZ|Inventory")
	void MessageShow();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "AZ|Inventory")
	void MessageHide();

	void SetMessage(const FText& Message);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_Message;

	UPROPERTY(EditAnywhere, Category="AZ|Inventory")
	float MessageLifeTime{3.0f};

	FTimerHandle TimerHandle;
	bool bIsMessageActive{false};
};
