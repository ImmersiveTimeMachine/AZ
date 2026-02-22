// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/Items/Manifest/AZ_Inv_ItemManifest.h"
#include "StructUtils/InstancedStruct.h"
#include "AZ_Inv_InventoryItem.generated.h"

/**
 * 
 */
UCLASS()
class AZ_API UAZ_Inv_InventoryItem : public UObject
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsSupportedForNetworking() const override { return true; }

	bool IsStackable() const;
	bool IsConsumable() const;

	void SetItemManifest(const FAZ_Inv_ItemManifest& Manifest);

	const FAZ_Inv_ItemManifest& GetItemManifest() const
	{
		// Add a check to make invalid state explicit
		if (!ensureMsgf(ItemManifest.IsValid(), TEXT("ItemManifestData is invalid, returning default manifest")))
		{
			static const FAZ_Inv_ItemManifest DefaultManifest;
			return DefaultManifest;
		}
		return ItemManifest.Get<FAZ_Inv_ItemManifest>();
	}

	FAZ_Inv_ItemManifest& GetItemManifestMutable()
	{
		ensure(ItemManifest.IsValid());
		return ItemManifest.GetMutable<FAZ_Inv_ItemManifest>();
	}
	
	int32 GetTotalStackCount() const { return TotalStackCount; }
	void SetTotalStackCount(int32 Count) { TotalStackCount = Count; }
	
private:
	UPROPERTY(VisibleAnywhere, meta = (BaseStruct = "/Script/AZ.AZ_Inv_ItemManifest"), Replicated)
	FInstancedStruct ItemManifest;

	UPROPERTY(Replicated)
	int32 TotalStackCount{0};
};

template<typename FragmentType>
requires std::derived_from<FragmentType, FAZ_Inv_ItemFragment>
const FragmentType* GetFragment(const UAZ_Inv_InventoryItem* Item, const FGameplayTag& Tag)
{
	if (!IsValid(Item)) return nullptr;

	const FAZ_Inv_ItemManifest& Manifest = Item->GetItemManifest();
	return Manifest.GetFragmentOfTypeByTag<FragmentType>(Tag);
}