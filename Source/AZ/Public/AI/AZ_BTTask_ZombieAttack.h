// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "AZ_BTTask_ZombieAttack.generated.h"

class UAbilitySystemComponent;

/**
 * Chase-branch attack: activates the pawn's melee ability (UAZ_GA_ZombieMelee by default) through GAS
 * and stays latent until the ability ends — so the BT's cadence IS the ability's cadence (one timed
 * swing per execution; the sequence loops for sustained clawing). Replaces the old Wait breather.
 * Instanced: per-AI delegate binding + timeout state live on the node instance.
 */
UCLASS()
class AZ_API UAZ_BTTask_ZombieAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UAZ_BTTask_ZombieAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual FString GetStaticDescription() const override;

	/** The melee ability to fire. Default = the native zombie claw swipe. */
	UPROPERTY(EditAnywhere, Category = "AZ")
	TSubclassOf<class UGameplayAbility> AbilityClass;

	/** Safety net: finish (failed) if the ability never reports ending. */
	UPROPERTY(EditAnywhere, Category = "AZ", meta = (ClampMin = "1"))
	float TimeoutSeconds = 5.f;

	// ---- GRAB (random, mid-engagement, no telegraph — user design 2026-07-24) ----
	// When this Chalkie has already won its attack slot and is about to swing, a small roll turns the
	// swing into a GRAB instead (UAZ_GA_ChalkieGrab). To the tree it's just a longer attack: same
	// latent wait, same facing, same SetMeleeTaskActive crowd lock. az.Grab.* CVars override for tests.

	/** The grab ability. Default = the native UAZ_GA_ChalkieGrab. */
	UPROPERTY(EditAnywhere, Category = "AZ|Grab")
	TSubclassOf<class UGameplayAbility> GrabAbilityClass;

	/** Per-attack-opportunity chance the swing becomes a grab (az.Grab.Chance overrides). */
	UPROPERTY(EditAnywhere, Category = "AZ|Grab", meta = (ClampMin = "0", ClampMax = "1"))
	float GrabChance = 0.10f;

	/** Per-Chalkie cooldown, counted from the END of its last grab (az.Grab.CooldownSeconds overrides). */
	UPROPERTY(EditAnywhere, Category = "AZ|Grab", meta = (ClampMin = "0"))
	float GrabCooldownSeconds = 45.f;

	/** Safety timeout for a GRAB run (hold window + exit montage — much longer than a swing). */
	UPROPERTY(EditAnywhere, Category = "AZ|Grab", meta = (ClampMin = "5"))
	float GrabTimeoutSeconds = 30.f;

	// ---- MOTION WARPING (replaces the per-tick facing override) ----
	// The old soft-track steered the ORIENTATION system for the whole task, so the zombie kept turning
	// after the claw had already committed (reads as aimbot) and it fought the "moving and fighting never
	// overlap" rule on the same channel movement uses. Warping instead deforms the montage's own
	// root-motion delta, inside a window the animator authors — the zombie tracks during wind-up and
	// legitimately MISSES a late dodge.

	/** Use motion warping for mid-swing tracking. False = fall back to the legacy facing override
	 *  (kept so the two can be A/B'd in PIE without a rebuild). */
	UPROPERTY(EditAnywhere, Category = "AZ|Warp")
	bool bUseMotionWarping = true;

	/** Rendezvous name between the warp target we register here and the WarpTargetName on the montage's
	 *  MotionWarping notify. Must match the notify exactly or the window is a no-op. */
	UPROPERTY(EditAnywhere, Category = "AZ|Warp", meta = (EditCondition = "bUseMotionWarping"))
	FName WarpTargetName = TEXT("AttackTarget");

	/** How far IN FRONT of the target the warp point sits, measured back along the target->zombie vector
	 *  (so it tracks a strafing player automatically). This is where translation warping parks the body.
	 *
	 *  It matters because the claw clips are IN-PLACE: with no root translation of their own, SkewWarp
	 *  takes its "add translation" branch and eases the zombie from where it stood at window-open all the
	 *  way ONTO the warp point. Aim that point at the target's centre and the zombie ends up standing
	 *  inside them, so it is offset back to claw range instead.
	 *
	 *  ★ DERIVED FROM MEASURED REACH, not from feel. Contact needs
	 *      centre-to-centre ≤ claw_forward_extension + sweep_radius + victim_capsule_radius.
	 *  Sampled off the clips (AnimPoseExtensions, pelvis-relative, mesh yaw -90 so anim +Y is forward):
	 *  Zombie_Atk_Arm_2_R reaches +93cm inside its window, Arm_1_L only +57cm. With a 15cm claw sweep and
	 *  the hero's 30cm capsule that is 138cm and 102cm of reach — so the old 120 parked the zombie 18cm
	 *  BEYOND its own shorter claw and that attack could never land. 100 clears the worst clip with
	 *  margin. Raise this and you silently un-arm whichever attack has the shortest reach.
	 *
	 *  Since swings only start within AZ_MaxAttackStartDistance (180cm), the most this can ever move a
	 *  zombie is ~80cm across the whole window — a lean-in, not a slide. */
	UPROPERTY(EditAnywhere, Category = "AZ|Warp", meta = (EditCondition = "bUseMotionWarping", ClampMin = "50", ForceUnits = "cm"))
	float WarpApproachDistance = 100.f;

private:
	/** Point the pawn's warping component at the chase target's root, in follow mode so a strafing target
	 *  is re-read every frame rather than aimed at once. No-op when warping is off or either side is
	 *  missing. Called before the ability activates — the montage can start synchronously. */
	void RegisterWarpTarget(UBehaviorTreeComponent& OwnerComp);
	void ClearWarpTarget();

	void OnAbilityEnded(const struct FAbilityEndedData& EndedData);
	void Cleanup();

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<UBehaviorTreeComponent> OwningComp;
	FDelegateHandle AbilityEndedHandle;
	float ElapsedSeconds = 0.f;

	/** What THIS run activated (melee or grab) — every cancel/end lookup keys on this, not AbilityClass. */
	TSubclassOf<class UGameplayAbility> ChosenAbilityClass;
	/** True while the current latent run is a grab (longer timeout, cooldown stamp on exit). */
	bool bGrabRun = false;

	/** Root-motion generation THIS swing queued (0 = none). The mid-swing move-break exempts motion only
	 *  while this generation is still the live drive — so our own warp lunge doesn't cancel the swing,
	 *  but a knockback that supersedes it still does. Reset in Cleanup. */
	uint64 MeleeRootMotionGen = 0;
};
