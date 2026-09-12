#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"

#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AZ_GameplayTags.h"
#include "Engine/World.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h"
#include "Player/AZ_PlayerController.h"
#include "TimerManager.h"
#include "Weapon/AZ_Weapon.h"

double UAZ_Inv_CommonUI_EquipmentComponent::GetFirearmReadyServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World) return 0.0;
	const AGameStateBase* GameState = World->GetGameState();
	return GetOwner() && !GetOwner()->HasAuthority() && GameState
		? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

float UAZ_Inv_CommonUI_EquipmentComponent::GetFirearmReadyDuration() const
{
	const auto* Definition = IsValid(Selection.Item)
		? Selection.Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>() : nullptr;
	return Definition && FMath::IsFinite(Definition->ReadyDurationSeconds)
		? FMath::Clamp(Definition->ReadyDurationSeconds, 0.1f, 60.f) : 3.f;
}

bool UAZ_Inv_CommonUI_EquipmentComponent::ValidateFirearmReadySource(const AAZ_Weapon* ExpectedSource,
	const FGuid& ExpectedItemId, uint32 ExpectedGeneration) const
{
	const AAZ_PlayerController* Player = Cast<AAZ_PlayerController>(OwningPlayerController.Get());
	const UAZ_AbilitySystemComponent* ASC = GetASC();
	if (!GetOwner() || !Player || (!GetOwner()->HasAuthority() && !Player->IsLocalController())
		|| Player->IsInventoryInputCaptured() || !GetWorld() || !ASC
		|| !IsActiveWeaponSource(ExpectedSource) || Selection.Generation != ExpectedGeneration
		|| !ExpectedItemId.IsValid() || Selection.Item->GetInstanceId() != ExpectedItemId) return false;
	const auto* Definition = Selection.Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
	return Definition && Definition->bUsesDetachableMagazines
		&& !ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Ability_State_MeleeAttacking)
		&& !ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Movement_Sprinting);
}

bool UAZ_Inv_CommonUI_EquipmentComponent::IsFirearmRaised() const
{
	const UAZ_AbilitySystemComponent* ASC = GetASC();
	if (!ASC || !IsActiveWeaponSource(Selection.Weapon)) return false;
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	return ASC->HasMatchingGameplayTag(Tags.Ability_State_Aiming)
		|| ASC->HasMatchingGameplayTag(Tags.Ability_State_FirearmReady);
}

bool UAZ_Inv_CommonUI_EquipmentComponent::BeginFirearmPreparation(AAZ_Weapon* ExpectedSource,
	const FGuid& ExpectedItemId, uint32 ExpectedGeneration, float& OutRaiseDelay, int32 PreparationKey)
{
	return StartFirearmReady(ExpectedSource, ExpectedItemId, ExpectedGeneration, false, OutRaiseDelay, PreparationKey);
}

void UAZ_Inv_CommonUI_EquipmentComponent::CancelUnfulfilledFirearmPreparation(AAZ_Weapon* ExpectedSource,
	const FGuid& ExpectedItemId, uint32 ExpectedGeneration)
{
	if (!FirearmReady.LifetimeId.IsValid() || !FirearmReady.bOwnsReadyTag
		|| FirearmReady.Weapon.Get() != ExpectedSource || FirearmReady.ItemId != ExpectedItemId
		|| FirearmReady.Generation != ExpectedGeneration || !FirearmReady.AcceptedShotIds.IsEmpty()
		|| FirearmReady.ReloadActionId.IsValid() || FirearmReady.AutoFireActionId.IsValid()) return;
	// On the owner, the fire-cancel receipt can precede the weapon's reload-hold RPC.
	// A live reload keeps the preparation available for that exact, validated hand-off.
	if (const UAZ_AbilitySystemComponent* ASC = FirearmReady.ASC.Get();
		ASC && ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Ability_State_Reloading)) return;
	CancelFirearmReady();
}

