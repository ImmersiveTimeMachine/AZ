// Copyright Artur. AZ project.

#include "AI/AZ_BTService_ChalkieTargetSelection.h"

#include "AI/AZ_InfectedAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"

UAZ_BTService_ChalkieTargetSelection::UAZ_BTService_ChalkieTargetSelection()
{
	NodeName = TEXT("Chalkie Target Selection");
	// Per-AI instance: the fresh->lost edge tracker below is a plain member, not node-memory.
	bCreateNodeInstance = true;
	// Perception mirroring wants low latency — 0.5 s default lag on aggro is visible.
	Interval = 0.1f;
	RandomDeviation = 0.02f;
}

void UAZ_BTService_ChalkieTargetSelection::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAZ_InfectedAIController* Chalkie = Cast<AAZ_InfectedAIController>(OwnerComp.GetAIOwner());
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!Chalkie || !BB)
	{
		return;
	}

	// Refresh the poll (frame-guarded on the controller — cheap if its Tick already ran this frame).
	Chalkie->UpdatePerception();

	APawn* Fresh = Chalkie->GetFreshPerceivedTarget();
	const bool bFresh = (Fresh != nullptr);

	if (bFresh)
	{
		BB->SetValueAsObject(AZ_ChalkieBBKeys::TargetActor, Fresh);
	}
	else
	{
		BB->ClearValue(AZ_ChalkieBBKeys::TargetActor);
		if (bHadFreshTarget)
		{
			// The fresh->lost EDGE: arm the Investigate branch with where we last knew them to be.
			// URGENT — it was mid-chase, it KNOWS someone is here: search at Run gait, wider radius.
			Chalkie->ArmInvestigation(Chalkie->GetLastKnownTargetLocation(), /*bUrgent*/ true);
		}
	}
	bHadFreshTarget = bFresh;
}
