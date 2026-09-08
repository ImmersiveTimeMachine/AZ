// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"

#include "Net/UnrealNetwork.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "GameFramework/Actor.h"

void UAZ_Inv_CommonUI_InventoryItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	UObject::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, ItemManifest);
	DOREPLIFETIME(ThisClass, TotalStackCount);
	DOREPLIFETIME(ThisClass, InstanceState);
}

bool UAZ_Inv_CommonUI_InventoryItem::IsStackable() const
{
	return GetItemManifest().IsStackable();
}

bool UAZ_Inv_CommonUI_InventoryItem::IsConsumable() const
{
	return GetItemManifest().GetItemCategory() == EInv_ItemCategory::Consumable;
}

bool UAZ_Inv_CommonUI_InventoryItem::IsMagazine() const
{
	return GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_MagazineFragment>() != nullptr;
}

bool UAZ_Inv_CommonUI_InventoryItem::IsWeapon() const
{
	const auto* Weapon = GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
	return Weapon && Weapon->IsWeaponItem();
}

FGameplayTag UAZ_Inv_CommonUI_InventoryItem::GetWeaponProfileTag() const
{
	const auto* Weapon = GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
	return Weapon ? Weapon->WeaponTag : FGameplayTag::EmptyTag;
}

int32 UAZ_Inv_CommonUI_InventoryItem::GetMagazineCapacity() const
{
	const auto* Magazine = GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_MagazineFragment>();
	return Magazine ? Magazine->Capacity : 0;
}

void UAZ_Inv_CommonUI_InventoryItem::InitializeInstance(const FAZ_InventoryItemState& State, int32 StackCount)
{
	InstanceState = State;
	TotalStackCount = IsStackable() ? FMath::Max(1, StackCount) : 1;
}

void UAZ_Inv_CommonUI_InventoryItem::OnRep_ItemChanged()
{
	if (AActor* Owner = GetTypedOuter<AActor>())
	{
		if (auto* Inventory = Owner->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>())
		{
			Inventory->NotifyInventoryChanged();
		}
	}
}

void UAZ_Inv_CommonUI_InventoryItem::SetItemManifest(const FAZ_Inv_CommonUI_ItemManifest& Manifest)
{
	ItemManifest = FInstancedStruct::Make<FAZ_Inv_CommonUI_ItemManifest>(Manifest);
}

const FAZ_Inv_CommonUI_ItemManifest& UAZ_Inv_CommonUI_InventoryItem::GetItemManifest() const
{
	if (const FAZ_Inv_CommonUI_ItemManifest* ManifestPtr = ItemManifest.GetPtr<FAZ_Inv_CommonUI_ItemManifest>())
	{
		return *ManifestPtr;
	}

	ensureMsgf(false, TEXT("ItemManifestData is invalid or wrong type, returning default manifest"));
	static const FAZ_Inv_CommonUI_ItemManifest DefaultManifest;
	return DefaultManifest;
}
FAZ_Inv_CommonUI_ItemManifest& UAZ_Inv_CommonUI_InventoryItem::GetItemManifestMutable()
{
	ensure(ItemManifest.IsValid());
	return ItemManifest.GetMutable<FAZ_Inv_CommonUI_ItemManifest>();
}
