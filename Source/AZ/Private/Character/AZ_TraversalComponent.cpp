// Copyright Artur. AZ project.

#include "Character/AZ_TraversalComponent.h"
#include "Character/AZ_TraversalSurfaceData.h"
#include "AZ_GameplayTags.h"

#include "AbilitySystemComponent.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
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

namespace
{
	// Tools/traversal_bake_climb_and_clearance.py records final root travel beyond the authored front
	// face plus this margin as RequiredTopSupport. Recover that measured travel for the drop exit check.
	constexpr float ClimbSupportBakeMarginCm = 10.f;

	float GetClimbExitForwardDistance(float BakedTopSupport)
	{
		return BakedTopSupport - ClimbSupportBakeMarginCm;
	}
}

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

bool UAZ_TraversalComponent::SelectEntry(EAZ_TraversalAction InAction, EAZ_MantleApproach InApproach,
	EAZ_MantleFoot InFoot, float LedgeDistance, float UsableTopSupport,
	UAnimMontage*& OutMontage, float& OutStartTime, float& OutRemainingApproach,
	FVector2D& OutFrontLedgeRootOffset, float& OutAuthoredClearHeight,
	float& OutRequiredTopSupport, bool bAllowUnsupportedClimbExit, float MinClimbExitForwardDistance) const
{
	const bool bClimbDropSelection = bAllowUnsupportedClimbExit && InAction == EAZ_TraversalAction::Climb;
	// Families to consider, best first. Dropping to a SHORTER approach is allowed when the ledge is too
	// close for the one our speed implies, but never two steps: a standing clip at full running speed is
	// not a graceful degradation, it is a different animation playing at the wrong velocity.
	TArray<EAZ_MantleApproach, TInlineAllocator<3>> Families;
	Families.Add(InApproach);
	if (bAllowShorterApproachFallback)
	{
		if (InApproach == EAZ_MantleApproach::Run)
		{
			Families.Add(EAZ_MantleApproach::Walk);

			// CLIMB IS THE ONE ACTION THAT MAY DROP TWO FAMILIES. The rule against it holds for mantle and
			// hurdle -- a standing clip at full running speed is a different animation at the wrong
			// velocity, not a graceful degradation. Climb is different because the obstacle is two and a
			// half metres of wall: you cannot carry speed through it, so braking into a standing reach is
			// what actually happens. Without this, a sprint that closes to 71-91cm has NOTHING -- the run
			// and walk climbs start at 162 and 191cm, while the standing pair covers 57-99cm and fits
			// those presses almost exactly. Four refusals in one session were all this.
			// Family rank still orders Run > Walk > Stand, so the standing clip is reached only when
			// neither of the others fits at all.
			if (InAction == EAZ_TraversalAction::Climb)
			{
				Families.Add(EAZ_MantleApproach::Stand);
			}
		}
		else if (InApproach == EAZ_MantleApproach::Walk)
		{
			Families.Add(EAZ_MantleApproach::Stand);
		}
	}

#if !UE_BUILD_SHIPPING
	// Why-not counters. A silent `return false` here reads as "no ledge" at the call site, which is the
	// one thing it never means -- the geometry already passed. Each rejection reason is counted so one
	// press names the gate that closed instead of costing a build to find out.
	int32 DbgAction = 0, DbgStyle = 0, DbgTopSupport = 0, DbgNoSamples = 0, DbgOutOfTol = 0;
	int32 DbgClimbExit = 0;
	float DbgNearestErr = TNumericLimits<float>::Max();
	float DbgNearestRemaining = 0.f, DbgNearestTime = 0.f, DbgTolUsed = 0.f;
	const UAnimMontage* DbgNearestMontage = nullptr;
#endif

	UAnimMontage* BestMontage = nullptr;
	float BestTime = 0.f;
	float BestRemaining = 0.f;
	float BestError = TNumericLimits<float>::Max();
	int32 BestFamilyRank = TNumericLimits<int32>::Max();
	FVector2D BestFrontOffset = FVector2D::ZeroVector;
	float BestClearHeight = 0.f;
	float BestTopSupport = 0.f;

	for (int32 Rank = 0; Rank < Families.Num(); ++Rank)
	{
		for (const FAZ_MantleClipEntry& Entry : Clips)
		{
			if (Entry.Action != InAction || Entry.Approach != Families[Rank] || !Entry.Montage)
			{
				continue;
			}
#if !UE_BUILD_SHIPPING
			++DbgAction;
#endif
			// STYLE IS A PREFERENCE, NOT A GATE — same reasoning as the foot below, and the same mechanism.
			// As a gate it made the character's carriage decide whether an obstacle was traversable at all:
			// on a 20cm wall in Relaxed, the standing hurdles are step-ons the wall is too thin for and the
			// Relaxed walk clips only start at 131cm, so everything from 61cm inward was refused — while
			// Hurdle_1_0_Walk_Neutral_Lfoot enters from 89cm needing no top support at all. The penalty is
			// larger than any legal error can be, so a style MATCH always wins when one is legal; a
			// mismatch is reached only when the alternative is refusing a press the geometry allows.
			const bool bStyleMatches = (Entry.Style == Style);
#if !UE_BUILD_SHIPPING
			if (!bStyleMatches) { ++DbgStyle; }
#endif

			// THE CONTACT GATE, and the reason this requirement lives in data. A step-on hurdle plants a
			// foot on the obstacle top, so it needs that much usable top; a clearing hurdle touches nothing
			// and needs none. Measured: 29-44cm for stand/walk step-ons, 70-73cm for run. This is what
			// stops a 20cm wall selecting a clip that would plant a foot in mid-air past it.
			if (bClimbDropSelection)
			{
				// Only terminal standing support is waived. The authored climb must carry the whole
				// capsule past the far face before collision is restored and Falling takes ownership.
				const float ExitForward = GetClimbExitForwardDistance(Entry.RequiredTopSupport);
				if (!FMath::IsFinite(ExitForward) || ExitForward <= 0.f || ExitForward < MinClimbExitForwardDistance)
				{
#if !UE_BUILD_SHIPPING
					++DbgClimbExit;
#endif
					continue;
				}
			}
			else if (Entry.RequiredTopSupport > UsableTopSupport)
			{
#if !UE_BUILD_SHIPPING
				++DbgTopSupport;
#endif
				continue;
			}

			// No baked samples means the clip is UNUSABLE, not "enterable at time 0". The comment here
			// used to promise the latter while the code did the former, which is the worst of both: a
			// silently dropped clip that reads as supported. A clip lands in this state when its first
			// warp window opens on frame 0, leaving the bake no legal entry to emit — the fix belongs in
			// the asset (move the window off frame 0, as the Relaxed run mantles needed) or in the bake,
			// never in a guess here. Warned once per clip so it cannot hide again.
			if (Entry.EntrySamples.Num() == 0)
			{
#if !UE_BUILD_SHIPPING
				++DbgNoSamples;
				static TSet<FName> WarnedOnce;
				const FName ClipName = Entry.Montage->GetFName();
				if (!WarnedOnce.Contains(ClipName))
				{
					WarnedOnce.Add(ClipName);
					UE_LOG(LogTemp, Warning,
						TEXT("[MantleReq] clip %s has NO entry samples and can never be selected — its first ")
						TEXT("warp window almost certainly opens at t=0; re-author the window or re-bake."),
						*ClipName.ToString());
				}
#endif
				continue;
			}

			// Tolerance is PER ACTION, and a clip may override it. Detection reach and acceptable animation
			// distortion are separate questions: widening this to close a coverage gap buys availability
			// with visible spatial compression, which is the lurch we spent two builds removing.
			const float Tolerance =
				(Entry.ApproachToleranceOverride > 0.f) ? Entry.ApproachToleranceOverride
				: (InAction == EAZ_TraversalAction::Hurdle ? HurdleApproachTolerance : ApproachTolerance);

			for (const FAZ_MantleEntrySample& Sample : Entry.EntrySamples)
			{
				const float Error = FMath::Abs(Sample.RemainingApproach - LedgeDistance);
#if !UE_BUILD_SHIPPING
				// The NEAREST rejected entry is the whole diagnosis of a too-close press: it says how far
				// off the best authored approach was, and therefore whether the answer is a later entry
				// sample, a wider tolerance, or genuinely nothing.
				if (Error < DbgNearestErr)
				{
					DbgNearestErr       = Error;
					DbgNearestRemaining = Sample.RemainingApproach;
					DbgNearestTime      = Sample.Time;
					DbgNearestMontage   = Entry.Montage;
					DbgTolUsed          = Tolerance;
				}
#endif
				if (Error > Tolerance)
				{
#if !UE_BUILD_SHIPPING
					++DbgOutOfTol;
#endif
					continue;   // warping would have to compress or stretch the approach too far
				}

				// FOOT IS A PREFERENCE, NOT A GATE. A geometrically legal obstacle must not become
				// impossible to traverse just because no clip carries the matching binary foot label —
				// several genuinely have none (the standing hurdles lift both feet together, recorded as
				// Unknown). Mismatches are still ranked below matches, so the best available pose wins.
				const bool bFootMatches =
					(Sample.PlantedFoot == InFoot) || (Sample.PlantedFoot == EAZ_MantleFoot::Unknown);
				constexpr float FootMismatchPenaltyCm = 40.f;
				// Bigger than tolerance + the foot penalty combined, so it orders STRICTLY above them:
				// every entry that reaches this point is already legal, so the penalty only ranks, and a
				// style match can never lose to a mismatch that merely fits the distance better.
				constexpr float StyleMismatchPenaltyCm = 500.f;
				const float Score = Error
					+ (bFootMatches  ? 0.f : FootMismatchPenaltyCm)
					+ (bStyleMatches ? 0.f : StyleMismatchPenaltyCm);

				// Prefer the better family; inside a family prefer the best score.
				if (Rank < BestFamilyRank || (Rank == BestFamilyRank && Score < BestError))
				{
					BestFamilyRank = Rank;
					BestError      = Score;
					BestMontage    = Entry.Montage;
					BestTime        = Sample.Time;
					BestRemaining   = Sample.RemainingApproach;
					BestFrontOffset = Entry.FrontLedgeRootOffset;
					BestClearHeight = Entry.AuthoredClearHeight;
					BestTopSupport  = Entry.RequiredTopSupport;
				}
			}
		}

		if (BestMontage)
		{
			break;   // a fit in a better family always wins; do not keep scanning shorter ones
		}
	}

	if (!BestMontage)
	{
#if !UE_BUILD_SHIPPING
		// "unbounded" is the real meaning of TNumericLimits<float>::Max() here (a platform whose far side
		// we never found has all the room a contact could want); printing it as 340282346638528859811704183484516925440 helped nobody.
		const FString TopSupportText = (UsableTopSupport >= TNumericLimits<float>::Max() * 0.5f)
			? FString(TEXT("inf")) : FString::Printf(TEXT("%.0f"), UsableTopSupport);
		UE_LOG(LogTemp, Warning,
			TEXT("[MantleReq] no-entry action=%d approach=%d foot=%d dist=%.0f topSupport=%s style=%d ")
			TEXT("| clips(action)=%d styleMismatch=%d skipped: topSupport=%d noSamples=%d outOfTol=%d climbExit=%d minExit=%.0f ")
			TEXT("| nearest=%s t=%.2f authored=%.0f err=%.0f tol=%.0f"),
			static_cast<int32>(InAction), static_cast<int32>(InApproach), static_cast<int32>(InFoot),
			LedgeDistance, *TopSupportText, static_cast<int32>(Style),
			DbgAction, DbgStyle, DbgTopSupport, DbgNoSamples, DbgOutOfTol, DbgClimbExit, MinClimbExitForwardDistance,
			*GetNameSafe(DbgNearestMontage), DbgNearestTime, DbgNearestRemaining,
			(DbgNearestErr == TNumericLimits<float>::Max()) ? -1.f : DbgNearestErr, DbgTolUsed);
#endif
		return false;   // explicit no — the caller must not invent a fit
	}

	OutMontage               = BestMontage;
	OutStartTime             = BestTime;
	OutRemainingApproach     = BestRemaining;
	OutFrontLedgeRootOffset  = BestFrontOffset;
	OutAuthoredClearHeight   = BestClearHeight;
	OutRequiredTopSupport    = BestTopSupport;
	return true;
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
		// DETECTION reach, not mantle's band: a 250cm climb ledge has to survive this filter to be
		// considered at all. Per-action legality is decided below, where each action states its own band.
		const float DetectMaxHeight = FMath::Max(MaxLedgeHeight, ClimbMaxHeight);
		if (Height < MinLedgeHeight || Height > DetectMaxHeight)
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

bool UAZ_TraversalComponent::SelectLedgeFromSurface(const FAZ_TraversalSurfaceGeometry& Surface,
	const FVector& PawnLocation, const FVector& Forward, float FeetZ, float MaxDistance,
	float CapsuleRadius, FVector& OutLip, FVector& OutNormal, FVector& OutFarLip,
	bool& OutHasFarLip, float& OutDeclaredDepth, FString& OutReason) const
{
	const float EndMargin = CapsuleRadius + Surface.EdgeEndMargin;
	if (!FMath::IsFinite(EndMargin) || EndMargin < 0.f)
	{
		OutReason = TEXT("invalid edge end margin");
		return false;
	}

	const float CosTolerance = FMath::Cos(FMath::DegreesToRadians(FacingToleranceDeg));
	const float DetectMaxHeight = FMath::Max(MaxLedgeHeight, ClimbMaxHeight);
	float BestPlanarDistSq = TNumericLimits<float>::Max();
	bool bFound = false;
	for (int32 Index = 0; Index < Surface.Edges.Num(); ++Index)
	{
		const FAZ_TraversalSurfaceWorldEdge& Edge = Surface.Edges[Index];
		if (Edge.Start.ContainsNaN() || Edge.End.ContainsNaN() || Edge.OutwardNormal.ContainsNaN())
		{
			continue;
		}
		const FVector Along = Edge.End - Edge.Start;
		const double Length = Along.Size();
		if (Length <= KINDA_SMALL_NUMBER || Length < 2.0 * EndMargin)
		{
			continue;
		}
		const double Alpha = FMath::Clamp(FVector::DotProduct(PawnLocation - Edge.Start, Along) / Along.SizeSquared(),
			EndMargin / Length, 1.0 - EndMargin / Length);
		const FVector Lip = FMath::Lerp(Edge.Start, Edge.End, Alpha);
		FVector Normal = Edge.OutwardNormal;
		Normal.Z = 0.f;
		if (!Normal.Normalize())
		{
			continue;
		}
		const float Height = Lip.Z - FeetZ;
		FVector ToLip = Lip - PawnLocation;
		ToLip.Z = 0.f;
		const float PlanarDistSq = ToLip.SizeSquared();
		if (Height < MinLedgeHeight || Height > DetectMaxHeight
			|| PlanarDistSq > FMath::Square(MaxDistance) || PlanarDistSq <= KINDA_SMALL_NUMBER
			|| FVector::DotProduct(ToLip.GetSafeNormal(), Forward) <= 0.f
			|| FVector::DotProduct(Forward, -Normal) < CosTolerance)
		{
			continue;
		}

		// A front-only authored ledge can support a standing action after real support traces. Crossing
		// requires its declared opposite edge; it may never borrow one from legacy splines on this actor.
		FVector FarLip = FVector::ZeroVector;
		float Depth = 0.f;
		const bool bHasFarLip = Edge.OppositeIndex != INDEX_NONE;
		if (bHasFarLip)
		{
			if (!Surface.Edges.IsValidIndex(Edge.OppositeIndex) || Edge.OppositeIndex == Index)
			{
				continue;
			}
			const FAZ_TraversalSurfaceWorldEdge& Opposite = Surface.Edges[Edge.OppositeIndex];
			if (Opposite.Start.ContainsNaN() || Opposite.End.ContainsNaN() || Opposite.OutwardNormal.ContainsNaN())
			{
				continue;
			}
			FVector FarNormal = Opposite.OutwardNormal;
			FarNormal.Z = 0.f;
			const FVector FarAlong = Opposite.End - Opposite.Start;
			const double FarLength = FarAlong.Size();
			if (!FarNormal.Normalize() || FVector::DotProduct(FarNormal, -Normal) < 0.8f
				|| FarLength <= KINDA_SMALL_NUMBER || FarLength < 2.0 * EndMargin)
			{
				continue;
			}
			const double FarAlpha = FMath::Clamp(FVector::DotProduct(Lip - Opposite.Start, FarAlong) / FarAlong.SizeSquared(),
				EndMargin / FarLength, 1.0 - EndMargin / FarLength);
			FarLip = FMath::Lerp(Opposite.Start, Opposite.End, FarAlpha);
			FVector Across = FarLip - Lip;
			Across.Z = 0.f;
			Depth = FVector::DotProduct(Across, -Normal);
			if (Depth <= KINDA_SMALL_NUMBER || FMath::Abs(FarLip.Z - Lip.Z) > 10.f)
			{
				continue;
			}
		}
		if (PlanarDistSq < BestPlanarDistSq)
		{
			BestPlanarDistSq = PlanarDistSq;
			OutLip = Lip;
			OutNormal = Normal;
			OutFarLip = FarLip;
			OutHasFarLip = bHasFarLip;
			OutDeclaredDepth = Depth;
			bFound = true;
		}
	}
	OutReason = bFound ? FString() : TEXT("no edge fits height, reach, facing and capsule end margins");
	return bFound;
}

bool UAZ_TraversalComponent::FindFarSide(const AActor* HitActor, const FVector& Lip, const FVector& Normal,
	float ApproachFeetZ, float CapsuleRadius, float CapsuleHalfHeight, const FCollisionQueryParams& Params,
	FVector& OutFarEdge, FVector& OutLanding, float& OutDepth, const FVector* SuppliedFarLip) const
{
	const UWorld* World = GetWorld();
	if (!World || !HitActor)
	{
		return false;
	}

	// ---- The far top edge, from the authored OPPOSITE spline ----
	// Blocks pair their edges, so the far side is authored rather than inferred. We look for the spline
	// whose outward normal points the way we are travelling — that is the face we will come down past.
	TArray<USplineComponent*> Splines;
	if (!SuppliedFarLip)
	{
		HitActor->GetComponents<USplineComponent>(Splines);
	}

	const FVector Inward = -Normal;
	float BestDot = -1.f;
	FVector FarEdge = FVector::ZeroVector;
	bool bFoundEdge = false;
	if (SuppliedFarLip)
	{
		if (SuppliedFarLip->ContainsNaN() || FVector::DotProduct(*SuppliedFarLip - Lip, Inward) <= 0.f)
		{
			return false;
		}
		FarEdge = *SuppliedFarLip;
		bFoundEdge = true;
	}

	for (const USplineComponent* Spline : Splines)
	{
		if (!Spline || Spline->GetNumberOfSplinePoints() < 2)
		{
			continue;
		}
		const float Key = Spline->FindInputKeyClosestToWorldLocation(Lip);
		FVector FaceNormal = Spline->GetUpVectorAtSplineInputKey(Key, ESplineCoordinateSpace::World);
		FaceNormal.Z = 0.f;
		if (!FaceNormal.Normalize())
		{
			continue;
		}
		const float Facing = FVector::DotProduct(FaceNormal, Inward);
		if (Facing < 0.8f)
		{
			continue;   // a side edge, not the opposite face
		}
		const FVector Point = Spline->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World);
		FVector Across = Point - Lip;
		Across.Z = 0.f;
		if (FVector::DotProduct(Across, Inward) <= 0.f)
		{
			continue;   // not actually beyond us
		}
		if (Facing > BestDot)
		{
			BestDot = Facing;
			FarEdge = Point;
			bFoundEdge = true;
		}
	}

	if (!bFoundEdge)
	{
		return false;   // cannot measure the crossing, so we refuse rather than guess its width
	}

	FVector Across = FarEdge - Lip;
	Across.Z = 0.f;
	OutDepth = static_cast<float>(Across.Size());
	if (OutDepth <= KINDA_SMALL_NUMBER || OutDepth > MaxHurdleDepth)
	{
		return false;   // too wide to be a barrier; that is something you get ONTO, not over
	}

	// ---- A landing we are willing to commit to ----
	// Probed at several distances beyond the far edge, because the authored clips land 83-167cm past the
	// front anchor and the obstacle eats part of that. Nothing found = no landing = refuse. We never
	// commit to an unmeasured drop; a character that hurdles a parapet off a roof is not a review note.
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	static const float ProbeDistances[] = { 45.f, 70.f, 95.f };

	for (const float Distance : ProbeDistances)
	{
		if (Distance > FarSideProbeDistance)
		{
			break;
		}
		const FVector XY = FarEdge + Inward * Distance;

		FHitResult Floor;
		if (!World->LineTraceSingleByChannel(Floor,
				FVector(XY.X, XY.Y, Lip.Z + 20.f),
				FVector(XY.X, XY.Y, ApproachFeetZ - MaxLandingDrop - 20.f),
				TraceChannel, Params))
		{
			continue;   // open air below within our tolerance
		}
		if (Floor.ImpactNormal.Z < MinWalkableNormalZ)
		{
			continue;   // too steep to land on
		}
		const float Delta = static_cast<float>(Floor.ImpactPoint.Z) - ApproachFeetZ;
		if (Delta < -MaxLandingDrop || Delta > MaxLandingRise)
		{
			continue;
		}
		const FVector Stand(XY.X, XY.Y, Floor.ImpactPoint.Z + CapsuleHalfHeight + 2.f);
		if (World->OverlapBlockingTestByChannel(Stand, FQuat::Identity, TraceChannel, Capsule, Params))
		{
			continue;   // occupied or no headroom
		}

		OutFarEdge = FarEdge;
		// The FEET, not the capsule centre. These hurdle windows use no warp-point provider, so the target
		// specifies the ROOT destination directly — and the root sits on the floor. Handing it the capsule
		// centre would land the character a full half-height in the air. The centre is still what the
		// clearance test above used, because that is what actually has to fit.
		OutLanding = FVector(XY.X, XY.Y, Floor.ImpactPoint.Z);
		return true;
	}

	return false;
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

	// Speed ALONE is not intent. A capture showed a mantle selected off residual momentum 0.389 s into a
	// WalkFwdStop with the stick already released: 82 cm/s picked the Walk family, so the character
	// "walked" into a ledge it was in the middle of stopping in front of. If there is no raw movement
	// intent, this is a standing entry whatever the body is still coasting at.
	// RAW intent, not the Mover's clamped input: the walking clamp zeroes intent precisely when you face a
	// wall, which is exactly where a mantle is wanted (§11).
	bool bHasMoveIntent = true;
	if (const AAZ_PawnMoverHeroCharacter* Hero = Cast<AAZ_PawnMoverHeroCharacter>(Pawn))
	{
		bHasMoveIntent = Hero->GetWorldMoveIntentRaw().SizeSquared2D() > FMath::Square(0.1f);
	}
	const FAZ_MantleApproachBand Band = bHasMoveIntent
		? ResolveApproachBand(ApproachSpeed)
		: ResolveApproachBand(0.f);

	// ★ GROUNDED ENTRY. Every authored clip in the set starts from a foot on the floor, so a press made
	// in the air would warp the character out of a fall and into a run-up it never had. Nothing else in
	// the chain refuses it: the candidate query never asked about movement state and the jump ability's
	// CanActivate only tests the requester interface. Falling is the one air mode the hero registers;
	// RMAction (jump takeoff) is left alone because the jump ability owns the body there anyway and a
	// press is already refused as BodyBusy.
	if (bRequireGroundedEntry)
	{
		if (const UMoverComponent* GroundCheck = Pawn->FindComponentByClass<UMoverComponent>())
		{
			if (GroundCheck->GetMovementModeName() == TEXT("Falling"))
			{
#if !UE_BUILD_SHIPPING
				UE_LOG(LogTemp, Warning, TEXT("[MantleReq] airborne mode=Falling — grounded entry required"));
#endif
				return false;
			}
		}
	}

	// Actor forward, not move intent: a standing Jump must find the ledge with no stick held.
	FCollisionQueryParams Params(TEXT("AZ_MantleProbe"), /*bTraceComplex*/ false, Pawn);
	FHitResult FaceHit;
	const bool bHitFace = World->SweepSingleByChannel(FaceHit, PawnLocation, PawnLocation + Forward * Band.ForwardReach,
		FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(ProbeRadius), Params);

	const AActor* HitActor = bHitFace ? FaceHit.GetActor() : nullptr;
	if (!HitActor)
	{
#if !UE_BUILD_SHIPPING
		// Nothing within reach. Printed because "the probe found no wall" and "the wall had no usable
		// ledge" are different failures and were previously indistinguishable from the outside.
		UE_LOG(LogTemp, Warning,
			TEXT("[MantleReq] no-hit reach=%.0f approach=%d spd=%.0f from=(%.0f,%.0f,%.0f) fwd=(%.2f,%.2f)"),
			Band.ForwardReach, static_cast<int32>(Band.Approach), ApproachSpeed,
			PawnLocation.X, PawnLocation.Y, PawnLocation.Z, Forward.X, Forward.Y);
#endif
		return false;
	}

	FAZ_TraversalSurfaceGeometry Surface;
	FString SurfaceReason;
	const EAZ_TraversalSurfaceResolveResult SurfaceResult = UAZ_TraversalSurfaceData::Resolve(FaceHit, Surface, SurfaceReason);
	const bool bExplicitSurface = SurfaceResult == EAZ_TraversalSurfaceResolveResult::Resolved;
	if (SurfaceResult != EAZ_TraversalSurfaceResolveResult::NotConfigured && !bExplicitSurface)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning,
			TEXT("[MantleReq] surface-%s hit=%s comp=%s source=%s reason=%s"),
			SurfaceResult == EAZ_TraversalSurfaceResolveResult::Disabled ? TEXT("disabled") : TEXT("invalid"),
			*HitActor->GetActorNameOrLabel(), *GetNameSafe(FaceHit.GetComponent()),
			*Surface.SourceDescription, *SurfaceReason);
