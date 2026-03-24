#include "AbilitySystem/Abilities/AZ_GA_Shoot.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AZ_AnimInstance.h"
#include "AbilitySystem/AbilityTasks/AZ_AT_PlayMontageAndWaitForEvent.h"
#include "AbilitySystem/AbilityTasks/AZ_AT_WaitTargetDataUsingActor.h"
#include "AbilitySystem/AttributeSets/AZ_WeaponAttributeSet.h"
#include "AbilitySystem/TargetActors/AZ_GATA_LineTrace.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Weapon/AZ_Weapon.h"


UAZ_GA_Shoot::UAZ_GA_Shoot()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

bool UAZ_GA_Shoot::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	return HasAmmo();
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

	bInputHeld = true;
	ShotsFiredInBurst = 0;

	SetShootingState(true);

	// Fire first shot
	FireShot();

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UAZ_GA_Shoot::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	bInputHeld = false;

	// Clear fire loop timer
	if (const AActor* AvatarActor = GetAvatarActorFromActorInfo())
	{
		AvatarActor->GetWorldTimerManager().ClearTimer(FireLoopTimerHandle);
	}

	// bIsShooting is cleared by the timeout timer, not here

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAZ_GA_Shoot::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	bInputHeld = false;

	// Auto fire ends when input is released
	if (FireMode == EFireMode::Auto)
	{
		if (const AActor* AvatarActor = GetAvatarActorFromActorInfo())
		{
			AvatarActor->GetWorldTimerManager().ClearTimer(FireLoopTimerHandle);
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

// --- Core: fire one shot ---
void UAZ_GA_Shoot::FireShot()
{
	if (!HasAmmo())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	AAZ_Weapon* Weapon = GetPrimaryWeapon();

	// Reset shooting pose timeout
	SetShootingState(true);

	// Select montage based on fire mode
	UAnimMontage* ActiveMontage = nullptr;
	switch (FireMode)
	{
	case EFireMode::Single: ActiveMontage = FireMontageSingle; break;
	case EFireMode::Auto:   ActiveMontage = FireMontageAuto; break;
	}

	// Montage (cosmetic)
	if (ActiveMontage)
	{
		UAZ_AT_PlayMontageAndWaitForEvent* MontageTask = UAZ_AT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
			this,
			FName("FireMontage"),
			ActiveMontage,
			FGameplayTagContainer(),
			1.f,
			NAME_None,
			true,
			0.f
		);
		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UAZ_GA_Shoot::OnMontageFinished);
			MontageTask->OnBlendOut.AddDynamic(this, &UAZ_GA_Shoot::OnMontageFinished);
			MontageTask->OnInterrupted.AddDynamic(this, &UAZ_GA_Shoot::OnMontageFinished);
			MontageTask->OnCancelled.AddDynamic(this, &UAZ_GA_Shoot::OnMontageFinished);
			MontageTask->ReadyForActivation();
		}
	}

	// VFX + sound
	PlayFireEffects(Weapon);

	// Trace + damage
	if (Weapon)
	{
		if (AAZ_GATA_LineTrace* TargetActor = Weapon->GetLineTraceTargetActor())
		{
			ConfigureTargetActor(TargetActor);

			UAZ_AT_WaitTargetDataUsingActor* WaitTargetDataTask = UAZ_AT_WaitTargetDataUsingActor::WaitTargetDataWithReusableActor(
				this,
				FName("WaitTargetData"),
				EGameplayTargetingConfirmation::Instant,
				TargetActor,
				true
			);

			WaitTargetDataTask->ValidData.AddDynamic(this, &UAZ_GA_Shoot::OnTargetDataReady);
			WaitTargetDataTask->Cancelled.AddDynamic(this, &UAZ_GA_Shoot::OnTargetDataCancelled);
			WaitTargetDataTask->ReadyForActivation();
			return;
		}
	}

	// Fallback
	PerformFallbackLineTrace();
	ConsumeAmmo();

	// Schedule next shot for burst/auto
	ShotsFiredInBurst++;
	ScheduleNextShot();
}

void UAZ_GA_Shoot::PlayFireEffects(AAZ_Weapon* Weapon)
{
	if (!Weapon) return;

	USkeletalMeshComponent* WeaponMesh = Weapon->GetWeaponMesh3P();
	if (!WeaponMesh) return;

	if (MuzzleFlashEffect && WeaponMesh->DoesSocketExist(MuzzleSocketName))
	{
		UGameplayStatics::SpawnEmitterAttached(MuzzleFlashEffect, WeaponMesh, MuzzleSocketName,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
	}

	if (FireSound)
	{
		UGameplayStatics::SpawnSoundAttached(FireSound, WeaponMesh, MuzzleSocketName);
	}
}

void UAZ_GA_Shoot::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& Data)
{
	HandleDamage(Data);
	ConsumeAmmo();

	ShotsFiredInBurst++;
	ScheduleNextShot();
}