bool UAZ_Inv_CommonUI_EquipmentComponent::StartFirearmReady(AAZ_Weapon* ExpectedSource,
	const FGuid& ExpectedItemId, uint32 ExpectedGeneration, bool bForReloadHold, float& OutRaiseDelay, int32 PreparationKey)
{
	OutRaiseDelay = 0.f;
	BindAbilityEvents();
	if (!ValidateFirearmReadySource(ExpectedSource, ExpectedItemId, ExpectedGeneration)) return false;
	UAZ_AbilitySystemComponent* ASC = GetASC();
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	if (!bForReloadHold && ASC->HasMatchingGameplayTag(Tags.Ability_State_Reloading)) return false;
	const double Now = GetFirearmReadyServerTime();
	const bool bCurrentLifetime = FirearmReady.LifetimeId.IsValid() && FirearmReady.bOwnsReadyTag
		&& FirearmReady.Weapon.Get() == ExpectedSource && FirearmReady.ItemId == ExpectedItemId
		&& FirearmReady.Generation == ExpectedGeneration && FirearmReady.ASC.Get() == ASC
		&& FirearmReady.Pawn.Get() == OwningPlayerController->GetPawn();
	if (bCurrentLifetime)
	{
		if (!FirearmReady.ReloadActionId.IsValid() && !FirearmReady.AutoFireActionId.IsValid()
			&& FirearmReady.ExpiresServerTime <= Now)
		{
			// A fresh press can beat the expired timer callback. The pose is still raised;
			// renew its ownership without a zero-tag pulse that would cancel that press.
			FirearmReady.LifetimeId = FGuid::NewGuid();
			FirearmReady.StartedServerTime = Now;
			FirearmReady.ExpiresServerTime = Now + GetFirearmReadyDuration();
			FirearmReady.RaisedServerTime = Now;
			FirearmReady.AcceptedShotIds.Reset();
			FirearmReady.PreparationKeys.Reset();
			ScheduleFirearmReadyExpiry();
		}
		// A rejected/dry trigger must not keep extending the grace period.
		OutRaiseDelay = ASC->HasMatchingGameplayTag(Tags.Ability_State_Aiming)
			? 0.f : static_cast<float>(FMath::Max(0.0, FirearmReady.RaisedServerTime - Now));
		RecordFirearmPreparationKey(PreparationKey);
		return true;
	}
	ClearFirearmReady(false);
	if (!ValidateFirearmReadySource(ExpectedSource, ExpectedItemId, ExpectedGeneration)) return false;
	ASC = GetASC();
	const auto* Definition = Selection.Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
	const bool bPrecisionAiming = ASC->HasMatchingGameplayTag(Tags.Ability_State_Aiming);
	const float RaiseDelay = bPrecisionAiming || bForReloadHold ? 0.f
		: FMath::IsFinite(Definition->FirearmRaiseDelaySeconds)
			? FMath::Clamp(Definition->FirearmRaiseDelaySeconds, 0.f, 2.f) : 0.12f;
	const FGuid LifetimeId = FGuid::NewGuid();
	FirearmReady.LifetimeId = LifetimeId;
	FirearmReady.ItemId = ExpectedItemId;
	FirearmReady.Weapon = ExpectedSource;
	FirearmReady.Pawn = OwningPlayerController->GetPawn();
	FirearmReady.ASC = ASC;
	FirearmReady.Generation = ExpectedGeneration;
	FirearmReady.StartedServerTime = Now;
	FirearmReady.ExpiresServerTime = Now + RaiseDelay + GetFirearmReadyDuration();
	FirearmReady.RaisedServerTime = Now + RaiseDelay;
	// Claim each contribution before publishing it: tag listeners may synchronously cancel us.
	FirearmReady.bOwnsReadyTag = true;
	ASC->AddStateTag(Tags.Ability_State_FirearmReady);
	if (FirearmReady.LifetimeId != LifetimeId) return false;
	FirearmReady.bOwnsStrafeTag = true;
	ASC->AddStateTag(Tags.Movement_Strafe);
	if (FirearmReady.LifetimeId != LifetimeId) return false;
	if (!ValidateFirearmReadySource(ExpectedSource, ExpectedItemId, ExpectedGeneration))
	{
		CancelFirearmReady();
		return false;
	}
	ScheduleFirearmReadyExpiry();
	ReconcilePresentation();
	if (FirearmReady.LifetimeId != LifetimeId) return false;
	RecordFirearmPreparationKey(PreparationKey);
	OutRaiseDelay = RaiseDelay;
	UE_LOG(LogTemp, Display, TEXT("[FirearmReady] begin item=%s lifetime=%s delay=%.3f expires=%.3f"),
		*ExpectedItemId.ToString(), *LifetimeId.ToString(), RaiseDelay, FirearmReady.ExpiresServerTime);
	return FirearmReady.LifetimeId == LifetimeId;
}

