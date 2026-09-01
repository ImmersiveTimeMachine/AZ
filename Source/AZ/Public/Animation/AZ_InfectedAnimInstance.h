// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/TrajectoryTypes.h"   // FTransformTrajectory — PSI collector feed
#include "AZ_InfectedAnimInstance.generated.h"

class AAZ_CmcInfectedCharacter;
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

	// ========================================
	// PSI trajectory — feed for the AnimGraph PoseHistory collector
	// ========================================
	/** Mesh-component world-space trajectory: ~1s of sampled history + a few constant-velocity future
	 *  samples. The AZ_ABP_Chalkie PoseHistory collector binds its TransformTrajectory pin to this —
	 *  on a Mover pawn the collector's own bGenerateTrajectory is INERT (the engine path hard-requires
	 *  ACharacter + UCharacterMovementComponent, PoseSearchTrajectoryLibrary.cpp:48-71), and without a
	 *  trajectory every MotionMatchMulti this Chalkie joins reads identity root transforms and costs of
	 *  ~(distance-to-origin)^2. Built each NativeUpdateAnimation; see project_grab_grapple_design. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Infected|PSI")
	FTransformTrajectory Trajectory;

private:
	/** History ring for Trajectory. TimeInSeconds holds ABSOLUTE game-time seconds until emission
	 *  (rebased to <=0 when copied out; float absolute drifts ~2ms after 8h — irrelevant here). */
	TArray<FTransformTrajectorySample> TrajectoryHistoryRing;
	/** Last game time a ring sample was taken (throttles to ~30Hz independent of anim tick rate). */
	double LastTrajectorySampleTime = -1.0;

protected:

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

	/** Fraction of the arm chain the IK may use before the target is clamped onto the prey's body
	 *  surface (shared math: UAZ_MoverAnimInstance::ResolveGrabIKTarget). The Chalkie arms took the
	 *  worse retarget hit — hands measured 17-19cm short of their grips at the worst wrestle frames —
	 *  so this side is where the clamp earns its keep. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Infected|GrabIK", meta = (ClampMin = "0.5", ClampMax = "1"))
	float GrabIKReachScale = 0.97f;

	/** Smoothing on the FINAL IK target (per second); snapped on the grab's first frame. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Infected|GrabIK", meta = (ClampMin = "1"))
	float GrabIKTargetInterpSpeed = 25.f;

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

	/** [SPIKE: spike/cmc-backport] The CMC (v3) Chalkie, when this instance animates that generation
	 *  instead. Exactly ONE of Cached_Pawn / Cached_CmcPawn is set; every read branches on which. */
	UPROPERTY(Transient)
	TObjectPtr<AAZ_CmcInfectedCharacter> Cached_CmcPawn;

	/** Per-grab clamp instrumentation (measure rule): how many frames a grip was beyond reach and the
	 *  worst deficit, logged once when the grab releases. Answers "are the hands still missing" with a
	 *  number instead of a feeling. */
	int32 GrabIKClampedFrames = 0;
	float GrabIKMaxDeficit = 0.f;

	/** Cached on init — the pawn's Mover component (the source of GetVelocity()). */
	UPROPERTY(Transient)
	TObjectPtr<UAZ_PawnMoverComponent> Cached_MoverComponent;
};
