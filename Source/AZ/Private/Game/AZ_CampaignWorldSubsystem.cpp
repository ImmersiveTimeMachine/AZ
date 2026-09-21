#include "Game/AZ_CampaignWorldSubsystem.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "Throwables/AZ_ThrowableProjectile.h"
#include "Weapon/AZ_Weapon.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
bool IsEquipmentRepresentation(const UAZ_Inv_CommonUI_ItemComponent* Component)
{
	const AActor* Actor = Component ? Component->GetOwner() : nullptr;
	// Equipment spawns cosmetic weapon actors owned by the pawn. World pickups
	// produced through Manifest::SpawnPickupActor have no pawn owner.
	return Actor && Actor->IsA<AAZ_Weapon>() && Cast<APawn>(Actor->GetOwner());
}
FAZ_InventoryPickupRecord CleanRecord(FAZ_InventoryPickupRecord Record)
{
	if (auto* F = Record.Manifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>()) F->ResetRuntimeState();
	if (auto* F = Record.Manifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_AbilityGrantFragment>()) F->ResetRuntimeState();
	return Record;
}
FAZ_CampaignPickupSnapshot ReadPickup(UAZ_Inv_CommonUI_ItemComponent* Component)
{
	FAZ_CampaignPickupSnapshot R;
	R.WorldId = Component->CampaignPickupId;
	R.bRuntimeSpawned = Component->IsRuntimeCampaignPickup();
	R.ActorClass = Component->GetOwner()->GetClass();
	R.Transform = Component->GetOwner()->GetActorTransform();
	R.Root = CleanRecord(Component->GetRootRecord());
	for (const auto& Child : Component->GetContainedItems()) R.Children.Add(CleanRecord(Child));
	return R;
}
}

void UAZ_CampaignWorldSubsystem::RegisterPickup(UAZ_Inv_CommonUI_ItemComponent* Component)
{
	if (bRestoring || !IsValid(Component) || !Component->GetOwner()->HasAuthority() || IsEquipmentRepresentation(Component)) return;
	if (Component->CampaignPickupId.IsValid())
		for (const auto& Other : LivePickups)
			if (Other.IsValid() && Other.Get() != Component && !Other->GetOwner()->IsActorBeingDestroyed() &&
				Other->CampaignPickupId == Component->CampaignPickupId) AmbiguousIds.Add(Component->CampaignPickupId);
	LivePickups.Add(Component);
	if (Component->CampaignPickupId.IsValid() && !Component->IsRuntimeCampaignPickup())
		AuthoredCatalog.Add(Component->CampaignPickupId, ReadPickup(Component));
}

void UAZ_CampaignWorldSubsystem::PickupRemoved(UAZ_Inv_CommonUI_ItemComponent* Component)
{
	if (bRestoring || !Component || !Component->GetOwner()->HasAuthority() || IsEquipmentRepresentation(Component)) return;
	if (LivePickups.Contains(Component) && !Component->CampaignPickupId.IsValid() && !Component->IsRuntimeCampaignPickup())
		bUnidentifiedAuthoredPickupRemoved = true;
	LivePickups.Remove(Component);
	if (Component->CampaignPickupId.IsValid() && !Component->IsRuntimeCampaignPickup())
	{
		auto Record = ReadPickup(Component);
		Record.bCollected = true;
		AuthoredCatalog.Add(Record.WorldId, MoveTemp(Record));
	}
}

bool UAZ_CampaignWorldSubsystem::SetWorldFact(FName Id, bool Value)
{
	if (Id.IsNone() || GetWorld()->GetNetMode() == NM_Client || bRestoring) return false;
	Facts.Add(Id, Value);
	return true;
}

bool UAZ_CampaignWorldSubsystem::GetWorldFact(FName Id) const { return Facts.FindRef(Id); }