void UAZ_Inv_CommonUI_EquipmentComponent::RecordFirearmPreparationKey(int32 PreparationKey)
{
	if (PreparationKey <= 0 || !FirearmReady.LifetimeId.IsValid() || FirearmReady.PreparationKeys.Contains(PreparationKey)) return;
	if (FirearmReady.PreparationKeys.Num() == 64) FirearmReady.PreparationKeys.RemoveAt(0);
	FirearmReady.PreparationKeys.Add(PreparationKey);
}

bool UAZ_Inv_CommonUI_EquipmentComponent::RefreshFirearmReadyAfterShot(AAZ_Weapon* ExpectedSource,
	const FGuid& ExpectedItemId, uint32 ExpectedGeneration, const FGuid& ShotId, double AcceptedShotServerTime,
	int32 AcceptedPreparationKey)
{
	if (!ShotId.IsValid() || !FirearmReady.LifetimeId.IsValid() || !FirearmReady.bOwnsReadyTag
		|| FirearmReady.Weapon.Get() != ExpectedSource || FirearmReady.ItemId != ExpectedItemId
		|| FirearmReady.Generation != ExpectedGeneration
		|| FirearmReady.ASC.Get() != GetASC() || !OwningPlayerController.IsValid()
		|| FirearmReady.Pawn.Get() != OwningPlayerController->GetPawn()
		|| !ValidateFirearmReadySource(ExpectedSource, ExpectedItemId, ExpectedGeneration)) return false;
	const double Now = GetFirearmReadyServerTime();
	if (AcceptedShotServerTime < 0.0 && GetOwner()->HasAuthority()) AcceptedShotServerTime = Now;
	// Prediction keys correlate the owner's preparation with authority's receipt without
	// comparing two estimated start clocks. Cancelling a lifetime discards all of its keys.
	const bool bCorrelatedOwner = !GetOwner()->HasAuthority() && AcceptedPreparationKey > 0;
	if (bCorrelatedOwner && !FirearmReady.PreparationKeys.Contains(AcceptedPreparationKey)) return false;
	if (!FMath::IsFinite(AcceptedShotServerTime) || AcceptedShotServerTime < 0.0
		|| (!bCorrelatedOwner && (AcceptedShotServerTime < FirearmReady.StartedServerTime
			|| (FirearmReadyCancellationWorld.Get() == GetWorld() && AcceptedShotServerTime < FirearmReadyCancelledServerTime)))
		|| AcceptedShotServerTime + GetFirearmReadyDuration() <= Now) return false;
	// A listen host sees this receipt once in gameplay and again in its owning-player cosmetic RPC.
	// It is still valid for cosmetics; only the deadline mutation is deduplicated here.
	if (FirearmReady.AcceptedShotIds.Contains(ShotId)) return true;
	const bool bFirstAcceptedShot = FirearmReady.AcceptedShotIds.IsEmpty();
	if (FirearmReady.AcceptedShotIds.Num() == 32) FirearmReady.AcceptedShotIds.RemoveAt(0);
	FirearmReady.AcceptedShotIds.Add(ShotId);
	const double AcceptedDeadline = AcceptedShotServerTime + GetFirearmReadyDuration();
	FirearmReady.ExpiresServerTime = bFirstAcceptedShot ? AcceptedDeadline
		: FMath::Max(FirearmReady.ExpiresServerTime, AcceptedDeadline);
	FirearmReady.RaisedServerTime = FMath::Min(FirearmReady.RaisedServerTime, AcceptedShotServerTime);
	ScheduleFirearmReadyExpiry();
	return true;
}

