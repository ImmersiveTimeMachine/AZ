#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/AZ_GA_FirearmFire.h"
#include "AbilitySystem/Abilities/AZ_GA_FirearmReload.h"
#include "Animation/AZ_WeaponAnimationProfile.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AZ_GameplayTags.h"
#include "Character/AZ_HeroPawn.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "Inventory/AZ_QuickBarComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "Net/UnrealNetwork.h"
#include "Player/AZ_PlayerController.h"
#include "TimerManager.h"
#include "Weapon/AZ_Weapon.h"

namespace
{
	FGameplayTagContainer EquipmentBlockTags()
	{
		const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
		FGameplayTagContainer Result;
		Result.AddTag(Tags.Character_Dead);
		Result.AddTag(Tags.Character_Dying);
		Result.AddTag(Tags.State_Grabbed);
		Result.AddTag(Tags.State_Combat_Grabbing);
		Result.AddTag(Tags.State_Combat_Staggered);
		Result.AddTag(Tags.State_Combat_StruckPair);
		return Result;
	}
}

UAZ_Inv_CommonUI_EquipmentComponent::UAZ_Inv_CommonUI_EquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UAZ_Inv_CommonUI_EquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAZ_Inv_CommonUI_EquipmentComponent, Selection);
}

UAZ_AbilitySystemComponent* UAZ_Inv_CommonUI_EquipmentComponent::GetASC() const
{
	if (!OwningPlayerController.IsValid()) return nullptr;
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningPlayerController->GetPawn());
	if (!ASC) ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningPlayerController->PlayerState);
	return Cast<UAZ_AbilitySystemComponent>(ASC);
}

bool UAZ_Inv_CommonUI_EquipmentComponent::IsHardBlocked() const
{
	const UAZ_AbilitySystemComponent* ASC = GetASC();
	return !ASC || ASC->HasAnyMatchingGameplayTags(EquipmentBlockTags());
}

UAZ_WeaponAnimationProfile* UAZ_Inv_CommonUI_EquipmentComponent::GetActiveAnimationProfile() const
{
	const auto* WeaponState = IsValid(Selection.Item)
		? Selection.Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>() : nullptr;
	return WeaponState ? WeaponState->AnimationProfile.Get() : nullptr;
}

bool UAZ_Inv_CommonUI_EquipmentComponent::TryGetDrawAnimationPresentation(
	UAZ_WeaponAnimationProfile*& OutProfile, FGameplayTag& OutWeaponTag) const
{
	OutProfile = nullptr;
	OutWeaponTag = FGameplayTag();
	if (!WeaponTransition.PhaseId.IsValid() || WeaponTransition.bHolsterPhase
		|| !WeaponTransition.bSocketApplied || !IsSwitchContextValid()) return false;
	const UAZ_Inv_CommonUI_InventoryItem* Item = WeaponTransition.TargetItem.Get();
	const auto* WeaponState = IsValid(Item) && Item->IsWeapon()
		? Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>() : nullptr;
	if (!WeaponState || !IsValid(WeaponState->AnimationProfile) || !Item->GetWeaponProfileTag().IsValid()) return false;
	// Selection, ability ownership and movement speeds remain committed to the
	// outgoing item. Only the AnimInstance's copied chooser inputs use this view.
	OutProfile = WeaponState->AnimationProfile.Get();
	OutWeaponTag = Item->GetWeaponProfileTag();
	return true;
}

bool UAZ_Inv_CommonUI_EquipmentComponent::IsActiveWeaponSource(const UObject* Source) const
{
	return !bIsProxy && !bCommitting && !IsSwitchingWeapon() && !IsHardBlocked() && IsValid(Selection.Item)
		&& Selection.Item->IsInitialized() && Selection.Item->IsWeapon() && IsValid(Selection.Weapon)
		&& Source == Selection.Weapon && InventoryComponent.IsValid() && InventoryComponent->ContainsItem(Selection.Item)
		&& Selection.Profile.IsValid() && Selection.Profile == Selection.Item->GetWeaponProfileTag()
		&& OwningPlayerController.IsValid() && Selection.Weapon->GetOwner() == OwningPlayerController->GetPawn();
}

bool UAZ_Inv_CommonUI_EquipmentComponent::IsSwitchingWeapon() const
{
	const UAZ_AbilitySystemComponent* ASC = GetASC();
	return WeaponTransition.Id.IsValid()
		|| (ASC && ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Ability_State_WeaponSwitching));
}

void UAZ_Inv_CommonUI_EquipmentComponent::CancelActiveAim()
{
	CancelFirearmReady();
	if (IsValid(Selection.Weapon)) Selection.Weapon->StopFirearmAnimation();
	UAZ_AbilitySystemComponent* ASC = BoundASC.IsValid() ? BoundASC.Get() : GetASC();
	if (!ASC) return;
	const FGameplayTag AimInput = FAZ_GameplayTags::Get().Input_Action_Aim;
	FGameplayTagContainer InputTags(AimInput);
	InputTags.AddTag(FAZ_GameplayTags::Get().Input_Action_Reload);
	// Menu/hard interrupts also terminate a firearm waiting for release. Keep fist
	// actions out of this cancellation path even though they share primary input.
	if (Selection.Item && Selection.Item->IsWeapon()) InputTags.AddTag(FAZ_GameplayTags::Get().Input_Action_PrimaryAttack);
	ASC->ClearWeaponInput(InputTags);
	if (OwningPlayerController.IsValid() && OwningPlayerController->HasAuthority() && !OwningPlayerController->IsLocalController())
	{
		Client_ClearOutgoingInput(InputTags);
	}
	TArray<FGameplayAbilitySpecHandle> AimHandles;
	{
		FScopedAbilityListLock AbilityLock(*ASC);
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.IsActive() && (Spec.GetDynamicSpecSourceTags().HasTagExact(AimInput)
				|| (Spec.Ability && (Spec.Ability->IsA<UAZ_GA_FirearmFire>()
					|| Spec.Ability->IsA<UAZ_GA_FirearmReload>())))) AimHandles.Add(Spec.Handle);
		}
	}
	for (const FGameplayAbilitySpecHandle& Handle : AimHandles) ASC->CancelAbilityHandle(Handle);
	if (IsValid(Selection.Weapon)) Selection.Weapon->StopReloadAnimation();
}

bool UAZ_Inv_CommonUI_EquipmentComponent::RequestReloadIfEmpty()
{
	if (!IsValid(Selection.Item) || !Selection.Item->IsInitialized() || !InventoryComponent.IsValid()) return false;
	const FAZ_WeaponAmmoSnapshot Ammo = InventoryComponent->GetWeaponAmmoSnapshot(Selection.Item->GetInstanceId());
	return RequestReloadIfEmpty(Selection.Weapon, Selection.Item->GetInstanceId(), Selection.Generation,
		Ammo.MagazineItemId, Ammo.AmmoRevision);
}

bool UAZ_Inv_CommonUI_EquipmentComponent::RequestReloadIfEmpty(AAZ_Weapon* ExpectedSource,
	const FGuid& ExpectedItemId, uint32 ExpectedGeneration, const FGuid& ExpectedMagazineId, int64 ExpectedAmmoRevision)
{
	const AAZ_PlayerController* Player = Cast<AAZ_PlayerController>(OwningPlayerController.Get());
	UAZ_AbilitySystemComponent* ASC = GetASC();
	if (!GetOwner() || !Player || Player->IsInventoryInputCaptured() || !ASC || !InventoryComponent.IsValid()
		|| !IsActiveWeaponSource(ExpectedSource) || Selection.Generation != ExpectedGeneration
		|| !IsValid(Selection.Item) || Selection.Item->GetInstanceId() != ExpectedItemId
		|| ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Ability_State_Reloading)) return false;
	const FAZ_WeaponAmmoSnapshot Ammo = InventoryComponent->GetWeaponAmmoSnapshot(ExpectedItemId);
	if ((Ammo.MagazineState != EAZ_WeaponMagazineState::Empty && Ammo.MagazineState != EAZ_WeaponMagazineState::NoMagazine)
		|| Ammo.MagazineItemId != ExpectedMagazineId || Ammo.AmmoRevision != ExpectedAmmoRevision
		|| !InventoryComponent->CanReloadMagazine(ExpectedSource, ExpectedItemId, ExpectedGeneration, FGuid(), true)) return false;
	if (!GetOwner()->HasAuthority())
	{
		// The empty receipt must cross the network with its skip-empty policy.
		// Ordinary GAS R activation intentionally has the manual, include-empty default.
		Server_RequestReloadIfEmpty(ExpectedSource, ExpectedItemId, ExpectedGeneration, ExpectedMagazineId, ExpectedAmmoRevision);
		return true;
	}
	return RequestMagazineReload(ExpectedSource, ExpectedItemId, ExpectedGeneration, FGuid(), true);
}

void UAZ_Inv_CommonUI_EquipmentComponent::Server_RequestReloadIfEmpty_Implementation(AAZ_Weapon* WeaponSource,
	FGuid ItemId, uint32 Generation, FGuid ExpectedMagazineId, int64 ExpectedAmmoRevision)
{
	RequestReloadIfEmpty(WeaponSource, ItemId, Generation, ExpectedMagazineId, ExpectedAmmoRevision);
}

