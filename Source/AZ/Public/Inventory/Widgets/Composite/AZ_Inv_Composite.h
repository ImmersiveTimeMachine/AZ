// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/Widgets/Composite/AZ_Inv_CompositeBase.h"
#include "AZ_Inv_Composite.generated.h"

/**
 * 
 */
UCLASS()
class AZ_API UAZ_Inv_Composite : public UAZ_Inv_CompositeBase
{
	GENERATED_BODY()
	
public:
	virtual void NativeOnInitialized() override;
	virtual void ApplyFunction(FuncType Function) override;
	virtual void Collapse() override;
	TArray<UAZ_Inv_CompositeBase*> GetChildren() { return Children; }
	
private:
	UPROPERTY()
	TArray<TObjectPtr<UAZ_Inv_CompositeBase>> Children;
};
