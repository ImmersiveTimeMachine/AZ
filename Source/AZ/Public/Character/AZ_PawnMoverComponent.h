// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "AZ_PawnMoverComponent.generated.h"

/**
 * UAZ_PawnMoverComponent — thin UCharacterMoverComponent subclass.
 *
 * Sole reason for existing right now: call RefreshSharedSettings() in the ctor so
 * the CDO's SharedSettings array is populated with every UCommonLegacyMovementSettings
 * instance the default modes (Walking / Falling / Flying) need. Without this,
 * a fresh Blueprint child of the pawn class is born with an empty SharedSettings
 * array baked into its serialized state — at PIE the modes' OnRegistered ensure-fails:
 *   "Failed to find instance of CommonLegacyMovementSettings on …DefaultWalkingMode"
 *
 * UMoverComponent::RefreshSharedSettings is protected, so we expose it here only by
 * subclassing and invoking it. PostInitProperties on instances also calls it, but
 * an instanced subobject duplicated from a BP CDO does not re-run PostInitProperties
 * — duplicates inherit whatever the parent CDO has serialized.
 *
 * Also owns the GAIT-SCALED JUMP (v2): overrides Jump() so the impulse depends on the current gait.
 */
UCLASS(ClassGroup=(Movement), meta=(BlueprintSpawnableComponent))
class AZ_API UAZ_PawnMoverComponent : public UCharacterMoverComponent
{
	GENERATED_BODY()

public:
	UAZ_PawnMoverComponent();

	/** Gait-scaled jump impulse. The engine's flat CommonLegacyMovementSettings::JumpUpwardsSpeed (500)
	 *  gives a walk hop the same ~1s air time — and therefore run-like distance — as a run leap.
	 *  Air time = 2*v/gravity; height = v²/(2*gravity). Walk 350 → ~0.71s / ~62cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Jump", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float JumpUpwardsSpeedWalk = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Jump", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float JumpUpwardsSpeedRun = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Jump", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float JumpUpwardsSpeedSprint = 550.f;

	/** HYBRID JUMP prototype (A/B toggle).
	 *  true  = RM rise → physics fall: Jump() enters the RMAction mode; the gait/foot Start clip's root
	 *          motion drives the capsule through anticipation + rise (perfect anim/capsule sync, per-gait
	 *          arcs baked), and RMAction hands itself to Falling at the clip apex (terrain-adaptive descent,
	 *          air control, land MM on real contact).
	 *  false = pure physics impulse with the gait-scaled speeds above.
	 *  NOTE: the hybrid expects the chooser takeoff rows at StartTime=0 (anticipation IS the sync); the
	 *  physics branch expects launch-frame StartTimes (0.175/0.15/0.15/0.117/0.217) — see memory. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Jump")
	bool bUseHybridJump = true;

	/** Engine UCharacterMoverComponent::Jump() queues FJumpImpulseEffect with the flat settings value;
	 *  this override applies the per-gait speed above. Reached via the bHandleJump p`re-sim path
	 *  (OnMoverPreSimulationTick → CanActorJump → Jump). */
	virtual bool Jump() override;

	/**
	 * THE root-motion bridge. On a Mover pawn a bare Montage_Play animates the mesh in place — the clip's
	 * root delta only reaches the CAPSULE while an FLayeredMove_RootMotionAttribute is live. Every montage
	 * that is supposed to move the body (attacks, hit-reacts, step-backs, death slides) needs this.
	 *
	 * Cancel-and-replace, never stack: a second call supersedes the first. That matters because the move
	 * is OverrideAll — while one is live past its montage, RootMotionFromEverything keeps writing a ~zero
	 * root delta from whatever in-place loco clips are playing, and the override faithfully applies it:
	 * the pawn freezes. (Intermittent by nature — it depends on what else is playing — which is exactly
	 * why it must not be left to call sites to get right.)
	 *
	 * Seconds is the GAMEPLAY BEAT, not the clip length. They are frequently different: the Chalkie's
	 * KnockBack_Chase clips recoil for ~45% of their length and then walk back in, and a lunging punch
	 * keeps travelling through its recovery tail. Pass how long the capsule should follow the animation,
	 * and cut the montage at the same moment.
	 *
	 * Returns a GENERATION id (0 = nothing queued: no-op seconds or sim proxy). Hold onto it and end the
	 * drive with ReleaseRootMotion(Gen) — never with a raw CancelFeaturesWithTag, which kills WHOEVER'S
	 * move is live, including a successor's. (Concretely: a punch's EndAbility firing after a flinch has
	 * started must not cancel the flinch's knockback. Generation matching makes that ordering-proof.)
	 *
	 * Simulated proxies are skipped — they follow the replicated transform instead.
	 *
	 * NOTE: deliberately does NOT cover three special cases that are not this pattern —
	 * AZ_HeroPawn's permanent (DurationMs = -1) bridge, AZ_MoverAnimInstance's per-frame recomputed
	 * transition remainder, and FLayeredMove_AZ_GrabAnchor (a different layered move entirely).
	 */
	uint64 DriveRootMotion(float Seconds);

	/** End a drive started by DriveRootMotion — no-ops if that drive has already been superseded (or
	 *  Generation is 0), so a stale caller can never cancel a newer owner's move. */
	void ReleaseRootMotion(uint64 Generation);
	
protected:
	// Override OnRegister so we can populate SharedSettings BEFORE the parent calls
	// FindAndRegisterDefaults, which triggers each mode's OnRegistered → lookup of
	// CommonLegacyMovementSettings. NewObject is forbidden inside ctors, allowed here.
	virtual void OnRegister() override;
	
	virtual void OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd) override;

private:
	/** Monotonic id of the most recent DriveRootMotion call — ReleaseRootMotion only cancels if it still
	 *  holds the latest. Never reset (a uint64 will not wrap in a play session). */
	uint64 RootMotionGeneration = 0;
};