bool UAZ_Inv_CommonUI_EquipmentComponent::RequestMagazineReload(AAZ_Weapon* ExpectedSource,
	const FGuid& ExpectedItemId, uint32 ExpectedGeneration, const FGuid& RequestedMagazineId, bool bSkipEmpty)
{
	const AAZ_PlayerController* Player = Cast<AAZ_PlayerController>(OwningPlayerController.Get());
	UAZ_AbilitySystemComponent* ASC = GetASC();
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Player || Player->IsInventoryInputCaptured()
		|| !ASC || !InventoryComponent.IsValid() || !IsActiveWeaponSource(ExpectedSource)
		|| Selection.Generation != ExpectedGeneration || !IsValid(Selection.Item)
		|| Selection.Item->GetInstanceId() != ExpectedItemId
		|| ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Ability_State_Reloading)
		|| !InventoryComponent->CanReloadMagazine(ExpectedSource, ExpectedItemId, ExpectedGeneration, RequestedMagazineId, bSkipEmpty)) return false;
	FGameplayAbilitySpecHandle ReloadHandle;
	TWeakObjectPtr<UAZ_GA_FirearmReload> ReloadAbility;
	{
		FScopedAbilityListLock AbilityLock(*ASC);
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->IsA<UAZ_GA_FirearmReload>()
				&& Spec.SourceObject.Get() == ExpectedSource && !Spec.IsActive())
			{
				ReloadHandle = Spec.Handle;
				ReloadAbility = Cast<UAZ_GA_FirearmReload>(Spec.GetPrimaryInstance());
				break;
			}
		}
	}
	return ReloadHandle.IsValid() && ReloadAbility.IsValid()
		&& ReloadAbility->TryActivateMagazineRequest(ASC, ReloadHandle, ExpectedSource, ExpectedItemId,
			ExpectedGeneration, RequestedMagazineId, bSkipEmpty);
}

void UAZ_Inv_CommonUI_EquipmentComponent::CancelActiveFire()
{
	if (IsValid(Selection.Weapon)) Selection.Weapon->StopFirearmAnimation();
	UAZ_AbilitySystemComponent* ASC = BoundASC.IsValid() ? BoundASC.Get() : GetASC();
	if (!ASC) return;
	const FGameplayTagContainer InputTags(FAZ_GameplayTags::Get().Input_Action_PrimaryAttack);
	ASC->ClearWeaponInput(InputTags);
	if (OwningPlayerController.IsValid() && OwningPlayerController->HasAuthority() && !OwningPlayerController->IsLocalController())
	{
		Client_ClearOutgoingInput(InputTags);
	}
	TArray<FGameplayAbilitySpecHandle> FireHandles;
	{
		FScopedAbilityListLock AbilityLock(*ASC);
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.IsActive() && Spec.Ability && Spec.Ability->IsA<UAZ_GA_FirearmFire>()) FireHandles.Add(Spec.Handle);
		}
	}
	for (const FGameplayAbilitySpecHandle& Handle : FireHandles) ASC->CancelAbilityHandle(Handle);
}

bool UAZ_Inv_CommonUI_EquipmentComponent::ValidateFireModeRequest(const AAZ_Weapon* WeaponSource, const FGuid& ItemId,
	uint32 Generation, int64 ExpectedRevision, EAZ_FirearmFireMode NewMode) const
{
	const AAZ_PlayerController* Controller = Cast<AAZ_PlayerController>(OwningPlayerController.Get());
	const UAZ_AbilitySystemComponent* ASC = GetASC();
	if (!Controller || Controller->IsInventoryInputCaptured() || !ASC || bPendingSelection
		|| IsActionCommitted() || ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Ability_State_Reloading)
		|| !IsActiveWeaponSource(WeaponSource) || Selection.Generation != Generation || !InventoryComponent.IsValid()
		|| !Selection.Item || Selection.Item->GetInstanceId() != ItemId
		|| ExpectedRevision < 0 || ExpectedRevision == MAX_int64 || Selection.Item->GetFireModeRevision() != ExpectedRevision
		|| Selection.Item->GetSelectedFireMode() == NewMode) return false;
	const auto* Definition = Selection.Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
	return Definition && Definition->IsFireModeSupported(Selection.Item->GetSelectedFireMode()) && Definition->IsFireModeSupported(NewMode);
}

bool UAZ_Inv_CommonUI_EquipmentComponent::RequestCycleFireMode()
{
	if (!IsValid(Selection.Item) || !Selection.Item->IsInitialized()) return false;
	const auto* Definition = Selection.Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
	const EAZ_FirearmFireMode Current = Selection.Item->GetSelectedFireMode();
	if (!Definition || !Definition->IsFireModeSupported(Current)) return false;
	const int32 CurrentIndex = Definition->SupportedFireModes.IndexOfByKey(Current);
	EAZ_FirearmFireMode Next = Current;
	for (int32 Offset = 1; Offset < Definition->SupportedFireModes.Num(); ++Offset)
	{
		const EAZ_FirearmFireMode Candidate = Definition->SupportedFireModes[(CurrentIndex + Offset) % Definition->SupportedFireModes.Num()];
		if (Candidate != Current && Definition->IsFireModeSupported(Candidate)) { Next = Candidate; break; }
	}
	AAZ_Weapon* WeaponSource = Selection.Weapon;
	const FGuid ItemId = Selection.Item->GetInstanceId();
	const uint32 Generation = Selection.Generation;
	const int64 Revision = Selection.Item->GetFireModeRevision();
	if (!ValidateFireModeRequest(WeaponSource, ItemId, Generation, Revision, Next)) return false;
	if (GetOwner()->HasAuthority()) return CommitFireModeChange(WeaponSource, ItemId, Generation, Revision, Next);

	// Stop local held-fire presentation immediately, but only authority changes the mode.
	CancelActiveFire();
	if (!ValidateFireModeRequest(WeaponSource, ItemId, Generation, Revision, Next)) return false;
	Server_RequestFireMode(WeaponSource, ItemId, Generation, Revision, Next);
	return true;
}

bool UAZ_Inv_CommonUI_EquipmentComponent::CommitFireModeChange(AAZ_Weapon* WeaponSource, const FGuid& ItemId,
	uint32 Generation, int64 ExpectedRevision, EAZ_FirearmFireMode NewMode)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !ValidateFireModeRequest(WeaponSource, ItemId, Generation, ExpectedRevision, NewMode)) return false;
	CancelActiveFire();
	// Cancellation delegates may change selection or permissions; never mutate the captured item blindly.
	if (!ValidateFireModeRequest(WeaponSource, ItemId, Generation, ExpectedRevision, NewMode)) return false;
	return InventoryComponent->TrySetWeaponFireMode(WeaponSource, ItemId, Generation, ExpectedRevision, NewMode);
}

void UAZ_Inv_CommonUI_EquipmentComponent::Server_RequestFireMode_Implementation(AAZ_Weapon* WeaponSource, FGuid ItemId,
	uint32 Generation, int64 ExpectedRevision, EAZ_FirearmFireMode NewMode)
{
	CommitFireModeChange(WeaponSource, ItemId, Generation, ExpectedRevision, NewMode);
}

bool UAZ_Inv_CommonUI_EquipmentComponent::IsActionCommitted() const
{
	UAZ_AbilitySystemComponent* ASC = GetASC();
	if (!ASC) return true;
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	if (ASC->HasMatchingGameplayTag(Tags.Ability_State_MeleeAttacking)
		&& !ASC->HasMatchingGameplayTag(Tags.State_Combat_CancelWindow)) return true;
	// Future weapon actions may expose an engine cancellation lock as well as the melee beat tag.
	for (const FGameplayAbilitySpecHandle& Handle : GrantedHandles)
	{
		if (const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle))
		{
			for (const UGameplayAbility* Ability : Spec->GetAbilityInstances())
			{
				if (Ability && Ability->IsActive() && !Ability->CanBeCanceled()) return true;
			}
		}
	}
	return false;
}

bool UAZ_Inv_CommonUI_EquipmentComponent::ValidateSelection(UAZ_Inv_CommonUI_InventoryItem* Item, int32 IntrinsicSlot) const
{
	if (bIsProxy || !OwningPlayerController.IsValid() || !OwningPlayerController->GetPawn() || !GetASC()) return false;
	if (Item)
	{
		if (!InventoryComponent.IsValid() || !InventoryComponent->ContainsItem(Item)
			|| Item->GetLocation() != EAZ_InventoryItemLocation::Backpack) return false;
		const auto* Equipment = Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_EquipmentFragment>();
		const auto* Weapon = Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
		if (!Equipment) return false;
		// Preserve the old inventory's modifier/ability-only equipment. It never spawned the
		// EquipmentFragment's generic prop class; a physical weapon has the stricter contract below.
		if (!Weapon) return true;
		if (!Weapon->WeaponActorClass || !Item->GetWeaponProfileTag().IsValid()
			|| !Weapon->WeaponActorClass->IsChildOf(AAZ_Weapon::StaticClass()) || !OwningSkeletalMesh.IsValid()) return false;
		const AAZ_Weapon* Defaults = Cast<AAZ_Weapon>(Weapon->WeaponActorClass->GetDefaultObject());
		if (!Defaults) return false;
		const bool bHasSockets = OwningSkeletalMesh->DoesSocketExist(Defaults->RelaxedSocketName)
			&& OwningSkeletalMesh->DoesSocketExist(Defaults->CarrySocketName);
		if (!bHasSockets)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Equipment] required body sockets missing mesh=%s hand=%s carry=%s"),
				*GetNameSafe(OwningSkeletalMesh->GetSkeletalMeshAsset()), *Defaults->RelaxedSocketName.ToString(), *Defaults->CarrySocketName.ToString());
		}
		return bHasSockets;
	}
	if (IntrinsicSlot != INDEX_NONE)
	{
		const UAZ_QuickBarComponent* QuickBar = GetOwner()->FindComponentByClass<UAZ_QuickBarComponent>();
		const FAZ_QuickSlot* Slot = QuickBar ? QuickBar->GetSlotDefinition(IntrinsicSlot) : nullptr;
		return Slot && Slot->bEnabled && !Slot->bInventoryBacked && Slot->WeaponTag.IsValid();
	}
	return true;
}

