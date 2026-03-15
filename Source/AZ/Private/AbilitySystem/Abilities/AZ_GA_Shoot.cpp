// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/AZ_GA_Shoot.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/AbilityTasks/AZ_AT_WaitTargetDataUsingActor.h"
#include "AbilitySystem/TargetActors/AZ_GATA_LineTrace.h"
#include "Weapon/AZ_Weapon.h"


UAZ_GA_Shoot::UAZ_GA_Shoot()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UAZ_GA_Shoot::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Try GASShooter pattern: weapon source → target actor → WaitTargetData
	AAZ_Weapon* Weapon = GetEquippedWeapon();
	if (Weapon)
	{
		AAZ_GATA_LineTrace* TargetActor = Weapon->GetLineTraceTargetActor();
		if (TargetActor)
		{
			ConfigureTargetActor(TargetActor);

			UAZ_AT_WaitTargetDataUsingActor* WaitTargetDataTask = UAZ_AT_WaitTargetDataUsingActor::WaitTargetDataWithReusableActor(
				this,
				FName("WaitTargetData"),
				EGameplayTargetingConfirmation::Instant,
				TargetActor,
				true // bCreateKeyIfNotValidForMorePredicting — for batching
			);

			WaitTargetDataTask->ValidData.AddDynamic(this, &UAZ_GA_Shoot::OnTargetDataReady);
			WaitTargetDataTask->Cancelled.AddDynamic(this, &UAZ_GA_Shoot::OnTargetDataCancelled);
			WaitTargetDataTask->ReadyForActivation();
			return;
		}
	}

	// Fallback: raw line trace (no weapon source object)
	PerformFallbackLineTrace();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UAZ_GA_Shoot::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAZ_GA_Shoot::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& Data)
{
	HandleDamage(Data);
	ConsumeAmmo();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UAZ_GA_Shoot::OnTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAZ_GA_Shoot::HandleDamage(const FGameplayAbilityTargetDataHandle& Data)
{
	if (!DamageGameplayEffect) return;

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageGameplayEffect, GetAbilityLevel());
	if (!SpecHandle.IsValid()) return;

	SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.Damage")), BaseDamage);

	// Apply to each target in the target data
	for (int32 i = 0; i < Data.Num(); i++)
	{
		if (const FGameplayAbilityTargetData* TargetData = Data.Get(i))
		{
			TArray<TWeakObjectPtr<AActor>> Actors = TargetData->GetActors();
			for (const TWeakObjectPtr<AActor>& ActorPtr : Actors)
			{
				if (AActor* HitActor = ActorPtr.Get())
				{
					if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitActor))
					{
						GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(
							*SpecHandle.Data.Get(), TargetASC);
					}
				}
			}
		}
	}
}

void UAZ_GA_Shoot::ConsumeAmmo()
{
	AAZ_Weapon* Weapon = GetEquippedWeapon();
	if (!Weapon) return;
	if (Weapon->HasInfiniteAmmo()) return;

	const int32 CurrentAmmo = Weapon->GetPrimaryClipAmmo();
	if (CurrentAmmo > 0)
	{
		Weapon->SetPrimaryClipAmmo(CurrentAmmo - 1);
	}
}

AAZ_Weapon* UAZ_GA_Shoot::GetEquippedWeapon() const
{
	return Cast<AAZ_Weapon>(GetCurrentSourceObject());
}

void UAZ_GA_Shoot::ConfigureTargetActor(AAZ_GATA_LineTrace* TargetActor) const
{
	FGameplayAbilityTargetingLocationInfo StartLocation;
	StartLocation.LocationType = EGameplayAbilityTargetingLocationType::SocketTransform;

	AAZ_Weapon* Weapon = GetEquippedWeapon();
	if (Weapon && Weapon->GetWeaponMesh3P())
	{
		StartLocation.SourceComponent = Weapon->GetWeaponMesh3P();
		StartLocation.SourceSocketName = FName("Muzzle");
	}

	TargetActor->Configure(
		StartLocation,
		FGameplayTag(),						// AimingTag
		FGameplayTag(),						// AimingRemovalTag
		FCollisionProfileName(TEXT("Weapon")),
		FGameplayTargetDataFilterHandle(),
		nullptr,							// ReticleClass
		FWorldReticleParameters(),
		false,								// bIgnoreBlockingHits
		false,								// bShouldProduceTargetDataOnServer
		false,								// bUsePersistentHitResults
		bDebugTrace,						// bDebug
		true,								// bTraceAffectsAimPitch
		true,								// bTraceFromPlayerViewPoint
		false,								// bUseAimingSpreadMod
		MaxRange,
		BaseSpread,
		0.f,								// AimingSpreadMod
		0.f,								// TargetingSpreadIncrement
		0.f,								// TargetingSpreadMax
		1,									// MaxHitResultsPerTrace
		1									// NumberOfTraces
	);
}

// ---------------------------------------------------------------------------
// Fallback: raw line trace when no weapon source object is available
// ---------------------------------------------------------------------------

void UAZ_GA_Shoot::PerformFallbackLineTrace()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor)) return;

	APlayerController* PC = Cast<APlayerController>(GetCurrentActorInfo()->PlayerController.Get());
	if (!IsValid(PC)) return;

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector TraceStart = CameraLocation;
	const FVector TraceEnd = CameraLocation + CameraRotation.Vector() * MaxRange;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(AvatarActor);
	QueryParams.bTraceComplex = true;
	QueryParams.bReturnPhysicalMaterial = true;

	FHitResult HitResult;
	bool bHit = AvatarActor->GetWorld()->LineTraceSingleByProfile(
		HitResult, TraceStart, TraceEnd, TEXT("Weapon"), QueryParams);

#if ENABLE_DRAW_DEBUG
	if (bDebugTrace)
	{
		DrawDebugLine(AvatarActor->GetWorld(), TraceStart, bHit ? HitResult.ImpactPoint : TraceEnd,
			bHit ? FColor::Red : FColor::Green, false, 2.f, 0, 1.f);
		if (bHit)
		{
			DrawDebugSphere(AvatarActor->GetWorld(), HitResult.ImpactPoint, 10.f, 8, FColor::Red, false, 2.f);
		}
	}
#endif

	if (bHit && IsValid(HitResult.GetActor()))
	{
		ApplyDamageToTarget(HitResult.GetActor(), HitResult);
	}
}

void UAZ_GA_Shoot::ApplyDamageToTarget(AActor* HitActor, const FHitResult& HitResult)
{
	if (!DamageGameplayEffect) return;

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitActor);
	if (!TargetASC) return;

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageGameplayEffect, GetAbilityLevel());
	if (!SpecHandle.IsValid()) return;

	SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.Damage")), BaseDamage);

	GetAbilitySystemComponentFromActorInfo()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
}
