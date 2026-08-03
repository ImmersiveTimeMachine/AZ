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

UAZ_AT_MeleeSweep* UAZ_AT_MeleeSweep::MeleeSweepWindow(UGameplayAbility* OwningAbility, const TArray<FName>& InSocketNames,
	float InSphereRadius, bool bInHostilesOnly, bool bInSingleTarget)
{
	UAZ_AT_MeleeSweep* Task = NewAbilityTask<UAZ_AT_MeleeSweep>(OwningAbility);
	// De-duplicate and drop NAME_None here rather than per tick: a socket the skeleton doesn't have
	// resolves to the COMPONENT's own location (GetSocketLocation's silent fallback), which would trace a
	// sphere out of the pawn's centre and "hit" whatever it is standing next to.
	for (const FName& Socket : InSocketNames)
	{
		if (!Socket.IsNone())
		{
			Task->SocketNames.AddUnique(Socket);
		}
	}
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
	USkeletalMeshComponent* MeshComp = Mesh.Get();
	if (!MeshComp)
	{
		EndTask();
		return;
	}
	// Drop sockets this skeleton doesn't have. GetSocketLocation falls back to the COMPONENT's own
	// transform for an unknown name — silently tracing a sphere out of the pawn's centre, which both
	// misses the fist and can "hit" whatever the pawn is standing next to.
	for (int32 i = SocketNames.Num() - 1; i >= 0; --i)
	{
		if (!MeshComp->DoesSocketExist(SocketNames[i]))
		{
			UE_LOG(LogTemp, Warning, TEXT("[MeleeSweep] '%s' has no socket/bone '%s' — not traced."),
				*GetNameSafe(MeshComp->GetSkeletalMeshAsset()), *SocketNames[i].ToString());
			SocketNames.RemoveAt(i);
		}
	}
	if (SocketNames.IsEmpty())
	{
		EndTask();   // nothing to trace — don't tick a detector that can never detect
		return;
	}

	// Seed the previous positions HERE, not on the first tick. Skipping a tick to establish them costs
	// the whole first frame of the window, and a window shorter than two ticks (a 0.38s jab at 15 FPS,
	// or any window whose begin and end land inside one frame) would detect nothing at all.
	PrevLocations.SetNum(SocketNames.Num());
	for (int32 i = 0; i < SocketNames.Num(); ++i)
	{
		PrevLocations[i] = MeshComp->GetSocketLocation(SocketNames[i]);
	}
	bHasPrevious = true;
}

void UAZ_AT_MeleeSweep::OnDestroy(bool bInOwnerFinished)
{
	// Close the trailing gap: WindowEnd (or the ability ending) lands between ticks, so the last stretch
	// of the fist's path would otherwise never be swept.
	SweepSinceLastFrame();
	Super::OnDestroy(bInOwnerFinished);
}

void UAZ_AT_MeleeSweep::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);
	SweepSinceLastFrame();
}

void UAZ_AT_MeleeSweep::SweepSinceLastFrame()
{
	USkeletalMeshComponent* MeshComp = Mesh.Get();
	AActor* Avatar = GetAvatarActor();
	if (!MeshComp || !Avatar || !bHasPrevious || bConsumed || !Avatar->GetWorld())
	{
		return;
	}

	// Gather EVERY socket's hits for this frame before judging any of them. Reporting the first hit the
	// loop happens to reach would make array order decide which fist landed — left always winning a
	// two-fisted frame purely for being listed first. Both segments span the same frame, so FHitResult
	// Time (fraction along its own segment) is comparable between them: the smallest is the contact that
	// physically happened first.
	TArray<FHitResult> Candidates;
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AZMeleeSocketSweep), /*bTraceComplex*/ false, Avatar);

	for (int32 i = 0; i < SocketNames.Num(); ++i)
	{
		const FVector Current = MeshComp->GetSocketLocation(SocketNames[i]);
		const FVector Previous = PrevLocations[i];
		PrevLocations[i] = Current;   // advance before any early-out, or the next segment would be a gap

		Hits.Reset();
		Avatar->GetWorld()->SweepMultiByObjectType(Hits, Previous, Current, FQuat::Identity,
			FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(SphereRadius), Params);
		Candidates.Append(Hits);
	}

	Candidates.Sort([](const FHitResult& A, const FHitResult& B) { return A.Time < B.Time; });

	for (const FHitResult& Hit : Candidates)
	{
		AActor* Target = Hit.GetActor();
		if (!Target || Target == Avatar || AlreadyHit.Contains(Target))
		{
			continue;
		}
		if (bHostilesOnly && FGenericTeamId::GetAttitude(Avatar, Target) != ETeamAttitude::Hostile)
		{
			continue;
		}
		// CORPSES DON'T EAT PUNCHES (audit rules-finding #2): permanent corpses keep hostile team + live
		// ASC. Filtered before the single-target consume, so a body on the floor can't spend the hit
		// meant for the live attacker whose capsule the fist touches next frame.
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

		// Marked only once it is a REAL hit. Marking rejected actors would make a pawn that was merely
		// neutral or unresolved for one frame permanently immune for the rest of the swing.
		AlreadyHit.Add(Target);

		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnHit.Broadcast(Hit);
		}
		if (bSingleTarget)
		{
			bConsumed = true;   // a punch is not a cleave — and not one hit per fist either
			return;
		}
	}
}
