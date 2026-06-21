#pragma once

#include "CoreMinimal.h"
#include "Animation/BlendProfile.h"
#include "Components/CapsuleComponent.h"
#include "GameplayTagContainer.h"
#include "MoverTypes.h"
#include "AZ_LocomotionTypes.generated.h"

// ========================================
// ENUMS (matching GASP Data model)
// ========================================

/** Character gait state. */
UENUM(BlueprintType)
enum class EAZ_Gait : uint8
{
	Walk	= 0,
	Run		= 1,
	Sprint	= 2
};

/** Character stance (standing vs crouching). */
UENUM(BlueprintType)
enum class EAZ_Stance : uint8
{
	Standing	= 0,
	Crouching	= 1
};

/** Movement mode from Mover or CMC. */
UENUM(BlueprintType)
enum class EAZ_MovementMode : uint8
{
	OnGround	= 0,
	InAir		= 1,
	Slide		= 2,
	Traversing	= 3
};

/** Movement state (idle vs moving). */
UENUM(BlueprintType)
enum class EAZ_MovementState : uint8
{
	Idle	= 0,
	Moving	= 1
};

/** Movement direction relative to character facing — matches GASP E_MovementDirection.
 *  6 values encode direction + foot phase for strafe animation selection:
 *  - F/B: Forward/Backward (foot phase ignored)
 *  - LL:  Left-side move,  Left-foot-first
 *  - LR:  Left-side move,  Right-foot-first
 *  - RL:  Right-side move, Left-foot-first
 *  - RR:  Right-side move, Right-foot-first
 */
UENUM(BlueprintType)
enum class EAZ_MovementDirection : uint8
{
	F	= 0,	// Forward
	B	= 1,	// Backward
	LL	= 2,	// Left side, Left foot leading
	LR	= 3,	// Left side, Right foot leading
	RL	= 4,	// Right side, Left foot leading
	RR	= 5		// Right side, Right foot leading
};

/** Bias for resolving ambiguous diagonal directions. */
UENUM(BlueprintType)
enum class EAZ_MovementDirectionBias : uint8
{
	LeftBias	= 0,
	RightBias	= 1
};

/** Rotation mode (how the character orients). */
UENUM(BlueprintType)
enum class EAZ_RotationMode : uint8
{
	OrientToMovement	= 0,
	Strafe				= 1,
	Aiming				= 2
};

/**
 * EAZ_StateMachineState - the phases of the v2 locomotion phase machine
 * (UAZ_LocomotionStateMachine::ComputeNextState - PURE C++ each tick, NOT an AnimGraph State Machine node).
 *
 * Exactly ONE phase is active per frame, and it is the single source of truth the rest of the pipeline keys off:
 *   - the phase machine OUTPUTS it each tick from intent (bIsMoving), the Mover MovementMode (air), the
 *     transition / idle-break timers, and the latched turn/land data;
 *   - the CHT_v2_CharacterAnimations chooser's SMState column SELECTS the clip row from it;
 *   - SetBlendStackAnimFromChooser then plays the chosen clip on the single Blend Stack.
 *
 * Two kinds of phase:
 *   - LOOPS  (IdleLoop / LocomotionLoop / InAirLoop / SlideLoop): steady state, held until something changes.
 *   - TRANS  (TransitionTo*): one-shot bridges; each plays a root-motion clip that owns the capsule for its
 *     length (the RM bridge, approach A'), then falls through to its target loop on a timer.
 *
 * WARNING - the integer values are an ABI with the chooser ASSET: CHT_v2's SMState column stores these numbers,
 * so reordering or renumbering silently remaps every authored row. Append new states at the END; never renumber.
 * (Values originated from GASP E_ExperimentalStateMachineState - that is HISTORICAL only; v2 dropped GASP parity,
 * the binding constraint now is the chooser asset, not GASP.)
 */
