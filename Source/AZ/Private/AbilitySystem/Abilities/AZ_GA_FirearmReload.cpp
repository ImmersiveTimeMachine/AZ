#include "AbilitySystem/Abilities/AZ_GA_FirearmReload.h"

#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AZ_WeaponAnimationProfile.h"
#include "AZ_GameplayTags.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Engine/World.h"
#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "Player/AZ_PlayerController.h"
#include "TimerManager.h"
#include "Weapon/AZ_Weapon.h"

namespace
{
	struct FReloadSource
	{
		AAZ_PawnMoverHeroCharacter* Hero = nullptr;
		UAZ_Inv_CommonUI_EquipmentComponent* Equipment = nullptr;
		UAZ_Inv_CommonUI_InventoryComponent* Inventory = nullptr;
		UAZ_PawnMoverComponent* Mover = nullptr;
		UAZ_Inv_CommonUI_InventoryItem* Item = nullptr;
		AAZ_Weapon* Weapon = nullptr;
		const UAZ_WeaponAnimationProfile* Profile = nullptr;
	};

	bool ResolveReloadSource(const FGameplayAbilityActorInfo* ActorInfo, const UObject* Source, FReloadSource& Out)
	{
		Out.Hero = ActorInfo ? Cast<AAZ_PawnMoverHeroCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
		const AAZ_PlayerController* Controller = Out.Hero ? Cast<AAZ_PlayerController>(Out.Hero->GetController()) : nullptr;
		if (!Controller || Controller->IsInventoryInputCaptured()) return false;
		Out.Equipment = Controller->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
		Out.Inventory = Controller->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>();
		Out.Mover = Out.Hero->GetMoverComponent();
		if (!Out.Equipment || !Out.Inventory || !Out.Mover || !Out.Equipment->IsActiveWeaponSource(Source)) return false;
		Out.Item = Out.Equipment->GetActiveItem();
		Out.Weapon = Out.Equipment->GetActiveWeapon();
		const auto* Definition = Out.Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
		Out.Profile = Definition ? Definition->AnimationProfile.Get() : nullptr;
		return Definition && Definition->bUsesDetachableMagazines && IsValid(Out.Profile);
	}

	bool SelectReloadAnimation(const FReloadSource& Source, bool bRaised,
		UAnimSequence*& OutSequence, float& OutDuration)
	{
		OutSequence = Source.Mover->IsCrouching()
			? (bRaised ? Source.Profile->CrouchingAimReloadAnimation.Get() : Source.Profile->CrouchingReloadAnimation.Get())
			: (bRaised ? Source.Profile->StandingAimReloadAnimation.Get() : Source.Profile->StandingReloadAnimation.Get());
		if (!IsValid(OutSequence)) return false;
		const double Length = OutSequence->GetPlayLength();
		const double Rate = static_cast<double>(OutSequence->RateScale) * Source.Profile->ReloadAnimationPlayRate;
		if (!FMath::IsFinite(Length) || Length <= 0.0 || !FMath::IsFinite(OutSequence->RateScale)
			|| OutSequence->RateScale <= 0.f || !FMath::IsFinite(Source.Profile->ReloadAnimationPlayRate)
			|| Source.Profile->ReloadAnimationPlayRate <= 0.f || !FMath::IsFinite(Rate) || Rate <= 0.0
			|| !FMath::IsFinite(Source.Profile->ReloadAnimationBlendIn) || Source.Profile->ReloadAnimationBlendIn < 0.f
			|| !FMath::IsFinite(Source.Profile->ReloadAnimationBlendOut) || Source.Profile->ReloadAnimationBlendOut < 0.f) return false;
		OutDuration = static_cast<float>(Length / Rate);
		return FMath::IsFinite(OutDuration) && OutDuration > UE_KINDA_SMALL_NUMBER;
	}
}

UAZ_GA_FirearmReload::UAZ_GA_FirearmReload()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// Empty-shot requests originate on authority. ServerInitiated also accepts a
	// normal GAS request from R and mirrors its active lifetime to the owning client.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	bServerRespectsRemoteAbilityCancellation = true;
}

