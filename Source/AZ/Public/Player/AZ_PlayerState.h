// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "AZ_PlayerState.generated.h"

// Forward declarations
class UAZ_InventoryComponent;
class UAZ_LevelUpInfo;
class UAttributeSet;
class UAbilitySystemComponent;

// Multicast delegates for stat changes
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerStatChanged, int32 /*StatValue*/)
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLevelChanged, int32 /*StatValue*/, bool /*bLevelUp*/)

UCLASS()
class AZ_API AAZ_PlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// ========================================
	// CONSTRUCTION & INITIALIZATION
	// ========================================

	AAZ_PlayerState();

	// ========================================
	// CORE OVERRIDES
	// ========================================

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ========================================
	// ATTRIBUTE SYSTEM
	// ========================================

	UAttributeSet* GetAttributeSet() const { return AttributeSet; }
	void SetAttributeSet(UAttributeSet* InAttributeSet);

	// ========================================
	// PROGRESSION CONFIGURATION
	// ========================================

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Progression")
	TObjectPtr<UAZ_LevelUpInfo> LevelUpInfo;

	// ========================================
	// INVENTORY SYSTEM
	// ========================================

	// ========================================
	// INVENTORY SYSTEM
	// ========================================
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AZ|Inventory")
	UAZ_InventoryComponent* InventoryComponent;

	UFUNCTION(BlueprintPure, Category = "AZ|Inventory")
	UAZ_InventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	// ========================================
	// STAT DELEGATES
	// ========================================

	FOnPlayerStatChanged OnXPChangedDelegate;
	FOnLevelChanged OnLevelChangedDelegate;
	FOnPlayerStatChanged OnAttributePointsChangedDelegate;
	FOnPlayerStatChanged OnSpellPointsChangedDelegate;

	// ========================================
	// STAT ACCESSORS (GETTERS)
	// ========================================

	FORCEINLINE int32 GetPlayerLevel() const { return Level; }
	FORCEINLINE int32 GetXP() const { return XP; }
	FORCEINLINE int32 GetAttributePoints() const { return AttributePoints; }
	FORCEINLINE int32 GetSpellPoints() const { return SpellPoints; }

	// ========================================
	// STAT MODIFIERS (ADDITIVE)
	// ========================================

	void AddToXP(int32 InXP);
	void AddToLevel(int32 InLevel);
	void AddToAttributePoints(int32 InPoints);
	void AddToSpellPoints(int32 InPoints);

	// ========================================
	// STAT MODIFIERS (ABSOLUTE)
	// ========================================

	void SetXP(int32 InXP);
	void SetLevel(int32 InLevel);
	void SetAttributePoints(int32 InPoints);
	void SetSpellPoints(int32 InPoints);

protected:
	// ========================================
	// PROTECTED PROPERTIES
	// ========================================

	// Ability System Components
	UPROPERTY(VisibleAnywhere, Category = "AZ|AbilitySystem")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UAttributeSet> AttributeSet;

private:
	// ========================================
	// REPLICATED PLAYER STATS
	// ========================================

	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_Level, Category = "AZ|Stats")
	int32 Level = 1;

	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_XP, Category = "AZ|Stats")
	int32 XP = 0;

	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_AttributePoints, Category = "AZ|Stats")
	int32 AttributePoints = 0;

	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_SpellPoints, Category = "AZ|Stats")
	int32 SpellPoints = 0;

	// ========================================
	// REPLICATION CALLBACKS
	// ========================================

	UFUNCTION()
	void OnRep_Level(int32 OldLevel);

	UFUNCTION()
	void OnRep_XP(int32 OldXP);

	UFUNCTION()
	void OnRep_AttributePoints(int32 OldAttributePoints);

	UFUNCTION()
	void OnRep_SpellPoints(int32 OldSpellPoints);
	
};