#endif
		return false;   // Explicit configuration owns the result, including refusal. No legacy fallback.
	}
	if (bExplicitSurface
		&& !(Surface.bSupportsStanding && (Surface.bAllowMantle || Surface.bAllowClimb))
		&& !(Surface.bSupportsCrossing && (Surface.bAllowHurdle || (Surface.bAllowClimb && Surface.bAllowClimbAndDrop))))
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning,
			TEXT("[MantleReq] unsupported-surface-actions hit=%s comp=%s source=%s allow=%d/%d/%d standing=%d crossing=%d climbDrop=%d"),
			*HitActor->GetActorNameOrLabel(), *GetNameSafe(FaceHit.GetComponent()), *Surface.SourceDescription,
			Surface.bAllowMantle, Surface.bAllowHurdle, Surface.bAllowClimb,
			Surface.bSupportsStanding, Surface.bSupportsCrossing, Surface.bAllowClimbAndDrop);
#endif
		return false;
	}

	FVector Lip = FVector::ZeroVector;
	FVector Normal = FVector::ForwardVector;
	FVector SuppliedFarLip = FVector::ZeroVector;
	bool bHasSuppliedFarLip = false;
	float DeclaredSurfaceDepth = 0.f;
	if (bExplicitSurface)
	{
		if (!SelectLedgeFromSurface(Surface, PawnLocation, Forward, FeetZ, Band.MaxLedgeDistance,
				CapsuleRadius, Lip, Normal, SuppliedFarLip, bHasSuppliedFarLip, DeclaredSurfaceDepth, SurfaceReason))
		{
#if !UE_BUILD_SHIPPING
			UE_LOG(LogTemp, Warning,
				TEXT("[MantleReq] unfit-surface-geometry hit=%s comp=%s source=%s reason=%s"),
				*HitActor->GetActorNameOrLabel(), *GetNameSafe(FaceHit.GetComponent()),
				*Surface.SourceDescription, *SurfaceReason);
#endif
			return false;
		}
	}
	else if (!SelectLedgeFromSplines(HitActor, PawnLocation, Forward, FeetZ, Band.MaxLedgeDistance, Lip, Normal))
	{
#if !UE_BUILD_SHIPPING
		// Name the actual thing we hit. "NoCandidate" alone cannot distinguish a plain block with no
		// authored ledges from a traversable whose splines failed the height/facing test, and guessing
		// between those from a screenshot is exactly what we must not do.
		TArray<USplineComponent*> HitSplines;
		HitActor->GetComponents<USplineComponent>(HitSplines);
		UE_LOG(LogTemp, Warning,
			TEXT("[MantleReq] no-ledge hit=%s class=%s comp=%s splines=%d dist=%.0f provider=none reason=%s"),
			*HitActor->GetActorNameOrLabel(), *GetNameSafe(HitActor->GetClass()),
			*GetNameSafe(FaceHit.GetComponent()), HitSplines.Num(),
			FVector::Dist2D(PawnLocation, FaceHit.ImpactPoint),
			HitSplines.IsEmpty() ? TEXT("no-provider") : TEXT("unfit-legacy-ledge"));
#endif
		return false;
	}

	// ---- Top support, measured where the capsule will actually stand ----
	// Probing at the landing inset rather than just past the lip is what rejects a narrow top: on a 20cm
	// wall this point lies beyond the far face, so the trace falls through to the floor a metre below and
	// the height check below fails. One ray at the edge would have accepted it.
	const FVector InsetXY = Lip - Normal * LandingInset;
	FHitResult TopHit;
	FVector LandingLocation = FVector::ZeroVector;
	// bTopStandable answers ONE question -- is there a surface up there the capsule can stand on -- and
	// mantle and climb both need exactly that answer, differing only in how high it is. Deriving both from
	// one trace is what keeps them from drifting apart.
	bool bTopStandable = World->LineTraceSingleByChannel(TopHit,
		FVector(InsetXY.X, InsetXY.Y, Lip.Z + 40.f),
		FVector(InsetXY.X, InsetXY.Y, Lip.Z - 30.f),
		TraceChannel, Params);

	if (bTopStandable && TopHit.ImpactNormal.Z < MinWalkableNormalZ)
	{
		bTopStandable = false;   // sloped or overhanging top
	}
	if (bTopStandable && FMath::Abs(TopHit.ImpactPoint.Z - Lip.Z) > 10.f)
	{
		bTopStandable = false;   // not the ledge top (thin wall, gap, or a step further in)
	}
	if (bTopStandable)
	{
		// The destination must be free for the WHOLE capsule, not just its centre.
		LandingLocation = FVector(InsetXY.X, InsetXY.Y, TopHit.ImpactPoint.Z + CapsuleHalfHeight + 2.f);
		if (World->OverlapBlockingTestByChannel(LandingLocation, FQuat::Identity, TraceChannel,
				FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight), Params))
		{
			bTopStandable = false;   // occupied, or no headroom to stand up into
		}
	}

	// Height decides WHICH of the two standing actions the surface qualifies for. The bands are disjoint
	// by construction, so this is a classification, not a contest.
	const float LipHeightAboveFeet = Lip.Z - FeetZ;
	const bool bMantleGeometryLegal = bTopStandable
		&& LipHeightAboveFeet >= MinLedgeHeight && LipHeightAboveFeet <= MaxLedgeHeight;
	const bool bWithinClimbHeight = LipHeightAboveFeet >= ClimbMinHeight && LipHeightAboveFeet <= ClimbMaxHeight;
	const bool bClimbGeometryLegal = bTopStandable && bWithinClimbHeight;
	const bool bMantleLegal = bMantleGeometryLegal
		&& (!bExplicitSurface || (Surface.bAllowMantle && Surface.bSupportsStanding));
	const bool bStandingClimbLegal = bClimbGeometryLegal
		&& (!bExplicitSurface || (Surface.bAllowClimb && Surface.bSupportsStanding));

	// ---- Hurdle legality is a DIFFERENT question, asked independently ----
	// Mantle asks "can I stand on top of this"; hurdle asks "can I cross it and land beyond". A 20cm wall
	// fails the first and can pass the second; a broad platform is the reverse. Neither answer is derived
	// from the other, and neither is derived from a hardcoded depth threshold.
	FVector FarEdge = FVector::ZeroVector;
	FVector FarLanding = FVector::ZeroVector;
	float ObstacleDepth = 0.f;
	const bool bClimbDropPermitted = bExplicitSurface && Surface.bAllowClimbAndDrop
		&& Surface.bAllowClimb && Surface.bSupportsCrossing && bHasSuppliedFarLip && bWithinClimbHeight;
	const bool bCanProbeFarSide = bClimbDropPermitted
		|| (LipHeightAboveFeet <= MaxHurdleHeight && (!bExplicitSurface || bHasSuppliedFarLip));
	const bool bFarSideLegal = bCanProbeFarSide
		&& FindFarSide(HitActor, Lip, Normal, FeetZ, CapsuleRadius, CapsuleHalfHeight,
			Params, FarEdge, FarLanding, ObstacleDepth, bExplicitSurface ? &SuppliedFarLip : nullptr);
	const bool bHurdleGeometryLegal = LipHeightAboveFeet <= MaxHurdleHeight && bFarSideLegal;
	const bool bHurdleLegal = bHurdleGeometryLegal
		&& (!bExplicitSurface || (Surface.bAllowHurdle && Surface.bSupportsCrossing));
	const bool bClimbAndDropLegal = bClimbDropPermitted && bFarSideLegal;
	const bool bClimbLegal = bStandingClimbLegal || bClimbAndDropLegal;
	const bool bUseClimbAndDrop = bClimbAndDropLegal && !bStandingClimbLegal;

	if (!bMantleLegal && !bHurdleLegal && !bClimbLegal)
	{
#if !UE_BUILD_SHIPPING
		if (bExplicitSurface)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[MantleReq] %s hit=%s comp=%s source=%s allow=%d/%d/%d standing=%d crossing=%d paired=%d climbDrop=%d"),
				(bMantleGeometryLegal || bClimbGeometryLegal || bHurdleGeometryLegal)
					? TEXT("unsupported-surface-actions") : TEXT("unfit-surface-geometry"),
				*HitActor->GetActorNameOrLabel(), *GetNameSafe(FaceHit.GetComponent()), *Surface.SourceDescription,
				Surface.bAllowMantle, Surface.bAllowHurdle, Surface.bAllowClimb,
				Surface.bSupportsStanding, Surface.bSupportsCrossing, bHasSuppliedFarLip, Surface.bAllowClimbAndDrop);
		}
		// Both legality questions answered no. Printing the measurements that decided it is the only way
		// to tell a too-thin top from a missing far-side landing without re-deriving the geometry by hand.
		UE_LOG(LogTemp, Warning,
			TEXT("[MantleReq] illegal lip=(%.0f,%.0f,%.0f) hAboveFeet=%.0f standable=%d | mantle %.0f-%.0f ")
			TEXT("climb %.0f-%.0f hurdleMax %.0f | topHit=%d topZ=%.0f nZ=%.2f ")
			TEXT("| hurdle: depth=%.0f farEdge=(%.0f,%.0f,%.0f) landing=(%.0f,%.0f,%.0f)"),
			Lip.X, Lip.Y, Lip.Z, LipHeightAboveFeet, bTopStandable ? 1 : 0,
			MinLedgeHeight, MaxLedgeHeight, ClimbMinHeight, ClimbMaxHeight, MaxHurdleHeight,
			TopHit.bBlockingHit ? 1 : 0, TopHit.ImpactPoint.Z, TopHit.ImpactNormal.Z,
			ObstacleDepth, FarEdge.X, FarEdge.Y, FarEdge.Z,
			FarLanding.X, FarLanding.Y, FarLanding.Z);
