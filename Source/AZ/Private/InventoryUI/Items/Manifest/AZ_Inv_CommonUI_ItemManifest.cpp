// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/Items/Manifest/AZ_Inv_CommonUI_ItemManifest.h"

#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_CompositeBaseWidget.h"

UAZ_Inv_CommonUI_InventoryItem* FAZ_Inv_CommonUI_ItemManifest::Manifest(UObject* NewOuter)
{
	UAZ_Inv_CommonUI_InventoryItem* Item = NewObject<UAZ_Inv_CommonUI_InventoryItem>(NewOuter, UAZ_Inv_CommonUI_InventoryItem::StaticClass());
	Item->SetItemManifest(*this);
	for (auto& Fragment : Item->GetItemManifestMutable().GetFragmentsMutable())
	{
		Fragment.GetMutable().Manifest();
	}
	ClearFragments();

	return Item;
}

void FAZ_Inv_CommonUI_ItemManifest::AssimilateInventoryFragments(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const
{
	const auto& InventoryItemFragments = GetAllFragmentsOfType<FAZ_Inv_CommonUI_InventoryItem_Fragment>();
	for (const auto* Fragment : InventoryItemFragments)
	{
		Composite->ApplyFunction([Fragment](UAZ_Inv_CommonUI_CompositeBaseWidget* Widget)
		{
			Fragment->Assimilate(Widget);
		});
	}
}

void FAZ_Inv_CommonUI_ItemManifest::SpawnPickupActor(const UObject* WorldContextObject, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	if (!IsValid(PickupActorClass) || !IsValid(WorldContextObject)) return;

	AActor* SpawnedActor = WorldContextObject->GetWorld()->SpawnActor<AActor>(PickupActorClass, SpawnLocation, SpawnRotation);
	if (!IsValid(SpawnedActor)) return;

	// Set the item manifest, item category, item type, etc.
	UAZ_Inv_CommonUI_ItemComponent* ItemComp = SpawnedActor->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>();
	check(ItemComp);

	ItemComp->InitItemManifest(*this);
}

void FAZ_Inv_CommonUI_ItemManifest::ClearFragments()
{
	for (auto& Fragment : Fragments)
	{
		Fragment.Reset();
	}
	Fragments.Empty();
}
