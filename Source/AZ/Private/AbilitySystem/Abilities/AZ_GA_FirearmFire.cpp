#include "AbilitySystem/Abilities/AZ_GA_FirearmFire.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h"
#include "AbilitySystem/GameplayEffects/AZ_GE_Damage.h"
#include "AZ_GameplayTags.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.h"
#include "GenericTeamAgentInterface.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h"
#include "InventoryUI/AZ_Inv_CommonUI_InventoryItem.h"
#include "Particles/ParticleSystem.h"
#include "Perception/AISense_Hearing.h"
#include "Player/AZ_PlayerController.h"
#include "TimerManager.h"
#include "Weapon/AZ_Weapon.h"

namespace
{
	struct FFirearmSource
	{
		AAZ_PawnMoverHeroCharacter* Hero = nullptr;
		AAZ_PlayerController* Controller = nullptr;
		UAZ_Inv_CommonUI_EquipmentComponent* Equipment = nullptr;
		UAZ_Inv_CommonUI_InventoryComponent* Inventory = nullptr;
		UAZ_Inv_CommonUI_InventoryItem* Item = nullptr;
		AAZ_Weapon* Weapon = nullptr;
		const FAZ_Inv_CommonUI_WeaponStateFragment* Definition = nullptr;
	};

	bool ResolveFirearmSource(const FGameplayAbilityActorInfo* ActorInfo, const UObject* Source, FFirearmSource& Out,
		bool bAllowPreparation = false)
	{
		Out.Hero = ActorInfo ? Cast<AAZ_PawnMoverHeroCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
		Out.Controller = Out.Hero ? Cast<AAZ_PlayerController>(Out.Hero->GetController()) : nullptr;
		if (!Out.Controller || Out.Controller->IsInventoryInputCaptured()) return false;
		Out.Equipment = Out.Controller->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>();
		Out.Inventory = Out.Controller->FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>();
		if (!Out.Equipment || !Out.Inventory || !Out.Equipment->IsActiveWeaponSource(Source)) return false;
		Out.Item = Out.Equipment->GetActiveItem();
		Out.Weapon = Out.Equipment->GetActiveWeapon();
		Out.Definition = Out.Item->GetItemManifest().GetFragmentOfType<FAZ_Inv_CommonUI_WeaponStateFragment>();
		if (!Out.Definition || !Out.Definition->bUsesDetachableMagazines
			|| !Out.Definition->IsFireModeSupported(Out.Item->GetSelectedFireMode()) || Out.Item->GetFireModeRevision() < 0
			|| !FMath::IsFinite(Out.Definition->FireRate) || Out.Definition->FireRate <= 0.f
			|| !FMath::IsFinite(Out.Definition->MaxRange) || Out.Definition->MaxRange <= 0.f
			|| !FMath::IsFinite(Out.Definition->BaseDamage) || Out.Definition->BaseDamage <= 0.f
			|| !FMath::IsFinite(Out.Definition->SpreadAim) || Out.Definition->SpreadAim < 0.f) return false;
		const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		if (!ASC || ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Ability_State_Reloading)
			|| (!bAllowPreparation && Out.Definition->bRequiresAimToFire && !Out.Equipment->IsFirearmRaised())) return false;
		const USkeletalMeshComponent* WeaponMesh = Out.Weapon->GetWeaponMesh3P();
		return WeaponMesh && WeaponMesh->DoesSocketExist(Out.Definition->MuzzleSocketName);
	}

	/** Pawn/CharacterMesh profiles ignore Visibility. Compare their physical query
	 * geometry with the first visible scenery hit so capsules remain hittable and
	 * scenery still wins when it is nearer. This does not grant bullet penetration. */
	bool TraceFirearmSegment(UWorld& World, const FVector& Start, const FVector& End,
		const FCollisionQueryParams& Params, FHitResult& OutHit)
	{
		FHitResult SceneryHit;
		FHitResult PawnHit;
		const bool bScenery = World.LineTraceSingleByChannel(SceneryHit, Start, End, ECC_Visibility, Params);
		FCollisionObjectQueryParams PawnObjects;
		PawnObjects.AddObjectTypesToQuery(ECC_Pawn);
		const bool bPawn = World.LineTraceSingleByObjectType(PawnHit, Start, End, PawnObjects, Params);
		if (bScenery || bPawn)
		{
			OutHit = bScenery && (!bPawn || SceneryHit.Time <= PawnHit.Time) ? SceneryHit : PawnHit;
			return true;
		}
		OutHit = FHitResult();
		OutHit.TraceStart = Start;
		OutHit.TraceEnd = End;
		OutHit.Location = End;
		OutHit.ImpactPoint = End;
		return false;
	}

	bool BuildAuthoritativeHit(const FFirearmSource& Source, float SpreadAngleDegrees, FHitResult& OutHit, FVector& OutMuzzle)
	{
		UWorld* World = Source.Hero->GetWorld();
		if (!World) return false;
		OutMuzzle = Source.Weapon->GetWeaponMesh3P()->GetSocketLocation(Source.Definition->MuzzleSocketName);
		FVector CameraLocation;
		FRotator CameraRotation;
		Source.Controller->GetPlayerViewPoint(CameraLocation, CameraRotation);
		if (OutMuzzle.ContainsNaN() || CameraLocation.ContainsNaN() || CameraRotation.ContainsNaN()) return false;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AZFirearmShot), true);
		Params.bReturnPhysicalMaterial = true;
		Params.AddIgnoredActor(Source.Hero);
		Params.AddIgnoredActor(Source.Weapon);
		FHitResult CameraHit;
		TraceFirearmSegment(*World, CameraLocation,
			CameraLocation + CameraRotation.Vector() * Source.Definition->MaxRange, Params, CameraHit);

		// A barrel already poking through a wall must not start its shot beyond that
		// wall. Start this guard on the capsule's interior axis at the muzzle's height.
		FVector SafeOrigin = Source.Hero->GetActorLocation();
		if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Source.Hero->GetRootComponent()))
		{
			const FVector Axis = Capsule->GetUpVector();
			const float AxisExtent = FMath::Max(0.f, Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius());
			SafeOrigin = Capsule->GetComponentLocation() + Axis * FMath::Clamp(
				FVector::DotProduct(OutMuzzle - Capsule->GetComponentLocation(), Axis), -AxisExtent, AxisExtent);
		}
		if (TraceFirearmSegment(*World, SafeOrigin, OutMuzzle, Params, OutHit)) return true;

		FVector AimDirection = (CameraHit.ImpactPoint - OutMuzzle).GetSafeNormal();
		if (AimDirection.IsNearlyZero() || FVector::DotProduct(AimDirection, CameraRotation.Vector()) <= 0.f) return false;
		const float Spread = FMath::Clamp(SpreadAngleDegrees, 0.f, 180.f);
		AimDirection = FMath::VRandCone(AimDirection, FMath::DegreesToRadians(Spread * 0.5f));
		TraceFirearmSegment(*World, OutMuzzle, OutMuzzle + AimDirection * Source.Definition->MaxRange, Params, OutHit);
		return true;
	}
}