#endif
		return false;
	}

	// Foot comes from the LIVE animation state, never from the clip's filename — the source suffix means
	// the planted foot on the stand clips and the lifting foot on walk/run, so trusting names would enter
	// every moving mantle on the wrong foot.
	const EAZ_MantleFoot Foot = ResolveLivePlantedFoot();

	// How far the ledge ACTUALLY is — the number the authored approach has to be matched against.
	FVector ToLipPlanar = Lip - PawnLocation;
	ToLipPlanar.Z = 0.f;
	const float LedgeDistance = static_cast<float>(ToLipPlanar.Size());

	// ---- Which action? Geometry already decided LEGALITY; movement context only breaks a genuine tie ----
	// Mantle when standing or walking, hurdle when running. The approach band comes from real velocity
	// plus RAW intent, so this is not the speed left over after the wall clamp has slowed the character.
	TArray<EAZ_TraversalAction, TInlineAllocator<3>> ActionOrder;
	// Climb goes first whenever it is legal, and it can only be legal in a band the other two are shut out
	// of, so this is not a preference that can steal a mantle -- it is the only answer at that height.
	if (bClimbLegal)
	{
		ActionOrder.Add(EAZ_TraversalAction::Climb);
	}
	if (bMantleLegal && bHurdleLegal)
	{
		if (Band.Approach == EAZ_MantleApproach::Run)
		{
			ActionOrder.Add(EAZ_TraversalAction::Hurdle);
			ActionOrder.Add(EAZ_TraversalAction::Mantle);
		}
		else
		{
			ActionOrder.Add(EAZ_TraversalAction::Mantle);
			ActionOrder.Add(EAZ_TraversalAction::Hurdle);
		}
	}
	else if (bMantleLegal)
	{
		ActionOrder.Add(EAZ_TraversalAction::Mantle);
	}
	else if (bHurdleLegal)
	{
		ActionOrder.Add(EAZ_TraversalAction::Hurdle);
	}

	// Usable top support is the obstacle's OWN depth where we measured a far side. A broad platform whose
	// far side we never found is effectively unbounded for contact purposes — a step-on clip has all the
	// room it could want there, it just is not a hurdle target.
	// ★ MEASURED, not inherited. This used to be `bHurdleLegal ? ObstacleDepth : FLT_MAX`, and since a
	// climb ledge is ~248cm it is always above MaxHurdleHeight, so hurdle was always illegal there and the
	// support was always infinite. That made every climb clip's baked RequiredTopSupport (59-74cm) dead
	// data: a ledge with a hand's width of top would have passed. Walk the top surface inward and find
	// where it actually stops.
	float UsableTopSupport = bHurdleLegal
		? ObstacleDepth
		: (bTopStandable ? MeasureTopSupportDepth(Lip, Normal, Params) : 0.f);
	if (bExplicitSurface)
	{
		// Bounds describe the permitted surface, not physical support. Every contact-requiring action,
		// including a step-on hurdle, needs real top support and may not use space past a declared edge.
		// A front-only ledge retains measured support; absent metadata never becomes infinite support.
		UsableTopSupport = Surface.bSupportsStanding ? MeasureTopSupportDepth(Lip, Normal, Params) : 0.f;
		if (bHasSuppliedFarLip)
		{
			UsableTopSupport = FMath::Min(UsableTopSupport, DeclaredSurfaceDepth);
		}
	}

	UAnimMontage* Montage = nullptr;
	float StartTime = 0.f;
	float RemainingApproach = 0.f;
	FVector2D FrontOffset = FVector2D::ZeroVector;
	float AuthoredClearHeight = 0.f;
	float SelectedTopSupport  = 0.f;
	EAZ_TraversalAction ChosenAction = ActionOrder[0];
	bool bSelected = false;
	const float MinClimbExitForwardDistance = bUseClimbAndDrop
		? DeclaredSurfaceDepth + CapsuleRadius + Surface.EdgeEndMargin : 0.f;
	for (const EAZ_TraversalAction Action : ActionOrder)
	{
		if (SelectEntry(Action, Band.Approach, Foot, LedgeDistance, UsableTopSupport,
				Montage, StartTime, RemainingApproach, FrontOffset, AuthoredClearHeight,
				SelectedTopSupport, bUseClimbAndDrop && Action == EAZ_TraversalAction::Climb,
				MinClimbExitForwardDistance))
		{
			ChosenAction = Action;
			bSelected = true;
			break;   // preferred action first; the other is only a fallback when nothing fits
		}
	}
	if (!bSelected)
	{
#if !UE_BUILD_SHIPPING
		// SelectEntry has already named the gate per action; this line supplies the context it cannot see.
		UE_LOG(LogTemp, Warning,
			TEXT("[MantleReq] no-fit dist=%.0f band=%d spd=%.0f foot=%d mantleLegal=%d hurdleLegal=%d climbLegal=%d topSupport=%s actions=%d hit=%s source=%s"),
			LedgeDistance, static_cast<int32>(Band.Approach), ApproachSpeed, static_cast<int32>(Foot),
			bMantleLegal ? 1 : 0, bHurdleLegal ? 1 : 0, bClimbLegal ? 1 : 0,
			*((UsableTopSupport >= TNumericLimits<float>::Max() * 0.5f)
				? FString(TEXT("inf")) : FString::Printf(TEXT("%.0f"), UsableTopSupport)),
			ActionOrder.Num(), *HitActor->GetActorNameOrLabel(),
			bExplicitSurface ? *Surface.SourceDescription : TEXT("legacy-splines"));
#endif
		return false;   // nothing fits from here; the press falls through to an ordinary jump
	}

	const bool bSelectedClimbAndDrop = bUseClimbAndDrop && ChosenAction == EAZ_TraversalAction::Climb;
	if (bSelectedClimbAndDrop)
	{
		const float ExitForward = GetClimbExitForwardDistance(SelectedTopSupport);
		const FVector ExitAtLipHeight = Lip - Normal * ExitForward;
		const FVector TerminalLocation = ExitAtLipHeight + FVector(0.f, 0.f, CapsuleHalfHeight + 2.f);
		const FCollisionShape ExitCapsule = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
		const bool bTerminalClear = !World->OverlapBlockingTestByChannel(
			TerminalLocation, FQuat::Identity, TraceChannel, ExitCapsule, Params);

		// FindFarSide already proved a landing beyond the paired edge. Also check directly below the
		// selected clip's measured exit: that point can differ from the generic 45/70/95cm landing probes.
		FHitResult ExitFloor;
		const bool bHitExitFloor = World->LineTraceSingleByChannel(ExitFloor,
			FVector(ExitAtLipHeight.X, ExitAtLipHeight.Y, Lip.Z + 20.f),
			FVector(ExitAtLipHeight.X, ExitAtLipHeight.Y, FeetZ - MaxLandingDrop - 20.f), TraceChannel, Params);
		const float FloorDelta = ExitFloor.ImpactPoint.Z - FeetZ;
		const bool bExitFloorValid = bHitExitFloor && ExitFloor.ImpactNormal.Z >= MinWalkableNormalZ
			&& FloorDelta >= -MaxLandingDrop && FloorDelta <= MaxLandingRise;
		const FVector FloorCapsuleLocation(ExitAtLipHeight.X, ExitAtLipHeight.Y,
			ExitFloor.ImpactPoint.Z + CapsuleHalfHeight + 2.f);
		const bool bFloorCapsuleClear = bExitFloorValid && !World->OverlapBlockingTestByChannel(
			FloorCapsuleLocation, FQuat::Identity, TraceChannel, ExitCapsule, Params);
		FHitResult DropHit;
		const bool bDropPathClear = bTerminalClear && bFloorCapsuleClear
			&& !World->SweepSingleByChannel(DropHit, TerminalLocation, FloorCapsuleLocation,
				FQuat::Identity, TraceChannel, ExitCapsule, Params);
		if (!bDropPathClear)
		{
#if !UE_BUILD_SHIPPING
			const TCHAR* Reason = !bTerminalClear ? TEXT("terminal-overlap")
				: (!bExitFloorValid ? TEXT("no-valid-floor-under-exit")
					: (!bFloorCapsuleClear ? TEXT("landing-overlap") : TEXT("blocked-drop-path")));
			UE_LOG(LogTemp, Warning,
				TEXT("[MantleReq] unsafe-climb-drop hit=%s source=%s clip=%s reason=%s exitForward=%.0f minExit=%.0f terminal=(%.0f,%.0f,%.0f)"),
				*HitActor->GetActorNameOrLabel(), *Surface.SourceDescription, *GetNameSafe(Montage), Reason,
				ExitForward, MinClimbExitForwardDistance, TerminalLocation.X, TerminalLocation.Y, TerminalLocation.Z);
#endif
			return false;
		}

		// This is the predicted airborne handoff pose, not a supported landing or a teleport target.
		// The executor plays the climb, restores collision and enters Falling. Keep the real floor separate.
		LandingLocation = TerminalLocation;
		FarLanding = ExitFloor.ImpactPoint;
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning,
			TEXT("[MantleReq] climb-drop-exit hit=%s source=%s clip=%s exitForward=%.0f minExit=%.0f depth=%.0f terminal=(%.0f,%.0f,%.0f) floor=(%.0f,%.0f,%.0f)"),
			*HitActor->GetActorNameOrLabel(), *Surface.SourceDescription, *GetNameSafe(Montage),
			ExitForward, MinClimbExitForwardDistance, ObstacleDepth, TerminalLocation.X, TerminalLocation.Y, TerminalLocation.Z,
			FarLanding.X, FarLanding.Y, FarLanding.Z);
