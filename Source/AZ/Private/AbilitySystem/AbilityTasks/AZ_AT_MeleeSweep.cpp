// Copyright Artur. AZ project.

#include "AbilitySystem/AbilityTasks/AZ_AT_MeleeSweep.h"

#include "AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GenericTeamAgentInterface.h"

UAZ_AT_MeleeSweep::UAZ_AT_MeleeSweep(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

UAZ_AT_MeleeSweep* UAZ_AT_MeleeSweep::MeleeSweepWindow(UGameplayAbility* OwningAbility, FName InSocketName,
	float InSphereRadius, bool bInHostilesOnly, bool bInSingleTarget)
{
	UAZ_AT_MeleeSweep* Task = NewAbilityTask<UAZ_AT_MeleeSweep>(OwningAbility);
	Task->SocketName = InSocketName;
	Task->SphereRadius = InSphereRadius;
	Task->bHostilesOnly = bInHostilesOnly;
	Task->bSingleTarget = bInSingleTarget;
	return Task;
}

void UAZ_AT_MeleeSweep::Activate()
{
	Super::Activate();

	if (const AActor* Avatar = GetAvatarActor())
	{
		Mesh = Avatar->FindComponentByClass<USkeletalMeshComponent>();
	}
	// First tick establishes PrevLocation; detection starts on the second — no zero-length sweep.
	bHasPrevious = false;
}

void UAZ_AT_MeleeSweep::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	USkeletalMeshComponent* MeshComp = Mesh.Get();
	AActor* Avatar = GetAvatarActor();
	if (!MeshComp || !Avatar || bConsumed)
	{
		return;
	}

	const FVector Current = MeshComp->GetSocketLocation(SocketName);
	if (!bHasPrevious)
	{
		PrevLocation = Current;
		bHasPrevious = true;
		return;
	}

	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AZMeleeSocketSweep), /*bTraceComplex*/ false, Avatar);
	Avatar->GetWorld()->SweepMultiByObjectType(Hits, PrevLocation, Current, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(SphereRadius), Params);
	PrevLocation = Current;

	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!Target || Target == Avatar || AlreadyHit.Contains(Target))
		{
			continue;
		}
		AlreadyHit.Add(Target);   // once per target per swing, even if this hit is filtered below

		if (bHostilesOnly && FGenericTeamId::GetAttitude(Avatar, Target) != ETeamAttitude::Hostile)
		{
			continue;
		}
		// CORPSES DON'T EAT PUNCHES (audit rules-finding #2): permanent corpses keep hostile team + live
		// ASC. Filtered HERE, before the single-target consume, so a body on the floor can't spend the
		// hit meant for the live attacker whose capsule the fist touches next frame.
		if (const UAbilitySystemComponent* TargetASC =
				UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target))
		{
			bool bHasVitals = false;
			const float Health = TargetASC->GetGameplayAttributeValue(
				UAZ_VitalsAttributeSet::GetHealthAttribute(), bHasVitals);
			if (bHasVitals && Health <= 0.f)
			{
				continue;
			}
		}
		else
		{
			continue;   // no ASC = nothing to damage
		}

		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnHit.Broadcast(Hit);
		}
		if (bSingleTarget)
		{
			bConsumed = true;   // a punch is not a cleave
			return;
		}
	}
}