UENUM(BlueprintType)
enum class EAZ_StateMachineState : uint8
{
	IdleLoop					= 0,	// LOOP  - resting pose (standing OR crouching); schedules the next idle break on a random timer
	TransitionToIdle			= 1,	// TRANS - STOP (moving->idle, foot-aware RM stop clip) OR standing land (JumpIdleLand); held by TransitionEndTime
	LocomotionLoop				= 2,	// LOOP  - moving steady state (walk/run loop); velocity-driven, the clip's baked root motion is discarded
	TransitionToLocomotion		= 3,	// TRANS - START / moving pivot-reversal / moving land; RM turn-start clip; latches StartDirection + bMovingTransition
	InAirLoop					= 4,	// LOOP  - fall loop; held by MovementMode==InAir (engine Falling) until real floor contact
	TransitionToInAir			= 5,	// TRANS - takeoff; hybrid jump's RM rise (RMAction) owns the capsule, held past the takeoff timer to the apex handoff
	IdleBreak					= 6,	// LOOP* - cosmetic idle fidget; from IdleLoop on a timer, held to clip end, cancelled by move / air / stance change
	TransitionToSlide			= 7,	// TRANS - RESERVED: slide entry (no ComputeNextState path emits this yet)
	SlideLoop					= 8,	// LOOP  - RESERVED: slide steady state (not wired yet)
	TransitionStance			= 9,	// TRANS - in-place stance change (Idle2Crouch)
	IdleTurnLeft				= 10,	// VESTIGIAL - strafe idle turn-in-place (removed; strafe now continuously faces camera). Kept for ABI; no ComputeNextState path emits these. Dead CHT rows 77/78 still map them (harmless).
	IdleTurnRight				= 11	// VESTIGIAL - see above.
};

/** Turn magnitude + side for a from-idle start (TransitionToLocomotion).
 *  Selects the RM turn-start clip (RTG_RM_*Start{90,135,180}_{L,R}) whose baked root rotation
 *  pivots the body toward the desired heading while accelerating from rest. Bucketed from the
 *  signed yaw between current facing and desired move direction, LATCHED at transition entry
 *  (the clip's own root rotation collapses the live angle as it plays, so it must NOT be
 *  re-bucketed mid-turn). Fwd = |angle| <= 45 deg (no turn — the plain forward start). */
UENUM(BlueprintType)
enum class EAZ_StartDirection : uint8
{
	Fwd		= 0,	// |angle| <= 45        -> RTG_RM_{Walk,Run}FwdStart
	L90		= 1,	// turn ~90 left
	R90		= 2,	// turn ~90 right
	L135	= 3,	// turn ~135 left
	R135	= 4,	// turn ~135 right
	L180	= 5,	// about-face, lead left
	R180	= 6		// about-face, lead right
};

/** Traversal action type (mirrors GASP E_TraversalActionType). */
UENUM(BlueprintType)
enum class EAZ_TraversalActionType : uint8
{
	None	= 0,
	Hurdle	= 1,
	Vault	= 2,
	Mantle	= 3
};

/** Analog stick behavior for gait selection. */
UENUM(BlueprintType)
enum class EAZ_AnalogStickBehavior : uint8
{
	FixedSpeed_SingleGait	= 0,
	FixedSpeed_WalkRun		= 1,
	VariableSpeed_SingleGait= 2,
	VariableSpeed_WalkRun	= 3
};

// ========================================
// STRUCTS (matching GASP Data model)
// ========================================

/** Player input desire flags — raw intent before derived state. */
USTRUCT(BlueprintType)
struct FAZ_PlayerInputState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bWantsToSprint = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bWantsToWalk = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bWantsToStrafe = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bWantsToAim = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bWantsToCrouch = false;
};

/** Custom inputs sent to/from the Mover system. Mirrors GASP S_MoverCustomInputs —
 *  lives in the Mover InputCollection (FMoverDataStructBase), replicated by Mover. */
