// Copyright Artur. AZ project.

#include "Character/AZ_TraversalComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "Animation/AnimMontage.h"
#include "Animation/AZ_MoverAnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "MoverComponent.h"

UAZ_TraversalComponent::UAZ_TraversalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Seeded from the MEASURED clips, not from the "_1_0" in their names. Each family's authored approach
	// distance (start of root track to its own warp anchor) is roughly: stand 51-67cm, walk 200-266cm,
	// run 318-341cm. Reach and accepted distance follow those, with margin; warping absorbs the rest.
	ApproachBands.Add({ EAZ_MantleApproach::Stand,   0.f,  60.f, 120.f, 110.f });
	ApproachBands.Add({ EAZ_MantleApproach::Walk,   60.f, 250.f, 300.f, 290.f });
	ApproachBands.Add({ EAZ_MantleApproach::Run,   250.f, 2000.f, 440.f, 430.f });
}

UAnimMontage* UAZ_TraversalComponent::ResolveMontage(EAZ_MantleApproach InApproach, EAZ_MantleStyle InStyle, EAZ_MantleFoot InFoot) const
{
	for (const FAZ_MantleClipEntry& Entry : Clips)
	{
		if (Entry.Approach == InApproach && Entry.Style == InStyle && Entry.PlantedFoot == InFoot)
		{
			return Entry.Montage;
		}
	}
	return nullptr;   // unassigned combination fails the candidate rather than substituting another family
}

FAZ_MantleApproachBand UAZ_TraversalComponent::ResolveApproachBand(float Speed) const
{
	for (const FAZ_MantleApproachBand& Band : ApproachBands)
	{
		if (Speed >= Band.MinSpeed && Speed < Band.MaxSpeed)
		{
			return Band;
		}
	}
	return FAZ_MantleApproachBand();   // standing defaults
}

EAZ_MantleFoot UAZ_TraversalComponent::ResolveLivePlantedFoot() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FallbackPlantedFoot;
	}

	TArray<USkeletalMeshComponent*> Meshes;
	Owner->GetComponents<USkeletalMeshComponent>(Meshes);
	for (const USkeletalMeshComponent* Mesh : Meshes)
	{
		if (const UAZ_MoverAnimInstance* AnimInst = Mesh ? Cast<UAZ_MoverAnimInstance>(Mesh->GetAnimInstance()) : nullptr)
		{
			return AnimInst->IsLeftFootDown() ? EAZ_MantleFoot::Left : EAZ_MantleFoot::Right;
		}
	}
	return FallbackPlantedFoot;
}

bool UAZ_TraversalComponent::SelectLedgeFromSplines(const AActor* HitActor, const FVector& PawnLocation,
	const FVector& Forward, float FeetZ, float MaxDistance, FVector& OutLip, FVector& OutNormal) const
{
	TArray<USplineComponent*> Splines;
	HitActor->GetComponents<USplineComponent>(Splines);
	if (Splines.Num() == 0)
	{
		return false;   // not an authored traversable — the generic-geometry provider goes here later
	}

	const float CosTolerance = FMath::Cos(FMath::DegreesToRadians(FacingToleranceDeg));
	float BestPlanarDistSq = TNumericLimits<float>::Max();
	bool bFound = false;

	for (const USplineComponent* Spline : Splines)
	{
		if (!Spline || Spline->GetNumberOfSplinePoints() < 2)
		{
			continue;
		}

		const float Key = Spline->FindInputKeyClosestToWorldLocation(PawnLocation);
		const FVector Lip = Spline->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World);

		// The authored ledge encodes its OUTWARD direction in the spline's UP vector (authored via roll) —
		// not in world Z, and not in its tangent, which runs ALONG the edge. Each block carries one spline
		// per side, so picking the spline is picking which face we approach.
		FVector Normal = Spline->GetUpVectorAtSplineInputKey(Key, ESplineCoordinateSpace::World);
		Normal.Z = 0.f;
		if (!Normal.Normalize())
		{
			continue;
		}

		const float Height = Lip.Z - FeetZ;
		if (Height < MinLedgeHeight || Height > MaxLedgeHeight)
		{
			continue;
		}

		FVector ToLip = Lip - PawnLocation;
		ToLip.Z = 0.f;
		const float PlanarDistSq = ToLip.SizeSquared();
		if (PlanarDistSq > FMath::Square(MaxDistance) || PlanarDistSq <= KINDA_SMALL_NUMBER)
		{
			continue;
		}
		if (FVector::DotProduct(ToLip.GetSafeNormal(), Forward) <= 0.f)
		{
			continue;   // that edge is behind us (the block's far side)
		}
		if (FVector::DotProduct(Forward, -Normal) < CosTolerance)
		{
			continue;   // we are not facing into this face
		}

		if (PlanarDistSq < BestPlanarDistSq)
		{
			BestPlanarDistSq = PlanarDistSq;
			OutLip = Lip;
			OutNormal = Normal;
			bFound = true;
		}
	}

	return bFound;
}

