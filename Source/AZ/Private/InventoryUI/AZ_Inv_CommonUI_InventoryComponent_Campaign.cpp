#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_InventorySnapshot.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "AbilitySystemBlueprintLibrary.h"

namespace
{
void SanitizeCampaignManifest(FAZ_Inv_CommonUI_ItemManifest& Manifest)
{
	if (auto* Fragment = Manifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>()) Fragment->ResetRuntimeState();
	if (auto* Fragment = Manifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_AbilityGrantFragment>()) Fragment->ResetRuntimeState();
}
}

bool UAZ_Inv_CommonUI_InventoryComponent::CanCaptureCampaignInventory(FString& OutError) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { OutError = TEXT("Inventory checkpoint requires authority."); return false; }
	if (bMagazineReloadMutation || bCampaignRestorePrepared || bCampaignRestoreCommitted ||
		MagazineReload.ReloadId.IsValid() || ThrowReservation.ThrowActionId.IsValid())
	{ OutError = TEXT("Inventory transaction is still active."); return false; }
	if (const auto* Equipment = GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>())
		if (!Equipment->CanCaptureCampaignState(OutError)) return false;
	return true;
}

bool UAZ_Inv_CommonUI_InventoryComponent::CaptureCampaignInventory(FAZ_InventorySnapshot& OutSnapshot, FString& OutError) const
{
	if (!CanCaptureCampaignInventory(OutError)) return false;
	FAZ_InventorySnapshot Result;
	const auto* Equipment = GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
	for (const auto* Item : GetItems())
	{
		auto& Record = Result.Items.AddDefaulted_GetRef();
		Record.Manifest = Item->GetItemManifest();
		Record.State = Item->GetInstanceState();
		Record.StackCount = Item->GetTotalStackCount();
		// Legacy ASC-backed ammunition is copied into the snapshot, not mutated on the live item.
		if (Equipment && Equipment->GetActiveItem() == Item)
			if (auto* Weapon = Record.Manifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_WeaponStateFragment>();
				Weapon && !Weapon->bUsesDetachableMagazines)
				if (auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner())) Weapon->SaveFromASC(ASC);
		SanitizeCampaignManifest(Record.Manifest);
	}
	Result.Placements = GridPlacements;
	if (!ValidateCampaignInventory(Result, OutError)) return false;
	OutSnapshot = MoveTemp(Result);
	return true;
}