bool UAZ_Inv_CommonUI_EquipmentComponent::RequestEquipItem(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	if (!IsValid(Item)) return false;
	if (!GetOwner()->HasAuthority())
	{
		Server_RequestItem(Item->GetInstanceId(), true);
		return true;
	}
	return RequestSelection(Item, INDEX_NONE);
}

bool UAZ_Inv_CommonUI_EquipmentComponent::RequestEquipIntrinsic(int32 QuickSlotIndex)
{
	if (!GetOwner()->HasAuthority())
	{
		Server_RequestIntrinsic(QuickSlotIndex);
		return true;
	}
	return RequestSelection(nullptr, QuickSlotIndex);
}

bool UAZ_Inv_CommonUI_EquipmentComponent::RequestUnequipItem(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	if (Item && Item != Selection.Item) return false;
	if (!GetOwner()->HasAuthority())
	{
		Server_RequestItem(Item ? Item->GetInstanceId() : FGuid(), false);
		return true;
	}
	return RequestSelection(nullptr, INDEX_NONE);
}

void UAZ_Inv_CommonUI_EquipmentComponent::Server_RequestItem_Implementation(FGuid ItemId, bool bEquip)
{
	UAZ_Inv_CommonUI_InventoryItem* Item = InventoryComponent.IsValid() ? InventoryComponent->FindItemById(ItemId) : nullptr;
	if (bEquip) RequestEquipItem(Item);
	else if (!ItemId.IsValid() || Item) RequestUnequipItem(Item);
}

void UAZ_Inv_CommonUI_EquipmentComponent::Server_RequestIntrinsic_Implementation(int32 QuickSlotIndex)
{
	RequestSelection(nullptr, QuickSlotIndex);
}

void UAZ_Inv_CommonUI_EquipmentComponent::ClearOutgoingInput()
{
	CancelFirearmReady();
	if (IsValid(Selection.Weapon)) Selection.Weapon->StopFirearmAnimation();
	if (IsValid(Selection.Weapon)) Selection.Weapon->StopReloadAnimation();
	UAZ_AbilitySystemComponent* ASC = BoundASC.IsValid() ? BoundASC.Get() : GetASC();
	if (!ASC) return;
	FGameplayTagContainer InputTags;
	for (const FGameplayAbilitySpecHandle& Handle : GrantedHandles)
	{
		if (const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle))
		{
			if (const UAZ_GameplayAbility* Ability = Cast<UAZ_GameplayAbility>(Spec->Ability)) InputTags.AddTag(Ability->InputTag);
		}
	}
	ASC->ClearWeaponInput(InputTags);
	// Held input and the melee buffer live on the owning client. Send the authority's
	// exact outgoing tags before its grants disappear; a listen host already cleared locally.
	if (!InputTags.IsEmpty() && OwningPlayerController.IsValid()
		&& OwningPlayerController->HasAuthority() && !OwningPlayerController->IsLocalController())
	{
		Client_ClearOutgoingInput(InputTags);
	}
}

void UAZ_Inv_CommonUI_EquipmentComponent::Client_ClearOutgoingInput_Implementation(const FGameplayTagContainer& InputTags)
{
	// Primary-only clears also service reload/fire-mode changes, which retain Ready.
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	if (InputTags.HasTagExact(Tags.Input_Action_Aim) || InputTags.HasTagExact(Tags.Input_Action_Reload)) CancelFirearmReady();
	if (UAZ_AbilitySystemComponent* ASC = GetASC()) ASC->ClearWeaponInput(InputTags);
}

void UAZ_Inv_CommonUI_EquipmentComponent::RequestTrackedSelection(UAZ_Inv_CommonUI_InventoryItem* Item,
	int32 IntrinsicSlot, const FGuid& RequestId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !RequestId.IsValid()) return;
	RequestSelection(Item, IntrinsicSlot, RequestId);
}

void UAZ_Inv_CommonUI_EquipmentComponent::CancelPendingSelectionRequest()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		TGuardValue<bool> CommitGuard(bCommitting, true);
		ClearPendingSelection(EAZ_EquipmentRequestOutcome::Superseded);
		CancelWeaponSwitch(EAZ_EquipmentRequestOutcome::Superseded);
	}
}

void UAZ_Inv_CommonUI_EquipmentComponent::FinishTrackedRequest(const FGuid& RequestId,
	EAZ_EquipmentRequestOutcome Outcome, const FText& Reason)
{
	if (RequestId.IsValid()) OnTrackedRequestResult.Broadcast(RequestId, Outcome, Reason);
}

bool UAZ_Inv_CommonUI_EquipmentComponent::RequestSelection(UAZ_Inv_CommonUI_InventoryItem* Item, int32 IntrinsicSlot,
	const FGuid& RequestId)
{
	BindAbilityEvents();
	if (bCommitting || IsHardBlocked() || !ValidateSelection(Item, IntrinsicSlot))
	{
		UE_LOG(LogTemp, Display, TEXT("[Equipment] request refused item=%s slot=%d (ownership/profile/socket/body gate)"), *GetNameSafe(Item), IntrinsicSlot);
		FinishTrackedRequest(RequestId, EAZ_EquipmentRequestOutcome::Rejected,
			NSLOCTEXT("AZEquipment", "SelectionUnavailable", "This equipment cannot be selected right now."));
		return false;
	}
	// Supersede under a guard: a receipt delegate may synchronously try another request.
	{
		TGuardValue<bool> CommitGuard(bCommitting, true);
		ClearPendingSelection(EAZ_EquipmentRequestOutcome::Superseded);
		CancelWeaponSwitch(EAZ_EquipmentRequestOutcome::Superseded);
	}
	if (Selection.Item == Item && Selection.IntrinsicSlotIndex == IntrinsicSlot)
	{
		ClearPendingSelection(EAZ_EquipmentRequestOutcome::Superseded);
		FinishTrackedRequest(RequestId, EAZ_EquipmentRequestOutcome::Activated);
		return true;
	}
	{
		TGuardValue<bool> CommitGuard(bCommitting, true);
		ClearOutgoingInput();
	}
	if (IsActionCommitted())
	{
		PendingItem = Item;
		PendingRequestId = RequestId;
		PendingIntrinsicSlot = IntrinsicSlot;
		bPendingWasItem = Item != nullptr;
		bPendingSelection = true;
		UE_LOG(LogTemp, Display, TEXT("[Equipment] queued latest selection item=%s slot=%d until action recovery"), *GetNameSafe(Item), IntrinsicSlot);
		FinishTrackedRequest(RequestId, EAZ_EquipmentRequestOutcome::Deferred,
			NSLOCTEXT("AZEquipment", "SelectionDeferred", "Switching after the current action."));
		return true;
	}
	return BeginSelectionChange(Item, IntrinsicSlot, RequestId);
}

bool UAZ_Inv_CommonUI_EquipmentComponent::BuildSwitchPhase(UAZ_Inv_CommonUI_InventoryItem* Item,
	bool bDraw, bool bCrouching, FWeaponSwitchPhase& Out) const
{
	Out = FWeaponSwitchPhase();
	if (!IsValid(Item) || !Item->IsWeapon()) return true;
	const auto* Definition = Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
	const UAZ_WeaponAnimationProfile* Profile = Definition ? Definition->AnimationProfile.Get() : nullptr;
	if (!Profile || !OwningSkeletalMesh.IsValid()) return false;
	const FAZ_WeaponSwitchAnimation& Standing = bDraw ? Profile->StandingDraw : Profile->StandingHolster;
	const FAZ_WeaponSwitchAnimation& Crouching = bDraw ? Profile->CrouchingDraw : Profile->CrouchingHolster;
	// A missing crouch clip deliberately uses the profile's authored standing action.
	const FAZ_WeaponSwitchAnimation& Action = bCrouching && IsValid(Crouching.Animation) ? Crouching : Standing;
	UAnimSequence* Sequence = Action.Animation;
	const USkeletalMesh* Mesh = OwningSkeletalMesh->GetSkeletalMeshAsset();
	const USkeleton* MeshSkeleton = Mesh ? Mesh->GetSkeleton() : nullptr;
	const USkeleton* SequenceSkeleton = Sequence ? Sequence->GetSkeleton() : nullptr;
	const double Rate = Sequence ? static_cast<double>(Sequence->RateScale) * Profile->SwitchAnimationPlayRate : 0.;
	if (!IsValid(Sequence) || Sequence->GetAdditiveAnimType() != AAT_None || Sequence->HasRootMotion()
		|| !MeshSkeleton || !SequenceSkeleton || !SequenceSkeleton->IsCompatibleMesh(Mesh)
		|| Profile->SwitchAnimationSlot.IsNone()
		|| SequenceSkeleton->GetSlotGroupName(Profile->SwitchAnimationSlot) != FName(TEXT("WeaponFire"))
		|| MeshSkeleton->GetSlotGroupName(Profile->SwitchAnimationSlot) != FName(TEXT("WeaponFire"))
		|| !FMath::IsFinite(Sequence->GetPlayLength()) || Sequence->GetPlayLength() <= 0.
		|| !FMath::IsFinite(Sequence->RateScale) || Sequence->RateScale <= 0.f
		|| !FMath::IsFinite(Profile->SwitchAnimationPlayRate) || Profile->SwitchAnimationPlayRate <= 0.f
		|| !FMath::IsFinite(Rate) || Rate <= 0.
		|| !FMath::IsFinite(Action.AttachTime) || Action.AttachTime < 0.f || Action.AttachTime > Sequence->GetPlayLength()
		|| !FMath::IsFinite(Profile->SwitchAnimationBlendIn) || Profile->SwitchAnimationBlendIn < 0.f
		|| !FMath::IsFinite(Profile->SwitchAnimationBlendOut) || Profile->SwitchAnimationBlendOut < 0.f
		|| !FMath::IsFinite(Profile->SwitchSocketBlendDuration) || Profile->SwitchSocketBlendDuration < 0.f) return false;
	Out.Animation = Sequence;
	Out.Slot = Profile->SwitchAnimationSlot;
	Out.PlayRate = Profile->SwitchAnimationPlayRate;
	Out.BlendIn = Profile->SwitchAnimationBlendIn;
	Out.BlendOut = Profile->SwitchAnimationBlendOut;
	Out.SocketBlendDuration = Profile->SwitchSocketBlendDuration;
	Out.AttachTime = Action.AttachTime / Rate;
	// Gameplay must remain blocked until the visual transfer also reaches its socket.
	Out.Duration = FMath::Max(Sequence->GetPlayLength() / Rate, Out.AttachTime + Out.SocketBlendDuration);
	return FMath::IsFinite(Out.Duration) && Out.Duration > UE_KINDA_SMALL_NUMBER;
}

