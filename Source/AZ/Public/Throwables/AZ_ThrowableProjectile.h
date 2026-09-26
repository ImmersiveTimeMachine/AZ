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
class UNiagaraSystem;
class USoundBase;

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
	/** Saving waits for an in-flight item/fuse to reach its durable world outcome. */
	bool HasPendingWorldOutcome() const { return bActivated && !bShattered && !bDetonated && bHasRecoveryPayload; }

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

protected:
	/**
	 * Blow up here, now. Authority only and exactly once.
	 *
	 * Lives on the PROJECTILE and not on UAZ_GA_Throw because the ability is already over: it ends as the
	 * grenade leaves the hand, seconds before the fuse runs out. Anything that has to outlive the throw has
	 * to belong to the thing that is still in the world.
	 */
	void Detonate();

	/** Damage, reaction and corpse impulse in ONE pass over the blast's targets — they share the overlap,
	 *  the line-of-sight trace and the falloff, and splitting them would triple all three. */
	void ApplyBlast(const FVector& Origin);

	/** Niagara, sound, camera shake and scorch decal. Cosmetic only: never decides anything. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDetonationFX(const FVector& Origin, const FVector& SurfaceNormal);

	/** Second, deferred half of the blast: push the bodies it killed. Separate because the ragdoll it needs
	 *  does not exist yet at the moment the damage lands — the death ability hands it over afterwards, and one
	 *  infected class delays even that on a timer. */
	UFUNCTION()
	void ApplyCorpseImpulses();

private:
	/** Reports a bounce to AI hearing, rate-limited and speed-gated. */
	void ReportImpactNoise(const FVector& Location, float ImpactSpeed);

	/** Convert the resting object into the ordinary world pickup. Hands ownership over exactly once. */
	bool ConvertToPickup(const FVector& RestSurfacePoint);
	void Shatter(const FHitResult& Hit);
	UFUNCTION(NetMulticast, Reliable)
	void MulticastShatterFX(const FVector& Location, const FVector& Normal, UNiagaraSystem* Effect, USoundBase* Sound);
	bool bShattered = false;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * The item this projectile IS while it is in flight. Ownership is single and explicit: inventory spent
	 * it, this actor holds it, and the pickup receives it — cleared the instant it is handed on so a
	 * re-entrant settle cannot mint a second copy of the same stone.
	 */
	UPROPERTY()
	FAZ_InventoryPickupRecord RecoveryPayload;

	bool bHasRecoveryPayload = false;

	/**
	 * ★ REPLICATED, because the detonation's cosmetics are read off it on every machine. The multicast
	 * that spawns the Niagara, the sound, the shake and the decal runs on clients too, and a client that
	 * never received this would early-out and show an explosion that makes no noise and leaves no mark.
	 * A data asset is stable-named, so the reference costs a path and nothing else.
	 */
	UPROPERTY(Replicated)
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

	/** Counts down from release for a FuseAndDetonate throwable. Cleared in EndPlay so a projectile removed
	 *  early (level teardown, MaxFlightTime, a destroyed thrower) cannot fire a blast from a dead actor. */
	FTimerHandle FuseTimer;

	/** Targets the blast damaged, revisited a moment later so the ones it killed can be thrown. Weak: a
	 *  corpse may be cleaned up between the blast and the push. */
	TArray<TWeakObjectPtr<AActor>> PendingImpulseTargets;
	FVector PendingImpulseOrigin = FVector::ZeroVector;
	FTimerHandle ImpulseTimer;

	/** One detonation per object, whatever route reaches it — fuse, or a future impact trigger. */
	bool bDetonated = false;

	double LastNoiseTime = -1000.0;
	int32 BounceCount = 0;
	bool bSettled = false;
};
