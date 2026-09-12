// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"

#include "Net/UnrealNetwork.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "GameFramework/Actor.h"

void UAZ_Inv_CommonUI_InventoryItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	UObject::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, ItemManifest);
	DOREPLIFETIME(ThisClass, TotalStackCount);
	DOREPLIFETIME(ThisClass, InstanceState);
	DOREPLIFETIME_CONDITION(ThisClass, FirearmSpread, COND_OwnerOnly);
}

bool UAZ_Inv_CommonUI_InventoryItem::IsStackable() const
{
	return GetItemManifest().IsStackable();
}

bool UAZ_Inv_CommonUI_InventoryItem::IsConsumable() const
{
	return GetItemManifest().GetItemCategory() == EInv_ItemCategory::Consumable;
}

bool UAZ_Inv_CommonUI_InventoryItem::IsMagazine() const
{
	return GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_MagazineFragment>() != nullptr;
}

bool UAZ_Inv_CommonUI_InventoryItem::IsWeapon() const
{
	const auto* Weapon = GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
	return Weapon && Weapon->IsWeaponItem();
}

FGameplayTag UAZ_Inv_CommonUI_InventoryItem::GetWeaponProfileTag() const
{
	const auto* Weapon = GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
	return Weapon ? Weapon->WeaponTag : FGameplayTag::EmptyTag;
}

int32 UAZ_Inv_CommonUI_InventoryItem::GetMagazineCapacity() const
{
	const auto* Magazine = GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_MagazineFragment>();
	return Magazine ? Magazine->Capacity : 0;
}

void UAZ_Inv_CommonUI_InventoryItem::InitializeInstance(const FAZ_InventoryItemState& State, int32 StackCount)
{
	InstanceState = State;
	TotalStackCount = IsStackable() ? FMath::Max(1, StackCount) : 1;
	// Recoil survives ordinary selection/ability changes on this item, but is not
	// durable ammo or pickup state. A new/reconstructed item explicitly starts settled.
	FirearmSpread = FAZ_FirearmSpreadState();
}

float UAZ_Inv_CommonUI_InventoryItem::GetFirearmExtraSpreadRadiusDegrees(double ServerTime) const
{
	if (!IsInitialized() || !FMath::IsFinite(ServerTime)) return 0.f;
	const auto* Definition = GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
	if (!Definition || !Definition->bUsesDetachableMagazines || !Definition->Recoil.bEnabled
		|| !FMath::IsFinite(FirearmSpread.PeakExtraRadiusDegrees)
		|| !FMath::IsFinite(FirearmSpread.LastShotServerTime)
		|| !FMath::IsFinite(FirearmSpread.RecoveryDelaySeconds)
		|| !FMath::IsFinite(FirearmSpread.RecoverySpeedDegreesPerSecond)) return 0.f;
	const float Baseline = FMath::IsFinite(Definition->SpreadAim) ? FMath::Clamp(Definition->SpreadAim, 0.f, 180.f) : 0.f;
	const double RecoveryTime = FMath::Max(0.0, ServerTime - FirearmSpread.LastShotServerTime
		- FMath::Clamp(FirearmSpread.RecoveryDelaySeconds, 0.f, 60.f));
	const double RecoverySpeed = FMath::Clamp(FirearmSpread.RecoverySpeedDegreesPerSecond, 0.01f, 360.f);
	const double Remaining = FirearmSpread.PeakExtraRadiusDegrees - RecoveryTime * RecoverySpeed;
	return static_cast<float>(FMath::Clamp(Remaining, 0.0, 90.0 - Baseline * 0.5));
}

