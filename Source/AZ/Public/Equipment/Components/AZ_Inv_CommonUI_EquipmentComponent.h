#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemState.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "AZ_Inv_CommonUI_EquipmentComponent.generated.h"

class UAZ_Inv_CommonUI_InventoryItem;
class UAZ_Inv_CommonUI_InventoryComponent;
class UAZ_AbilitySystemComponent;
class UAZ_GameplayAbility;
class UAZ_WeaponAnimationProfile;
class AAZ_Weapon;
class USkeletalMeshComponent;
class UAnimSequence;
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

/** Authority receipts for explicit quick-select requests; not another selection store. */
enum class EAZ_EquipmentRequestOutcome : uint8
{
	Rejected,
	Deferred,
	Activated,
	Superseded
};
DECLARE_MULTICAST_DELEGATE_ThreeParams(FAZ_EquipmentTrackedRequestResult, const FGuid&, EAZ_EquipmentRequestOutcome, const FText&);

/** Canonical owner of weapon selection, grants, and hand/back presentation. */
UCLASS(ClassGroup=(Custom), Blueprintable, meta=(BlueprintSpawnableComponent))
class AZ_API UAZ_Inv_CommonUI_EquipmentComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UAZ_Inv_CommonUI_EquipmentComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void SetOwningSkeletalMesh(USkeletalMeshComponent* OwningMesh);
	void SetIsProxy(bool bProxy) { bIsProxy = bProxy; }
	void InitializeOwner(APlayerController* PlayerController);

	UFUNCTION(BlueprintPure, Category="AZ|Equipment") AAZ_Weapon* GetActiveWeapon() const { return Selection.Weapon; }
	UFUNCTION(BlueprintPure, Category="AZ|Equipment") UAZ_Inv_CommonUI_InventoryItem* GetActiveItem() const { return Selection.Item; }
	UFUNCTION(BlueprintPure, Category="AZ|Equipment") FGameplayTag GetActiveProfile() const { return Selection.Profile; }
	UFUNCTION(BlueprintPure, Category="AZ|Equipment") int32 GetActiveIntrinsicSlotIndex() const { return Selection.IntrinsicSlotIndex; }
	uint32 GetSelectionGeneration() const { return Selection.Generation; }
	UFUNCTION(BlueprintPure, Category="AZ|Equipment") UAZ_WeaponAnimationProfile* GetActiveAnimationProfile() const;
	/** Game-thread cosmetic snapshot: prime the incoming base pose beneath its draw montage. */
	bool TryGetDrawAnimationPresentation(UAZ_WeaponAnimationProfile*& OutProfile, FGameplayTag& OutWeaponTag) const;
	UFUNCTION(BlueprintPure, Category="AZ|Equipment") bool IsSwitchingWeapon() const;
	/** A valid source is the committed, owned physical item and its current representation. */
	bool IsActiveWeaponSource(const UObject* Source) const;
	/** Begin one Ready lifetime. Repeated requests do not extend it; an accepted shot does. */
	bool BeginFirearmPreparation(AAZ_Weapon* ExpectedSource, const FGuid& ExpectedItemId,
		uint32 ExpectedGeneration, float& OutRaiseDelay, int32 PreparationKey = 0);
	/** Roll back a cancelled first-shot preparation without taking away an accepted shot's Ready period. */
	void CancelUnfulfilledFirearmPreparation(AAZ_Weapon* ExpectedSource, const FGuid& ExpectedItemId,
		uint32 ExpectedGeneration);
	/** Called only by authority's accepted shot or its reliable owning-player receipt. */
	bool RefreshFirearmReadyAfterShot(AAZ_Weapon* ExpectedSource, const FGuid& ExpectedItemId,
		uint32 ExpectedGeneration, const FGuid& ShotId, double AcceptedShotServerTime = -1.0,
		int32 AcceptedPreparationKey = 0);
	/** Effective raised posture; RMB precision and timed Ready have independent owners. */
	UFUNCTION(BlueprintPure, Category="AZ|Equipment|Firearm") bool IsFirearmRaised() const;
	/** Held automatic fire pins the posture without changing the last accepted-shot deadline. */
	bool BeginFirearmAutoHold(AAZ_Weapon* ExpectedSource, const FGuid& ExpectedItemId,
		uint32 ExpectedGeneration, const FGuid& FireActionId);
	void EndFirearmAutoHold(const FGuid& FireActionId);
	/** Retain an already raised posture through this exact reload; relaxed reloads stay relaxed. */
	bool BeginFirearmReloadHold(AAZ_Weapon* ExpectedSource, const FGuid& ExpectedItemId,
		uint32 ExpectedGeneration, const FGuid& ReloadActionId, bool bAuthoritativeRaised = false,
		double ReloadStartedServerTime = -1.0);
	void EndFirearmReloadHold(const FGuid& ReloadActionId, bool bCommitted);
	/** Release only this component's Ready and strafe contributions. */
	void CancelFirearmReady();
	/** Cancel firearm actions and discard their input without disturbing locomotion or melee. */
	void CancelActiveAim();
	/** Fire-mode changes stop firing and require another trigger press, while preserving held aim. */
	void CancelActiveFire();
	UFUNCTION(BlueprintCallable, Category="AZ|Equipment|Firearm") bool RequestCycleFireMode();
	/** One fresh dry trigger or one queued empty-shot request; never an inventory-change retry loop. */
	bool RequestReloadIfEmpty();
	bool RequestReloadIfEmpty(AAZ_Weapon* ExpectedSource, const FGuid& ExpectedItemId,
		uint32 ExpectedGeneration, const FGuid& ExpectedMagazineId, int64 ExpectedAmmoRevision);
	/** Authority entry for an exact magazine, manual ring, or automatic ring request. */
	bool RequestMagazineReload(AAZ_Weapon* ExpectedSource, const FGuid& ExpectedItemId,
		uint32 ExpectedGeneration, const FGuid& RequestedMagazineId, bool bSkipEmpty);

	/** True means accepted (possibly queued at the outgoing action's cancel boundary). */
	UFUNCTION(BlueprintCallable, Category="AZ|Equipment") bool RequestEquipItem(UAZ_Inv_CommonUI_InventoryItem* Item);
	UFUNCTION(BlueprintCallable, Category="AZ|Equipment") bool RequestEquipIntrinsic(int32 QuickSlotIndex);
	UFUNCTION(BlueprintCallable, Category="AZ|Equipment") bool RequestUnequipItem(UAZ_Inv_CommonUI_InventoryItem* Item);
	/** Authority-only adapter, retaining the normal validation and cancellation queue. */
	void RequestTrackedSelection(UAZ_Inv_CommonUI_InventoryItem* Item, int32 IntrinsicSlot, const FGuid& RequestId);
	/** Authority-only: a newer consumable selection replaces a queued weapon request, not current equipment. */
	void CancelPendingSelectionRequest();
	FAZ_EquipmentTrackedRequestResult OnTrackedRequestResult;
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
	struct FWeaponPresentation
	{
		TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> Item;
		TWeakObjectPtr<AAZ_Weapon> Weapon;
		uint64 HolsterOrder = 0;
	};
	/** Physical representations survive selection changes; inventory still owns all gameplay state. */
	TMap<FGuid, FWeaponPresentation> WeaponPresentations;
	uint64 NextHolsterOrder = 0;
	struct FWeaponSwitchPhase
	{
		TWeakObjectPtr<UAnimSequence> Animation;
		FName Slot;
		float PlayRate = 1.f;
		float BlendIn = .1f;
		float BlendOut = .12f;
		float SocketBlendDuration = .15f;
		double AttachTime = 0.;
		double Duration = 0.;
	};
	struct FWeaponSelectionTransition
	{
		FGuid Id;
		FGuid RequestId;
		FGuid PhaseId;
		TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> TargetItem;
		TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> SourceItem;
		TWeakObjectPtr<AAZ_Weapon> SourceWeapon;
		TWeakObjectPtr<AAZ_Weapon> TargetWeapon;
		TWeakObjectPtr<AAZ_Weapon> Coordinator;
		TWeakObjectPtr<APawn> Pawn;
		TWeakObjectPtr<USkeletalMeshComponent> Mesh;
		TWeakObjectPtr<UAZ_AbilitySystemComponent> ASC;
		FWeaponSwitchPhase Holster;
		FWeaponSwitchPhase Draw;
		uint32 SourceGeneration = 0;
		int32 TargetIntrinsicSlot = INDEX_NONE;
		bool bTargetWasItem = false;
		bool bCrouching = false;
		bool bHolsterPhase = true;
		bool bSocketApplied = false;
		double PhaseStartedAt = 0.;
	};
	FWeaponSelectionTransition WeaponTransition;
	FDelegateHandle SwitchInterruptedHandle;
	bool bOwnsSwitchingTag = false;
	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryItem> PendingItem;
	FGuid PendingRequestId;
	int32 PendingIntrinsicSlot = INDEX_NONE;
	bool bPendingSelection = false;
	bool bPendingWasItem = false;
	bool bRetryScheduled = false;
	bool bCommitting = false;
	bool bOwnsStrafeTag = false;
	bool bIsProxy = false;
	TMap<FGameplayTag, FDelegateHandle> GateDelegateHandles;
	FDelegateHandle AbilityEndedHandle;

	struct FFirearmReadyState
	{
		FGuid LifetimeId;
		FGuid ItemId;
		FGuid ReloadActionId;
		FGuid AutoFireActionId;
		TWeakObjectPtr<AAZ_Weapon> Weapon;
		TWeakObjectPtr<APawn> Pawn;
		TWeakObjectPtr<UAZ_AbilitySystemComponent> ASC;
		uint32 Generation = 0;
		double StartedServerTime = 0.0;
		double ExpiresServerTime = 0.0;
		double RaisedServerTime = 0.0;
		bool bOwnsReadyTag = false;
		bool bOwnsStrafeTag = false;
		bool bCreatedForReloadHold = false;
		TArray<FGuid> AcceptedShotIds;
		TArray<int32> PreparationKeys;
	};
	FFirearmReadyState FirearmReady;
	FTimerHandle FirearmReadyTimer;
	/** Explicit cancellation survives lifetime reset, so delayed server starts cannot re-raise the weapon. */
	double FirearmReadyCancelledServerTime = -1.0;
	TWeakObjectPtr<UWorld> FirearmReadyCancellationWorld;
	void ClearFirearmReady(bool bExplicitCancellation);
	bool ValidateFirearmReadySource(const AAZ_Weapon* ExpectedSource, const FGuid& ExpectedItemId,
		uint32 ExpectedGeneration) const;
	bool StartFirearmReady(AAZ_Weapon* ExpectedSource, const FGuid& ExpectedItemId,
		uint32 ExpectedGeneration, bool bForReloadHold, float& OutRaiseDelay, int32 PreparationKey = 0);
	void RecordFirearmPreparationKey(int32 PreparationKey);
	double GetFirearmReadyServerTime() const;
	float GetFirearmReadyDuration() const;
	void ScheduleFirearmReadyExpiry();
	void OnFirearmReadyExpired(FGuid ExpectedLifetimeId);

	UAZ_AbilitySystemComponent* GetASC() const;
	bool IsHardBlocked() const;
	bool IsActionCommitted() const;
	bool ValidateSelection(UAZ_Inv_CommonUI_InventoryItem* Item, int32 IntrinsicSlot) const;
	bool RequestSelection(UAZ_Inv_CommonUI_InventoryItem* Item, int32 IntrinsicSlot, const FGuid& RequestId = FGuid());
	bool CommitSelection(UAZ_Inv_CommonUI_InventoryItem* Item, int32 IntrinsicSlot);
	bool BeginSelectionChange(UAZ_Inv_CommonUI_InventoryItem* Item, int32 IntrinsicSlot, const FGuid& RequestId);
	bool BuildSwitchPhase(UAZ_Inv_CommonUI_InventoryItem* Item, bool bDraw, bool bCrouching, FWeaponSwitchPhase& Out) const;
	bool IsSwitchContextValid() const;
	void StartSwitchPhase(bool bHolster);
	void ApplySwitchSocket();
	void FinishSwitch();
	void CancelWeaponSwitch(EAZ_EquipmentRequestOutcome Outcome = EAZ_EquipmentRequestOutcome::Rejected, bool bRestorePresentation = true);
	void ClearWeaponSwitch();
	void OnSwitchAnimationInterrupted(const FGuid& PhaseId);
	void ReleaseActiveSelection();
	void ClearOutgoingInput();
	void GrantAbilities(const TArray<TSubclassOf<UAZ_GameplayAbility>>& Abilities, UObject* Source);
	AAZ_Weapon* PrepareWeaponActor(UAZ_Inv_CommonUI_InventoryItem* Item);
	void MarkWeaponCarried(UAZ_Inv_CommonUI_InventoryItem* Item);
	void RefreshCarryPresentation(bool bRestoreSockets = false, float BlendDuration = 0.f);
	/** Reconcile the selected weapon's hand mode or the selected rifle's temporary sprint carry. */
	void ReconcilePresentation();
	void DestroyPresentation(const FGuid& ItemId);
	void DestroyAllPresentations();
	void BindAbilityEvents();
	void UnbindAbilityEvents();
	void SchedulePendingSelection();
	void RetryPendingSelection();
	void ClearPendingSelection(EAZ_EquipmentRequestOutcome Outcome = EAZ_EquipmentRequestOutcome::Rejected);
	void FinishTrackedRequest(const FGuid& RequestId, EAZ_EquipmentRequestOutcome Outcome, const FText& Reason = FText::GetEmpty());
	void OnGateTagChanged(FGameplayTag Tag, int32 Count);
	void OnAbilityEnded(const FAbilityEndedData& Data);
	bool ValidateFireModeRequest(const AAZ_Weapon* WeaponSource, const FGuid& ItemId, uint32 Generation,
		int64 ExpectedRevision, EAZ_FirearmFireMode NewMode) const;
	bool CommitFireModeChange(AAZ_Weapon* WeaponSource, const FGuid& ItemId, uint32 Generation,
		int64 ExpectedRevision, EAZ_FirearmFireMode NewMode);
	void PublishSelection(UAZ_Inv_CommonUI_InventoryItem* PreviousItem);

	UFUNCTION() void OnRep_Selection(FAZ_EquipmentSelection Previous);
	UFUNCTION() void OnPossessedPawnChange(APawn* OldPawn, APawn* NewPawn);
	UFUNCTION() void OnInventoryChanged();
	UFUNCTION(Server, Reliable) void Server_RequestItem(FGuid ItemId, bool bEquip);
	UFUNCTION(Server, Reliable) void Server_RequestIntrinsic(int32 QuickSlotIndex);
	UFUNCTION(Server, Reliable) void Server_RequestFireMode(AAZ_Weapon* WeaponSource, FGuid ItemId,
		uint32 Generation, int64 ExpectedRevision, EAZ_FirearmFireMode NewMode);
	UFUNCTION(Server, Reliable) void Server_RequestReloadIfEmpty(AAZ_Weapon* WeaponSource, FGuid ItemId,
		uint32 Generation, FGuid ExpectedMagazineId, int64 ExpectedAmmoRevision);
	UFUNCTION(Client, Reliable) void Client_ClearOutgoingInput(const FGameplayTagContainer& InputTags);
};
