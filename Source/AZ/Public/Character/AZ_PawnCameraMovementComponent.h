#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AZ_PawnCameraMovementComponent.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UAbilitySystemComponent;
class UCharacterMoverComponent;

/**
 * FAZ_CameraDirectionalOffset: the boom SocketOffset to use while the player holds a movement KEY, one entry
 * per key — Forward = W, Backward = S, Left = A, Right = D.
 *
 * KEYED TO INPUT IN CAMERA SPACE, never to where the body travels or faces. The first cut blended by velocity
 * in actor space; with the body parked inside the aim cone, turning the camera with W held swept that blend
 * through the diagonals and the camera slid sideways on every look (the 2026-09-09 "position changes when I
 * rotate" bug). A key means the same thing whichever way the body points, so keys it is.
 *
 * ABSOLUTE, not a delta: each entry IS the socket offset for that key, in the stance's SocketOffset space and
 * units (X toward/away from the pawn, Y screen right, Z up). The first cut ADDED the entry to SocketOffset, and
 * the natural way to author "start from the idle framing" — copying SocketOffset into every key — doubled the
 * offset instead. Now an entry left at (0,0,0) means "not authored" and keeps the stance's idle SocketOffset
 * for that key, so only the directions you want re-framed need values. Two keys held (a diagonal) give the
 * stick-weighted AVERAGE of the two entries, never their sum.
 *
 * WHY it exists: the body does not hold still relative to the capsule while it moves — measured on the rifle
 * aim set (2026-09-09), the torso leans up to 29cm FORWARD and sways 16cm sideways out of the pose it holds in
 * the aim idle, so a framing tuned on the idle is wrong the moment you walk. These re-frame each direction
 * back to the idle composition by eye, without touching the animation.
 */
USTRUCT(BlueprintType)
struct AZ_API FAZ_CameraDirectionalOffset
{
	GENERATED_BODY()

	/** Socket offset while W is held. (0,0,0) = keep the stance's idle SocketOffset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Directional")
	FVector Forward = FVector::ZeroVector;

	/** Socket offset while S is held. (0,0,0) = keep the stance's idle SocketOffset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Directional")
	FVector Backward = FVector::ZeroVector;

	/** Socket offset while A is held. (0,0,0) = keep the stance's idle SocketOffset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Directional")
	FVector Left = FVector::ZeroVector;

	/** Socket offset while D is held. (0,0,0) = keep the stance's idle SocketOffset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Directional")
	FVector Right = FVector::ZeroVector;

	/** The socket offset for a move input (X = forward, Y = right, camera space, each in [-1, 1]) given the
	 *  stance's idle SocketOffset. No input returns Idle; one key returns its entry (Idle when unauthored);
	 *  two keys return the stick-weighted average of theirs. */
	FVector Resolve(const FVector2D& MoveInput, const FVector& Idle) const
	{
		const float X = static_cast<float>(MoveInput.X);
		const float Y = static_cast<float>(MoveInput.Y);
		const float WF = FMath::Max(X, 0.f), WB = FMath::Max(-X, 0.f);
		const float WR = FMath::Max(Y, 0.f), WL = FMath::Max(-Y, 0.f);
		const float Total = WF + WB + WR + WL;
		if (Total <= KINDA_SMALL_NUMBER)
		{
			return Idle;
		}
		auto Pick = [&Idle](const FVector& Entry) { return Entry.IsZero() ? Idle : Entry; };
		return (Pick(Forward) * WF + Pick(Backward) * WB + Pick(Right) * WR + Pick(Left) * WL) / Total;
	}
};

/**
 * FAZ_CameraStanceConfig: Camera settings for a single stance (default, aim, crouch, etc.)
 */
USTRUCT(BlueprintType)
struct AZ_API FAZ_CameraStanceConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float BoomLength = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FVector SocketOffset = FVector(0.f, 70.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float FOV = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.1"))
	float InterpSpeed = 5.f;

	/** Camera-local rotation offset, applied to the CAMERA and not the boom: it re-aims the view about the
	 *  camera's own position, so the character can be recentred on screen without moving the boom's arm
	 *  direction (which would fight bUsePawnControlRotation and change the collision probe). Yaw pushes the
	 *  pawn across the screen, Pitch up/down, Roll tilts the horizon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FRotator RotationOffset = FRotator::ZeroRotator;

	// ---- Spring-arm positional lag, per stance ----
	// The boom trails the capsule while it accelerates, which slides the pawn across the screen by up to
	// CameraLagMaxDistance before it settles. That is a weight you may want while exploring and the wrong one
	// for aiming, where the framing must hold still, so each stance carries its own settings. THE STANCE OWNS
	// THESE: UpdateCameraForMode rewrites the boom's lag properties from the active stance every tick, so lag
	// values authored on the boom component itself never apply. Rotation lag is deliberately NOT here: it
	// smooths the player's own look input, which should feel the same in every stance.

	/** Whether the boom lags behind the capsule at all in this stance. Default OFF everywhere: that is how the
	 *  hero's boom was authored before lag became per-stance (CameraLagSpeed 0 on AZ_BP_PawnMoverHero_MHC = no
	 *  lag in the engine). The first cut defaulted to the C++ boom values (8 / 50cm) and silently gave
	 *  Explore/Strafe a 50cm trail they never had. Turn it on per stance when you actually want the trail. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Lag")
	bool bEnableCameraLag = false;

	/** How quickly the boom catches up to the capsule. Higher = tighter. Ignored when bEnableCameraLag is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Lag", meta = (ClampMin = "0", EditCondition = "bEnableCameraLag"))
	float CameraLagSpeed = 8.f;

	/** Hard cap (cm) on how far the boom may trail the capsule. Ignored when bEnableCameraLag is false.
	 *  ⚠ ENGINE SEMANTICS: 0 means NO CAP (unlimited trail), not "no lag" — USpringArmComponent only clamps
	 *  when this is > 0 (SpringArmComponent.cpp:158). To remove lag, clear bEnableCameraLag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Lag", meta = (ClampMin = "0", ForceUnits = "cm", EditCondition = "bEnableCameraLag"))
	float CameraLagMaxDistance = 50.f;
};

/**
 * FAZ_CameraMoveOffset: Per-direction camera offset for a single stance.
 * X = boom length delta, Y = socket offset Y delta.
 */
USTRUCT(BlueprintType)
struct AZ_API FAZ_CameraMoveOffset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|MoveOffset")
	FVector2D Forward = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|MoveOffset")
	FVector2D Backward = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|MoveOffset")
	FVector2D Left = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|MoveOffset")
	FVector2D Right = FVector2D::ZeroVector;
};

