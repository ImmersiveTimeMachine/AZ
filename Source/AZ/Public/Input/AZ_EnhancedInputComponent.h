#pragma once

#include "CoreMinimal.h"
#include "AZ_InputConfig.h"
#include "EnhancedInputComponent.h"
#include "AZ_EnhancedInputComponent.generated.h"

UCLASS()
class AZ_API UAZ_EnhancedInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()
	
public:

	template<class UserClass, typename PressedFuncType, typename ReleasedFuncType, typename HeldFuncType>
	void BindAbilityActions(const UAZ_InputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, HeldFuncType HeldFunc);
};

template<class UserClass, typename PressedFuncType, typename ReleasedFuncType, typename HeldFuncType>
	void UAZ_EnhancedInputComponent::BindAbilityActions(const UAZ_InputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, HeldFuncType HeldFunc)
{
	check(InputConfig);

	for (const auto& [InputAction, InputTag] : InputConfig->AbilityInputActions)
	{	
		if (InputAction && InputTag.IsValid())
		{
			if (PressedFunc)
			{
				BindAction(InputAction, ETriggerEvent::Started, Object, PressedFunc, InputTag);
			}

			if (ReleasedFunc)
			{
				BindAction(InputAction, ETriggerEvent::Completed, Object, ReleasedFunc, InputTag);
			}
			
			if (HeldFunc)
			{
				BindAction(InputAction, ETriggerEvent::Triggered, Object, HeldFunc, InputTag);
			}
		}
	}
}