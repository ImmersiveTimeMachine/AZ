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

	/** Gait for ANIMATION POOL SELECTION — momentum-aware, and LATCHED for the duration of a stop.
	 *  Gait above is COMMANDED (tag-derived) and correctly drives MaxWalkSpeed; this one is what the body
	 *  is doing. Computed by AAZ_CmcCharacterBase::UpdateSelectionGait so there is one owner. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Anim")
	EAZ_Gait SelectionGait = EAZ_Gait::Walk;

	/**
	 * The gait->speed table, published so the anim layer can classify what the BODY is doing.
	 *
	 * Gait above is the COMMANDED gait (tag-derived): releasing the sprint input drops it to Walk on that
	 * frame while the character is still travelling at 565 cm/s, which narrowed the database gate rows to
	 * WalkMove and offered a 162 cm/s stop clip to a sprinting body (measured 2026-08-23). Selection needs
	 * momentum, not intent — so it derives its own gait from Speed2D against these, and takes the higher
	 * of the two. Owned by AAZ_CmcCharacterBase::SetGait; published here rather than duplicated on the
	 * AnimInstance so there stays exactly one owner of the table.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Anim")
	float WalkSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Anim")
	float RunSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Cmc|Anim")
	float SprintSpeed = 0.f;

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

	// ==================================================================================
	// Stage-B/C axes — AUTHORED BUT NOT YET CONSULTED (added 2026-08-23).
	//
	// Matches() deliberately still tests only the four original axes, so every existing row behaves
	// EXACTLY as before: empty array = Any, bExclusive = false. The fields live here now because adding
	// UPROPERTYs to a USTRUCT changes its layout and therefore costs an editor-closed build, whereas
	// WIRING them is a function body Live Coding can patch. Expensive part now, cheap part later.
	//
	// The plan these serve is [[project_mm_state_selection_plan]]: rows declare the available SET and
	// motion matching chooses WITHIN it. Four separate bugs on 2026-08-22 shared one root cause — MM
	// cannot separate near-tied candidates in sparse pools — and the handedness case proved cost can
	// never fix it, because cost cannot see intent. These axes are how intent gets in.
	// ==================================================================================

	/** Empty = Any. The locomotion phase from UAZ_LocomotionStateMachine (Starting / Stopping / Pivoting
	 *  / TurnInPlace / StanceChange / the loops). This is the axis that makes a discrete event's pool
	 *  selectable by STATE rather than by cost. */
	UPROPERTY(EditAnywhere, Category = "Gate")
	TArray<EAZ_StateMachineState> States;

	/** Empty = Any. Which way the turn goes, LATCHED at event onset. Reuses the SM's existing bucketing
	 *  (UAZ_LocomotionStateMachine::BucketStartDirection over PendingStartAngleDeg) rather than inventing
	 *  a second one. The latch is mandatory: at ~180 degrees the raw sign flickers frame to frame with
	 *  stick noise, which is exactly how `_L` came to win 9 of 10 starts regardless of actual direction. */
	UPROPERTY(EditAnywhere, Category = "Gate")
	TArray<EAZ_StartDirection> StartDirections;

	/** Empty = Any. OrientToMovement / Strafe / Aiming — needed by the combat strafe slice, and included
	 *  now so that work does not cost a second editor-closed build. */
	UPROPERTY(EditAnywhere, Category = "Gate")
	TArray<EAZ_RotationMode> RotationModes;

	/** When this row matches, it is the ONLY pool searched — first-match-wins instead of union. This is
	 *  what turns the gate table into a chooser for discrete events while leaving the loops unioned.
	 *  NOTE there is deliberately no MaxAcceptableCost field: v2's MMCostLimit works because that code
	 *  owns the push and can decline it, whereas the MM node commits internally before we ever see the
	 *  result. There is no veto hook, so the field would be dead weight. */
	UPROPERTY(EditAnywhere, Category = "Gate")
	bool bExclusive = false;

	bool Matches(const EAZ_MovementMode InMode, const EAZ_Stance InStance,
	             const EAZ_MovementState InState, const EAZ_Gait InGait) const
	{
		return (MovementModes.IsEmpty()  || MovementModes.Contains(InMode))
			&& (Stances.IsEmpty()        || Stances.Contains(InStance))
			&& (MovementStates.IsEmpty() || MovementStates.Contains(InState))
			&& (Gaits.IsEmpty()          || Gaits.Contains(InGait));
	}
};
