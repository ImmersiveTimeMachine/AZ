// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/Items/AZ_Inv_InventoryItem.h"

#include "Inventory/Items/Fragments/AZ_Inv_ItemFragment.h"
#include "Net/UnrealNetwork.h"


void UAZ_Inv_InventoryItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	UObject::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, ItemManifest);
	DOREPLIFETIME(ThisClass, TotalStackCount);
}

bool UAZ_Inv_InventoryItem::IsStackable() const
{
	const FAZ_Inv_StackableFragment* Stackable = GetItemManifest().GetFragmentOfType<FAZ_Inv_StackableFragment>();
	return Stackable != nullptr;
}

bool UAZ_Inv_InventoryItem::IsConsumable() const
{
	return GetItemManifest().GetItemCategory() == EInv_ItemCategory::Consumable;
}

void UAZ_Inv_InventoryItem::SetItemManifest(const FAZ_Inv_ItemManifest& Manifest)
{
	ItemManifest = FInstancedStruct::Make<FAZ_Inv_ItemManifest>(Manifest);
}
