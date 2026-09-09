// Copyright Artur. AZ project.

#include "UI/AZ_PlayerUIComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h"
#include "AZ_GameplayTags.h"
#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "Player/AZ_PlayerState.h"
#include "UI/AZ_HUDReticleDefinition.h"
#include "UI/AZ_HUDReticleWidget.h"
#include "Weapon/AZ_Weapon.h"

namespace
{
	FGameplayTagContainer ReticleBlockedTags()
	{
		const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
		FGameplayTagContainer Result;
		Result.AddTag(Tags.Character_Dead);
		Result.AddTag(Tags.Character_Dying);
		Result.AddTag(Tags.State_Grabbed);
		Result.AddTag(Tags.State_Combat_Grabbing);
		Result.AddTag(Tags.State_Combat_Staggered);
		Result.AddTag(Tags.State_Combat_StruckPair);
		Result.AddTag(Tags.Ability_State_MeleeAttacking);
		Result.AddTag(Tags.Ability_State_Reloading);
		Result.AddTag(Tags.Movement_Sprinting);
		return Result;
	}

	bool ReticleViewsEqual(const FAZ_PlayerReticleView& A, const FAZ_PlayerReticleView& B)
	{
		return A.bVisible == B.bVisible && A.bAiming == B.bAiming
			&& A.WeaponProfile == B.WeaponProfile && A.WeaponItemId == B.WeaponItemId
			&& A.Definition == B.Definition && A.SpreadAngleDegrees == B.SpreadAngleDegrees;
	}
}

UAZ_PlayerUIComponent::UAZ_PlayerUIComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UAZ_PlayerUIComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshBindings();
}

void UAZ_PlayerUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	RefreshReticle(); // Publish the hidden state while the HUD can still be subscribed.
	UnbindVitals();
	UnbindInventory();
	UnbindEquipment();
	UnbindController();
	VitalsView = FAZ_PlayerVitalsView();
	WeaponView = FAZ_PlayerWeaponView();
	bInventoryOpen = false;
	Super::EndPlay(EndPlayReason);
}

void UAZ_PlayerUIComponent::RefreshBindings()
{
	if (bEndingPlay) return;
	APlayerController* Player = Cast<APlayerController>(GetOwner());
	if (!IsValid(Player) || !Player->IsLocalController())
	{
		UnbindVitals();
		UnbindInventory();
		UnbindEquipment();
		UnbindController();
		VitalsView = FAZ_PlayerVitalsView();
		WeaponView = FAZ_PlayerWeaponView();
		bInventoryOpen = false;
		RefreshReticle();
		return;
	}
	if (OwningController.Get() != Player)
	{
		UnbindController();
		OwningController = Player;
		Player->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}

	// PlayerState vitals exist independently of whether an avatar has initialized ActorInfo.
	const AAZ_PlayerState* PlayerState = Player->GetPlayerState<AAZ_PlayerState>();
	UAbilitySystemComponent* ASC = IsValid(PlayerState) ? PlayerState->GetAbilitySystemComponent() : nullptr;
	if (BoundASC.Get() != ASC)
	{
		UnbindVitals();
		BoundASC = ASC;
		if (IsValid(ASC))
		{
			// Subscribe even if a set is still arriving. Its replicated values will refresh the view.
			HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UAZ_VitalsAttributeSet::GetHealthAttribute())
				.AddUObject(this, &ThisClass::HandleVitalsChanged);
			MaxHealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(UAZ_VitalsAttributeSet::GetMaxHealthAttribute())
				.AddUObject(this, &ThisClass::HandleVitalsChanged);
			FGameplayTagContainer ReticleTags = ReticleBlockedTags();
			ReticleTags.AddTag(FAZ_GameplayTags::Get().Ability_State_Aiming);
			for (const FGameplayTag& Tag : ReticleTags)
			{
				ReticleTagChangedHandles.Add(Tag,
					ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
						.AddUObject(this, &ThisClass::HandleReticleTagChanged));
			}
		}
	}

	UAZ_Inv_CommonUI_InventoryComponent* Inventory = Player->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>();
	if (BoundInventory.Get() != Inventory)
	{
		UnbindInventory();
		BoundInventory = Inventory;
		if (IsValid(Inventory))
		{
			Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::HandleInventoryChanged);
			Inventory->OnNoRoomInInventory.AddUniqueDynamic(this, &ThisClass::HandleInventoryFull);
			Inventory->OnInventoryMenuToggled.AddUniqueDynamic(this, &ThisClass::HandleInventoryVisibilityChanged);
		}
	}

	UAZ_Inv_CommonUI_EquipmentComponent* Equipment = Player->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
	if (BoundEquipment.Get() != Equipment)
	{
		UnbindEquipment();
		BoundEquipment = Equipment;
		if (IsValid(Equipment)) Equipment->OnEquipmentChanged.AddUniqueDynamic(this, &ThisClass::HandleEquipmentChanged);
	}

	// Every source is subscribed before any initial snapshot is published.
	RefreshVitals();
	RefreshWeapon();
	HandleInventoryVisibilityChanged(IsValid(Inventory) && Inventory->IsMenuOpen());
}

