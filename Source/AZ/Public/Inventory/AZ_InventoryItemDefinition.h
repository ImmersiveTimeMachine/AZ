// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Item.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "AZ_InventoryItemDefinition.generated.h"

/**
 * 
 */
UCLASS()
class AZ_API UAZ_InventoryItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UAZ_InventoryItemDefinition();

	// -------------------------
	// Asset/Identity
	// -------------------------

	/** Primary asset type used by Asset Manager */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Asset")
	FPrimaryAssetType ItemType;

	/** Overridden to specify the primary asset type. */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// -------------------------
	// Classification & Tags
	// -------------------------

	/** High-level item type tag (e.g., Item.Type.Weapon, Item.Type.Consumable). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Classification")
	FGameplayTag ItemTypeTag;

	/** Additional tags that further classify this item (e.g., Item.Weapon.Rifle). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Classification")
	FGameplayTagContainer EquipmentTags;

	/** A unique tag granted to the owner while this item is equipped (useful for gating abilities/UI). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Classification")
	FGameplayTag EquipStateTag;

	// -------------------------
	// Equipment & Placement
	// -------------------------

	/** Which equipment slot this item can occupy (e.g., Equipment.Slot.Weapon, Equipment.Slot.Armor.Head). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Equipment")
	FGameplayTag AllowedSlotTag;

	/** If true, show this item as an attachment (e.g., holstered) when not active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Equipment")
	bool bSetActorHiddenInGame = false;

	/** If true, attach the spawned item actor to the owning character/actor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Equipment")
	bool bAttachToOwner = true;

	// -------------------------
	// Display
	// -------------------------

	/** Visual display name for UI. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Display")
	FText ItemName;

	/** UI icon for the item. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Display")
	TSoftObjectPtr<UTexture2D> ItemIcon;

	// -------------------------
	// Inventory layout
	// -------------------------

	/** Inventory grid footprint (Width = X, Height = Y). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Inventory")
	FIntPoint Dimensions = FIntPoint(1, 1);

	// -------------------------
	// Spawning
	// -------------------------

	/** Actor class to spawn for this item (pickup/world representation). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Spawning")
	TSubclassOf<AAZ_Item> ItemClass;

	// -------------------------
	// Gameplay Abilities & Effects
	// -------------------------

	/** Gameplay abilities to grant while this item is equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> AbilitiesToGrant;

	/** Ongoing gameplay effects while equipped (e.g., passive buffs/debuffs). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|Effects")
	TArray<TSubclassOf<UGameplayEffect>> OngoingEffectsToApply;
};