#include "Inventory/AZ_QuickBarComponent.h"

#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "Net/UnrealNetwork.h"

UAZ_QuickBarComponent::UAZ_QuickBarComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAZ_QuickBarComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAZ_QuickBarComponent, SlotItemIds);
}

UAZ_Inv_CommonUI_InventoryComponent* UAZ_QuickBarComponent::GetInventory() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>() : nullptr;
}

UAZ_Inv_CommonUI_EquipmentComponent* UAZ_QuickBarComponent::GetEquipment() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>() : nullptr;
}

const FAZ_QuickSlot* UAZ_QuickBarComponent::GetSlotDefinition(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? &Slots[SlotIndex] : nullptr;
}

UAZ_Inv_CommonUI_InventoryItem* UAZ_QuickBarComponent::GetBoundItem(int32 SlotIndex) const
{
	const UAZ_Inv_CommonUI_InventoryComponent* Inventory = GetInventory();
	return Inventory && SlotItemIds.IsValidIndex(SlotIndex) ? Inventory->FindItemById(SlotItemIds[SlotIndex]) : nullptr;
}

int32 UAZ_QuickBarComponent::GetActiveSlotIndex() const
{
	const UAZ_Inv_CommonUI_EquipmentComponent* Equipment = GetEquipment();
	if (!Equipment) return INDEX_NONE;
	if (const UAZ_Inv_CommonUI_InventoryItem* Item = Equipment->GetActiveItem())
	{
		return SlotItemIds.IndexOfByKey(Item->GetInstanceId());
	}
	return Equipment->GetActiveIntrinsicSlotIndex();
}

bool UAZ_QuickBarComponent::CanBindItem(int32 SlotIndex, const UAZ_Inv_CommonUI_InventoryItem* Item) const
{
	const FAZ_QuickSlot* Slot = GetSlotDefinition(SlotIndex);
	const UAZ_Inv_CommonUI_InventoryComponent* Inventory = GetInventory();
	return Slot && Slot->bInventoryBacked && IsValid(Item) && Inventory && Inventory->ContainsItem(Item)
		&& Item->GetInstanceId().IsValid()
		&& Item->GetLocation() == EAZ_InventoryItemLocation::Backpack
		&& Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_EquipmentFragment>()
		&& (!Slot->InventoryItemType.IsValid() || Item->GetItemManifest().GetItemTypeTag().MatchesTag(Slot->InventoryItemType));
}

bool UAZ_QuickBarComponent::BindItemToSlot(int32 SlotIndex, UAZ_Inv_CommonUI_InventoryItem* Item)
{
	if (!CanBindItem(SlotIndex, Item)) return false;
	if (!GetOwner()->HasAuthority())
	{
		Server_BindItem(SlotIndex, Item->GetInstanceId());
		return true;
	}
	SlotItemIds.SetNum(Slots.Num());
	// A physical item has one loadout binding, regardless of how many slots accept its type.
	for (FGuid& BoundId : SlotItemIds)
	{
		if (BoundId == Item->GetInstanceId()) BoundId.Invalidate();
	}
	SlotItemIds[SlotIndex] = Item->GetInstanceId();
	GetOwner()->ForceNetUpdate();
	return true;
}

void UAZ_QuickBarComponent::Server_BindItem_Implementation(int32 SlotIndex, FGuid ItemId)
{
	if (UAZ_Inv_CommonUI_InventoryComponent* Inventory = GetInventory()) BindItemToSlot(SlotIndex, Inventory->FindItemById(ItemId));
}

void UAZ_QuickBarComponent::BindSelectedItem(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	if (!GetOwner()->HasAuthority() || !IsValid(Item)) return;
	if (SlotItemIds.Contains(Item->GetInstanceId())) return;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (CanBindItem(Index, Item))
		{
			BindItemToSlot(Index, Item);
			return;
		}
	}
}

void UAZ_QuickBarComponent::Select(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex) || !GetOwner()) return;
	if (GetOwner()->HasAuthority()) SelectInternal(SlotIndex);
	else Server_Select(SlotIndex);
}

void UAZ_QuickBarComponent::Server_Select_Implementation(int32 SlotIndex)
{
	SelectInternal(SlotIndex);
}

void UAZ_QuickBarComponent::SelectInternal(int32 SlotIndex)
{
	const FAZ_QuickSlot* Slot = GetSlotDefinition(SlotIndex);
	UAZ_Inv_CommonUI_EquipmentComponent* Equipment = GetEquipment();
	if (!Slot || !Equipment) return;
	if (GetActiveSlotIndex() == SlotIndex)
	{
		Equipment->RequestUnequipItem(Equipment->GetActiveItem());
	}
	else if (Slot->bInventoryBacked)
	{
		Equipment->RequestEquipItem(GetBoundItem(SlotIndex));
	}
	else
	{
		Equipment->RequestEquipIntrinsic(SlotIndex);
	}
}

void UAZ_QuickBarComponent::Cycle(int32 Direction)
{
	if (Slots.IsEmpty()) return;
	const int32 Active = GetActiveSlotIndex();
	int32 Index = Active == INDEX_NONE ? (Direction > 0 ? -1 : 0) : Active;
	for (int32 Attempt = 0; Attempt < Slots.Num(); ++Attempt)
	{
		Index = (Index + Direction + Slots.Num()) % Slots.Num();
		if (Index != Active && (!Slots[Index].bInventoryBacked || GetBoundItem(Index)))
		{
			Select(Index);
			return;
		}
	}
}

void UAZ_QuickBarComponent::CycleNext() { Cycle(1); }
void UAZ_QuickBarComponent::CyclePrev() { Cycle(-1); }

void UAZ_QuickBarComponent::OnInventoryChanged()
{
	UAZ_Inv_CommonUI_InventoryComponent* Inventory = GetInventory();
	if (!GetOwner()->HasAuthority() || !Inventory) return;
	SlotItemIds.SetNum(Slots.Num());
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (!Slots[Index].bInventoryBacked) continue;
		if (CanBindItem(Index, GetBoundItem(Index))) continue;
		SlotItemIds[Index].Invalidate();
		for (UAZ_Inv_CommonUI_InventoryItem* Item : Inventory->GetItems())
		{
			if (CanBindItem(Index, Item) && !SlotItemIds.Contains(Item->GetInstanceId()))
			{
				SlotItemIds[Index] = Item->GetInstanceId();
				break;
			}
		}
	}
	GetOwner()->ForceNetUpdate();
}

void UAZ_QuickBarComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UAZ_Inv_CommonUI_InventoryComponent* Inventory = GetInventory())
	{
		Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::OnInventoryChanged);
		OnInventoryChanged();
	}
}
