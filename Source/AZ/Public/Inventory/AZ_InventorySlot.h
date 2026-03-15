// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_AssetManager.h"
#include "Items/AZ_Item.h"
#include "AZ_InventorySlot.generated.h"

USTRUCT(BlueprintType)
struct FInventorySlot
{
	GENERATED_BODY()

	// The class of item in this slot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AAZ_Item> ItemClass;

	// How many of this item are in the slot (for stackable items like ammo/potions).
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Quantity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag InventorySlotTag;

	FInventorySlot() : ItemClass(nullptr), Quantity(0) {}
};

UCLASS()
class AZ_API AAZ_InventorySlot : public AActor
{
	GENERATED_BODY()
		
	
};
