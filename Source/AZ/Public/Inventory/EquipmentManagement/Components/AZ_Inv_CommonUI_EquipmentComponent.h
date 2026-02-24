// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AZ_Inv_CommonUI_EquipmentComponent.generated.h"


struct FGameplayTag;
class UAZ_Inv_CommonUI_InventoryItem;
struct FAZ_Inv_CommonUI_ItemManifest;
class AAZ_Inv_EquipActor;
struct FAZ_Inv_CommonUI_EquipmentFragment;
class UAZ_Inv_CommonUI_InventoryComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AZ_API UAZ_Inv_CommonUI_EquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAZ_Inv_CommonUI_EquipmentComponent();

	void SetOwningSkeletalMesh(USkeletalMeshComponent* OwningMesh);

	void SetIsProxy(bool bProxy)
	{
		bIsProxy = bProxy;
	}

	void InitializeOwner(APlayerController* PlayerController);

protected:
	virtual void BeginPlay() override;

private:

	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryComponent> InventoryComponent;
	TWeakObjectPtr<APlayerController> OwningPlayerController;
	TWeakObjectPtr<USkeletalMeshComponent> OwningSkeletalMesh;

	UFUNCTION()
	void OnItemEquipped(UAZ_Inv_CommonUI_InventoryItem* EquippedItem);

	UFUNCTION()
	void OnItemUnequipped(UAZ_Inv_CommonUI_InventoryItem* UnequippedItem);

	void InitPlayerController();
	void InitInventoryComponent();
	AAZ_Inv_EquipActor* SpawnEquippedActor(FAZ_Inv_CommonUI_EquipmentFragment* EquipmentFragment, const FAZ_Inv_CommonUI_ItemManifest& Manifest, USkeletalMeshComponent* AttachMesh);

	UPROPERTY()
	TArray<TObjectPtr<AAZ_Inv_EquipActor>> EquippedActors;

	AAZ_Inv_EquipActor* FindEquippedActor(const FGameplayTag& EquipmentTypeTag);
	void RemoveEquippedActor(const FGameplayTag& EquipmentTypeTag);

	UFUNCTION()
	void OnPossessedPawnChange(APawn* OldPawn, APawn* NewPawn);

	bool bIsProxy{false};
};
