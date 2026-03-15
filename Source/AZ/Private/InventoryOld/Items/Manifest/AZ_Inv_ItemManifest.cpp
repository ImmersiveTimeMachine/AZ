// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryOld/Items/Manifest/AZ_Inv_ItemManifest.h"
#include "Items/AZ_Inv_InventoryItem.h"
#include "Items/AZ_Inv_ItemComponent.h"
#include "InventoryOld/Items/Fragments/AZ_Inv_ItemFragment.h"

UAZ_Inv_InventoryItem* FAZ_Inv_ItemManifest::Manifest(UObject* NewOuter)
{
	auto Item = NewObject<UAZ_Inv_InventoryItem>(NewOuter, UAZ_Inv_InventoryItem::StaticClass());
	Item->SetItemManifest(*this);
	for (auto& Fragment : Item->GetItemManifestMutable().GetFragmentsMutable())
	{
		Fragment.GetMutable().Manifest();
	}
	ClearFragments();

	return Item;
}

void FAZ_Inv_ItemManifest::AssimilateInventoryFragments(UAZ_Inv_CompositeBase* Composite) const
{
	const auto& InventoryItemFragments = GetAllFragmentsOfType<FAZ_Inv_InventoryItemFragment>();
	for (const auto* Fragment : InventoryItemFragments)
	{
		Composite->ApplyFunction([Fragment](UAZ_Inv_CompositeBase* Widget)
		{
			Fragment->Assimilate(Widget);
		});
	}
}

void FAZ_Inv_ItemManifest::SpawnPickupActor(const UObject* WorldContextObject, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	if (!IsValid(PickupActorClass) || !IsValid(WorldContextObject)) return;

	AActor* SpawnedActor = WorldContextObject->GetWorld()->SpawnActor<AActor>(PickupActorClass, SpawnLocation, SpawnRotation);
	if (!IsValid(SpawnedActor)) return;

	// Set the item manifest, item category, item type, etc.
	UAZ_Inv_ItemComponent* ItemComp = SpawnedActor->FindComponentByClass<UAZ_Inv_ItemComponent>();
	check(ItemComp);

	ItemComp->InitItemManifest(*this);
}

void FAZ_Inv_ItemManifest::ClearFragments()
{
	for (auto& Fragment : Fragments)
	{
		Fragment.Reset();
	}
	Fragments.Empty();
}

