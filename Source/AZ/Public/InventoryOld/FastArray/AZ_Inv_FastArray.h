#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"


#include "AZ_Inv_FastArray.generated.h"

struct FAZ_Inv_InventoryFastArray;
struct FGameplayTag;
class UAZ_Inv_ItemComponent;
class UAZ_Inv_InventoryItem;
class UAZ_Inv_InventoryComponent;

/**
 * Represents an individual entry within an inventory system.
 * Inherits from FFastArraySerializerItem for use in multiplayer scenarios to efficiently
 * synchronize data between server and clients.
 *
 * This struct is designed to be used alongside custom Unreal Engine inventory systems,
 * allowing for the storage and management of inventory items.
 */

USTRUCT(BlueprintType)
struct FAZ_Inv_InventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	
	FAZ_Inv_InventoryEntry(){}

private:

	friend FAZ_Inv_InventoryFastArray;
	friend UAZ_Inv_InventoryComponent;
	
	UPROPERTY()
	TObjectPtr<UAZ_Inv_InventoryItem> InventoryItem = nullptr;
	
};

//----------------------------------------------------------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FAZ_Inv_InventoryFastArray : public FFastArraySerializer
{

	GENERATED_BODY()

	FAZ_Inv_InventoryFastArray() : OwnerComponent(nullptr) {};
	FAZ_Inv_InventoryFastArray(UActorComponent* InOwnerComponent) : OwnerComponent(InOwnerComponent) {};

	TArray<UAZ_Inv_InventoryItem*> GetAllItems() const;

	// FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	// End FFastArraySerializer contract

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FastArrayDeltaSerialize<FAZ_Inv_InventoryEntry,FAZ_Inv_InventoryFastArray>(Entries, DeltaParams, *this);
	}

	UAZ_Inv_InventoryItem* CreateInventoryEntryFromItemComponent(UAZ_Inv_ItemComponent* ItemComponent);
	UAZ_Inv_InventoryItem* AddInventoryItem(UAZ_Inv_InventoryItem* Item);
	void RemoveInventoryItem(UAZ_Inv_InventoryItem* Item);
	UAZ_Inv_InventoryItem* FindFirstItemByTypeTag(const FGameplayTag& ItemTypeTag);

private:

	friend UAZ_Inv_InventoryComponent;

	UPROPERTY()
	TArray<FAZ_Inv_InventoryEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UActorComponent> OwnerComponent;
	
};

template<>
struct TStructOpsTypeTraits<FAZ_Inv_InventoryFastArray> : TStructOpsTypeTraitsBase2<FAZ_Inv_InventoryFastArray>
{
	enum { WithNetDeltaSerializer = true };
};
