#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "DefaultMovementSet/MovementModifiers/StanceModifier.h"
#include "AZ_GA_FirearmReload.generated.h"

class AAZ_Weapon;
class UAZ_Inv_CommonUI_InventoryComponent;
class UAZ_Inv_CommonUI_EquipmentComponent;
class UAZ_PawnMoverComponent;

/** One authority-owned magazine swap, committed when the selected reload clip finishes. */
UCLASS()
class AZ_API UAZ_GA_FirearmReload : public UAZ_GameplayAbility
{
	GENERATED_BODY()

public:
	UAZ_GA_FirearmReload();
	/** Authority-only request context scoped to this source-owned instance's synchronous activation. */
	bool TryActivateMagazineRequest(UAbilitySystemComponent* ASC, FGameplayAbilitySpecHandle Handle,
		AAZ_Weapon* ExpectedSource, const FGuid& ExpectedItemId, uint32 ExpectedGeneration,
		const FGuid& RequestedMagazineId, bool bSkipEmpty);
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
	struct FMagazineActivationRequest
	{
		TWeakObjectPtr<AAZ_Weapon> WeaponSource;
		FGuid WeaponItemId;
		uint32 Generation = 0;
		FGuid MagazineItemId;
		bool bSkipEmpty = false;
	};
	FMagazineActivationRequest PendingMagazineRequest;
	bool bMagazineRequestInProgress = false;
	FGuid ReloadActionId;
	FGuid ReloadedItemId;
	FGuid RequestedReloadMagazineId;
	uint32 EquipmentGeneration = 0;
	bool bReloadSkipEmpty = false;
	bool bReloadCrouching = false;
	bool bReloadRaised = false;
	bool bEndingReload = false;
	TWeakObjectPtr<AAZ_Weapon> ReloadingWeapon;
	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryComponent> ReloadInventory;
	TWeakObjectPtr<UAZ_Inv_CommonUI_EquipmentComponent> ReloadEquipment;
	TWeakObjectPtr<UAZ_PawnMoverComponent> ReloadMover;
	FTimerHandle CompletionTimer;
	FDelegateHandle AnimationInterruptedHandle;

	void OnReloadCompleted(FGuid ExpectedActionId);
	void OnReloadAnimationInterrupted(const FGuid& InterruptedActionId);
	UFUNCTION() void OnReloadStanceChanged(EStanceMode PreviousStance, EStanceMode NewStance);
};
