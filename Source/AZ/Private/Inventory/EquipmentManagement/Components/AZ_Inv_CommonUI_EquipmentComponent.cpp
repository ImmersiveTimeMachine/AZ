// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/EquipmentManagement/Components/AZ_Inv_CommonUI_EquipmentComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/CommonUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "Inventory/CommonUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "Inventory/EquipmentManagement/EquipActor/AZ_Inv_EquipActor.h"
#include "Inventory/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h"
#include "Inventory/Widgets/Utils/AZ_Inv_InventoryStatics.h"


UAZ_Inv_CommonUI_EquipmentComponent::UAZ_Inv_CommonUI_EquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAZ_Inv_CommonUI_EquipmentComponent::SetOwningSkeletalMesh(USkeletalMeshComponent* OwningMesh)
{
	OwningSkeletalMesh = OwningMesh;
}

void UAZ_Inv_CommonUI_EquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	InitPlayerController();
}

void UAZ_Inv_CommonUI_EquipmentComponent::OnItemEquipped(UAZ_Inv_CommonUI_InventoryItem* EquippedItem)
{
	if (!IsValid(EquippedItem)) return;
	if (!OwningPlayerController->HasAuthority()) return;

	FAZ_Inv_CommonUI_ItemManifest& ItemManifest = EquippedItem->GetItemManifestMutable();
	FAZ_Inv_CommonUI_EquipmentFragment* EquipmentFragment = ItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>();
	if (!EquipmentFragment) return;
	if (!bIsProxy)
	{
		EquipmentFragment->OnEquip(OwningPlayerController.Get());
	}

	if (!OwningSkeletalMesh.IsValid()) return;
	AAZ_Inv_EquipActor* SpawnedEquipActor = SpawnEquippedActor(EquipmentFragment, ItemManifest, OwningSkeletalMesh.Get());

	EquippedActors.Add(SpawnedEquipActor);
}

void UAZ_Inv_CommonUI_EquipmentComponent::OnItemUnequipped(UAZ_Inv_CommonUI_InventoryItem* UnequippedItem)
{
	if (!IsValid(UnequippedItem)) return;
	if (!OwningPlayerController->HasAuthority()) return;

	FAZ_Inv_CommonUI_ItemManifest& ItemManifest = UnequippedItem->GetItemManifestMutable();
	FAZ_Inv_CommonUI_EquipmentFragment* EquipmentFragment = ItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>();
	if (!EquipmentFragment) return;

	if (!bIsProxy)
	{
		EquipmentFragment->OnUnequip(OwningPlayerController.Get());
	}

	RemoveEquippedActor(EquipmentFragment->GetEquipmentType());
}

void UAZ_Inv_CommonUI_EquipmentComponent::InitPlayerController()
{
	auto PC = GetOwner();
	auto APC = Cast<APlayerController>(PC);
	if (OwningPlayerController = APC; OwningPlayerController.IsValid())
	{
		if (ACharacter* OwnerCharacter = Cast<ACharacter>(OwningPlayerController->GetPawn()); IsValid(OwnerCharacter))
		{
			OnPossessedPawnChange(nullptr, OwnerCharacter);
		}
		else
		{
			OwningPlayerController->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::OnPossessedPawnChange);
		}
	}
}

void UAZ_Inv_CommonUI_EquipmentComponent::InitInventoryComponent()
{
	InventoryComponent = UAZ_Inv_InventoryStatics::Get_CommonUI_InventoryComponent(OwningPlayerController.Get());
	if (!InventoryComponent.IsValid()) return;

	if (!InventoryComponent->OnItemEquipped.IsAlreadyBound(this, &ThisClass::OnItemEquipped))
	{
		InventoryComponent->OnItemEquipped.AddDynamic(this, &ThisClass::OnItemEquipped);
	}

	if (!InventoryComponent->OnItemUnequipped.IsAlreadyBound(this, &ThisClass::OnItemUnequipped))
	{
		InventoryComponent->OnItemUnequipped.AddDynamic(this, &ThisClass::OnItemUnequipped);
	}
}

AAZ_Inv_EquipActor* UAZ_Inv_CommonUI_EquipmentComponent::SpawnEquippedActor(FAZ_Inv_CommonUI_EquipmentFragment* EquipmentFragment,
	const FAZ_Inv_CommonUI_ItemManifest& Manifest, USkeletalMeshComponent* AttachMesh)
{
	AAZ_Inv_EquipActor* SpawnedEquipActor = EquipmentFragment->SpawnAttachedActor(AttachMesh);
	SpawnedEquipActor->SetEquipmentType(EquipmentFragment->GetEquipmentType());
	SpawnedEquipActor->SetOwner(GetOwner());
	EquipmentFragment->SetEquippedActor(SpawnedEquipActor);
	return SpawnedEquipActor;
}

AAZ_Inv_EquipActor* UAZ_Inv_CommonUI_EquipmentComponent::FindEquippedActor(const FGameplayTag& EquipmentTypeTag)
{
	auto FoundActor = EquippedActors.FindByPredicate([&EquipmentTypeTag](const AAZ_Inv_EquipActor* EquippedActor)
	{
		return EquippedActor->GetEquipmentType().MatchesTagExact(EquipmentTypeTag);
	});
	return FoundActor ? *FoundActor : nullptr;
}

void UAZ_Inv_CommonUI_EquipmentComponent::RemoveEquippedActor(const FGameplayTag& EquipmentTypeTag)
{
	if (AAZ_Inv_EquipActor* EquippedActor = FindEquippedActor(EquipmentTypeTag); IsValid(EquippedActor))
	{
		EquippedActors.Remove(EquippedActor);
		EquippedActor->Destroy();
	}
}

void UAZ_Inv_CommonUI_EquipmentComponent::OnPossessedPawnChange(APawn* OldPawn, APawn* NewPawn)
{
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(NewPawn); IsValid(OwnerCharacter))
	{
		OwningSkeletalMesh = OwnerCharacter->GetMesh();
	}
	InitInventoryComponent();
}

void UAZ_Inv_CommonUI_EquipmentComponent::InitializeOwner(APlayerController* PlayerController)
{
	if (IsValid(PlayerController))
	{
		OwningPlayerController = PlayerController;
	}
	InitInventoryComponent();
}
