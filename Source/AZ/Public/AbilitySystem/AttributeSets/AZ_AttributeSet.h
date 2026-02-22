#pragma once

#include <CoreMinimal.h>
#include <AttributeSet.h>
#include <GameplayEffectTypes.h>

#include "Weapon/AZ_Weapon.h"
#include "AZ_AttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

// typedef is specific to the FGameplayAttribute() signature, but TStaticFunPtr is generic to any signature chosen
//typedef TBaseStaticDelegateInstance<FGameplayAttribute(), FDefaultDelegateUserPolicy>::FFuncPtr FAttributeFuncPtr;
template<class T>
using TStaticFuncPtr = typename TBaseStaticDelegateInstance<T, FDefaultDelegateUserPolicy>::FFuncPtr;

USTRUCT()
struct AZ_API FEffectContextInfo
{
	GENERATED_BODY()

	/** Handle to the Gameplay Effect context */
	UPROPERTY()
	FGameplayEffectContextHandle EffectContextHandle;

	/** Source of the Gameplay Effect */
	UPROPERTY()
	UAbilitySystemComponent* SourceAbilitySystemComponent;

	/** Source Avatar Actor */
	UPROPERTY()
	AActor* SourceAvatarActor;

	/** Source Controller (often the PlayerController on the client) */
	UPROPERTY()
	const APlayerController* SourceController;

	/** Source Character (if applicable) */
	UPROPERTY()
	const ACharacter* SourceCharacter;

	/** Target of the Gameplay Effect */
	UPROPERTY()
	UAbilitySystemComponent* TargetAbilitySystemComponent;

	/** Target Avatar Actor */
	UPROPERTY()
	AActor* TargetAvatarActor;

	/** Target Controller (often the PlayerController on the client) */
	UPROPERTY()
	APlayerController* TargetController;

	/** Target Character (if applicable) */
	UPROPERTY()
	ACharacter* TargetCharacter;
};

UCLASS()
class AZ_API UAZ_AttributeSet : public UAttributeSet
{
	GENERATED_BODY()

protected:
	
	void ExtractEffectContextInfo(const FGameplayEffectModCallbackData& Data, FEffectContextInfo& ContextInfo) const;

	TMap<FGameplayTag, TStaticFuncPtr<FGameplayAttribute()>> TagsToAttributes;

};
