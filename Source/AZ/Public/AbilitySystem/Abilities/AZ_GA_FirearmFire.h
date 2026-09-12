#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemState.h"
#include "AZ_GA_FirearmFire.generated.h"

class AAZ_Weapon;
class UAZ_Inv_CommonUI_EquipmentComponent;

/** A fresh press starts a single shot or an authority-owned automatic burst. */
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
	FGuid FireActionId;
	TWeakObjectPtr<AAZ_Weapon> FiringWeapon;
	TWeakObjectPtr<UAZ_Inv_CommonUI_EquipmentComponent> FiringEquipment;
	FTimerHandle InitialFireTimer;
	FTimerHandle AutomaticFireTimer;
	double InitialRaiseDeadline = 0.0;
	FGuid InitialMagazineId;
	int64 InitialAmmoRevision = -1;
	uint32 EquipmentGeneration = 0;
	int64 FireModeRevision = 0;
	EAZ_FirearmFireMode ActiveFireMode = EAZ_FirearmFireMode::Single;
	bool bShotInProgress = false;
	bool bAnimationStarted = false;
	bool bRequiresAim = true;
	bool bInitialShotPending = false;
	bool bInputReleased = false;
	bool bEndingFire = false;
	FDelegateHandle AimChangedHandle;
	FDelegateHandle ReadyChangedHandle;

	bool FireAuthoritativeShot();
	void OnInitialShotDue(FGuid ExpectedActionId);
	void ScheduleAutomaticShot();
	void OnAutomaticShotDue(FGuid ExpectedActionId);
	void OnAimTagChanged(FGameplayTag Tag, int32 Count);
	UFUNCTION() void OnFireInputReleased(float TimeHeld);
};