/**
 * FAZ_MovementSpeedConfig: Movement speed settings for a single stance.
 */
USTRUCT(BlueprintType)
struct AZ_API FAZ_MovementSpeedConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxSpeed = 195.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float Acceleration = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float Deceleration = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0"))
	float GroundFriction = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0"))
	float BrakingFrictionFactor = 0.8f;
};

/**
 * FAZ_StanceDefinition: Complete definition of a stance — camera + movement + offsets.
 */
USTRUCT(BlueprintType)
struct AZ_API FAZ_StanceDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FAZ_CameraStanceConfig Camera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|MoveOffset")
	FAZ_CameraMoveOffset MoveOffsets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FAZ_MovementSpeedConfig Movement;
};

/**
 * UAZ_PawnCameraMovementComponent
 *
 * Reusable component that manages camera and movement state transitions.
 * Reads GAS tags each tick, determines the active stance, and interpolates
 * camera boom / FOV / movement speed between stances.
 *
 * Drop it on any pawn with a SpringArm + Camera + ASC.
 */
UCLASS(ClassGroup = (AZ), meta = (BlueprintSpawnableComponent))
class AZ_API UAZ_PawnCameraMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAZ_PawnCameraMovementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ========================================
	// Stance Definitions
	// ========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Stance|Default")
	FAZ_StanceDefinition DefaultStance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Stance|Aim")
	FAZ_StanceDefinition AimStance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Stance|Crouch")
	FAZ_StanceDefinition CrouchStance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Stance|CrouchAim")
	FAZ_StanceDefinition CrouchAimStance;

	// ========================================
	// Movement — Jump & Air
	// ========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Movement|Jump", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float JumpUpwardsSpeed = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Movement|Jump", meta = (ClampMin = "0"))
	float GravityScale = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Movement|Air", meta = (ClampMin = "0"))
	float AirControl = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Movement|Air", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float AirBrakingDeceleration = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Movement|Air", meta = (ClampMin = "0"))
	float AirLateralFriction = 0.2f;

	// ========================================
	// Movement — Sprint
	// ========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Movement|Sprint", meta = (ClampMin = "1.0"))
	float SprintSpeedMultiplier = 2.15f;

	// ========================================
	// Movement — Terrain
	// ========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Movement|Terrain", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxStepHeight = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Movement|Terrain", meta = (ClampMin = "0", ClampMax = "90", ForceUnits = "degrees"))
	float WalkableFloorAngle = 38.f;

	// ========================================
	// Interpolation
	// ========================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Interpolation")
	float MoveOffsetInterpSpeed = 8.f;

	// ========================================
	// Runtime State (read-only)
	// ========================================

	UFUNCTION(BlueprintPure, Category = "AZ|Stance")
	const FAZ_StanceDefinition& GetActiveStance() const { return ActiveStance; }

	UFUNCTION(BlueprintPure, Category = "AZ|Stance")
	float GetActiveMaxSpeed() const;

protected:
	/** Resolve which stance is active based on GAS tags. */
	const FAZ_StanceDefinition& ResolveStance(bool bIsAiming, bool bIsCrouching) const;

	/** Compute per-direction camera move offset based on input direction. */
	FVector2D ComputeMoveOffset(const FAZ_CameraMoveOffset& Offsets, const FVector& LocalMoveInput) const;

	/** Cache references on BeginPlay. */
	UPROPERTY()
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY()
	TWeakObjectPtr<UAbilitySystemComponent> ASC;

	/** v2 doctrine: visuals derive from the Mover RESULT, not GAS intent. Crouch framing reads
	 *  IsCrouching() off this (the channel the capsule actually resizes on) — the ASC tag is intent
	 *  and can lead/lag the body (e.g. ceiling-blocked uncrouch: capsule stays crouched, the tag is
	 *  already gone). Null on pawns without a Mover (legacy CMC hero) → falls back to the GAS tag. */
	UPROPERTY()
	TWeakObjectPtr<UCharacterMoverComponent> CharacterMover;

	FAZ_StanceDefinition ActiveStance;
	FVector2D CurrentMoveOffset = FVector2D::ZeroVector;
	bool bIsSprinting = false;
};
