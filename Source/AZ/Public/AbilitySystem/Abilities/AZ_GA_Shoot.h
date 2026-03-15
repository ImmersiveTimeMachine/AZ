// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "AZ_GA_Shoot.generated.h"

class AAZ_Weapon;
class AAZ_GATA_LineTrace;

UCLASS()
class AZ_API UAZ_GA_Shoot : public UAZ_GameplayAbility
{
	GENERATED_BODY()

public:

	UAZ_GA_Shoot();

protected:

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	UFUNCTION()
	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& Data);

	UFUNCTION()
	void OnTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data);

	void HandleDamage(const FGameplayAbilityTargetDataHandle& Data);
	void ConsumeAmmo();

	AAZ_Weapon* GetEquippedWeapon() const;
	AAZ_GATA_LineTrace* GetOrSpawnLineTraceTargetActor();
	void ConfigureTargetActor(AAZ_GATA_LineTrace* TargetActor) const;

	// Fallback if no weapon source object
	void PerformFallbackLineTrace();
	void ApplyDamageToTarget(AActor* HitActor, const FHitResult& HitResult);

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Shoot")
	float MaxRange = 10000.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Shoot")
	float BaseDamage = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Shoot")
	TSubclassOf<UGameplayEffect> DamageGameplayEffect;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Shoot")
	float BaseSpread = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Shoot")
	bool bDebugTrace = false;
};
