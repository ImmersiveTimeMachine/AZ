// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_CommonUI_GameInventoryMenu.h"
#include "Components/ActorComponent.h"
#include "InventoryUI/FastArray/AZ_Inv_CommonUI_FastArray.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemState.h"
#include "AZ_Inv_CommonUI_InventoryComponent.generated.h"


class UAZ_Inv_CommonUI_InventoryItem;
class UAZ_Inv_CommonUI_ItemComponent;
class AAZ_Weapon;
struct FAZ_Inv_CommonUI_ItemManifest;
struct FAZ_InventoryPickupRecord;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonUI_InventoryItemChanged, UAZ_Inv_CommonUI_InventoryItem*, InventoryItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCommonUI_NoRoomInInventory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonUI_StackChange, const FAZ_Inv_CommonUI_SlotAvailabilityResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonUI_ItemEquipStatusChanged, UAZ_Inv_CommonUI_InventoryItem*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCommonUI_InventoryMenuToggled, bool, bOpen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCommonUI_InventoryChanged);

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void ToggleInventoryMenu();
	bool IsMenuOpen() const { return bInventoryMenuOpen; }
	TArray<UAZ_Inv_CommonUI_InventoryItem*> GetItems() const { return InventoryList.GetAllItems(); }
	bool ContainsItem(const UAZ_Inv_CommonUI_InventoryItem* Item) const;
	UAZ_Inv_CommonUI_InventoryItem* FindItemById(const FGuid& ItemId) const;
	UFUNCTION(BlueprintPure, Category="AZ|Inventory|Ammo")
	FAZ_WeaponAmmoSnapshot GetWeaponAmmoSnapshot(const FGuid& WeaponItemId) const;
	/** Authority only. A successful compare-and-swap authorizes one shot; retain its expected revision across callbacks. */
	bool TryConsumeWeaponRound(const UObject* WeaponSource, const FGuid& WeaponItemId,
		const FGuid& ExpectedMagazineId, int64 ExpectedAmmoRevision, uint32 ExpectedEquipmentGeneration,
		const FGuid& ShotId, FAZ_WeaponAmmoSnapshot& OutSnapshot);
	/** Authority-clock cadence for this physical weapon; zero means no pending restriction. */
	double GetWeaponNextAllowedFireTime(const FGuid& WeaponItemId) const;
	/** Authority only. Equipment validates/cancels firing first; this revalidates and commits the item mutation. */
	bool TrySetWeaponFireMode(const UObject* WeaponSource, const FGuid& WeaponItemId,
		uint32 ExpectedEquipmentGeneration, int64 ExpectedFireModeRevision, EAZ_FirearmFireMode NewMode);
	/** Invalid requested ID cycles by stable magazine identity. Manual cycling includes empty magazines. */
	bool CanReloadMagazine(const UObject* WeaponSource, const FGuid& WeaponItemId, uint32 ExpectedEquipmentGeneration,
		const FGuid& RequestedMagazineId = FGuid(), bool bSkipEmpty = false) const;
	/** Reserve the exact requested magazine or next eligible identity and its return space without changing rounds. */
	bool TryBeginMagazineReload(const UObject* WeaponSource, const FGuid& WeaponItemId,
		uint32 ExpectedEquipmentGeneration, const FGuid& ReloadId,
		const FGuid& RequestedMagazineId = FGuid(), bool bSkipEmpty = false);
	/** Commit the matching swap once. An already committed live action returns true without another mutation. */
	bool TryCommitMagazineReload(const FGuid& ReloadId);
	/** Read before ending the action, including during reentrant inventory-changed callbacks. */
	bool IsMagazineReloadCommitted(const FGuid& ReloadId) const;
	/** End/cancel only this action; an already committed swap is never rolled back. */
	void EndMagazineReload(const FGuid& ReloadId);
	bool IsItemReloadReserved(const FGuid& ItemId) const;
	bool IsWeaponReloading(const FGuid& WeaponItemId) const;
	/** Inventory-menu preview for loading this exact magazine into the active rifle. */
	bool CanLoadMagazine(const UAZ_Inv_CommonUI_InventoryItem* Item) const;
	/** Submit an exact-magazine reload after releasing inventory input capture. Never swaps ammunition directly. */
	bool RequestLoadMagazine(UAZ_Inv_CommonUI_InventoryItem* Item);
	FIntPoint GetGridDimensions(EInv_ItemCategory Category) const;
	const TArray<FAZ_InventoryGridPlacement>& GetPlacements() const { return GridPlacements; }
	FAZ_Inv_CommonUI_SlotAvailabilityResult GetRoomForItem(const FAZ_Inv_CommonUI_ItemManifest& Manifest, int32 StackAmountOverride = -1) const;
	bool TryPickupItem(UAZ_Inv_CommonUI_ItemComponent* ItemComponent);
	void NotifyInventoryChanged();
	virtual void ReadyForReplication() override;

	UFUNCTION(Server, Reliable)
	void Server_MoveItem(UAZ_Inv_CommonUI_InventoryItem* Item, int32 SourceGridIndex, int32 TargetGridIndex, int32 StackCount);
	
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

	/** Published after an ownership/placement transaction commits. Widgets render this snapshot. */
	UPROPERTY(BlueprintAssignable, Category="AZ|Inventory")
	FCommonUI_InventoryChanged OnInventoryChanged;

	FCommonUI_ItemEquipStatusChanged OnItemEquipped;
	FCommonUI_ItemEquipStatusChanged OnItemUnequipped;
	FCommonUI_InventoryItemChanged OnItemDropped;
	FCommonUI_InventoryMenuToggled OnInventoryMenuToggled;

	UAZ_Inv_CommonUI_GameInventoryMenu* GetInventoryMenu() const { return InventoryMenu; }

