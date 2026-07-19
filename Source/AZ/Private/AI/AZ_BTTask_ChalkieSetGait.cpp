// Copyright Artur. AZ project.

#include "AI/AZ_BTTask_ChalkieSetGait.h"

#include "AI/AZ_InfectedAIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Character/AZ_PawnMoverInfectedCharacter.h"

UAZ_BTTask_ChalkieSetGait::UAZ_BTTask_ChalkieSetGait()
{
	NodeName = TEXT("Chalkie Set Gait");
}

EBTNodeResult::Type UAZ_BTTask_ChalkieSetGait::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	const AAZ_InfectedAIController* Chalkie = Cast<AAZ_InfectedAIController>(OwnerComp.GetAIOwner());
	AAZ_PawnMoverInfectedCharacter* InfectedPawn = Chalkie ? Chalkie->GetInfectedPawn() : nullptr;
	if (!InfectedPawn)
	{
		return EBTNodeResult::Failed;
	}

	InfectedPawn->SetGait(Gait);
	return EBTNodeResult::Succeeded;
}
