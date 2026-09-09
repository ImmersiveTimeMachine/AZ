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

	bool ResolveFirearmSource(const FGameplayAbilityActorInfo* ActorInfo, const UObject* Source, FFirearmSource& Out)
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
			|| !FMath::IsFinite(Out.Definition->FireRate) || Out.Definition->FireRate <= 0.f
			|| !FMath::IsFinite(Out.Definition->MaxRange) || Out.Definition->MaxRange <= 0.f
			|| !FMath::IsFinite(Out.Definition->BaseDamage) || Out.Definition->BaseDamage <= 0.f
			|| !FMath::IsFinite(Out.Definition->SpreadAim) || Out.Definition->SpreadAim < 0.f) return false;
		const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		if (!ASC || ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Ability_State_Reloading)
			|| (Out.Definition->bRequiresAimToFire && !ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Ability_State_Aiming))) return false;
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

	bool BuildAuthoritativeHit(const FFirearmSource& Source, FHitResult& OutHit, FVector& OutMuzzle)
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
		const float Spread = FMath::Clamp(Source.Definition->SpreadAim, 0.f, 180.f);
		AimDirection = FMath::VRandCone(AimDirection, FMath::DegreesToRadians(Spread * 0.5f));
		TraceFirearmSegment(*World, OutMuzzle, OutMuzzle + AimDirection * Source.Definition->MaxRange, Params, OutHit);
		return true;
	}
}

UAZ_GA_FirearmFire::UAZ_GA_FirearmFire()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
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
	if (!ActorInfo || !Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
	FFirearmSource Source;
	if (!ResolveFirearmSource(ActorInfo, GetSourceObject(Handle, ActorInfo), Source)) return false;
	const UWorld* World = Source.Hero->GetWorld();
	if (!World || World->GetTimeSeconds() < NextAllowedFireTime) return false;
	return Source.Inventory->GetWeaponAmmoSnapshot(Source.Item->GetInstanceId()).MagazineState == EAZ_WeaponMagazineState::Loaded;
}

void UAZ_GA_FirearmFire::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	FFirearmSource Source;
	if (!ResolveFirearmSource(ActorInfo, GetSourceObject(Handle, ActorInfo), Source)
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActive()) return;
	FiredItemId = Source.Item->GetInstanceId();
	EquipmentGeneration = Source.Equipment->GetSelectionGeneration();
	ShotId = FGuid::NewGuid();
	bShotResolved = false;
	bRequiresAim = Source.Definition->bRequiresAimToFire;
	NextAllowedFireTime = Source.Hero->GetWorld()->GetTimeSeconds() + 1.0 / Source.Definition->FireRate;

	if (bRequiresAim)
	{
		AimChangedHandle = ActorInfo->AbilitySystemComponent->RegisterGameplayTagEvent(
			FAZ_GameplayTags::Get().Ability_State_Aiming, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::OnAimTagChanged);
	}
	// The owning client keeps only the input lifetime. Authority alone traces,
	// debits, damages, reports hearing and emits the accepted-shot presentation.
	if (ActorInfo->IsNetAuthority() && !FireAuthoritativeShot())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!IsActive()) return;
	UAbilityTask_WaitInputRelease* WaitRelease = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	WaitRelease->OnRelease.AddDynamic(this, &ThisClass::OnFireInputReleased);
	WaitRelease->ReadyForActivation();
}

bool UAZ_GA_FirearmFire::FireAuthoritativeShot()
{
	if (bShotResolved || !IsActive() || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()) return false;
	bShotResolved = true;
	FFirearmSource Source;
	if (!ResolveFirearmSource(CurrentActorInfo, GetCurrentSourceObject(), Source)
		|| Source.Item->GetInstanceId() != FiredItemId
		|| Source.Equipment->GetSelectionGeneration() != EquipmentGeneration) return false;
	const FAZ_WeaponAmmoSnapshot Expected = Source.Inventory->GetWeaponAmmoSnapshot(FiredItemId);
	if (Expected.MagazineState != EAZ_WeaponMagazineState::Loaded) return false;
	FHitResult Hit;
	FVector Muzzle;
	if (!BuildAuthoritativeHit(Source, Hit, Muzzle)) return false;
	FGameplayEffectSpecHandle DamageSpec = MakeOutgoingGameplayEffectSpec(UAZ_GE_Damage::StaticClass(), GetAbilityLevel());
	if (!DamageSpec.IsValid()) return false;
	DamageSpec.Data->SetSetByCallerMagnitude(FAZ_GameplayTags::Get().SetByCaller_Damage, Source.Definition->BaseDamage);
	DamageSpec.Data->GetContext().AddHitResult(Hit);
	// Inventory publication can synchronously run listeners, including equipment
	// cancellation. Keep the accepted shot's receipt and tuning independent of the
	// ability members that EndAbility clears and of later manifest changes.
	const FGuid AcceptedShotId = ShotId;
	const FGuid AcceptedItemId = FiredItemId;
	const uint32 AcceptedGeneration = EquipmentGeneration;
	const float NoiseLoudness = FMath::Max(0.f, Source.Definition->ShotNoiseLoudness);
	const float NoiseMaxRange = FMath::Max(0.f, Source.Definition->ShotNoiseMaxRange);
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
	UE_LOG(LogTemp, Display, TEXT("[Fire] shot=%s item=%s magazine=%s generation=%u rounds=%d hit=%s confirmed=%d"),
		*AcceptedShotId.ToString(), *AcceptedItemId.ToString(), *Committed.MagazineItemId.ToString(), AcceptedGeneration,
		Committed.Rounds, *GetNameSafe(Target), bHitConfirmed);
	return true;
}

void UAZ_GA_FirearmFire::OnAimTagChanged(FGameplayTag Tag, int32 Count)
{
	if (bRequiresAim && Count <= 0 && IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAZ_GA_FirearmFire::OnFireInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAZ_GA_FirearmFire::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo)) return;
	if (UAZ_AbilitySystemComponent* ASC = ActorInfo ? Cast<UAZ_AbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr)
	{
		ASC->RegisterGameplayTagEvent(FAZ_GameplayTags::Get().Ability_State_Aiming, EGameplayTagEventType::NewOrRemoved).Remove(AimChangedHandle);
		ASC->ClearWeaponInput(FGameplayTagContainer(InputTag));
	}
	AimChangedHandle.Reset();
	bShotResolved = true;
	FiredItemId.Invalidate();
	ShotId.Invalidate();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