USTRUCT(BlueprintType)
struct AZ_API FAZ_MoverCustomInputs : public FMoverDataStructBase
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAZ_MovementDirection MovementDirection = EAZ_MovementDirection::F;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAZ_Gait Gait = EAZ_Gait::Run;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAZ_RotationMode RotationMode = EAZ_RotationMode::OrientToMovement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double RotationOffset = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bWantsToCrouch = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double ControlRotationRate = 0.0;

	// --- FMoverDataStructBase overrides ---

	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override
	{
		const FAZ_MoverCustomInputs& Auth = static_cast<const FAZ_MoverCustomInputs&>(AuthorityState);
		return (Auth.MovementDirection != MovementDirection)
			|| (Auth.Gait != Gait)
			|| (Auth.RotationMode != RotationMode)
			|| !FMath::IsNearlyEqual(Auth.RotationOffset, RotationOffset)
			|| (Auth.bWantsToCrouch != bWantsToCrouch)
			|| !FMath::IsNearlyEqual(Auth.ControlRotationRate, ControlRotationRate);
	}

	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float LerpFactor) override
	{
		const FAZ_MoverCustomInputs& TypedFrom = static_cast<const FAZ_MoverCustomInputs&>(From);
		const FAZ_MoverCustomInputs& TypedTo   = static_cast<const FAZ_MoverCustomInputs&>(To);
		const FAZ_MoverCustomInputs& Source = (LerpFactor < 0.5f) ? TypedFrom : TypedTo;
		MovementDirection   = Source.MovementDirection;
		Gait                = Source.Gait;
		RotationMode        = Source.RotationMode;
		bWantsToCrouch      = Source.bWantsToCrouch;
		RotationOffset      = FMath::Lerp(TypedFrom.RotationOffset, TypedTo.RotationOffset, LerpFactor);
		ControlRotationRate = FMath::Lerp(TypedFrom.ControlRotationRate, TypedTo.ControlRotationRate, LerpFactor);
	}

	virtual void Merge(const FMoverDataStructBase& From) override
	{
		const FAZ_MoverCustomInputs& TypedFrom = static_cast<const FAZ_MoverCustomInputs&>(From);
		bWantsToCrouch |= TypedFrom.bWantsToCrouch;
	}

	virtual FMoverDataStructBase* Clone() const override
	{
		return new FAZ_MoverCustomInputs(*this);
	}

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override
	{
		Super::NetSerialize(Ar, Map, bOutSuccess);
		uint8 MD = static_cast<uint8>(MovementDirection);
		uint8 G  = static_cast<uint8>(Gait);
		uint8 RM = static_cast<uint8>(RotationMode);
		Ar.SerializeBits(&MD, 3);
		Ar.SerializeBits(&G,  2);
		Ar.SerializeBits(&RM, 2);
		Ar.SerializeBits(&bWantsToCrouch, 1);
		Ar << RotationOffset;
		Ar << ControlRotationRate;
		if (Ar.IsLoading())
		{
			MovementDirection = static_cast<EAZ_MovementDirection>(MD);
			Gait              = static_cast<EAZ_Gait>(G);
			RotationMode      = static_cast<EAZ_RotationMode>(RM);
		}
		bOutSuccess = true;
		return true;
	}

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }

	virtual void ToString(FAnsiStringBuilderBase& Out) const override
	{
		Super::ToString(Out);
		Out.Appendf("MovementDirection: %u\n", static_cast<uint32>(MovementDirection));
		Out.Appendf("Gait: %u\n", static_cast<uint32>(Gait));
		Out.Appendf("RotationMode: %u\n", static_cast<uint32>(RotationMode));
		Out.Appendf("bWantsToCrouch: %i\n", bWantsToCrouch);
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override { Super::AddReferencedObjects(Collector); }
};

/** Data passed from character to ABP via interface. */
USTRUCT(BlueprintType)
struct FAZ_CharacterPropertiesForAnimation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAZ_MovementDirection MovementDirection = EAZ_MovementDirection::F;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator AimingRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bJustLanded = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector LandVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator OrientationIntent = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double SteeringTime = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector GroundNormal = FVector::UpVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector GroundLocation = FVector::ZeroVector;
};

/** Data passed from character to camera director. */
USTRUCT(BlueprintType)
struct FAZ_CharacterPropertiesForCamera
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 CameraMode = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAZ_Stance Stance = EAZ_Stance::Standing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAZ_Gait Gait = EAZ_Gait::Run;
};

