// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/AZ_InventoryItemInstance.h"

#include "Net/UnrealNetwork.h"


void UAZ_InventoryItemInstance::PostInitProperties()
{
	UObject::PostInitProperties();

	// Only generate for real instances (not the class default object)
	if (!HasAnyFlags(RF_ClassDefaultObject) && !InstanceId.IsValid())
	{
		InstanceId = FGuid::NewGuid();
	}

}

void UAZ_InventoryItemInstance::PostLoad()
{
	UObject::PostLoad();

	// If loaded without a valid GUID (older assets), generate one
	if (!HasAnyFlags(RF_ClassDefaultObject) && !InstanceId.IsValid())
	{
		InstanceId = FGuid::NewGuid();
	}

}

void UAZ_InventoryItemInstance::PostDuplicate(bool bDuplicateForPIE)
{
	UObject::PostDuplicate(bDuplicateForPIE);

	// Duplicates should get a new identity
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		InstanceId = FGuid::NewGuid();
	}

}

void UAZ_InventoryItemInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);


	DOREPLIFETIME(UAZ_InventoryItemInstance, TopLeftInGrid);
	DOREPLIFETIME(UAZ_InventoryItemInstance, ItemDefinition);
	DOREPLIFETIME(UAZ_InventoryItemInstance, InstanceId);

}
