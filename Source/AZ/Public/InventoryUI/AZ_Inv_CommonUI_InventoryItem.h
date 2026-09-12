// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemState.h"
#include "InventoryUI/Items/Manifest/AZ_Inv_CommonUI_ItemManifest.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Object.h"
#include "AZ_Inv_CommonUI_InventoryItem.generated.h"

/**
 * 
 */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_InventoryItem : public UObject
{
	GENERATED_BODY()
	
public:
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsSupportedForNetworking() const override { return true; }

	bool IsStackable() const;
	bool IsConsumable() const;
	bool IsMagazine() const;
	bool IsInitialized() const { return ItemManifest.IsValid() && InstanceState.InstanceId.IsValid(); }
	bool IsWeapon() const;
	FGameplayTag GetWeaponProfileTag() const;
	FGuid GetInstanceId() const { return InstanceState.InstanceId; }
	FGuid GetItemInstanceId() const { return GetInstanceId(); }
	EAZ_InventoryItemLocation GetLocation() const { return InstanceState.Location; }
	FGuid GetParentItemId() const { return InstanceState.ParentItemId; }
	FGuid GetInsertedMagazineId() const { return InstanceState.InsertedMagazineId; }
	int32 GetMagazineRounds() const { return InstanceState.CurrentRounds; }
	int32 GetMagazineCapacity() const;
	EAZ_FirearmFireMode GetSelectedFireMode() const { return InstanceState.SelectedFireMode; }
	int64 GetFireModeRevision() const { return InstanceState.FireModeRevision; }
	const FAZ_InventoryItemState& GetInstanceState() const { return InstanceState; }
	void InitializeInstance(const FAZ_InventoryItemState& State, int32 StackCount);

	/** Actual FULL cone at the supplied synchronized server time; reading never grows or resets recoil. */
	float GetFirearmSpreadAngleDegrees(double ServerTime) const;

	/** Additional HALF-angle above baseline, analytically recovered from the last accepted shot. */
	float GetFirearmExtraSpreadRadiusDegrees(double ServerTime) const;
	
	        void SetItemManifest(const FAZ_Inv_CommonUI_ItemManifest& Manifest);
	        const FAZ_Inv_CommonUI_ItemManifest& GetItemManifest() const;
	        FAZ_Inv_CommonUI_ItemManifest& GetItemManifestMutable();	
	int32 GetTotalStackCount() const { return TotalStackCount; }
	void SetTotalStackCount(const int32 Count) { TotalStackCount = Count; }
	
private:
	friend class UAZ_Inv_CommonUI_InventoryComponent;
	/** Only the accepted ammo transaction may append spread, before it publishes inventory changes. */
	void RecordAcceptedFirearmShot(double ServerTime, const FAZ_FirearmRecoilSettings& Settings);

	/** Deliberately absent from pickup/save payloads. A newly initialized item starts recovered. */
	UPROPERTY(Transient, ReplicatedUsing=OnRep_ItemChanged)
	FAZ_FirearmSpreadState FirearmSpread;

	UPROPERTY(ReplicatedUsing=OnRep_ItemChanged)
	FAZ_InventoryItemState InstanceState;

	UFUNCTION()
	void OnRep_ItemChanged();

	UPROPERTY(VisibleAnywhere, meta = (BaseStruct = "/Script/AZ.AZ_Inv_CommonUI_ItemManifest"), ReplicatedUsing=OnRep_ItemChanged)
	FInstancedStruct ItemManifest;

	UPROPERTY(ReplicatedUsing=OnRep_ItemChanged)
	int32 TotalStackCount{1};
};

template<typename FragmentType>
requires std::derived_from<FragmentType, FAZ_Inv_CommonUI_ItemFragment>
const FragmentType* GetFragment(const UAZ_Inv_CommonUI_InventoryItem* Item, const FGameplayTag& Tag)
{
	if (!IsValid(Item)) return nullptr;

	const FAZ_Inv_CommonUI_ItemManifest& Manifest = Item->GetItemManifest();
	return Manifest.GetFragmentOfTypeByTag<FragmentType>(Tag);
}
