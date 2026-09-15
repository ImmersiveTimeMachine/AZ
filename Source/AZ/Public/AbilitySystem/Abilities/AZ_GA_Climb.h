// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GA_Traversal.h"
#include "AZ_GA_Climb.generated.h"

/**
 * UAZ_GA_Climb — the CLIMB action: get onto a ledge too high to mantle.
 *
 * Execution is entirely UAZ_GA_Traversal's, and deliberately so: climb needs the same Traversing mode, the
 * same generation-scoped root motion, the same collision exemption and the same event-driven handoff that
 * mantle and hurdle already use. What it does NOT need is a second ownership context — every one of the six
 * authored clips carries the whole action, approach through pull-up to standing, so there is no reach phase
 * to hand off from. Verified rather than assumed, because "Start" in the source filename suggests otherwise:
 * all six end with the root at z 247.9 and both feet on the platform, none of them ends hanging.
 *
 * ONE warp target, the same shape as mantle:
 *
 *   FrontLedge — the REAL front top edge of the platform. The clips are authored with that edge at their
 *                own origin (front face at y=0, top at z=247.9), which is what the per-clip Static warp
 *                point on each montage states, so the modifier derives the root destination for each
 *                window from its own window-end transform. Anchoring at the LEDGE rather than at the
 *                ground is the difference that matters here: a climb is a reach, so the hands must meet
 *                the edge even when the platform is not exactly the authored height.
 *
 * The destination is the standing position on top — the same capsule the detector proved was free — so
 * releasing into Walking at the end needs no special case.
 */
UCLASS()
class AZ_API UAZ_GA_Climb : public UAZ_GA_Traversal
{
	GENERATED_BODY()

protected:
	virtual bool BuildTraversalRequest(FAZ_TraversalRequest& OutRequest) override;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Climb")
	FName FrontLedgeTargetName = FName("FrontLedge");
};
