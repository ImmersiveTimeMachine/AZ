// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "MovementMode.h"   // UBaseMovementMode
#include "AZ_PawnMovementMode_RMAction.generated.h"

struct FMoverSimContext;
struct FMoverTickStartData;
struct FMoverTimeStep;
struct FProposedMove;
struct FSimulationTickParams;
struct FMoverTickEndData;

/**
 * UAZ_PawnMovementMode_RMAction — a gravity-free, floor-snap-free movement mode whose ONLY job is to
 * let an animation root-motion clip drive the capsule through a self-contained action (a jump-in-place,
 * later a vault / mantle / traversal) with its FULL 3D translation INCLUDING the vertical lift.
 *
 * WHY THIS EXISTS (the bug it fixes):
 *   The RM jump clip (RTG_RM_Jump_place_ALL) carries vertical Z in its root motion. We bridge that to the
 *   Mover capsule via FLayeredMove_RootMotionAttribute (MixMode = OverrideAll), which proposes the clip's
 *   full XYZ velocity each tick. But while the pawn is in Walking mode, UWalkingMode::SimulationTick calls
 *   TryMoveToAdjustHeightAboveFloor every tick (WalkingMode.cpp:271) and snaps the capsule straight back
 *   down to the floor — eating the jump's vertical lift. The capsule never leaves the ground, so the
 *   air states never trigger. (Routing the jump through the engine Falling mode does NOT help either: that
 *   mode adds Mover.SkipVerticalAnimRootMotion, which MovementModeStateMachine.cpp:327-332 uses to downgrade
 *   the layered move from OverrideAll to OverrideAllExceptVerticalVelocity — stripping the Z and replacing it
 *   with gravity. Either way the lift is lost.)
 *
 * WHAT THIS MODE DOES DIFFERENTLY:
 *   - GenerateMove produces ZERO velocity of its own — no gravity. On any tick the root-motion attribute is
 *     not contributing (e.g. the jump-press edge frame, or a momentary missing attribute) the capsule HOLDS
 *     in place instead of falling. When the attribute IS contributing, its OverrideAll fully replaces this.
 *   - SimulationTick performs a swept TrySafeMoveUpdatedComponent by the proposed (root-motion) delta —
 *     collision against walls/ceilings still resolves so the capsule can't tunnel — but performs NO floor
 *     query and NO height-adjust, so the vertical component survives.
 *   - It carries NEITHER skip tag (SkipAnimRootMotion / SkipVerticalAnimRootMotion), so the layered move
 *     keeps full OverrideAll incl. Z.
 *
 * ENTRY / EXIT:
 *   Orchestrated by UAZ_MoverAnimInstance, which holds the TransitionToInAir SM phase for the jump clip's
 *   exact length. On the phase ENTER edge it QueueNextMode("RMAction"); on the EXIT edge it
 *   QueueNextMode("Walking"). The clip's own root motion both lifts the capsule and sets it back down, so by
 *   the hand-off the capsule is at ground level and Walking re-finds the floor on Activate. The phase length
 *   is bounded (clip length, or a ~1s safety), so even a missing clip recovers — there is no way to get
 *   stuck floating. Registered at runtime in UAZ_PawnMoverComponent::OnRegister (not in the BP mode map).
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "AZ Pawn Movement Mode - RM Action"))
class AZ_API UAZ_PawnMovementMode_RMAction : public UBaseMovementMode
{
	GENERATED_BODY()

public:
	UAZ_PawnMovementMode_RMAction();

	/** HYBRID JUMP (RM rise → physics fall): when true, this mode watches the proposed root-motion
	 *  vertical delta and switches itself to "Falling" at the clip apex (Z-delta non-positive for 2
	 *  consecutive ticks after a net rise > 10cm). The descent then belongs to physics — terrain-adaptive,
	 *  air-controllable, landing on real floor contact. Set FALSE for self-contained actions that must
	 *  play to completion (vault / mantle / jump-in-place). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|RMAction")
	bool bHandOffToFallingAtApex = true;

	/** Safety: if no apex is detected within this time after entering the mode, hand off to Falling
	 *  anyway (covers a missing/non-rising clip — there is no way to get stuck floating). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|RMAction", meta = (ClampMin = "0", ForceUnits = "s"))
	float MaxRiseSeconds = 1.0f;

	/** Produce no motion of our own (zero velocity, no gravity). The root-motion-attribute layered move
	 *  (OverrideAll) supplies the real motion when it is active; on ticks where it is not, zero velocity
	 *  holds the capsule rather than letting it fall. */
	virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState,
		const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;

	/** Swept move by the proposed (root-motion) delta with NO floor-snap and NO gravity. Collision with
	 *  blocking geometry still resolves; the vertical lift survives. */
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

private:
	// ---- Apex-detection sim state (hybrid jump) ----
	// Lives on the mode instance, NOT in the sync state — NOT rollback-safe under NetworkPrediction
	// resimulation (prototype scope; migrate into a sync-state data block if MP resim jitter shows).
	// Fresh-activation is detected by a gap in sim time instead of lifecycle hooks.
	float NetRiseCm = 0.f;
	int32 NonPositiveZDeltaTicks = 0;
	float RiseElapsedMs = 0.f;
	float ExpectedNextTickStartMs = -1.f;
};