void UAZ_GA_Shoot::OnTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UAZ_GA_Shoot::OnMontageFinished(FGameplayTag EventTag, FGameplayEventData EventData)
{
	// For single fire, end ability when montage finishes
	// For burst/auto, the fire loop handles ability end
	if (FireMode == EFireMode::Single)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UAZ_GA_Shoot::ScheduleNextShot()
{
	switch (FireMode)
	{
	case EFireMode::Single:
		if (!FireMontageSingle && !FireMontageAuto)
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		}
		break;

	case EFireMode::Auto:
		{
			const bool bBurstLimitReached = (BurstCount > 0) && (ShotsFiredInBurst >= BurstCount);

			if (bInputHeld && HasAmmo() && !bBurstLimitReached)
			{
				if (const AActor* AvatarActor = GetAvatarActorFromActorInfo())
				{
					AvatarActor->GetWorldTimerManager().SetTimer(
						FireLoopTimerHandle, this, &UAZ_GA_Shoot::OnFireDelayComplete,
						TimeBetweenShots, false);
				}
			}
			else
			{
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
			}
		}
		break;
	}
}

void UAZ_GA_Shoot::OnFireDelayComplete()
{
	FireShot();
}

// --- Helpers ---

bool UAZ_GA_Shoot::HasAmmo() const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		return ASC->GetNumericAttribute(UAZ_WeaponAttributeSet::GetRifleClipAmmoAttribute()) > 0.f;
	}
	return false;
}

void UAZ_GA_Shoot::SetShootingState(bool bShooting)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character) return;

	UAZ_AnimInstance* AnimInstance = Cast<UAZ_AnimInstance>(Character->GetMesh()->GetAnimInstance());
	if (!AnimInstance) return;

	AnimInstance->bIsShooting = bShooting;

	if (bShooting)
	{
		// Reset the timeout timer
		FTimerManager& TimerManager = Character->GetWorldTimerManager();
		TimerManager.ClearTimer(ShootingPoseTimerHandle);

		TWeakObjectPtr<UAZ_AnimInstance> WeakAnim = AnimInstance;
		TimerManager.SetTimer(ShootingPoseTimerHandle, [WeakAnim]()
		{
			if (WeakAnim.IsValid())
			{
				WeakAnim->bIsShooting = false;
			}
		}, ShootingPoseTimeout, false);
	}
}

// --- Damage ---

void UAZ_GA_Shoot::HandleDamage(const FGameplayAbilityTargetDataHandle& Data)
{
	if (!DamageGameplayEffect) return;

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(DamageGameplayEffect, GetAbilityLevel());
	if (!SpecHandle.IsValid()) return;

	UAbilitySystemComponent* OwnerASC = GetAbilitySystemComponentFromActorInfo();
	float Damage = BaseDamage;
	if (OwnerASC)
	{
		const float ASCDamage = OwnerASC->GetNumericAttribute(UAZ_WeaponAttributeSet::GetBaseDamageAttribute());
		if (ASCDamage > 0.f)
		{
			Damage = ASCDamage;
		}
	}

	SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.Damage")), Damage);

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
						OwnerASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
					}
				}
			}
		}
	}
}

void UAZ_GA_Shoot::ConsumeAmmo()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	const float CurrentAmmo = ASC->GetNumericAttribute(UAZ_WeaponAttributeSet::GetRifleClipAmmoAttribute());
	if (CurrentAmmo > 0.f)
	{
		ASC->SetNumericAttributeBase(UAZ_WeaponAttributeSet::GetRifleClipAmmoAttribute(), CurrentAmmo - 1.f);
	}
}

void UAZ_GA_Shoot::ConfigureTargetActor(AAZ_GATA_LineTrace* TargetActor) const
{
	FGameplayAbilityTargetingLocationInfo StartLocation;
	StartLocation.LocationType = EGameplayAbilityTargetingLocationType::SocketTransform;

	AAZ_Weapon* Weapon = GetPrimaryWeapon();
	if (Weapon && Weapon->GetWeaponMesh3P())
	{
		StartLocation.SourceComponent = Weapon->GetWeaponMesh3P();
		StartLocation.SourceSocketName = MuzzleSocketName;
	}

	TargetActor->Configure(
		StartLocation,
		FGameplayTag(),
		FGameplayTag(),
		FCollisionProfileName(TEXT("Weapon")),
		FGameplayTargetDataFilterHandle(),
		nullptr,
		FWorldReticleParameters(),
		false,
		false,
		false,
		bDebugTrace,
		true,
		true,
		false,
		MaxRange,
		BaseSpread,
		0.f,
		0.f,
		0.f,
		1,
		1
	);
}

// --- Fallback trace ---

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

	UAbilitySystemComponent* OwnerASC = GetAbilitySystemComponentFromActorInfo();
	float Damage = BaseDamage;
	if (OwnerASC)
	{
		const float ASCDamage = OwnerASC->GetNumericAttribute(UAZ_WeaponAttributeSet::GetBaseDamageAttribute());
		if (ASCDamage > 0.f)
		{
			Damage = ASCDamage;
		}
	}

	SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName("Data.Damage")), Damage);
	OwnerASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
}
