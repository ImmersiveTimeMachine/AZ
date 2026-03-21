// Fill out your copyright notice in the Description page of Project Settings.


#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "AbilitySystemGlobals.h"
#include "AZ_GameplayTags.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h"
#include "InventoryUI/Utils/AZ_Inv_InventoryStatics.h"
#include "Weapon/AZ_Weapon.h"


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

// --- Pickup: spawn prop actor at carry socket ---
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
		FAZ_Inv_CommonUI_WeaponStateFragment* WeaponState = ItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_WeaponStateFragment>();

		if (WeaponState && WeaponState->IsWeaponItem())
		{
			// Spawn weapon at carry socket in cosmetic mode
			AActor* WeaponActor = SpawnWeaponActor(WeaponState->WeaponActorClass, OwningSkeletalMesh.Get());
			if (AAZ_Weapon* Weapon = Cast<AAZ_Weapon>(WeaponActor))
			{
				Weapon->MakeCosmetic();
				Weapon->AttachToComponent(OwningSkeletalMesh.Get(),
					FAttachmentTransformRules::SnapToTargetNotIncludingScale,
					Weapon->CarrySocketName);
				EquipmentFragment->SetEquippedActor(Weapon);
			}
		}
	}
}

// --- Equip: weapon swap or reattach, grant abilities, set weapon tag ---
void UAZ_Inv_CommonUI_EquipmentComponent::OnItemEquipped(UAZ_Inv_CommonUI_InventoryItem* EquippedItem)
{
	if (!IsValid(EquippedItem)) return;
	if (!OwningPlayerController->HasAuthority()) return;

	FAZ_Inv_CommonUI_ItemManifest& ItemManifest = EquippedItem->GetItemManifestMutable();
	FAZ_Inv_CommonUI_EquipmentFragment* EquipmentFragment = ItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>();
	if (!EquipmentFragment) return;

	if (!bIsProxy)
	{
		FAZ_Inv_CommonUI_WeaponStateFragment* WeaponState = ItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_WeaponStateFragment>();

		// State + modifiers
		EquipmentFragment->OnEquip(OwningPlayerController.Get());

		// World operations: reattach weapon to relaxed socket + push ASC state
		if (AAZ_Weapon* Weapon = Cast<AAZ_Weapon>(EquipmentFragment->GetEquippedActor()))
		{
			ActiveWeapon = Weapon;
			Weapon->Tags.AddUnique(FAZ_GameplayTags::Get().Weapon_Slot_Primary.GetTagName());
			EquipmentFragment->ReattachActor(Weapon->RelaxedSocketName);

			// Tag ASC with primary weapon equipped
			if (APawn* Pawn = OwningPlayerController->GetPawn())
			{
				if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
				{
					ASC->AddLooseGameplayTag(FAZ_GameplayTags::Get().State_Equipped_Weapon_Primary);
				}
			}

			if (WeaponState)
			{
				if (APawn* Pawn = OwningPlayerController->GetPawn())
				{
					if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
					{
						WeaponState->ApplyToASC(ASC);
					}
				}
			}
		}

		// Abilities — source object is the equipped actor (weapon or prop)
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

// --- Unequip: weapon swap back or reattach, remove abilities, clear weapon tag ---
void UAZ_Inv_CommonUI_EquipmentComponent::OnItemUnequipped(UAZ_Inv_CommonUI_InventoryItem* UnequippedItem)
{
	if (!IsValid(UnequippedItem)) return;
	if (!OwningPlayerController->HasAuthority()) return;

	FAZ_Inv_CommonUI_ItemManifest& ItemManifest = UnequippedItem->GetItemManifestMutable();
	FAZ_Inv_CommonUI_EquipmentFragment* EquipmentFragment = ItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>();
	if (!EquipmentFragment) return;

	if (!bIsProxy)
	{
		FAZ_Inv_CommonUI_WeaponStateFragment* WeaponState = ItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_WeaponStateFragment>();

		// World operations: save ASC state + reattach weapon to carry socket
		if (AAZ_Weapon* Weapon = Cast<AAZ_Weapon>(EquipmentFragment->GetEquippedActor()))
		{
			if (WeaponState)
			{
				SaveWeaponStateToFragment(WeaponState);
			}
			EquipmentFragment->ReattachActor(Weapon->CarrySocketName);
		}

		if (ActiveWeapon.IsValid())
		{
			ActiveWeapon->Tags.Remove(FAZ_GameplayTags::Get().Weapon_Slot_Primary.GetTagName());
		}
		ActiveWeapon = nullptr;

		// Remove primary weapon tag
		if (APawn* Pawn = OwningPlayerController->GetPawn())
		{
			if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
			{
				ASC->RemoveLooseGameplayTag(FAZ_GameplayTags::Get().State_Equipped_Weapon_Primary);
			}
		}

		// State + modifiers
		EquipmentFragment->OnUnequip(OwningPlayerController.Get());

		if (FAZ_Inv_CommonUI_AbilityGrantFragment* AbilityFragment = ItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_AbilityGrantFragment>())
		{
			AbilityFragment->OnUnequip(OwningPlayerController.Get());
		}

		// Clear weapon tag
		if (APawn* Pawn = OwningPlayerController->GetPawn())
		{
			if (UAZ_AbilitySystemComponent* ASC = Cast<UAZ_AbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn)))
			{
				ASC->OnWeaponEquipped(FGameplayTag::EmptyTag);
			}
		}
	}
}