bool UAZ_TraversalComponent::FindMantleCandidate(FAZ_MantleCandidate& OutCandidate) const
{
	OutCandidate = FAZ_MantleCandidate();

	const APawn* Pawn = Cast<APawn>(GetOwner());
	const UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		return false;
	}

	const UCapsuleComponent* Capsule = Pawn->FindComponentByClass<UCapsuleComponent>();
	if (!Capsule)
	{
		return false;
	}

	const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector PawnLocation = Pawn->GetActorLocation();
	const float FeetZ = PawnLocation.Z - CapsuleHalfHeight;

	FVector Forward = Pawn->GetActorForwardVector();
	Forward.Z = 0.f;
	if (!Forward.Normalize())
	{
		return false;
	}

	// Approach is decided by SPEED at the press, and it changes how far we are even willing to look: the
	// run clips are authored from ~330cm out, so the standing band's 110cm would reject every moving
	// mantle before any geometry was considered. Velocity (what the body is actually doing), not intent —
	// intent is clamped to zero against a wall, which is exactly where a mantle is wanted.
	// The Mover component's velocity, which is what the anim spine reads for Speed2D — APawn::GetVelocity
	// is not the same number on a Mover pawn, and a mismatch here would pick a different clip family than
	// the gait the character is visibly in.
	const UMoverComponent* MoverComp = Pawn->FindComponentByClass<UMoverComponent>();
	const float ApproachSpeed = MoverComp ? static_cast<float>(MoverComp->GetVelocity().Size2D()) : 0.f;
	const FAZ_MantleApproachBand Band = ResolveApproachBand(ApproachSpeed);

	// Actor forward, not move intent: a standing Jump must find the ledge with no stick held.
	FCollisionQueryParams Params(TEXT("AZ_MantleProbe"), /*bTraceComplex*/ false, Pawn);
	FHitResult FaceHit;
	const bool bHitFace = World->SweepSingleByChannel(FaceHit, PawnLocation, PawnLocation + Forward * Band.ForwardReach,
		FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(ProbeRadius), Params);

	const AActor* HitActor = bHitFace ? FaceHit.GetActor() : nullptr;
	if (!HitActor)
	{
		return false;
	}

	FVector Lip = FVector::ZeroVector;
	FVector Normal = FVector::ForwardVector;
	if (!SelectLedgeFromSplines(HitActor, PawnLocation, Forward, FeetZ, Band.MaxLedgeDistance, Lip, Normal))
	{
		return false;
	}

	// ---- Top support, measured where the capsule will actually stand ----
	// Probing at the landing inset rather than just past the lip is what rejects a narrow top: on a 20cm
	// wall this point lies beyond the far face, so the trace falls through to the floor a metre below and
	// the height check below fails. One ray at the edge would have accepted it.
	const FVector InsetXY = Lip - Normal * LandingInset;
	FHitResult TopHit;
	if (!World->LineTraceSingleByChannel(TopHit,
			FVector(InsetXY.X, InsetXY.Y, Lip.Z + 40.f),
			FVector(InsetXY.X, InsetXY.Y, Lip.Z - 30.f),
			TraceChannel, Params))
	{
		return false;
	}
	if (TopHit.ImpactNormal.Z < MinWalkableNormalZ)
	{
		return false;   // sloped or overhanging top
	}
	if (FMath::Abs(TopHit.ImpactPoint.Z - Lip.Z) > 10.f)
	{
		return false;   // that surface is not the ledge top (thin wall, gap, or a step further in)
	}

	// ---- Destination must be free for the whole capsule, not just its centre ----
	const FVector LandingLocation(InsetXY.X, InsetXY.Y, TopHit.ImpactPoint.Z + CapsuleHalfHeight + 2.f);
	if (World->OverlapBlockingTestByChannel(LandingLocation, FQuat::Identity, TraceChannel,
			FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight), Params))
	{
		return false;   // occupied, or no headroom to stand up into
	}

	// Foot comes from the LIVE animation state, never from the clip's filename — the source suffix means
	// the planted foot on the stand clips and the lifting foot on walk/run, so trusting names would enter
	// every moving mantle on the wrong foot.
	const EAZ_MantleFoot Foot = ResolveLivePlantedFoot();

	UAnimMontage* Montage = ResolveMontage(Band.Approach, Style, Foot);
	if (!Montage)
	{
		return false;
	}

	OutCandidate.TargetComponent  = FaceHit.GetComponent();
	OutCandidate.LedgeLocation    = Lip;
	OutCandidate.LedgeNormal      = Normal;
	OutCandidate.LandingTransform = FTransform((-Normal).Rotation(), LandingLocation);
	OutCandidate.HeightAboveFeet  = Lip.Z - FeetZ;
	OutCandidate.Style            = Style;
	OutCandidate.PlantedFoot      = Foot;
	OutCandidate.Approach         = Band.Approach;
	OutCandidate.ApproachSpeed    = ApproachSpeed;
	OutCandidate.Montage          = Montage;

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Lip, 8.f, 12, FColor::Green, false, 3.f);
		DrawDebugDirectionalArrow(GetWorld(), Lip, Lip + Normal * 40.f, 12.f, FColor::Cyan, false, 3.f);
		DrawDebugCapsule(GetWorld(), LandingLocation, CapsuleHalfHeight, CapsuleRadius, FQuat::Identity,
			FColor::Yellow, false, 3.f);
	}