bool UAZ_GA_FirearmReload::TryActivateMagazineRequest(UAbilitySystemComponent* ASC,
	FGameplayAbilitySpecHandle Handle, AAZ_Weapon* ExpectedSource, const FGuid& ExpectedItemId,
	uint32 ExpectedGeneration, const FGuid& RequestedMagazineId, bool bSkipEmpty)
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative() || IsActive() || bEndingReload || bMagazineRequestInProgress
		|| !IsValid(ExpectedSource) || !ExpectedItemId.IsValid()) return false;
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
	if (!Spec || Spec->IsActive() || Spec->SourceObject.Get() != ExpectedSource || Spec->GetPrimaryInstance() != this) return false;
	FMagazineActivationRequest Request;
	Request.WeaponSource = ExpectedSource;
	Request.WeaponItemId = ExpectedItemId;
	Request.Generation = ExpectedGeneration;
	Request.MagazineItemId = RequestedMagazineId;
	Request.bSkipEmpty = bSkipEmpty;
	TGuardValue<bool> RequestGuard(bMagazineRequestInProgress, true);
	TGuardValue<FMagazineActivationRequest> ContextGuard(PendingMagazineRequest, Request);
	// InternalTryActivateAbility calls CanActivateAbility and ActivateAbility on
	// Spec->GetPrimaryInstance(). The context cannot survive failure or leak into R.
	return ASC->TryActivateAbility(Handle, false);
}

void UAZ_GA_FirearmReload::DeclareAbilityTags()
{
	Super::DeclareAbilityTags();
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	ActivationOwnedTags.AddTag(Tags.Ability_State_Reloading);
	ActivationBlockedTags.AddTag(Tags.Ability_State_Reloading);
	ActivationBlockedTags.AddTag(Tags.Character_Dead);
	ActivationBlockedTags.AddTag(Tags.Character_Dying);
	ActivationBlockedTags.AddTag(Tags.State_Grabbed);
	ActivationBlockedTags.AddTag(Tags.State_Combat_Grabbing);
	ActivationBlockedTags.AddTag(Tags.State_Combat_Staggered);
	ActivationBlockedTags.AddTag(Tags.State_Combat_StruckPair);
	ActivationBlockedTags.AddTag(Tags.Ability_State_MeleeAttacking);
	CancelAbilitiesWithTag.AddTag(Tags.Movement_Sprinting);
	BlockAbilitiesWithTag.AddTag(Tags.Movement_Sprinting);
}

bool UAZ_GA_FirearmReload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || bEndingReload || !Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	FReloadSource Source;
	UAnimSequence* Sequence = nullptr;
	float Duration = 0.f;
	if (!ResolveReloadSource(ActorInfo, GetSourceObject(Handle, ActorInfo), Source)
		|| !SelectReloadAnimation(Source, Source.Equipment->IsFirearmRaised(), Sequence, Duration)) return false;
	const FGuid ItemId = Source.Item->GetInstanceId();
	const uint32 Generation = Source.Equipment->GetSelectionGeneration();
	if (bMagazineRequestInProgress && (PendingMagazineRequest.WeaponSource.Get() != Source.Weapon
		|| PendingMagazineRequest.WeaponItemId != ItemId || PendingMagazineRequest.Generation != Generation)) return false;
	return Source.Inventory->CanReloadMagazine(Source.Weapon, ItemId, Generation,
		bMagazineRequestInProgress ? PendingMagazineRequest.MagazineItemId : FGuid(),
		bMagazineRequestInProgress && PendingMagazineRequest.bSkipEmpty);
}

