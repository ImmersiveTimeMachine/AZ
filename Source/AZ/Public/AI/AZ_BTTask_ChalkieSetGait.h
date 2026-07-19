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
 */
UCLASS()
class AZ_API UAZ_BTTask_ChalkieSetGait : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UAZ_BTTask_ChalkieSetGait();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, Category = "AZ")
	EAZ_Gait Gait = EAZ_Gait::Walk;
};
