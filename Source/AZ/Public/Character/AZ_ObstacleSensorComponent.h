// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Animation/AZ_LocomotionTypes.h"   // EAZ_ObstacleReaction
#include "AZ_ObstacleSensorComponent.generated.h"

class UMoverComponent;
class UCapsuleComponent;

/**
 * UAZ_ObstacleSensorComponent — forward obstacle sensor for the locomotion pawn.
 *
 * Each GATED frame (grounded + forward movement INTENT) it casts THREE short forward sphere-sweeps at LOW / MID
 * / HIGH heights along the movement direction (up to TraceDistance). Reading WHICH body band is blocked (a
 * vertical, opposing normal) classifies the reaction directly — no top-edge height guessing:
 *   - MID (chest) blocked     -> a WALL          -> Brace (fast Run2Wall)
 *   - mid clear, LOW (knee)    -> a low barrier    -> Stumble  (low-band hit, e.g. KB_LowRight)
 *   - mid+low clear, HIGH(head) -> an overhead beam -> HeadHit (high-band hit, e.g. KB_HighFront)
 * IMPACT-ONLY: the entry one-shot fires on a FAST rising-edge hit and clears to None — there is no sustained
 * "Blocked" state. "Can't move into the wall" is owned upstream by UAZ_MovementDirectionCapabilityComponent
 * (it clamps the move intent), so this sensor is purely the cosmetic flinch layer. Resolves a single
 * EAZ_ObstacleReaction (CurrentReaction) the AnimInstance copies into ChooserContext.Reaction.
 *
 * Pawn-driven so it works on ANY geometry with no per-obstacle setup; the hit actor is exposed for optional
 * semantics later. Cheap: gated, 3 short sweeps per moving frame. First leg of project_traversal_system.
 */
UCLASS(ClassGroup = (AZ), meta = (BlueprintSpawnableComponent))
class AZ_API UAZ_ObstacleSensorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAZ_ObstacleSensorComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

public:
	// ---- Outputs (read by UAZ_MoverAnimInstance each tick) ----
	/** A vertical, reactable obstacle is in front this frame (any band hit). */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Obstacle|Out")
	bool bObstacleAhead = false;
	/** The reaction resolved this frame — Brace (wall) / Stumble (low) / HeadHit (overhead) / Blocked / None.
	 *  The AnimInstance copies this into ChooserContext.Reaction. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Obstacle|Out")
	EAZ_ObstacleReaction CurrentReaction = EAZ_ObstacleReaction::None;
	/** Distance (cm) to the PRIMARY band's hit this frame (0 when none). */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Obstacle|Out", meta = (ForceUnits = "cm"))
	float ObstacleDistance = 0.f;
	/** Surface normal of the primary band's hit this frame. */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Obstacle|Out")
	FVector ObstacleNormal = FVector::ZeroVector;
	/** Speed (cm/s) the pawn is closing on the obstacle face (velocity projected onto -normal). */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Obstacle|Out", meta = (ForceUnits = "cm/s"))
	float ObstacleClosingSpeed = 0.f;
	/** The actor that was hit (for reading optional obstacle semantics later). */
	UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|Obstacle|Out")
	TWeakObjectPtr<AActor> ObstacleActor;

	// ---- Trace params ----
	/** Forward look-ahead range of each band sweep (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Obstacle|Trace", meta = (ClampMin = "0", ForceUnits = "cm"))
	float TraceDistance = 100.f;
	/** Within this distance the obstacle counts as REACHED -> evaluates the reaction. Keep <= TraceDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Obstacle|Trace", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ImpactTriggerDistance = 45.f;
	/** Radius of each band's forward sphere sweep (cm). Smaller = tighter bands; falls back to the capsule radius if 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Obstacle|Trace", meta = (ClampMin = "0", ForceUnits = "cm"))
	float TraceRadius = 22.f;
	/** Max |normal.Z| accepted as a vertical obstacle (0 = perfectly vertical; rejects floors / ramps). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Obstacle|Trace", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxWallNormalZ = 0.4f;
	/** Collision channel the band sweeps run against. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Obstacle|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_WorldStatic;

	// ---- Body-band probe heights (cm above the feet). Which band is blocked picks the reaction. ----
	/** LOW band (knee). Keep ABOVE the pawn's auto-step-up height so curbs / small steps don't trigger a low hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Obstacle|Bands", meta = (ClampMin = "0", ForceUnits = "cm"))
	float LowProbeHeight = 50.f;
	/** MID band (chest). MID blocked = a WALL (dominant over low/high). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Obstacle|Bands", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MidProbeHeight = 100.f;
	/** HIGH band (head). HIGH-only (chest+knee clear) = an overhead beam -> head hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Obstacle|Bands", meta = (ClampMin = "0", ForceUnits = "cm"))
	float HighProbeHeight = 165.f;

	// ---- Reaction params ----
	/** Closing speed (cm/s) above which reaching a WALL fires Brace (below = slow approach -> Blocked). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Obstacle|Impact", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float ImpactMinSpeed = 150.f;
	/** Closing speed (cm/s) above which a LOW/HIGH band hit fires Stumble/HeadHit (below = gentle bump -> Blocked). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Obstacle|Impact", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float StumbleMinSpeed = 150.f;
	// --- Per-reaction HOLD TIMES — REMOVED from params: the reaction hold is now CLIP-DRIVEN. The SM holds
	// LocomotionLoop until the CHT-selected reaction clip is almost done (UAZ_LocomotionStateMachine::
	// NotifyReactionClipPushed, fed the clip's length by the AnimInstance). The sensor only FIRES the reaction (a
	// brief trigger window, in the cpp) — it no longer owns the hold duration, so these per-clip timers are gone.
	// Kept as commented reference in case we revert to sensor-owned holds:
	// float ImpactHoldTime  = 1.0f;   // Brace   (WallBlocked01 ~1.0s)
	// float StumbleHoldTime = 1.3f;   // Stumble (HitFromFront_LightStandingStumble ~1.33s)
	// float HeadHoldTime    = 1.0f;   // HeadHit (HeadBonk ~1.0s)

	/** Draw the 3 band sweeps + hits + an on-screen readout. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Obstacle|Debug")
	bool bDrawDebug = false;

private:
	UPROPERTY(Transient) TWeakObjectPtr<UMoverComponent> MoverComp;
	UPROPERTY(Transient) TWeakObjectPtr<UCapsuleComponent> Capsule;
	/** World time the one-shot ENTRY reaction (Brace/Stumble/HeadHit) ends; <0 inactive. */
	float EntryReactionEndTime = -1.f;
	/** The entry reaction latched on the rising edge of reaching the obstacle; held until its timer expires. */
	EAZ_ObstacleReaction LatchedEntry = EAZ_ObstacleReaction::None;
	/** Previous-frame within-trigger flag, for rising-edge entry detection. */
	bool bWasWithinTrigger = false;
	/** PEAK closing speed over the current approach (reset when nothing is ahead). The entry gate uses THIS, not the
	 *  instantaneous closing speed: the movement-capability clamp decelerates the resolved velocity as you near a
	 *  wall, so by the trigger frame GetVelocity() is already low — the instantaneous value fails the gate even on a
	 *  full-speed run. The peak captures the approach speed (seen at TraceDistance, before the clamp bled it off). */
	float ApproachClosingSpeed = 0.f;

	void ClearSenseOutputs();
};
