#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "AZ_QuickBarComponent.generated.h"

class UAZ_GameplayAbility;
class UGameplayEffect;
class UAZ_Inv_CommonUI_InventoryItem;
class UAZ_Inv_CommonUI_InventoryComponent;
class UAZ_Inv_CommonUI_EquipmentComponent;
class UTexture2D;
class APawn;
enum class EAZ_EquipmentRequestOutcome : uint8;

UENUM(BlueprintType)
enum class EAZ_QuickSlotPosition : uint8
{
	Center,
	Left,
	Right,
	Up,
	Down
};

UENUM(BlueprintType)
enum class EAZ_QuickBarRequestOutcome : uint8
{
	Assigned,
	Rejected,
	Deferred,
	Activated,
	Superseded
};

/** An owner receipt. Equipment owns active weapons; QuickBar owns a readied consumable identity. */
USTRUCT(BlueprintType)
struct FAZ_QuickBarRequestResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="AZ|QuickBar") FGuid RequestId;
	UPROPERTY(BlueprintReadOnly, Category="AZ|QuickBar") int32 SlotIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category="AZ|QuickBar") FGuid ItemId;
	UPROPERTY(BlueprintReadOnly, Category="AZ|QuickBar") EAZ_QuickBarRequestOutcome Outcome = EAZ_QuickBarRequestOutcome::Rejected;
	UPROPERTY(BlueprintReadOnly, Category="AZ|QuickBar") int64 BindingRevision = 0;
	UPROPERTY(BlueprintReadOnly, Category="AZ|QuickBar") FText Reason;
};

USTRUCT()
struct FAZ_QuickSlotBinding
{
	GENERATED_BODY()
	UPROPERTY() FGuid ItemId;
	/** Records a user-authored assignment, including a slot emptied by removal. */
	UPROPERTY() bool bExplicit = false;
	bool operator==(const FAZ_QuickSlotBinding&) const = default;
};

/** Bindings and their revision travel together; no client-side optimistic assignment. */
USTRUCT()
struct FAZ_QuickBarBindingState
{
	GENERATED_BODY()
	UPROPERTY() TArray<FAZ_QuickSlotBinding> Slots;
	UPROPERTY() int64 Revision = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAZ_QuickBarBindingsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAZ_QuickBarReadyItemChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAZ_QuickBarRequestCompleted, const FAZ_QuickBarRequestResult&, Result);

/** Intrinsic profiles keep their authored data; physical slots bind an owned item identity. */
USTRUCT(BlueprintType)
struct FAZ_QuickSlot
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickBar") FGameplayTag WeaponTag;
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickBar") TArray<TSubclassOf<UAZ_GameplayAbility>> WeaponAbilities;
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickBar") bool bStrafeOnEquip = false;
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickBar") TArray<TSubclassOf<UGameplayEffect>> EffectsOnEquip;
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickBar") bool bInventoryBacked = false;
	/** Legacy content field. All inventory-backed quick slots now accept the same eligible items. */
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickBar", meta=(EditCondition="bInventoryBacked")) FGameplayTag InventoryItemType;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|QuickBar|Presentation") FText DisplayName;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|QuickBar|Presentation") TObjectPtr<UTexture2D> Icon = nullptr;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|QuickBar|Presentation") EAZ_QuickSlotPosition Position = EAZ_QuickSlotPosition::Center;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|QuickBar") bool bEnabled = true;
};