bool UAZ_Inv_CommonUI_EquipmentComponent::BeginFirearmAutoHold(AAZ_Weapon* ExpectedSource,
	const FGuid& ExpectedItemId, uint32 ExpectedGeneration, const FGuid& FireActionId)
{
	if (!FireActionId.IsValid() || !FirearmReady.LifetimeId.IsValid() || !FirearmReady.bOwnsReadyTag
		|| FirearmReady.ReloadActionId.IsValid() || FirearmReady.Weapon.Get() != ExpectedSource
		|| FirearmReady.ItemId != ExpectedItemId || FirearmReady.Generation != ExpectedGeneration
		|| FirearmReady.ASC.Get() != GetASC() || !OwningPlayerController.IsValid()
		|| FirearmReady.Pawn.Get() != OwningPlayerController->GetPawn()
		|| !ValidateFirearmReadySource(ExpectedSource, ExpectedItemId, ExpectedGeneration)) return false;
	if (FirearmReady.AutoFireActionId.IsValid()) return FirearmReady.AutoFireActionId == FireActionId;
	FirearmReady.AutoFireActionId = FireActionId;
	GetWorld()->GetTimerManager().ClearTimer(FirearmReadyTimer);
	return true;
}

void UAZ_Inv_CommonUI_EquipmentComponent::EndFirearmAutoHold(const FGuid& FireActionId)
{
	if (!FireActionId.IsValid() || FirearmReady.AutoFireActionId != FireActionId) return;
	FirearmReady.AutoFireActionId.Invalidate();
	ScheduleFirearmReadyExpiry();
}

bool UAZ_Inv_CommonUI_EquipmentComponent::BeginFirearmReloadHold(AAZ_Weapon* ExpectedSource,
	const FGuid& ExpectedItemId, uint32 ExpectedGeneration, const FGuid& ReloadActionId, bool bAuthoritativeRaised,
	double ReloadStartedServerTime)
{
	if (!ReloadActionId.IsValid() || !ValidateFirearmReadySource(ExpectedSource, ExpectedItemId, ExpectedGeneration)
		|| (!bAuthoritativeRaised && !IsFirearmRaised())) return false;
	if (FirearmReady.ReloadActionId.IsValid()) return FirearmReady.ReloadActionId == ReloadActionId;
	const double Now = GetFirearmReadyServerTime();
	if (bAuthoritativeRaised && (!FMath::IsFinite(ReloadStartedServerTime) || ReloadStartedServerTime < 0.0
		|| ReloadStartedServerTime > Now + 1.0
		|| (FirearmReadyCancellationWorld.Get() == GetWorld() && ReloadStartedServerTime < FirearmReadyCancelledServerTime)
		|| (FirearmReady.LifetimeId.IsValid() && ReloadStartedServerTime < FirearmReady.StartedServerTime))) return false;
	const bool bHadReadyLifetime = FirearmReady.LifetimeId.IsValid() && FirearmReady.bOwnsReadyTag
		&& FirearmReady.Weapon.Get() == ExpectedSource && FirearmReady.ItemId == ExpectedItemId
		&& FirearmReady.Generation == ExpectedGeneration
		&& (FirearmReady.ExpiresServerTime > Now || FirearmReady.AutoFireActionId.IsValid());
	float IgnoredRaiseDelay = 0.f;
	if (!StartFirearmReady(ExpectedSource, ExpectedItemId, ExpectedGeneration, true, IgnoredRaiseDelay)) return false;
	FirearmReady.ReloadActionId = ReloadActionId;
	FirearmReady.bCreatedForReloadHold = !bHadReadyLifetime;
	GetWorld()->GetTimerManager().ClearTimer(FirearmReadyTimer);
	return true;
}

