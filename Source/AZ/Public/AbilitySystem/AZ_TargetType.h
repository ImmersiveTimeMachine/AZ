// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/AZ_CharacterBase.h"
#include "AZ_TargetType.generated.h"

/**
 * Class that is used to determine targeting for abilities
 * It is meant to be blueprinted to run target logic
 * This does not subclass GameplayAbilityTargetActor because this class is never instanced into the world
 * This can be used as a basis for a game-specific targeting blueprint
 * If your targeting is more complicated you may need to instance into the world once or as a pooled actor
 */

UCLASS(Blueprintable, meta = (ShowWorldContextPin))
class AZ_API UAZ_TargetType : public UObject
{
	GENERATED_BODY()
public:
	// Constructor and overrides
	UAZ_TargetType() {}

	/** Called to determine targets to apply gameplay effects to */
	UFUNCTION(BlueprintNativeEvent)
	void GetTargets(AAZ_CharacterBase* TargetingCharacter, AActor* TargetingActor, FGameplayEventData EventData, TArray<FGameplayAbilityTargetDataHandle>& OutTargetData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const;
	virtual void GetTargets_Implementation(AAZ_CharacterBase* TargetingCharacter, AActor* TargetingActor, FGameplayEventData EventData, TArray<FGameplayAbilityTargetDataHandle>& OutTargetData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const;
};

/** Trivial target type that uses the owner */
UCLASS(NotBlueprintable)
class AZ_API UAZ_TargetType_UseOwner : public UAZ_TargetType
{
	GENERATED_BODY()

public:
	// Constructor and overrides
	UAZ_TargetType_UseOwner() {}

	/** Uses the passed in event data */
	virtual void GetTargets_Implementation(AAZ_CharacterBase* TargetingCharacter, AActor* TargetingActor, FGameplayEventData EventData, TArray<FGameplayAbilityTargetDataHandle>& OutTargetData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const override;
};

/** Trivial target type that pulls the target out of the event data */
UCLASS(NotBlueprintable)
class AZ_API UAZ_TargetType_UseEventData : public UAZ_TargetType
{
	GENERATED_BODY()

public:
	// Constructor and overrides
	UAZ_TargetType_UseEventData() {}

	/** Uses the passed in event data */
	virtual void GetTargets_Implementation(AAZ_CharacterBase* TargetingCharacter, AActor* TargetingActor, FGameplayEventData EventData, TArray<FGameplayAbilityTargetDataHandle>& OutTargetData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const override;
};