// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Animation/AZ_LocomotionTypes.h"   // EAZ_Gait
#include "AZ_BTTask_ChalkieSetGait.generated.h"

/**
 * UAZ_BTTask_ChalkieSetGait — instant task: set the Chalkie's gait for the branch it runs in.
 * Patrol/return = Walk (58, the Chase-loop shamble); chase/investigate = Run (273, HyperChase).
 * Writes the pawn's AI intent surface (SetGait) — the walking mode resolves speed, anim follows the capsule.
 *
 * Urgency mode (Investigate branch): with bGaitFromInvestigateUrgency the fixed Gait is ignored and the
 * Blackboard's bInvestigateUrgent picks it — heard-only noise = Walk (wary/curious, the TLOU creep-over),
 * lost-a-chase or escalated = Run (it KNOWS someone is here).
 */
UCLASS()
class AZ_API UAZ_BTTask_ChalkieSetGait : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UAZ_BTTask_ChalkieSetGait();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "AZ")
	EAZ_Gait Gait = EAZ_Gait::Walk;

	/** Ignore Gait; read bInvestigateUrgent from the Blackboard instead: true -> UrgentGait, false -> CalmGait. */
	UPROPERTY(EditAnywhere, Category = "AZ")
	bool bGaitFromInvestigateUrgency = false;

	UPROPERTY(EditAnywhere, Category = "AZ", meta = (EditCondition = "bGaitFromInvestigateUrgency"))
	EAZ_Gait CalmGait = EAZ_Gait::Walk;

	UPROPERTY(EditAnywhere, Category = "AZ", meta = (EditCondition = "bGaitFromInvestigateUrgency"))
	EAZ_Gait UrgentGait = EAZ_Gait::Run;
};
