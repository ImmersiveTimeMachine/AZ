// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_CompositeBase.h"
#include "AZ_Inv_Leaf.generated.h"

/**
 * 
 */
UCLASS()
class AZ_API UAZ_Inv_Leaf : public UAZ_Inv_CompositeBase
{
	GENERATED_BODY()
	
public:
	virtual void ApplyFunction(FuncType Function) override;
};
