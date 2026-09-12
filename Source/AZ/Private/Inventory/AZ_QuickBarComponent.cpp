#include "Inventory/AZ_QuickBarComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h"
#include "AZ_GameplayTags.h"
#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Player/AZ_PlayerController.h"

UAZ_QuickBarComponent::UAZ_QuickBarComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAZ_QuickBarComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAZ_QuickBarComponent, Bindings);
	DOREPLIFETIME(UAZ_QuickBarComponent, ReadyItemId);
}

UAZ_Inv_CommonUI_InventoryComponent* UAZ_QuickBarComponent::GetInventory() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>() : nullptr;
}

UAZ_Inv_CommonUI_EquipmentComponent* UAZ_QuickBarComponent::GetEquipment() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>() : nullptr;
}

const FAZ_QuickSlot* UAZ_QuickBarComponent::GetSlotDefinition(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? &Slots[SlotIndex] : nullptr;
}

UAZ_Inv_CommonUI_InventoryItem* UAZ_QuickBarComponent::GetReadyItem() const
{
	if (!ReadyItemId.IsValid()) return nullptr;
	for (int32 Index = 0; Index < Bindings.Slots.Num(); ++Index)
	{
		if (Bindings.Slots[Index].ItemId != ReadyItemId) continue;
		UAZ_Inv_CommonUI_InventoryItem* Item = GetBoundItem(Index);
		return Item && Item->IsConsumable() ? Item : nullptr;
	}
	return nullptr;
}

int32 UAZ_QuickBarComponent::GetReadySlotIndex() const
{
	if (!GetReadyItem()) return INDEX_NONE;
	return Bindings.Slots.IndexOfByPredicate([this](const FAZ_QuickSlotBinding& Binding)
	{
		return Binding.ItemId == ReadyItemId;
	});
}

UAZ_Inv_CommonUI_InventoryItem* UAZ_QuickBarComponent::GetBoundItem(int32 SlotIndex) const
{
	const UAZ_Inv_CommonUI_InventoryComponent* Inventory = GetInventory();
	UAZ_Inv_CommonUI_InventoryItem* Item = Inventory ? Inventory->FindItemById(GetBoundItemId(SlotIndex)) : nullptr;
	return CanBindItem(SlotIndex, Item) ? Item : nullptr;
}

FGuid UAZ_QuickBarComponent::GetBoundItemId(int32 SlotIndex) const
{
	return Bindings.Slots.IsValidIndex(SlotIndex) ? Bindings.Slots[SlotIndex].ItemId : FGuid();
}

bool UAZ_QuickBarComponent::IsBindingExplicit(int32 SlotIndex) const
{
	return Bindings.Slots.IsValidIndex(SlotIndex) && Bindings.Slots[SlotIndex].bExplicit;
}

TArray<UAZ_Inv_CommonUI_InventoryItem*> UAZ_QuickBarComponent::GetCompatibleItems(int32 SlotIndex) const
{
	TArray<UAZ_Inv_CommonUI_InventoryItem*> Result;
	if (const UAZ_Inv_CommonUI_InventoryComponent* Inventory = GetInventory())
	{
		for (UAZ_Inv_CommonUI_InventoryItem* Item : Inventory->GetItems())
		{
			if (CanBindItem(SlotIndex, Item)) Result.Add(Item);
		}
	}
	// An inventory replication refresh cannot randomly reorder the wheel's candidates.
	Result.Sort([](const UAZ_Inv_CommonUI_InventoryItem& A, const UAZ_Inv_CommonUI_InventoryItem& B)
	{
		return A.GetInstanceId().ToString() < B.GetInstanceId().ToString();
	});
	return Result;
}

int32 UAZ_QuickBarComponent::GetActiveSlotIndex() const
{
	const UAZ_Inv_CommonUI_EquipmentComponent* Equipment = GetEquipment();
	if (!Equipment) return INDEX_NONE;
	if (const UAZ_Inv_CommonUI_InventoryItem* Item = Equipment->GetActiveItem())
	{
		return Bindings.Slots.IndexOfByPredicate([Item](const FAZ_QuickSlotBinding& Binding)
		{
			return Binding.ItemId == Item->GetInstanceId();
		});
	}
	return Equipment->GetActiveIntrinsicSlotIndex();
}