void UAZ_GA_FirearmReload::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Snapshot before Super can run ability callbacks. Manual R has no explicit
	// context and walks the complete magazine ring; automatic requests skip empties.
	const FMagazineActivationRequest AcceptedRequest = PendingMagazineRequest;
	const bool bHasRequest = bMagazineRequestInProgress;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive()) return;
	FReloadSource Source;
	// A server-success activation bypasses the client's CanActivateAbility. UI,
	// source ownership or stance may already have changed while it was in flight.
	if (!ActorInfo || !ResolveReloadSource(ActorInfo, GetSourceObject(Handle, ActorInfo), Source))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!ActorInfo->IsNetAuthority())
	{
		// The owner mirrors only the cancellation lifetime. Its actual Mover stance
		// can cancel the authority before the movement update arrives there; no
		// client reservation, completion timer or ammunition mutation is created.
		ReloadedItemId = Source.Item->GetInstanceId();
		EquipmentGeneration = Source.Equipment->GetSelectionGeneration();
		bReloadCrouching = Source.Mover->IsCrouching();
		ReloadingWeapon = Source.Weapon;
		ReloadMover = Source.Mover;
		Source.Mover->OnStanceChanged.AddDynamic(this, &ThisClass::OnReloadStanceChanged);
		return;
	}
	if (bHasRequest && (AcceptedRequest.WeaponSource.Get() != Source.Weapon
		|| AcceptedRequest.WeaponItemId != Source.Item->GetInstanceId()
		|| AcceptedRequest.Generation != Source.Equipment->GetSelectionGeneration()))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	UAnimSequence* Sequence = nullptr;
	float Duration = 0.f;
	const UWorld* ReloadStartWorld = Source.Hero->GetWorld();
	if (!ReloadStartWorld)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const double ReloadStartedServerTime = ReloadStartWorld->GetTimeSeconds();
	bReloadRaised = Source.Equipment->IsFirearmRaised();
	if (!SelectReloadAnimation(Source, bReloadRaised, Sequence, Duration))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ReloadActionId = FGuid::NewGuid();
	ReloadedItemId = Source.Item->GetInstanceId();
	RequestedReloadMagazineId = bHasRequest ? AcceptedRequest.MagazineItemId : FGuid();
	bReloadSkipEmpty = bHasRequest && AcceptedRequest.bSkipEmpty;
	EquipmentGeneration = Source.Equipment->GetSelectionGeneration();
	bReloadCrouching = Source.Mover->IsCrouching();
	ReloadingWeapon = Source.Weapon;
	ReloadInventory = Source.Inventory;
	ReloadEquipment = Source.Equipment;
	ReloadMover = Source.Mover;
	const FGuid ActionId = ReloadActionId;
	const float PlayRate = Source.Profile->ReloadAnimationPlayRate;
	const float BlendIn = Source.Profile->ReloadAnimationBlendIn;
	const float BlendOut = Source.Profile->ReloadAnimationBlendOut;
	// Keep the selected raised grip through the complete reload, even if precision
	// aim is released or the normal Ready deadline passes. Relaxed reloads never raise.
	if (bReloadRaised && !Source.Equipment->BeginFirearmReloadHold(Source.Weapon, ReloadedItemId, EquipmentGeneration, ActionId))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!IsActive() || ReloadActionId != ActionId) return;
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!IsActive() || ReloadActionId != ActionId) return;
	FReloadSource CommittedSource;
	if (!ResolveReloadSource(ActorInfo, GetSourceObject(Handle, ActorInfo), CommittedSource)
		|| CommittedSource.Item->GetInstanceId() != ReloadedItemId || CommittedSource.Weapon != ReloadingWeapon.Get()
		|| CommittedSource.Inventory != ReloadInventory.Get() || CommittedSource.Mover != ReloadMover.Get()
		|| CommittedSource.Equipment->GetSelectionGeneration() != EquipmentGeneration
		|| CommittedSource.Mover->IsCrouching() != bReloadCrouching
		|| !CommittedSource.Inventory->TryBeginMagazineReload(CommittedSource.Weapon, ReloadedItemId, EquipmentGeneration,
			ActionId, RequestedReloadMagazineId, bReloadSkipEmpty))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!IsActive() || ReloadActionId != ActionId) return;
	CommittedSource.Equipment->CancelActiveFire();
	// Fire ending and its cosmetic stop can synchronously open UI, switch the
	// source, or cancel this reload. Never start the captured clip after that.
	FReloadSource Current;
	if (!IsActive() || ReloadActionId != ActionId) return;
	if (!ResolveReloadSource(ActorInfo, GetSourceObject(Handle, ActorInfo), Current)
		|| Current.Item->GetInstanceId() != ReloadedItemId || Current.Weapon != ReloadingWeapon.Get()
		|| Current.Inventory != ReloadInventory.Get() || !IsValid(Sequence)
		|| Current.Equipment->GetSelectionGeneration() != EquipmentGeneration
		|| Current.Mover != ReloadMover.Get() || Current.Mover->IsCrouching() != bReloadCrouching)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	UWorld* World = Current.Hero->GetWorld();
	if (!World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Current.Mover->OnStanceChanged.AddDynamic(this, &ThisClass::OnReloadStanceChanged);
	AnimationInterruptedHandle = Current.Weapon->OnReloadAnimationInterrupted.AddUObject(
		this, &ThisClass::OnReloadAnimationInterrupted);
	World->GetTimerManager().SetTimer(CompletionTimer,
		FTimerDelegate::CreateUObject(this, &ThisClass::OnReloadCompleted, ActionId), Duration, false);
	UE_LOG(LogTemp, Display, TEXT("[Reload] begin action=%s item=%s generation=%u sequence=%s duration=%.3f crouched=%d requestedMagazine=%s skipEmpty=%d"),
		*ActionId.ToString(), *ReloadedItemId.ToString(), EquipmentGeneration, *GetNameSafe(Sequence), Duration, bReloadCrouching,
		*RequestedReloadMagazineId.ToString(), bReloadSkipEmpty);
	Current.Weapon->Multicast_BeginReloadAnimation(ActionId, Sequence, PlayRate, BlendIn, BlendOut, bReloadCrouching,
		bReloadRaised, ReloadedItemId, EquipmentGeneration, ReloadStartedServerTime);
}

