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
 * SWEEPS EVERY STRIKE SOCKET, not the one the ability's Hand enum picked. Measured reason: the clip's
 * name lies about which fist lands. AM_Zombie_Atk_L's in-window forward rake is the RIGHT claw (+57cm at
 * 1.15s) while its left hand is BEHIND the body; RTG_RM_Fists_Punch_Heavy2Idle's biggest strike is a
 * right hook at 0.50s. Sweeping the "matching" socket meant those swings could never connect. The
 * ANIMATION owns which hand connects — code sweeping both and letting geometry decide is the only
 * version of that fact with one owner. AlreadyHit is shared across sockets, so a two-fisted clip still
 * lands one hit per victim per swing.
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

	/** Start sweeping every socket in SocketNames each tick until EndTask (the ability calls that on
	 *  WindowEnd/end). Empty or all-None = nothing to trace; the task ends itself. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Ability|Tasks",
		meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAZ_AT_MeleeSweep* MeleeSweepWindow(UGameplayAbility* OwningAbility, const TArray<FName>& SocketNames,
		float SphereRadius = 12.f, bool bHostilesOnly = true, bool bSingleTarget = true);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	/** Final flush: one last sweep from the previous tick's socket positions to where they are when the
	 *  window closes. Without it, contact between the last tick and WindowEnd is silently dropped — the
	 *  same off-by-one-frame hole as the old first-tick skip, at the other end of the window. */
	virtual void OnDestroy(bool bInOwnerFinished) override;

	/** One broadcast per valid target, at the actual moment of contact. */
	UPROPERTY(BlueprintAssignable)
	FAZMeleeHitDelegate OnHit;

private:
	/** Sweep every socket from its last recorded position to where it is now, then report the EARLIEST
	 *  valid contact across all of them. Shared by the per-tick pass and the closing flush. */
	void SweepSinceLastFrame();

	/** Every socket traced this window (both fists for unarmed; a weapon's tip/hilt pair later).
	 *  Sockets the skeleton doesn't have are dropped at Activate. */
	TArray<FName> SocketNames;
	float SphereRadius = 12.f;
	bool bHostilesOnly = true;
	bool bSingleTarget = true;

	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	/** Parallel to SocketNames — last frame's world position of each socket. */
	TArray<FVector> PrevLocations;
	bool bHasPrevious = false;
	/** Single-target punch landed — keep ticking cheaply but detect nothing further. */
	bool bConsumed = false;
	TSet<TWeakObjectPtr<AActor>> AlreadyHit;
};
