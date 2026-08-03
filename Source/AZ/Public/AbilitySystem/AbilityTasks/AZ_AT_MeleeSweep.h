// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AZ_AT_MeleeSweep.generated.h"

class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAZMeleeHitDelegate, const FHitResult&, Hit);

/**
 * SOCKET-SWEPT hit detection — the well-known AAA melee shape, replacing the old actor-forward volume
 * at a hand-placed contact frame (the source of every melee timing bug this project has had: the
 * contact moment had to be GUESSED per clip, and the guess was wrong twice on one clip).
 *
 * Runs between the montage's WindowBegin/WindowEnd events (GA_MeleeAttack starts/stops it): each tick,
 * sweeps a small sphere from the strike socket's PREVIOUS world position to its current one.
 *  - The frame the fist/claw actually touches a pawn IS the hit — contact timing is physics, not data.
 *  - Prev→current segment sweep: a fast swing cannot tunnel through a capsule between frames.
 *  - GetSocketLocation is WORLD space — accurate under motion warping (reads where the hand really is,
 *    not where the clip authored it), and immune to the mesh-space/actor-space confusion that produced
 *    garbage measurements before.
 *  - No facing cone: the fist's actual path IS the filter. The cone was a patch for an actor-space
 *    volume that could "hit" things the hand never approached.
 *
 * Filters (all in here so the ability just applies damage): self, once-per-target-per-swing, hostiles
 * only, and corpses (permanent corpses keep a hostile team + live ASC — audit rules-finding #2: a dead
 * body must not eat a single-target punch meant for the live attacker behind it). bSingleTarget: the
 * first CONSUMED hit ends detection — a punch is not a cleave; multi-hit weapons flip the flag.
 *
 * State lives HERE, on the ability task instance — deliberately not on a notify state, whose objects
 * are shared between all concurrent players of the montage (the classic engine trap).
 */
UCLASS()
class AZ_API UAZ_AT_MeleeSweep : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAZ_AT_MeleeSweep(const FObjectInitializer& ObjectInitializer);

	/** Start sweeping SocketName every tick until EndTask (the ability calls that on WindowEnd/end). */
	UFUNCTION(BlueprintCallable, Category = "AZ|Ability|Tasks",
		meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAZ_AT_MeleeSweep* MeleeSweepWindow(UGameplayAbility* OwningAbility, FName SocketName,
		float SphereRadius = 12.f, bool bHostilesOnly = true, bool bSingleTarget = true);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

	/** One broadcast per valid target, at the actual moment of contact. */
	UPROPERTY(BlueprintAssignable)
	FAZMeleeHitDelegate OnHit;

private:
	FName SocketName;
	float SphereRadius = 12.f;
	bool bHostilesOnly = true;
	bool bSingleTarget = true;

	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	FVector PrevLocation = FVector::ZeroVector;
	bool bHasPrevious = false;
	/** Single-target punch landed — keep ticking cheaply but detect nothing further. */
	bool bConsumed = false;
	TSet<TWeakObjectPtr<AActor>> AlreadyHit;
};
