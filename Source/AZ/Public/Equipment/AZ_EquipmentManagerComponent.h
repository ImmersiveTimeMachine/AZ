#pragma once

#include "CoreMinimal.h"
#include "Inventory/AZ_InventoryComponent.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "AZ_EquipmentManagerComponent.generated.h"

class AAZ_Item;
// Forward Declarations
class UAZ_InventoryItemInstance;
class UAZ_InventoryItemDefinition;
// NOTE: Replaced spawnable item definition with the new EquipmentDefinition
class UAZ_EquipmentDefinition;
// Removed AAZ_Weapon forward declaration; visuals are generic actors now
class UAZ_AbilitySystemComponent;
class AAZ_CharacterBase;

/** A replicated struct holding the data for a single item in an equipment slot. */
USTRUCT(BlueprintType)
struct FSlottedItem
{
    GENERATED_BODY()

    UPROPERTY()
    FGameplayTag SlotTag;

    UPROPERTY()
    TObjectPtr<UAZ_InventoryItemInstance> ItemInstance;

    FSlottedItem() : SlotTag(FGameplayTag()), ItemInstance(nullptr) {}
    FSlottedItem(const FGameplayTag& InTag, UAZ_InventoryItemInstance* InItem) : SlotTag(InTag), ItemInstance(InItem) {}
};

/** Delegate that broadcasts when the ACTIVE item in the character's hands changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActiveEquipmentChanged, const UAZ_InventoryItemDefinition*, ItemDef);

// Fire when overlapped items array updates (client-side via rep-notify).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShowPickupPrompt, bool, bVisiable );


/**
 * Manages a character's equipped items, handling the spawning and destruction
 * of both cosmetic (sheathed) and functional (active) weapon actors.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class AZ_API UAZ_EquipmentManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UAZ_EquipmentManagerComponent();

	/** Delegate that broadcasts when the active item changes. Essential for UI updates. */
	UPROPERTY(BlueprintAssignable, Category = "AZ|Equipment")
	FOnActiveEquipmentChanged OnActiveEquipmentChanged;

	/** Takes an item from inventory and attaches its cosmetic actor to a sheathed/holstered socket. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Equipment")
	void EquipItemToSlot(UAZ_InventoryItemInstance* ItemInstance, FGameplayTag SlotTag);

	/** Moves a weapon from its sheathed socket to the hand, making it the active weapon. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Equipment")
	void DrawWeaponFromSlot(FGameplayTag SlotTag);

	/** Unequips an item from a slot, destroying its spawned actor and removing it from the loadout. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Equipment")
	void UnequipItemFromSlot(FGameplayTag SlotTag);
	
	UFUNCTION(BlueprintCallable, Category = "AZ|Equipment")
	void CollectItem();

	/** The Character calls this function at startup to give this component a reference to the inventory. */
	void Initialize(UAZ_InventoryComponent* InInventoryComponent);

	// OverlappedItemsId helpers
	
	UFUNCTION(BlueprintPure, Category = "AZ|Interaction")
	const TArray<FGuid>& GetOverlappedItemsId() const { return OverlappedItemsId; }
	
	UFUNCTION(BlueprintCallable, Category = "AZ|Interaction")
	bool AddOverlappedItemId(const FGuid InItemId);

	UFUNCTION(BlueprintCallable, Category = "AZ|Interaction")
	bool RemoveOverlappedItemId(const FGuid InItemId);

	// OverlappedItems helpers
	
	UFUNCTION(BlueprintPure, Category = "AZ|Interaction")
	const TArray<AAZ_Item*>& GetOverlappedItems() const { return OverlappedItems; }
	
	UFUNCTION(BlueprintCallable, Category = "AZ|Interaction")
	bool AddOverlappedItem(AAZ_Item* InItem);

	UFUNCTION(BlueprintCallable, Category = "AZ|Interaction")
	bool RemoveOverlappedItem(AAZ_Item* InItem);

	UPROPERTY(BlueprintAssignable, Category="AZ|Interaction")
	FOnShowPickupPrompt OnShowPickupPrompt;
	
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Inventory")
	TMap<FGameplayTag, FName> TagToSocket;

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_OverlappedItemsId, Category = "AZ|Interaction")
	TArray<FGuid> OverlappedItemsId;

	UFUNCTION()
	void OnRep_OverlappedItemsId();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- Server RPCs for Client Requests ---
	UFUNCTION(Server, Reliable)
	void Server_EquipItemToSlot(UAZ_InventoryItemInstance* ItemInstance, FGameplayTag SlotTag);

	UFUNCTION(Server, Reliable)
	void Server_DrawWeaponFromSlot(FGameplayTag SlotTag);

	UFUNCTION(Server, Reliable)
	void Server_UnequipItemFromSlot(FGameplayTag SlotTag);

	// --- Replicated Properties & OnRep Functions ---
	UPROPERTY(ReplicatedUsing = OnRep_SlottedItems)
	TArray<FSlottedItem> SlottedItems;

	UPROPERTY(ReplicatedUsing = OnRep_ActiveSlotTag)
	FGameplayTag ActiveSlotTag;

	UFUNCTION()
	void OnRep_SlottedItems();
	
	UFUNCTION()
	void OnRep_ActiveSlotTag();

	UFUNCTION()
	void ItemAddedToInventoryChanged(FGuid InInventoryItemInstanceId);

private:

	UPROPERTY()
	TObjectPtr<UAZ_InventoryComponent> InventoryComponent;
	
	// --- Server-Only Data ---
	UPROPERTY()
	TMap<FGameplayTag, TObjectPtr<AActor>> SheathedWeaponActors;

	// --- Helper Functions ---
	
	void GrantAbilitiesAndEffectsFor(const UAZ_InventoryItemDefinition* ItemDef, UAZ_InventoryItemInstance* ItemInstance);
	void RemoveAbilitiesAndEffectsFor(const UAZ_InventoryItemDefinition* ItemDef);
	FSlottedItem* FindSlottedItem(const FGameplayTag& SlotTag);

	// Add this declaration to match the implementation in the .cpp
	UAZ_AbilitySystemComponent* GetOwnerASC() const;

	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;
	TArray<FActiveGameplayEffectHandle> GrantedEffectHandles;

	void HandleActiveSlotChanged_Server();
	void HandleEquipItemToSlot_Server(UAZ_InventoryItemInstance* ItemInstance, const FGameplayTag& SlotTag);
	void RefreshClientSheathedCosmetics();
	FName ResolveSheathedSocketForSlot(const FGameplayTag& SlotTag) const;

	

protected:

	UPROPERTY()
	TArray<AAZ_Item*> OverlappedItems;

	UFUNCTION()
	AActor* SpawnItem(TSubclassOf<AActor> ActorClass) const;

};