bool UAZ_CampaignWorldSubsystem::Capture(FAZ_CampaignWorldSnapshot& Out, FString& Error) const
{
	if (bUnidentifiedAuthoredPickupRemoved)
	{ Error = TEXT("An authored pickup was removed without CampaignPickupId. Author stable IDs and restart the map before checkpointing."); return false; }
	if (bRestoring || !AmbiguousIds.IsEmpty()) { Error = TEXT("World restore is busy or pickup IDs are ambiguous."); return false; }
	// A projectile temporarily owns an inventory payload. Waiting is safer than
	// omitting it or modifying the independently owned projectile implementation.
	for (TActorIterator<AAZ_ThrowableProjectile> It(GetWorld()); It; ++It)
		if (!It->IsActorBeingDestroyed()) { Error = TEXT("Wait for active thrown objects to resolve before saving."); return false; }
	TMap<FGuid, FAZ_CampaignPickupSnapshot> Records = AuthoredCatalog;
	for (const auto& Weak : LivePickups)
	{
		auto* Component = Weak.Get();
		if (!IsValid(Component) || Component->GetOwner()->IsActorBeingDestroyed()) continue;
		if (!Component->CampaignPickupId.IsValid())
		{ Error = FString::Printf(TEXT("Authored pickup %s needs a persistent CampaignPickupId."), *Component->GetOwner()->GetName()); return false; }
		if (!Component->GetOwner()->GetVelocity().IsNearlyZero(1.f)) { Error = TEXT("Wait for moving pickups to settle."); return false; }
		Records.Add(Component->CampaignPickupId, ReadPickup(Component));
	}
	Out = FAZ_CampaignWorldSnapshot();
	Records.GenerateValueArray(Out.Pickups);
	Out.Facts = Facts;
	return true;
}

bool UAZ_CampaignWorldSubsystem::Validate(const FAZ_CampaignWorldSnapshot& State,
	const FAZ_InventorySnapshot& Inventory, FString& Error) const
{
	const auto Fail = [&Error](const TCHAR* Message) { Error = Message; return false; };
	if (bRestoring || bUnidentifiedAuthoredPickupRemoved || !AmbiguousIds.IsEmpty() || State.Pickups.Num() > 10000 || State.Facts.Num() > 10000)
		return Fail(TEXT("World state is busy, ambiguous or too large."));
	TSet<FGuid> WorldIds, ItemIds, AuthoredIds;
	for (const auto& Item : Inventory.Items) ItemIds.Add(Item.State.InstanceId);
	for (const auto& R : State.Pickups)
	{
		if (!R.WorldId.IsValid() || WorldIds.Contains(R.WorldId) || R.Transform.ContainsNaN() || !R.Transform.IsValid())
			return Fail(TEXT("Invalid world pickup identity/transform."));
		WorldIds.Add(R.WorldId);
		if (!R.bRuntimeSpawned) AuthoredIds.Add(R.WorldId);
		if (R.bCollected) { if (R.bRuntimeSpawned) return Fail(TEXT("Invalid runtime pickup tombstone.")); continue; }
		UClass* Class = R.ActorClass.LoadSynchronous();
		if (!Class || !Class->IsChildOf(AActor::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract) ||
			!R.Root.State.InstanceId.IsValid() || ItemIds.Contains(R.Root.State.InstanceId) || R.Root.StackCount <= 0 ||
			!R.Root.Manifest.GetItemTypeTag().IsValid() || R.Root.State.Location != EAZ_InventoryItemLocation::World ||
			R.Root.State.ParentItemId.IsValid() || (!R.Root.Manifest.IsStackable() && R.Root.StackCount != 1))
			return Fail(TEXT("Saved pickup payload is invalid or also owned by inventory."));
		ItemIds.Add(R.Root.State.InstanceId);
		const auto* Weapon = R.Root.Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
		if (R.Children.Num() > 1 || (R.Children.IsEmpty() != !R.Root.State.InsertedMagazineId.IsValid()))
			return Fail(TEXT("Saved pickup has invalid magazine links."));
		for (const auto& Child : R.Children)
		{
			const auto* Mag = Child.Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_MagazineFragment>();
			if (!Child.State.InstanceId.IsValid() || ItemIds.Contains(Child.State.InstanceId) || !Weapon || !Weapon->bUsesDetachableMagazines ||
				!Mag || Child.StackCount != 1 || Child.State.Location != EAZ_InventoryItemLocation::WeaponMagazine ||
				Child.State.ParentItemId != R.Root.State.InstanceId || R.Root.State.InsertedMagazineId != Child.State.InstanceId ||
				Child.State.InsertedMagazineId.IsValid() || Weapon->MagazineFamily != Mag->MagazineFamily || Mag->Capacity <= 0 ||
				Child.State.CurrentRounds < 0 || Child.State.CurrentRounds > Mag->Capacity || Child.State.AmmoRevision < 0)
				return Fail(TEXT("Saved world magazine state is invalid."));
			ItemIds.Add(Child.State.InstanceId);
		}
		if (const auto* Mag = R.Root.Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_MagazineFragment>(); Mag &&
			(Mag->Capacity <= 0 || R.Root.State.CurrentRounds < 0 || R.Root.State.CurrentRounds > Mag->Capacity || R.Root.State.AmmoRevision < 0))
			return Fail(TEXT("Saved loose magazine state is invalid."));
	}
	// The supported map must be fully loaded with the same authored pickup IDs.
	// Do not silently apply a save from a different layout/streaming generation.
	if (AuthoredIds.Num() != AuthoredCatalog.Num()) return Fail(TEXT("Authored pickup layout differs from the save; load the matching map."));
	for (const auto& Pair : AuthoredCatalog) if (!AuthoredIds.Contains(Pair.Key)) return Fail(TEXT("Authored pickup ID is missing from save."));
	for (const auto& Weak : LivePickups)
		if (Weak.IsValid() && !Weak->CampaignPickupId.IsValid()) return Fail(TEXT("Current map has an unidentified pickup."));
	for (const auto& Pair : State.Facts) if (Pair.Key.IsNone()) return Fail(TEXT("Invalid world fact ID."));
	return true;
}

