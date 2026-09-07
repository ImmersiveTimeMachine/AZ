// Copyright Artur. AZ project.

#include "AbilitySystem/AbilityTasks/AZ_AT_MeleeSweep.h"

#include "AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h"
#include "AbilitySystem/AZ_MeleeEnvironment.h"
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
	Task->SphereRadius = FMath::Max(0.1f, InSphereRadius);
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
		PreviousAvatarLocation = Avatar->GetActorLocation();
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
	// Explicit WindowEnd closes the trailing gap. Once the owning ability is finishing, destruction
	// must not land a new hit; callers can also discard before explicitly ending this task.
	if (!bInOwnerFinished) SweepSinceLastFrame();
	bConsumed = true;
	OnBlocked.Unbind();
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

	// Pawn and scenery contacts share ONE ordering across all sockets. A later wall contact must not
	// erase an earlier victim hit, and an earlier wall must not let a pawn-only trace hit through it.
	struct FContact
	{
		FHitResult Hit;
		bool bEnvironment = false;
	};
	TArray<FContact> Candidates;
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AZMeleeSocketSweep), /*bTraceComplex*/ false, Avatar);
	TArray<AActor*> AttachedActors;
	Avatar->GetAttachedActors(AttachedActors, true, true);
	Params.AddIgnoredActors(AttachedActors);
	const FVector CurrentAvatarLocation = Avatar->GetActorLocation();

	for (int32 i = 0; i < SocketNames.Num(); ++i)
	{
		const FVector Current = MeshComp->GetSocketLocation(SocketNames[i]);
		const FVector Previous = PrevLocations[i];
		PrevLocations[i] = Current;   // advance before any early-out, or the next segment would be a gap

		Hits.Reset();
		Avatar->GetWorld()->SweepMultiByObjectType(Hits, Previous, Current, FQuat::Identity,
			FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(SphereRadius), Params);
		for (const FHitResult& Hit : Hits)
		{
			Candidates.Add({ Hit, false });
		}

		FHitResult Obstruction;
		// Foot/ball sockets are also used by kicks. A planted foot touching its supporting floor is
		// not a blocked attack; upright scenery still participates in the same temporal ordering.
		if (FAZ_MeleeEnvironment::SweepEnvironment(*Avatar, Previous, Current, SphereRadius, Obstruction, true))
		{
			Candidates.Add({ Obstruction, true });
		}
		if (bCheckInitialObstruction)
		{
			FVector Body = PreviousAvatarLocation;
			Body.Z = Previous.Z;
			if (FAZ_MeleeEnvironment::SweepEnvironment(*Avatar, Body, Previous, 2.f, Obstruction, true))
			{
				// This limb was already beyond scenery at the window's first sample. Its body-to-limb
				// trace fraction is spatial, not temporal: the obstruction predates all swept contacts.
				Obstruction.Time = 0.f;
				Candidates.Add({ Obstruction, true });
			}
		}
	}
	bCheckInitialObstruction = false;
	const FVector FrameStartAvatarLocation = PreviousAvatarLocation;
	PreviousAvatarLocation = CurrentAvatarLocation;

	Candidates.Sort([](const FContact& A, const FContact& B)
	{
		return A.Hit.Time == B.Hit.Time ? A.bEnvironment && !B.bEnvironment : A.Hit.Time < B.Hit.Time;
	});

	for (const FContact& Contact : Candidates)
	{
		const FHitResult& Hit = Contact.Hit;
		if (Contact.bEnvironment)
		{
			bConsumed = true; // before the callback, which can synchronously destroy this task
			if (ShouldBroadcastAbilityTaskDelegates()) OnBlocked.ExecuteIfBound(Hit);
			return;
		}
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

		// A moving body or very thin obstruction can leave the current hand on the far side without a
		// new hand sweep intersection. Reject that unreachable victim as well. Interpolate the body to
		// this candidate's contact time rather than testing it from the end-of-frame actor position.
		FVector Body = FMath::Lerp(FrameStartAvatarLocation, CurrentAvatarLocation, Hit.Time);
		Body.Z = Hit.ImpactPoint.Z;
		FHitResult Obstruction;
		if (FAZ_MeleeEnvironment::SweepEnvironment(*Avatar, Body, Hit.ImpactPoint, 2.f, Obstruction, true, Target))
		{
			continue;
		}

		// Marked only once it is a REAL hit. Marking rejected actors would make a pawn that was merely
		// neutral or unresolved for one frame permanently immune for the rest of the swing.
		AlreadyHit.Add(Target);
		if (bSingleTarget) bConsumed = true;

		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnHit.Broadcast(Hit);
		}
		if (bConsumed || !ShouldBroadcastAbilityTaskDelegates())
		{
			return;
		}
	}
}