UAZ_GA_FirearmFire::UAZ_GA_FirearmFire()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	bServerRespectsRemoteAbilityCancellation = true;
}

void UAZ_GA_FirearmFire::DeclareAbilityTags()
{
	Super::DeclareAbilityTags();
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	ActivationOwnedTags.AddTag(Tags.Ability_State_Shooting);
	ActivationBlockedTags.AddTag(Tags.Character_Dead);
	ActivationBlockedTags.AddTag(Tags.Character_Dying);
	ActivationBlockedTags.AddTag(Tags.State_Grabbed);
	ActivationBlockedTags.AddTag(Tags.State_Combat_Grabbing);
	ActivationBlockedTags.AddTag(Tags.State_Combat_Staggered);
	ActivationBlockedTags.AddTag(Tags.State_Combat_StruckPair);
	ActivationBlockedTags.AddTag(Tags.Ability_State_MeleeAttacking);
	ActivationBlockedTags.AddTag(Tags.Ability_State_Reloading);
	CancelAbilitiesWithTag.AddTag(Tags.Movement_Sprinting);
	BlockAbilitiesWithTag.AddTag(Tags.Movement_Sprinting);
}

bool UAZ_GA_FirearmFire::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || bEndingFire || bShotInProgress
		|| !Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	FFirearmSource Source;
	if (!ResolveFirearmSource(ActorInfo, GetSourceObject(Handle, ActorInfo), Source, true)) return false;
	const UWorld* World = Source.Hero->GetWorld();
	if (!World || World->GetTimeSeconds() < NextAllowedFireTime) return false;
	if (ActorInfo->IsNetAuthority()
		&& World->GetTimeSeconds() + UE_KINDA_SMALL_NUMBER < Source.Inventory->GetWeaponNextAllowedFireTime(Source.Item->GetInstanceId())) return false;
	return Source.Inventory->GetWeaponAmmoSnapshot(Source.Item->GetInstanceId()).MagazineState == EAZ_WeaponMagazineState::Loaded;
}

