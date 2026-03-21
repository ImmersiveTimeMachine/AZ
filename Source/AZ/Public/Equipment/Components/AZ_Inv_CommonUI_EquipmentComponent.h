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
class AAZ_Weapon;

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

	UFUNCTION(BlueprintPure, Category = "AZ|Equipment")
	AAZ_Weapon* GetActiveWeapon() const { return ActiveWeapon.Get(); }

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

	TWeakObjectPtr<AAZ_Weapon> ActiveWeapon;

	bool bIsProxy{false};
};
