// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "AZ_InputConfig.generated.h"


USTRUCT(BlueprintType)
struct FAZ_InputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	const class UInputAction* InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly)
	FGameplayTag InputTag = FGameplayTag();
};

UCLASS()
class AZ_API UAZ_InputConfig : public UDataAsset
{
	GENERATED_BODY()
	
public:

	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InInputTag, bool bLogNotFound = false) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FAZ_InputAction> AbilityInputActions;
};
