#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h"
#include "InventoryUI/Items/Manifest/AZ_Inv_CommonUI_ItemManifest.h"
#include "Net/UnrealNetwork.h"
#include "Player/AZ_PlayerController.h"
#include "Weapon/AZ_Weapon.h"

DEFINE_LOG_CATEGORY_STATIC(LogAZInventory, Log, All);

namespace
{
FIntPoint ItemDimensions(const FAZ_Inv_CommonUI_ItemManifest& Manifest)
{
    const auto* Grid = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_GridFragment>();
    return Grid ? Grid->GetGridSize() : FIntPoint(1, 1);
}

bool MagazineIdentityLess(const FGuid& Left, const FGuid& Right)
{
    if (Left.A != Right.A) return Left.A < Right.A;
    if (Left.B != Right.B) return Left.B < Right.B;
    if (Left.C != Right.C) return Left.C < Right.C;
    return Left.D < Right.D;
}

FAZ_InventoryPickupRecord MakePickupRecord(const UAZ_Inv_CommonUI_InventoryItem* Item)
{
    FAZ_InventoryPickupRecord Record;
    Record.Manifest = Item->GetItemManifest();
    Record.State = Item->GetInstanceState();
    Record.StackCount = Item->GetTotalStackCount();
    if (auto* Equipment = Record.Manifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>()) Equipment->ResetRuntimeState();
    if (auto* Grants = Record.Manifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_AbilityGrantFragment>()) Grants->ResetRuntimeState();
    return Record;
}
}

UAZ_Inv_CommonUI_InventoryComponent::UAZ_Inv_CommonUI_InventoryComponent() : InventoryList(this)
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    bReplicateUsingRegisteredSubObjectList = true;
}

void UAZ_Inv_CommonUI_InventoryComponent::BeginPlay()
{
    Super::BeginPlay();
    ConstructInventory();
}

void UAZ_Inv_CommonUI_InventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    MagazineReload = FMagazineReloadReservation();
    Super::EndPlay(EndPlayReason);
}

void UAZ_Inv_CommonUI_InventoryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UAZ_Inv_CommonUI_InventoryComponent::ReadyForReplication()
{
    Super::ReadyForReplication();
    for (auto* Item : GetItems()) AddRepSubObjects(Item);
}

void UAZ_Inv_CommonUI_InventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, InventoryList);
    DOREPLIFETIME(ThisClass, GridPlacements);
}

bool UAZ_Inv_CommonUI_InventoryComponent::ContainsItem(const UAZ_Inv_CommonUI_InventoryItem* Item) const
{
    return IsValid(Item) && GetItems().Contains(Item);
}

UAZ_Inv_CommonUI_InventoryItem* UAZ_Inv_CommonUI_InventoryComponent::FindItemById(const FGuid& ItemId) const
{
    if (!ItemId.IsValid()) return nullptr;
    for (auto* Item : GetItems()) if (Item->GetInstanceId() == ItemId) return Item;
    return nullptr;
}

FAZ_WeaponAmmoSnapshot UAZ_Inv_CommonUI_InventoryComponent::GetWeaponAmmoSnapshot(const FGuid& WeaponItemId) const
{
    FAZ_WeaponAmmoSnapshot Snapshot;
    Snapshot.WeaponItemId = WeaponItemId;
    const auto* WeaponItem = FindItemById(WeaponItemId);
    if (!WeaponItem || !WeaponItem->IsWeapon() || WeaponItem->IsMagazine() ||
        WeaponItem->GetLocation() != EAZ_InventoryItemLocation::Backpack || WeaponItem->GetParentItemId().IsValid() ||
        WeaponItem->GetTotalStackCount() != 1) return Snapshot;
    const auto* Weapon = WeaponItem->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
    if (!Weapon || !Weapon->bUsesDetachableMagazines || Weapon->MagazineFamily.IsNone()) return Snapshot;

    Snapshot.MagazineItemId = WeaponItem->GetInsertedMagazineId();
    // Retain a derived loaded-spare count for callers. The gameplay HUD displays
    // only the inserted magazine; empty magazines remain inventory items.
    for (const auto* Item : GetItems())
    {
        if (Item->GetLocation() != EAZ_InventoryItemLocation::Backpack || Item->GetParentItemId().IsValid() ||
            Item->GetInsertedMagazineId().IsValid() || Item->IsWeapon() || Item->GetTotalStackCount() != 1) continue;
        const auto* Magazine = Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_MagazineFragment>();
        if (Magazine && Magazine->MagazineFamily == Weapon->MagazineFamily && Magazine->Capacity > 0 &&
            Item->GetMagazineRounds() > 0 && Item->GetMagazineRounds() <= Magazine->Capacity &&
            Item->GetInstanceState().AmmoRevision >= 0) ++Snapshot.SpareMagazineCount;
    }
    if (!Snapshot.MagazineItemId.IsValid())
    {
        Snapshot.MagazineState = EAZ_WeaponMagazineState::NoMagazine;
        return Snapshot;
    }
    const auto* MagazineItem = FindItemById(Snapshot.MagazineItemId);
    if (!MagazineItem || MagazineItem == WeaponItem || MagazineItem->IsWeapon() ||
        MagazineItem->GetLocation() != EAZ_InventoryItemLocation::WeaponMagazine ||
        MagazineItem->GetParentItemId() != WeaponItemId || MagazineItem->GetInsertedMagazineId().IsValid() ||
        MagazineItem->GetTotalStackCount() != 1) return Snapshot;
    const auto* Magazine = MagazineItem->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_MagazineFragment>();
    if (!Magazine || Magazine->MagazineFamily != Weapon->MagazineFamily || Magazine->Capacity <= 0 ||
        MagazineItem->GetMagazineRounds() < 0 || MagazineItem->GetMagazineRounds() > Magazine->Capacity ||
        MagazineItem->GetInstanceState().AmmoRevision < 0) return Snapshot;

    Snapshot.Rounds = MagazineItem->GetMagazineRounds();
    Snapshot.Capacity = Magazine->Capacity;
    Snapshot.AmmoRevision = MagazineItem->GetInstanceState().AmmoRevision;
    Snapshot.MagazineState = Snapshot.Rounds > 0 ? EAZ_WeaponMagazineState::Loaded : EAZ_WeaponMagazineState::Empty;
    return Snapshot;
}

