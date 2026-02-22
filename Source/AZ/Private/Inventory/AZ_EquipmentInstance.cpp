// UAZ_EquipmentInstance.cpp

#include "Inventory/AZ_EquipmentInstance.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Inventory/AZ_InventoryItemDefinition.h"
#include "Inventory/AZ_InventoryItemInstance.h"

void UAZ_EquipmentInstance::InitializeFromInventoryItem(UAZ_InventoryItemInstance* InInventoryInstance, FName InSlot)
{
	if (!ensureAlways(GetOuter() && GetOuter()->HasAnyFlags(RF_ClassDefaultObject) == false))
	{
		return;
	}

	SourceInventoryInstance = InInventoryInstance;
	ItemDefinition = InInventoryInstance ? InInventoryInstance->ItemDefinition : nullptr;
	EquipmentSlot = InSlot;
	InstanceId = FGuid::NewGuid();
}

void UAZ_EquipmentInstance::SetEquipped(bool bInEquipped)
{
	if (bIsEquipped == bInEquipped)
	{
		return;
	}
	bIsEquipped = bInEquipped;

	// If hosted by an actor/component, nudge replication
	if (AActor* OwnerActor = GetTypedOuter<AActor>())
	{
		OwnerActor->ForceNetUpdate();
	}
}

void UAZ_EquipmentInstance::OnRep_Equipped()
{
	// Hook: update visuals, notify UI, play sounds, etc.
	// Intentionally empty by default.
}

void UAZ_EquipmentInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Typically owner-only for equipment
	DOREPLIFETIME_CONDITION(UAZ_EquipmentInstance, ItemDefinition, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UAZ_EquipmentInstance, SourceInventoryInstance, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UAZ_EquipmentInstance, EquipmentSlot, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UAZ_EquipmentInstance, bIsEquipped, COND_OwnerOnly);
}

UWorld* UAZ_EquipmentInstance::GetWorld() const
{
	if (const UObject* Outer = GetOuter())
	{
		if (const AActor* AsActor = Cast<AActor>(Outer))
		{
			return AsActor->GetWorld();
		}
		if (const UActorComponent* AsComp = Cast<UActorComponent>(Outer))
		{
			return AsComp->GetWorld();
		}
	}
	return nullptr;
}