void UAZ_GA_FirearmFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	FFirearmSource Source;
	if (!ResolveFirearmSource(ActorInfo, GetSourceObject(Handle, ActorInfo), Source, true)
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive()) return;
	FFirearmSource ActivatedSource;
	if (!ResolveFirearmSource(ActorInfo, GetSourceObject(Handle, ActorInfo), ActivatedSource, true))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Source = ActivatedSource;
	FiredItemId = Source.Item->GetInstanceId();
	EquipmentGeneration = Source.Equipment->GetSelectionGeneration();
	FireActionId = FGuid::NewGuid();
	FiringWeapon = Source.Weapon;
	FiringEquipment = Source.Equipment;
	FireModeRevision = Source.Item->GetFireModeRevision();
	ActiveFireMode = Source.Item->GetSelectedFireMode();
	bAnimationStarted = false;
	bRequiresAim = Source.Definition->bRequiresAimToFire;
	bInitialShotPending = true;
	bInputReleased = false;
	const FGuid ActionId = FireActionId;
	const FAZ_WeaponAmmoSnapshot InitialAmmo = Source.Inventory->GetWeaponAmmoSnapshot(FiredItemId);
	InitialMagazineId = InitialAmmo.MagazineItemId;
	InitialAmmoRevision = InitialAmmo.AmmoRevision;

	// This task sends its reliable release event before calling our callback. Bind
	// it before readiness callbacks or timers; an already-released tap still owns
	// exactly one initial shot unless another gameplay action interrupts it.
	UAbilityTask_WaitInputRelease* WaitRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	WaitRelease->OnRelease.AddDynamic(this, &ThisClass::OnFireInputReleased);
	WaitRelease->ReadyForActivation();
	if (!IsActive() || FireActionId != ActionId) return;
	float RaiseDelay = 0.f;
	if (!Source.Equipment->BeginFirearmPreparation(Source.Weapon, FiredItemId, EquipmentGeneration, RaiseDelay,
		ActivationInfo.GetActivationPredictionKey().Current))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!IsActive() || FireActionId != ActionId) return;
	FFirearmSource Prepared;
	if (!ResolveFirearmSource(ActorInfo, GetSourceObject(Handle, ActorInfo), Prepared)
		|| Prepared.Weapon != FiringWeapon.Get() || Prepared.Item->GetInstanceId() != FiredItemId
		|| Prepared.Equipment->GetSelectionGeneration() != EquipmentGeneration)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	UWorld* World = Prepared.Hero->GetWorld();
	if (!World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	// Preparation can replace an expired lifetime. Observe loss only after that
	// transaction has established and revalidated this action's raised state.
	if (bRequiresAim)
	{
		AimChangedHandle = ActorInfo->AbilitySystemComponent->RegisterGameplayTagEvent(
			FAZ_GameplayTags::Get().Ability_State_Aiming, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::OnAimTagChanged);
		ReadyChangedHandle = ActorInfo->AbilitySystemComponent->RegisterGameplayTagEvent(
			FAZ_GameplayTags::Get().Ability_State_FirearmReady, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::OnAimTagChanged);
	}
	InitialRaiseDeadline = World->GetTimeSeconds() + RaiseDelay;
	if (ActiveFireMode == EAZ_FirearmFireMode::Automatic && !bInputReleased
		&& !Prepared.Equipment->BeginFirearmAutoHold(Prepared.Weapon, FiredItemId, EquipmentGeneration, ActionId))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!IsActive() || FireActionId != ActionId) return;
	if (!ActorInfo->IsNetAuthority())
	{
		// No client shot timer/debit. Its release cannot end authority before the
		// independently timed first shot; authority eventually ends this mirror.
		NextAllowedFireTime = InitialRaiseDeadline + 1.0 / Prepared.Definition->FireRate;
		return;
	}
	if (RaiseDelay <= UE_KINDA_SMALL_NUMBER)
	{
		OnInitialShotDue(ActionId);
	}
	else
	{
		World->GetTimerManager().SetTimer(InitialFireTimer,
			FTimerDelegate::CreateUObject(this, &ThisClass::OnInitialShotDue, ActionId), RaiseDelay, false);
		UE_LOG(LogTemp, Display, TEXT("[Fire] raise action=%s item=%s delay=%.3f"),
			*ActionId.ToString(), *FiredItemId.ToString(), RaiseDelay);
	}
}

