#pragma once

#include "CoreMinimal.h"
#include "Animation/BlendProfile.h"
#include "Components/CapsuleComponent.h"
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

/** State machine state for the SM+BlendStack locomotion system.
 *  Values MUST match GASP E_ExperimentalStateMachineState for chooser parity. */
UENUM(BlueprintType)
enum class EAZ_StateMachineState : uint8
{
	IdleLoop					= 0,
	TransitionToIdle			= 1,
	LocomotionLoop				= 2,
	TransitionToLocomotion		= 3,
	InAirLoop					= 4,
	TransitionToInAir			= 5,
	IdleBreak					= 6,
	TransitionToSlide			= 7,
	SlideLoop					= 8
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
