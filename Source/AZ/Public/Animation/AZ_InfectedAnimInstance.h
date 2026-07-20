// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "AZ_InfectedAnimInstance.generated.h"

class AAZ_PawnMoverInfectedCharacter;
class UAZ_PawnMoverComponent;

/**
 * UAZ_InfectedAnimInstance — slim CLASSIC locomotion driver for the Mover-driven infected ("Chalkie") NPC.
 *
 * Option B (see project_npc_foundation): NPCs use a classic state-machine / blendspace ABP, NOT the hero's
 * Motion Matching pipeline. This class was GUTTED from a verbatim copy of the hero MM anim instance
 * (UAZ_MoverAnimInstance) down to just the locomotion DRIVER variables a classic AZ_ABP_Chalkie reads.
 * Removed: PoseSearch / Chooser / Trajectory / BlendStack / hybrid-jump machinery (all MM-only).
 *
 * It is the GENERIC infected driver (reusable by future infected variants); AZ_ABP_Chalkie is the specific
 * creature asset that uses it. Anim instance != movement backend: the pawn keeps the full Mover stack — this
 * class just READS the Mover component's velocity (physics -> anim) and exposes a couple of floats/bools.
 *
 * Per tick (game thread): read GetMoverComponent()->GetVelocity() -> GroundSpeed + bIsMoving. That is the
 * whole first pass. Gameplay state (attacks / death / phase) is GAS-driven (tags + montages into a slot),
 * NOT polled here. Turn-in-place yaw-delta, foot contact, and a combat overlay can be added incrementally.
 */
UCLASS()
class AZ_API UAZ_InfectedAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	/** 2D ground speed (cm/s) read from the Mover component each tick. Drives the locomotion blendspace. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Infected|Locomotion")
	float GroundSpeed = 0.f;

	/** True when GroundSpeed exceeds MoveSpeedThreshold — the idle vs moving split for the state machine. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Infected|Locomotion")
	bool bIsMoving = false;

	/** Speed (cm/s) below which the infected is considered idle. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Infected|Locomotion", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MoveSpeedThreshold = 5.f;

	// AI phase, mirrored from the pawn's ASC State.Infected.* tags (NOT the AI controller — controllers exist
	// only on the server; the ASC tags replicate, so these bools are valid on client-side Chalkies in co-op).
	// Drives alert/scream telegraphs and wary-vs-committed posture overlays in AZ_ABP_Chalkie (anim pass).

	/** State.Infected.Alerted — reaction beat / investigating: wary posture, look-around. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Infected|Phase")
	bool bIsAlerted = false;

	/** State.Infected.Aggressive — committed to a target: chase posture, scream on the rising edge. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Infected|Phase")
	bool bIsAggressive = false;

	/** Cached on init — the owning infected pawn (saves a per-tick TryGetPawnOwner cast). */
	UPROPERTY(Transient)
	TObjectPtr<AAZ_PawnMoverInfectedCharacter> Cached_Pawn;

	/** Cached on init — the pawn's Mover component (the source of GetVelocity()). */
	UPROPERTY(Transient)
	TObjectPtr<UAZ_PawnMoverComponent> Cached_MoverComponent;
};