bool UAZ_Inv_CommonUI_InventoryComponent::TryConsumeWeaponRound(const UObject* WeaponSource, const FGuid& WeaponItemId,
    const FGuid& ExpectedMagazineId, int64 ExpectedAmmoRevision, uint32 ExpectedEquipmentGeneration,
    const FGuid& ShotId, FAZ_WeaponAmmoSnapshot& OutSnapshot)
{
    OutSnapshot = GetWeaponAmmoSnapshot(WeaponItemId);
    if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld() || bMagazineReloadMutation ||
        IsWeaponReloading(WeaponItemId) || !ShotId.IsValid() ||
        !ExpectedMagazineId.IsValid() || ExpectedAmmoRevision < 0 || ExpectedAmmoRevision == MAX_int64) return false;
    const auto* Equipment = GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
    auto* WeaponItem = FindItemById(WeaponItemId);
    if (!Equipment || !WeaponItem || Equipment->GetActiveItem() != WeaponItem ||
        Equipment->GetSelectionGeneration() != ExpectedEquipmentGeneration || !Equipment->IsActiveWeaponSource(WeaponSource)) return false;
    if (OutSnapshot.MagazineState != EAZ_WeaponMagazineState::Loaded ||
        OutSnapshot.MagazineItemId != ExpectedMagazineId || OutSnapshot.AmmoRevision != ExpectedAmmoRevision) return false;

    const auto* Weapon = WeaponItem->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
    if (!Weapon || !FMath::IsFinite(Weapon->FireRate) || Weapon->FireRate <= 0.f) return false;
    const double Now = GetWorld()->GetTimeSeconds();
    const double Interval = 1.0 / static_cast<double>(Weapon->FireRate);
    const double PreviousDue = GetWeaponNextAllowedFireTime(WeaponItemId);
    if (PreviousDue > Now + UE_KINDA_SMALL_NUMBER) return false;
    for (auto It = WeaponNextShotTimes.CreateIterator(); It; ++It)
    {
        if (It.Key() != WeaponItemId && It.Value() <= Now) It.RemoveCurrent();
    }

    auto* MagazineItem = FindItemById(ExpectedMagazineId);
    if (!MagazineItem) return false;
    // Finish both mutations and cadence bookkeeping before callbacks can submit the same request again.
    --MagazineItem->InstanceState.CurrentRounds;
    ++MagazineItem->InstanceState.AmmoRevision;
    // Preserve the cadence phase across normal frame quantization. A hitch/long
    // idle resets it instead of compressing missed shots into a catch-up burst.
    const double PhaseDue = PreviousDue + Interval;
    const double NextDue = PreviousDue > 0.0 && PhaseDue >= Now + Interval * 0.5
        ? PhaseDue : Now + Interval;
    WeaponNextShotTimes.Add(WeaponItemId, NextDue);
    // This accepted ammo transaction owns recoil growth too. Publish its complete
    // snapshot before listeners can cancel fire, change mode, or replace the actor.
    WeaponItem->RecordAcceptedFirearmShot(Now, Weapon->Recoil);
    OutSnapshot = GetWeaponAmmoSnapshot(WeaponItemId);
    UE_LOG(LogAZInventory, Log, TEXT("[Inventory] Shot debited shot=%s weapon=%s magazine=%s generation=%u revision=%lld rounds=%d/%d"),
        *ShotId.ToString(), *WeaponItemId.ToString(), *ExpectedMagazineId.ToString(), ExpectedEquipmentGeneration,
        OutSnapshot.AmmoRevision, OutSnapshot.Rounds, OutSnapshot.Capacity);
    NotifyInventoryChanged();
    return true;
}

double UAZ_Inv_CommonUI_InventoryComponent::GetWeaponNextAllowedFireTime(const FGuid& WeaponItemId) const
{
    const double* Due = WeaponNextShotTimes.Find(WeaponItemId);
    return Due ? *Due : 0.0;
}

bool UAZ_Inv_CommonUI_InventoryComponent::TrySetWeaponFireMode(const UObject* WeaponSource, const FGuid& WeaponItemId,
    uint32 ExpectedEquipmentGeneration, int64 ExpectedFireModeRevision, EAZ_FirearmFireMode NewMode)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || bMagazineReloadMutation || IsWeaponReloading(WeaponItemId) ||
        ExpectedFireModeRevision < 0 || ExpectedFireModeRevision == MAX_int64) return false;
    const auto* Equipment = GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
    auto* Item = FindItemById(WeaponItemId);
    if (!Equipment || !Item || !Item->IsInitialized() || !Item->IsWeapon() || Item->IsMagazine()
        || Equipment->GetActiveItem() != Item || Equipment->GetSelectionGeneration() != ExpectedEquipmentGeneration
        || !Equipment->IsActiveWeaponSource(WeaponSource) || Item->GetLocation() != EAZ_InventoryItemLocation::Backpack
        || Item->GetParentItemId().IsValid() || Item->GetTotalStackCount() != 1
        || Item->GetFireModeRevision() != ExpectedFireModeRevision || Item->GetSelectedFireMode() == NewMode) return false;
    const auto* Definition = Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
    if (!Definition || !Definition->IsFireModeSupported(Item->GetSelectedFireMode()) || !Definition->IsFireModeSupported(NewMode)) return false;

    Item->InstanceState.SelectedFireMode = NewMode;
    ++Item->InstanceState.FireModeRevision;
    UE_LOG(LogAZInventory, Log, TEXT("[Inventory] Fire mode weapon=%s generation=%u revision=%lld mode=%s"),
        *WeaponItemId.ToString(), ExpectedEquipmentGeneration, Item->GetFireModeRevision(),
        NewMode == EAZ_FirearmFireMode::Automatic ? TEXT("AUTO") : TEXT("SINGLE"));
    NotifyInventoryChanged();
    return true;
}

bool UAZ_Inv_CommonUI_InventoryComponent::BuildMagazineReloadReservation(const UObject* WeaponSource,
    const FGuid& WeaponItemId, uint32 ExpectedEquipmentGeneration, const FGuid& RequestedMagazineId, bool bSkipEmpty,
    FMagazineReloadReservation& OutReservation) const
{
    OutReservation = FMagazineReloadReservation();
    if (!GetOwner() || bMagazineReloadMutation || MagazineReload.ReloadId.IsValid() || !IsValid(WeaponSource)) return false;
    const auto* Equipment = GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
    const auto* WeaponItem = FindItemById(WeaponItemId);
    if (!Equipment || !WeaponItem || !WeaponItem->IsInitialized() || Equipment->GetActiveItem() != WeaponItem ||
        Equipment->GetSelectionGeneration() != ExpectedEquipmentGeneration || !Equipment->IsActiveWeaponSource(WeaponSource)) return false;
    const FAZ_WeaponAmmoSnapshot Snapshot = GetWeaponAmmoSnapshot(WeaponItemId);
    if (Snapshot.MagazineState == EAZ_WeaponMagazineState::Unavailable ||
        (Snapshot.MagazineState != EAZ_WeaponMagazineState::NoMagazine && Snapshot.AmmoRevision == MAX_int64) ||
        (RequestedMagazineId.IsValid() && RequestedMagazineId == Snapshot.MagazineItemId)) return false;
    const auto* Weapon = WeaponItem->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
    if (!Weapon) return false;
    const auto* Outgoing = FindItemById(Snapshot.MagazineItemId);
    if (Outgoing && GridPlacements.ContainsByPredicate([&](const auto& P) { return P.ItemId == Snapshot.MagazineItemId; })) return false;

    TArray<UAZ_Inv_CommonUI_InventoryItem*> Candidates;
    for (auto* Item : GetItems())
    {
        if (!IsValid(Item) || !Item->IsInitialized() || Item->IsWeapon() || Item->IsStackable() ||
            Item->GetLocation() != EAZ_InventoryItemLocation::Backpack || Item->GetParentItemId().IsValid() ||
            Item->GetInsertedMagazineId().IsValid() || Item->GetTotalStackCount() != 1 ||
            Item->GetMagazineRounds() < 0 || (bSkipEmpty && Item->GetMagazineRounds() == 0) ||
            (RequestedMagazineId.IsValid() && Item->GetInstanceId() != RequestedMagazineId) ||
            Item->GetInstanceState().AmmoRevision < 0 ||
            Item->GetInstanceState().AmmoRevision == MAX_int64) continue;
        const auto* Magazine = Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_MagazineFragment>();
        if (!Magazine || Magazine->MagazineFamily != Weapon->MagazineFamily || Magazine->Capacity <= 0 ||
            Item->GetMagazineRounds() > Magazine->Capacity) continue;
        Candidates.Add(Item);
    }
    Candidates.Sort([](const UAZ_Inv_CommonUI_InventoryItem& A, const UAZ_Inv_CommonUI_InventoryItem& B)
    {
        return MagazineIdentityLess(A.GetInstanceId(), B.GetInstanceId());
    });
    // The inserted identity is the ring cursor, so successful swaps advance it
    // without separate state. Pickup/drop and exact selection retain each identity's order.
    int32 StartIndex = 0;
    if (Snapshot.MagazineItemId.IsValid() && !RequestedMagazineId.IsValid())
    {
        const int32 FollowingIndex = Candidates.IndexOfByPredicate([&](const auto* Item)
        {
            return MagazineIdentityLess(Snapshot.MagazineItemId, Item->GetInstanceId());
        });
        if (FollowingIndex != INDEX_NONE) StartIndex = FollowingIndex;
    }
    for (int32 Offset = 0; Offset < Candidates.Num(); ++Offset)
    {
        const auto* Incoming = Candidates[(StartIndex + Offset) % Candidates.Num()];
        const FGuid IncomingId = Incoming->GetInstanceId();
        const auto* Placement = GridPlacements.FindByPredicate([&](const auto& P) { return P.ItemId == IncomingId; });
        if (!Placement || Placement->StackCount != 1) continue;
        auto Working = GridPlacements;
        if (Working.RemoveAll([&](const auto& P) { return P.ItemId == IncomingId; }) != 1 ||
            !IsPlacementFree(Incoming->GetItemManifest(), Placement->GridIndex, Working)) continue;
        FAZ_InventoryGridPlacement ReturnPlacement;
        if (Outgoing)
        {
            ReturnPlacement.ItemId = Outgoing->GetInstanceId();
            const auto& Manifest = Outgoing->GetItemManifest();
            if (IsPlacementFree(Manifest, Placement->GridIndex, Working)) ReturnPlacement.GridIndex = Placement->GridIndex;
            else
            {
                const FIntPoint Grid = GetGridDimensions(Manifest.GetItemCategory());
                for (int32 Index = 0; Index < Grid.X * Grid.Y; ++Index)
                {
                    if (IsPlacementFree(Manifest, Index, Working)) { ReturnPlacement.GridIndex = Index; break; }
                }
            }
            if (ReturnPlacement.GridIndex == INDEX_NONE) continue;
        }
        OutReservation.WeaponSource = WeaponSource;
        OutReservation.WeaponItemId = WeaponItemId;
        OutReservation.EquipmentGeneration = ExpectedEquipmentGeneration;
        OutReservation.IncomingMagazineId = IncomingId;
        OutReservation.OutgoingMagazineId = Snapshot.MagazineItemId;
        OutReservation.IncomingAmmoRevision = Incoming->GetInstanceState().AmmoRevision;
        OutReservation.OutgoingAmmoRevision = Snapshot.AmmoRevision;
        OutReservation.IncomingRounds = Incoming->GetMagazineRounds();
        OutReservation.OutgoingRounds = Snapshot.Rounds;
        OutReservation.IncomingPlacement = *Placement;
        OutReservation.ReturnPlacement = ReturnPlacement;
        OutReservation.bSkipEmpty = bSkipEmpty;
        return true;
    }
    return false;
}

