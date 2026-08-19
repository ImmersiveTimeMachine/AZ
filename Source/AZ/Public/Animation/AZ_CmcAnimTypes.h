// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Animation/AZ_LocomotionTypes.h"   // EAZ_Gait / EAZ_Stance / EAZ_MovementMode / EAZ_RotationMode / FAZ_PlayerInputState
#include "AZ_CmcAnimTypes.generated.h"

class UPoseSearchDatabase;

/**
 * FAZ_CmcAnimContract — the ENTIRE pawn -> animation seam for the CMC (v3) generation.
 * [SPIKE: spike/cmc-backport]
 *
 * Modelled on GASP 5.8's S_CharacterPropertiesForAnimation, which the sandbox pawn hands to its ABP
 * through ONE interface call per frame. We keep that shape deliberately: a single struct filled by a
 * single virtual (AAZ_CmcCharacterBase::FillAnimContract) is the only thing the anim layer is allowed
 * to know about the pawn. Anything that can fill this struct can drive the graph — a character today,
 * a vehicle seat or a mounted turret later — without the AnimInstance learning a second pawn type.
 * That is the standing answer to the concrete-cast disease that has followed us across two pawn
 * generations (v1 refs still litter the codebase; GAs still cast the v2 pawn directly).
 *
 * ONE OWNER PER FACT: every field here is a RAW MEASUREMENT the pawn already knows. Nothing derived
 * lives in this struct — direction, foot phase, lean, aim offset and trajectory are computed by
 * UAZ_CmcAnimInstance from these inputs. GASP carries MovementDirection on the pawn side only because
 * its pawn feeds direction back into Mover for rotation; CMC has no such need, and direction depends
 * on foot phase, which is a curve only the anim instance can read.
 *
 * Written on the game thread in NativeUpdateAnimation, read on the worker thread in
 * NativeThreadSafeUpdateAnimation. The anim task boundary is the fence (same arrangement as
 * FAZ_v2_ChooserContext) — do not write it from anywhere else.
 */
USTRUCT(BlueprintType)
struct AZ_API FAZ_CmcAnimContract
{
	GENERATED_BODY()

	// ======================================== Spatial ========================================

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FTransform ActorTransform = FTransform::Identity;

