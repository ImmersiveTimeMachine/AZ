// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AZ_GA_Traversal.h"
#include "AZ_GA_Hurdle.generated.h"

/**
 * UAZ_GA_Hurdle — the HURDLE action: cross an obstacle and land beyond it.
 *
 * Execution is entirely UAZ_GA_Traversal's. This class only describes the action, and the description
 * differs from mantle in one structural way: hurdle registers up to THREE warp targets instead of one.
 *
 *   FrontLedge — launch, and the one target that is NOT a point the geometry query returns directly: it
 *                is the obstacle's front face at GROUND level. Every clip is authored with that face at
 *                the animation origin, so with a Static warp point at the origin each of the two
 *                FrontLedge windows derives its own root destination — the first still on the ground a
 *                third of a metre out, the second airborne at the face. `attach` is un-keyed on these
 *                clips, so the Bone provider they shipped with degenerated to identity and drove BOTH
 *                windows onto one transform, freezing the character at the takeoff apex for up to 0.4s.
 *   BackLedge  — the far top edge, used only by step-on clips. Aimed at the REAL edge, so it adapts.
 *   BackFloor  — the validated landing beyond. Also real, also adaptive.
 *
 * All three are registered unconditionally: a target a clip has no window for is inert, and that is
 * cheaper than teaching this class which clips carry which windows.
 */
UCLASS()
class AZ_API UAZ_GA_Hurdle : public UAZ_GA_Traversal
{
	GENERATED_BODY()

protected:
	virtual bool BuildTraversalRequest(FAZ_TraversalRequest& OutRequest) override;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Hurdle")
	FName FrontLedgeTargetName = FName("FrontLedge");

	/** The LAUNCH endpoint: the same front face at ground level, plus ApexLift. Separate from FrontLedge
	 *  because a clip's early warp window ends with the character still ON the ground and must stay there
	 *  — lifting that one would float the run-up. Only windows whose authored root end is already
	 *  airborne carry this name. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Hurdle")
	FName FrontLedgeApexTargetName = FName("FrontLedgeApex");

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Hurdle")
	FName BackLedgeTargetName = FName("BackLedge");

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Hurdle")
	FName BackFloorTargetName = FName("BackFloor");
};