bool UAZ_QuickBarComponent::IsFightMode() const
{
	const UAZ_Inv_CommonUI_EquipmentComponent* Equipment = GetEquipment();
	if (!Equipment) return false;
	const FGameplayTag Profile = Equipment->GetActiveProfile();
	const FGameplayTag None = FAZ_GameplayTags::Get().Weapon_None;
	return Profile.IsValid() && Profile != None && Profile.MatchesTag(None.RequestDirectParent());
}

int64 UAZ_QuickBarComponent::GetCombatModeRevision() const
{
	const UAZ_Inv_CommonUI_EquipmentComponent* Equipment = GetEquipment();
	return Equipment ? static_cast<int64>(Equipment->GetSelectionGeneration()) : INDEX_NONE;
}

bool UAZ_QuickBarComponent::IsOwnedPhysicalWeapon(const UAZ_Inv_CommonUI_InventoryItem* Item) const
{
	const UAZ_Inv_CommonUI_InventoryComponent* Inventory = GetInventory();
	return IsValid(Item) && Item->IsInitialized() && Item->IsWeapon() && !Item->IsConsumable()
		&& Inventory && Inventory->ContainsItem(Item) && Item->GetLocation() == EAZ_InventoryItemLocation::Backpack
		&& !Item->GetParentItemId().IsValid() && Item->GetTotalStackCount() == 1
		&& Item->GetWeaponProfileTag().IsValid()
		&& Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_EquipmentFragment>();
}

UAZ_Inv_CommonUI_InventoryItem* UAZ_QuickBarComponent::GetRememberedFightWeapon() const
{
	const UAZ_Inv_CommonUI_InventoryComponent* Inventory = GetInventory();
	UAZ_Inv_CommonUI_InventoryItem* Item = Inventory ? Inventory->FindItemById(LastFightWeaponId) : nullptr;
	return IsOwnedPhysicalWeapon(Item) ? Item : nullptr;
}

bool UAZ_QuickBarComponent::CanBindItem(int32 SlotIndex, const UAZ_Inv_CommonUI_InventoryItem* Item) const
{
	const FAZ_QuickSlot* Slot = GetSlotDefinition(SlotIndex);
	const UAZ_Inv_CommonUI_InventoryComponent* Inventory = GetInventory();
	return Slot && Slot->bEnabled && Slot->bInventoryBacked && IsValid(Item) && Item->IsInitialized()
		&& Inventory && Inventory->ContainsItem(Item)
		&& Item->GetLocation() == EAZ_InventoryItemLocation::Backpack
		&& !Item->GetParentItemId().IsValid()
		&& Item->GetTotalStackCount() > 0
		&& (Item->IsConsumable() || (Item->IsWeapon()
			&& Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_EquipmentFragment>()));
}

bool UAZ_QuickBarComponent::BindItemToSlot(int32 SlotIndex, UAZ_Inv_CommonUI_InventoryItem* Item)
{
	if (!GetOwner() || !CanBindItem(SlotIndex, Item)) return false;
	const FGuid RequestId = FGuid::NewGuid();
	if (!GetOwner()->HasAuthority())
	{
		Server_RequestAssignItem(SlotIndex, Item->GetInstanceId(), Bindings.Revision, RequestId);
		return true;
	}
	return AssignItemInternal(SlotIndex, Item->GetInstanceId(), Bindings.Revision, RequestId);
}

bool UAZ_QuickBarComponent::SetBinding(int32 SlotIndex, const FGuid& ItemId, bool bExplicit)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Slots.IsValidIndex(SlotIndex)) return false;
	const TArray<FAZ_QuickSlotBinding> Previous = Bindings.Slots;
	Bindings.Slots.SetNum(Slots.Num());
	// Moving an explicit assignment also leaves the previous slot explicitly empty.
	for (int32 Index = 0; Index < Bindings.Slots.Num(); ++Index)
	{
		if (Index != SlotIndex && ItemId.IsValid() && Bindings.Slots[Index].ItemId == ItemId)
		{
			Bindings.Slots[Index].ItemId.Invalidate();
			Bindings.Slots[Index].bExplicit |= bExplicit;
		}
	}
	Bindings.Slots[SlotIndex].ItemId = ItemId;
	Bindings.Slots[SlotIndex].bExplicit = bExplicit;
	PruneReadyItem();
	PublishBindingsIfChanged(Previous);
	return true;
}