/** Data passed from character to traversal logic. */
USTRUCT(BlueprintType)
struct FAZ_CharacterPropertiesForTraversal
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USkeletalMeshComponent> Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UCapsuleComponent> Capsule = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAZ_MovementMode MovementMode = EAZ_MovementMode::OnGround;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAZ_Gait Gait = EAZ_Gait::Run;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double Speed = 0.0;
};

/** Inputs for the BlendStack node — written by SetBlendStackAnimFromChooser. */
USTRUCT(BlueprintType)
struct FAZ_BlendStackInputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UAnimationAsset> Anim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double StartTime = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double BlendTime = 0.2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UBlendProfile> BlendProfile = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> Tags;
};

/** Output from Chooser table evaluation — controls how anim is selected and blended. */
USTRUCT(BlueprintType)
struct FAZ_ChooserOutputs
{
	GENERATED_BODY()

	/** If true, do a single-frame MotionMatch on the ValidAnims instead of picking first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUseMM = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double StartTime = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double BlendTime = 0.2;

	/** Crossfade duration used when this clip is REPLACED (its blend-OUT). When > 0 it overrides the
	 *  incoming clip's BlendTime for that one crossfade (outgoing-wins), so a transition owns its own
	 *  release regardless of what follows. <= 0 (default) = no override; the incoming BlendTime governs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double BlendOut = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName BlendProfile = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> Tags;

	/** If > 0, MM result must beat this cost threshold. Otherwise NoValidAnim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double MMCostLimit = 0.0;
};

/** Angle thresholds for resolving movement direction quadrants. */
USTRUCT(BlueprintType)
struct FAZ_MovementDirectionThresholds
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double FL = 55.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double FR = 55.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double BL = 125.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double BR = 125.0;
};

/** Input for the rotation offset curve Chooser. */
USTRUCT(BlueprintType)
struct FAZ_RotationOffsetChooserInputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAZ_MovementMode MovementMode = EAZ_MovementMode::OnGround;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAZ_MovementDirection MovementDirection = EAZ_MovementDirection::F;
};

/** Traversal trace input parameters. */
USTRUCT(BlueprintType)
struct FAZ_TraversalCheckInputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector TraceForwardDirection = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double TraceForwardDistance = 200.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector TraceOriginOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector TraceEndOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double TraceRadius = 30.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double TraceHalfHeight = 60.0;
};

/** Result of traversal geometry analysis. */
USTRUCT(BlueprintType)
struct FAZ_TraversalCheckResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 ActionType = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double ObstacleDepth = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double ObstacleHeight = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasFrontLedge = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector FrontLedgeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector FrontLedgeNormal = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasBackLedge = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector BackLedgeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector BackLedgeNormal = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasBackFloor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector BackFloorLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double BackLedgeHeight = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UPrimitiveComponent> HitComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UAnimMontage> ChosenMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double StartTime = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double PlayRate = 1.0;
};

/** Traversal chooser inputs. */
USTRUCT(BlueprintType)
struct FAZ_TraversalChooserInputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 ActionType = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasFrontLedge = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasBackLedge = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasBackFloor = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double ObstacleHeight = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double ObstacleDepth = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double BackLedgeHeight = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double DistanceToLedge = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAZ_MovementMode MovementMode = EAZ_MovementMode::OnGround;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAZ_Gait Gait = EAZ_Gait::Run;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double Speed = 0.0;
};

/** Traversal chooser outputs. */
USTRUCT(BlueprintType)
struct FAZ_TraversalChooserOutputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 ActionType = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double MontageStartTime = 0.0;
};

// ========================================
// V2 chooser context
// ========================================

/**
 * Snapshot of every input the v2 chooser tables read to pick an anim.
 * Populated once per tick by UAZ_PawnMoverAnimInstance from Mover sync state + GAS tags.
 * The chooser exposes ONE input property of this type and pulls subfields via property
 * accessors — keeps the CHT signature stable as new fields are added.
 *
 * Tag-driven state (weapon, aiming, reloading, etc.) lives in OwnedTags rather than as
 * typed enum fields so adding a new gameplay tag never requires a struct/chooser change.
 */
USTRUCT(BlueprintType)
struct AZ_API FAZ_v2_ChooserContext
{
	GENERATED_BODY()

