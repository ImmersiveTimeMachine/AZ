// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_CommonUI_GameInventoryMenu.h"
#include "Components/ActorComponent.h"
#include "InventoryUI/FastArray/AZ_Inv_CommonUI_FastArray.h"
#include "AZ_Inv_CommonUI_InventoryComponent.generated.h"


class UAZ_Inv_CommonUI_InventoryItem;
class UAZ_Inv_CommonUI_ItemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonUI_InventoryItemChanged, UAZ_Inv_CommonUI_InventoryItem*, InventoryItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCommonUI_NoRoomInInventory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonUI_StackChange, const FAZ_Inv_CommonUI_SlotAvailabilityResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonUI_ItemEquipStatusChanged, UAZ_Inv_CommonUI_InventoryItem*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonUI_InventoryMenuToggled, bool, bOpen);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class AZ_API UAZ_Inv_CommonUI_InventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UAZ_Inv_CommonUI_InventoryComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void ToggleInventoryMenu();
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="AZ|Inventory")
	void TryAddItem(UAZ_Inv_CommonUI_ItemComponent* ItemComponent);
	
	UFUNCTION(Server, Reliable)
	void Server_AddNewItem(UAZ_Inv_CommonUI_ItemComponent* ItemComponent, int32 StackCount, int32 Remainder);

	UFUNCTION(Server, Reliable)
	void Server_AddStacksToItem(UAZ_Inv_CommonUI_ItemComponent* ItemComponent, int32 StackCount, int32 RemainingRooms);
	
	UFUNCTION(Server, Reliable)
	void Server_DropItem(UAZ_Inv_CommonUI_InventoryItem* Item, int32 StackCount);
	
	UFUNCTION(Server, Reliable)
	void Server_ConsumeItem(UAZ_Inv_CommonUI_InventoryItem* Item);
	
	UFUNCTION(Server, Reliable)
	void Server_EquipSlotClicked(UAZ_Inv_CommonUI_InventoryItem* ItemToEquip, UAZ_Inv_CommonUI_InventoryItem* ItemToUnequip);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_EquipSlotClicked(UAZ_Inv_CommonUI_InventoryItem* ItemToEquip, UAZ_Inv_CommonUI_InventoryItem* ItemToUnequip);
	
	void AddRepSubObjects(UObject* SubObjects);
	
	UPROPERTY(BlueprintAssignable, Category = "AZ|Inventory")
	FCommonUI_InventoryItemChanged OnItemAdded;

	UPROPERTY(BlueprintAssignable, Category = "AZ|Inventory")
	FCommonUI_InventoryItemChanged OnItemRemoved;

	UPROPERTY(BlueprintAssignable, Category = "AZ|Inventory")
	FCommonUI_NoRoomInInventory OnNoRoomInInventory;

	UPROPERTY(BlueprintAssignable, Category = "AZ|Inventory")
	FCommonUI_StackChange OnStackChange;

	FCommonUI_ItemEquipStatusChanged OnItemEquipped;
	FCommonUI_ItemEquipStatusChanged OnItemUnequipped;
	FCommonUI_InventoryItemChanged OnItemDropped;
	FCommonUI_InventoryMenuToggled OnInventoryMenuToggled;

	UAZ_Inv_CommonUI_GameInventoryMenu* GetInventoryMenu() const { return InventoryMenu; }

private:
	
	UPROPERTY(Replicated)
	FAZ_Inv_CommonUI_InventoryFastArray InventoryList;

	TWeakObjectPtr<APlayerController> OwningController;

	UPROPERTY()
	TObjectPtr<UAZ_Inv_CommonUI_GameInventoryMenu> InventoryMenu;

	UPROPERTY(EditAnywhere, Category="AZ|Inventory")
	TSubclassOf<UAZ_Inv_CommonUI_GameInventoryMenu> InventoryScreenClass;

	bool bInventoryMenuOpen{false};
	void OpenInventoryMenu();
	void CloseInventoryMenu();
	void ConstructInventory();
	
	void SpawnDroppedItem(UAZ_Inv_CommonUI_InventoryItem* Item, int32 StackCount);

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float DropSpawnAngleMin = -85.f;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float DropSpawnAngleMax = 85.f;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float DropSpawnDistanceMin = 150.f;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float DropSpawnDistanceMax = 250.f;

	UPROPERTY(EditAnywhere, Category = "AZ|Inventory")
	float RelativeSpawnElevation = 70.f;
};
