#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "InventoryUI/AZ_InventorySnapshot.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AZ_GameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Weapon/AZ_Weapon.h"
#include "Engine/World.h"

bool UAZ_Inv_CommonUI_EquipmentComponent::CanCaptureCampaignState(FString& Error) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bIsProxy || bCommitting || bCampaignRestoring ||
		bPendingSelection || IsSwitchingWeapon() || IsHardBlocked() || IsActionCommitted())
	{ Error = TEXT("Equipment is not in a stable authoritative state."); return false; }
	const auto& T = FAZ_GameplayTags::Get();
	for (const auto& Tag : {T.Ability_State_Shooting, T.Ability_State_Reloading, T.Ability_State_ThrowPreparing,
		T.Ability_State_Throwing, T.Ability_State_MeleeAttacking, T.State_Traversing})
		if (GetASC()->HasMatchingGameplayTag(Tag)) { Error = TEXT("Finish the active action before checkpointing."); return false; }
	return true;
}

bool UAZ_Inv_CommonUI_EquipmentComponent::CaptureCampaignState(FAZ_CampaignEquipmentState& State, FString& Error) const
{
	if (!CanCaptureCampaignState(Error)) return false;
	State.ActiveItemId = Selection.Item ? Selection.Item->GetInstanceId() : FGuid();
	State.IntrinsicSlot = Selection.IntrinsicSlotIndex;
	return true;
}

bool UAZ_Inv_CommonUI_EquipmentComponent::PrepareCampaignRestore(const FAZ_CampaignEquipmentState& State, FString& Error)
{
	if (!CanCaptureCampaignState(Error)) return false;
	if (State.ActiveItemId.IsValid() && State.IntrinsicSlot != INDEX_NONE)
	{ Error = TEXT("Saved equipment has two selections."); return false; }
	auto* Item = State.ActiveItemId.IsValid() && InventoryComponent.IsValid()
		? InventoryComponent->FindCampaignStagedItem(State.ActiveItemId) : nullptr;
	if (State.ActiveItemId.IsValid() && (!Item || Item->GetLocation() != EAZ_InventoryItemLocation::Backpack ||
		!Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_EquipmentFragment>()))
	{ Error = TEXT("Saved equipped item is unavailable."); return false; }
	if (!Item && !ValidateSelection(nullptr, State.IntrinsicSlot))
	{ Error = TEXT("Saved intrinsic equipment is unavailable."); return false; }
	AAZ_Weapon* Weapon = nullptr;
	if (Item && Item->IsWeapon())
	{
		const auto* D = Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
		APawn* Pawn = OwningPlayerController.IsValid() ? OwningPlayerController->GetPawn() : nullptr;
		if (!D || !D->WeaponActorClass || !Item->GetWeaponProfileTag().IsValid() || !Pawn || !OwningSkeletalMesh.IsValid())
		{ Error = TEXT("Saved weapon presentation is unavailable."); return false; }
		Weapon = GetWorld()->SpawnActorDeferred<AAZ_Weapon>(D->WeaponActorClass, Pawn->GetActorTransform(), Pawn, Pawn,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Weapon) { Error = TEXT("Could not stage saved weapon."); return false; }
		Weapon->bSpawnWithCollision = false;
		Weapon->MakeCosmetic();
		Weapon->SetActorHiddenInGame(true);
		Weapon->FinishSpawning(Pawn->GetActorTransform());
		if (IsValid(Weapon) && !Weapon->IsActorBeingDestroyed()) { Weapon->MakeCosmetic(); Weapon->ConfigureFirearmPresentation(*D); }
		if (!IsValid(Weapon) || Weapon->IsActorBeingDestroyed() || !Weapon->GetWeaponMesh3P() ||
			!Weapon->GetWeaponMesh3P()->GetSkeletalMeshAsset() || !OwningSkeletalMesh->DoesSocketExist(Weapon->RelaxedSocketName) ||
			!OwningSkeletalMesh->DoesSocketExist(Weapon->CarrySocketName))
		{ if (IsValid(Weapon)) Weapon->Destroy(); Error = TEXT("Saved weapon mesh/socket contract is invalid."); return false; }
		Weapon->GetWeaponMesh3P()->SetRelativeTransform(FTransform::Identity);
		Weapon->GetWeaponMesh3P()->SetVisibility(true, true);
		if (Weapon->MeshComponent) Weapon->MeshComponent->SetVisibility(false, true);
		if (Weapon->SkeletalMeshComponent) Weapon->SkeletalMeshComponent->SetVisibility(false, true);
		if (!Weapon->AttachToComponent(OwningSkeletalMesh.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Weapon->CarrySocketName))
		{ Weapon->Destroy(); Error = TEXT("Could not stage weapon attachment."); return false; }
	}
	CampaignPreparedItem = Item;
	CampaignPreparedWeapon = Weapon;
	CampaignIntrinsicSlot = State.IntrinsicSlot;
	bCampaignRestoring = true;
	return true;
}