bool UAZ_Inv_CommonUI_EquipmentComponent::BeginSelectionChange(UAZ_Inv_CommonUI_InventoryItem* Item,
	int32 IntrinsicSlot, const FGuid& RequestId)
{
	if (bCommitting || IsHardBlocked() || IsActionCommitted() || !ValidateSelection(Item, IntrinsicSlot))
	{
		FinishTrackedRequest(RequestId, EAZ_EquipmentRequestOutcome::Rejected);
		return false;
	}
	const APawn* Pawn = OwningPlayerController->GetPawn();
	const UCharacterMoverComponent* Mover = Pawn ? Pawn->FindComponentByClass<UCharacterMoverComponent>() : nullptr;
	const bool bCrouching = Mover && Mover->IsCrouching();
	FWeaponSwitchPhase Holster, Draw;
	const bool bAlreadyCarried = IsValid(Selection.Weapon) && Selection.Weapon->GetRootComponent()
		&& Selection.Weapon->GetRootComponent()->GetAttachParent() == OwningSkeletalMesh.Get()
		&& Selection.Weapon->GetRootComponent()->GetAttachSocketName() == Selection.Weapon->CarrySocketName;
	if ((!bAlreadyCarried && !BuildSwitchPhase(Selection.Item, false, bCrouching, Holster))
		|| !BuildSwitchPhase(Item, true, bCrouching, Draw))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Equipment] switch rejected: missing/invalid action profile source=%s target=%s"),
			*GetNameSafe(Selection.Item), *GetNameSafe(Item));
		FinishTrackedRequest(RequestId, EAZ_EquipmentRequestOutcome::Rejected,
			NSLOCTEXT("AZEquipment", "SwitchAnimationUnavailable", "This weapon's draw or holster animation is unavailable."));
		return false;
	}
	if (!Holster.Animation.IsValid() && !Draw.Animation.IsValid())
	{
		const bool bCommitted = CommitSelection(Item, IntrinsicSlot);
		FinishTrackedRequest(RequestId, bCommitted ? EAZ_EquipmentRequestOutcome::Activated : EAZ_EquipmentRequestOutcome::Rejected);
		return bCommitted;
	}
	TGuardValue<bool> CommitGuard(bCommitting, true);
	AAZ_Weapon* TargetWeapon = Item && Item->IsWeapon() ? PrepareWeaponActor(Item) : nullptr;
	if ((Item && Item->IsWeapon() && !TargetWeapon) || !ValidateSelection(Item, IntrinsicSlot))
	{
		FinishTrackedRequest(RequestId, EAZ_EquipmentRequestOutcome::Rejected);
		return false;
	}
	FWeaponSelectionTransition Transition;
	Transition.Id = FGuid::NewGuid();
	Transition.RequestId = RequestId;
	Transition.SourceItem = Selection.Item;
	Transition.SourceWeapon = Selection.Weapon;
	Transition.TargetItem = Item;
	Transition.TargetWeapon = TargetWeapon;
	Transition.Coordinator = IsValid(Selection.Weapon) ? Selection.Weapon.Get() : TargetWeapon;
	Transition.TargetIntrinsicSlot = IntrinsicSlot;
	Transition.bTargetWasItem = Item != nullptr;
	Transition.SourceGeneration = Selection.Generation;
	Transition.Pawn = OwningPlayerController->GetPawn();
	Transition.Mesh = OwningSkeletalMesh;
	Transition.ASC = GetASC();
	Transition.bCrouching = bCrouching;
	Transition.Holster = Holster;
	Transition.Draw = Draw;
	if (!Transition.Coordinator.IsValid() || !Transition.ASC.IsValid())
	{
		FinishTrackedRequest(RequestId, EAZ_EquipmentRequestOutcome::Rejected);
		return false;
	}
	WeaponTransition = Transition;
	SetComponentTickEnabled(true);
	SwitchInterruptedHandle = Transition.Coordinator->OnEquipmentAnimationInterrupted.AddUObject(this, &ThisClass::OnSwitchAnimationInterrupted);
	bOwnsSwitchingTag = true;
	Transition.ASC->AddStateTag(FAZ_GameplayTags::Get().Ability_State_WeaponSwitching);
	if (WeaponTransition.Id != Transition.Id) return false;
	ClearOutgoingInput();
	if (WeaponTransition.Id != Transition.Id || !Transition.ASC.IsValid()) return false;
	const FGameplayTagContainer SprintInput(FAZ_GameplayTags::Get().Input_Action_Sprint);
	Transition.ASC->ClearWeaponInput(SprintInput);
	if (!OwningPlayerController->IsLocalController()) Client_ClearOutgoingInput(SprintInput);
	const FGameplayTagContainer SprintTags(FAZ_GameplayTags::Get().Movement_Sprinting);
	Transition.ASC->CancelAbilities(&SprintTags);
	if (WeaponTransition.Id != Transition.Id) return false;
	const TArray<FGameplayAbilitySpecHandle> OutgoingHandles = GrantedHandles;
	for (const FGameplayAbilitySpecHandle& Handle : OutgoingHandles)
	{
		if (WeaponTransition.Id != Transition.Id) return false;
		Transition.ASC->CancelAbilityHandle(Handle);
	}
	if (!IsSwitchContextValid())
	{
		CancelWeaponSwitch();
		return false;
	}
	StartSwitchPhase(Holster.Animation.IsValid());
	if (WeaponTransition.Id != Transition.Id) return false;
	UE_LOG(LogTemp, Display, TEXT("[Equipment] switch begin id=%s source=%s target=%s generation=%u crouch=%d"),
		*Transition.Id.ToString(), *GetNameSafe(Selection.Item), *GetNameSafe(Item), Selection.Generation, bCrouching);
	FinishTrackedRequest(RequestId, EAZ_EquipmentRequestOutcome::Deferred,
		NSLOCTEXT("AZEquipment", "SwitchAnimationDeferred", "Drawing or holstering the weapon."));
	return true;
}

bool UAZ_Inv_CommonUI_EquipmentComponent::IsSwitchContextValid() const
{
	if (!WeaponTransition.Id.IsValid() || !GetOwner() || !GetOwner()->HasAuthority()
		|| IsHardBlocked() || !OwningPlayerController.IsValid() || !WeaponTransition.Pawn.IsValid()
		|| OwningPlayerController->GetPawn() != WeaponTransition.Pawn.Get()
		|| !WeaponTransition.Mesh.IsValid() || OwningSkeletalMesh != WeaponTransition.Mesh
		|| !WeaponTransition.ASC.IsValid() || GetASC() != WeaponTransition.ASC.Get()
		|| Selection.Generation != WeaponTransition.SourceGeneration
		|| Selection.Item.Get() != WeaponTransition.SourceItem.Get()
		|| Selection.Weapon.Get() != WeaponTransition.SourceWeapon.Get()
		|| !WeaponTransition.Coordinator.IsValid()
		|| WeaponTransition.Coordinator->IsActorBeingDestroyed()
		|| WeaponTransition.Pawn->IsActorBeingDestroyed()
		|| (WeaponTransition.bTargetWasItem && !WeaponTransition.TargetItem.IsValid())
		|| !ValidateSelection(WeaponTransition.TargetItem.Get(), WeaponTransition.TargetIntrinsicSlot)) return false;
	if (Selection.Item && (!InventoryComponent.IsValid() || !InventoryComponent->ContainsItem(Selection.Item))) return false;
	for (AAZ_Weapon* Weapon : {WeaponTransition.SourceWeapon.Get(), WeaponTransition.TargetWeapon.Get()})
	{
		if (Weapon && (Weapon->IsActorBeingDestroyed() || Weapon->GetOwner() != WeaponTransition.Pawn.Get() || !Weapon->GetRootComponent()
			|| Weapon->GetRootComponent()->GetAttachParent() != WeaponTransition.Mesh.Get())) return false;
	}
	if ((Selection.Item && Selection.Item->IsWeapon() && !WeaponTransition.SourceWeapon.IsValid())
		|| (WeaponTransition.TargetItem.IsValid() && WeaponTransition.TargetItem->IsWeapon()
			&& !WeaponTransition.TargetWeapon.IsValid())) return false;
	const UCharacterMoverComponent* Mover = WeaponTransition.Pawn->FindComponentByClass<UCharacterMoverComponent>();
	return (Mover && Mover->IsCrouching()) == WeaponTransition.bCrouching;
}

