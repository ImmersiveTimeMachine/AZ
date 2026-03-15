
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_CommonUI_CompositeBaseWidget.h"
#include "AZ_Inv_CommonUI_LeafWidget.generated.h"

/**
 * CommonUI version of leaf widget in the Composite pattern.
 * Represents a terminal node that applies functions to itself.
 */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_LeafWidget : public UAZ_Inv_CommonUI_CompositeBaseWidget
{
	GENERATED_BODY()
	
public:
	virtual void ApplyFunction(FuncType Function) override;
};