bool UAZ_Inv_CommonUI_InventoryComponent::CanReloadMagazine(const UObject* WeaponSource,
    const FGuid& WeaponItemId, uint32 ExpectedEquipmentGeneration, const FGuid& RequestedMagazineId, bool bSkipEmpty) const
{
    FMagazineReloadReservation Reservation;
    return BuildMagazineReloadReservation(WeaponSource, WeaponItemId, ExpectedEquipmentGeneration,
        RequestedMagazineId, bSkipEmpty, Reservation);
}

bool UAZ_Inv_CommonUI_InventoryComponent::TryBeginMagazineReload(const UObject* WeaponSource,
    const FGuid& WeaponItemId, uint32 ExpectedEquipmentGeneration, const FGuid& ReloadId,
    const FGuid& RequestedMagazineId, bool bSkipEmpty)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !ReloadId.IsValid()) return false;
    FMagazineReloadReservation Reservation;
    if (!BuildMagazineReloadReservation(WeaponSource, WeaponItemId, ExpectedEquipmentGeneration,
        RequestedMagazineId, bSkipEmpty, Reservation)) return false;
    Reservation.ReloadId = ReloadId;
    MagazineReload = Reservation;
    UE_LOG(LogAZInventory, Log, TEXT("[Inventory] Reload reserved action=%s weapon=%s incoming=%s/%d outgoing=%s/%d return=%d generation=%u"),
        *ReloadId.ToString(), *WeaponItemId.ToString(), *Reservation.IncomingMagazineId.ToString(), Reservation.IncomingRounds,
        *Reservation.OutgoingMagazineId.ToString(), Reservation.OutgoingRounds, Reservation.ReturnPlacement.GridIndex, ExpectedEquipmentGeneration);
    // No inventory-change event: ownership, placements and ammunition have not changed.
    return true;
}

bool UAZ_Inv_CommonUI_InventoryComponent::TryCommitMagazineReload(const FGuid& ReloadId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || bMagazineReloadMutation || !ReloadId.IsValid() ||
        MagazineReload.ReloadId != ReloadId) return false;
    if (MagazineReload.bCommitted) return true;
    const FMagazineReloadReservation Reservation = MagazineReload;
    const auto* Equipment = GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
    auto* WeaponItem = FindItemById(Reservation.WeaponItemId);
    auto* Incoming = FindItemById(Reservation.IncomingMagazineId);
    auto* Outgoing = FindItemById(Reservation.OutgoingMagazineId);
    if (!Equipment || !WeaponItem || !Incoming || Equipment->GetActiveItem() != WeaponItem ||
        Equipment->GetSelectionGeneration() != Reservation.EquipmentGeneration ||
        !Equipment->IsActiveWeaponSource(Reservation.WeaponSource.Get()) ||
        WeaponItem->GetInsertedMagazineId() != Reservation.OutgoingMagazineId) return false;
    const FAZ_WeaponAmmoSnapshot Snapshot = GetWeaponAmmoSnapshot(Reservation.WeaponItemId);
    if (Snapshot.MagazineState == EAZ_WeaponMagazineState::Unavailable ||
        Snapshot.MagazineItemId != Reservation.OutgoingMagazineId || Snapshot.Rounds != Reservation.OutgoingRounds ||
        Snapshot.AmmoRevision != Reservation.OutgoingAmmoRevision ||
        (Reservation.OutgoingMagazineId.IsValid() && (!Outgoing || Snapshot.AmmoRevision == MAX_int64))) return false;
    const auto* Definition = WeaponItem->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
    const auto* Magazine = Incoming->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_MagazineFragment>();
    if (!Definition || !Magazine || Incoming->IsWeapon() || Incoming->IsStackable() || Incoming->GetTotalStackCount() != 1 ||
        Incoming->GetLocation() != EAZ_InventoryItemLocation::Backpack || Incoming->GetParentItemId().IsValid() ||
        Incoming->GetInsertedMagazineId().IsValid() || Magazine->MagazineFamily != Definition->MagazineFamily ||
        Magazine->Capacity <= 0 || Incoming->GetMagazineRounds() > Magazine->Capacity ||
        Incoming->GetMagazineRounds() != Reservation.IncomingRounds || Incoming->GetMagazineRounds() < 0 ||
        (Reservation.bSkipEmpty && Incoming->GetMagazineRounds() == 0) ||
        Incoming->GetInstanceState().AmmoRevision != Reservation.IncomingAmmoRevision ||
        Reservation.IncomingAmmoRevision < 0 || Reservation.IncomingAmmoRevision == MAX_int64) return false;
    const auto* Placement = GridPlacements.FindByPredicate([&](const auto& P) { return P.ItemId == Reservation.IncomingMagazineId; });
    if (!Placement || Placement->GridIndex != Reservation.IncomingPlacement.GridIndex || Placement->StackCount != 1) return false;
    auto Working = GridPlacements;
    if (Working.RemoveAll([&](const auto& P) { return P.ItemId == Reservation.IncomingMagazineId; }) != 1 ||
        !IsPlacementFree(Incoming->GetItemManifest(), Placement->GridIndex, Working, false)) return false;
    if (Outgoing)
    {
        if (Working.ContainsByPredicate([&](const auto& P) { return P.ItemId == Reservation.OutgoingMagazineId; }) ||
            !IsPlacementFree(Outgoing->GetItemManifest(), Reservation.ReturnPlacement.GridIndex, Working, false)) return false;
        Working.Add(Reservation.ReturnPlacement);
    }
    // All checks precede writes, and all writes precede callbacks. Existing item objects and rounds survive.
    Incoming->InstanceState.Location = EAZ_InventoryItemLocation::WeaponMagazine;
    Incoming->InstanceState.ParentItemId = Reservation.WeaponItemId;
    ++Incoming->InstanceState.AmmoRevision;
    if (Outgoing)
    {
        Outgoing->InstanceState.Location = EAZ_InventoryItemLocation::Backpack;
        Outgoing->InstanceState.ParentItemId.Invalidate();
        ++Outgoing->InstanceState.AmmoRevision;
    }
    WeaponItem->InstanceState.InsertedMagazineId = Reservation.IncomingMagazineId;
    GridPlacements = MoveTemp(Working);
    MagazineReload.bCommitted = true;
    UE_LOG(LogAZInventory, Log, TEXT("[Inventory] Reload committed action=%s weapon=%s inserted=%s/%d returned=%s/%d generation=%u"),
        *ReloadId.ToString(), *Reservation.WeaponItemId.ToString(), *Reservation.IncomingMagazineId.ToString(), Reservation.IncomingRounds,
        *Reservation.OutgoingMagazineId.ToString(), Reservation.OutgoingRounds, Reservation.EquipmentGeneration);
    NotifyInventoryChanged();
    return true;
}

