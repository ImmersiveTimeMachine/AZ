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

	// EditAnywhere, not EditDefaultsOnly: a data asset is only ever edited as an asset, and EditDefaultsOnly
	// (CPF_DisableEditOnInstance) makes the Python bridge refuse to author rows ("cannot be edited on
	// instances", 2026-09-03) — the RT input mirror is scripted.
	UPROPERTY(EditAnywhere)
	const class UInputAction* InputAction = nullptr;

	UPROPERTY(EditAnywhere)
	FGameplayTag InputTag = FGameplayTag();
};

UCLASS()
class AZ_API UAZ_InputConfig : public UDataAsset
{
	GENERATED_BODY()
	
public:

	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InInputTag, bool bLogNotFound = false) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FAZ_InputAction> AbilityInputActions;
};
