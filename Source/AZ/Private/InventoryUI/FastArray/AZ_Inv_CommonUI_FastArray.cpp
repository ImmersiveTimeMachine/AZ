// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/FastArray/AZ_Inv_CommonUI_FastArray.h"

#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"

TArray<UAZ_Inv_CommonUI_InventoryItem*> FAZ_Inv_CommonUI_InventoryFastArray::GetAllItems() const
{
	TArray<UAZ_Inv_CommonUI_InventoryItem*> Results;
	Results.Reserve(Entries.Num());
	for (const auto& Entry : Entries)
	{
		if (!IsValid(Entry.InventoryItem)) continue;
		Results.Add(Entry.InventoryItem);
	}
	return Results;
}

void FAZ_Inv_CommonUI_InventoryFastArray::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	UAZ_Inv_CommonUI_InventoryComponent* InventoryComponent = Cast<UAZ_Inv_CommonUI_InventoryComponent>(OwnerComponent);
	if (!IsValid(InventoryComponent)) return;

	for (int32 Index : RemovedIndices)
	{
		InventoryComponent->OnItemRemoved.Broadcast(Entries[Index].InventoryItem);
	}
}

void FAZ_Inv_CommonUI_InventoryFastArray::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	UAZ_Inv_CommonUI_InventoryComponent* InventoryComponent = Cast<UAZ_Inv_CommonUI_InventoryComponent>(OwnerComponent);
	if (!IsValid(InventoryComponent)) return;

	for (int32 Index : AddedIndices)
	{
		InventoryComponent->OnItemAdded.Broadcast(Entries[Index].InventoryItem);
	}
}

UAZ_Inv_CommonUI_InventoryItem* FAZ_Inv_CommonUI_InventoryFastArray::CreateInventoryEntryFromItemComponent(UAZ_Inv_CommonUI_ItemComponent* ItemComponent)
{
	check(OwnerComponent);
	AActor* OwnerActor = OwnerComponent->GetOwner();
	check(OwnerActor->HasAuthority());

	const auto InventoryComponent = Cast<UAZ_Inv_CommonUI_InventoryComponent>(OwnerComponent);
	if (!IsValid(InventoryComponent)) return nullptr;

	FAZ_Inv_CommonUI_InventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.InventoryItem = ItemComponent->GetItemManifest().Manifest(OwnerActor);

	InventoryComponent->AddRepSubObjects(NewEntry.InventoryItem);
	MarkItemDirty(NewEntry);

	return NewEntry.InventoryItem;
}

UAZ_Inv_CommonUI_InventoryItem* FAZ_Inv_CommonUI_InventoryFastArray::AddInventoryItem(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	check(OwnerComponent);
	AActor* OwnerActor = OwnerComponent->GetOwner();
	check(OwnerActor->HasAuthority());

	FAZ_Inv_CommonUI_InventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.InventoryItem = Item;

	MarkItemDirty(NewEntry);

	return NewEntry.InventoryItem;
}

void FAZ_Inv_CommonUI_InventoryFastArray::RemoveInventoryItem(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	check(OwnerComponent);
	AActor* OwnerActor = OwnerComponent->GetOwner();
	check(OwnerActor->HasAuthority());

	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		if (Entries[EntryIndex].InventoryItem == Item)
		{
			Entries.RemoveAt(EntryIndex);
			MarkArrayDirty();
			break;
		}
	}
}

UAZ_Inv_CommonUI_InventoryItem* FAZ_Inv_CommonUI_InventoryFastArray::FindFirstItemByTypeTag(const FGameplayTag& ItemTypeTag)
{
	auto* FoundItem = Entries.FindByPredicate([TagToCheck = ItemTypeTag](const FAZ_Inv_CommonUI_InventoryEntry& InventoryEntry)
	{
		return IsValid(InventoryEntry.InventoryItem) && InventoryEntry.InventoryItem->GetItemManifest().GetItemTypeTag().MatchesTagExact(TagToCheck);
	});
	return FoundItem ? FoundItem->InventoryItem : nullptr;
}