bool UAZ_Inv_CommonUI_InventoryComponent::IsMagazineReloadCommitted(const FGuid& ReloadId) const
{
    return ReloadId.IsValid() && MagazineReload.ReloadId == ReloadId && MagazineReload.bCommitted;
}

void UAZ_Inv_CommonUI_InventoryComponent::EndMagazineReload(const FGuid& ReloadId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !ReloadId.IsValid() || MagazineReload.ReloadId != ReloadId) return;
    const FGuid EndedReloadId = ReloadId;
    const bool bCommitted = MagazineReload.bCommitted;
    const FGuid WeaponItemId = MagazineReload.WeaponItemId;
    MagazineReload = FMagazineReloadReservation();
    UE_LOG(LogAZInventory, Log, TEXT("[Inventory] Reload ended action=%s weapon=%s committed=%d"),
        *EndedReloadId.ToString(), *WeaponItemId.ToString(), bCommitted);
}

bool UAZ_Inv_CommonUI_InventoryComponent::IsItemReloadReserved(const FGuid& ItemId) const
{
    return ItemId.IsValid() && MagazineReload.ReloadId.IsValid() &&
        (ItemId == MagazineReload.WeaponItemId || ItemId == MagazineReload.IncomingMagazineId || ItemId == MagazineReload.OutgoingMagazineId);
}

bool UAZ_Inv_CommonUI_InventoryComponent::IsWeaponReloading(const FGuid& WeaponItemId) const
{
    return WeaponItemId.IsValid() && MagazineReload.ReloadId.IsValid() && MagazineReload.WeaponItemId == WeaponItemId;
}

bool UAZ_Inv_CommonUI_InventoryComponent::CanLoadMagazine(const UAZ_Inv_CommonUI_InventoryItem* Item) const
{
    if (!GetOwner() || !IsValid(Item) || !Item->IsInitialized() || !Item->IsMagazine() || !ContainsItem(Item)) return false;
    const auto* Equipment = GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
    const auto* WeaponItem = Equipment ? Equipment->GetActiveItem() : nullptr;
    // Menu capture is expected during this read-only preview. The normal gameplay
    // and ability gates are checked again after the menu closes on both peers.
    return IsValid(WeaponItem) && CanReloadMagazine(Equipment->GetActiveWeapon(), WeaponItem->GetInstanceId(),
        Equipment->GetSelectionGeneration(), Item->GetInstanceId(), false);
}

bool UAZ_Inv_CommonUI_InventoryComponent::RequestLoadMagazine(UAZ_Inv_CommonUI_InventoryItem* Item)
{
    const auto* Player = Cast<AAZ_PlayerController>(GetOwner());
    if (!Player || !Player->IsLocalController() || !bInventoryMenuOpen || !CanLoadMagazine(Item)) return false;
    const auto* Equipment = GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
    const TWeakObjectPtr<AAZ_Weapon> Source = Equipment->GetActiveWeapon();
    const FGuid WeaponItemId = Equipment->GetActiveItem()->GetInstanceId();
    const uint32 Generation = Equipment->GetSelectionGeneration();
    const FGuid MagazineItemId = Item->GetInstanceId();
    const int64 IncomingRevision = Item->GetInstanceState().AmmoRevision;
    const FAZ_WeaponAmmoSnapshot Ammo = GetWeaponAmmoSnapshot(WeaponItemId);
    // Closing publishes UI callbacks, so carry identities/revisions across it,
    // not a later read of whichever grid item or weapon happens to be selected.
    // The capture-close RPC and this component RPC share the controller's channel.
    CloseInventoryMenu();
    Server_LoadMagazine(Source.Get(), WeaponItemId, Generation, MagazineItemId, IncomingRevision,
        Ammo.MagazineItemId, Ammo.AmmoRevision);
    return true;
}

void UAZ_Inv_CommonUI_InventoryComponent::Server_LoadMagazine_Implementation(AAZ_Weapon* ExpectedSource,
    FGuid WeaponItemId, uint32 ExpectedGeneration, FGuid MagazineItemId, int64 ExpectedIncomingRevision,
    FGuid ExpectedInsertedMagazineId, int64 ExpectedInsertedRevision)
{
    const auto* Player = Cast<AAZ_PlayerController>(GetOwner());
    if (!Player || !Player->HasAuthority() || Player->IsInventoryInputCaptured() || !MagazineItemId.IsValid()) return;
    auto* Equipment = GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
    const auto* Incoming = FindItemById(MagazineItemId);
    const FAZ_WeaponAmmoSnapshot Ammo = GetWeaponAmmoSnapshot(WeaponItemId);
    if (!Equipment || !Incoming || Incoming->GetInstanceState().AmmoRevision != ExpectedIncomingRevision ||
        Ammo.MagazineState == EAZ_WeaponMagazineState::Unavailable ||
        Ammo.MagazineItemId != ExpectedInsertedMagazineId || Ammo.AmmoRevision != ExpectedInsertedRevision) return;
    // Selection, compatibility, capacity, animation and GAS eligibility are all
    // revalidated before the shared reload transaction can reserve these items.
    Equipment->RequestMagazineReload(ExpectedSource, WeaponItemId, ExpectedGeneration, MagazineItemId, false);
}

FIntPoint UAZ_Inv_CommonUI_InventoryComponent::GetGridDimensions(EInv_ItemCategory Category) const
{
    switch (Category)
    {
    case EInv_ItemCategory::Equippable: return EquippableGridDimensions;
    case EInv_ItemCategory::Consumable: return ConsumableGridDimensions;
    case EInv_ItemCategory::Craftable: return CraftableGridDimensions;
    default: return FIntPoint::ZeroValue;
    }
}