void UAZ_QuickBarComponent::PublishBindingsIfChanged(const TArray<FAZ_QuickSlotBinding>& Previous)
{
	if (Previous == Bindings.Slots) return;
	++Bindings.Revision;
	OnBindingsChanged.Broadcast();
	GetOwner()->ForceNetUpdate();
}

void UAZ_QuickBarComponent::OnRep_Bindings()
{
	OnBindingsChanged.Broadcast();
}

void UAZ_QuickBarComponent::SetReadyItemId(const FGuid& ItemId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || ReadyItemId == ItemId) return;
	ReadyItemId = ItemId;
	OnReadyItemChanged.Broadcast();
	GetOwner()->ForceNetUpdate();
}

void UAZ_QuickBarComponent::OnRep_ReadyItemId()
{
	OnReadyItemChanged.Broadcast();
}

void UAZ_QuickBarComponent::PruneReadyItem()
{
	if (ReadyItemId.IsValid() && !GetReadyItem()) SetReadyItemId(FGuid());
}

bool UAZ_QuickBarComponent::CanReadyConsumable() const
{
	const AAZ_PlayerController* Controller = Cast<AAZ_PlayerController>(GetOwner());
	if (!Controller || !IsValid(Controller->GetPawn()) || Controller->GetPawn()->IsActorBeingDestroyed()
		|| Controller->IsInventoryMenuOpen()) return false;
	const UAbilitySystemComponent* ASC = Controller->GetAbilitySystemComponent();
	const auto& Tags = FAZ_GameplayTags::Get();
	const FGameplayTagContainer Blocked = FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{Tags.Character_Dead,
		Tags.Character_Dying, Tags.Character_Stunned, Tags.State_Grabbed, Tags.State_Combat_Grabbing,
		Tags.State_Combat_Staggered, Tags.State_Combat_StruckPair});
	if (!ASC || ASC->HasAnyMatchingGameplayTags(Blocked)) return false;
	const UAZ_VitalsAttributeSet* Vitals = ASC->GetSet<UAZ_VitalsAttributeSet>();
	return Vitals && FMath::IsFinite(Vitals->GetHealth()) && Vitals->GetHealth() > 0.f;
}

void UAZ_QuickBarComponent::HandleEquipmentChanged()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		const UAZ_Inv_CommonUI_EquipmentComponent* Equipment = GetEquipment();
		const UAZ_Inv_CommonUI_InventoryItem* Item = Equipment ? Equipment->GetActiveItem() : nullptr;
		if (IsOwnedPhysicalWeapon(Item)) LastFightWeaponId = Item->GetInstanceId();
	}
	// A successfully committed equipment change wins over the separately readied
	// consumable. Merely previewing, assigning, or queueing a weapon does not.
	SetReadyItemId(FGuid());
}

void UAZ_QuickBarComponent::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	LastFightWeaponId.Invalidate();
	SetReadyItemId(FGuid());
}

void UAZ_QuickBarComponent::BindEquipmentEvents()
{
	UAZ_Inv_CommonUI_EquipmentComponent* Equipment = GetEquipment();
	if (BoundEquipment == Equipment) return;
	if (BoundEquipment.IsValid())
	{
		BoundEquipment->OnTrackedRequestResult.Remove(EquipmentRequestHandle);
		BoundEquipment->OnEquipmentChanged.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
	}
	BoundEquipment = Equipment;
	EquipmentRequestHandle.Reset();
	if (Equipment)
	{
		EquipmentRequestHandle = Equipment->OnTrackedRequestResult.AddUObject(this, &ThisClass::HandleEquipmentRequestResult);
		Equipment->OnEquipmentChanged.AddUniqueDynamic(this, &ThisClass::HandleEquipmentChanged);
	}
}

