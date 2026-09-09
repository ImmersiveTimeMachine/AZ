#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "AZ_Inv_CommonUI_EquipmentComponent.generated.h"

class UAZ_Inv_CommonUI_InventoryItem;
class UAZ_Inv_CommonUI_InventoryComponent;
class UAZ_AbilitySystemComponent;
class UAZ_GameplayAbility;
class UAZ_WeaponAnimationProfile;
class AAZ_Weapon;
class USkeletalMeshComponent;
struct FAbilityEndedData;

/** One committed selection, replicated together so UI never guesses ahead of authority. */
USTRUCT()
struct FAZ_EquipmentSelection
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UAZ_Inv_CommonUI_InventoryItem> Item = nullptr;
	UPROPERTY() TObjectPtr<AAZ_Weapon> Weapon = nullptr;
	UPROPERTY() FGameplayTag Profile;
	UPROPERTY() int32 IntrinsicSlotIndex = INDEX_NONE;
	UPROPERTY() uint32 Generation = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAZ_EquipmentChanged);

/** Canonical owner of weapon selection, grants, and hand/back presentation. */
UCLASS(ClassGroup=(Custom), Blueprintable, meta=(BlueprintSpawnableComponent))
class AZ_API UAZ_Inv_CommonUI_EquipmentComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAZ_Inv_CommonUI_EquipmentComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void SetOwningSkeletalMesh(USkeletalMeshComponent* OwningMesh);
	void SetIsProxy(bool bProxy) { bIsProxy = bProxy; }
	void InitializeOwner(APlayerController* PlayerController);

	UFUNCTION(BlueprintPure, Category="AZ|Equipment") AAZ_Weapon* GetActiveWeapon() const { return Selection.Weapon; }
	UFUNCTION(BlueprintPure, Category="AZ|Equipment") UAZ_Inv_CommonUI_InventoryItem* GetActiveItem() const { return Selection.Item; }
	UFUNCTION(BlueprintPure, Category="AZ|Equipment") FGameplayTag GetActiveProfile() const { return Selection.Profile; }
	UFUNCTION(BlueprintPure, Category="AZ|Equipment") int32 GetActiveIntrinsicSlotIndex() const { return Selection.IntrinsicSlotIndex; }
	uint32 GetSelectionGeneration() const { return Selection.Generation; }
	UFUNCTION(BlueprintPure, Category="AZ|Equipment") UAZ_WeaponAnimationProfile* GetActiveAnimationProfile() const;
	/** A valid source is the committed, owned physical item and its current representation. */
	bool IsActiveWeaponSource(const UObject* Source) const;
	/** Cancel aim/fire and discard their input without disturbing locomotion or melee. */
	void CancelActiveAim();

	/** True means accepted (possibly queued at the outgoing action's cancel boundary). */
	UFUNCTION(BlueprintCallable, Category="AZ|Equipment") bool RequestEquipItem(UAZ_Inv_CommonUI_InventoryItem* Item);
	UFUNCTION(BlueprintCallable, Category="AZ|Equipment") bool RequestEquipIntrinsic(int32 QuickSlotIndex);
	UFUNCTION(BlueprintCallable, Category="AZ|Equipment") bool RequestUnequipItem(UAZ_Inv_CommonUI_InventoryItem* Item);
	bool CanDropItem(UAZ_Inv_CommonUI_InventoryItem* Item) const;
	/** Called by inventory only after it prepared a valid world representation. */
	void PrepareItemForDrop(UAZ_Inv_CommonUI_InventoryItem* Item);
	UPROPERTY(BlueprintAssignable, Category="AZ|Equipment") FAZ_EquipmentChanged OnEquipmentChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(ReplicatedUsing=OnRep_Selection) FAZ_EquipmentSelection Selection;
	UPROPERTY() TArray<FGameplayAbilitySpecHandle> GrantedHandles;
	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryComponent> InventoryComponent;
	TWeakObjectPtr<APlayerController> OwningPlayerController;
	TWeakObjectPtr<USkeletalMeshComponent> OwningSkeletalMesh;
	TWeakObjectPtr<UAZ_AbilitySystemComponent> BoundASC;
	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> PresentedItem;
	TWeakObjectPtr<AAZ_Weapon> PresentedWeapon;
	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> PendingItem;
	int32 PendingIntrinsicSlot = INDEX_NONE;
	bool bPendingSelection = false;
	bool bPendingWasItem = false;
	bool bRetryScheduled = false;
	bool bCommitting = false;
	bool bOwnsStrafeTag = false;
	bool bIsProxy = false;
	TMap<FGameplayTag, FDelegateHandle> GateDelegateHandles;
	FDelegateHandle AbilityEndedHandle;

	UAZ_AbilitySystemComponent* GetASC() const;
	bool IsHardBlocked() const;
	bool IsActionCommitted() const;
	bool ValidateSelection(UAZ_Inv_CommonUI_InventoryItem* Item, int32 IntrinsicSlot) const;
	bool RequestSelection(UAZ_Inv_CommonUI_InventoryItem* Item, int32 IntrinsicSlot);
	bool CommitSelection(UAZ_Inv_CommonUI_InventoryItem* Item, int32 IntrinsicSlot);
	void ReleaseActiveSelection();
	void ClearOutgoingInput();
	void GrantAbilities(const TArray<TSubclassOf<UAZ_GameplayAbility>>& Abilities, UObject* Source);
	AAZ_Weapon* PrepareWeaponActor(UAZ_Inv_CommonUI_InventoryItem* Item);
	/** Reconcile the selected weapon's hand mode or the selected rifle's temporary sprint carry. */
	void ReconcilePresentation();
	void DestroyPresentation();
	void BindAbilityEvents();
	void UnbindAbilityEvents();
	void SchedulePendingSelection();
	void RetryPendingSelection();
	void ClearPendingSelection();
	void OnGateTagChanged(FGameplayTag Tag, int32 Count);
	void OnAbilityEnded(const FAbilityEndedData& Data);
	void PublishSelection(UAZ_Inv_CommonUI_InventoryItem* PreviousItem);

	UFUNCTION() void OnRep_Selection(FAZ_EquipmentSelection Previous);
	UFUNCTION() void OnPossessedPawnChange(APawn* OldPawn, APawn* NewPawn);
	UFUNCTION() void OnInventoryChanged();
	UFUNCTION(Server, Reliable) void Server_RequestItem(FGuid ItemId, bool bEquip);
	UFUNCTION(Server, Reliable) void Server_RequestIntrinsic(int32 QuickSlotIndex);
	UFUNCTION(Client, Reliable) void Client_ClearOutgoingInput(const FGameplayTagContainer& InputTags);
};