void UAZ_GA_FirearmFire::OnInitialShotDue(FGuid ExpectedActionId)
{
	if (!IsActive() || FireActionId != ExpectedActionId || !bInitialShotPending
		|| !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()) return;
	FFirearmSource Source;
	if (!ResolveFirearmSource(CurrentActorInfo, GetCurrentSourceObject(), Source)
		|| Source.Weapon != FiringWeapon.Get() || Source.Item->GetInstanceId() != FiredItemId
		|| Source.Equipment->GetSelectionGeneration() != EquipmentGeneration
		|| Source.Item->GetSelectedFireMode() != ActiveFireMode
		|| Source.Item->GetFireModeRevision() != FireModeRevision)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	const FAZ_WeaponAmmoSnapshot Ammo = Source.Inventory->GetWeaponAmmoSnapshot(FiredItemId);
	if (Ammo.MagazineState != EAZ_WeaponMagazineState::Loaded
		|| Ammo.MagazineItemId != InitialMagazineId || Ammo.AmmoRevision != InitialAmmoRevision)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	UWorld* World = Source.Hero->GetWorld();
	if (!World)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	const double Remaining = InitialRaiseDeadline - World->GetTimeSeconds();
	if (Remaining > UE_KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(InitialFireTimer,
			FTimerDelegate::CreateUObject(this, &ThisClass::OnInitialShotDue, ExpectedActionId), static_cast<float>(Remaining), false);
		return;
	}
	// Clear first: inventory publication can synchronously cancel this action.
	bInitialShotPending = false;
	if (!FireAuthoritativeShot())
	{
		if (IsActive() && FireActionId == ExpectedActionId)
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	if (!IsActive() || FireActionId != ExpectedActionId) return;
	if (bInputReleased)
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	else if (ActiveFireMode == EAZ_FirearmFireMode::Automatic)
		ScheduleAutomaticShot();
}

bool UAZ_GA_FirearmFire::FireAuthoritativeShot()
{
	if (bShotInProgress || !IsActive() || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()) return false;
	TGuardValue<bool> ShotGuard(bShotInProgress, true);
	FFirearmSource Source;
	if (!ResolveFirearmSource(CurrentActorInfo, GetCurrentSourceObject(), Source)
		|| Source.Item->GetInstanceId() != FiredItemId
		|| Source.Equipment->GetSelectionGeneration() != EquipmentGeneration
		|| Source.Item->GetSelectedFireMode() != ActiveFireMode
		|| Source.Item->GetFireModeRevision() != FireModeRevision) return false;
	const FAZ_WeaponAmmoSnapshot Expected = Source.Inventory->GetWeaponAmmoSnapshot(FiredItemId);
	if (Expected.MagazineState != EAZ_WeaponMagazineState::Loaded) return false;
	const UWorld* World = Source.Hero->GetWorld();
	if (!World) return false;
	// Read the pre-shot cone without mutation. Only a later accepted ammo debit
	// grows recoil, so previews, rejected requests and trigger taps add nothing.
	const float ShotSpreadDegrees = Source.Item->GetFirearmSpreadAngleDegrees(World->GetTimeSeconds());
	FHitResult Hit;
	FVector Muzzle;
	if (!BuildAuthoritativeHit(Source, ShotSpreadDegrees, Hit, Muzzle)) return false;
	FGameplayEffectSpecHandle DamageSpec = MakeOutgoingGameplayEffectSpec(UAZ_GE_Damage::StaticClass(), GetAbilityLevel());
	if (!DamageSpec.IsValid()) return false;
	DamageSpec.Data->SetSetByCallerMagnitude(FAZ_GameplayTags::Get().SetByCaller_Damage, Source.Definition->BaseDamage);
	DamageSpec.Data->GetContext().AddHitResult(Hit);
	// Inventory publication can synchronously run listeners, including equipment
	// cancellation. Keep the accepted shot's receipt and tuning independent of the
	// ability members that EndAbility clears and of later manifest changes.
	const FGuid AcceptedShotId = FGuid::NewGuid();
	const FGuid AcceptedActionId = FireActionId;
	const FGuid AcceptedItemId = FiredItemId;
	const uint32 AcceptedGeneration = EquipmentGeneration;
	const double AcceptedShotServerTime = World->GetTimeSeconds();
	const int32 AcceptedPreparationKey = CurrentActivationInfo.GetActivationPredictionKey().Current;
	const bool bAutomatic = ActiveFireMode == EAZ_FirearmFireMode::Automatic;
	const float NoiseLoudness = FMath::Max(0.f, Source.Definition->ShotNoiseLoudness);
	const float NoiseMaxRange = FMath::Max(0.f, Source.Definition->ShotNoiseMaxRange);
	const FAZ_FirearmRecoilSettings RecoilSettings = Source.Definition->Recoil;
	// Classify before damage can destroy the target, and snapshot cosmetics before
	// ammunition publication can change the selected item's definition. Pawn hits
	// need their own response; never emit stone dust on a character or on a miss.
	const UPrimitiveComponent* HitComponent = Hit.GetComponent();
	const bool bWorldImpact = Hit.IsValidBlockingHit()
		&& IsValid(HitComponent) && HitComponent->GetCollisionObjectType() != ECC_Pawn
		&& !Cast<APawn>(Hit.GetActor()) && !Hit.ImpactPoint.ContainsNaN()
		&& !Hit.ImpactNormal.ContainsNaN() && !Hit.ImpactNormal.IsNearlyZero();
	UParticleSystem* WorldImpactEffect = bWorldImpact ? Source.Definition->WorldImpactEffect.Get() : nullptr;
	const float WorldImpactScale = Source.Definition->WorldImpactScale;
	UAbilitySystemComponent* SourceASC = CurrentActorInfo->AbilitySystemComponent.Get();
	FAZ_WeaponAmmoSnapshot Committed;
	if (!Source.Inventory->TryConsumeWeaponRound(Source.Weapon, AcceptedItemId, Expected.MagazineItemId,
		Expected.AmmoRevision, AcceptedGeneration, AcceptedShotId, Committed)) return false;
	if (IsValid(Source.Equipment))
	{
		Source.Equipment->RefreshFirearmReadyAfterShot(Source.Weapon, AcceptedItemId,
			AcceptedGeneration, AcceptedShotId, AcceptedShotServerTime, AcceptedPreparationKey);
	}
	if (IsValid(Source.Controller) && IsValid(Source.Weapon))
	{
		// Reliable owning-player receipt, independent of unreliable muzzle/impact
		// cosmetics. The controller rejects stale selection/ownership receipts.
		Source.Controller->Client_ApplyFirearmRecoil(Source.Weapon, AcceptedItemId,
			AcceptedGeneration, AcceptedShotId, AcceptedShotServerTime, AcceptedPreparationKey, RecoilSettings);
	}
	if (IsActive() && FireActionId == AcceptedActionId && IsValid(Source.Inventory))
	{
		NextAllowedFireTime = Source.Inventory->GetWeaponNextAllowedFireTime(AcceptedItemId);
		// Publication may have synchronously cancelled this ability or selected a
		// different weapon. A cancelled shot must never restart a stopped loop.
		if (!bAnimationStarted && IsValid(Source.Weapon) && IsValid(Source.Equipment)
			&& Source.Equipment->GetSelectionGeneration() == AcceptedGeneration
			&& Source.Equipment->IsActiveWeaponSource(Source.Weapon))
		{
			bAnimationStarted = true;
			Source.Weapon->Multicast_BeginFirearmAnimation(AcceptedActionId, bAutomatic);
		}
	}

	bool bHitConfirmed = false;
	AActor* Target = Hit.GetActor();
	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (Target && Target != Source.Hero && SourceASC && TargetASC
		&& FGenericTeamId::GetAttitude(Source.Hero, Target) == ETeamAttitude::Hostile)
	{
		bool bHasVitals = false;
		const float Health = TargetASC->GetGameplayAttributeValue(UAZ_VitalsAttributeSet::GetHealthAttribute(), bHasVitals);
		const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
		if (bHasVitals && Health > 0.f && !TargetASC->HasMatchingGameplayTag(Tags.Character_Dead)
			&& !TargetASC->HasMatchingGameplayTag(Tags.Character_Dying))
		{
			SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpec.Data.Get(), TargetASC);
			bHitConfirmed = IsValid(TargetASC) && TargetASC->GetNumericAttribute(UAZ_VitalsAttributeSet::GetHealthAttribute()) < Health;
		}
	}
	if (IsValid(Source.Hero))
	{
		UAISense_Hearing::ReportNoiseEvent(Source.Hero->GetWorld(), Muzzle,
			NoiseLoudness, Source.Hero, NoiseMaxRange, FName("Combat"));
	}
	if (IsValid(Source.Weapon))
	{
		Source.Weapon->Multicast_PlayFirearmShot(Hit, bHitConfirmed, WorldImpactEffect, WorldImpactScale);
	}
	UE_LOG(LogTemp, Display, TEXT("[Fire] shot=%s item=%s magazine=%s generation=%u rounds=%d hit=%s confirmed=%d mode=%s action=%s spreadFullDeg=%.3f"),
		*AcceptedShotId.ToString(), *AcceptedItemId.ToString(), *Committed.MagazineItemId.ToString(), AcceptedGeneration,
		Committed.Rounds, *GetNameSafe(Target), bHitConfirmed, bAutomatic ? TEXT("AUTO") : TEXT("SINGLE"),
		*AcceptedActionId.ToString(), ShotSpreadDegrees);
	if (Committed.Rounds == 0 && IsActive() && FireActionId == AcceptedActionId)
	{
		const TWeakObjectPtr<UAZ_Inv_CommonUI_EquipmentComponent> EmptyEquipment = Source.Equipment;
		const TWeakObjectPtr<AAZ_Weapon> EmptyWeapon = Source.Weapon;
		const FGuid EmptyMagazineId = Committed.MagazineItemId;
		const int64 EmptyAmmoRevision = Committed.AmmoRevision;
		// Finish the accepted shot's damage, recoil and presentation before ending
		// fire. End clears held input, so a completed reload never resumes a burst.
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		if (UWorld* ReloadWorld = EmptyEquipment.IsValid() && EmptyWeapon.IsValid() ? EmptyEquipment->GetWorld() : nullptr)
		{
			// Never activate inside inventory publication or EndAbility delegates.
			// Equipment validates the exact empty receipt again on the next tick.
			ReloadWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(
				EmptyEquipment.Get(), [EmptyEquipment, EmptyWeapon, AcceptedItemId, AcceptedGeneration, EmptyMagazineId, EmptyAmmoRevision]()
				{
					if (EmptyEquipment.IsValid() && EmptyWeapon.IsValid())
					{
						EmptyEquipment->RequestReloadIfEmpty(EmptyWeapon.Get(), AcceptedItemId, AcceptedGeneration,
							EmptyMagazineId, EmptyAmmoRevision);
					}
				}));
		}
	}
	return true;
}

