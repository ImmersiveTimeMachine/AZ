// Fill out your copyright notice in the Description page of Project Settings.


#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "Equipment/EquipActor/AZ_Inv_EquipActor.h"
#include "InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h"
#include "InventoryUI/Utils/AZ_Inv_InventoryStatics.h"


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

// --- Pickup: spawn actor at carry socket (back/holster) ---
void UAZ_Inv_CommonUI_EquipmentComponent::OnItemAdded(UAZ_Inv_CommonUI_InventoryItem* AddedItem)
{
	if (!IsValid(AddedItem)) return;
	if (!OwningPlayerController->HasAuthority()) return;

	FAZ_Inv_CommonUI_ItemManifest& ItemManifest = AddedItem->GetItemManifestMutable();
	FAZ_Inv_CommonUI_EquipmentFragment* EquipmentFragment = ItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>();
	if (!EquipmentFragment) return;

	if (!bIsProxy)
	{
		EquipmentFragment->OnPickup(OwningPlayerController.Get());
	}

	if (OwningSkeletalMesh.IsValid())
	{
		AAZ_Inv_EquipActor* CarryActor = SpawnCarryActor(EquipmentFragment, OwningSkeletalMesh.Get());
		if (CarryActor)
		{
			ManagedActors.Add(CarryActor);
		}
	}
}

// --- Equip: move actor to hand socket, grant abilities/modifiers, set weapon tag ---
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

		if (FAZ_Inv_CommonUI_AbilityGrantFragment* AbilityFragment = ItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_AbilityGrantFragment>())
		{
			AbilityFragment->OnEquip(OwningPlayerController.Get(), EquipmentFragment->GetEquippedActor());
		}

		// Set weapon tag on ASC for anim state
		if (APawn* Pawn = OwningPlayerController->GetPawn())
		{
			if (UAZ_AbilitySystemComponent* ASC = Cast<UAZ_AbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn)))
			{
				ASC->OnWeaponEquipped(EquipmentFragment->GetEquipmentType());
			}
		}
	}
}

// --- Unequip: move actor back to carry socket, remove abilities/modifiers, clear weapon tag ---
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

		if (FAZ_Inv_CommonUI_AbilityGrantFragment* AbilityFragment = ItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_AbilityGrantFragment>())
		{
			AbilityFragment->OnUnequip(OwningPlayerController.Get());
		}

		// Clear weapon tag — revert to unarmed
		if (APawn* Pawn = OwningPlayerController->GetPawn())
		{
			if (UAZ_AbilitySystemComponent* ASC = Cast<UAZ_AbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn)))
			{
				ASC->OnWeaponEquipped(FGameplayTag::EmptyTag);
			}
		}
	}
}

// --- Drop: destroy actor, remove from managed list ---
void UAZ_Inv_CommonUI_EquipmentComponent::OnItemDropped(UAZ_Inv_CommonUI_InventoryItem* DroppedItem)
{
	if (!IsValid(DroppedItem)) return;
	if (!OwningPlayerController->HasAuthority()) return;

	FAZ_Inv_CommonUI_ItemManifest& ItemManifest = DroppedItem->GetItemManifestMutable();
	FAZ_Inv_CommonUI_EquipmentFragment* EquipmentFragment = ItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>();
	if (!EquipmentFragment) return;

	if (!bIsProxy)
	{
		// If still equipped, remove abilities/modifiers first
		if (EquipmentFragment->GetState() == EEquipmentState::Equipped)
		{
			EquipmentFragment->OnUnequip(OwningPlayerController.Get());

			if (FAZ_Inv_CommonUI_AbilityGrantFragment* AbilityFragment = ItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_AbilityGrantFragment>())
			{
				AbilityFragment->OnUnequip(OwningPlayerController.Get());
			}
		}
	}

	RemoveManagedActor(EquipmentFragment->GetEquipmentType());
	EquipmentFragment->OnDrop();
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

	if (!InventoryComponent->OnItemAdded.IsAlreadyBound(this, &ThisClass::OnItemAdded))
	{
		InventoryComponent->OnItemAdded.AddDynamic(this, &ThisClass::OnItemAdded);
	}

	if (!InventoryComponent->OnItemEquipped.IsAlreadyBound(this, &ThisClass::OnItemEquipped))
	{
		InventoryComponent->OnItemEquipped.AddDynamic(this, &ThisClass::OnItemEquipped);
	}

	if (!InventoryComponent->OnItemUnequipped.IsAlreadyBound(this, &ThisClass::OnItemUnequipped))
	{
		InventoryComponent->OnItemUnequipped.AddDynamic(this, &ThisClass::OnItemUnequipped);
	}

	if (!InventoryComponent->OnItemDropped.IsAlreadyBound(this, &ThisClass::OnItemDropped))
	{
		InventoryComponent->OnItemDropped.AddDynamic(this, &ThisClass::OnItemDropped);
	}
}

AAZ_Inv_EquipActor* UAZ_Inv_CommonUI_EquipmentComponent::SpawnCarryActor(FAZ_Inv_CommonUI_EquipmentFragment* EquipmentFragment,
	USkeletalMeshComponent* AttachMesh)
{
	AAZ_Inv_EquipActor* SpawnedActor = EquipmentFragment->SpawnAttachedActor(AttachMesh, EquipmentFragment->GetCarrySocket());
	if (!SpawnedActor) return nullptr;

	SpawnedActor->SetEquipmentType(EquipmentFragment->GetEquipmentType());
	SpawnedActor->SetOwner(GetOwner());
	EquipmentFragment->SetEquippedActor(SpawnedActor);
	return SpawnedActor;
}

AAZ_Inv_EquipActor* UAZ_Inv_CommonUI_EquipmentComponent::FindManagedActor(const FGameplayTag& EquipmentTypeTag)
{
	auto FoundActor = ManagedActors.FindByPredicate([&EquipmentTypeTag](const AAZ_Inv_EquipActor* Actor)
	{
		return Actor->GetEquipmentType().MatchesTagExact(EquipmentTypeTag);
	});
	return FoundActor ? *FoundActor : nullptr;
}

void UAZ_Inv_CommonUI_EquipmentComponent::RemoveManagedActor(const FGameplayTag& EquipmentTypeTag)
{
	if (AAZ_Inv_EquipActor* Actor = FindManagedActor(EquipmentTypeTag); IsValid(Actor))
	{
		ManagedActors.Remove(Actor);
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