// --- Drop: save state if equipped, destroy, remove from managed list ---
void UAZ_Inv_CommonUI_EquipmentComponent::OnItemDropped(UAZ_Inv_CommonUI_InventoryItem* DroppedItem)
{
	if (!IsValid(DroppedItem)) return;
	if (!OwningPlayerController->HasAuthority()) return;

	FAZ_Inv_CommonUI_ItemManifest& ItemManifest = DroppedItem->GetItemManifestMutable();
	FAZ_Inv_CommonUI_EquipmentFragment* EquipmentFragment = ItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>();
	if (!EquipmentFragment) return;

	if (!bIsProxy)
	{
		if (EquipmentFragment->GetState() == EEquipmentState::Equipped)
		{
			FAZ_Inv_CommonUI_WeaponStateFragment* WeaponState = ItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_WeaponStateFragment>();

			// Save weapon ASC state before dropping
			if (WeaponState && WeaponState->IsWeaponItem())
			{
				SaveWeaponStateToFragment(WeaponState);
			}

			EquipmentFragment->OnUnequip(OwningPlayerController.Get());

			if (FAZ_Inv_CommonUI_AbilityGrantFragment* AbilityFragment = ItemManifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_AbilityGrantFragment>())
			{
				AbilityFragment->OnUnequip(OwningPlayerController.Get());
			}

			if (ActiveWeapon.IsValid())
			{
				ActiveWeapon->Tags.Remove(FAZ_GameplayTags::Get().Weapon_Slot_Primary.GetTagName());
			}
			ActiveWeapon = nullptr;

			// Clear weapon tags — revert to unarmed
			if (APawn* Pawn = OwningPlayerController->GetPawn())
			{
				if (UAZ_AbilitySystemComponent* ASC = Cast<UAZ_AbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn)))
				{
					ASC->OnWeaponEquipped(FGameplayTag::EmptyTag);
					ASC->RemoveLooseGameplayTag(FAZ_GameplayTags::Get().State_Equipped_Weapon_Primary);
				}
			}
		}
	}

	EquipmentFragment->OnDrop();
}

void UAZ_Inv_CommonUI_EquipmentComponent::SaveWeaponStateToFragment(FAZ_Inv_CommonUI_WeaponStateFragment* WeaponState)
{
	if (!WeaponState || !OwningPlayerController.IsValid()) return;

	if (APawn* Pawn = OwningPlayerController->GetPawn())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
		{
			WeaponState->SaveFromASC(ASC);
		}
	}
}

AActor* UAZ_Inv_CommonUI_EquipmentComponent::SpawnWeaponActor(TSubclassOf<AActor> WeaponClass, USkeletalMeshComponent* AttachMesh)
{
	if (!WeaponClass || !IsValid(AttachMesh)) return nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = Cast<APawn>(GetOwner());
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(WeaponClass, FTransform::Identity, SpawnParams);
	if (!SpawnedActor) return nullptr;

	if (AAZ_Weapon* Weapon = Cast<AAZ_Weapon>(SpawnedActor))
	{
		Weapon->bSpawnWithCollision = false;
	}

	return SpawnedActor;
}

// --- Helpers ---

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
