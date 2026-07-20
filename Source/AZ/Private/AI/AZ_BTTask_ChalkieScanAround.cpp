// Copyright Artur. AZ project.

#include "AI/AZ_BTTask_ChalkieScanAround.h"

#include "AI/AZ_InfectedAIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Character/AZ_PawnMoverInfectedCharacter.h"

UAZ_BTTask_ChalkieScanAround::UAZ_BTTask_ChalkieScanAround()
{
	NodeName = TEXT("Chalkie Scan Around");
	bNotifyTick = true;
	// Per-AI instance so the pulse state below is plain members (same pattern as the target-selection service).
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UAZ_BTTask_ChalkieScanAround::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAZ_InfectedAIController* Chalkie = Cast<AAZ_InfectedAIController>(OwnerComp.GetAIOwner());
	const AAZ_PawnMoverInfectedCharacter* InfectedPawn = Chalkie ? Chalkie->GetInfectedPawn() : nullptr;
	if (!InfectedPawn)
	{
		return EBTNodeResult::Failed;
	}

	BaseFacing = InfectedPawn->GetActorForwardVector();
	BaseFacing.Z = 0.f;
	BaseFacing = BaseFacing.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	StepIndex = 0;
	StepElapsedSeconds = 0.f;

	// Pulse sequence: +side, -side, back to the arrival heading. First pulse starts immediately.
	Chalkie->SetFacingOverrideWorld(BaseFacing.RotateAngleAxis(ScanHalfAngleDegrees, FVector::UpVector));
	return EBTNodeResult::InProgress;
}

void UAZ_BTTask_ChalkieScanAround::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAZ_InfectedAIController* Chalkie = Cast<AAZ_InfectedAIController>(OwnerComp.GetAIOwner());
	if (!Chalkie)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	StepElapsedSeconds += DeltaSeconds;
	if (StepElapsedSeconds < PulseHoldSeconds)
	{
		return;
	}
	StepElapsedSeconds = 0.f;
	++StepIndex;

	switch (StepIndex)
	{
	case 1:
		Chalkie->SetFacingOverrideWorld(BaseFacing.RotateAngleAxis(-ScanHalfAngleDegrees, FVector::UpVector));
		break;
	case 2:
		Chalkie->SetFacingOverrideWorld(BaseFacing);
		break;
	default:
		// Swept both sides and settled forward — hand facing back to the controller.
		Chalkie->ClearFacingOverride();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		break;
	}
}

EBTNodeResult::Type UAZ_BTTask_ChalkieScanAround::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// Preempted (spotted a target / new noise) mid-scan: the chase/alert logic must own facing again.
	if (AAZ_InfectedAIController* Chalkie = Cast<AAZ_InfectedAIController>(OwnerComp.GetAIOwner()))
	{
		Chalkie->ClearFacingOverride();
	}
	return EBTNodeResult::Aborted;
}

FString UAZ_BTTask_ChalkieScanAround::GetStaticDescription() const
{
	return FString::Printf(TEXT("look +/-%.0f deg, %.1fs per pulse"),
		ScanHalfAngleDegrees, PulseHoldSeconds);
}