void UAZ_Inv_CommonUI_EquipmentComponent::EndFirearmReloadHold(const FGuid& ReloadActionId, bool bCommitted)
{
	if (!ReloadActionId.IsValid() || FirearmReady.ReloadActionId != ReloadActionId) return;
	const bool bCreatedForHold = FirearmReady.bCreatedForReloadHold;
	FirearmReady.ReloadActionId.Invalidate();
	FirearmReady.bCreatedForReloadHold = false;
	if ((!bCommitted && bCreatedForHold)
		|| !ValidateFirearmReadySource(FirearmReady.Weapon.Get(), FirearmReady.ItemId, FirearmReady.Generation))
	{
		CancelFirearmReady();
		return;
	}
	if (bCommitted) FirearmReady.ExpiresServerTime = GetFirearmReadyServerTime() + GetFirearmReadyDuration();
	ScheduleFirearmReadyExpiry();
}

void UAZ_Inv_CommonUI_EquipmentComponent::ScheduleFirearmReadyExpiry()
{
	UWorld* World = GetWorld();
	if (!World || !FirearmReady.LifetimeId.IsValid()) return;
	World->GetTimerManager().ClearTimer(FirearmReadyTimer);
	if (FirearmReady.ReloadActionId.IsValid() || FirearmReady.AutoFireActionId.IsValid()) return;
	const double Remaining = FirearmReady.ExpiresServerTime - GetFirearmReadyServerTime();
	if (Remaining <= 0.0)
	{
		ClearFirearmReady(false);
		return;
	}
	const FTimerDelegate Expire = FTimerDelegate::CreateUObject(this, &ThisClass::OnFirearmReadyExpired, FirearmReady.LifetimeId);
	World->GetTimerManager().SetTimer(FirearmReadyTimer, Expire, static_cast<float>(FMath::Max(0.001, Remaining)), false);
}

void UAZ_Inv_CommonUI_EquipmentComponent::OnFirearmReadyExpired(FGuid ExpectedLifetimeId)
{
	if (FirearmReady.LifetimeId != ExpectedLifetimeId || FirearmReady.ReloadActionId.IsValid()
		|| FirearmReady.AutoFireActionId.IsValid()) return;
	if (FirearmReady.ExpiresServerTime > GetFirearmReadyServerTime())
	{
		ScheduleFirearmReadyExpiry();
		return;
	}
	ClearFirearmReady(false);
}

void UAZ_Inv_CommonUI_EquipmentComponent::CancelFirearmReady()
{
	ClearFirearmReady(true);
}

void UAZ_Inv_CommonUI_EquipmentComponent::ClearFirearmReady(bool bExplicitCancellation)
{
	if (bExplicitCancellation)
	{
		const double Now = GetFirearmReadyServerTime();
		FirearmReadyCancelledServerTime = FirearmReadyCancellationWorld.Get() == GetWorld()
			? FMath::Max(FirearmReadyCancelledServerTime, Now) : Now;
		FirearmReadyCancellationWorld = GetWorld();
	}
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(FirearmReadyTimer);
	const FFirearmReadyState Previous = MoveTemp(FirearmReady);
	FirearmReady = FFirearmReadyState();
	// Clear ownership before notifications; a listener may already begin another lifetime.
	if (UAZ_AbilitySystemComponent* ASC = Previous.ASC.Get())
	{
		const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
		if (Previous.bOwnsReadyTag) ASC->RemoveStateTag(Tags.Ability_State_FirearmReady);
		if (Previous.bOwnsStrafeTag) ASC->RemoveStateTag(Tags.Movement_Strafe);
	}
	if (Previous.LifetimeId.IsValid())
	{
		ReconcilePresentation();
		UE_LOG(LogTemp, Display, TEXT("[FirearmReady] end item=%s lifetime=%s"),
			*Previous.ItemId.ToString(), *Previous.LifetimeId.ToString());
	}
}
