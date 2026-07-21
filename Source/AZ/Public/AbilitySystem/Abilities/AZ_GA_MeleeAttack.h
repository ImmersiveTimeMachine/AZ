// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"   // our base, NOT UGameplayAbility
#include "AZ_GA_MeleeAttack.generated.h"

class UAZ_AT_PlayMontageAndWaitForEvent;
class UGameplayEffect;

// Which hand threw it — latched at activation from the InputTag.
UENUM(BlueprintType)
enum class EAZ_MeleeHand : uint8 { Left, Right };

UCLASS()
class AZ_API UAZ_GA_MeleeAttack : public UAZ_GameplayAbility
{
	GENERATED_BODY()

public:
	UAZ_GA_MeleeAttack();
	
protected:
	
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;
	
	virtual void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	
	// The selector. First pass: hardcoded switch over the 4 fist clips. CHT_Melee later.
	UAnimMontage* SelectMontage() const;

	// Montage finished / interrupted / cancelled -> end the ability.
	UFUNCTION()
	void OnMontageFinished(FGameplayTag EventTag, FGameplayEventData EventData);

	// Hit-window (and later combo-window) GameplayEvents from notifies. On the hit window: forward
	// sphere sweep from the avatar, team-filtered to hostiles, GE_Damage w/ SetByCaller.Damage applied
	// to each — authority only.
	UFUNCTION()
	void OnMontageEvent(FGameplayTag EventTag, FGameplayEventData EventData);

	// --- Avatar access, pawn-class-agnostic (hero AND infected run this same ability) ---
	USkeletalMeshComponent* GetAvatarMesh() const;
	bool ResolveAvatarIsMoving() const;

	// --- Tunables: assign the 4 fist montages in a BP child (like GA_Shoot's FireMontage*) ---
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Animation") UAnimMontage* PunchIdle_L = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Animation") UAnimMontage* PunchIdle_R = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Animation") UAnimMontage* PunchMove_L = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Animation") UAnimMontage* PunchMove_R = nullptr;

	// The tag the hit-window notify sends (matched in OnMontageEvent). Left unset = defaults to
	// Event.Montage.Melee.Hit at activation (can't read the native tag registry in the CDO ctor).
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee") FGameplayTag HitWindowEventTag;

	// --- Damage (S1 spine) ---
	/** GE applied to each swept hostile; magnitude rides SetByCaller.Damage. Default = UAZ_GE_Damage. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Damage") TSubclassOf<UGameplayEffect> DamageEffect;
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Damage", meta = (ClampMin = "0")) float DamageAmount = 25.f;
	/** Sweep reach forward from the avatar's center (cm) and the sphere radius swept along it. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Damage", meta = (ClampMin = "0", ForceUnits = "cm")) float MeleeRange = 160.f;
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee|Damage", meta = (ClampMin = "0", ForceUnits = "cm")) float MeleeRadius = 60.f;

	// GEs applied to the owner on each activation — e.g. GE_CombatReady to REFRESH the fists-up stance every punch.
	// The GE owns its own duration + refresh-on-reapply stacking; this just re-applies it. Set in the BP ability.
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Melee") TArray<TSubclassOf<UGameplayEffect>> EffectsOnActivate;

	// --- Runtime state, latched at activation ---
	UPROPERTY() UAZ_AT_PlayMontageAndWaitForEvent* MontageTask = nullptr;
	UPROPERTY(EditDefaultsOnly) EAZ_MeleeHand Hand = EAZ_MeleeHand::Left;
	bool bIsMovingLatched = false;
	FGameplayTag ProfileTag;          // current Weapon.* — used by SelectMontage when CHT lands
	int32 ComboIndex = 0;             // later
	
};
