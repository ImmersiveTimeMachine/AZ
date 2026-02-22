// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "AZ_Inv_CommonUI_FastArray.generated.h"


struct FGameplayTag;
class UAZ_Inv_CommonUI_ItemComponent;
class UAZ_Inv_CommonUI_InventoryComponent;
struct FAZ_Inv_CommonUI_InventoryFastArray;
class UAZ_Inv_CommonUI_InventoryItem;

USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_InventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	
	FAZ_Inv_CommonUI_InventoryEntry(){}

private:

	friend FAZ_Inv_CommonUI_InventoryFastArray;
	friend UAZ_Inv_CommonUI_InventoryComponent;	
	
	UPROPERTY()
	TObjectPtr<UAZ_Inv_CommonUI_InventoryItem> InventoryItem = nullptr;
	
};

//----------------------------------------------------------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FAZ_Inv_CommonUI_InventoryFastArray : public FFastArraySerializer
{

	GENERATED_BODY()

	FAZ_Inv_CommonUI_InventoryFastArray() : OwnerComponent(nullptr) {};
	FAZ_Inv_CommonUI_InventoryFastArray(UActorComponent* InOwnerComponent) : OwnerComponent(InOwnerComponent) {};

	TArray<UAZ_Inv_CommonUI_InventoryItem*> GetAllItems() const;

	// FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	// End FFastArraySerializer contract

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FastArrayDeltaSerialize<FAZ_Inv_CommonUI_InventoryEntry,FAZ_Inv_CommonUI_InventoryFastArray>(Entries, DeltaParams, *this);
	}

	UAZ_Inv_CommonUI_InventoryItem* CreateInventoryEntryFromItemComponent(UAZ_Inv_CommonUI_ItemComponent* ItemComponent);
	UAZ_Inv_CommonUI_InventoryItem* AddInventoryItem(UAZ_Inv_CommonUI_InventoryItem* Item);
	void RemoveInventoryItem(UAZ_Inv_CommonUI_InventoryItem* Item);
	UAZ_Inv_CommonUI_InventoryItem* FindFirstItemByTypeTag(const FGameplayTag& ItemTypeTag);

private:

	friend UAZ_Inv_CommonUI_InventoryComponent;

	UPROPERTY()
	TArray<FAZ_Inv_CommonUI_InventoryEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UActorComponent> OwnerComponent;
	
};

template<>
struct TStructOpsTypeTraits<FAZ_Inv_CommonUI_InventoryFastArray> : TStructOpsTypeTraitsBase2<FAZ_Inv_CommonUI_InventoryFastArray>
{
	enum { WithNetDeltaSerializer = true };
};
