// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemState.h"
#include "InventoryUI/Items/Manifest/AZ_Inv_CommonUI_ItemManifest.h"
#include "AZ_Inv_CommonUI_ItemComponent.generated.h"

/** Complete item record used only while ownership resides in a world pickup. */
USTRUCT(BlueprintType)
struct AZ_API FAZ_InventoryPickupRecord
{
	GENERATED_BODY()
	UPROPERTY()
	FAZ_Inv_CommonUI_ItemManifest Manifest;
	UPROPERTY()
	FAZ_InventoryItemState State;
	UPROPERTY()
	int32 StackCount = 1;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable, editinlinenew)
class AZ_API UAZ_Inv_CommonUI_ItemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UAZ_Inv_CommonUI_ItemComponent();
	
	float GetPickupRadius() const { return PickupRadius; }
	FString GetPickupMessage() const { return PickupMessage; }
	FAZ_Inv_CommonUI_ItemManifest GetItemManifest() const { return PickupItemManifest; }
	FAZ_Inv_CommonUI_ItemManifest& GetItemManifestMutable() { return PickupItemManifest; }

	UFUNCTION()
	void DestroyItem() const;
	
	void PickedUp();
	void InitItemManifest(FAZ_Inv_CommonUI_ItemManifest CopyOfManifest);
	bool InitializePickupPayload();
	bool HasBeenPickedUp() const { return bPickupCommitted; }
	FAZ_InventoryPickupRecord GetRootRecord() const;
	const TArray<FAZ_InventoryPickupRecord>& GetContainedItems() const { return ContainedItems; }
	void SetPickupPayload(const FAZ_InventoryPickupRecord& RootRecord, const TArray<FAZ_InventoryPickupRecord>& Children);
	void SetRemainingStackCount(int32 Count);
	/** Authorable initial contents, materialized once with fresh identities on spawn. */
	void SetInitialContainedItemManifests(const TArray<FAZ_Inv_CommonUI_ItemManifest>& Manifests) { InitialContainedItemManifests = Manifests; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "AZ|Inventory")
	void OnPickedUp();
	
	UPROPERTY(Replicated, EditAnywhere, Category = "AZ|Inventory")
	FAZ_Inv_CommonUI_ItemManifest PickupItemManifest;
	UPROPERTY(EditAnywhere, Category="AZ|Inventory")
	TArray<FAZ_Inv_CommonUI_ItemManifest> InitialContainedItemManifests;
	UPROPERTY(Transient, Replicated)
	FAZ_InventoryItemState PickupState;
	UPROPERTY(Transient, Replicated)
	TArray<FAZ_InventoryPickupRecord> ContainedItems;
	UPROPERTY(Transient, Replicated)
	int32 PickupStackCount = 1;
	bool bPickupCommitted = false;
	
	// Radius for overlap volume (editable)
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory", meta=(ClampMin="10.0"))
	float PickupRadius = 100.f;

	/*UPROPERTY(Replicated, EditAnywhere, Category = "AZ|Inventory")
	FAZ_Inv_ItemManifest ItemManifest;*/
	
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	FString PickupMessage;
};