	/** CMC's current velocity (cm/s, world space). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract", meta = (ForceUnits = "cm/s"))
	FVector Velocity = FVector::ZeroVector;

	/** CMC::GetCurrentAcceleration() — INTENT, not measured motion. Non-zero the instant the stick moves,
	 *  which is why every "is the player asking to move" test keys off this rather than Velocity. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FVector InputAcceleration = FVector::ZeroVector;

	/** Where the player is looking. Locally controlled = ControlRotation at full rate; remote proxies get
	 *  the compressed replicated BaseAimRotation (AAZ_CmcCharacterBase::GetAimRotation resolves which). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FRotator AimingRotation = FRotator::ZeroRotator;

	/** Where the body WANTS to face this frame: the acceleration direction while orienting to movement,
	 *  the aim yaw while strafing/aiming. The steering + turn-in-place logic keys off this, not off the
	 *  capsule's actual rotation — the capsule may already be there (we rotate instantly on the ground). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FRotator OrientationIntent = FRotator::ZeroRotator;

	/** World-space movement contributed by the base we are standing on (moving platform / vehicle roof)
	 *  this frame. Must be subtracted before trajectory prediction, or standing still on a moving lift
	 *  reads as running. Zero when unbased, which is every case in AZ today. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FVector BasedMovementDelta = FVector::ZeroVector;

	// ============================ CMC parameters (live, not constants) ============================
	// The hero recomputes acceleration/braking EVERY FRAME from speed and stick state (the GASP feel
	// pass), so these are genuinely per-frame facts, not settings.
	//
	// They do NOT feed the motion-matching predictor, despite the obvious guess: FPoseSearchTrajectoryData
	// exposes no such fields, and its internal FDerived reads MaxSpeed / BrakingDeceleration / Friction
	// straight off the movement component every frame (PoseSearchTrajectoryLibrary.h:31-48). The engine
	// already guarantees predictor and simulation agree. These are published for GRAPH decisions —
	// "how hard am I braking", "how close to top speed" — and for the debug dump.

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	float CurrentMaxAcceleration = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	float CurrentMaxDeceleration = 0.f;

	/** CMC::GetMaxSpeed() — the gait's target speed, already stance- and crouch-adjusted. The reference
	 *  every "fraction of top speed" test needs; without it the anim layer would have to duplicate the
	 *  gait->speed table that AAZ_CmcCharacterBase::SetGait owns. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract", meta = (ForceUnits = "cm/s"))
	float CurrentMaxSpeed = 0.f;

	// ======================================== Floor ========================================

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FVector GroundLocation = FVector::ZeroVector;

	/** Consumed by OffsetRootBone's bOnGround projection (it flattens the simulated root onto this
	 *  plane every frame) and by foot placement. Up vector whenever there is no valid floor. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FVector GroundNormal = FVector::UpVector;

	// ==================================== Discrete state ====================================

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	EAZ_Gait Gait = EAZ_Gait::Run;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	EAZ_Stance Stance = EAZ_Stance::Standing;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	EAZ_MovementMode MovementMode = EAZ_MovementMode::OnGround;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	EAZ_RotationMode RotationMode = EAZ_RotationMode::OrientToMovement;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FAZ_PlayerInputState InputState;

	// ======================================== Landing ========================================
	// CMC zeroes fall velocity the instant it lands, so neither of these is observable by polling —
	// the character latches both at the Landed() event and holds them for JustLandedDuration.

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	bool bJustLanded = false;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FVector LandVelocity = FVector::ZeroVector;

	// ========================================= Tags =========================================

	/** Full ASC snapshot. Tag-driven state travels here rather than as typed fields so adding a gameplay
	 *  tag stays a pure asset edit — no struct change, no chooser signature change, no rebuild. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Contract")
	FGameplayTagContainer OwnedTags;
};

/**
 * FAZ_DatabaseGate — one row of the database-selection table, typed.
 *
 * This is GASP's CHT_PoseSearchDatabases_Dense row model (MovementMode × Stance × MovementState × Gait
 * -> database array, first-to-last, every matching row unioned) expressed as a struct array instead of
 * a chooser asset. Same evaluation semantics as EvaluateChooserMulti; none of the chooser costs we have
 * paid twice before: no property-name reflection binding (the AnimSet scar), no anonymous bool columns
 * (the CHT_v2 scar), no scripted-edit row/column reordering hazards — and it diffs in git and prints in
 * the debug line. GASP's meta-chooser layer above this is only a DDCvar-driven density-tier A/B switch;
 * when we grow a second tier, that becomes a second gate array behind an FAZModule-registered CVar.
 *
 * An EMPTY axis array is the chooser's "Any" cell. Rows live on the ABP CDO (editor-assigned — no
 * /Game/ paths in code) and are unioned by UAZ_CmcAnimInstance::Get_DatabasesToSearch.
 */
USTRUCT(BlueprintType)
struct AZ_API FAZ_DatabaseGate
{
	GENERATED_BODY()

	/** Row name, printed by the bDebugAnim line so a bad pool traces to a row in one read. */
	UPROPERTY(EditAnywhere, Category = "Gate")
	FName Label;

	/** Empty = Any. */
	UPROPERTY(EditAnywhere, Category = "Gate")
	TArray<EAZ_MovementMode> MovementModes;

	/** Empty = Any. */
	UPROPERTY(EditAnywhere, Category = "Gate")
	TArray<EAZ_Stance> Stances;

	/** Empty = Any. */
	UPROPERTY(EditAnywhere, Category = "Gate")
	TArray<EAZ_MovementState> MovementStates;

	/** Empty = Any. Gate on gait only where content demands it (e.g. sprint-only pools) — speed already
	 *  lives in the trajectory features, so gait gating is a cost optimization, not a correctness one,
	 *  and strict per-gait gating with gait-incomplete content (no sprint pivots) punches holes. */
	UPROPERTY(EditAnywhere, Category = "Gate")
	TArray<EAZ_Gait> Gaits;

	/** Databases this row contributes to the search when it matches. */
	UPROPERTY(EditAnywhere, Category = "Gate")
	TArray<TObjectPtr<UPoseSearchDatabase>> Databases;

	bool Matches(const EAZ_MovementMode InMode, const EAZ_Stance InStance,
	             const EAZ_MovementState InState, const EAZ_Gait InGait) const
	{
		return (MovementModes.IsEmpty()  || MovementModes.Contains(InMode))
			&& (Stances.IsEmpty()        || Stances.Contains(InStance))
			&& (MovementStates.IsEmpty() || MovementStates.Contains(InState))
			&& (Gaits.IsEmpty()          || Gaits.Contains(InGait));
	}
};