void UAZ_Inv_CommonUI_EquipmentComponent::StartSwitchPhase(bool bHolster)
{
	if (!WeaponTransition.Id.IsValid()) return;
	const FGuid TransitionId = WeaponTransition.Id;
	const FGuid PreviousPhase = WeaponTransition.PhaseId;
	WeaponTransition.PhaseId.Invalidate();
	if (PreviousPhase.IsValid() && WeaponTransition.Coordinator.IsValid())
		WeaponTransition.Coordinator->Multicast_EndEquipmentAnimation(PreviousPhase);
	if (WeaponTransition.Id != TransitionId) return;
	WeaponTransition.bHolsterPhase = bHolster;
	WeaponTransition.bSocketApplied = false;
	WeaponTransition.PhaseStartedAt = GetWorld()->GetTimeSeconds();
	WeaponTransition.PhaseId = FGuid::NewGuid();
	const FWeaponSwitchPhase& Phase = bHolster ? WeaponTransition.Holster : WeaponTransition.Draw;
	if (!Phase.Animation.IsValid() || !WeaponTransition.Coordinator.IsValid())
	{
		CancelWeaponSwitch();
		return;
	}
	RefreshCarryPresentation();
	WeaponTransition.Coordinator->Multicast_BeginEquipmentAnimation(WeaponTransition.PhaseId,
		Phase.Animation.Get(), Phase.Slot, Phase.PlayRate, Phase.BlendIn, Phase.BlendOut);
}

void UAZ_Inv_CommonUI_EquipmentComponent::ApplySwitchSocket()
{
	if (!WeaponTransition.Id.IsValid() || WeaponTransition.bSocketApplied) return;
	WeaponTransition.bSocketApplied = true;
	const bool bHolster = WeaponTransition.bHolsterPhase;
	AAZ_Weapon* Weapon = bHolster ? WeaponTransition.SourceWeapon.Get() : WeaponTransition.TargetWeapon.Get();
	if (!IsValid(Weapon)) { CancelWeaponSwitch(); return; }
	const FWeaponSwitchPhase& Phase = bHolster ? WeaponTransition.Holster : WeaponTransition.Draw;
	Weapon->BlendToEquipmentSocket(bHolster ? Weapon->CarrySocketName : Weapon->RelaxedSocketName, Phase.SocketBlendDuration);
	if (bHolster) MarkWeaponCarried(WeaponTransition.SourceItem.Get());
	RefreshCarryPresentation();
}

void UAZ_Inv_CommonUI_EquipmentComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!WeaponTransition.Id.IsValid()) { SetComponentTickEnabled(false); return; }
	if (bCommitting) return;
	TGuardValue<bool> CommitGuard(bCommitting, true);
	if (!IsSwitchContextValid()) { CancelWeaponSwitch(); return; }
	const FGuid TransitionId = WeaponTransition.Id;
	const FWeaponSwitchPhase Phase = WeaponTransition.bHolsterPhase ? WeaponTransition.Holster : WeaponTransition.Draw;
	const double Elapsed = GetWorld()->GetTimeSeconds() - WeaponTransition.PhaseStartedAt;
	if (!WeaponTransition.bSocketApplied && Elapsed >= Phase.AttachTime) ApplySwitchSocket();
	if (WeaponTransition.Id != TransitionId || Elapsed < Phase.Duration) return;
	if (WeaponTransition.bHolsterPhase && WeaponTransition.Draw.Animation.IsValid()) StartSwitchPhase(false);
	else FinishSwitch();
}

void UAZ_Inv_CommonUI_EquipmentComponent::ClearWeaponSwitch()
{
	const FWeaponSelectionTransition Previous = WeaponTransition;
	WeaponTransition = FWeaponSelectionTransition();
	SetComponentTickEnabled(false);
	const bool bRemoveTag = bOwnsSwitchingTag;
	bOwnsSwitchingTag = false;
	if (Previous.Coordinator.IsValid())
	{
		Previous.Coordinator->OnEquipmentAnimationInterrupted.Remove(SwitchInterruptedHandle);
		SwitchInterruptedHandle.Reset();
		if (Previous.PhaseId.IsValid()) Previous.Coordinator->Multicast_EndEquipmentAnimation(Previous.PhaseId);
	}
	SwitchInterruptedHandle.Reset();
	if (bRemoveTag && Previous.ASC.IsValid()) Previous.ASC->RemoveStateTag(FAZ_GameplayTags::Get().Ability_State_WeaponSwitching);
}

void UAZ_Inv_CommonUI_EquipmentComponent::CancelWeaponSwitch(EAZ_EquipmentRequestOutcome Outcome, bool bRestorePresentation)
{
	if (!WeaponTransition.Id.IsValid()) return;
	TGuardValue<bool> CommitGuard(bCommitting, true);
	const FGuid RequestId = WeaponTransition.RequestId;
	const FGuid TransitionId = WeaponTransition.Id;
	ClearWeaponSwitch();
	if (bRestorePresentation) RefreshCarryPresentation(true, .1f);
	UE_LOG(LogTemp, Display, TEXT("[Equipment] switch canceled id=%s outcome=%d"), *TransitionId.ToString(), static_cast<int32>(Outcome));
	FinishTrackedRequest(RequestId, Outcome, Outcome == EAZ_EquipmentRequestOutcome::Superseded
		? NSLOCTEXT("AZEquipment", "SwitchSuperseded", "A newer selection replaced this request.")
		: NSLOCTEXT("AZEquipment", "SwitchCanceled", "The weapon switch was interrupted."));
}

void UAZ_Inv_CommonUI_EquipmentComponent::FinishSwitch()
{
	if (!IsSwitchContextValid()) { CancelWeaponSwitch(); return; }
	const FWeaponSelectionTransition Previous = WeaponTransition;
	ClearWeaponSwitch();
	const bool bCommitted = CommitSelection(Previous.TargetItem.Get(), Previous.TargetIntrinsicSlot);
	if (!bCommitted) RefreshCarryPresentation(true, .1f);
	FinishTrackedRequest(Previous.RequestId, bCommitted ? EAZ_EquipmentRequestOutcome::Activated : EAZ_EquipmentRequestOutcome::Rejected,
		bCommitted ? FText::GetEmpty() : NSLOCTEXT("AZEquipment", "SelectionFailed", "The equipment could not be selected."));
}

void UAZ_Inv_CommonUI_EquipmentComponent::OnSwitchAnimationInterrupted(const FGuid& PhaseId)
{
	if (WeaponTransition.PhaseId == PhaseId) CancelWeaponSwitch();
}

void UAZ_Inv_CommonUI_EquipmentComponent::GrantAbilities(const TArray<TSubclassOf<UAZ_GameplayAbility>>& Abilities, UObject* Source)
{
	UAZ_AbilitySystemComponent* ASC = GetASC();
	if (!ASC) return;
	TSet<UClass*> GrantedClasses;
	for (const TSubclassOf<UAZ_GameplayAbility>& AbilityClass : Abilities)
	{
		if (!AbilityClass || GrantedClasses.Contains(AbilityClass.Get())) continue;
		GrantedClasses.Add(AbilityClass.Get());
		const UAZ_GameplayAbility* Defaults = AbilityClass.GetDefaultObject();
		FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, Source);
		if (Defaults && Defaults->InputTag.IsValid()) Spec.GetDynamicSpecSourceTags().AddTag(Defaults->InputTag);
		GrantedHandles.Add(ASC->GiveAbility(Spec));
	}
}

