// Copyright Artur. AZ project.

#include "AbilitySystem/Abilities/AZ_GA_Hurdle.h"

#include "Animation/AnimMontage.h"
#include "Character/AZ_TraversalComponent.h"

bool UAZ_GA_Hurdle::BuildTraversalRequest(FAZ_TraversalRequest& OutRequest)
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UAZ_TraversalComponent* Traversal =
		Avatar ? Avatar->FindComponentByClass<UAZ_TraversalComponent>() : nullptr;
	if (!Traversal)
	{
		return false;
	}

	const FAZ_MantleCandidate Candidate = Traversal->GetPendingCandidate();
	if (!Candidate.IsValid() || Candidate.Action != EAZ_TraversalAction::Hurdle)
	{
		return false;
	}
	// A hurdle without a measured landing must never reach here — refusing is the whole point of the
	// far-side validation, and a zero landing would send the character at the world origin.
	if (Candidate.FarLandingLocation.IsNearlyZero())
	{
		return false;
	}

	OutRequest.Montage            = Candidate.Montage;
	OutRequest.StartTime          = Candidate.StartTime;
	OutRequest.TaskName           = FName("Hurdle");
	OutRequest.CollisionExemption = Candidate.TargetComponent;

	// Facing is shared by all three: the character crosses along -Normal throughout.
	const FRotator Facing = FRotationMatrix::MakeFromX(-Candidate.LedgeNormal).Rotator();

	// FrontLedge: the obstacle's front face AT GROUND LEVEL. Ground level is derived from the measured
	// height above the character's ACTUAL supporting feet, not world Z, so an obstacle standing on a
	// raised surface still resolves correctly.
	const FVector FrontFaceGround(
		Candidate.LedgeLocation.X,
		Candidate.LedgeLocation.Y,
		Candidate.LedgeLocation.Z - Candidate.HeightAboveFeet);

	FAZ_TraversalWarpTarget Front;
	Front.Name = FrontLedgeTargetName;
	// The OBSTACLE, not the root. Every hurdle clip is authored with the barrier's front face at the
	// animation origin and its top at z=100 (the same convention mantle's calibrated warp point states
	// explicitly), so the front face at ground level IS the warp point, and each window derives its own
	// root destination from the root transform at its own end.
	//
	// This used to carry a per-clip baked root offset instead, because `attach` is un-keyed on these
	// clips and the Bone provider silently degenerates to identity. That degeneration is the whole bug:
	// EVERY clip has TWO FrontLedge windows whose authored root ends differ enormously -- the first on
	// the ground a third of a metre out (y-36 z2), the second airborne at the face (y-1 z79) -- and with
	// an identity offset BOTH were driven onto the SAME world transform. The character was yanked to the
	// takeoff apex up to 0.4s early and then held frozen there until the second window closed, which is
	// the float-and-graze that reads as unrealistic. Fixing the provider on the assets restores the two
	// destinations; the offset is then implicit and no longer belongs here.
	Front.Transform = FTransform(Facing, FrontFaceGround);
	OutRequest.WarpTargets.Add(Front);

	// FrontLedgeApex: the same point raised by whatever the authored arc lacks against THIS obstacle.
	// Warping only corrects at window ends and the motion after the last one is authored and untouched,
	// so raising this endpoint raises the whole remaining arc — and the barrier crossing happens before
	// the BackFloor window opens, so the lift is carried at full value exactly where it is needed.
	FAZ_TraversalWarpTarget Apex;
	Apex.Name = FrontLedgeApexTargetName;
	Apex.Transform = FTransform(Facing, FrontFaceGround + FVector(0.f, 0.f, Candidate.ApexLift));
	OutRequest.WarpTargets.Add(Apex);

	// BackLedge: the REAL far top edge. Step-on clips plant a foot here; clearing clips have no window for
	// it and simply ignore it.
	FAZ_TraversalWarpTarget Back;
	Back.Name = BackLedgeTargetName;
	Back.Transform = FTransform(Facing, Candidate.FarEdgeLocation);
	OutRequest.WarpTargets.Add(Back);

	// BackFloor: the validated landing. Registered as the capsule-centre standing position the detector
	// proved was free; the modifier drives the root onto it.
	FAZ_TraversalWarpTarget Landing;
	Landing.Name = BackFloorTargetName;
	Landing.Transform = FTransform(Facing, Candidate.FarLandingLocation);
	OutRequest.WarpTargets.Add(Landing);

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Warning,
		TEXT("[Hurdle] approach=%d spd=%.0f foot=%d clip=%s startT=%.2f dist=%.0f authored=%.0f warpErr=%+.0f depth=%.0f hAboveFeet=%.0f lift=%.0f faceGround=(%.0f,%.0f,%.0f) farEdge=(%.0f,%.0f,%.0f) landing=(%.0f,%.0f,%.0f)"),
		static_cast<int32>(Candidate.Approach), Candidate.ApproachSpeed,
		static_cast<int32>(Candidate.PlantedFoot), *GetNameSafe(Candidate.Montage), Candidate.StartTime,
		Candidate.LedgeDistance, Candidate.RemainingApproach,
		Candidate.RemainingApproach - Candidate.LedgeDistance, Candidate.ObstacleDepth,
		Candidate.HeightAboveFeet, Candidate.ApexLift,
		FrontFaceGround.X, FrontFaceGround.Y, FrontFaceGround.Z,
		Candidate.FarEdgeLocation.X, Candidate.FarEdgeLocation.Y, Candidate.FarEdgeLocation.Z,
		Candidate.FarLandingLocation.X, Candidate.FarLandingLocation.Y, Candidate.FarLandingLocation.Z);
#endif

	return true;
}
