// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GA_Traversal.h"
#include "AZ_GA_Mantle.generated.h"

/**
 * UAZ_GA_Mantle — the MANTLE action: climb onto a ledge and stay on top.
 *
 * Execution lives entirely in UAZ_GA_Traversal (Traversing mode, root-motion drive, scoped collision
 * exemption, montage outcomes, watchdog, release). This class only DESCRIBES the action: which clip, from
 * what time, and where the single FrontLedge target sits.
 *
 * Deliberately kept as its own class with its original name and path so existing Blueprint parents,
 * ability grants and asset references continue to resolve after the shared executor was extracted.
 *
 * NOT input-bound. UAZ_GA_PawnJump owns Input.Action.Jump and routes here through
 * UAZ_TraversalComponent::TryStartMantle, which distinguishes Started / NoCandidate / BodyBusy so a
 * committed reaction cannot be mistaken for permission to jump.
 */
UCLASS()
class AZ_API UAZ_GA_Mantle : public UAZ_GA_Traversal
{
	GENERATED_BODY()

protected:
	/** Mantle needs exactly one target: the physical front lip. The montages' warp point is authored as a
	 *  Static transform at the animation's contact anchor, so this is the LIP — not the destination
	 *  capsule; the modifier composes the two into the root's end transform itself. */
	virtual bool BuildTraversalRequest(FAZ_TraversalRequest& OutRequest) override;

	/** Must match the warp windows authored on the mantle montages. A mismatch is inert, not an error. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Mantle")
	FName WarpTargetName = FName("FrontLedge");

	/** Lift off the lip so the target is not exactly coplanar with the surface. GASP authored 0.5cm. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Mantle", meta = (ForceUnits = "cm"))
	float WarpTargetZOffset = 0.5f;
};
