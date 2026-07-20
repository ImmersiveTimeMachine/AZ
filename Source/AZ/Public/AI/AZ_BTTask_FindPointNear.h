// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "AZ_BTTask_FindPointNear.generated.h"

/**
 * UAZ_BTTask_FindPointNear — pick a random REACHABLE nav point near a center key, write it to an out key.
 *
 * The search/wander workhorse (instant task):
 *  - Investigate: FindPointNear(LastKnownLocation, ~400, SearchLocation) -> MoveTo(SearchLocation) — the
 *    Chalkie checks spots AROUND where it lost you instead of statue-standing on the exact point.
 *  - Home wander: FindPointNear(HomeLocation, ~400, SearchLocation) -> MoveTo — patrol-lite idle drift.
 *
 * Reachability comes from UNavigationSystemV1::GetRandomReachablePointInRadius (pathfinding-verified, not
 * just projected), so the point is never through a wall the Chalkie can't round. When the Blackboard's
 * bInvestigateUrgent is set (escalated / lost-a-chase searches) the radius widens — it is "onto you" and
 * casts a wider net. Fails when the center key is unset or no reachable point exists (Selector falls through).
 */
UCLASS()
class AZ_API UAZ_BTTask_FindPointNear : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UAZ_BTTask_FindPointNear();

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	/** Vector key to search around (LastKnownLocation for searches, HomeLocation for wander). */
	UPROPERTY(EditAnywhere, Category = "AZ")
	FBlackboardKeySelector CenterKey;

	/** Vector key that receives the picked point (SearchLocation). */
	UPROPERTY(EditAnywhere, Category = "AZ")
	FBlackboardKeySelector OutKey;

	/** Search radius around the center. */
	UPROPERTY(EditAnywhere, Category = "AZ", meta = (ClampMin = "0", ForceUnits = "cm"))
	float Radius = 400.f;

	/** Multiply Radius by UrgentRadiusScale while the Blackboard's bInvestigateUrgent is true. */
	UPROPERTY(EditAnywhere, Category = "AZ")
	bool bScaleRadiusWhenUrgent = true;

	UPROPERTY(EditAnywhere, Category = "AZ", meta = (ClampMin = "1", EditCondition = "bScaleRadiusWhenUrgent"))
	float UrgentRadiusScale = 1.75f;
};
