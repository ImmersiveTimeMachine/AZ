// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/AZ_CharacterClassInfo.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AZ_AbilityFunctionLibrary.generated.h"

class UAbilitySystemComponent;
struct FAZ_GameplayEffectContainerSpec;
class UAZ_CharacterClassInfo;
class UAZ_GameplayAbility;
class UAttributeMenuWidgetController;
class UOverlayWidgetController;

/**
 * 
 */
UCLASS()
class AZ_API UAZ_AbilityFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/*UFUNCTION(BlueprintPure, Category="EchoAbilityFunctionLibrary|WidgetController", meta = (DefaultToSelf = "WorldContextObject"))
	static UOverlayWidgetController* GetOverlayWidgetController(const UObject* WorldContextObject);*/

	/*UFUNCTION(BlueprintPure, Category="EchoAbilityFunctionLibrary|WidgetController", meta = (DefaultToSelf = "WorldContextObject"))
	static UAttributeMenuWidgetController* GetAttributeMenuWidgetController(const UObject* WorldContextObject);*/

	// Returns the player's editor window role - Server (listen host) or Client #
	UFUNCTION(BlueprintCallable)
	static FString GetPlayerEditorWindowRole(UWorld* World);


	/**
	* GameplayAbility
	*/
	UFUNCTION(BlueprintCallable, Category = "Ability")
	static UAZ_GameplayAbility* GetPrimaryAbilityInstanceFromHandle(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAbilitySpecHandle Handle);

	UFUNCTION(BlueprintCallable, Category = "Ability")
	static UAZ_GameplayAbility* GetPrimaryAbilityInstanceFromClass(UAbilitySystemComponent* AbilitySystemComponent, TSubclassOf<UGameplayAbility> InAbilityClass);

	UFUNCTION(BlueprintCallable, Category = "Ability")
	static bool IsPrimaryAbilityInstanceActive(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAbilitySpecHandle Handle);


	/**
	* FGameplayAbilitySpecHandle
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability")
	static bool IsAbilitySpecHandleValid(FGameplayAbilitySpecHandle Handle);


	/**
	* FEchoGameplayEffectContainerSpec
	*/

	// USTRUCTs cannot contain UFUNCTIONS so we make static functions here
	// Checks if spec has any effects
	UFUNCTION(BlueprintPure, Category = "Ability|Container")
	static bool DoesEffectContainerSpecHaveEffects(const FAZ_GameplayEffectContainerSpec& ContainerSpec);

	// Checks if spec has any targets
	UFUNCTION(BlueprintPure, Category = "Ability|Container")
	static bool DoesEffectContainerSpecHaveTargets(const FAZ_GameplayEffectContainerSpec& ContainerSpec);

	// Clears spec's targets
	UFUNCTION(BlueprintCallable, Category = "Ability|Container")
	static void ClearEffectContainerSpecTargets(UPARAM(ref) FAZ_GameplayEffectContainerSpec& ContainerSpec);

	// Adds targets to a copy of the passed in effect container spec and returns it
	UFUNCTION(BlueprintCallable, Category = "Ability|Container", Meta = (AutoCreateRefTerm = "TargetData, HitResults, TargetActors"))
	static void AddTargetsToEffectContainerSpec(UPARAM(ref) FAZ_GameplayEffectContainerSpec& ContainerSpec, const TArray<FGameplayAbilityTargetDataHandle>& TargetData, const TArray<FHitResult>& HitResults, const TArray<AActor*>& TargetActors);

	// Applies container spec that was made from an ability
	UFUNCTION(BlueprintCallable, Category = "Ability|Container")
	static TArray<FActiveGameplayEffectHandle> ApplyExternalEffectContainerSpec(const FAZ_GameplayEffectContainerSpec& ContainerSpec);


	/**
	* FGSGameplayEffectContext
	*/

	// Returns TargetData
	UFUNCTION(BlueprintCallable, Category = "Ability|EffectContext", Meta = (DisplayName = "GetTargetData"))
	static FGameplayAbilityTargetDataHandle EffectContextGetTargetData(FGameplayEffectContextHandle EffectContext);

	// Adds TargetData
	UFUNCTION(BlueprintCallable, Category = "Ability|EffectContext", Meta = (DisplayName = "AddTargetData"))
	static void EffectContextAddTargetData(FGameplayEffectContextHandle EffectContextHandle, const FGameplayAbilityTargetDataHandle& TargetData);


	/**
	* FGameplayAbilityTargetDataHandle
	*/
	UFUNCTION(BlueprintCallable, Category = "Ability|TargetData")
	static void ClearTargetData(UPARAM(ref) FGameplayAbilityTargetDataHandle& TargetData);

	UFUNCTION(BlueprintCallable, Category="AuraAbilitySystemLibrary|CharacterClassDefaults")
	static void InitializeDefaultAttributes(const UObject* WorldContextObject, EAZCharacterClass CharacterClass, float Level, UAbilitySystemComponent* ASC);

	UFUNCTION(BlueprintCallable, Category="AuraAbilitySystemLibrary|CharacterClassDefaults")
	static UAZ_CharacterClassInfo* GetCharacterClassInfo(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "AZ|Debug", meta = (WorldContext = "WorldContextObject"))
	static void PrintDebugMessage(const UObject* WorldContextObject, const FString& Message, FColor Color = FColor::Cyan, float Duration = 15.f);

	
};