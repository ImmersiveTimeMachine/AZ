// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"   // FBlackboardKeySelector
#include "AZ_BTTask_ChalkieClearBBKey.generated.h"

/**
 * UAZ_BTTask_ChalkieClearBBKey — instant task: clear ONE Blackboard key.
 * Used at the end of the Investigate branch to consume LastKnownLocation (the "searched, found nothing,
 * giving up" moment) so the branch's IsSet decorator releases and the tree falls through to Patrol.
 */
UCLASS()
class AZ_API UAZ_BTTask_ChalkieClearBBKey : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UAZ_BTTask_ChalkieClearBBKey();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

protected:
	UPROPERTY(EditAnywhere, Category = "AZ")
	FBlackboardKeySelector Key;
};