#endif
	}

	OutCandidate.TargetComponent  = FaceHit.GetComponent();
	OutCandidate.LedgeLocation    = Lip;
	OutCandidate.LedgeNormal      = Normal;
	// A mantle ends ON the obstacle; a hurdle ends BEYOND it. Same field, genuinely different destination.
	OutCandidate.LandingTransform = FTransform((-Normal).Rotation(),
		(ChosenAction == EAZ_TraversalAction::Hurdle) ? FarLanding : LandingLocation);
	OutCandidate.Action               = ChosenAction;
	OutCandidate.bClimbAndDrop        = bSelectedClimbAndDrop;
	OutCandidate.FarEdgeLocation      = FarEdge;
	OutCandidate.FarLandingLocation   = FarLanding;
	OutCandidate.ObstacleDepth        = ObstacleDepth;
	OutCandidate.FrontLedgeRootOffset = FrontOffset;
	// Lift the launch by exactly what the authored arc lacks against the REAL obstacle, measured BODY
	// against obstacle -- the two quantities that actually decide whether anything touches. The clearing
	// clips pass their own 100cm barrier with 0-11cm of margin, which is the graze.
	//
	// CLEARING CLIPS ONLY. On a step-on the lowest point over the barrier IS the foot planting on it, so
	// a lift there does not buy clearance, it lifts the plant off the wall. RequiredTopSupport is exactly
	// the "this clip touches the top" flag, and it is measured from the clip rather than guessed from the
	// filename -- one step-on carries a BackLedge window without "V2" in its name.
	const bool bClearingClip = (SelectedTopSupport <= 0.f);
	OutCandidate.ApexLift = (ChosenAction == EAZ_TraversalAction::Hurdle
			&& bClearingClip && AuthoredClearHeight > 0.f)
		// (Lip.Z - FeetZ), NOT OutCandidate.HeightAboveFeet: that member is assigned further down, so
		// reading it here silently used 0 and clamped every lift to zero.
		? FMath::Max(0.f, ((Lip.Z - FeetZ) + HurdleApexClearance) - AuthoredClearHeight)
		: 0.f;
	OutCandidate.HeightAboveFeet  = Lip.Z - FeetZ;
	OutCandidate.Style            = Style;
	OutCandidate.PlantedFoot      = Foot;
	OutCandidate.Approach          = Band.Approach;
	OutCandidate.ApproachSpeed     = ApproachSpeed;
	OutCandidate.Montage           = Montage;
	OutCandidate.StartTime         = StartTime;
	OutCandidate.LedgeDistance     = LedgeDistance;
	OutCandidate.RemainingApproach = RemainingApproach;

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

