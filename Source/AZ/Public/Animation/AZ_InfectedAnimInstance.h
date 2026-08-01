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
public:
	/** Movement latch for GA_MeleeAttack's idle-vs-moving montage pick (mirror of the hero's ChooserContext read). */
	bool IsMoving() const { return bIsMoving; }

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

	// ========================================
	// Grab hand-IK — mirror of the hero's block in UAZ_MoverAnimInstance
	// ========================================
	// While this Chalkie holds prey, its hands are pinned onto grip sockets authored on the VICTIM's
	// skeleton. Two TwoBoneIK nodes near the AnimGraph output (after the DefaultSlot node, or the montage
	// overwrites them) bind to the targets below, with GrabIKAlpha as their alpha. Gathered on the game
	// thread because reading another actor's socket transform is not thread-safe-update legal.

	/** 0..1 ramp, driven to 1 while a grab is live. Alpha for both TwoBoneIK nodes. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Infected|GrabIK")
	float GrabIKAlpha = 0.f;

	/** World-space goal for hand_l — the victim's GrabIKPreySocketForHandL socket. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Infected|GrabIK")
	FVector GrabIKTarget_HandL = FVector::ZeroVector;

	/** World-space goal for hand_r. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Infected|GrabIK")
	FVector GrabIKTarget_HandR = FVector::ZeroVector;

	/** Alpha blend speed (per second) — how fast the hands commit to / release the grip. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Infected|GrabIK", meta = (ClampMin = "0.1"))
	float GrabIKBlendSpeed = 8.f;

	/** Socket on the PREY's mesh each hand reaches for. Straight mapping (L->L, R->R) to match the
	 *  socket naming, where GrabIK_HandL means "the grip the partner's LEFT hand takes".
	 *  NOTE the hero's equivalent pair is CROSSED and needs reconciling with this convention. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Infected|GrabIK")
	FName GrabIKPreySocketForHandL = TEXT("GrabIK_HandL");

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Infected|GrabIK")
	FName GrabIKPreySocketForHandR = TEXT("GrabIK_HandR");

	/** Cached on init — the owning infected pawn (saves a per-tick TryGetPawnOwner cast). */
	UPROPERTY(Transient)
	TObjectPtr<AAZ_PawnMoverInfectedCharacter> Cached_Pawn;

	/** Cached on init — the pawn's Mover component (the source of GetVelocity()). */
	UPROPERTY(Transient)
	TObjectPtr<UAZ_PawnMoverComponent> Cached_MoverComponent;
};