bool UAZ_QuickBarComponent::BeginRequest(int32 SlotIndex, const FGuid& ItemId, const FGuid& RequestId)
{
	if (!RequestId.IsValid())
	{
		FAZ_QuickBarRequestResult Result;
		Result.SlotIndex = SlotIndex;
		Result.ItemId = ItemId;
		Result.BindingRevision = Bindings.Revision;
		Result.Reason = NSLOCTEXT("AZQuickBar", "InvalidRequest", "This selection request is invalid.");
		SendRequestResult(Result);
		return false;
	}
	if (const FAZ_QuickBarRequestResult* Existing = RequestReceipts.Find(RequestId))
	{
		const FAZ_QuickBarRequestResult Receipt = *Existing;
		SendRequestResult(Receipt);
		return false;
	}
	// Keep receipts bounded without evicting a still-deferred equipment request.
	while (ReceiptOrder.Num() >= 128)
	{
		const int32 Evict = ReceiptOrder.IndexOfByPredicate([this](const FGuid& Id)
		{
			const FAZ_QuickBarRequestResult* Receipt = RequestReceipts.Find(Id);
			return !Receipt || Receipt->Outcome != EAZ_QuickBarRequestOutcome::Deferred;
		});
		if (Evict == INDEX_NONE) break;
		RequestReceipts.Remove(ReceiptOrder[Evict]);
		ReceiptOrder.RemoveAt(Evict);
	}
	FAZ_QuickBarRequestResult& Result = RequestReceipts.Add(RequestId);
	Result.RequestId = RequestId;
	Result.SlotIndex = SlotIndex;
	Result.ItemId = ItemId;
	Result.BindingRevision = Bindings.Revision;
	ReceiptOrder.Add(RequestId);
	return true;
}

void UAZ_QuickBarComponent::CompleteRequest(const FGuid& RequestId, EAZ_QuickBarRequestOutcome Outcome, const FText& Reason)
{
	FAZ_QuickBarRequestResult* Result = RequestReceipts.Find(RequestId);
	if (!Result) return;
	Result->Outcome = Outcome;
	Result->BindingRevision = Bindings.Revision;
	Result->Reason = Reason;
	// A listen host may synchronously submit another request from its UI delegate.
	const FAZ_QuickBarRequestResult Receipt = *Result;
	SendRequestResult(Receipt);
}

void UAZ_QuickBarComponent::SendRequestResult(const FAZ_QuickBarRequestResult& Result)
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (Controller && Controller->IsLocalController()) OnRequestResult.Broadcast(Result);
	else if (GetOwner() && GetOwner()->HasAuthority()) Client_RequestResult(Result);
}

void UAZ_QuickBarComponent::Client_RequestResult_Implementation(const FAZ_QuickBarRequestResult& Result)
{
	OnRequestResult.Broadcast(Result);
}

void UAZ_QuickBarComponent::HandleEquipmentRequestResult(const FGuid& RequestId, EAZ_EquipmentRequestOutcome Outcome, const FText& Reason)
{
	// Selecting the already-active weapon is an idempotent success with no
	// OnEquipmentChanged event; it must still end consumable readiness.
	if (Outcome == EAZ_EquipmentRequestOutcome::Activated) SetReadyItemId(FGuid());
	EAZ_QuickBarRequestOutcome Result = EAZ_QuickBarRequestOutcome::Rejected;
	switch (Outcome)
	{
	case EAZ_EquipmentRequestOutcome::Activated: Result = EAZ_QuickBarRequestOutcome::Activated; break;
	case EAZ_EquipmentRequestOutcome::Deferred: Result = EAZ_QuickBarRequestOutcome::Deferred; break;
	case EAZ_EquipmentRequestOutcome::Superseded: Result = EAZ_QuickBarRequestOutcome::Superseded; break;
	default: break;
	}
	CompleteRequest(RequestId, Result, Reason);
}

