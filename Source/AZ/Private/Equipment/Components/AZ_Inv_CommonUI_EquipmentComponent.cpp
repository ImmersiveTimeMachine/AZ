#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/AZ_GA_FirearmFire.h"
#include "Animation/AZ_WeaponAnimationProfile.h"
#include "AZ_GameplayTags.h"
#include "Character/AZ_HeroPawn.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffect.h"
#include "Inventory/AZ_QuickBarComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "Net/UnrealNetwork.h"
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
	PrimaryComponentTick.bCanEverTick = false;
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

bool UAZ_Inv_CommonUI_EquipmentComponent::IsActiveWeaponSource(const UObject* Source) const
{
	return !bIsProxy && !bCommitting && !IsHardBlocked() && IsValid(Selection.Item)
		&& Selection.Item->IsInitialized() && Selection.Item->IsWeapon() && IsValid(Selection.Weapon)
		&& Source == Selection.Weapon && InventoryComponent.IsValid() && InventoryComponent->ContainsItem(Selection.Item)
		&& Selection.Profile.IsValid() && Selection.Profile == Selection.Item->GetWeaponProfileTag()
		&& OwningPlayerController.IsValid() && Selection.Weapon->GetOwner() == OwningPlayerController->GetPawn();
}

void UAZ_Inv_CommonUI_EquipmentComponent::CancelActiveAim()
{
	UAZ_AbilitySystemComponent* ASC = BoundASC.IsValid() ? BoundASC.Get() : GetASC();
	if (!ASC) return;
	const FGameplayTag AimInput = FAZ_GameplayTags::Get().Input_Action_Aim;
	FGameplayTagContainer InputTags(AimInput);
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
				|| (Spec.Ability && Spec.Ability->IsA<UAZ_GA_FirearmFire>()))) AimHandles.Add(Spec.Handle);
		}
	}
	for (const FGameplayAbilitySpecHandle& Handle : AimHandles) ASC->CancelAbilityHandle(Handle);
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
		return Slot && !Slot->bInventoryBacked && Slot->WeaponTag.IsValid();
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
	if (UAZ_AbilitySystemComponent* ASC = GetASC()) ASC->ClearWeaponInput(InputTags);
}

