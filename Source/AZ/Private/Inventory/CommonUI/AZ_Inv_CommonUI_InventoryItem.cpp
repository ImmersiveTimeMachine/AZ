// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/CommonUI/AZ_Inv_CommonUI_InventoryItem.h"

#include "Net/UnrealNetwork.h"

void UAZ_Inv_CommonUI_InventoryItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	UObject::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, ItemManifest);
	DOREPLIFETIME(ThisClass, TotalStackCount);
}

bool UAZ_Inv_CommonUI_InventoryItem::IsStackable() const
{
	const FAZ_Inv_CommonUI_Stackable_Fragment* Stackable = GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_Stackable_Fragment>();
	return Stackable != nullptr;
}

bool UAZ_Inv_CommonUI_InventoryItem::IsConsumable() const
{
	return GetItemManifest().GetItemCategory() == EInv_ItemCategory::Consumable;
}

void UAZ_Inv_CommonUI_InventoryItem::SetItemManifest(const FAZ_Inv_CommonUI_ItemManifest& Manifest)
{
	ItemManifest = FInstancedStruct::Make<FAZ_Inv_CommonUI_ItemManifest>(Manifest);
}

FAZ_Inv_CommonUI_ItemManifest UAZ_Inv_CommonUI_InventoryItem::GetItemManifest() const
{
	// Add a check to make invalid state explicit
	if (!ensureMsgf(ItemManifest.IsValid(), TEXT("ItemManifestData is invalid, returning default manifest")))
	{
		static const FAZ_Inv_CommonUI_ItemManifest DefaultManifest;
		return DefaultManifest;
	}
	return ItemManifest.Get<FAZ_Inv_CommonUI_ItemManifest>();
}

FAZ_Inv_CommonUI_ItemManifest& UAZ_Inv_CommonUI_InventoryItem::GetItemManifestMutable()
{
	ensure(ItemManifest.IsValid());
	return ItemManifest.GetMutable<FAZ_Inv_CommonUI_ItemManifest>();
}