void UAZ_QuickBarComponent::RequestAssignItem(int32 SlotIndex, FGuid CandidateItemId, int64 ExpectedRevision, FGuid RequestId)
{
	if (!GetOwner()) return;
	if (GetOwner()->HasAuthority()) AssignItemInternal(SlotIndex, CandidateItemId, ExpectedRevision, RequestId);
	else Server_RequestAssignItem(SlotIndex, CandidateItemId, ExpectedRevision, RequestId);
}

void UAZ_QuickBarComponent::Server_RequestAssignItem_Implementation(int32 SlotIndex, FGuid CandidateItemId, int64 ExpectedRevision, FGuid RequestId)
{
	AssignItemInternal(SlotIndex, CandidateItemId, ExpectedRevision, RequestId);
}

bool UAZ_QuickBarComponent::AssignItemInternal(int32 SlotIndex, const FGuid& CandidateItemId, int64 ExpectedRevision, const FGuid& RequestId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !BeginRequest(SlotIndex, CandidateItemId, RequestId)) return false;
	if (ExpectedRevision != Bindings.Revision)
	{
		CompleteRequest(RequestId, EAZ_QuickBarRequestOutcome::Rejected,
			NSLOCTEXT("AZQuickBar", "BindingsChanged", "Your quick slots changed. Choose the item again."));
		return false;
	}
	UAZ_Inv_CommonUI_InventoryComponent* Inventory = GetInventory();
	UAZ_Inv_CommonUI_InventoryItem* Item = Inventory ? Inventory->FindItemById(CandidateItemId) : nullptr;
	if (!CanBindItem(SlotIndex, Item))
	{
		CompleteRequest(RequestId, EAZ_QuickBarRequestOutcome::Rejected,
			NSLOCTEXT("AZQuickBar", "ItemUnavailable", "This item is no longer available for this slot."));
		return false;
	}
	SetBinding(SlotIndex, CandidateItemId, true);
	CompleteRequest(RequestId, EAZ_QuickBarRequestOutcome::Assigned);
	return true;
}

void UAZ_QuickBarComponent::RequestToggleCombatMode(int64 ExpectedSelectionGeneration, FGuid RequestId)
{
	if (!GetOwner()) return;
	if (GetOwner()->HasAuthority()) ToggleCombatModeInternal(ExpectedSelectionGeneration, RequestId);
	else Server_RequestToggleCombatMode(ExpectedSelectionGeneration, RequestId);
}

void UAZ_QuickBarComponent::Server_RequestToggleCombatMode_Implementation(int64 ExpectedSelectionGeneration, FGuid RequestId)
{
	ToggleCombatModeInternal(ExpectedSelectionGeneration, RequestId);
}

void UAZ_QuickBarComponent::ToggleCombatModeInternal(int64 ExpectedSelectionGeneration, const FGuid& RequestId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !BeginRequest(0, FGuid(), RequestId)) return;
	BindEquipmentEvents();
	if (!BoundEquipment.IsValid() || ExpectedSelectionGeneration < 0
		|| ExpectedSelectionGeneration != GetCombatModeRevision())
	{
		CompleteRequest(RequestId, EAZ_QuickBarRequestOutcome::Rejected,
			NSLOCTEXT("AZQuickBar", "CombatModeChanged", "Equipment changed. Try switching mode again."));
		return;
	}
	if (IsFightMode())
	{
		// Preserve the last successfully committed weapon, including one equipped
		// directly from inventory without assigning any manual quick slot.
		const UAZ_Inv_CommonUI_InventoryItem* Item = BoundEquipment->GetActiveItem();
		if (IsOwnedPhysicalWeapon(Item)) LastFightWeaponId = Item->GetInstanceId();
		BoundEquipment->RequestTrackedSelection(nullptr, INDEX_NONE, RequestId);
		return;
	}
	UAZ_Inv_CommonUI_InventoryItem* Item = bRestoreLastWeaponOnFight ? GetRememberedFightWeapon() : nullptr;
	if (!Item)
	{
		const FAZ_QuickSlot* Fists = GetSlotDefinition(0);
		if (!Fists || !Fists->bEnabled || Fists->bInventoryBacked || Fists->WeaponTag != FAZ_GameplayTags::Get().Weapon_Fist)
		{
			CompleteRequest(RequestId, EAZ_QuickBarRequestOutcome::Rejected,
				NSLOCTEXT("AZQuickBar", "FistsUnavailable", "Fight mode is unavailable."));
			return;
		}
	}
	if (FAZ_QuickBarRequestResult* Receipt = RequestReceipts.Find(RequestId))
	{
		Receipt->ItemId = Item ? Item->GetInstanceId() : FGuid();
	}
	BoundEquipment->RequestTrackedSelection(Item, Item ? INDEX_NONE : 0, RequestId);
}

