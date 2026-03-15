// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InventoryOld/Components/AZ_Inv_InventoryComponent.h"
#include "StructUtils/InstancedStruct.h"
#include "Inventory/Types/AZ_Inv_GridTypes.h"
#include "InventoryOld/Widgets/AZ_Inv_ItemDescription/AZ_Inv_ItemDescription.h"

#include "AZ_Inv_ItemManifest.generated.h"

struct FAZ_Inv_ItemFragment;
class UAZ_Inv_InventoryItem;

template <typename T>
concept DerivedFromItemFragment = std::derived_from<T, FAZ_Inv_ItemFragment>;

USTRUCT(BlueprintType)
struct AZ_API FAZ_Inv_ItemManifest
{
	GENERATED_BODY()

	TArray<TInstancedStruct<FAZ_Inv_ItemFragment>>& GetFragmentsMutable() { return Fragments; }
	UAZ_Inv_InventoryItem* Manifest(UObject* NewOuter);
	EInv_ItemCategory GetItemCategory() const {return ItemCategory;}
	FGameplayTag GetItemTypeTag() const { return ItemTypeTag; }
	void AssimilateInventoryFragments(UAZ_Inv_CompositeBase* Composite) const;

	template <DerivedFromItemFragment T>
	const T* GetFragmentOfTypeByTag(const FGameplayTag& Tag) const;

	template <DerivedFromItemFragment T>
	const T* GetFragmentOfType() const;

	template <DerivedFromItemFragment T>
	T* GetFragmentOfTypeMutable();

	template <typename T> requires std::derived_from<T, FAZ_Inv_ItemFragment>
	TArray<const T*> GetAllFragmentsOfType() const;
	
	void SpawnPickupActor(const UObject* WorldContextObject, const FVector& SpawnLocation, const FRotator& SpawnRotation);

private:

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory", meta = (ExcludeBaseStruct))
	TArray<TInstancedStruct<FAZ_Inv_ItemFragment>> Fragments;
	
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	EInv_ItemCategory ItemCategory{EInv_ItemCategory::None};
	
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FGameplayTag ItemTypeTag;
	
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TSubclassOf<AActor> PickupActorClass;
	
	void ClearFragments();
};

template <DerivedFromItemFragment T>
const T* FAZ_Inv_ItemManifest::GetFragmentOfTypeByTag(const FGameplayTag& Tag) const
{
	for (const auto& Fragment : Fragments)
	{
		const T* FragmentPtr = Fragment.GetPtr<T>();
		if (FragmentPtr)
		{
			if (!FragmentPtr->GetFragmentTag().MatchesTagExact(Tag)) continue;
			return FragmentPtr;
		}
	}

	return nullptr;
}

template <DerivedFromItemFragment T>
const T* FAZ_Inv_ItemManifest::GetFragmentOfType() const
{
	for (const auto& Fragment : Fragments)
	{
		if (auto FragmentPtr = Fragment.GetPtr<T>())
		{
			return FragmentPtr;
		}
	}

	return nullptr;
}

template <DerivedFromItemFragment T>
T* FAZ_Inv_ItemManifest::GetFragmentOfTypeMutable()
{
	for (auto& Fragment : Fragments)
	{
		if (auto FragmentPtr = Fragment.GetMutablePtr<T>())
		{
			return FragmentPtr;
		}
	}

	return nullptr;
}

template <typename T> requires std::derived_from<T, FAZ_Inv_ItemFragment>
TArray<const T*> FAZ_Inv_ItemManifest::GetAllFragmentsOfType() const
{
	TArray<const T*> Result;
	for (const TInstancedStruct<FAZ_Inv_ItemFragment>& Fragment : Fragments)
	{
		if (const T* FragmentPtr = Fragment.GetPtr<T>())
		{
			Result.Add(FragmentPtr);
		}
	}
	return Result;
}