#endif

	return true;
}

bool UAZ_TraversalComponent::IsImpactReactionOwningBody() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	TArray<USkeletalMeshComponent*> Meshes;
	Owner->GetComponents<USkeletalMeshComponent>(Meshes);
	for (const USkeletalMeshComponent* Mesh : Meshes)
	{
		if (const UAZ_MoverAnimInstance* AnimInst = Mesh ? Cast<UAZ_MoverAnimInstance>(Mesh->GetAnimInstance()) : nullptr)
		{
			return AnimInst->IsPlayingImpactReaction();
		}
	}
	return false;
}

bool UAZ_TraversalComponent::TryStartMantle()
{
	if (!MantleAbilityClass)
	{
		return false;
	}

	// A flinch that was already committed keeps the body. The press is dropped rather than deferred: a
	// mantle that fired itself when the reaction happened to end would land seconds after the player asked
	// for it, against a ledge they may no longer be facing.
	if (IsImpactReactionOwningBody())
	{
		return false;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (!ASC)
	{
		return false;
	}

	FAZ_MantleCandidate Candidate;
	if (!FindMantleCandidate(Candidate))
	{
		return false;
	}

	// The ability reads PendingCandidate inside its ActivateAbility, which runs synchronously below.
	PendingCandidate = Candidate;
	if (!ASC->TryActivateAbilityByClass(MantleAbilityClass))
	{
		ClearPendingCandidate();
		return false;   // blocked/ungranted: the caller must still be free to run the ordinary jump
	}
	return true;
}