void UAZ_QuickBarComponent::RequestActivateSlot(int32 SlotIndex, FGuid ExpectedBoundItemId, int64 ExpectedRevision, FGuid RequestId)
{
	if (!GetOwner()) return;
	if (GetOwner()->HasAuthority()) ActivateSlotInternal(SlotIndex, ExpectedBoundItemId, ExpectedRevision, RequestId);
	else Server_RequestActivateSlot(SlotIndex, ExpectedBoundItemId, ExpectedRevision, RequestId);
}

void UAZ_QuickBarComponent::Server_RequestActivateSlot_Implementation(int32 SlotIndex, FGuid ExpectedBoundItemId, int64 ExpectedRevision, FGuid RequestId)
{
	ActivateSlotInternal(SlotIndex, ExpectedBoundItemId, ExpectedRevision, RequestId);
}

void UAZ_QuickBarComponent::ActivateSlotInternal(int32 SlotIndex, const FGuid& ExpectedBoundItemId, int64 ExpectedRevision, const FGuid& RequestId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !BeginRequest(SlotIndex, ExpectedBoundItemId, RequestId)) return;
	BindEquipmentEvents();
	const FAZ_QuickSlot* Slot = GetSlotDefinition(SlotIndex);
	if (!Slot || !Slot->bEnabled || !BoundEquipment.IsValid())
	{
		CompleteRequest(RequestId, EAZ_QuickBarRequestOutcome::Rejected,
			NSLOCTEXT("AZQuickBar", "SlotUnavailable", "This quick slot is unavailable."));
		return;
	}
	if (ExpectedRevision != Bindings.Revision || GetBoundItemId(SlotIndex) != ExpectedBoundItemId)
	{
		CompleteRequest(RequestId, EAZ_QuickBarRequestOutcome::Rejected,
			NSLOCTEXT("AZQuickBar", "AssignmentChanged", "This quick slot's assignment changed."));
		return;
	}
	UAZ_Inv_CommonUI_InventoryItem* Item = Slot->bInventoryBacked ? GetBoundItem(SlotIndex) : nullptr;
	if ((Slot->bInventoryBacked && (!ExpectedBoundItemId.IsValid() || !Item))
		|| (!Slot->bInventoryBacked && (ExpectedBoundItemId.IsValid() || !Slot->WeaponTag.IsValid())))
	{
		CompleteRequest(RequestId, EAZ_QuickBarRequestOutcome::Rejected,
			NSLOCTEXT("AZQuickBar", "AssignmentUnavailable", "No available item is assigned to this slot."));
		return;
	}
	if (Item && Item->IsConsumable())
	{
		if (!CanReadyConsumable())
		{
			CompleteRequest(RequestId, EAZ_QuickBarRequestOutcome::Rejected,
				NSLOCTEXT("AZQuickBar", "CannotReady", "This item cannot be selected right now."));
			return;
		}
		// A newer consumable selection supersedes a queued weapon change without
		// holstering the current weapon, granting abilities, or consuming the item.
		BoundEquipment->CancelPendingSelectionRequest();
		SetReadyItemId(Item->GetInstanceId());
		CompleteRequest(RequestId, EAZ_QuickBarRequestOutcome::Activated);
		return;
	}
	BoundEquipment->RequestTrackedSelection(Item, Slot->bInventoryBacked ? INDEX_NONE : SlotIndex, RequestId);
}

void UAZ_QuickBarComponent::BindSelectedItem(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	// Equipping through inventory changes equipment only. Every quick-slot binding
	// is chosen explicitly by the player, including the initial empty loadout.
}

