// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "AZ_Inv_CommonUI_InfoMessage.generated.h"

class UTextBlock;

/**
 * CommonUI version of the info message widget.
 */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_InfoMessage : public UCommonUserWidget
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
