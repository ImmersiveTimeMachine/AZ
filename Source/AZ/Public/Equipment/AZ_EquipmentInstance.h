// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AZ_EquipmentInstance.generated.h"

class UAZ_InventoryItemDefinition;
class UAZ_InventoryItemInstance;
/**
 * 
 */
UCLASS(BlueprintType, DefaultToInstanced, EditInlineNew)
class AZ_API UAZ_EquipmentInstance : public UObject
{
	GENERATED_BODY()

public:
	// Subobjects should explicitly opt-in to networking.
	virtual bool IsSupportedForNetworking() const override { return true; }

	// Lightweight identity (optional, but handy for UI/logic)
	UFUNCTION(BlueprintPure, Category="Equipment")
	FGuid GetInstanceId() const { return InstanceId; }

	// Initialize once when created (server-side)
	UFUNCTION(BlueprintCallable, Category="Equipment")
	void InitializeFromInventoryItem(UAZ_InventoryItemInstance* InInventoryInstance, FName InSlot);

	// Equip/Unequip lifecycle toggles (server-side; will replicate state)
	UFUNCTION(BlueprintCallable, Category="Equipment")
	void SetEquipped(bool bInEquipped);

	// Accessors
	UFUNCTION(BlueprintPure, Category="Equipment")
	UAZ_InventoryItemDefinition* GetItemDefinition() const { return ItemDefinition; }

	UFUNCTION(BlueprintPure, Category="Equipment")
	UAZ_InventoryItemInstance* GetSourceInventoryInstance() const { return SourceInventoryInstance; }

	UFUNCTION(BlueprintPure, Category="Equipment")
	FName GetEquipmentSlot() const { return EquipmentSlot; }

	UFUNCTION(BlueprintPure, Category="Equipment")
	bool IsEquipped() const { return bIsEquipped; }

	// UObject
	virtual UWorld* GetWorld() const override;

protected:
	// Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Rep notifies (owner-only by default)
	UFUNCTION()
	void OnRep_Equipped();

protected:
	// Underlying data driving visuals/stats (usually from the inventory definition)
	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Equipment")
	TObjectPtr<UAZ_InventoryItemDefinition> ItemDefinition = nullptr;

	// Optional back-reference to the inventory instance that produced this equipment
	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Equipment")
	TObjectPtr<UAZ_InventoryItemInstance> SourceInventoryInstance = nullptr;

	// Which slot we live in (e.g., "Head", "Chest", "PrimaryWeapon")
	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Equipment")
	FName EquipmentSlot;

	// Runtime identity (not required to replicate; derive as needed)
	UPROPERTY(VisibleInstanceOnly, Category="Equipment")
	FGuid InstanceId;

	// Equipped state replicated to owner
	UPROPERTY(ReplicatedUsing=OnRep_Equipped, VisibleInstanceOnly, Category="Equipment")
	bool bIsEquipped = false;

};
