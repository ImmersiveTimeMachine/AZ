// Copyright Artur. AZ project.

#include "AbilitySystem/Abilities/AZ_GA_Climb.h"

#include "Animation/AnimMontage.h"
#include "Character/AZ_TraversalComponent.h"

bool UAZ_GA_Climb::BuildTraversalRequest(FAZ_TraversalRequest& OutRequest)
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UAZ_TraversalComponent* Traversal =
		Avatar ? Avatar->FindComponentByClass<UAZ_TraversalComponent>() : nullptr;
	if (!Traversal)
	{
		return false;
	}

	// Copied, not referenced: the shared release clears the component's pending candidate, and EndAbility
	// can run re-entrantly from the executor's own failure paths.
	const FAZ_MantleCandidate Candidate = Traversal->GetPendingCandidate();
	if (!Candidate.IsValid() || Candidate.Action != EAZ_TraversalAction::Climb)
	{
		return false;
	}
	// A climb whose terminal position was never measured must not start. The detector proves capsule
	// clearance there; normal climbs also require support, while the explicit climb-and-drop prototype
	// hands that unsupported terminal position to Falling instead of pretending it is standing ground.
	if (Candidate.LandingTransform.GetLocation().IsNearlyZero())
	{
		return false;
	}

	OutRequest.Montage   = Candidate.Montage;
	OutRequest.StartTime = Candidate.StartTime;
	OutRequest.TaskName  = FName("Climb");
	OutRequest.SuccessMovementMode = Candidate.bClimbAndDrop ? FName("Falling") : FName("Walking");

	// The one primitive the capsule is allowed through — the face it is climbing.
	OutRequest.CollisionExemption = Candidate.TargetComponent;

	// FrontLedge is the REAL front top edge. Each montage carries its own measured Static warp point at
	// that edge in animation space, so every window resolves its own root destination from it — which is
	// what keeps the two-window walk clips from collapsing both windows onto one transform the way the
	// un-keyed `attach` bone provider did.
	FAZ_TraversalWarpTarget FrontLedge;
	FrontLedge.Name = FrontLedgeTargetName;
	FrontLedge.Transform = FTransform(
		FRotationMatrix::MakeFromX(-Candidate.LedgeNormal).Rotator(),
		Candidate.LedgeLocation);
	OutRequest.WarpTargets.Add(FrontLedge);

#if !UE_BUILD_SHIPPING
	// warpErr is the number that predicts a lurch: how far the authored approach is from the real distance,
	// i.e. how much correction warping has been asked to absorb.
	const FVector LandLoc = Candidate.LandingTransform.GetLocation();
	UE_LOG(LogTemp, Warning,
		TEXT("[Climb] approach=%d spd=%.0f style=%d foot=%d clip=%s startT=%.2f dist=%.0f authored=%.0f warpErr=%+.0f lip=(%.0f,%.0f,%.0f) n=(%.2f,%.2f) hAboveFeet=%.1f land=(%.0f,%.0f,%.0f)"),
		static_cast<int32>(Candidate.Approach), Candidate.ApproachSpeed,
		static_cast<int32>(Candidate.Style), static_cast<int32>(Candidate.PlantedFoot),
		*GetNameSafe(Candidate.Montage), Candidate.StartTime,
		Candidate.LedgeDistance, Candidate.RemainingApproach,
		Candidate.RemainingApproach - Candidate.LedgeDistance,
		Candidate.LedgeLocation.X, Candidate.LedgeLocation.Y, Candidate.LedgeLocation.Z,
		Candidate.LedgeNormal.X, Candidate.LedgeNormal.Y, Candidate.HeightAboveFeet,
		LandLoc.X, LandLoc.Y, LandLoc.Z);
#endif

	return true;
}
