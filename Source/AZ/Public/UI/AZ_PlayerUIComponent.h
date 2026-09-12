// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/AZ_PlayerUITypes.h"
#include "AZ_PlayerUIComponent.generated.h"

class APlayerController;
class APawn;
class AAZ_Weapon;
class UAbilitySystemComponent;
class UAZ_Inv_CommonUI_EquipmentComponent;
class UAZ_Inv_CommonUI_InventoryComponent;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAZ_PlayerUIVitalsChanged, const FAZ_PlayerVitalsView&, Vitals);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAZ_PlayerUIWeaponChanged, const FAZ_PlayerWeaponView&, Weapon);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAZ_PlayerUIReticleChanged, const FAZ_PlayerReticleView&, Reticle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAZ_PlayerUIHitConfirmed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAZ_PlayerUIInventoryFull);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAZ_PlayerUIInventoryVisibilityChanged, bool, bOpen);

/**
 * Per-controller, local presentation subscriptions shared by HUD and inventory.
 * Reads the PlayerState ASC and controller inventory/equipment; never replicates
 * a second truth, mutates gameplay, polls attributes or waits for avatar readiness.
 */
UCLASS(ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class AZ_API UAZ_PlayerUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAZ_PlayerUIComponent();

	/** Call when the local controller's PlayerState or possession changes. */
	UFUNCTION(BlueprintCallable, Category="AZ|UI")
	void RefreshBindings();
	void NotifyInputCaptureChanged();

	UFUNCTION(BlueprintPure, Category="AZ|UI")
	FAZ_PlayerVitalsView GetVitalsView() const { return VitalsView; }

	UFUNCTION(BlueprintPure, Category="AZ|UI")
	FAZ_PlayerWeaponView GetWeaponView() const { return WeaponView; }

	UFUNCTION(BlueprintPure, Category="AZ|UI")
	FAZ_PlayerReticleView GetReticleView() const { return ReticleView; }

	UFUNCTION(BlueprintPure, Category="AZ|UI")
	bool IsInventoryOpen() const { return bInventoryOpen; }

	UPROPERTY(BlueprintAssignable, Category="AZ|UI")
	FAZ_PlayerUIVitalsChanged OnVitalsChanged;

	UPROPERTY(BlueprintAssignable, Category="AZ|UI")
	FAZ_PlayerUIWeaponChanged OnWeaponChanged;

	UPROPERTY(BlueprintAssignable, Category="AZ|UI")
	FAZ_PlayerUIReticleChanged OnReticleChanged;

	UPROPERTY(BlueprintAssignable, Category="AZ|UI")
	FAZ_PlayerUIHitConfirmed OnHitConfirmed;

	UPROPERTY(BlueprintAssignable, Category="AZ|UI")
	FAZ_PlayerUIInventoryFull OnInventoryFull;

	UPROPERTY(BlueprintAssignable, Category="AZ|UI")
	FAZ_PlayerUIInventoryVisibilityChanged OnInventoryVisibilityChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category="AZ|UI|Vitals", meta=(ClampMin="0", ClampMax="1"))
	float CriticalHealthFraction = .25f;

private:
	TWeakObjectPtr<APlayerController> OwningController;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<UAZ_Inv_CommonUI_InventoryComponent> BoundInventory;
	TWeakObjectPtr<UAZ_Inv_CommonUI_EquipmentComponent> BoundEquipment;
	TWeakObjectPtr<AAZ_Weapon> PresentationWeapon;
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FDelegateHandle WeaponOwnershipChangedHandle;
	/** Samples the item's analytical recovery only while a visible reticle is recovering. */
	FTimerHandle ReticleRecoveryTimer;
	TMap<FGameplayTag, FDelegateHandle> ReticleTagChangedHandles;
	bool bInventoryOpen = false;
	bool bEndingPlay = false;

	UPROPERTY(Transient)
	FAZ_PlayerVitalsView VitalsView;

	UPROPERTY(Transient)
	FAZ_PlayerWeaponView WeaponView;

	UPROPERTY(Transient)
	FAZ_PlayerReticleView ReticleView;

	void UnbindController();
	void UnbindVitals();
	void UnbindInventory();
	void UnbindEquipment();
	void SetPresentationWeapon(AAZ_Weapon* Weapon);
	void RefreshVitals();
	void RefreshWeapon();
	void RefreshReticle();
	void HandleVitalsChanged(const FOnAttributeChangeData& Change);
	void HandleReticleTagChanged(FGameplayTag Tag, int32 Count);
	void HandleWeaponOwnershipChanged();

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleEquipmentChanged();

	UFUNCTION()
	void HandleInventoryVisibilityChanged(bool bOpen);

	UFUNCTION()
	void HandleInventoryFull();

	UFUNCTION()
	void HandleHitConfirmed(const FHitResult& Hit);

	UFUNCTION()
	void HandleWeaponDestroyed(AActor* DestroyedActor);
};