float UAZ_TraversalComponent::MeasureTopSupportDepth(const FVector& Lip, const FVector& Normal,
	const FCollisionQueryParams& Params) const
{
	const UWorld* World = GetWorld();
	if (!World || TopSupportStep <= 0.f)
	{
		return 0.f;
	}

	// Walk inward from the lip. The surface counts as "still the top" while a downward ray lands within
	// 10cm of the lip height -- the same tolerance the standability check uses, so a platform that steps
	// up or drops away reads as the end of the usable top rather than as more of it.
	float Depth = 0.f;
	for (float Inset = TopSupportStep; Inset <= MaxMeasuredTopSupport; Inset += TopSupportStep)
	{
		const FVector At = Lip - Normal * Inset;
		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit,
			FVector(At.X, At.Y, Lip.Z + 40.f),
			FVector(At.X, At.Y, Lip.Z - 30.f),
			TraceChannel, Params);
		if (!bHit || FMath::Abs(Hit.ImpactPoint.Z - Lip.Z) > 10.f
			|| Hit.ImpactNormal.Z < MinWalkableNormalZ)
		{
			break;
		}
		Depth = Inset;
	}
	return Depth;
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

EAZ_MantleRequestResult UAZ_TraversalComponent::TryStartMantle()
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	const UMoverComponent* ActiveMover = GetOwner() ? GetOwner()->FindComponentByClass<UMoverComponent>() : nullptr;
	const bool bTraversalTagActive = ASC && ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().State_Traversing);
	const bool bTraversalModeActive = ActiveMover && ActiveMover->GetMovementModeName() == TEXT("Traversing");
	if (bTraversalTagActive || bTraversalModeActive)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("[MantleReq] result=BodyBusy reason=active-traversal tag=%d mode=%d"),
			bTraversalTagActive, bTraversalModeActive);
