// Copyright Artur. AZ project.

#include "AI/AZ_InfectedAIController.h"

#include "Animation/AZ_LocomotionTypes.h"   // EAZ_Gait
#include "Character/AZ_PawnMoverInfectedCharacter.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

AAZ_InfectedAIController::AAZ_InfectedAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AAZ_InfectedAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	InfectedPawn = Cast<AAZ_PawnMoverInfectedCharacter>(InPawn);
}

void AAZ_InfectedAIController::OnUnPossess()
{
	if (AAZ_PawnMoverInfectedCharacter* P = InfectedPawn.Get())
	{
		// Don't leave a stale intent latched on the pawn after we let go of it.
		P->SetMoveIntentWorld(FVector::ZeroVector);
	}
	InfectedPawn = nullptr;
	Super::OnUnPossess();
}

void AAZ_InfectedAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bDebugHomeToPlayer)
	{
		return;
	}

	AAZ_PawnMoverInfectedCharacter* P = InfectedPawn.Get();
	if (!P)
	{
		return;
	}

	// TEMP scaffolding: chase the local player pawn. GetPlayerPawn(0) is acceptable for a single-player foundation
	// demo only; the real version reads this controller's own AIPerception target (co-op-safe, per-controller).
	const APawn* Target = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Target)
	{
		P->SetMoveIntentWorld(FVector::ZeroVector);
		return;
	}

	FVector ToTarget = Target->GetActorLocation() - P->GetActorLocation();
	ToTarget.Z = 0.f;
	const float Distance = ToTarget.Size();
	const FVector Dir = ToTarget.GetSafeNormal();

	if (Dir.IsNearlyZero())
	{
		P->SetMoveIntentWorld(FVector::ZeroVector);
		return;
	}

	// Always face the target; move toward it until inside StopDistance.
	P->SetDesiredFacingWorld(Dir);
	if (Distance > StopDistance)
	{
		P->SetMoveIntentWorld(Dir);
		P->SetGait(EAZ_Gait::Run);   // a chasing Chalkie runs (Phase-2 aggressive)
	}
	else
	{
		P->SetMoveIntentWorld(FVector::ZeroVector);   // arrived: hold position, keep facing the target
	}

	// Keep the controller's control rotation aligned with the facing so the AnimInstance's AimingRotation
	// (read from the controller) yields ~zero rotation offset (body looks where it faces).
	SetControlRotation(Dir.Rotation());
}