/** Manual shortcut bindings and consumable readiness. Equipment owns all grants and weapon transitions. */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AZ_API UAZ_QuickBarComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAZ_QuickBarComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION(BlueprintCallable, Category="AZ|QuickBar") void Select(int32 SlotIndex);
	UFUNCTION(BlueprintCallable, Category="AZ|QuickBar") void CycleNext();
	UFUNCTION(BlueprintCallable, Category="AZ|QuickBar") void CyclePrev();
	UFUNCTION(BlueprintPure, Category="AZ|QuickBar") int32 GetActiveSlotIndex() const;
	/** Derived from committed equipment, independent of readied consumables or UI previews. */
	UFUNCTION(BlueprintPure, Category="AZ|QuickBar|CombatMode") bool IsFightMode() const;
	UFUNCTION(BlueprintPure, Category="AZ|QuickBar|CombatMode") int64 GetCombatModeRevision() const;
	/** Explore holsters; Fight uses intrinsic Fists unless last-weapon restoration is explicitly enabled. */
	UFUNCTION(BlueprintCallable, Category="AZ|QuickBar|CombatMode") void RequestToggleCombatMode(
		int64 ExpectedSelectionGeneration, FGuid RequestId);
	/** Readiness selects a consumable for a separate use action; it never consumes or grants effects. */
	UFUNCTION(BlueprintPure, Category="AZ|QuickBar") FGuid GetReadyItemId() const { return ReadyItemId; }
	UFUNCTION(BlueprintPure, Category="AZ|QuickBar") UAZ_Inv_CommonUI_InventoryItem* GetReadyItem() const;
	UFUNCTION(BlueprintPure, Category="AZ|QuickBar") int32 GetReadySlotIndex() const;
	UFUNCTION(BlueprintPure, Category="AZ|QuickBar") UAZ_Inv_CommonUI_InventoryItem* GetBoundItem(int32 SlotIndex) const;
	UFUNCTION(BlueprintPure, Category="AZ|QuickBar") FGuid GetBoundItemId(int32 SlotIndex) const;
	UFUNCTION(BlueprintPure, Category="AZ|QuickBar") bool IsBindingExplicit(int32 SlotIndex) const;
	UFUNCTION(BlueprintPure, Category="AZ|QuickBar") int32 GetSlotCount() const { return Slots.Num(); }
	UFUNCTION(BlueprintPure, Category="AZ|QuickBar") int64 GetBindingRevision() const { return Bindings.Revision; }
	UFUNCTION(BlueprintPure, Category="AZ|QuickBar") TArray<UAZ_Inv_CommonUI_InventoryItem*> GetCompatibleItems(int32 SlotIndex) const;
	UFUNCTION(BlueprintCallable, Category="AZ|QuickBar") bool BindItemToSlot(int32 SlotIndex, UAZ_Inv_CommonUI_InventoryItem* Item);
	/** Assignment never equips. Repeated RequestIds replay their receipt without another mutation. */
	UFUNCTION(BlueprintCallable, Category="AZ|QuickBar") void RequestAssignItem(int32 SlotIndex, FGuid CandidateItemId,
		int64 ExpectedRevision, FGuid RequestId);
	/** Non-toggle activation; intrinsic slots require an invalid ExpectedBoundItemId. */
	UFUNCTION(BlueprintCallable, Category="AZ|QuickBar") void RequestActivateSlot(int32 SlotIndex, FGuid ExpectedBoundItemId,
		int64 ExpectedRevision, FGuid RequestId);
	UPROPERTY(BlueprintAssignable, Category="AZ|QuickBar") FAZ_QuickBarBindingsChanged OnBindingsChanged;
	UPROPERTY(BlueprintAssignable, Category="AZ|QuickBar") FAZ_QuickBarReadyItemChanged OnReadyItemChanged;
	UPROPERTY(BlueprintAssignable, Category="AZ|QuickBar") FAZ_QuickBarRequestCompleted OnRequestResult;
	const FAZ_QuickSlot* GetSlotDefinition(int32 SlotIndex) const;
	/** Compatibility callback; equipment changes never assign quick slots. */
	void BindSelectedItem(UAZ_Inv_CommonUI_InventoryItem* Item);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(EditDefaultsOnly, Category="AZ|QuickBar") TArray<FAZ_QuickSlot> Slots;
	/** Optional policy. Default preserves Fists as the Explore-to-Fight destination. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AZ|QuickBar|CombatMode")
	bool bRestoreLastWeaponOnFight = false;

private:
	UPROPERTY(ReplicatedUsing=OnRep_Bindings) FAZ_QuickBarBindingState Bindings;
	UPROPERTY(ReplicatedUsing=OnRep_ReadyItemId) FGuid ReadyItemId;
	/** Authority-only recollection, never a binding and never carried across possession. */
	FGuid LastFightWeaponId;
	TWeakObjectPtr<UAZ_Inv_CommonUI_EquipmentComponent> BoundEquipment;
	FDelegateHandle EquipmentRequestHandle;
	TMap<FGuid, FAZ_QuickBarRequestResult> RequestReceipts;
	TArray<FGuid> ReceiptOrder;
	UAZ_Inv_CommonUI_InventoryComponent* GetInventory() const;
	UAZ_Inv_CommonUI_EquipmentComponent* GetEquipment() const;
	bool CanBindItem(int32 SlotIndex, const UAZ_Inv_CommonUI_InventoryItem* Item) const;
	bool IsOwnedPhysicalWeapon(const UAZ_Inv_CommonUI_InventoryItem* Item) const;
	UAZ_Inv_CommonUI_InventoryItem* GetRememberedFightWeapon() const;
	bool SetBinding(int32 SlotIndex, const FGuid& ItemId, bool bExplicit);
	void PublishBindingsIfChanged(const TArray<FAZ_QuickSlotBinding>& Previous);
	bool CanReadyConsumable() const;
	void SetReadyItemId(const FGuid& ItemId);
	void PruneReadyItem();
	void BindEquipmentEvents();
	void HandleEquipmentRequestResult(const FGuid& RequestId, EAZ_EquipmentRequestOutcome Outcome, const FText& Reason);
	bool BeginRequest(int32 SlotIndex, const FGuid& ItemId, const FGuid& RequestId);
	void CompleteRequest(const FGuid& RequestId, EAZ_QuickBarRequestOutcome Outcome, const FText& Reason = FText::GetEmpty());
	void SendRequestResult(const FAZ_QuickBarRequestResult& Result);
	bool AssignItemInternal(int32 SlotIndex, const FGuid& CandidateItemId, int64 ExpectedRevision, const FGuid& RequestId);
	void ActivateSlotInternal(int32 SlotIndex, const FGuid& ExpectedBoundItemId, int64 ExpectedRevision, const FGuid& RequestId);
	void ToggleCombatModeInternal(int64 ExpectedSelectionGeneration, const FGuid& RequestId);
	void SelectInternal(int32 SlotIndex);
	void Cycle(int32 Direction);
	UFUNCTION() void OnInventoryChanged();
	UFUNCTION() void HandleEquipmentChanged();
	UFUNCTION() void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);
	UFUNCTION() void OnRep_Bindings();
	UFUNCTION() void OnRep_ReadyItemId();
	UFUNCTION(Server, Reliable) void Server_Select(int32 SlotIndex);
	UFUNCTION(Server, Reliable) void Server_RequestAssignItem(int32 SlotIndex, FGuid CandidateItemId, int64 ExpectedRevision, FGuid RequestId);
	UFUNCTION(Server, Reliable) void Server_RequestActivateSlot(int32 SlotIndex, FGuid ExpectedBoundItemId, int64 ExpectedRevision, FGuid RequestId);
	UFUNCTION(Server, Reliable) void Server_RequestToggleCombatMode(int64 ExpectedSelectionGeneration, FGuid RequestId);
	UFUNCTION(Client, Reliable) void Client_RequestResult(const FAZ_QuickBarRequestResult& Result);
};