bool UAZ_Inv_CommonUI_EquipmentComponent::RequestSelection(UAZ_Inv_CommonUI_InventoryItem* Item, int32 IntrinsicSlot)
{
	BindAbilityEvents();
	if (bCommitting || IsHardBlocked() || !ValidateSelection(Item, IntrinsicSlot))
	{
		UE_LOG(LogTemp, Display, TEXT("[Equipment] request refused item=%s slot=%d (ownership/profile/socket/body gate)"), *GetNameSafe(Item), IntrinsicSlot);
		return false;
	}
	if (Selection.Item == Item && Selection.IntrinsicSlotIndex == IntrinsicSlot)
	{
		ClearPendingSelection();
		return true;
	}
	ClearOutgoingInput();
	if (IsActionCommitted())
	{
		PendingItem = Item;
		PendingIntrinsicSlot = IntrinsicSlot;
		bPendingWasItem = Item != nullptr;
		bPendingSelection = true;
		UE_LOG(LogTemp, Display, TEXT("[Equipment] queued latest selection item=%s slot=%d until action recovery"), *GetNameSafe(Item), IntrinsicSlot);
		return true;
	}
	ClearPendingSelection();
	return CommitSelection(Item, IntrinsicSlot);
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
	if (PresentedItem == Item && PresentedWeapon.IsValid()) return PresentedWeapon.Get();
	const auto* WeaponState = Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
	APawn* Pawn = OwningPlayerController->GetPawn();
	AAZ_Weapon* Weapon = GetWorld()->SpawnActorDeferred<AAZ_Weapon>(WeaponState->WeaponActorClass,
		Pawn->GetActorTransform(), Pawn, Pawn, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Weapon) return nullptr;
	Weapon->bSpawnWithCollision = false;
	Weapon->MakeCosmetic();
	Weapon->SetActorHiddenInGame(true);
	Weapon->FinishSpawning(Pawn->GetActorTransform());
	Weapon->MakeCosmetic();
	Weapon->ConfigureFirearmPresentation(*WeaponState);
	if (!Weapon->GetWeaponMesh3P() || !Weapon->GetWeaponMesh3P()->GetSkeletalMeshAsset()
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
	return Weapon;
}

void UAZ_Inv_CommonUI_EquipmentComponent::ReleaseActiveSelection()
{
	// OnPossessedPawnChanged already points the controller at the NEW pawn. Grants belong to
	// the ASC bound for the outgoing selection, which can be a different actor's component.
	UAZ_AbilitySystemComponent* ASC = BoundASC.IsValid() ? BoundASC.Get() : GetASC();
	if (!ASC) return;
	ClearOutgoingInput();
	// Cancel first: montage, paired interaction and root-motion cleanup belongs to the old ability.
	for (const FGameplayAbilitySpecHandle& Handle : GrantedHandles) ASC->CancelAbilityHandle(Handle);
	for (const FGameplayAbilitySpecHandle& Handle : GrantedHandles) ASC->ClearAbility(Handle);
	GrantedHandles.Reset();
	if (UAZ_Inv_CommonUI_InventoryItem* Item = Selection.Item)
	{
		auto& Manifest = Item->GetItemManifestMutable();
		if (auto* WeaponState = Manifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_WeaponStateFragment>())
		{
			if (!WeaponState->bUsesDetachableMagazines) WeaponState->SaveFromASC(ASC);
		}
		if (auto* Equipment = Manifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>())
		{
			Equipment->OnUnequip(OwningPlayerController.Get());
		}
		if (Selection.Weapon)
		{
			Selection.Weapon->Tags.Remove(FAZ_GameplayTags::Get().Weapon_Slot_Primary.GetTagName());
			if (OwningSkeletalMesh.IsValid()) Selection.Weapon->AttachToComponent(OwningSkeletalMesh.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Selection.Weapon->CarrySocketName);
			ASC->RemoveStateTag(FAZ_GameplayTags::Get().State_Equipped_Weapon_Primary);
		}
	}
	ASC->OnWeaponEquipped(FAZ_GameplayTags::Get().Weapon_None);
	if (bOwnsStrafeTag) ASC->RemoveStateTag(FAZ_GameplayTags::Get().Movement_Strafe);
	bOwnsStrafeTag = false;
	const uint32 Generation = Selection.Generation;
	Selection = FAZ_EquipmentSelection();
	Selection.Generation = Generation;
	Selection.Profile = FAZ_GameplayTags::Get().Weapon_None;
}

bool UAZ_Inv_CommonUI_EquipmentComponent::CommitSelection(UAZ_Inv_CommonUI_InventoryItem* Item, int32 IntrinsicSlot)
{
	if (!ValidateSelection(Item, IntrinsicSlot) || IsHardBlocked() || IsActionCommitted()) return false;
	const bool bPhysicalWeapon = Item && Item->IsWeapon();
	AAZ_Weapon* NewWeapon = bPhysicalWeapon ? PrepareWeaponActor(Item) : nullptr;
	if (bPhysicalWeapon && !NewWeapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Equipment] spawn failed; keeping previous selection item=%s"), *GetNameSafe(Item));
		return false;
	}
	TGuardValue<bool> CommitGuard(bCommitting, true);
	UAZ_Inv_CommonUI_InventoryItem* PreviousItem = Selection.Item;
	ReleaseActiveSelection();
	if (NewWeapon && PresentedItem != Item) DestroyPresentation();
	UAZ_AbilitySystemComponent* ASC = GetASC();
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	if (Item)
	{
		auto& Manifest = Item->GetItemManifestMutable();
		auto* Equipment = Manifest.GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>();
		const auto* WeaponState = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
		if (NewWeapon)
		{
			PresentedItem = Item;
			PresentedWeapon = NewWeapon;
			Equipment->SetEquippedActor(NewWeapon);
			NewWeapon->AttachToComponent(OwningSkeletalMesh.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, NewWeapon->RelaxedSocketName);
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
	if (bIsProxy || !GetOwner()->HasAuthority() || !OwningPlayerController.IsValid()
		|| !OwningSkeletalMesh.IsValid() || !IsValid(Selection.Item) || !IsValid(Selection.Weapon)
		|| PresentedItem.Get() != Selection.Item.Get() || PresentedWeapon.Get() != Selection.Weapon.Get()) return;
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
	const bool bAiming = ASC->HasMatchingGameplayTag(Tags.Ability_State_Aiming);
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
		UE_LOG(LogTemp, Display, TEXT("[Equipment] presentation socket item=%s socket=%s aiming=%d sprintCarry=%d"),
			*Selection.Item->GetInstanceId().ToString(), *DesiredSocket.ToString(), bAiming, bSprintCarry);
	}
}

void UAZ_Inv_CommonUI_EquipmentComponent::DestroyPresentation()
{
	if (PresentedItem.IsValid())
	{
		if (auto* Equipment = PresentedItem->GetItemManifestMutable().GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>()) Equipment->SetEquippedActor(nullptr);
	}
	if (PresentedWeapon.IsValid()) PresentedWeapon->Destroy();
	PresentedWeapon.Reset();
	PresentedItem.Reset();
}

bool UAZ_Inv_CommonUI_EquipmentComponent::CanDropItem(UAZ_Inv_CommonUI_InventoryItem* Item) const
{
	if (bCommitting || IsHardBlocked()) return false;
	return Item != Selection.Item || !IsActionCommitted();
}

void UAZ_Inv_CommonUI_EquipmentComponent::PrepareItemForDrop(UAZ_Inv_CommonUI_InventoryItem* Item)
{
	if (bIsProxy || !GetOwner()->HasAuthority() || !Item) return;
	if (PendingItem == Item) ClearPendingSelection();
	if (Selection.Item == Item)
	{
		TGuardValue<bool> CommitGuard(bCommitting, true);
		ReleaseActiveSelection();
		++Selection.Generation;
		PublishSelection(Item);
	}
	if (PresentedItem == Item) DestroyPresentation();
	if (auto* Equipment = Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FAZ_Inv_CommonUI_EquipmentFragment>()) Equipment->OnDrop();
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
	PublishSelection(Previous.Item);
}

void UAZ_Inv_CommonUI_EquipmentComponent::ClearPendingSelection()
{
	bPendingSelection = false;
	bPendingWasItem = false;
	PendingItem.Reset();
	PendingIntrinsicSlot = INDEX_NONE;
}

void UAZ_Inv_CommonUI_EquipmentComponent::SchedulePendingSelection()
{
	if (!bPendingSelection || bRetryScheduled || bCommitting) return;
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
	ClearPendingSelection();
	CommitSelection(Item, IntrinsicSlot);
}

void UAZ_Inv_CommonUI_EquipmentComponent::OnGateTagChanged(FGameplayTag Tag, int32 Count)
{
	if (Tag == FAZ_GameplayTags::Get().Ability_State_Aiming || Tag == FAZ_GameplayTags::Get().Movement_Sprinting)
	{
		// Aim/sprint cleanup also runs inside selection/drop commits. Their draw/holster
		// operations own attachment until the committed selection is ready.
		if (!bCommitting) ReconcilePresentation();
		return;
	}
	if (Count > 0 && EquipmentBlockTags().HasTagExact(Tag))
	{
		ClearPendingSelection();
		CancelActiveAim();
	}
	else SchedulePendingSelection();
}

void UAZ_Inv_CommonUI_EquipmentComponent::OnAbilityEnded(const FAbilityEndedData& Data)
{
	SchedulePendingSelection();
}

void UAZ_Inv_CommonUI_EquipmentComponent::BindAbilityEvents()
{
	if (bIsProxy || !GetOwner()->HasAuthority()) return;
	UAZ_AbilitySystemComponent* ASC = GetASC();
	if (BoundASC == ASC) return;
	UnbindAbilityEvents();
	if (!ASC) return;
	BoundASC = ASC;
	FGameplayTagContainer Tags = EquipmentBlockTags();
	Tags.AddTag(FAZ_GameplayTags::Get().State_Combat_CancelWindow);
	Tags.AddTag(FAZ_GameplayTags::Get().Ability_State_Aiming);
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
	OwningSkeletalMesh = OwningMesh;
}

void UAZ_Inv_CommonUI_EquipmentComponent::OnPossessedPawnChange(APawn* OldPawn, APawn* NewPawn)
{
	ClearPendingSelection();
	// Pawn replacement must not inherit an old weapon actor or its grants on the persistent player ASC.
	if (!bIsProxy && OldPawn && GetOwner()->HasAuthority())
	{
		UAZ_Inv_CommonUI_InventoryItem* PreviousItem = Selection.Item;
		ReleaseActiveSelection();
		DestroyPresentation();
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
	if (bIsProxy || !GetOwner()->HasAuthority() || !InventoryComponent.IsValid()) return;
	if (bPendingWasItem && !InventoryComponent->ContainsItem(PendingItem.Get())) ClearPendingSelection();
	if (PresentedItem.IsValid() && !InventoryComponent->ContainsItem(PresentedItem.Get())) PrepareItemForDrop(PresentedItem.Get());
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
	ClearPendingSelection();
	if (!bIsProxy && GetOwner()->HasAuthority())
	{
		ReleaseActiveSelection();
		DestroyPresentation();
	}
	UnbindAbilityEvents();
	if (InventoryComponent.IsValid()) InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &ThisClass::OnInventoryChanged);
	if (OwningPlayerController.IsValid()) OwningPlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::OnPossessedPawnChange);
	Super::EndPlay(EndPlayReason);
}
