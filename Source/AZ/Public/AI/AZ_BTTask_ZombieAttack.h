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

private:
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
};
