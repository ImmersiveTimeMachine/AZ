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

private:
	void OnAbilityEnded(const struct FAbilityEndedData& EndedData);
	void Cleanup();

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<UBehaviorTreeComponent> OwningComp;
	FDelegateHandle AbilityEndedHandle;
	float ElapsedSeconds = 0.f;
};
