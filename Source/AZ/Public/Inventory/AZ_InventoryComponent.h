// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_InventoryItemData.h"
#include "Components/ActorComponent.h"
#include "Weapon/AZ_Weapon.h"
#include "AZ_InventoryComponent.generated.h"


class UAZ_InventoryItemInstance;

USTRUCT()
struct FSlotByKey
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Key_1;

	UPROPERTY()
	FGameplayTag Key_2;

	UPROPERTY()
	FGameplayTag Key_3;

	UPROPERTY()
	FGameplayTag Key_4;
};

struct FInventorySlot;

// Delegate that can be used by the UI to update when the inventory changes.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryUpdated);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FItemAddedToInventory, FGuid, ItemInstanceId);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class AZ_API UAZ_InventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UAZ_InventoryComponent();
	
	UFUNCTION(BlueprintCallable, Category = "AZ|Inventory")
	void AddItemToInventory(UAZ_InventoryItemDefinition* ItemDef);

	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override ;
	
	/** Removes an item instance from the inventory.
	 * Must be called on the server.
	 */
	UFUNCTION()
	void RemoveItem(UAZ_InventoryItemInstance* ItemInstance);

	UFUNCTION(Server, Reliable)
	void Server_RemoveItem(UAZ_InventoryItemInstance* ItemInstance);

	/** Delegate that broadcasts on the client when the inventory list changes. */
	UPROPERTY(BlueprintAssignable, Category = "AZ|Inventory")
	FOnInventoryUpdated OnInventoryUpdated;

	FItemAddedToInventory OnItemAddedToInventory;
	
	/** Returns the complete list of all item instances in the inventory. */
	UFUNCTION(BlueprintPure, Category = "AZ|Inventory")
	TArray<UAZ_InventoryItemInstance*> GetInventoryList() const { return InventoryItemInstances; }

	UFUNCTION(BlueprintPure, Category = "AZ|Inventory")
	TArray<FAZ_InventoryItemData> GetInventoryItems() const { return InventoryItems; }

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentInventoryItemInstanceID)
	FGuid CurrentInventoryItemInstanceID;

	/** This function is called on clients when the AllItems array is replicated from the server. */
	UFUNCTION()
	void OnRep_AllItems();

	// React to grid size replication changes
	UFUNCTION()
	void OnRep_GridSize();

	UFUNCTION()
	void OnRep_CurrentInventoryItemInstanceID();

	// Local helper used by both server (after mutation) and client (from OnRep) to rebuild and notify UI
	void RefreshLocalStateAfterItemsChanged();

	// Optional: simple guard for server-side mutation authorization (owner-only)
	bool CanModifyInventory() const;

	// --- Grid Properties ---
	UPROPERTY(EditDefaultsOnly, Replicated, Category = "AZ|Inventory|Grid")
	int32 GridWidth;

	UPROPERTY(EditDefaultsOnly, Replicated, Category = "AZ|Inventory|Grid")
	int32 GridHeight;

	/** The replicated list of all unique item instances. This is the source of truth for the inventory. */
	UPROPERTY(ReplicatedUsing = OnRep_AllItems)
	TArray<UAZ_InventoryItemInstance*> InventoryItemInstances;

	UFUNCTION(Server, Reliable, Category = "AZ|Inventory")
	void ServerAddItemToInventory(UAZ_InventoryItemDefinition* ItemDef);

	UPROPERTY(ReplicatedUsing = OnRep_InventoryItems)
	TArray<FAZ_InventoryItemData> InventoryItems;

	UFUNCTION()
	void OnRep_InventoryItems();


private:
	/** A non-replicated 1D array representing our 2D grid. Used for placement logic. It is reconstructed from the AllItems list. */
	UPROPERTY()
	TArray<UAZ_InventoryItemInstance*> GridSlots;

	// --- Helper Functions ---
	bool FindEmptySpaceForItem(const UAZ_InventoryItemDefinition* ItemDef, FIntPoint& OutTopLeft) const;
	bool CanPlaceItemAt(const UAZ_InventoryItemDefinition* ItemDef, FIntPoint TopLeft) const;
	void PlaceItemOnGrid(UAZ_InventoryItemInstance* ItemInstance);
	void ClearItemFromGrid(UAZ_InventoryItemInstance* ItemInstance);
	int32 GetIndexFromCoords(int32 X, int32 Y) const;

	/** Reconstructs the visual grid from the replicated AllItems array. */
	void RebuildGrid();
	
};
