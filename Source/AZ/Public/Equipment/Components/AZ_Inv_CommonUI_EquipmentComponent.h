// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AZ_Inv_CommonUI_EquipmentComponent.generated.h"


struct FGameplayTag;
class UAZ_Inv_CommonUI_InventoryItem;
struct FAZ_Inv_CommonUI_EquipmentFragment;
struct FAZ_Inv_CommonUI_WeaponStateFragment;
struct FAZ_Inv_CommonUI_AbilityGrantFragment;
class UAZ_Inv_CommonUI_InventoryComponent;

UCLASS(ClassGroup=(Custom), Blueprintable, meta=(BlueprintSpawnableComponent))
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
	void OnItemAdded(UAZ_Inv_CommonUI_InventoryItem* AddedItem);

	UFUNCTION()
	void OnItemEquipped(UAZ_Inv_CommonUI_InventoryItem* EquippedItem);

	UFUNCTION()
	void OnItemUnequipped(UAZ_Inv_CommonUI_InventoryItem* UnequippedItem);

	UFUNCTION()
	void OnItemDropped(UAZ_Inv_CommonUI_InventoryItem* DroppedItem);

	void InitPlayerController();
	void InitInventoryComponent();

	AActor* SpawnWeaponActor(TSubclassOf<AActor> WeaponClass, USkeletalMeshComponent* AttachMesh);
	void SaveWeaponStateToFragment(FAZ_Inv_CommonUI_WeaponStateFragment* WeaponState);

	UFUNCTION()
	void OnPossessedPawnChange(APawn* OldPawn, APawn* NewPawn);

	bool bIsProxy{false};
};
