// Copyright Artur. AZ project.

#include "AI/AZ_BTTask_ChalkieSetGait.h"

#include "AI/AZ_InfectedAIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
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

	EAZ_Gait GaitToSet = Gait;
	if (bGaitFromInvestigateUrgency)
	{
		const UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
		const bool bUrgent = BB && BB->GetValueAsBool(AZ_ChalkieBBKeys::bInvestigateUrgent);
		GaitToSet = bUrgent ? UrgentGait : CalmGait;
	}

	InfectedPawn->SetGait(GaitToSet);
	return EBTNodeResult::Succeeded;
}

FString UAZ_BTTask_ChalkieSetGait::GetStaticDescription() const
{
	if (bGaitFromInvestigateUrgency)
	{
		return TEXT("gait from bInvestigateUrgent (calm/urgent)");
	}
	return FString::Printf(TEXT("gait = %s"), *UEnum::GetDisplayValueAsText(Gait).ToString());
}