AAZ_Weapon* UAZ_Inv_CommonUI_EquipmentComponent::PrepareWeaponActor(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	if (!IsValid(Item) || !OwningPlayerController.IsValid() || !OwningSkeletalMesh.IsValid()) return nullptr;
	const auto* WeaponState = Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
	APawn* Pawn = OwningPlayerController->GetPawn();
	if (!WeaponState || !WeaponState->WeaponActorClass || !Pawn || Pawn->IsActorBeingDestroyed()) return nullptr;
	const FGuid ItemId = Item->GetInstanceId();
	if (const FWeaponPresentation* Existing = WeaponPresentations.Find(ItemId))
	{
		AAZ_Weapon* Weapon = Existing->Weapon.Get();
		if (IsValid(Weapon) && Weapon->IsActorBeingDestroyed()) return nullptr;
		if (Existing->Item == Item && IsValid(Weapon) && !Weapon->IsActorBeingDestroyed() && Weapon->GetOwner() == Pawn
			&& Weapon->IsA(WeaponState->WeaponActorClass)
			&& OwningSkeletalMesh->DoesSocketExist(Weapon->RelaxedSocketName)
			&& OwningSkeletalMesh->DoesSocketExist(Weapon->CarrySocketName)) return Weapon;
		DestroyPresentation(ItemId);
	}
	AAZ_Weapon* Weapon = GetWorld()->SpawnActorDeferred<AAZ_Weapon>(WeaponState->WeaponActorClass,
		Pawn->GetActorTransform(), Pawn, Pawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Weapon) return nullptr;
	Weapon->bSpawnWithCollision = false;
	Weapon->MakeCosmetic();
	Weapon->SetActorHiddenInGame(true);
	Weapon->FinishSpawning(Pawn->GetActorTransform());
	if (!IsValid(Weapon) || Weapon->IsActorBeingDestroyed()) return nullptr;
	if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed() || !OwningSkeletalMesh.IsValid())
	{
		Weapon->Destroy();
		return nullptr;
	}
	Weapon->MakeCosmetic();
	Weapon->ConfigureFirearmPresentation(*WeaponState);
	if (Weapon->IsActorBeingDestroyed() || !Weapon->GetWeaponMesh3P() || !Weapon->GetWeaponMesh3P()->GetSkeletalMeshAsset()
		|| !OwningSkeletalMesh->DoesSocketExist(Weapon->RelaxedSocketName)
		|| !OwningSkeletalMesh->DoesSocketExist(Weapon->CarrySocketName))
	{
		Weapon->Destroy();
		return nullptr;
	}
	// The old actor constructor's pickup offset must not displace an inventory-mounted rifle.
	Weapon->GetWeaponMesh3P()->SetRelativeTransform(FTransform::Identity);
	Weapon->GetWeaponMesh3P()->SetVisibility(true, true);
	if (Weapon->MeshComponent) Weapon->MeshComponent->SetVisibility(false, true);
	if (Weapon->SkeletalMeshComponent) Weapon->SkeletalMeshComponent->SetVisibility(false, true);
	if (!Weapon->AttachToComponent(OwningSkeletalMesh.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Weapon->CarrySocketName))
	{
		Weapon->Destroy();
		return nullptr;
	}
	FWeaponPresentation& Presentation = WeaponPresentations.Add(ItemId);
	Presentation.Item = Item;
	Presentation.Weapon = Weapon;
	Presentation.HolsterOrder = ++NextHolsterOrder;
	if (auto* Equipment = Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>())
		Equipment->SetEquippedActor(Weapon);
	return Weapon;
}

void UAZ_Inv_CommonUI_EquipmentComponent::MarkWeaponCarried(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	if (!IsValid(Item)) return;
	if (FWeaponPresentation* Presentation = WeaponPresentations.Find(Item->GetInstanceId()))
		Presentation->HolsterOrder = ++NextHolsterOrder;
}

void UAZ_Inv_CommonUI_EquipmentComponent::RefreshCarryPresentation(bool bRestoreSockets, float BlendDuration)
{
	if (bIsProxy || !GetOwner() || !GetOwner()->HasAuthority() || !OwningSkeletalMesh.IsValid()
		|| !OwningPlayerController.IsValid()) return;
	const UAZ_AbilitySystemComponent* ASC = GetASC();
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	TMap<FName, FWeaponPresentation*> VisibleCarry;
	const auto CarryPriority = [this](const FWeaponPresentation* Presentation)
	{
		const AAZ_Weapon* Weapon = Presentation ? Presentation->Weapon.Get() : nullptr;
		if (Weapon && WeaponTransition.Id.IsValid() && !WeaponTransition.bHolsterPhase
			&& !WeaponTransition.bSocketApplied && Weapon == WeaponTransition.TargetWeapon) return 2;
		return Weapon && Weapon == Selection.Weapon ? 1 : 0;
	};
	for (auto& Pair : WeaponPresentations)
	{
		FWeaponPresentation& Presentation = Pair.Value;
		AAZ_Weapon* Weapon = Presentation.Weapon.Get();
		if (!IsValid(Weapon) || Weapon->IsActorBeingDestroyed() || Weapon->GetOwner() != OwningPlayerController->GetPawn()) continue;
		if (bRestoreSockets)
		{
			const bool bSelected = Weapon == Selection.Weapon;
			const bool bSprintCarry = bSelected && Selection.Profile.MatchesTag(Tags.Weapon_Rifle)
				&& ASC && ASC->HasMatchingGameplayTag(Tags.Movement_Sprinting);
			const bool bAiming = bSelected && ASC && (ASC->HasMatchingGameplayTag(Tags.Ability_State_Aiming)
				|| ASC->HasMatchingGameplayTag(Tags.Ability_State_FirearmReady));
			const FName Socket = !bSelected || bSprintCarry ? Weapon->CarrySocketName
				: (bAiming ? Weapon->AimSocketName : Weapon->RelaxedSocketName);
			Weapon->BlendToEquipmentSocket(Socket, BlendDuration);
		}
		if (!Weapon->GetRootComponent()
			|| Weapon->GetRootComponent()->GetAttachSocketName() != Weapon->CarrySocketName) continue;
		FWeaponPresentation*& Winner = VisibleCarry.FindOrAdd(Weapon->CarrySocketName);
		if (!Winner || CarryPriority(&Presentation) > CarryPriority(Winner)
			|| (CarryPriority(&Presentation) == CarryPriority(Winner) && Winner->HolsterOrder < Presentation.HolsterOrder)) Winner = &Presentation;
	}
	for (auto& Pair : WeaponPresentations)
	{
		FWeaponPresentation& Presentation = Pair.Value;
		AAZ_Weapon* Weapon = Presentation.Weapon.Get();
		if (!IsValid(Weapon) || Weapon->IsActorBeingDestroyed()) continue;
		const bool bSelected = Weapon == Selection.Weapon;
		const bool bDrawnTarget = WeaponTransition.Id.IsValid() && !WeaponTransition.bHolsterPhase
			&& WeaponTransition.bSocketApplied && Weapon == WeaponTransition.TargetWeapon;
		FWeaponPresentation* const* Winner = VisibleCarry.Find(Weapon->CarrySocketName);
		const bool bAtCarry = Weapon->GetRootComponent()
			&& Weapon->GetRootComponent()->GetAttachSocketName() == Weapon->CarrySocketName;
		// The draw target briefly owns a shared socket before its handoff; otherwise
		// selected sprint carry wins, then the most recently holstered cached item.
		const bool bVisible = bAtCarry ? Winner && *Winner == &Presentation : bSelected || bDrawnTarget;
		Weapon->SetActorHiddenInGame(!bVisible);
		Weapon->ForceNetUpdate();
	}
}

void UAZ_Inv_CommonUI_EquipmentComponent::ReleaseActiveSelection()
{
	// OnPossessedPawnChanged already points the controller at the NEW pawn. Grants belong to
	// the ASC bound for the outgoing selection, which can be a different actor's component.
	UAZ_AbilitySystemComponent* ASC = BoundASC.IsValid() ? BoundASC.Get() : GetASC();
	ClearOutgoingInput();
	// Cancel first: montage, paired interaction and root-motion cleanup belongs to the old ability.
	if (ASC)
	{
		for (const FGameplayAbilitySpecHandle& Handle : GrantedHandles) ASC->CancelAbilityHandle(Handle);
		for (const FGameplayAbilitySpecHandle& Handle : GrantedHandles) ASC->ClearAbility(Handle);
	}
	GrantedHandles.Reset();
	if (UAZ_Inv_CommonUI_InventoryItem* Item = Selection.Item)
	{
		auto& Manifest = Item->GetItemManifestMutable();
		if (auto* WeaponState = Manifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_WeaponStateFragment>())
		{
			if (ASC && !WeaponState->bUsesDetachableMagazines) WeaponState->SaveFromASC(ASC);
		}
		if (auto* Equipment = Manifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>())
		{
			Equipment->OnUnequip(OwningPlayerController.Get());
		}
		if (IsValid(Selection.Weapon))
		{
			Selection.Weapon->Tags.Remove(FAZ_GameplayTags::Get().Weapon_Slot_Primary.GetTagName());
			if (OwningSkeletalMesh.IsValid()) Selection.Weapon->BlendToEquipmentSocket(Selection.Weapon->CarrySocketName, .1f);
			MarkWeaponCarried(Item);
			if (ASC) ASC->RemoveStateTag(FAZ_GameplayTags::Get().State_Equipped_Weapon_Primary);
		}
	}
	if (ASC) ASC->OnWeaponEquipped(FAZ_GameplayTags::Get().Weapon_None);
	if (ASC && bOwnsStrafeTag) ASC->RemoveStateTag(FAZ_GameplayTags::Get().Movement_Strafe);
	bOwnsStrafeTag = false;
	const uint32 Generation = Selection.Generation;
	Selection = FAZ_EquipmentSelection();
	Selection.Generation = Generation;
	Selection.Profile = FAZ_GameplayTags::Get().Weapon_None;
}

