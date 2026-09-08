// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/Items/Manifest/AZ_Inv_CommonUI_ItemManifest.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_CompositeBaseWidget.h"

UAZ_Inv_CommonUI_InventoryItem* FAZ_Inv_CommonUI_ItemManifest::Manifest(UObject* NewOuter)
{
	UAZ_Inv_CommonUI_InventoryItem* Item = NewObject<UAZ_Inv_CommonUI_InventoryItem>(NewOuter, UAZ_Inv_CommonUI_InventoryItem::StaticClass());
	Item->SetItemManifest(*this);
	for (auto& Fragment : Item->GetItemManifestMutable().GetFragmentsMutable())
	{
		if (Fragment.IsValid())
		{
			Fragment.GetMutable().Manifest();
		}
	}
	return Item;
}

bool FAZ_Inv_CommonUI_ItemManifest::IsStackable() const
{
	// Stateful firearm and magazine instances may never be merged, even if old demo data has a stack fragment.
	if (GetFragmentOfType<FAZ_Inv_CommonUI_MagazineFragment>()) return false;
	if (const auto* Weapon = GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>(); Weapon && Weapon->IsWeaponItem()) return false;
	return GetFragmentOfType<FAZ_Inv_CommonUI_Stackable_Fragment>() != nullptr;
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

AActor* FAZ_Inv_CommonUI_ItemManifest::SpawnPickupActor(const UObject* WorldContextObject, const FVector& SpawnLocation, const FRotator& SpawnRotation) const
{
	if (!IsValid(PickupActorClass) || !IsValid(WorldContextObject) || !WorldContextObject->GetWorld()) return nullptr;

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	AActor* SpawnedActor = WorldContextObject->GetWorld()->SpawnActor<AActor>(PickupActorClass, SpawnLocation, SpawnRotation, SpawnParameters);
	if (!IsValid(SpawnedActor)) return nullptr;

	// Set the item manifest, item category, item type, etc.
	UAZ_Inv_CommonUI_ItemComponent* ItemComp = SpawnedActor->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>();
	if (!ItemComp)
	{
		SpawnedActor->Destroy();
		return nullptr;
	}

	ItemComp->InitItemManifest(*this);
	return SpawnedActor;
}

void FAZ_Inv_CommonUI_ItemManifest::ClearFragments()
{
	for (auto& Fragment : Fragments)
	{
		if (Fragment.IsValid())
		{
			Fragment.Reset();
		}
	}
	Fragments.Empty();
}
