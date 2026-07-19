// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "AZ_BTService_ChalkieTargetSelection.generated.h"

/**
 * UAZ_BTService_ChalkieTargetSelection — the perception->Blackboard bridge (attach to the BT ROOT selector).
 *
 * Every tick interval it refreshes the controller's perception poll and mirrors the result:
 *  - TargetActor        <- the fresh perceived hostile (cleared when lost past the grace window).
 *  - LastKnownLocation  <- written ONCE on the fresh->lost EDGE (arms the Investigate branch; the branch
 *                          consumes it and clears the key when the search is done — the service will not
 *                          re-write it until the target is seen and lost again).
 *
 * All perception logic (sight cone, crouch-sneak range, hostility, grace) lives on AAZ_InfectedAIController —
 * this service only MIRRORS controller state into the Blackboard, so the temp Tick brain and the BT read the
 * exact same truth. Instanced per-AI (bCreateNodeInstance) so the edge-tracking bool is per-Chalkie.
 */
UCLASS()
class AZ_API UAZ_BTService_ChalkieTargetSelection : public UBTService
{
	GENERATED_BODY()

public:
	UAZ_BTService_ChalkieTargetSelection();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	/** Edge tracker: had a fresh target last tick (per-AI — node is instanced). */
	bool bHadFreshTarget = false;
};