void UAZ_PlayerUIComponent::UnbindController()
{
	if (APlayerController* Player = OwningController.Get())
	{
		Player->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	}
	OwningController.Reset();
}

void UAZ_PlayerUIComponent::UnbindVitals()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UAZ_VitalsAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UAZ_VitalsAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
		for (const auto& Pair : ReticleTagChangedHandles)
		{
			ASC->RegisterGameplayTagEvent(Pair.Key, EGameplayTagEventType::NewOrRemoved).Remove(Pair.Value);
		}
	}
	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	ReticleTagChangedHandles.Reset();
	BoundASC.Reset();
}

void UAZ_PlayerUIComponent::UnbindInventory()
{
	if (UAZ_Inv_CommonUI_InventoryComponent* Inventory = BoundInventory.Get())
	{
		Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
		Inventory->OnNoRoomInInventory.RemoveDynamic(this, &ThisClass::HandleInventoryFull);
		Inventory->OnInventoryMenuToggled.RemoveDynamic(this, &ThisClass::HandleInventoryVisibilityChanged);
	}
	BoundInventory.Reset();
}

void UAZ_PlayerUIComponent::UnbindEquipment()
{
	if (UAZ_Inv_CommonUI_EquipmentComponent* Equipment = BoundEquipment.Get())
	{
		Equipment->OnEquipmentChanged.RemoveDynamic(this, &ThisClass::HandleEquipmentChanged);
	}
	SetPresentationWeapon(nullptr);
	BoundEquipment.Reset();
}

void UAZ_PlayerUIComponent::RefreshVitals()
{
	FAZ_PlayerVitalsView Next;
	const UAZ_VitalsAttributeSet* Vitals = BoundASC.IsValid() ? BoundASC->GetSet<UAZ_VitalsAttributeSet>() : nullptr;
	if (IsValid(Vitals))
	{
		const float Health = Vitals->GetHealth();
		const float MaxHealth = Vitals->GetMaxHealth();
		if (FMath::IsFinite(Health) && FMath::IsFinite(MaxHealth) && MaxHealth > 0.f)
		{
			Next.bAvailable = true;
			Next.Health = Health;
			Next.MaxHealth = MaxHealth;
			Next.Fraction = FMath::Clamp(Health / MaxHealth, 0.f, 1.f);
			Next.bCritical = Next.Fraction <= FMath::Clamp(CriticalHealthFraction, 0.f, 1.f);
		}
	}
	VitalsView = Next;
	OnVitalsChanged.Broadcast(VitalsView);
}