bool UAZ_Inv_CommonUI_InventoryComponent::IsPlacementFree(const FAZ_Inv_CommonUI_ItemManifest& Manifest, int32 GridIndex,
    const TArray<FAZ_InventoryGridPlacement>& Placements, bool bIncludeReloadReservation) const
{
    const FIntPoint Grid = GetGridDimensions(Manifest.GetItemCategory());
    const FIntPoint Size = ItemDimensions(Manifest);
    if (Grid.X <= 0 || Grid.Y <= 0 || Size.X <= 0 || Size.Y <= 0 || GridIndex < 0 || GridIndex >= Grid.X * Grid.Y) return false;
    const FIntPoint Start(GridIndex % Grid.X, GridIndex / Grid.X);
    if (Start.X + Size.X > Grid.X || Start.Y + Size.Y > Grid.Y) return false;
    for (const auto& Placement : Placements)
    {
        const auto* Other = FindItemById(Placement.ItemId);
        const auto& OtherManifest = Other ? Other->GetItemManifest() : Manifest;
        if (OtherManifest.GetItemCategory() != Manifest.GetItemCategory()) continue;
        const FIntPoint OtherSize = ItemDimensions(OtherManifest);
        const FIntPoint OtherStart(Placement.GridIndex % Grid.X, Placement.GridIndex / Grid.X);
        if (Start.X < OtherStart.X + OtherSize.X && Start.X + Size.X > OtherStart.X &&
            Start.Y < OtherStart.Y + OtherSize.Y && Start.Y + Size.Y > OtherStart.Y) return false;
    }
    if (bIncludeReloadReservation && MagazineReload.ReloadId.IsValid() && !MagazineReload.bCommitted)
    {
        const auto* Returning = FindItemById(MagazineReload.OutgoingMagazineId);
        if (Returning && Returning->GetItemManifest().GetItemCategory() == Manifest.GetItemCategory())
        {
            const FIntPoint OtherSize = ItemDimensions(Returning->GetItemManifest());
            const int32 ReturnIndex = MagazineReload.ReturnPlacement.GridIndex;
            const FIntPoint OtherStart(ReturnIndex % Grid.X, ReturnIndex / Grid.X);
            if (Start.X < OtherStart.X + OtherSize.X && Start.X + Size.X > OtherStart.X &&
                Start.Y < OtherStart.Y + OtherSize.Y && Start.Y + Size.Y > OtherStart.Y) return false;
        }
    }
    return true;
}

FAZ_Inv_CommonUI_SlotAvailabilityResult UAZ_Inv_CommonUI_InventoryComponent::GetRoomForItem(
    const FAZ_Inv_CommonUI_ItemManifest& Manifest, int32 StackAmountOverride) const
{
    FAZ_Inv_CommonUI_SlotAvailabilityResult Result;
    Result.bIsStackable = Manifest.IsStackable();
    const auto* Stack = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_Stackable_Fragment>();
    const int32 MaxStack = Result.bIsStackable && Stack ? FMath::Max(1, Stack->GetMaxStackSize()) : 1;
    int32 Remaining = Result.bIsStackable ? (StackAmountOverride >= 0 ? StackAmountOverride : (Stack ? Stack->GetStackCount() : 1)) : 1;
    Result.RemainingRooms = Remaining;
    if (Remaining <= 0) return Result;
    TArray<FAZ_InventoryGridPlacement> Working = GridPlacements;
    if (Result.bIsStackable)
    {
        for (auto* Item : GetItems())
        {
            if (Item->IsStackable() && Item->GetLocation() == EAZ_InventoryItemLocation::Backpack &&
                Item->GetItemManifest().GetItemTypeTag() == Manifest.GetItemTypeTag())
            {
                Result.Item = Item;
                break;
            }
        }
        if (Result.Item.IsValid())
        {
            for (auto& Placement : Working)
            {
                if (Placement.ItemId != Result.Item->GetInstanceId()) continue;
                const int32 Added = FMath::Min(Remaining, FMath::Max(0, MaxStack - Placement.StackCount));
                if (Added <= 0) continue;
                Result.AvailableSlots.Emplace(Placement.GridIndex, Added, true);
                Placement.StackCount += Added;
                Remaining -= Added;
                Result.TotalRoomToFill += Added;
                if (Remaining == 0) break;
            }
        }
    }
    const FIntPoint Grid = GetGridDimensions(Manifest.GetItemCategory());
    for (int32 Index = 0; Remaining > 0 && Index < Grid.X * Grid.Y; ++Index)
    {
        if (!IsPlacementFree(Manifest, Index, Working)) continue;
        const int32 Added = FMath::Min(Remaining, MaxStack);
        FAZ_InventoryGridPlacement& Placement = Working.AddDefaulted_GetRef();
        Placement.ItemId = Result.Item.IsValid() ? Result.Item->GetInstanceId() : FGuid();
        Placement.GridIndex = Index;
        Placement.StackCount = Added;
        Result.AvailableSlots.Emplace(Index, Added, false);
        Result.TotalRoomToFill += Added;
        Remaining -= Added;
    }
    Result.RemainingRooms = Remaining;
    return Result;
}

bool UAZ_Inv_CommonUI_InventoryComponent::ValidatePickupPayload(const FAZ_InventoryPickupRecord& Root,
    const TArray<FAZ_InventoryPickupRecord>& Children) const
{
    if (!Root.State.InstanceId.IsValid() || FindItemById(Root.State.InstanceId) || Root.StackCount <= 0 ||
        Root.State.Location != EAZ_InventoryItemLocation::World || Root.State.ParentItemId.IsValid()) return false;
    const auto* Weapon = Root.Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
    const auto* RootMag = Root.Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_MagazineFragment>();
    if (Weapon && Weapon->bUsesDetachableMagazines
        && (!Weapon->IsFireModeSupported(Root.State.SelectedFireMode) || Root.State.FireModeRevision < 0)) return false;
    if (!Root.Manifest.IsStackable() && Root.StackCount != 1) return false;
    if (RootMag && (RootMag->Capacity <= 0 || RootMag->MagazineFamily.IsNone() || Root.State.CurrentRounds < 0 || Root.State.CurrentRounds > RootMag->Capacity || Root.State.AmmoRevision < 0)) return false;
    if (Children.IsEmpty()) return !Root.State.InsertedMagazineId.IsValid();
    if (Children.Num() != 1 || !Weapon || !Weapon->bUsesDetachableMagazines || Weapon->MagazineFamily.IsNone() || RootMag) return false;
    const auto& Child = Children[0];
    const auto* Magazine = Child.Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_MagazineFragment>();
    return Magazine && Child.StackCount == 1 && Child.State.InstanceId.IsValid() && Child.State.InstanceId != Root.State.InstanceId &&
        !FindItemById(Child.State.InstanceId) && Child.State.InstanceId == Root.State.InsertedMagazineId &&
        Child.State.ParentItemId == Root.State.InstanceId && Child.State.Location == EAZ_InventoryItemLocation::WeaponMagazine &&
        !Child.State.InsertedMagazineId.IsValid() && Magazine->MagazineFamily == Weapon->MagazineFamily &&
        Magazine->Capacity > 0 && Child.State.CurrentRounds >= 0 && Child.State.CurrentRounds <= Magazine->Capacity && Child.State.AmmoRevision >= 0;
}

void UAZ_Inv_CommonUI_InventoryComponent::TryAddItem(UAZ_Inv_CommonUI_ItemComponent* ItemComponent)
{
    if (GetOwner() && GetOwner()->HasAuthority()) TryPickupItem(ItemComponent);
    else Server_AddNewItem(ItemComponent, 0, 0);
}

