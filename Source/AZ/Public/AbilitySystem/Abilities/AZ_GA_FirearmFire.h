#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "AZ_GA_FirearmFire.generated.h"

/** One press, one authoritative shot from the selected inventory firearm. */
UCLASS()
class AZ_API UAZ_GA_FirearmFire : public UAZ_GameplayAbility
{
	GENERATED_BODY()

public:
	UAZ_GA_FirearmFire();
	virtual void DeclareAbilityTags() override;
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	// Never reset on release: fast taps cannot reset the cadence. The inventory also
	// enforces an authoritative per-item cadence that survives removal of this grant.
	double NextAllowedFireTime = 0.0;
	FGuid FiredItemId;
	FGuid ShotId;
	uint32 EquipmentGeneration = 0;
	bool bShotResolved = false;
	bool bRequiresAim = true;
	FDelegateHandle AimChangedHandle;

	bool FireAuthoritativeShot();
	void OnAimTagChanged(FGameplayTag Tag, int32 Count);
	UFUNCTION() void OnFireInputReleased(float TimeHeld);
};