void UAZ_PlayerUIComponent::RefreshWeapon()
{
	FAZ_PlayerWeaponView Next;
	const UAZ_Inv_CommonUI_InventoryItem* Item = BoundEquipment.IsValid() ? BoundEquipment->GetActiveItem() : nullptr;
	Next.Profile = BoundEquipment.IsValid() ? BoundEquipment->GetActiveProfile() : FGameplayTag();
	if (IsValid(Item) && Item->IsInitialized())
	{
		const FAZ_Inv_CommonUI_ItemManifest& Manifest = Item->GetItemManifest();
		const FAZ_Inv_CommonUI_WeaponStateFragment* Definition = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
		if (Definition && Definition->IsWeaponItem())
		{
			Next.bHasWeapon = true;
			Next.bUsesMagazines = Definition->bUsesDetachableMagazines;
			const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
			const auto* Name = Manifest.GetFragmentOfTypeByTag<FAZ_Inv_CommonUI_Text_Fragment>(Tags.Item_Fragment_Name_StaticText);
			if (!Name) Name = Manifest.GetFragmentOfTypeByTag<FAZ_Inv_CommonUI_Text_Fragment>(Tags.Item_Fragment_Name);
			if (Name)
			{
				Next.DisplayName = Name->GetText();
			}
			const auto* Image = Manifest.GetFragmentOfTypeByTag<FAZ_Inv_CommonUI_ImageFragment>(Tags.Item_Fragment_Icon);
			if (!Image) Image = Manifest.GetFragmentOfType<FAZ_Inv_CommonUI_ImageFragment>();
			if (Image)
			{
				Next.Icon = Image->GetIcon();
				Next.IconDimensions = Image->GetIconDimensions();
			}
			if (Next.bUsesMagazines && BoundInventory.IsValid())
			{
				Next.Ammo = BoundInventory->GetWeaponAmmoSnapshot(Item->GetInstanceId());
			}
		}
	}

	// Subscribe as soon as the selected actor resolves, even when its item manifest or
	// Owner is still arriving. Those readiness events must not require a visible view.
	AAZ_Weapon* Weapon = BoundEquipment.IsValid() ? BoundEquipment->GetActiveWeapon() : nullptr;
	SetPresentationWeapon(IsValid(Weapon) && !Weapon->IsActorBeingDestroyed() ? Weapon : nullptr);
	WeaponView = Next;
	OnWeaponChanged.Broadcast(WeaponView);
}

void UAZ_PlayerUIComponent::RefreshReticle()
{
	FAZ_PlayerReticleView Next;
	const APlayerController* Player = OwningController.Get();
	const APawn* Pawn = IsValid(Player) ? Player->GetPawn() : nullptr;
	const UAbilitySystemComponent* ASC = BoundASC.Get();
	const UAZ_Inv_CommonUI_EquipmentComponent* Equipment = BoundEquipment.Get();
	const UAZ_Inv_CommonUI_InventoryComponent* Inventory = BoundInventory.Get();
	if (!bEndingPlay && IsValid(Player) && Player->IsLocalController()
		&& IsValid(Pawn) && !Pawn->IsActorBeingDestroyed()
		&& IsValid(ASC) && IsValid(Equipment) && IsValid(Inventory))
	{
		const UAZ_Inv_CommonUI_InventoryItem* Item = Equipment->GetActiveItem();
		const AAZ_Weapon* Weapon = Equipment->GetActiveWeapon();
		const FGameplayTag Profile = Equipment->GetActiveProfile();
		// Presentation reads the committed selection. IsActiveWeaponSource also checks a
		// transaction lock which is still raised when OnEquipmentChanged is published.
		if (IsValid(Item) && Item->IsInitialized() && Item->IsWeapon() && Inventory->ContainsItem(Item)
			&& IsValid(Weapon) && !Weapon->IsActorBeingDestroyed() && Weapon->GetOwner() == Pawn
			&& Profile.IsValid() && Profile == Item->GetWeaponProfileTag())
		{
			const auto* WeaponDefinition = Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
			UAZ_HUDReticleDefinition* Definition = WeaponDefinition ? WeaponDefinition->ReticleDefinition.Get() : nullptr;
			const UClass* WidgetClass = IsValid(Definition) ? Definition->WidgetClass.Get() : nullptr;
			if (IsValid(WidgetClass) && !WidgetClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				Next.Definition = Definition;
				Next.WeaponProfile = Profile;
				Next.WeaponItemId = Item->GetInstanceId();
				Next.bAiming = ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Ability_State_Aiming);
				// GA_FirearmFire currently uses this full cone angle in every firing mode.
				// Legacy ASC spread and cosmetic movement/recoil bloom are not shot inputs.
				if (WeaponDefinition->bUsesDetachableMagazines && FMath::IsFinite(WeaponDefinition->SpreadAim))
				{
					Next.SpreadAngleDegrees = FMath::Clamp(WeaponDefinition->SpreadAim, 0.f, 180.f);
				}
				// Read current source values: tag callbacks can run before the other UI snapshots
				// refresh, and inventory menu listeners may be invoked in either order.
				const UAZ_VitalsAttributeSet* Vitals = ASC->GetSet<UAZ_VitalsAttributeSet>();
				const bool bAlive = IsValid(Vitals) && FMath::IsFinite(Vitals->GetHealth())
					&& FMath::IsFinite(Vitals->GetMaxHealth()) && Vitals->GetHealth() > 0.f && Vitals->GetMaxHealth() > 0.f;
				Next.bVisible = bAlive && !Inventory->IsMenuOpen()
					&& !ASC->HasAnyMatchingGameplayTags(ReticleBlockedTags())
					&& (!Definition->bAimOnly || Next.bAiming);
			}
		}
	}

	if (!ReticleViewsEqual(ReticleView, Next))
	{
		ReticleView = Next;
		OnReticleChanged.Broadcast(ReticleView);
	}
}