bool UAZ_Inv_CommonUI_InventoryComponent::TryPickupItem(UAZ_Inv_CommonUI_ItemComponent* ItemComponent)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || bMagazineReloadMutation ||
        !IsValid(ItemComponent) || ItemComponent->HasBeenPickedUp()) return false;
    AActor* Pickup = ItemComponent->GetOwner();
    const auto* Controller = Cast<APlayerController>(GetOwner());
    const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    if (!IsValid(Pickup) || Pickup->IsActorBeingDestroyed() || !Pawn || Pickup->GetWorld() != GetWorld() ||
        FVector::DistSquared(Pawn->GetActorLocation(), Pickup->GetActorLocation()) > FMath::Square(MaximumPickupDistance)) return false;
    if (!Pawn->IsOverlappingActor(Pickup)) return false;
    FCollisionQueryParams PickupQuery(SCENE_QUERY_STAT(InventoryPickupVisibility), false, Pawn);
    FHitResult Obstruction;
    if (GetWorld()->LineTraceSingleByChannel(Obstruction, Pawn->GetPawnViewLocation(), Pickup->GetActorLocation(), ECC_Visibility, PickupQuery) &&
        Obstruction.GetActor() != Pickup) return false;
    if (!ItemComponent->InitializePickupPayload()) return false;
    const FAZ_InventoryPickupRecord Root = ItemComponent->GetRootRecord();
    const TArray<FAZ_InventoryPickupRecord> Children = ItemComponent->GetContainedItems();
    if (!ValidatePickupPayload(Root, Children))
    {
        UE_LOG(LogAZInventory, Warning, TEXT("[Inventory] Pickup refused: invalid/duplicate payload actor=%s id=%s"), *Pickup->GetName(), *Root.State.InstanceId.ToString());
        return false;
    }
    auto Room = GetRoomForItem(Root.Manifest, Root.StackCount);
    if (Room.TotalRoomToFill == 0)
    {
        UE_LOG(LogAZInventory, Log, TEXT("[Inventory] Pickup refused: full category=%d id=%s"), static_cast<int32>(Root.Manifest.GetItemCategory()), *Root.State.InstanceId.ToString());
        OnNoRoomInInventory.Broadcast();
        return false;
    }
    TArray<UAZ_Inv_CommonUI_InventoryItem*> AddedItems;
    UAZ_Inv_CommonUI_InventoryItem* RootItem = Room.Item.Get();
    if (!RootItem)
    {
        auto Manifest = Root.Manifest;
        RootItem = Manifest.Manifest(GetOwner());
        auto State = Root.State;
        State.Location = EAZ_InventoryItemLocation::Backpack;
        // A partial split of a fungible stack creates another stack identity. Rifles/magazines never split.
        if (Room.RemainingRooms > 0) State.InstanceId = FGuid::NewGuid();
        RootItem->InitializeInstance(State, Room.TotalRoomToFill);
        InventoryList.AddInventoryItem(RootItem);
        AddedItems.Add(RootItem);
    }
    else
    {
        RootItem->SetTotalStackCount(RootItem->GetTotalStackCount() + Room.TotalRoomToFill);
    }
    for (const auto& Child : Children)
    {
        auto Manifest = Child.Manifest;
        auto* Item = Manifest.Manifest(GetOwner());
        Item->InitializeInstance(Child.State, 1);
        InventoryList.AddInventoryItem(Item);
        AddedItems.Add(Item);
    }
    for (const auto& Slot : Room.AvailableSlots)
    {
        if (Slot.bItemAtIndex)
        {
            if (auto* Placement = GridPlacements.FindByPredicate([&](const auto& P) { return P.ItemId == RootItem->GetInstanceId() && P.GridIndex == Slot.Index; })) Placement->StackCount += Slot.AmountToFill;
        }
        else
        {
            auto& Placement = GridPlacements.AddDefaulted_GetRef();
            Placement.ItemId = RootItem->GetInstanceId();
            Placement.GridIndex = Slot.Index;
            Placement.StackCount = Slot.AmountToFill;
        }
    }
    // Close the world transfer before callbacks can request the same pickup again.
    if (Room.RemainingRooms == 0) ItemComponent->PickedUp();
    else ItemComponent->SetRemainingStackCount(Room.RemainingRooms);
    UE_LOG(LogAZInventory, Log, TEXT("[Inventory] Pickup committed item=%s type=%s count=%d magazine=%s contents=%d"),
        *RootItem->GetInstanceId().ToString(), *RootItem->GetItemManifest().GetItemTypeTag().ToString(), Room.TotalRoomToFill,
        *RootItem->GetInsertedMagazineId().ToString(), Children.Num());
    for (auto* Item : AddedItems) OnItemAdded.Broadcast(Item);
    NotifyInventoryChanged();
    return true;
}

void UAZ_Inv_CommonUI_InventoryComponent::Server_AddNewItem_Implementation(UAZ_Inv_CommonUI_ItemComponent* ItemComponent, int32 StackCount, int32 Remainder)
{
    // Legacy arguments are hints from widgets, never authority for capacity or item counts.
    TryPickupItem(ItemComponent);
}

void UAZ_Inv_CommonUI_InventoryComponent::Server_AddStacksToItem_Implementation(UAZ_Inv_CommonUI_ItemComponent* ItemComponent, int32 StackCount, int32 RemainingRooms)
{
    TryPickupItem(ItemComponent);
}

bool UAZ_Inv_CommonUI_InventoryComponent::RemoveStackPlacements(const FGuid& ItemId, int32 Count)
{
    int32 Available = 0;
    for (const auto& Placement : GridPlacements) if (Placement.ItemId == ItemId) Available += Placement.StackCount;
    if (Count <= 0 || Available < Count) return false;
    for (int32 Index = GridPlacements.Num() - 1; Index >= 0 && Count > 0; --Index)
    {
        auto& Placement = GridPlacements[Index];
        if (Placement.ItemId != ItemId) continue;
        const int32 Removed = FMath::Min(Count, Placement.StackCount);
        Placement.StackCount -= Removed;
        Count -= Removed;
        if (Placement.StackCount == 0) GridPlacements.RemoveAt(Index);
    }
    return true;
}

void UAZ_Inv_CommonUI_InventoryComponent::RemoveOwnedItem(UAZ_Inv_CommonUI_InventoryItem* Item)
{
    GridPlacements.RemoveAll([&](const auto& P) { return P.ItemId == Item->GetInstanceId(); });
    InventoryList.RemoveInventoryItem(Item);
    if (IsUsingRegisteredSubObjectList()) RemoveReplicatedSubObject(Item);
}