bool UAZ_CampaignWorldSubsystem::PrepareRestore(const FAZ_CampaignWorldSnapshot& State, FString& Error)
{
	if (bRestoring) { Error = TEXT("World restore is busy."); return false; }
	bRestoring = true;
	PendingState = State;
	for (const auto& R : State.Pickups)
	{
		if (R.bCollected) continue;
		UAZ_Inv_CommonUI_ItemComponent* Existing = nullptr;
		for (const auto& Weak : LivePickups)
			if (Weak.IsValid() && Weak->CampaignPickupId == R.WorldId && !Weak->GetOwner()->IsActorBeingDestroyed()) { Existing = Weak.Get(); break; }
		if (Existing)
		{
			if (Existing->GetOwner()->GetClass() != R.ActorClass.Get())
			{ Error = TEXT("Saved pickup class differs from current actor."); CancelRestore(); return false; }
			RestoreTargets.Add(R.WorldId, Existing);
			continue;
		}
		AActor* Actor = GetWorld()->SpawnActorDeferred<AActor>(R.ActorClass.Get(), R.Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Actor) { Error = TEXT("Could not stage saved world pickup."); CancelRestore(); return false; }
		StagedActors.Add(Actor);
		Actor->SetActorHiddenInGame(true); Actor->SetActorEnableCollision(false);
		Actor->FinishSpawning(R.Transform);
		auto* Component = IsValid(Actor) ? Actor->FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>() : nullptr;
		if (!Component || Actor->IsActorBeingDestroyed())
		{ Error = TEXT("Saved pickup actor lacks its inventory payload component."); CancelRestore(); return false; }
		Actor->SetActorHiddenInGame(true); Actor->SetActorEnableCollision(false);
		RestoreTargets.Add(R.WorldId, Component);
	}
	return true;
}

void UAZ_CampaignWorldSubsystem::CancelRestore()
{
	for (const auto& Actor : StagedActors) if (IsValid(Actor)) Actor->Destroy();
	StagedActors.Reset(); RestoreTargets.Reset(); PendingState = FAZ_CampaignWorldSnapshot();
	bRestoring = false;
}

void UAZ_CampaignWorldSubsystem::CommitRestore()
{
	check(bRestoring);
	for (const auto& Weak : LivePickups)
		if (Weak.IsValid() && !RestoreTargets.Contains(Weak->CampaignPickupId)) Weak->GetOwner()->Destroy();
	LivePickups.Reset(); AuthoredCatalog.Reset(); AmbiguousIds.Reset();
	for (const auto& R : PendingState.Pickups)
	{
		if (!R.bRuntimeSpawned) AuthoredCatalog.Add(R.WorldId, R);
		if (R.bCollected) continue;
		auto* Component = RestoreTargets.FindChecked(R.WorldId).Get();
		Component->RestoreCampaignPayload(R.WorldId, R.bRuntimeSpawned, R.Root, R.Children);
		Component->GetOwner()->SetActorTransform(R.Transform, false, nullptr, ETeleportType::TeleportPhysics);
		Component->GetOwner()->SetActorHiddenInGame(false); Component->GetOwner()->SetActorEnableCollision(true);
		LivePickups.Add(Component);
	}
	Facts = MoveTemp(PendingState.Facts);
	StagedActors.Reset(); RestoreTargets.Reset(); PendingState = FAZ_CampaignWorldSnapshot();
	bRestoring = false;
}
