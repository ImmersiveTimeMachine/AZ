// Fill out your copyright notice in the Description page of Project Settings.


#include "Input/AZ_InputConfig.h"


const UInputAction* UAZ_InputConfig::FindAbilityInputActionForTag(const FGameplayTag& InInputTag, bool bLogNotFound) const
{
	for (const auto& [InputAction, InputTag]: AbilityInputActions)
	{
		if (InputAction && InputTag == InInputTag)
		{
			return InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogTemp, Error, TEXT("Can't find AbilityInputAction for InputTag [%s], on InputConfig [%s]"), *InInputTag.ToString(), *GetNameSafe(this));
	}

	return nullptr;
}