void UAZ_Inv_CommonUI_InventoryComponent::Server_DropItem_Implementation(UAZ_Inv_CommonUI_InventoryItem* Item, int32 StackCount)
{
    if (bMagazineReloadMutation || !ContainsItem(Item) || Item->GetLocation() != EAZ_InventoryItemLocation::Backpack ||
        (IsItemReloadReserved(Item->GetInstanceId()) && !IsWeaponReloading(Item->GetInstanceId()))) return;
    const int32 Count = Item->IsStackable() ? StackCount : 1;
    if (Count <= 0 || Count > Item->GetTotalStackCount()) return;
    auto* Equipment = GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
    if (Equipment && !Equipment->CanDropItem(Item)) return;
    int32 PlacedCount = 0;
    for (const auto& P : GridPlacements) if (P.ItemId == Item->GetInstanceId()) PlacedCount += P.StackCount;
    if (PlacedCount < Count) return;
    UAZ_Inv_CommonUI_InventoryItem* Magazine = FindItemById(Item->GetInsertedMagazineId());
    if (Item->GetInsertedMagazineId().IsValid() && (!Magazine || Magazine->GetParentItemId() != Item->GetInstanceId())) return;
    const FGuid ItemId = Item->GetInstanceId();
    const FGuid InsertedId = Item->GetInsertedMagazineId();
    const int32 OriginalCount = Item->GetTotalStackCount();
    const int64 OriginalAmmoRevision = Magazine ? Magazine->GetInstanceState().AmmoRevision : -1;
    const int32 OriginalRounds = Magazine ? Magazine->GetMagazineRounds() : -1;
    // Spawning and equipment cleanup can invoke gameplay callbacks. Keep the prepared payload stable
    // until ownership is removed, while still allowing EndMagazineReload to release its action token.
    TGuardValue<bool> MutationGuard(bMagazineReloadMutation, true);
    AActor* Pickup = SpawnDroppedItem(Item, Count);
    if (!Pickup)
    {
        UE_LOG(LogAZInventory, Warning, TEXT("[Inventory] Drop refused: spawn failed item=%s"), *Item->GetInstanceId().ToString());
        return;
    }
    if (!ContainsItem(Item) || Item->GetInstanceId() != ItemId || Item->GetInsertedMagazineId() != InsertedId ||
        Item->GetLocation() != EAZ_InventoryItemLocation::Backpack || Item->GetTotalStackCount() != OriginalCount ||
        (Equipment && !Equipment->CanDropItem(Item)) ||
        (Magazine && (!ContainsItem(Magazine) || Magazine->GetParentItemId() != ItemId ||
            Magazine->GetInstanceState().AmmoRevision != OriginalAmmoRevision || Magazine->GetMagazineRounds() != OriginalRounds)))
    {
        Pickup->Destroy();
        return;
    }
    const bool bRemoveItem = Count == Item->GetTotalStackCount();
    TArray<UAZ_Inv_CommonUI_InventoryItem*> RemovedItems;
    if (bRemoveItem)
    {
        if (Equipment) Equipment->PrepareItemForDrop(Item);
        if (IsWeaponReloading(ItemId)) EndMagazineReload(MagazineReload.ReloadId);
        if (Magazine) { RemoveOwnedItem(Magazine); RemovedItems.Add(Magazine); }
        RemoveOwnedItem(Item);
        RemovedItems.Add(Item);
    }
    else
    {
        RemoveStackPlacements(Item->GetInstanceId(), Count);
        Item->SetTotalStackCount(Item->GetTotalStackCount() - Count);
    }
    UE_LOG(LogAZInventory, Log, TEXT("[Inventory] Drop committed item=%s count=%d magazine=%s pickup=%s"),
        *Item->GetInstanceId().ToString(), Count, *Item->GetInsertedMagazineId().ToString(), *Pickup->GetName());
    if (bRemoveItem) OnItemDropped.Broadcast(Item);
    for (auto* Removed : RemovedItems) OnItemRemoved.Broadcast(Removed);
    NotifyInventoryChanged();
}

AActor* UAZ_Inv_CommonUI_InventoryComponent::SpawnDroppedItem(UAZ_Inv_CommonUI_InventoryItem* Item, int32 StackCount)
{
    const auto* Controller = Cast<APlayerController>(GetOwner());
    const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    if (!Pawn || !GetWorld()) return nullptr;
    auto Root = MakePickupRecord(Item);
    Root.StackCount = StackCount;
    Root.State.Location = EAZ_InventoryItemLocation::World;
    Root.State.ParentItemId.Invalidate();
    if (StackCount < Item->GetTotalStackCount()) Root.State.InstanceId = FGuid::NewGuid();
    if (auto* Stack = Root.Manifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_Stackable_Fragment>()) Stack->SetStackCount(StackCount);
    TArray<FAZ_InventoryPickupRecord> Children;
    if (const auto* Magazine = FindItemById(Item->GetInsertedMagazineId())) Children.Add(MakePickupRecord(Magazine));
    const FVector PawnLocation = Pawn->GetActorLocation();
    const float FeetZ = PawnLocation.Z - Pawn->GetSimpleCollisionHalfHeight();
    constexpr float ClearanceRadius = 18.f;
    constexpr float MaximumFloorHeightDifference = 80.f;
    const float MinimumDistance = Pawn->GetSimpleCollisionRadius() + ClearanceRadius + 8.f;
    FCollisionQueryParams DropQuery(SCENE_QUERY_STAT(InventoryDropPlacement), false, Pawn);
    FVector Location = FVector::ZeroVector;
    bool bFoundSupportedLocation = false;
    // The pickup's overlap-only root cannot detect walls during SpawnActor. Preflight a reachable
    // anchor explicitly, keeping the entire inventory/equipment transaction unchanged on refusal.
    for (int32 Attempt = 0; Attempt < 6; ++Attempt)
    {
        FVector Forward = Pawn->GetActorForwardVector().RotateAngleAxis(FMath::FRandRange(DropSpawnAngleMin, DropSpawnAngleMax), FVector::UpVector);
        Forward.Z = 0.f;
        Forward = Forward.GetSafeNormal();
        FVector Candidate = PawnLocation + Forward * FMath::FRandRange(DropSpawnDistanceMin, DropSpawnDistanceMax);
        FHitResult WallHit;
        if (GetWorld()->SweepSingleByChannel(WallHit, PawnLocation, Candidate, FQuat::Identity, ECC_Visibility,
            FCollisionShape::MakeSphere(ClearanceRadius), DropQuery))
        {
            if (WallHit.bStartPenetrating) continue;
            Candidate = WallHit.Location - Forward * 8.f;
        }
        if (FVector::DistSquared2D(Candidate, PawnLocation) < FMath::Square(MinimumDistance)) continue;

        // A supported location must be near the pawn's current floor, including while crouched.
        // This rejects dropping into a lower storey/void merely because a long downward trace hits it.
        FHitResult FloorHit;
        const FVector FloorStart(Candidate.X, Candidate.Y, FeetZ + MaximumFloorHeightDifference + RelativeSpawnElevation);
        const FVector FloorEnd(Candidate.X, Candidate.Y, FeetZ - MaximumFloorHeightDifference);
        if (!GetWorld()->LineTraceSingleByChannel(FloorHit, FloorStart, FloorEnd, ECC_Visibility, DropQuery) ||
            FloorHit.ImpactNormal.Z < 0.6f || FMath::Abs(FloorHit.ImpactPoint.Z - FeetZ) > MaximumFloorHeightDifference) continue;
        Candidate = FloorHit.ImpactPoint + FVector(0.f, 0.f, 12.f);

        // Use the same sight line as pickup validation: a low wall must not strand the item either.
        FHitResult SightHit;
        if (GetWorld()->LineTraceSingleByChannel(SightHit, Pawn->GetPawnViewLocation(), Candidate, ECC_Visibility, DropQuery)) continue;
        Location = Candidate;
        bFoundSupportedLocation = true;
        break;
    }
    if (!bFoundSupportedLocation)
    {
        UE_LOG(LogAZInventory, Log, TEXT("[Inventory] Drop refused: no reachable supported location item=%s"), *Item->GetInstanceId().ToString());
        return nullptr;
    }
    AActor* Pickup = Root.Manifest.SpawnPickupActor(this, Location, FRotator::ZeroRotator);
    if (!IsValid(Pickup)) return nullptr;
    // Do not accept an engine collision adjustment or construction-script relocation outside the preflighted anchor.
    if (!Pickup->GetActorLocation().Equals(Location, 1.f)) { Pickup->Destroy(); return nullptr; }
    auto* Component = Pickup->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>();
    if (!Component) { Pickup->Destroy(); return nullptr; }
    Component->SetPickupPayload(Root, Children);
    return Pickup;
}

