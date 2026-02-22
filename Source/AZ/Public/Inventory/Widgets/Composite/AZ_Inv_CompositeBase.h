// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "AZ_Inv_CompositeBase.generated.h"

/**
 * 
 */
UCLASS()
class AZ_API UAZ_Inv_CompositeBase : public UUserWidget
{
	GENERATED_BODY()
	
public:
	
	FGameplayTag GetFragmentTag() const { return FragmentTag; }
	void SetFragmentTag(const FGameplayTag& Tag) { FragmentTag = Tag; }
	virtual void Collapse();
	void Expand();

	using FuncType = TFunction<void(UAZ_Inv_CompositeBase*)>;
	virtual void ApplyFunction(FuncType Function) {}
private:

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FGameplayTag FragmentTag;
};