bool UAZ_Inv_CommonUI_InventoryComponent::ValidateCampaignInventory(const FAZ_InventorySnapshot& Snapshot, FString& OutError) const
{
	const auto Fail = [&OutError](const TCHAR* Message) { OutError = Message; return false; };
	if (Snapshot.Items.Num() > 10000 || Snapshot.Placements.Num() > 10000) return Fail(TEXT("Inventory snapshot exceeds supported size."));
	TMap<FGuid, const FAZ_InventoryPickupRecord*> Items;
	for (const auto& Record : Snapshot.Items)
	{
		if (!Record.State.InstanceId.IsValid() || Items.Contains(Record.State.InstanceId) || Record.StackCount <= 0 ||
			!Record.Manifest.GetItemTypeTag().IsValid()) return Fail(TEXT("Invalid or duplicate inventory item identity."));
		if (Record.State.Location != EAZ_InventoryItemLocation::Backpack && Record.State.Location != EAZ_InventoryItemLocation::WeaponMagazine)
			return Fail(TEXT("Inventory snapshot contains a world item."));
		if (!Record.Manifest.IsStackable() && Record.StackCount != 1) return Fail(TEXT("Nonstackable item count is invalid."));
		if (Record.State.Location == EAZ_InventoryItemLocation::Backpack && Record.State.ParentItemId.IsValid())
			return Fail(TEXT("Backpack item has a parent."));
		if (const auto* Magazine = Record.Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_MagazineFragment>(); Magazine &&
			(Magazine->Capacity <= 0 || Magazine->MagazineFamily.IsNone() || Record.State.CurrentRounds < 0 ||
			 Record.State.CurrentRounds > Magazine->Capacity || Record.State.AmmoRevision < 0))
			return Fail(TEXT("Invalid magazine state."));
		if (const auto* Weapon = Record.Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>(); Weapon &&
			Weapon->bUsesDetachableMagazines && (!Weapon->IsFireModeSupported(Record.State.SelectedFireMode) || Record.State.FireModeRevision < 0))
			return Fail(TEXT("Invalid weapon fire mode."));
		Items.Add(Record.State.InstanceId, &Record);
	}
	for (const auto& Record : Snapshot.Items)
	{
		if (Record.State.Location == EAZ_InventoryItemLocation::WeaponMagazine)
		{
			const auto* const* Parent = Items.Find(Record.State.ParentItemId);
			const auto* Magazine = Record.Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_MagazineFragment>();
			const auto* Weapon = Parent ? (*Parent)->Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>() : nullptr;
			if (!Parent || !Magazine || !Weapon || !Weapon->bUsesDetachableMagazines || Record.StackCount != 1 ||
				(*Parent)->State.Location != EAZ_InventoryItemLocation::Backpack || (*Parent)->State.InsertedMagazineId != Record.State.InstanceId ||
				Record.State.InsertedMagazineId.IsValid() || Weapon->MagazineFamily != Magazine->MagazineFamily)
				return Fail(TEXT("Invalid inserted-magazine relationship."));
		}
		if (Record.State.InsertedMagazineId.IsValid())
		{
			const auto* const* Child = Items.Find(Record.State.InsertedMagazineId);
			if (!Child || (*Child)->State.ParentItemId != Record.State.InstanceId ||
				(*Child)->State.Location != EAZ_InventoryItemLocation::WeaponMagazine)
				return Fail(TEXT("Weapon references a missing or mismatched magazine."));
		}
	}
	TMap<FGuid, int64> PlacedCounts;
	TMap<EInv_ItemCategory, TSet<int32>> Occupied;
	for (const auto& Placement : Snapshot.Placements)
	{
		const auto* const* Found = Items.Find(Placement.ItemId);
		if (!Found || (*Found)->State.Location != EAZ_InventoryItemLocation::Backpack || Placement.StackCount <= 0)
			return Fail(TEXT("Placement references an invalid item."));
		const auto& Manifest = (*Found)->Manifest;
		const FIntPoint Dimensions = GetGridDimensions(Manifest.GetItemCategory());
		const auto* Grid = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_GridFragment>();
		const FIntPoint Size = Grid ? Grid->GetGridSize() : FIntPoint(1, 1);
		const auto* Stack = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_Stackable_Fragment>();
		const int32 Limit = Manifest.IsStackable() && Stack ? FMath::Max(1, Stack->GetMaxStackSize()) : 1;
		if (Dimensions.X <= 0 || Dimensions.Y <= 0 || Size.X <= 0 || Size.Y <= 0 ||
			Placement.GridIndex < 0 || Placement.GridIndex >= int64(Dimensions.X) * Dimensions.Y || Placement.StackCount > Limit)
			return Fail(TEXT("Placement is outside current inventory capacity."));
		const FIntPoint Start(Placement.GridIndex % Dimensions.X, Placement.GridIndex / Dimensions.X);
		if (Start.X + Size.X > Dimensions.X || Start.Y + Size.Y > Dimensions.Y) return Fail(TEXT("Item does not fit saved grid position."));
		auto& Cells = Occupied.FindOrAdd(Manifest.GetItemCategory());
		for (int32 Y = Start.Y; Y < Start.Y + Size.Y; ++Y)
			for (int32 X = Start.X; X < Start.X + Size.X; ++X)
			{
				const int32 Cell = Y * Dimensions.X + X;
				if (Cells.Contains(Cell)) return Fail(TEXT("Saved inventory placements overlap."));
				Cells.Add(Cell);
			}
		PlacedCounts.FindOrAdd(Placement.ItemId) += Placement.StackCount;
	}
	for (const auto& Record : Snapshot.Items)
		if (PlacedCounts.FindRef(Record.State.InstanceId) !=
			(Record.State.Location == EAZ_InventoryItemLocation::Backpack ? Record.StackCount : 0))
			return Fail(TEXT("Placement counts do not match owned quantities."));
	return true;
}

bool UAZ_Inv_CommonUI_InventoryComponent::PrepareCampaignInventoryRestore(const FAZ_InventorySnapshot& Snapshot, FString& OutError)
{
	if (!CanCaptureCampaignInventory(OutError) || !ValidateCampaignInventory(Snapshot, OutError)) return false;
	bMagazineReloadMutation = true;
	bCampaignRestorePrepared = true;
	CampaignOriginalPlacements = GridPlacements;
	CampaignStagedPlacements = Snapshot.Placements;
	for (const auto& Record : Snapshot.Items)
	{
		// Manifest::Manifest() rerolls randomized fragments. Restore exact saved data instead.
		auto* Item = NewObject<UAZ_Inv_CommonUI_InventoryItem>(GetOwner());
		auto Manifest = Record.Manifest;
		SanitizeCampaignManifest(Manifest);
		Item->SetItemManifest(Manifest);
		Item->InitializeInstance(Record.State, Record.StackCount);
		CampaignStagedItems.Add(Item);
	}
	return true;
}

void UAZ_Inv_CommonUI_InventoryComponent::CommitCampaignInventoryRestore()
{
	check(bCampaignRestorePrepared && !bCampaignRestoreCommitted && GetOwner()->HasAuthority());
	for (auto* Item : GetItems()) { CampaignRemovedItems.Add(Item); RemoveOwnedItem(Item); }
	for (const auto& Item : CampaignStagedItems) InventoryList.AddInventoryItem(Item.Get());
	GridPlacements = MoveTemp(CampaignStagedPlacements);
	WeaponNextShotTimes.Reset();
	bCampaignRestoreCommitted = true;
}