void UAZ_Inv_CommonUI_EquipmentComponent::CancelCampaignRestore()
{
	if (CampaignPreparedWeapon && !bCampaignSelectionCommitted) CampaignPreparedWeapon->Destroy();
	CampaignPreparedWeapon = nullptr; CampaignPreparedItem = nullptr; CampaignIntrinsicSlot = INDEX_NONE;
	bCampaignRestoring = bCampaignSelectionCommitted = false;
}

bool UAZ_Inv_CommonUI_EquipmentComponent::CommitCampaignRestore(FString& Error)
{
	if (!bCampaignRestoring || !ValidateSelection(CampaignPreparedItem, CampaignIntrinsicSlot) || IsHardBlocked() || IsActionCommitted())
	{ Error = TEXT("Equipment owner changed while restoring."); return false; }
	TGuardValue<bool> Guard(bCommitting, true);
	CancelFirearmReady();
	ReleaseActiveSelection();
	DestroyAllPresentations();
	if (CampaignPreparedWeapon)
	{
		auto& P = WeaponPresentations.Add(CampaignPreparedItem->GetInstanceId());
		P.Item = CampaignPreparedItem.Get(); P.Weapon = CampaignPreparedWeapon.Get(); P.HolsterOrder = ++NextHolsterOrder;
	}
	// Commit directly: loading must never schedule a gameplay draw/holster action.
	if (!CommitSelection(CampaignPreparedItem, CampaignIntrinsicSlot))
	{ Error = TEXT("Saved equipment could not commit after staging."); return false; }
	bCampaignSelectionCommitted = true;
	return true;
}

void UAZ_Inv_CommonUI_EquipmentComponent::PublishCampaignRestore(bool bNotify)
{
	check(bCampaignRestoring && bCampaignSelectionCommitted);
	CampaignPreparedItem = nullptr; CampaignPreparedWeapon = nullptr; CampaignIntrinsicSlot = INDEX_NONE;
	bCampaignRestoring = bCampaignSelectionCommitted = false;
	if (bNotify) PublishSelection(nullptr);
}

bool UAZ_Inv_CommonUI_EquipmentComponent::RecoverCampaignSelection(const FAZ_CampaignEquipmentState& State)
{
	CancelCampaignRestore();
	if ((Selection.Item ? Selection.Item->GetInstanceId() : FGuid()) == State.ActiveItemId &&
		Selection.IntrinsicSlotIndex == State.IntrinsicSlot && (!Selection.Item || !Selection.Item->IsWeapon() || IsValid(Selection.Weapon))) return true;
	ReleaseActiveSelection();
	DestroyAllPresentations();
	auto* Item = State.ActiveItemId.IsValid() && InventoryComponent.IsValid() ? InventoryComponent->FindItemById(State.ActiveItemId) : nullptr;
	return (!State.ActiveItemId.IsValid() || Item) && CommitSelection(Item, State.IntrinsicSlot);
}