void UAZ_PlayerUIComponent::SetPresentationWeapon(AAZ_Weapon* Weapon)
{
	if (PresentationWeapon.Get() == Weapon)
	{
		if (!Weapon)
		{
			PresentationWeapon.Reset();
			WeaponOwnershipChangedHandle.Reset();
		}
		return;
	}
	if (AAZ_Weapon* Previous = PresentationWeapon.Get())
	{
		Previous->OnFirearmHitConfirmed.RemoveDynamic(this, &ThisClass::HandleHitConfirmed);
		Previous->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleWeaponDestroyed);
		Previous->OnOwnershipChanged.Remove(WeaponOwnershipChangedHandle);
	}
	WeaponOwnershipChangedHandle.Reset();
	PresentationWeapon = Weapon;
	if (Weapon)
	{
		Weapon->OnFirearmHitConfirmed.AddUniqueDynamic(this, &ThisClass::HandleHitConfirmed);
		Weapon->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleWeaponDestroyed);
		WeaponOwnershipChangedHandle = Weapon->OnOwnershipChanged.AddUObject(this, &ThisClass::HandleWeaponOwnershipChanged);
	}
}

void UAZ_PlayerUIComponent::HandleVitalsChanged(const FOnAttributeChangeData& Change)
{
	RefreshVitals();
	RefreshReticle();
}

void UAZ_PlayerUIComponent::HandleReticleTagChanged(FGameplayTag Tag, int32 Count)
{
	RefreshReticle();
}

void UAZ_PlayerUIComponent::HandleWeaponOwnershipChanged()
{
	RefreshReticle();
}

void UAZ_PlayerUIComponent::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	RefreshBindings();
}

void UAZ_PlayerUIComponent::HandleInventoryChanged()
{
	RefreshWeapon();
	RefreshReticle();
}

void UAZ_PlayerUIComponent::HandleEquipmentChanged()
{
	RefreshWeapon();
	RefreshReticle();
}

void UAZ_PlayerUIComponent::HandleInventoryVisibilityChanged(bool bOpen)
{
	bInventoryOpen = bOpen;
	RefreshReticle();
	OnInventoryVisibilityChanged.Broadcast(bInventoryOpen);
}

void UAZ_PlayerUIComponent::HandleInventoryFull()
{
	OnInventoryFull.Broadcast();
}

void UAZ_PlayerUIComponent::HandleHitConfirmed(const FHitResult& Hit)
{
	if (OwningController.IsValid() && OwningController->IsLocalController() && PresentationWeapon.IsValid()
		&& IsValid(OwningController->GetPawn()) && PresentationWeapon->GetOwner() == OwningController->GetPawn()
		&& WeaponView.bUsesMagazines && BoundEquipment.IsValid()
		&& BoundEquipment->GetActiveWeapon() == PresentationWeapon.Get())
	{
		OnHitConfirmed.Broadcast();
	}
}

void UAZ_PlayerUIComponent::HandleWeaponDestroyed(AActor* DestroyedActor)
{
	SetPresentationWeapon(nullptr);
	RefreshWeapon();
	RefreshReticle();
}