#endif
		// The current action owns this press even when its moving capsule no longer sees a front face.
		// Querying first turned that ordinary geometry miss into permission to activate a second jump.
		return EAZ_MantleRequestResult::BodyBusy;
	}

	// ★ THE DISTINCTION THIS FUNCTION EXISTS TO MAKE.
	// "No ledge here" and "the body is already busy" are opposite answers for the caller: the first is
	// permission to jump, the second is not. Returning a bool conflated them, and Jump read every false as
	// consent — so a committed flinch produced a jump, which is precisely the policy that was supposed to
	// be enforced.
	if (!MantleAbilityClass && !HurdleAbilityClass && !ClimbAbilityClass)
	{
		return EAZ_MantleRequestResult::NoCandidate;
	}

	// The obstacle reaction is asked about AFTER the geometry, not before — see below. Everything
	// IsImpactReactionOwningBody() reports is a COSMETIC wall reaction (EAZ_ObstacleReaction: brace, stop,
	// blocked, stumble, head-hit); a damage flinch, a grab or death never reaches it, they refuse the
	// press by failing TryActivateAbilityByClass further down. So a latched brace is not a claim on the
	// body strong enough to outrank a traversal the geometry allows.

	if (!ASC)
	{
		return EAZ_MantleRequestResult::NoCandidate;
	}

	// ★ GEOMETRY FIRST, REACTION SECOND. Running into a wall is the single most common way to END UP at a
	// ledge, so asking "is a brace playing" before "is there anything to climb" threw away the press in
	// exactly the situation the feature exists for — 42 dropped presses against 20 traversals in one
	// session, seven of them to this gate. Ordering it this way does NOT defer anything: the press is
	// still answered on the frame it arrives, and a reaction still consumes it when there is nothing to
	// traverse. What changes is only which of two simultaneous claims on the body wins, and a cosmetic
	// brace losing to a real ledge is the right answer.
	FAZ_MantleCandidate Candidate;
	if (!FindMantleCandidate(Candidate))
	{
		// Nothing to traverse. NOW the reaction matters: it keeps the body and consumes the press rather
		// than letting Jump read this as consent to jump into the wall it is already braced against.
		if (IsImpactReactionOwningBody())
		{
#if !UE_BUILD_SHIPPING
			UE_LOG(LogTemp, Warning, TEXT("[MantleReq] result=BodyBusy reason=committed-impact-reaction"));
#endif
			return EAZ_MantleRequestResult::BodyBusy;
		}
#if !UE_BUILD_SHIPPING
		// Logged on EVERY rejected press, because the previous logging only began after a SUCCESSFUL
		// activation — which made it impossible to tell a late press from a missing ledge.
		const UMoverComponent* MoverComp = GetOwner() ? GetOwner()->FindComponentByClass<UMoverComponent>() : nullptr;
		const AActor* Owner = GetOwner();
		const APawn* OwnerPawn = Cast<APawn>(Owner);
		UE_LOG(LogTemp, Warning, TEXT("[MantleReq] result=NoCandidate spd=%.0f style=%d foot=%d loc=(%.0f,%.0f,%.0f)"),
			MoverComp ? MoverComp->GetVelocity().Size2D() : 0.0,
			static_cast<int32>(Style), static_cast<int32>(ResolveLivePlantedFoot()),
			OwnerPawn ? OwnerPawn->GetActorLocation().X : 0.0,
			OwnerPawn ? OwnerPawn->GetActorLocation().Y : 0.0,
			OwnerPawn ? OwnerPawn->GetActorLocation().Z : 0.0);
#endif
		return EAZ_MantleRequestResult::NoCandidate;
	}