void UAZ_GA_FirearmFire::ScheduleAutomaticShot()
{
	if (!IsActive() || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()
		|| ActiveFireMode != EAZ_FirearmFireMode::Automatic || bInitialShotPending || bInputReleased) return;
	FFirearmSource Source;
	if (!ResolveFirearmSource(CurrentActorInfo, GetCurrentSourceObject(), Source)
		|| Source.Item->GetInstanceId() != FiredItemId
		|| Source.Equipment->GetSelectionGeneration() != EquipmentGeneration
		|| Source.Item->GetSelectedFireMode() != ActiveFireMode
		|| Source.Item->GetFireModeRevision() != FireModeRevision
		|| Source.Inventory->GetWeaponAmmoSnapshot(FiredItemId).MagazineState != EAZ_WeaponMagazineState::Loaded)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	UWorld* World = Source.Hero->GetWorld();
	if (!World)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	NextAllowedFireTime = Source.Inventory->GetWeaponNextAllowedFireTime(FiredItemId);
	const float Delay = static_cast<float>(FMath::Max(NextAllowedFireTime - World->GetTimeSeconds(), static_cast<double>(UE_KINDA_SMALL_NUMBER)));
	// One callback means at most one accepted shot. The inventory's deadline owns
	// cadence; a late frame never creates a catch-up burst or accumulates timer drift.
	World->GetTimerManager().SetTimer(AutomaticFireTimer,
		FTimerDelegate::CreateUObject(this, &ThisClass::OnAutomaticShotDue, FireActionId), Delay, false);
}

