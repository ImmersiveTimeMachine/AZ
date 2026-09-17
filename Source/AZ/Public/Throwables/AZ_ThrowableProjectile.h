// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "Throwables/AZ_ThrowableTypes.h"
#include "AZ_ThrowableProjectile.generated.h"

class UAZ_ThrowableDefinition;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAZ_ThrowableSettled,
	AAZ_ThrowableProjectile*, Projectile, const FVector&, Location);

/**
 * AAZ_ThrowableProjectile — the authoritative thrown object.
 *
 * Owns flight and the world outcome from release onward. It does NOT own the item: inventory spent the unit
 * before this was activated, and if the object becomes recoverable it hands the payload to the existing
 * pickup flow. There is exactly one owner at any instant — inventory, this, or the pickup.
 *
 * ★ Spawned INERT. Construction runs callbacks, and a failure between spawn and the inventory commit must
 * leave nothing behind and nothing spent, so the actor is hidden and non-colliding until Activate() is
 * called after the commit has been written. Activation is therefore the point of no return, not spawning.
 *
 * Movement is UProjectileMovementComponent, not Chaos: the component owns the motion so the ballistic
 * preview and the real flight integrate the same way. Enabling physics on the updated component would hand
 * motion to Chaos and immediately invalidate the preview's contract.
 */
UCLASS()
class AZ_API AAZ_ThrowableProjectile : public AActor
{
	GENERATED_BODY()

public:
	AAZ_ThrowableProjectile();

	/**
	 * Arm and launch. Called by authority only, AFTER the inventory unit has been spent and the receipt
	 * written. Before this the actor is inert and safe to destroy.
	 */
	void Activate(const FAZ_ThrowLaunchSolution& Solution, const UAZ_ThrowableDefinition* InDefinition,
		APawn* InThrower);

	/**
	 * Hand this action's item record to the projectile, authority only and only after the inventory commit
	 * produced it. The record is the SAME one inventory spent — never a reconstruction — so a recovered
	 * stone is the stone that was thrown, with its identity rule already applied (whole item keeps its GUID,
	 * a split stack got a fresh one).
	 */
	void SetRecoveryPayload(const FAZ_InventoryPickupRecord& InPayload);

	/** Broadcast once when the object comes to rest and is eligible to become a pickup again. */
	UPROPERTY(BlueprintAssignable, Category = "AZ|Throwable")
	FAZ_ThrowableSettled OnSettled;

	const UAZ_ThrowableDefinition* GetDefinition() const { return Definition; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void HandleBounce(const FHitResult& Hit, const FVector& Velocity);

	UFUNCTION()
	void HandleStop(const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, Category = "AZ|Throwable")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "AZ|Throwable")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "AZ|Throwable")
	TObjectPtr<UProjectileMovementComponent> Movement;

private:
	/** Reports a bounce to AI hearing, rate-limited and speed-gated. */
	void ReportImpactNoise(const FVector& Location, float ImpactSpeed);

	/** Convert the resting object into the ordinary world pickup. Hands ownership over exactly once. */
	bool ConvertToPickup(const FVector& RestSurfacePoint);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * The item this projectile IS while it is in flight. Ownership is single and explicit: inventory spent
	 * it, this actor holds it, and the pickup receives it — cleared the instant it is handed on so a
	 * re-entrant settle cannot mint a second copy of the same stone.
	 */
	UPROPERTY()
	FAZ_InventoryPickupRecord RecoveryPayload;

	bool bHasRecoveryPayload = false;

	UPROPERTY()
	TObjectPtr<const UAZ_ThrowableDefinition> Definition;

	/**
	 * The thrower, kept because AI hearing is reported with the IMPACT location but the THROWER as
	 * instigator. AZ's hearing rejects non-hostile stimulus instigators, so a neutral projectile named as
	 * the source would make a stone silently inaudible — the opposite of what a stone is for.
	 */
	UPROPERTY(Replicated)
	TObjectPtr<APawn> Thrower;

	UPROPERTY(Replicated)
	bool bActivated = false;

	/** How far above the resting surface the recovered pickup is placed. Matches the inventory's own drop
	 *  placement, which lifts by the same amount so the spawn is not refused for intersecting the floor. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Throwable", meta = (ClampMin = "0", ForceUnits = "cm"))
	float PickupSurfaceClearance = 12.f;

	double LastNoiseTime = -1000.0;
	int32 BounceCount = 0;
	bool bSettled = false;
};
