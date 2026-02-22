// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/AZ_InventoryItemDefinition.h"


UAZ_InventoryItemDefinition::UAZ_InventoryItemDefinition()
{
	// Set a default asset type. This can be overridden by child classes if needed.
	ItemType = FPrimaryAssetType("Item");
}

FPrimaryAssetId UAZ_InventoryItemDefinition::GetPrimaryAssetId() const
{
	// This is the unique identifier for this asset, used by the Asset Manager.
	return FPrimaryAssetId(ItemType, GetFName());
}


