

#include "Inventory/FastArray/AZ_Inv_FastArray.h"

#include "Inventory/Components/AZ_Inv_InventoryComponent.h"
#include "Inventory/Items/AZ_Inv_InventoryItem.h"
#include "Inventory/Items/AZ_Inv_ItemComponent.h"


TArray<UAZ_Inv_InventoryItem*> FAZ_Inv_InventoryFastArray::GetAllItems() const
{
	TArray<UAZ_Inv_InventoryItem*> Results;
	Results.Reserve(Entries.Num());
	for (const auto& Entry : Entries)
	{
		if (!IsValid(Entry.InventoryItem)) continue;
		Results.Add(Entry.InventoryItem);
	}
	return Results;
}

void FAZ_Inv_InventoryFastArray::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	UAZ_Inv_InventoryComponent* InventoryComponent = Cast<UAZ_Inv_InventoryComponent>(OwnerComponent);
	if (!IsValid(InventoryComponent)) return;

	for (int32 Index : RemovedIndices)
	{
		InventoryComponent->OnItemRemoved.Broadcast(Entries[Index].InventoryItem);
	}
}

void FAZ_Inv_InventoryFastArray::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	UAZ_Inv_InventoryComponent* InventoryComponent = Cast<UAZ_Inv_InventoryComponent>(OwnerComponent);
	if (!IsValid(InventoryComponent)) return;

	for (int32 Index :AddedIndices)
	{
		InventoryComponent->OnItemAdded.Broadcast(Entries[Index].InventoryItem);
	}
}

UAZ_Inv_InventoryItem* FAZ_Inv_InventoryFastArray::CreateInventoryEntryFromItemComponent(UAZ_Inv_ItemComponent* ItemComponent)
{
	check(OwnerComponent);
	AActor* OwnerActor = OwnerComponent->GetOwner();
	check(OwnerActor->HasAuthority()); // Player Controller

	const auto InventoryComponent = Cast<UAZ_Inv_InventoryComponent>(OwnerComponent);
	if (!IsValid(InventoryComponent)) return nullptr;

	FAZ_Inv_InventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.InventoryItem = ItemComponent->GetItemManifest().Manifest(OwnerActor);

	InventoryComponent->AddRepSubObjects(NewEntry.InventoryItem);
	MarkItemDirty(NewEntry);
	
	return NewEntry.InventoryItem;
}

UAZ_Inv_InventoryItem* FAZ_Inv_InventoryFastArray::AddInventoryItem(UAZ_Inv_InventoryItem* Item)
{
	check(OwnerComponent);
	AActor* OwnerActor = OwnerComponent->GetOwner();
	check(OwnerActor->HasAuthority());

	FAZ_Inv_InventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.InventoryItem = Item;

	MarkItemDirty(NewEntry);
	
	return NewEntry.InventoryItem;
}

void FAZ_Inv_InventoryFastArray::RemoveInventoryItem(UAZ_Inv_InventoryItem* Item)
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

UAZ_Inv_InventoryItem* FAZ_Inv_InventoryFastArray::FindFirstItemByTypeTag(const FGameplayTag& ItemTypeTag)
{
	auto* FoundItem = Entries.FindByPredicate([TagToCheck = ItemTypeTag](const FAZ_Inv_InventoryEntry& InventoryEntry)
	{
		return IsValid(InventoryEntry.InventoryItem) && InventoryEntry.InventoryItem->GetItemManifest().GetItemTypeTag().MatchesTagExact(TagToCheck);
	});
	return FoundItem ? FoundItem->InventoryItem : nullptr;
}
