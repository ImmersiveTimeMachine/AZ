// Copyright Artur. AZ project.

#include "AI/AZ_BTTask_FindPointNear.h"

#include "AI/AZ_InfectedAIController.h"   // AZ_ChalkieBBKeys::bInvestigateUrgent
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "NavigationSystem.h"

UAZ_BTTask_FindPointNear::UAZ_BTTask_FindPointNear()
{
	NodeName = TEXT("Find Point Near");
	CenterKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UAZ_BTTask_FindPointNear, CenterKey));
	OutKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UAZ_BTTask_FindPointNear, OutKey));
}

void UAZ_BTTask_FindPointNear::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
	if (UBlackboardData* BBAsset = GetBlackboardAsset())
	{
		CenterKey.ResolveSelectedKey(*BBAsset);
		OutKey.ResolveSelectedKey(*BBAsset);
	}
}

EBTNodeResult::Type UAZ_BTTask_FindPointNear::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB || !CenterKey.SelectedKeyName.IsValid() || !OutKey.SelectedKeyName.IsValid())
	{
		return EBTNodeResult::Failed;
	}
	// A failed attempt must not retain the previous search point. The Investigate
	// sequence cannot reach its final ClearBBKey task when this task returns Failed.
	BB->ClearValue(OutKey.SelectedKeyName);
	if (!BB->IsVectorValueSet(CenterKey.SelectedKeyName))
	{
		return EBTNodeResult::Failed;
	}
	const FVector Center = BB->GetValueAsVector(CenterKey.SelectedKeyName);

	float EffectiveRadius = Radius;
	if (bScaleRadiusWhenUrgent && BB->GetValueAsBool(AZ_ChalkieBBKeys::bInvestigateUrgent))
	{
		EffectiveRadius *= UrgentRadiusScale;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(OwnerComp.GetWorld());
	FNavLocation Result;
	if (!NavSys || !NavSys->GetRandomReachablePointInRadius(Center, EffectiveRadius, Result))
	{
		if (CenterKey.SelectedKeyName == AZ_ChalkieBBKeys::LastKnownLocation)
		{
			// The tree observes this key and falls through to Return Home once it is
			// empty. Keep permanent HomeLocation intact for the separate wander task.
			BB->SetValueAsBool(AZ_ChalkieBBKeys::bInvestigateUrgent, false);
			BB->ClearValue(AZ_ChalkieBBKeys::LastKnownLocation);
			UE_LOG(LogTemp, Display, TEXT("[NoiseSearch] %s cleared unreachable investigation center=%s nav=%d"),
				*GetNameSafe(OwnerComp.GetAIOwner()), *Center.ToCompactString(), NavSys != nullptr);
		}
		return EBTNodeResult::Failed;
	}

	BB->SetValueAsVector(OutKey.SelectedKeyName, Result.Location);
	return EBTNodeResult::Succeeded;
}

FString UAZ_BTTask_FindPointNear::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s = reachable point within %.0f of %s"),
		*OutKey.SelectedKeyName.ToString(), Radius, *CenterKey.SelectedKeyName.ToString());
}