private:
	UFUNCTION(Server, Reliable)
	void Server_LoadMagazine(AAZ_Weapon* ExpectedSource, FGuid WeaponItemId, uint32 ExpectedGeneration,
		FGuid MagazineItemId, int64 ExpectedIncomingRevision, FGuid ExpectedInsertedMagazineId, int64 ExpectedInsertedRevision);
	
	UPROPERTY(Replicated)
	FAZ_Inv_CommonUI_InventoryFastArray InventoryList;
	UPROPERTY(ReplicatedUsing=OnRep_Placements)
	TArray<FAZ_InventoryGridPlacement> GridPlacements;

	UPROPERTY(EditAnywhere, Category="AZ|Inventory|Capacity", meta=(ClampMin="1"))
	FIntPoint EquippableGridDimensions = FIntPoint(11, 7);
	UPROPERTY(EditAnywhere, Category="AZ|Inventory|Capacity", meta=(ClampMin="1"))
	FIntPoint ConsumableGridDimensions = FIntPoint(11, 7);
	UPROPERTY(EditAnywhere, Category="AZ|Inventory|Capacity", meta=(ClampMin="1"))
	FIntPoint CraftableGridDimensions = FIntPoint(11, 7);
	UPROPERTY(EditAnywhere, Category="AZ|Inventory|Pickup", meta=(ClampMin="0"))
	float MaximumPickupDistance = 350.f;

	UFUNCTION()
	void OnRep_Placements();
	bool ValidatePickupPayload(const FAZ_InventoryPickupRecord& Root, const TArray<FAZ_InventoryPickupRecord>& Children) const;
	bool IsPlacementFree(const FAZ_Inv_CommonUI_ItemManifest& Manifest, int32 GridIndex,
		const TArray<FAZ_InventoryGridPlacement>& Placements, bool bIncludeReloadReservation = true) const;
	struct FMagazineReloadReservation
	{
		FGuid ReloadId;
		TWeakObjectPtr<const UObject> WeaponSource;
		FGuid WeaponItemId;
		uint32 EquipmentGeneration = 0;
		FGuid IncomingMagazineId;
		FGuid OutgoingMagazineId;
		int64 IncomingAmmoRevision = -1;
		int64 OutgoingAmmoRevision = -1;
		int32 IncomingRounds = 0;
		int32 OutgoingRounds = 0;
		FAZ_InventoryGridPlacement IncomingPlacement;
		FAZ_InventoryGridPlacement ReturnPlacement;
		bool bSkipEmpty = false;
		bool bCommitted = false;
	};
	FMagazineReloadReservation MagazineReload;
	bool bMagazineReloadMutation = false;
	bool BuildMagazineReloadReservation(const UObject* WeaponSource, const FGuid& WeaponItemId,
		uint32 ExpectedEquipmentGeneration, const FGuid& RequestedMagazineId, bool bSkipEmpty,
		FMagazineReloadReservation& OutReservation) const;
	void RemoveOwnedItem(UAZ_Inv_CommonUI_InventoryItem* Item);
	bool RemoveStackPlacements(const FGuid& ItemId, int32 Count);
	/** Authoritative cadence survives fire ability restarts and selection changes. Expired entries are pruned on shot requests. */
	TMap<FGuid, double> WeaponNextShotTimes;

	TWeakObjectPtr<APlayerController> OwningController;

	UPROPERTY()
	TObjectPtr<UAZ_Inv_CommonUI_GameInventoryMenu> InventoryMenu;

	UPROPERTY(EditAnywhere, Category="AZ|Inventory")
	TSubclassOf<UAZ_Inv_CommonUI_GameInventoryMenu> InventoryScreenClass;

	bool bInventoryMenuOpen{false};
	void OpenInventoryMenu();
	void CloseInventoryMenu();
	void ConstructInventory();
	
	AActor* SpawnDroppedItem(UAZ_Inv_CommonUI_InventoryItem* Item, int32 StackCount);

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