bool UAZ_Inv_CommonUI_EquipmentComponent::CommitSelection(UAZ_Inv_CommonUI_InventoryItem* Item, int32 IntrinsicSlot)
{
	if (!ValidateSelection(Item, IntrinsicSlot) || IsHardBlocked() || IsActionCommitted()) return false;
	TGuardValue<bool> CommitGuard(bCommitting, true);
	const bool bPhysicalWeapon = Item && Item->IsWeapon();
	AAZ_Weapon* NewWeapon = bPhysicalWeapon ? PrepareWeaponActor(Item) : nullptr;
	if (bPhysicalWeapon && !NewWeapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Equipment] spawn failed; keeping previous selection item=%s"), *GetNameSafe(Item));
		return false;
	}
	UAZ_Inv_CommonUI_InventoryItem* PreviousItem = Selection.Item;
	ReleaseActiveSelection();
	UAZ_AbilitySystemComponent* ASC = GetASC();
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	if (Item)
	{
		auto& Manifest = Item->GetItemManifestMutable();
		auto* Equipment = Manifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>();
		const auto* WeaponState = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
		if (NewWeapon)
		{
			Equipment->SetEquippedActor(NewWeapon);
			NewWeapon->BlendToEquipmentSocket(NewWeapon->RelaxedSocketName, .1f);
			NewWeapon->SetActorHiddenInGame(false);
			NewWeapon->Tags.AddUnique(Tags.Weapon_Slot_Primary.GetTagName());
			ASC->AddStateTag(Tags.State_Equipped_Weapon_Primary);
		}
		Selection.Item = Item;
		Selection.Weapon = NewWeapon;
		Selection.Profile = WeaponState ? Item->GetWeaponProfileTag() : Equipment->GetEquipmentType();
		if (!Selection.Profile.IsValid()) Selection.Profile = Tags.Weapon_None;
		ASC->OnWeaponEquipped(Selection.Profile);
		Equipment->OnEquip(OwningPlayerController.Get());
		// Magazine instances remain canonical. Only legacy weapons restore ASC ammo;
		// all profiles grant the explicitly authored, adapted actions from one owner.
		if (WeaponState && !WeaponState->bUsesDetachableMagazines) WeaponState->ApplyToASC(ASC);
		if (const auto* Abilities = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_AbilityGrantFragment>())
		{
			GrantAbilities(Abilities->GetAbilitiesToGrant(), NewWeapon ? static_cast<UObject*>(NewWeapon) : static_cast<UObject*>(OwningPlayerController->GetPawn()));
		}
		if (UAZ_QuickBarComponent* QuickBar = GetOwner()->FindComponentByClass<UAZ_QuickBarComponent>()) QuickBar->BindSelectedItem(Item);
	}
	else if (IntrinsicSlot != INDEX_NONE)
	{
		const FAZ_QuickSlot* Slot = GetOwner()->FindComponentByClass<UAZ_QuickBarComponent>()->GetSlotDefinition(IntrinsicSlot);
		Selection.IntrinsicSlotIndex = IntrinsicSlot;
		Selection.Profile = Slot->WeaponTag;
		ASC->OnWeaponEquipped(Selection.Profile);
		if (Slot->bStrafeOnEquip)
		{
			ASC->AddStateTag(Tags.Movement_Strafe);
			bOwnsStrafeTag = true;
			FGameplayTagContainer SprintTags(Tags.Movement_Sprinting);
			ASC->CancelAbilities(&SprintTags);
		}
		GrantAbilities(Slot->WeaponAbilities, this);
		for (const TSubclassOf<UGameplayEffect>& EffectClass : Slot->EffectsOnEquip)
		{
			if (!EffectClass) continue;
			FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
			Context.AddSourceObject(this);
			const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.f, Context);
			if (Spec.IsValid()) ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}
	++Selection.Generation;
	ReconcilePresentation();
	RefreshCarryPresentation();
	PublishSelection(PreviousItem);
	UE_LOG(LogTemp, Display, TEXT("[Equipment] committed generation=%u profile=%s item=%s actor=%s grants=%d"),
		Selection.Generation, *Selection.Profile.ToString(), Item ? *Item->GetInstanceId().ToString() : TEXT("intrinsic/none"), *GetNameSafe(NewWeapon), GrantedHandles.Num());
	return true;
}

void UAZ_Inv_CommonUI_EquipmentComponent::ReconcilePresentation()
{
	// Attachments already replicate with the weapon actor. Keep their authority in
	// the equipment owner; the AnimInstance and the cosmetic weapon never move the
	// hero mesh or compete with this socket attachment.
	if (bIsProxy || WeaponTransition.Id.IsValid() || !GetOwner()->HasAuthority() || !OwningPlayerController.IsValid()
		|| !OwningSkeletalMesh.IsValid() || !IsValid(Selection.Item) || !IsValid(Selection.Weapon)) return;
	AAZ_Weapon* Weapon = Selection.Weapon.Get();
	USkeletalMeshComponent* BodyMesh = OwningSkeletalMesh.Get();
	USceneComponent* WeaponRoot = Weapon->GetRootComponent();
	const UAZ_AbilitySystemComponent* ASC = GetASC();
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	if (!ASC || !WeaponRoot || Weapon->GetOwner() != OwningPlayerController->GetPawn()
		|| !Weapon->ActorHasTag(Tags.Weapon_Slot_Primary.GetTagName())) return;
	const bool bSelectedRifle = Selection.Profile.MatchesTag(Tags.Weapon_Rifle);
	// Sprint carry keeps the same selected item, representation and primary marker.
	// Ordinary holster clears that marker/selection, so a later sprint/aim event
	// cannot draw a normally carried secondary weapon. Detached actors stay untouched.
	const FName CurrentSocket = WeaponRoot->GetAttachSocketName();
	if (WeaponRoot->GetAttachParent() != BodyMesh
		|| (CurrentSocket != Weapon->RelaxedSocketName && CurrentSocket != Weapon->AimSocketName
			&& !(bSelectedRifle && CurrentSocket == Weapon->CarrySocketName))) return;
	const bool bAiming = ASC->HasMatchingGameplayTag(Tags.Ability_State_Aiming)
		|| ASC->HasMatchingGameplayTag(Tags.Ability_State_FirearmReady);
	const bool bSprintCarry = bSelectedRifle && ASC->HasMatchingGameplayTag(Tags.Movement_Sprinting);
	const FName DesiredSocket = bSprintCarry ? Weapon->CarrySocketName
		: (bAiming ? Weapon->AimSocketName : Weapon->RelaxedSocketName);
	if (CurrentSocket == DesiredSocket) return;
	if (DesiredSocket.IsNone() || !BodyMesh->DoesSocketExist(DesiredSocket))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Equipment] presentation socket missing item=%s mesh=%s socket=%s aiming=%d sprintCarry=%d"),
			*Selection.Item->GetInstanceId().ToString(), *GetNameSafe(BodyMesh->GetSkeletalMeshAsset()),
			*DesiredSocket.ToString(), bAiming, bSprintCarry);
		return;
	}
	if (Weapon->AttachToComponent(BodyMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, DesiredSocket))
	{
		Weapon->ForceNetUpdate();
		RefreshCarryPresentation();
		UE_LOG(LogTemp, Display, TEXT("[Equipment] presentation socket item=%s socket=%s aiming=%d sprintCarry=%d"),
			*Selection.Item->GetInstanceId().ToString(), *DesiredSocket.ToString(), bAiming, bSprintCarry);
	}
}

void UAZ_Inv_CommonUI_EquipmentComponent::DestroyPresentation(const FGuid& ItemId)
{
	FWeaponPresentation Presentation;
	if (!WeaponPresentations.RemoveAndCopyValue(ItemId, Presentation)) return;
	if (Presentation.Item.IsValid())
	{
		if (auto* Equipment = Presentation.Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>()) Equipment->SetEquippedActor(nullptr);
	}
	if (Presentation.Weapon.IsValid()) Presentation.Weapon->Destroy();
}

void UAZ_Inv_CommonUI_EquipmentComponent::DestroyAllPresentations()
{
	TArray<FGuid> ItemIds;
	WeaponPresentations.GetKeys(ItemIds);
	for (const FGuid& ItemId : ItemIds) DestroyPresentation(ItemId);
}

bool UAZ_Inv_CommonUI_EquipmentComponent::CanDropItem(UAZ_Inv_CommonUI_InventoryItem* Item) const
{
	if (bCommitting || IsHardBlocked()) return false;
	if (WeaponTransition.Id.IsValid() && (WeaponTransition.SourceItem == Item || WeaponTransition.TargetItem == Item)) return false;
	return Item != Selection.Item || !IsActionCommitted();
}

void UAZ_Inv_CommonUI_EquipmentComponent::PrepareItemForDrop(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	if (bIsProxy || !GetOwner()->HasAuthority() || !Item) return;
	TGuardValue<bool> CommitGuard(bCommitting, true);
	if (WeaponTransition.Id.IsValid() && (WeaponTransition.SourceItem == Item || WeaponTransition.TargetItem == Item)) CancelWeaponSwitch();
	if (PendingItem == Item) ClearPendingSelection();
	if (Selection.Item == Item)
	{
		ReleaseActiveSelection();
		++Selection.Generation;
		PublishSelection(Item);
	}
	DestroyPresentation(Item->GetInstanceId());
	if (auto* Equipment = Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>()) Equipment->OnDrop();
	RefreshCarryPresentation();
}

void UAZ_Inv_CommonUI_EquipmentComponent::PublishSelection(UAZ_Inv_CommonUI_InventoryItem* PreviousItem)
{
	if (InventoryComponent.IsValid() && PreviousItem != Selection.Item)
	{
		if (PreviousItem) InventoryComponent->OnItemUnequipped.Broadcast(PreviousItem);
		if (Selection.Item) InventoryComponent->OnItemEquipped.Broadcast(Selection.Item);
	}
	OnEquipmentChanged.Broadcast();
	if (GetOwner()->HasAuthority()) GetOwner()->ForceNetUpdate();
}

void UAZ_Inv_CommonUI_EquipmentComponent::OnRep_Selection(FAZ_EquipmentSelection Previous)
{
	if (FirearmReady.LifetimeId.IsValid() && (FirearmReady.Generation != Selection.Generation
		|| FirearmReady.Weapon.Get() != Selection.Weapon || !IsValid(Selection.Item)
		|| FirearmReady.ItemId != Selection.Item->GetInstanceId())) CancelFirearmReady();
	PublishSelection(Previous.Item);
}

