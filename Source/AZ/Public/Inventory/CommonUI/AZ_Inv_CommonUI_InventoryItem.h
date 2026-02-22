// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/Items/Manifest/AZ_Inv_CommonUI_ItemManifest.h"
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
	
	void SetItemManifest(const FAZ_Inv_CommonUI_ItemManifest& Manifest);
	FAZ_Inv_CommonUI_ItemManifest GetItemManifest() const;
	FAZ_Inv_CommonUI_ItemManifest& GetItemManifestMutable();
	
	int32 GetTotalStackCount() const { return TotalStackCount; }
	void SetTotalStackCount(const int32 Count) { TotalStackCount = Count; }
	
private:
	UPROPERTY(VisibleAnywhere, meta = (BaseStruct = "/Script/AZ.AZ_Inv_ItemManifest"), Replicated)
	FInstancedStruct ItemManifest;

	UPROPERTY(Replicated)
	int32 TotalStackCount{0};
};
