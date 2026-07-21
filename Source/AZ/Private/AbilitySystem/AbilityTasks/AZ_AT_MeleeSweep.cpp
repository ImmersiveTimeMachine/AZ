// Copyright Artur. AZ project.

#include "AbilitySystem/AbilityTasks/AZ_AT_MeleeSweep.h"

#include "Engine/World.h"
#include "GenericTeamAgentInterface.h"

UAZ_AT_MeleeSweep* UAZ_AT_MeleeSweep::MeleeSweep(UGameplayAbility* OwningAbility, float SweepRange,
	float SweepRadius, float ConeHalfAngleDegrees, bool bHostilesOnly, bool bSingleTarget)
{
	UAZ_AT_MeleeSweep* Task = NewAbilityTask<UAZ_AT_MeleeSweep>(OwningAbility);
	Task->SweepRange = SweepRange;
	Task->SweepRadius = SweepRadius;
	Task->ConeHalfAngleDegrees = ConeHalfAngleDegrees;
	Task->bHostilesOnly = bHostilesOnly;
	Task->bSingleTarget = bSingleTarget;
	return Task;
}

void UAZ_AT_MeleeSweep::Activate()
{
	Super::Activate();

	TArray<FHitResult> Filtered;
	AActor* Avatar = GetAvatarActor();
	if (Avatar)
	{
		// Precision form proven in GA_MeleeAttack: sweep starts a body-width ahead (a center-origin
		// sphere hits anything touching the attacker regardless of facing) + per-hit angular cone.
		const FVector Forward = Avatar->GetActorForwardVector().GetSafeNormal2D();
		const FVector Start = Avatar->GetActorLocation() + Forward * (SweepRadius * 0.8f);
		const FVector End = Start + Forward * SweepRange;
		const float MinFacingDot = FMath::Cos(FMath::DegreesToRadians(ConeHalfAngleDegrees));

		TArray<FHitResult> Hits;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AZMeleeSweepTask), /*bTraceComplex*/ false, Avatar);
		Avatar->GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity,
			FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(SweepRadius), Params);

		TArray<AActor*> AlreadyHit;
		for (const FHitResult& Hit : Hits)
		{
			AActor* Target = Hit.GetActor();
			if (!Target || Target == Avatar || AlreadyHit.Contains(Target))
			{
				continue;
			}
			AlreadyHit.Add(Target);

			const FVector DirToHit = (Target->GetActorLocation() - Avatar->GetActorLocation()).GetSafeNormal2D();
			if (FVector::DotProduct(Forward, DirToHit) < MinFacingDot)
			{
				continue;
			}
			if (bHostilesOnly && FGenericTeamId::GetAttitude(Avatar, Target) != ETeamAttitude::Hostile)
			{
				continue;
			}
			Filtered.Add(Hit);
			if (bSingleTarget)
			{
				break;
			}
		}
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnCompleted.Broadcast(Filtered);
	}
	EndTask();
}