void UAZ_GA_FirearmFire::OnAutomaticShotDue(FGuid ExpectedActionId)
{
	if (!IsActive() || FireActionId != ExpectedActionId || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()) return;
	const UWorld* World = GetWorld();
	if (World && World->GetTimeSeconds() + UE_KINDA_SMALL_NUMBER < NextAllowedFireTime)
	{
		// A timer may wake just before the authority clock reaches its deadline.
		ScheduleAutomaticShot();
		return;
	}
	if (!FireAuthoritativeShot())
	{
		if (IsActive() && FireActionId == ExpectedActionId)
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		}
		return;
	}
	if (IsActive() && FireActionId == ExpectedActionId) ScheduleAutomaticShot();
}

void UAZ_GA_FirearmFire::OnAimTagChanged(FGameplayTag Tag, int32 Count)
{
	if (!bRequiresAim || Count > 0 || !IsActive() || bEndingFire) return;
	const AAZ_PawnMoverHeroCharacter* Hero = CurrentActorInfo ? Cast<AAZ_PawnMoverHeroCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	const AAZ_PlayerController* Player = Hero ? Cast<AAZ_PlayerController>(Hero->GetController()) : nullptr;
	const auto* Equipment = Player ? Player->FindComponentByClass<UAZ_Inv_CommonUI_EquipmentComponent>() : nullptr;
	if (!Equipment || !Equipment->IsFirearmRaised())
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAZ_GA_FirearmFire::OnFireInputReleased(float TimeHeld)
{
	if (!IsActive() || bEndingFire) return;
	bInputReleased = true;
	if (FiringEquipment.IsValid()) FiringEquipment->EndFirearmAutoHold(FireActionId);
	if (!IsActive()) return;
	// The task already forwarded InputReleased to authority. A normal client End
	// here could cancel the server's pending first shot before its raise finishes.
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || bInitialShotPending) return;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAZ_GA_FirearmFire::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bEndingFire || !IsEndAbilityValid(Handle, ActorInfo)) return;
	TGuardValue<bool> EndingGuard(bEndingFire, true);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InitialFireTimer);
		World->GetTimerManager().ClearTimer(AutomaticFireTimer);
	}
	if (ActorInfo && ActorInfo->IsNetAuthority() && FireActionId.IsValid() && FiringWeapon.IsValid())
	{
		FiringWeapon->Multicast_EndFirearmAnimation(FireActionId, bWasCancelled);
	}
	if (UAZ_AbilitySystemComponent* ASC = ActorInfo ? Cast<UAZ_AbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr)
	{
		ASC->RegisterGameplayTagEvent(FAZ_GameplayTags::Get().Ability_State_Aiming, EGameplayTagEventType::NewOrRemoved).Remove(AimChangedHandle);
		ASC->RegisterGameplayTagEvent(FAZ_GameplayTags::Get().Ability_State_FirearmReady, EGameplayTagEventType::NewOrRemoved).Remove(ReadyChangedHandle);
		ASC->ClearWeaponInput(FGameplayTagContainer(InputTag));
	}
	AimChangedHandle.Reset();
	ReadyChangedHandle.Reset();
	if (UAZ_Inv_CommonUI_EquipmentComponent* Equipment = FiringEquipment.Get())
	{
		Equipment->EndFirearmAutoHold(FireActionId);
		if (bWasCancelled)
			Equipment->CancelUnfulfilledFirearmPreparation(FiringWeapon.Get(), FiredItemId, EquipmentGeneration);
	}
	bInitialShotPending = false;
	bInputReleased = false;
	InitialRaiseDeadline = 0.0;
	InitialMagazineId.Invalidate();
	InitialAmmoRevision = -1;
	bAnimationStarted = false;
	FiringWeapon.Reset();
	FiringEquipment.Reset();
	FiredItemId.Invalidate();
	FireActionId.Invalidate();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