void UAZ_Inv_CommonUI_EquipmentComponent::ClearPendingSelection(EAZ_EquipmentRequestOutcome Outcome)
{
	const FGuid RequestId = PendingRequestId;
	PendingRequestId.Invalidate();
	bPendingSelection = false;
	bPendingWasItem = false;
	PendingItem.Reset();
	PendingIntrinsicSlot = INDEX_NONE;
	FinishTrackedRequest(RequestId, Outcome,
		Outcome == EAZ_EquipmentRequestOutcome::Superseded
			? NSLOCTEXT("AZEquipment", "SelectionSuperseded", "A newer selection replaced this request.")
			: NSLOCTEXT("AZEquipment", "SelectionCanceled", "The pending equipment selection was canceled."));
}

void UAZ_Inv_CommonUI_EquipmentComponent::SchedulePendingSelection()
{
	if (!bPendingSelection || bRetryScheduled || bCommitting || WeaponTransition.Id.IsValid()) return;
	bRetryScheduled = true;
	// Tag changes fire from montage notifies / EndAbility. Never cancel an ability on its own stack.
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RetryPendingSelection);
}

void UAZ_Inv_CommonUI_EquipmentComponent::RetryPendingSelection()
{
	bRetryScheduled = false;
	if (!bPendingSelection) return;
	if (IsHardBlocked() || (bPendingWasItem && !PendingItem.IsValid()))
	{
		ClearPendingSelection();
		return;
	}
	if (IsActionCommitted()) return;
	UAZ_Inv_CommonUI_InventoryItem* Item = PendingItem.Get();
	const int32 IntrinsicSlot = PendingIntrinsicSlot;
	const FGuid RequestId = PendingRequestId;
	PendingRequestId.Invalidate();
	ClearPendingSelection();
	BeginSelectionChange(Item, IntrinsicSlot, RequestId);
}

void UAZ_Inv_CommonUI_EquipmentComponent::OnGateTagChanged(FGameplayTag Tag, int32 Count)
{
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	if (Count > 0 && (Tag == Tags.Movement_Sprinting || Tag == Tags.Ability_State_MeleeAttacking
		|| Tag == Tags.Ability_State_WeaponSwitching))
	{
		CancelFirearmReady();
		CancelActiveFire();
	}
	if (Tag == Tags.Ability_State_Aiming || Tag == Tags.Ability_State_FirearmReady || Tag == Tags.Movement_Sprinting)
	{
		// Aim/sprint cleanup also runs inside selection/drop commits. Their draw/holster
		// operations own attachment until the committed selection is ready.
		if (!bCommitting && (Tag == Tags.Ability_State_Aiming || Tag == Tags.Ability_State_FirearmReady)
			&& Count <= 0 && !IsFirearmRaised()
			&& IsValid(Selection.Item) && Selection.Item->IsInitialized() && IsValid(Selection.Weapon))
		{
			const auto* Definition = Selection.Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
			// SINGLE may have ended on trigger release while its accepted-shot pose is
			// still finishing. Lower only when both Ready and precision aim have ended.
			if (Definition && Definition->bUsesDetachableMagazines) Selection.Weapon->StopFirearmAnimation();
		}
		if (!bCommitting) ReconcilePresentation();
		return;
	}
	if (Count > 0 && EquipmentBlockTags().HasTagExact(Tag))
	{
		TGuardValue<bool> CommitGuard(bCommitting, true);
		ClearPendingSelection();
		CancelActiveAim();
		CancelWeaponSwitch();
	}
	else SchedulePendingSelection();
}

void UAZ_Inv_CommonUI_EquipmentComponent::OnAbilityEnded(const FAbilityEndedData& Data)
{
	SchedulePendingSelection();
}

void UAZ_Inv_CommonUI_EquipmentComponent::BindAbilityEvents()
{
	if (bIsProxy || !GetOwner() || !OwningPlayerController.IsValid()
		|| (!GetOwner()->HasAuthority() && !OwningPlayerController->IsLocalController())) return;
	UAZ_AbilitySystemComponent* ASC = GetASC();
	if (BoundASC == ASC) return;
	UnbindAbilityEvents();
	if (!ASC) return;
	BoundASC = ASC;
	FGameplayTagContainer Tags = EquipmentBlockTags();
	Tags.AddTag(FAZ_GameplayTags::Get().State_Combat_CancelWindow);
	Tags.AddTag(FAZ_GameplayTags::Get().Ability_State_Aiming);
	Tags.AddTag(FAZ_GameplayTags::Get().Ability_State_FirearmReady);
	Tags.AddTag(FAZ_GameplayTags::Get().Ability_State_MeleeAttacking);
	Tags.AddTag(FAZ_GameplayTags::Get().Ability_State_WeaponSwitching);
	Tags.AddTag(FAZ_GameplayTags::Get().Movement_Sprinting);
	for (const FGameplayTag& Tag : Tags)
	{
		GateDelegateHandles.Add(Tag, ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::OnGateTagChanged));
	}
	AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::OnAbilityEnded);
	ReconcilePresentation();
}

void UAZ_Inv_CommonUI_EquipmentComponent::UnbindAbilityEvents()
{
	CancelFirearmReady();
	if (BoundASC.IsValid())
	{
		for (const auto& Pair : GateDelegateHandles) BoundASC->RegisterGameplayTagEvent(Pair.Key, EGameplayTagEventType::NewOrRemoved).Remove(Pair.Value);
		BoundASC->OnAbilityEnded.Remove(AbilityEndedHandle);
	}
	GateDelegateHandles.Reset();
	AbilityEndedHandle.Reset();
	BoundASC.Reset();
}

void UAZ_Inv_CommonUI_EquipmentComponent::SetOwningSkeletalMesh(USkeletalMeshComponent* OwningMesh)
{
	if (OwningSkeletalMesh.Get() != OwningMesh) CancelWeaponSwitch(EAZ_EquipmentRequestOutcome::Rejected, false);
	OwningSkeletalMesh = OwningMesh;
}

void UAZ_Inv_CommonUI_EquipmentComponent::OnPossessedPawnChange(APawn* OldPawn, APawn* NewPawn)
{
	CancelFirearmReady();
	TGuardValue<bool> CommitGuard(bCommitting, true);
	ClearPendingSelection();
	CancelWeaponSwitch(EAZ_EquipmentRequestOutcome::Rejected, false);
	// Pawn replacement must not inherit an old weapon actor or its grants on the persistent player ASC.
	if (!bIsProxy && OldPawn && GetOwner()->HasAuthority())
	{
		UAZ_Inv_CommonUI_InventoryItem* PreviousItem = Selection.Item;
		ReleaseActiveSelection();
		DestroyAllPresentations();
		++Selection.Generation;
		PublishSelection(PreviousItem);
	}
	if (!bIsProxy)
	{
		OwningSkeletalMesh.Reset();
		if (const AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(NewPawn)) OwningSkeletalMesh = Hero->GetMesh();
		else if (const AAZ_HeroPawn* LegacyHero = Cast<AAZ_HeroPawn>(NewPawn)) OwningSkeletalMesh = LegacyHero->GetMainMesh();
		else if (const ACharacter* Character = Cast<ACharacter>(NewPawn)) OwningSkeletalMesh = Character->GetMesh();
	}
	BindAbilityEvents();
}

void UAZ_Inv_CommonUI_EquipmentComponent::OnInventoryChanged()
{
	if (FirearmReady.LifetimeId.IsValid() && InventoryComponent.IsValid()
		&& (!IsValid(Selection.Item) || !InventoryComponent->ContainsItem(Selection.Item))) CancelFirearmReady();
	if (bIsProxy || !GetOwner()->HasAuthority() || !InventoryComponent.IsValid()) return;
	TGuardValue<bool> CommitGuard(bCommitting, true);
	if (bPendingWasItem && !InventoryComponent->ContainsItem(PendingItem.Get())) ClearPendingSelection();
	if (WeaponTransition.Id.IsValid() && !IsSwitchContextValid()) CancelWeaponSwitch();
	TArray<FGuid> RemovedIds;
	for (const auto& Pair : WeaponPresentations)
	{
		if (!Pair.Value.Item.IsValid() || !InventoryComponent->ContainsItem(Pair.Value.Item.Get())) RemovedIds.Add(Pair.Key);
	}
	for (const FGuid& Id : RemovedIds)
	{
		const FWeaponPresentation* Presentation = WeaponPresentations.Find(Id);
		if (Presentation && Presentation->Item.IsValid()) PrepareItemForDrop(Presentation->Item.Get());
		else DestroyPresentation(Id);
	}
	RefreshCarryPresentation();
}

void UAZ_Inv_CommonUI_EquipmentComponent::InitializeOwner(APlayerController* PlayerController)
{
	if (!IsValid(PlayerController)) return;
	OwningPlayerController = PlayerController;
	InventoryComponent = PlayerController->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>();
	if (InventoryComponent.IsValid()) InventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::OnInventoryChanged);
	PlayerController->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::OnPossessedPawnChange);
	OnPossessedPawnChange(nullptr, PlayerController->GetPawn());
}

void UAZ_Inv_CommonUI_EquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeOwner(Cast<APlayerController>(GetOwner()));
}

void UAZ_Inv_CommonUI_EquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelFirearmReady();
	TGuardValue<bool> CommitGuard(bCommitting, true);
	ClearPendingSelection();
	CancelWeaponSwitch(EAZ_EquipmentRequestOutcome::Rejected, false);
	if (!bIsProxy && GetOwner()->HasAuthority())
	{
		ReleaseActiveSelection();
		DestroyAllPresentations();
	}
	UnbindAbilityEvents();
	if (InventoryComponent.IsValid()) InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &ThisClass::OnInventoryChanged);
	if (OwningPlayerController.IsValid()) OwningPlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::OnPossessedPawnChange);
	Super::EndPlay(EndPlayReason);
}
