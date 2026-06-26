// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AZ_InfectedAIController.generated.h"

class AAZ_PawnMoverInfectedCharacter;

/**
 * AAZ_InfectedAIController — controller for the Chalkie infected pawn.
 *
 * The AI side of the v2 universal-input design: it WRITES the pawn's intent surface
 * (SetMoveIntentWorld / SetDesiredFacingWorld / SetGait) on the server, and the pawn's ProduceInput turns that
 * into the deterministic Mover InputCmd — exactly where the player's Enhanced Input would feed the hero.
 *
 * FOUNDATION SCOPE: a temporary "home toward the player" drive (bDebugHomeToPlayer) so the Mover + animation
 * stack is end-to-end verifiable in PIE. This is SCAFFOLDING — the next step replaces it with AIPerception
 * (sight/hearing), a BehaviorTree, and NavMesh path-following (the engine's UNavMoverComponent bridges
 * RequestDirectMove into the same intent surface).
 */
UCLASS()
class AZ_API AAZ_InfectedAIController : public AAIController
{
	GENERATED_BODY()

public:
	AAZ_InfectedAIController();

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaTime) override;

protected:
	// ---- TEMPORARY foundation demo (replaced by perception + BehaviorTree + NavMesh next step) ----

	/** When true, steer straight toward the local player pawn each tick (ignores navmesh/obstacles — Mover's own
	 *  collision + the clearance clamp keep it from passing through walls; real pathfinding comes next). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|AI|Debug")
	bool bDebugHomeToPlayer = true;

	/** Stop (and just face the target) once within this distance — avoids jittering on top of the player. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|AI|Debug", meta = (ClampMin = "0", ForceUnits = "cm"))
	float StopDistance = 150.f;

	TWeakObjectPtr<AAZ_PawnMoverInfectedCharacter> InfectedPawn;
};
