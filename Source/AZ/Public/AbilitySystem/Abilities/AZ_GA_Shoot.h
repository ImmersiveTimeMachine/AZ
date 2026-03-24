#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "AZ_GA_Shoot.generated.h"

class AAZ_Weapon;
class AAZ_GATA_LineTrace;

UENUM(BlueprintType)
enum class EFireMode : uint8
{
	Single,
	Auto		// BurstCount > 0 = burst, BurstCount == 0 = full auto
};

UCLASS()
class AZ_API UAZ_GA_Shoot : public UAZ_GameplayAbility
{
	GENERATED_BODY()

public:

	UAZ_GA_Shoot();

protected:

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

	// --- Core fire logic (one shot) ---
	void FireShot();
	void PlayFireEffects(AAZ_Weapon* Weapon);

	UFUNCTION()
	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& Data);

	UFUNCTION()
	void OnTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data);

	void HandleDamage(const FGameplayAbilityTargetDataHandle& Data);
	void ConsumeAmmo();

	UFUNCTION()
	void OnMontageFinished(FGameplayTag EventTag, FGameplayEventData EventData);

	// --- Burst/Auto loop ---
	void ScheduleNextShot();

	UFUNCTION()
	void OnFireDelayComplete();

	AAZ_GATA_LineTrace* GetOrSpawnLineTraceTargetActor();
	void ConfigureTargetActor(AAZ_GATA_LineTrace* TargetActor) const;

	// Fallback if no weapon source object
	void PerformFallbackLineTrace();
	void ApplyDamageToTarget(AActor* HitActor, const FHitResult& HitResult);

	bool HasAmmo() const;
	void SetShootingState(bool bShooting);

	// --- Properties ---

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Shoot")
	EFireMode FireMode = EFireMode::Single;

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

	/** Time between shots for burst/auto (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Shoot")
	float TimeBetweenShots = 0.1f;

	/** Max shots per trigger pull in auto mode. 0 = unlimited (full auto), 3 = burst. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Shoot", meta=(EditCondition="FireMode==EFireMode::Auto"))
	int32 BurstCount = 0;

	/** Fire montage for single shot. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Shoot|Animation")
	UAnimMontage* FireMontageSingle = nullptr;

	/** Fire montage for auto/burst (plays per shot). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Shoot|Animation")
	UAnimMontage* FireMontageAuto = nullptr;

	/** Muzzle flash particle. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Shoot|VFX")
	UParticleSystem* MuzzleFlashEffect = nullptr;

	/** Fire sound. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Shoot|Audio")
	USoundBase* FireSound = nullptr;

	/** Muzzle socket name on weapon mesh. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Shoot|VFX")
	FName MuzzleSocketName{TEXT("Muzzle")};

	/** How long the aim pose holds after the last shot before returning to relaxed. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Shoot|Animation")
	float ShootingPoseTimeout = 5.f;

	// --- Runtime state ---
	FTimerHandle ShootingPoseTimerHandle;
	FTimerHandle FireLoopTimerHandle;
	int32 ShotsFiredInBurst = 0;
	bool bInputHeld = false;
};