UAZ_Inv_CommonUI_InventoryItem* UAZ_Inv_CommonUI_InventoryComponent::FindCampaignStagedItem(const FGuid& ItemId) const
{
	for (const auto& Item : CampaignStagedItems) if (Item && Item->GetInstanceId() == ItemId) return Item.Get();
	return nullptr;
}

void UAZ_Inv_CommonUI_InventoryComponent::CancelCampaignInventoryRestore()
{
	if (!bCampaignRestorePrepared) return;
	if (bCampaignRestoreCommitted)
	{
		for (auto* Item : GetItems()) RemoveOwnedItem(Item);
		for (const auto& Item : CampaignRemovedItems) InventoryList.AddInventoryItem(Item.Get());
		GridPlacements = CampaignOriginalPlacements;
	}
	CampaignStagedItems.Reset();
	CampaignStagedPlacements.Reset();
	CampaignOriginalPlacements.Reset();
	CampaignRemovedItems.Reset();
	bCampaignRestorePrepared = bCampaignRestoreCommitted = false;
	bMagazineReloadMutation = false;
}

void UAZ_Inv_CommonUI_InventoryComponent::PublishCampaignInventoryRestore(bool bNotify)
{
	check(bCampaignRestoreCommitted);
	const auto Removed = MoveTemp(CampaignRemovedItems);
	const auto Added = MoveTemp(CampaignStagedItems);
	CampaignOriginalPlacements.Reset();
	bCampaignRestorePrepared = bCampaignRestoreCommitted = bMagazineReloadMutation = false;
	if (!bNotify) return;
	for (const auto& Item : Removed) OnItemRemoved.Broadcast(Item.Get());
	for (const auto& Item : Added) OnItemAdded.Broadcast(Item.Get());
	NotifyInventoryChanged();
}

bool UAZ_Inv_CommonUI_InventoryComponent::TryConsumeForQuest(FGameplayTag ItemType, int32 Amount,
	TFunctionRef<bool()> CommitProgressSilently, FString& OutError)
{
	if (!ItemType.IsValid() || Amount <= 0) { OutError = TEXT("Invalid delivery quantity or item type."); return false; }
	if (!CanCaptureCampaignInventory(OutError)) return false;
	const auto* Equipment = GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
	TArray<UAZ_Inv_CommonUI_InventoryItem*> Candidates = GetItems();
	Candidates.Sort([](const auto& A, const auto& B) { return A.GetInstanceId().ToString() < B.GetInstanceId().ToString(); });
	TArray<TPair<UAZ_Inv_CommonUI_InventoryItem*, int32>> Plan;
	int32 Remaining = Amount;
	for (auto* Item : Candidates)
	{
		if (Item->GetLocation() != EAZ_InventoryItemLocation::Backpack || Item->GetParentItemId().IsValid() ||
			!Item->GetItemManifest().GetItemTypeTag().MatchesTag(ItemType)) continue;
		// Do not destroy grants/presentation or inserted ownership inside a delivery.
		if (Item->GetInsertedMagazineId().IsValid() || (Equipment && Equipment->GetActiveItem() == Item)) continue;
		const int32 Consume = FMath::Min(Remaining, Item->GetTotalStackCount());
		int64 Placed = 0;
		for (const auto& P : GridPlacements) if (P.ItemId == Item->GetInstanceId()) Placed += P.StackCount;
		if (Consume <= 0 || Placed != Item->GetTotalStackCount()) { OutError = TEXT("Delivery inventory placement is inconsistent."); return false; }
		Plan.Emplace(Item, Consume);
		Remaining -= Consume;
		if (Remaining == 0) break;
	}
	if (Remaining != 0) { OutError = TEXT("Not enough available items. Unequip or unload items before delivery."); return false; }
	TGuardValue<bool> Guard(bMagazineReloadMutation, true);
	if (!CommitProgressSilently()) { OutError = TEXT("Objective changed before delivery could commit."); return false; }
	TArray<UAZ_Inv_CommonUI_InventoryItem*> Removed;
	for (const auto& [Item, Count] : Plan)
	{
		if (Count == Item->GetTotalStackCount()) { RemoveOwnedItem(Item); Removed.Add(Item); }
		else { const bool bRemoved = RemoveStackPlacements(Item->GetInstanceId(), Count); check(bRemoved); Item->SetTotalStackCount(Item->GetTotalStackCount() - Count); }
	}
	for (auto* Item : Removed) OnItemRemoved.Broadcast(Item);
	NotifyInventoryChanged();
	return true;
}
