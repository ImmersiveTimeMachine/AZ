// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/Items/Fragments/AZ_Inv_CommonUI_Item_Fragment.h"
#include "Inventory/Types/AZ_Inv_GridTypes.h"

#include "AZ_Inv_CommonUI_ItemManifest.generated.h"

class UAZ_Inv_CommonUI_InventoryItem;
class UAZ_Inv_CommonUI_CompositeBaseWidget;

template <typename T>
concept DerivedFromCommonUIItemFragment = std::derived_from<T, FAZ_Inv_CommonUI_Item_Fragment>;

USTRUCT(BlueprintType)
struct AZ_API FAZ_Inv_CommonUI_ItemManifest
{
	GENERATED_BODY()
	
	TArray<TInstancedStruct<FAZ_Inv_CommonUI_Item_Fragment>>& GetFragmentsMutable() { return Fragments; }
	UAZ_Inv_CommonUI_InventoryItem* Manifest(UObject* NewOuter);
	EInv_ItemCategory GetItemCategory() const {return ItemCategory;}
	FGameplayTag GetItemTypeTag() const { return ItemTypeTag; }
	void AssimilateInventoryFragments(UAZ_Inv_CommonUI_CompositeBaseWidget* Composite) const;
	void SpawnPickupActor(const UObject* WorldContextObject, const FVector& SpawnLocation, const FRotator& SpawnRotation);

	template <DerivedFromCommonUIItemFragment T>
	const T* GetFragmentOfTypeByTag(const FGameplayTag& Tag) const;

	template <DerivedFromCommonUIItemFragment T>
	const T* GetFragmentOfType() const;

	template <DerivedFromCommonUIItemFragment T>
	T* GetFragmentOfTypeMutable();

	template <typename T> requires std::derived_from<T, FAZ_Inv_CommonUI_Item_Fragment>
	TArray<const T*> GetAllFragmentsOfType() const;

	
private:
	
	void ClearFragments();

	/*
			TInstancedStruct<T> is a type-erased, runtime-polymorphic container for UStruct instances.
			It lets you store different struct types (derived from a base) in one array or property, while still supporting:
			serialization
			replication
			reflection (USTRUCT metadata)
			editor exposure
			Think of it as TArray<std::unique_ptr<BaseStruct>>, but UE-friendly and struct-based.
	 */
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory", meta = (ExcludeBaseStruct))
	TArray<TInstancedStruct<FAZ_Inv_CommonUI_Item_Fragment>> Fragments;
	
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	EInv_ItemCategory ItemCategory{EInv_ItemCategory::None};
	
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FGameplayTag ItemTypeTag;
	
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	TSubclassOf<AActor> PickupActorClass;
};

template <DerivedFromCommonUIItemFragment T>
const T* FAZ_Inv_CommonUI_ItemManifest::GetFragmentOfTypeByTag(const FGameplayTag& Tag) const
{
	for (const auto& Fragment : Fragments)
	{
		if (const T* FragmentPtr = Fragment.GetPtr<T>())
		{
			if (!FragmentPtr->GetFragmentTag().MatchesTagExact(Tag)) continue;
			return FragmentPtr;
		}
	}

	return nullptr;
}

template <DerivedFromCommonUIItemFragment T>
const T* FAZ_Inv_CommonUI_ItemManifest::GetFragmentOfType() const
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

template <DerivedFromCommonUIItemFragment T>
T* FAZ_Inv_CommonUI_ItemManifest::GetFragmentOfTypeMutable()
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

template <typename T> requires std::derived_from<T, FAZ_Inv_CommonUI_Item_Fragment>
TArray<const T*> FAZ_Inv_CommonUI_ItemManifest::GetAllFragmentsOfType() const
{
	TArray<const T*> Result;
	for (const TInstancedStruct<FAZ_Inv_CommonUI_Item_Fragment>& Fragment : Fragments)
	{
		if (const T* FragmentPtr = Fragment.GetPtr<T>())
		{
			Result.Add(FragmentPtr);
		}
	}
	return Result;
}

