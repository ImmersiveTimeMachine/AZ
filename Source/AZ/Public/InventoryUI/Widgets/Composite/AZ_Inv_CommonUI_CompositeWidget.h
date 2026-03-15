// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_CommonUI_CompositeBaseWidget.h"
#include "AZ_Inv_CommonUI_CompositeWidget.generated.h"

/**
 * 
 */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_CompositeWidget : public UAZ_Inv_CommonUI_CompositeBaseWidget
{
	GENERATED_BODY()
	
public:
	virtual void NativeOnInitialized() override;
	virtual void ApplyFunction(FuncType Function) override;
	virtual void Collapse() override;
	TArray<UAZ_Inv_CommonUI_CompositeBaseWidget*> GetChildren() { return Children; }
	
private:
	UPROPERTY()
	TArray<TObjectPtr<UAZ_Inv_CommonUI_CompositeBaseWidget>> Children;
};
