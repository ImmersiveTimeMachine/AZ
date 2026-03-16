#pragma once

#include <CoreMinimal.h>
#include <Engine/DataAsset.h>
#include <ScalableFloat.h>
#include "AZ_CharacterClassInfo.generated.h"


class UGameplayAbility;
class UGameplayEffect;

UENUM(BlueprintType)
enum class EAZCharacterClass : uint8
{
	Zombie,
	Warrior,
	Ranger
};

USTRUCT(BlueprintType)
struct FAZCharacterClassDefaultInfo
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Class Defaults")
	TSubclassOf<UGameplayEffect> PrimaryAttributes;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Class Defaults")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Class Defaults")
	FScalableFloat XPReward = FScalableFloat();
};

UCLASS()
class AZ_API UAZ_CharacterClassInfo : public UDataAsset
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Character Class Defaults")
	TMap<EAZCharacterClass, FAZCharacterClassDefaultInfo> CharacterClassInformation;
	
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Common Class Defaults")
	TSubclassOf<UGameplayEffect> PrimaryAttributes_SetByCaller;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Common Class Defaults")
	TSubclassOf<UGameplayEffect> SecondaryAttributes;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Common Class Defaults")
	TSubclassOf<UGameplayEffect> SecondaryAttributes_Infinite;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Common Class Defaults")
	TSubclassOf<UGameplayEffect> VitalAttributes;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Common Class Defaults")
	TArray<TSubclassOf<UGameplayAbility>> CommonAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Common Class Defaults|Damage")
	TObjectPtr<UCurveTable> DamageCalculationCoefficients;

	FAZCharacterClassDefaultInfo GetClassDefaultInfo(EAZCharacterClass CharacterClass);
	
};