void UAZ_QuickBarComponent::Select(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex) || !GetOwner()) return;
	if (GetOwner()->HasAuthority()) SelectInternal(SlotIndex);
	else Server_Select(SlotIndex);
}

void UAZ_QuickBarComponent::Server_Select_Implementation(int32 SlotIndex)
{
	SelectInternal(SlotIndex);
}

void UAZ_QuickBarComponent::SelectInternal(int32 SlotIndex)
{
	const FAZ_QuickSlot* Slot = GetSlotDefinition(SlotIndex);
	UAZ_Inv_CommonUI_EquipmentComponent* Equipment = GetEquipment();
	if (!Slot || !Slot->bEnabled || !Equipment) return;
	if (UAZ_Inv_CommonUI_InventoryItem* Item = GetBoundItem(SlotIndex); Item && Item->IsConsumable())
	{
		ActivateSlotInternal(SlotIndex, Item->GetInstanceId(), Bindings.Revision, FGuid::NewGuid());
		return;
	}
	if (GetActiveSlotIndex() == SlotIndex)
	{
		Equipment->RequestUnequipItem(Equipment->GetActiveItem());
	}
	else if (Slot->bInventoryBacked)
	{
		Equipment->RequestEquipItem(GetBoundItem(SlotIndex));
	}
	else
	{
		Equipment->RequestEquipIntrinsic(SlotIndex);
	}
}

void UAZ_QuickBarComponent::Cycle(int32 Direction)
{
	if (Slots.IsEmpty()) return;
	const int32 ReadySlot = GetReadySlotIndex();
	const int32 Active = ReadySlot != INDEX_NONE ? ReadySlot : GetActiveSlotIndex();
	int32 Index = Active == INDEX_NONE ? (Direction > 0 ? -1 : 0) : Active;
	for (int32 Attempt = 0; Attempt < Slots.Num(); ++Attempt)
	{
		Index = (Index + Direction + Slots.Num()) % Slots.Num();
		if (Index != Active && Slots[Index].bEnabled && (!Slots[Index].bInventoryBacked || GetBoundItem(Index)))
		{
			Select(Index);
			return;
		}
	}
}

void UAZ_QuickBarComponent::CycleNext() { Cycle(1); }
void UAZ_QuickBarComponent::CyclePrev() { Cycle(-1); }

void UAZ_QuickBarComponent::OnInventoryChanged()
{
	UAZ_Inv_CommonUI_InventoryComponent* Inventory = GetInventory();
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Inventory) return;
	if (LastFightWeaponId.IsValid() && !GetRememberedFightWeapon()) LastFightWeaponId.Invalidate();
	const TArray<FAZ_QuickSlotBinding> Previous = Bindings.Slots;
	Bindings.Slots.SetNum(Slots.Num());
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (CanBindItem(Index, GetBoundItem(Index))) continue;
		Bindings.Slots[Index].ItemId.Invalidate();
	}
	PruneReadyItem();
	PublishBindingsIfChanged(Previous);
}

void UAZ_QuickBarComponent::BeginPlay()
{
	Super::BeginPlay();
	BindEquipmentEvents();
	if (APlayerController* Controller = Cast<APlayerController>(GetOwner()))
	{
		Controller->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
	}
	if (UAZ_Inv_CommonUI_InventoryComponent* Inventory = GetInventory())
	{
		Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::OnInventoryChanged);
		OnInventoryChanged();
	}
}

void UAZ_QuickBarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAZ_Inv_CommonUI_InventoryComponent* Inventory = GetInventory())
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::OnInventoryChanged);
	}
	if (BoundEquipment.IsValid())
	{
		BoundEquipment->OnTrackedRequestResult.Remove(EquipmentRequestHandle);
		BoundEquipment->OnEquipmentChanged.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
	}
	if (APlayerController* Controller = Cast<APlayerController>(GetOwner()))
	{
		Controller->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	}
	BoundEquipment.Reset();
	EquipmentRequestHandle.Reset();
	Super::EndPlay(EndPlayReason);
}
