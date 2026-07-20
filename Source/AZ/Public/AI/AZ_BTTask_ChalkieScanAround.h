// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "AZ_BTTask_ChalkieScanAround.generated.h"

/**
 * UAZ_BTTask_ChalkieScanAround — latent "look around" beat for search points (TLOU active search).
 *
 * Pulses the pawn's desired facing left / right / back-to-forward around the heading it arrived with, holding
 * each direction PulseHoldSeconds. The body turns via the walking mode's facing spring, so it reads as the
 * Chalkie scanning its surroundings; REAL turn anims come later by wiring the pack ABP's Turns SM to the
 * resulting yaw deltas (anim pass, this session's step 3).
 *
 * Facing goes through the controller's facing OVERRIDE (SetFacingOverrideWorld), not the pawn directly — the
 * controller Tick rewrites the pawn's desired facing every frame and would stomp a direct write. The override
 * is cleared on finish AND abort (perception preempting the scan must hand facing back to the chase logic).
 *
 * Instanced node (project pattern, like the target-selection service): per-run state lives in plain members
 * and is reset in ExecuteTask.
 */
UCLASS()
class AZ_API UAZ_BTTask_ChalkieScanAround : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UAZ_BTTask_ChalkieScanAround();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	/** How long each look direction is held. The facing spring needs most of this to actually get there. */
	UPROPERTY(EditAnywhere, Category = "AZ", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float PulseHoldSeconds = 0.7f;

	/** Yaw swept to each side from the arrival heading. 120 = looks well behind its shoulders. */
	UPROPERTY(EditAnywhere, Category = "AZ", meta = (ClampMin = "0", ClampMax = "180"))
	float ScanHalfAngleDegrees = 120.f;

	/** Per-run state (instanced node — reset in ExecuteTask). */
	FVector BaseFacing = FVector::ForwardVector;
	int32 StepIndex = 0;
	float StepElapsedSeconds = 0.f;
};