	// ---- Mover state (per-tick from FMoverDefaultSyncState) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|V2|Chooser")
	EAZ_Gait Gait = EAZ_Gait::Walk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|V2|Chooser")
	EAZ_Stance Stance = EAZ_Stance::Standing;

	/** The last SETTLED stance (what the body still SHOWS). During TransitionStance it holds the stance
	 *  being left, so a chooser column can disambiguate the transition pair the day a third stance exists
	 *  (Stand2Prone vs Crouch2Prone). Equal to Stance everywhere outside a stance transition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|V2|Chooser")
	EAZ_Stance FromStance = EAZ_Stance::Standing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|V2|Chooser")
	EAZ_MovementMode MovementMode = EAZ_MovementMode::OnGround;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|V2|Chooser")
	EAZ_MovementDirection MovementDirection = EAZ_MovementDirection::F;

	/** Which foot is planted this frame (from the playing clip's contact_l curve > 0.5).
	 *  Used as a BoolColumn filter on stop/start/pivot rows to pick the correct-foot variant —
	 *  MM refines the entry frame within the chosen-foot bucket but does not pick the foot.
	 *  False for clips without a contact curve (idle, break, jump). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|V2|Chooser")
	bool bLeftFootDown = false;

	/** Coarse phase the AnimInstance considers the pawn to be in. Reuses GASP's enum:
	 *  IdleLoop / TransitionToIdle / LocomotionLoop / TransitionToLocomotion / InAirLoop /
	 *  TransitionToInAir / IdleBreak / TransitionToSlide / SlideLoop. Drives top-level chooser branch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|V2|Chooser")
	EAZ_StateMachineState SMState = EAZ_StateMachineState::IdleLoop;

	/** Turn bucket for a from-idle start — selects the RM turn-start clip (90/135/180 L/R) on the
	 *  TransitionToLocomotion rows. Latched at transition entry and held for the whole start
	 *  transition; Fwd in every other phase (idle/loop/stop don't turn-start). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|V2|Chooser")
	EAZ_StartDirection StartDirection = EAZ_StartDirection::Fwd;

	/** True while the active TransitionToLocomotion was entered from LocomotionLoop (a moving pivot /
	 *  reversal) rather than from rest (idle). Lets the chooser route a hard moving turn to the dedicated
	 *  momentum-preserving pivot clip (e.g. RTG_RM_RunFwdTurn180_*) instead of the from-rest turn-start,
	 *  while idle starts keep the from-rest clip. Latched at transition entry; False in every other phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|V2|Chooser")
	bool bMovingTransition = false;

	/** True only while playing a touchdown LAND clip — a TransitionToIdle that was entered from the air
	 *  (vs a normal stop). Lets the chooser pick a jump-land (e.g. RTG_RM_JumpIdleLand) instead of a stop
	 *  clip under the shared TransitionToIdle phase. Latched at the air->ground edge; False otherwise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|V2|Chooser")
	bool bJustLanded = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|V2|Chooser", meta = (ForceUnits = "cm/s"))
	float Speed2D = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|V2|Chooser")
	bool bIsMoving = false;

	/** True while the strafe / combat-ready rotation mode is active (player ASC has Movement.Strafe,
	 *  set on equip of a strafe profile). Body faces camera/target; the chooser routes to the
	 *  directional strafe loco set instead of the orient-to-movement forward clips. Drives a
	 *  BoolColumn gate on the strafe rows. Replicated loose tag -> present on all roles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|V2|Chooser")
	bool bStrafe = false;

	// ---- GAS-driven state (full tag snapshot from the player ASC) ----
	/** Chooser predicates query with HasTag / HasMatchingTag — e.g. Weapon.Slot.Rifle,
	 *  State.Aiming, State.Reloading, Ability.Cooldown.*. Adding a new tag is a pure
	 *  asset edit (tag definition + chooser predicate), no C++ change. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|V2|Chooser")
	FGameplayTagContainer OwnedTags;

	// ---- Camera / facing (for rotation-aware choosers — TIP, AO chains, traversal) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|V2|Chooser")
	FRotator AimingRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|V2|Chooser", meta = (ForceUnits = "deg"))
	double RotationOffset = 0.0;
};
