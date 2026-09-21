#include "Inventory/AZ_QuickBarComponent.h"
#include "InventoryUI/AZ_InventorySnapshot.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"

bool UAZ_QuickBarComponent::CanCaptureCampaignState(FString& Error) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bCampaignRestoring)
	{ Error = TEXT("Quick bar is unavailable for checkpointing."); return false; }
	for (const auto& Pair : RequestReceipts)
		if (Pair.Value.Outcome == EAZ_QuickBarRequestOutcome::Deferred)
		{ Error = TEXT("A quick-bar request is pending."); return false; }
	return true;
}

bool UAZ_QuickBarComponent::CaptureCampaignState(FAZ_CampaignQuickBarState& State, FString& Error) const
{
	if (!CanCaptureCampaignState(Error)) return false;
	State = FAZ_CampaignQuickBarState();
	for (int32 I = 0; I < Slots.Num(); ++I) { State.ItemIds.Add(GetBoundItemId(I)); State.ExplicitBindings.Add(IsBindingExplicit(I)); }
	State.ReadyItemId = ReadyItemId; State.LastFightWeaponId = LastFightWeaponId;
	return true;
}

bool UAZ_QuickBarComponent::ValidateCampaignState(const FAZ_CampaignQuickBarState& State, const FAZ_InventorySnapshot& Inventory, FString& Error) const
{
	if (State.ItemIds.Num() != Slots.Num() || State.ExplicitBindings.Num() != Slots.Num())
	{ Error = TEXT("Saved quick-bar layout does not match this build."); return false; }
	TSet<FGuid> Seen;
	for (int32 I = 0; I < Slots.Num(); ++I)
	{
		const FGuid Id = State.ItemIds[I];
		if (!Id.IsValid()) continue;
		const auto* Item = Inventory.Items.FindByPredicate([&](const auto& R) { return R.State.InstanceId == Id; });
		if (!Slots[I].bEnabled || !Slots[I].bInventoryBacked || Seen.Contains(Id) || !Item ||
			Item->State.Location != EAZ_InventoryItemLocation::Backpack || Item->State.ParentItemId.IsValid())
		{ Error = TEXT("Invalid saved quick-slot binding."); return false; }
		const auto& Manifest = Item->Manifest;
		const bool Weapon = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>() != nullptr;
		const bool Consumable = Manifest.GetItemCategory() == EInv_ItemCategory::Consumable;
		const bool Throwable = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_ThrowableFragment>() != nullptr;
		if (!Consumable && !Throwable && !(Weapon && Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_EquipmentFragment>()))
		{ Error = TEXT("Saved item is incompatible with quick slots."); return false; }
		if (Id == State.ReadyItemId && !Consumable && !(Throwable && !Weapon))
		{ Error = TEXT("Saved ready item cannot be held as a consumable."); return false; }
		Seen.Add(Id);
	}
	if (State.ReadyItemId.IsValid() && !Seen.Contains(State.ReadyItemId))
	{ Error = TEXT("Saved ready item has no binding."); return false; }
	if (State.LastFightWeaponId.IsValid())
	{
		const auto* Item = Inventory.Items.FindByPredicate([&](const auto& R) { return R.State.InstanceId == State.LastFightWeaponId; });
		if (!Item || !Item->Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>())
		{ Error = TEXT("Saved remembered weapon is unavailable."); return false; }
	}
	return true;
}

void UAZ_QuickBarComponent::BeginCampaignRestore() { check(!bCampaignRestoring); bCampaignRestoring = true; }
void UAZ_QuickBarComponent::CancelCampaignRestore() { bCampaignRestoring = false; }

void UAZ_QuickBarComponent::RestoreCampaignState(const FAZ_CampaignQuickBarState& State)
{
	check(bCampaignRestoring && GetOwner()->HasAuthority());
	Bindings.Slots.SetNum(State.ItemIds.Num());
	for (int32 I = 0; I < State.ItemIds.Num(); ++I)
	{ Bindings.Slots[I].ItemId = State.ItemIds[I]; Bindings.Slots[I].bExplicit = State.ExplicitBindings[I]; }
	++Bindings.Revision; // Never accept a pre-load request by restoring its old revision.
	ReadyItemId = State.ReadyItemId; LastFightWeaponId = State.LastFightWeaponId;
	RequestReceipts.Reset(); ReceiptOrder.Reset();
}

void UAZ_QuickBarComponent::PublishCampaignRestore(bool bNotify)
{
	check(bCampaignRestoring);
	bCampaignRestoring = false;
	if (!bNotify) return;
	OnBindingsChanged.Broadcast(); OnReadyItemChanged.Broadcast(); GetOwner()->ForceNetUpdate();
}
