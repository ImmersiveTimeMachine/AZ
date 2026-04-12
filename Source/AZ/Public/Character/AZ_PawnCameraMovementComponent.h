#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AZ_PawnCameraMovementComponent.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UAbilitySystemComponent;

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

	FAZ_StanceDefinition ActiveStance;
	FVector2D CurrentMoveOffset = FVector2D::ZeroVector;
	bool bIsSprinting = false;
};