void UAZ_Inv_CommonUI_InventoryComponent::Server_ConsumeItem_Implementation(UAZ_Inv_CommonUI_InventoryItem* Item)
{
    if (bMagazineReloadMutation || !ContainsItem(Item) || IsItemReloadReserved(Item->GetInstanceId()) ||
        Item->GetLocation() != EAZ_InventoryItemLocation::Backpack || !Item->IsConsumable()) return;
    auto* Consumable = Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_ConsumableFragment>();
    if (!Consumable || !RemoveStackPlacements(Item->GetInstanceId(), 1)) return;
    const bool bRemoveItem = Item->GetTotalStackCount() <= 1;
    if (bRemoveItem) RemoveOwnedItem(Item);
    else Item->SetTotalStackCount(Item->GetTotalStackCount() - 1);
    Consumable->OnConsume(Cast<APlayerController>(GetOwner()));
    if (bRemoveItem) OnItemRemoved.Broadcast(Item);
    NotifyInventoryChanged();
}

void UAZ_Inv_CommonUI_InventoryComponent::Server_MoveItem_Implementation(UAZ_Inv_CommonUI_InventoryItem* Item,
    int32 SourceGridIndex, int32 TargetGridIndex, int32 StackCount)
{
    if (bMagazineReloadMutation || !ContainsItem(Item) || IsItemReloadReserved(Item->GetInstanceId()) ||
        Item->GetLocation() != EAZ_InventoryItemLocation::Backpack || StackCount <= 0) return;
    auto Working = GridPlacements;
    const int32 Source = Working.IndexOfByPredicate([&](const auto& P) { return P.ItemId == Item->GetInstanceId() && P.GridIndex == SourceGridIndex; });
    if (Source == INDEX_NONE || StackCount > Working[Source].StackCount || SourceGridIndex == TargetGridIndex) return;
    const auto SourcePlacement = Working[Source];
    if (StackCount == SourcePlacement.StackCount) Working.RemoveAt(Source);
    else Working[Source].StackCount -= StackCount;
    const int32 Target = Working.IndexOfByPredicate([&](const auto& P)
    {
        auto* Other = FindItemById(P.ItemId);
        return Other && Other->GetItemManifest().GetItemCategory() == Item->GetItemManifest().GetItemCategory() && P.GridIndex == TargetGridIndex;
    });
    if (Target != INDEX_NONE && IsItemReloadReserved(Working[Target].ItemId)) return;
    if (Target != INDEX_NONE && Working[Target].ItemId == Item->GetInstanceId() && Item->IsStackable())
    {
        const auto* Stack = Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_Stackable_Fragment>();
        if (!Stack || Working[Target].StackCount + StackCount > Stack->GetMaxStackSize()) return;
        Working[Target].StackCount += StackCount;
    }
    else if (Target != INDEX_NONE)
    {
        const auto TargetPlacement = Working[Target];
        auto* Other = FindItemById(TargetPlacement.ItemId);
        if (!Other || Item->IsStackable() || Other->IsStackable() || StackCount != SourcePlacement.StackCount) return;
        Working.RemoveAt(Target);
        if (!IsPlacementFree(Item->GetItemManifest(), TargetGridIndex, Working)) return;
        auto Moved = SourcePlacement;
        Moved.GridIndex = TargetGridIndex;
        Working.Add(Moved);
        if (!IsPlacementFree(Other->GetItemManifest(), SourceGridIndex, Working)) return;
        auto Swapped = TargetPlacement;
        Swapped.GridIndex = SourceGridIndex;
        Working.Add(Swapped);
    }
    else
    {
        if (!IsPlacementFree(Item->GetItemManifest(), TargetGridIndex, Working)) return;
        auto Moved = SourcePlacement;
        Moved.GridIndex = TargetGridIndex;
        Moved.StackCount = StackCount;
        Working.Add(Moved);
    }
    GridPlacements = MoveTemp(Working);
    NotifyInventoryChanged();
}

void UAZ_Inv_CommonUI_InventoryComponent::Server_EquipSlotClicked_Implementation(UAZ_Inv_CommonUI_InventoryItem* ItemToEquip,
    UAZ_Inv_CommonUI_InventoryItem* ItemToUnequip)
{
    auto* Equipment = GetOwner()->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
    if (!Equipment) return;
    if (ItemToEquip) Equipment->RequestEquipItem(ItemToEquip);
    else if (ItemToUnequip) Equipment->RequestUnequipItem(ItemToUnequip);
}

void UAZ_Inv_CommonUI_InventoryComponent::Multicast_EquipSlotClicked_Implementation(UAZ_Inv_CommonUI_InventoryItem* ItemToEquip,
    UAZ_Inv_CommonUI_InventoryItem* ItemToUnequip)
{
    // Kept as a legacy reflected entry point; only the authoritative equipment owner may change grants.
    if (GetOwner()->HasAuthority()) Server_EquipSlotClicked_Implementation(ItemToEquip, ItemToUnequip);
}

void UAZ_Inv_CommonUI_InventoryComponent::AddRepSubObjects(UObject* SubObject)
{
    if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && IsValid(SubObject)) AddReplicatedSubObject(SubObject);
}

void UAZ_Inv_CommonUI_InventoryComponent::NotifyInventoryChanged()
{
    OnInventoryChanged.Broadcast();
    if (GetOwner() && GetOwner()->HasAuthority()) GetOwner()->ForceNetUpdate();
}

void UAZ_Inv_CommonUI_InventoryComponent::OnRep_Placements()
{
    NotifyInventoryChanged();
}

void UAZ_Inv_CommonUI_InventoryComponent::ToggleInventoryMenu()
{
    if (bInventoryMenuOpen) CloseInventoryMenu();
    else OpenInventoryMenu();
}

void UAZ_Inv_CommonUI_InventoryComponent::ConstructInventory()
{
    OwningController = Cast<APlayerController>(GetOwner());
    if (!OwningController.IsValid() || !OwningController->IsLocalController() || !InventoryScreenClass) return;
    InventoryMenu = CreateWidget<UAZ_Inv_CommonUI_GameInventoryMenu>(OwningController.Get(), InventoryScreenClass);
    if (!InventoryMenu) return;
    InventoryMenu->OnBackAction.BindUObject(this, &ThisClass::CloseInventoryMenu);
    InventoryMenu->AddToViewport();
    CloseInventoryMenu();
}

void UAZ_Inv_CommonUI_InventoryComponent::OpenInventoryMenu()
{
    if (bInventoryMenuOpen || !IsValid(InventoryMenu) || !OwningController.IsValid()) return;
    FInputModeGameAndUI InputMode;
    InputMode.SetWidgetToFocus(InventoryMenu->TakeWidget());
    OwningController->SetInputMode(InputMode);
    OwningController->SetShowMouseCursor(true);
    InventoryMenu->SetVisibility(ESlateVisibility::Visible);
    InventoryMenu->ActivateWidget();
    InventoryMenu->SetUserFocus(OwningController.Get());
    bInventoryMenuOpen = true;
    OnInventoryMenuToggled.Broadcast(true);
    NotifyInventoryChanged();
}

void UAZ_Inv_CommonUI_InventoryComponent::CloseInventoryMenu()
{
    if (!IsValid(InventoryMenu) || !OwningController.IsValid()) return;
    const bool bWasOpen = bInventoryMenuOpen;
    if (bWasOpen) InventoryMenu->DeactivateWidget();
    InventoryMenu->SetVisibility(ESlateVisibility::Collapsed);
    bInventoryMenuOpen = false;
    FInputModeGameOnly InputMode;
    OwningController->SetInputMode(InputMode);
    OwningController->SetShowMouseCursor(false);
    if (bWasOpen) OnInventoryMenuToggled.Broadcast(false);
}
