// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryOld/FastArray/AZ_Inv_FastArray.h"
#include "AZ_Inv_InventoryComponent.generated.h"


class UAZ_Inv_ItemComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryItemChanged, UAZ_Inv_InventoryItem*, InventoryItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNoRoomInInventory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStackChange, const FAZ_Inv_SlotAvailabilityResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FItemEquipStatusChanged, UAZ_Inv_InventoryItem*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryMenuToggled, bool, bOpen);

class UAZ_Inv_InventoryMenuBase;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class AZ_API UAZ_Inv_InventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	
	UAZ_Inv_InventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Inventory")
	void TryAddItem(UAZ_Inv_ItemComponent* ItemComponent);

	UFUNCTION(Server, Reliable)
	void Server_AddNewItem(UAZ_Inv_ItemComponent* ItemComponent, int32 StackCount, int32 Remainder);

	UFUNCTION(Server, Reliable)
	void Server_AddStacksToItem(UAZ_Inv_ItemComponent* ItemComponent, int32 StackCount, int32 RemainingRooms);
	
	UFUNCTION(Server, Reliable)
	void Server_DropItem(UAZ_Inv_InventoryItem* Item, int32 StackCount);
	
	UFUNCTION(Server, Reliable)
	void Server_ConsumeItem(UAZ_Inv_InventoryItem* Item);
	
	UFUNCTION(Server, Reliable)
	void Server_EquipSlotClicked(UAZ_Inv_InventoryItem* ItemToEquip, UAZ_Inv_InventoryItem* ItemToUnequip);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_EquipSlotClicked(UAZ_Inv_InventoryItem* ItemToEquip, UAZ_Inv_InventoryItem* ItemToUnequip);


	void SpawnDroppedItem(UAZ_Inv_InventoryItem* Item, int32 StackCount);
	void ToggleInventoryMenu();
	void AddRepSubObjects(UObject* SubObjects);
	
	UAZ_Inv_InventoryMenuBase* GetInventoryMenu() const { return InventoryMenu; }

	UPROPERTY(BlueprintAssignable, Category = "AZ|Inventory")
	FInventoryItemChanged OnItemAdded;

	UPROPERTY(BlueprintAssignable, Category = "AZ|Inventory")
	FInventoryItemChanged OnItemRemoved;

	UPROPERTY(BlueprintAssignable, Category = "AZ|Inventory")
	FNoRoomInInventory OnNoRoomInInventory;

	UPROPERTY(BlueprintAssignable, Category = "AZ|Inventory")
	FStackChange OnStackChange;
	
	FItemEquipStatusChanged OnItemEquipped;
	FItemEquipStatusChanged OnItemUnequipped;
	FInventoryMenuToggled OnInventoryMenuToggled;

protected:
	
	virtual void BeginPlay() override;

private:

	void ConstructInventory();

	UPROPERTY(Replicated)
	FAZ_Inv_InventoryFastArray InventoryList;

	TWeakObjectPtr<APlayerController> OwningController;

	UPROPERTY()
	TObjectPtr<UAZ_Inv_InventoryMenuBase> InventoryMenu;

	UPROPERTY(EditAnywhere, Category="Inventory")
	TSubclassOf<UAZ_Inv_InventoryMenuBase> InventoryScreenClass;

	bool bInventoryMenuOpen;
	void OpenInventoryMenu();
	void CloseInventoryMenu();
	
	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float DropSpawnAngleMin = -85.f;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float DropSpawnAngleMax = 85.f;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float DropSpawnDistanceMin = 10.f;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float DropSpawnDistanceMax = 50.f;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float RelativeSpawnElevation = 70.f;
};