float UAZ_Inv_CommonUI_InventoryItem::GetFirearmSpreadAngleDegrees(double ServerTime) const
{
	if (!IsInitialized()) return 0.f;
	const auto* Definition = GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
	if (!Definition || !Definition->bUsesDetachableMagazines || !FMath::IsFinite(Definition->SpreadAim)) return 0.f;
	return FMath::Clamp(FMath::Clamp(Definition->SpreadAim, 0.f, 180.f)
		+ 2.f * GetFirearmExtraSpreadRadiusDegrees(ServerTime), 0.f, 180.f);
}

void UAZ_Inv_CommonUI_InventoryItem::RecordAcceptedFirearmShot(double ServerTime, const FAZ_FirearmRecoilSettings& Settings)
{
	const AActor* Owner = GetTypedOuter<AActor>();
	if (!Owner || !Owner->HasAuthority() || !IsInitialized() || !FMath::IsFinite(ServerTime)) return;
	const auto* Definition = GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
	if (!Definition || !Definition->bUsesDetachableMagazines) return;
	if (!Settings.bEnabled)
	{
		FirearmSpread = FAZ_FirearmSpreadState();
		return;
	}
	const float Baseline = FMath::IsFinite(Definition->SpreadAim) ? FMath::Clamp(Definition->SpreadAim, 0.f, 180.f) : 0.f;
	const float MaxRadius = FMath::IsFinite(Settings.MaxAdditionalSpreadRadiusDegrees)
		? FMath::Clamp(Settings.MaxAdditionalSpreadRadiusDegrees, 0.f, 90.f - Baseline * 0.5f) : 0.f;
	const float Increment = FMath::IsFinite(Settings.SpreadRadiusPerShotDegrees)
		? FMath::Clamp(Settings.SpreadRadiusPerShotDegrees, 0.f, 90.f) : 0.f;
	const float NextRadius = FMath::Min(MaxRadius, GetFirearmExtraSpreadRadiusDegrees(ServerTime) + Increment);
	FirearmSpread.PeakExtraRadiusDegrees = NextRadius;
	FirearmSpread.LastShotServerTime = ServerTime;
	FirearmSpread.RecoveryDelaySeconds = FMath::IsFinite(Settings.SpreadRecoveryDelaySeconds)
		? FMath::Clamp(Settings.SpreadRecoveryDelaySeconds, 0.f, 60.f) : 0.f;
	FirearmSpread.RecoverySpeedDegreesPerSecond = FMath::IsFinite(Settings.SpreadRecoverySpeedDegreesPerSecond)
		? FMath::Clamp(Settings.SpreadRecoverySpeedDegreesPerSecond, 0.01f, 360.f) : 1.25f;
}

void UAZ_Inv_CommonUI_InventoryItem::OnRep_ItemChanged()
{
	if (AActor* Owner = GetTypedOuter<AActor>())
	{
		if (auto* Inventory = Owner->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>())
		{
			Inventory->NotifyInventoryChanged();
		}
	}
}

void UAZ_Inv_CommonUI_InventoryItem::SetItemManifest(const FAZ_Inv_CommonUI_ItemManifest& Manifest)
{
	ItemManifest = FInstancedStruct::Make<FAZ_Inv_CommonUI_ItemManifest>(Manifest);
}

const FAZ_Inv_CommonUI_ItemManifest& UAZ_Inv_CommonUI_InventoryItem::GetItemManifest() const
{
	if (const FAZ_Inv_CommonUI_ItemManifest* ManifestPtr = ItemManifest.GetPtr<FAZ_Inv_CommonUI_ItemManifest>())
	{
		return *ManifestPtr;
	}

	ensureMsgf(false, TEXT("ItemManifestData is invalid or wrong type, returning default manifest"));
	static const FAZ_Inv_CommonUI_ItemManifest DefaultManifest;
	return DefaultManifest;
}
FAZ_Inv_CommonUI_ItemManifest& UAZ_Inv_CommonUI_InventoryItem::GetItemManifestMutable()
{
	ensure(ItemManifest.IsValid());
	return ItemManifest.GetMutable<FAZ_Inv_CommonUI_ItemManifest>();
}