void UAZ_GA_FirearmReload::OnReloadCompleted(FGuid ExpectedActionId)
{
	if (!IsActive() || ReloadActionId != ExpectedActionId || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()) return;
	FReloadSource Source;
	if (!ResolveReloadSource(CurrentActorInfo, GetCurrentSourceObject(), Source)
		|| Source.Item->GetInstanceId() != ReloadedItemId || Source.Weapon != ReloadingWeapon.Get()
		|| Source.Inventory != ReloadInventory.Get() || Source.Mover != ReloadMover.Get()
		|| Source.Equipment->GetSelectionGeneration() != EquipmentGeneration
		|| Source.Mover->IsCrouching() != bReloadCrouching)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	// Completion is the only gameplay commit point in this first reload slice.
	// Montage blend-out and presentation notifies never transfer ammunition.
	const bool bCommitted = Source.Inventory->TryCommitMagazineReload(ExpectedActionId);
	if (IsActive() && ReloadActionId == ExpectedActionId)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bCommitted);
	}
}

void UAZ_GA_FirearmReload::OnReloadStanceChanged(EStanceMode PreviousStance, EStanceMode NewStance)
{
	if (PreviousStance != NewStance && IsActive() && CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UAZ_GA_FirearmReload::OnReloadAnimationInterrupted(const FGuid& InterruptedActionId)
{
	if (IsActive() && ReloadActionId == InterruptedActionId && CurrentActorInfo)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UAZ_GA_FirearmReload::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bEndingReload || !IsEndAbilityValid(Handle, ActorInfo)) return;
	TGuardValue<bool> EndingGuard(bEndingReload, true);
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(CompletionTimer);
	if (UAZ_PawnMoverComponent* Mover = ReloadMover.Get())
	{
		Mover->OnStanceChanged.RemoveDynamic(this, &ThisClass::OnReloadStanceChanged);
	}
	if (AAZ_Weapon* Weapon = ReloadingWeapon.Get())
	{
		Weapon->OnReloadAnimationInterrupted.Remove(AnimationInterruptedHandle);
	}
	AnimationInterruptedHandle.Reset();
	const FGuid EndedActionId = ReloadActionId;
	const FGuid EndedItemId = ReloadedItemId;
	const TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryComponent> Inventory = ReloadInventory;
	const TWeakObjectPtr<UAZ_Inv_CommonUI_EquipmentComponent> Equipment = ReloadEquipment;
	const TWeakObjectPtr<AAZ_Weapon> Weapon = ReloadingWeapon;
	// The inventory records commit before publishing its change event. A listener
	// may reenter here before TryCommitMagazineReload returns to this ability.
	const bool bCommitted = ActorInfo && ActorInfo->IsNetAuthority() && Inventory.IsValid()
		&& Inventory->IsMagazineReloadCommitted(EndedActionId);
	const bool bEndedRaised = bReloadRaised;
	// Release local ownership before inventory or animation callbacks run. A late
	// callback only carries the old action token and cannot finish another reload.
	ReloadActionId.Invalidate();
	ReloadedItemId.Invalidate();
	RequestedReloadMagazineId.Invalidate();
	ReloadingWeapon.Reset();
	ReloadInventory.Reset();
	ReloadEquipment.Reset();
	ReloadMover.Reset();
	EquipmentGeneration = 0;
	bReloadSkipEmpty = false;
	bReloadCrouching = false;
	bReloadRaised = false;
	if (ActorInfo && ActorInfo->IsNetAuthority() && EndedActionId.IsValid())
	{
		if (Inventory.IsValid()) Inventory->EndMagazineReload(EndedActionId);
		if (Weapon.IsValid()) Weapon->Multicast_EndReloadAnimation(EndedActionId, bCommitted);
		UE_LOG(LogTemp, Display, TEXT("[Reload] end action=%s item=%s cancelled=%d"),
			*EndedActionId.ToString(), *EndedItemId.ToString(), bWasCancelled);
	}
	else if (ActorInfo && !ActorInfo->IsNetAuthority() && bWasCancelled && !RemoteInstanceEnded && Weapon.IsValid())
	{
		// Hide a locally cancelled pose while its cancellation travels to authority.
		// A received server end uses the exact-token cosmetic RPC instead: it must
		// not stop a newer presentation that arrived first on the weapon channel.
		Weapon->StopReloadAnimation();
	}
	if (UAZ_AbilitySystemComponent* ASC = ActorInfo ? Cast<UAZ_AbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr)
	{
		ASC->ClearWeaponInput(FGameplayTagContainer(FAZ_GameplayTags::Get().Input_Action_Reload));
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	if (bEndedRaised && Equipment.IsValid()) Equipment->EndFirearmReloadHold(EndedActionId, bCommitted);
}
