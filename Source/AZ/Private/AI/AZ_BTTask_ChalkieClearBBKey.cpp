// Copyright Artur. AZ project.

#include "AI/AZ_BTTask_ChalkieClearBBKey.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"

UAZ_BTTask_ChalkieClearBBKey::UAZ_BTTask_ChalkieClearBBKey()
{
	NodeName = TEXT("Chalkie Clear BB Key");
}

void UAZ_BTTask_ChalkieClearBBKey::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		Key.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UAZ_BTTask_ChalkieClearBBKey::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
	{
		BB->ClearValue(Key.SelectedKeyName);
		return EBTNodeResult::Succeeded;
	}
	return EBTNodeResult::Failed;
}