#if !UE_BUILD_SHIPPING
	// Worth its own line: this is the press that used to be thrown away.
	if (IsImpactReactionOwningBody())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[MantleReq] overriding cosmetic obstacle reaction — action=%d clip=%s dist=%.0f"),
			static_cast<int32>(Candidate.Action), *GetNameSafe(Candidate.Montage), Candidate.LedgeDistance);
	}
#endif

	// ONE ability per action, chosen by what the geometry actually selected. A single Jump press must
	// never reach two independently-activating abilities.
	TSubclassOf<UGameplayAbility> ActionClass = MantleAbilityClass;
	switch (Candidate.Action)
	{
		case EAZ_TraversalAction::Hurdle: ActionClass = HurdleAbilityClass; break;
		case EAZ_TraversalAction::Climb:  ActionClass = ClimbAbilityClass;  break;
		default:                          ActionClass = MantleAbilityClass; break;
	}
	if (!ActionClass)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("[MantleReq] result=NoCandidate reason=no-ability-class-for-action=%d"),
			static_cast<int32>(Candidate.Action));
#endif
		return EAZ_MantleRequestResult::NoCandidate;
	}

	// The ability reads PendingCandidate inside its ActivateAbility, which runs synchronously below.
	PendingCandidate = Candidate;
	if (!ASC->TryActivateAbilityByClass(ActionClass))
	{
		ClearPendingCandidate();
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("[MantleReq] result=BodyBusy reason=ability-blocked-or-ungranted"));
#endif
		// A geometrically valid mantle that GAS refused means something else owns the body (its own
		// State.Traversing, a grab, death). That is BodyBusy, not consent to jump.
		return EAZ_MantleRequestResult::BodyBusy;
	}
	return EAZ_MantleRequestResult::Started;
}
