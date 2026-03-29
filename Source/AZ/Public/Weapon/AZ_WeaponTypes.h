#pragma once

#include "CoreMinimal.h"
#include "AZ_WeaponTypes.generated.h"

/** Animation pose state for weapon IK adjustments. Resolved by AnimInstance from movement + action bools. */
UENUM(BlueprintType)
enum class EAZ_WeaponPoseState : uint8
{
	Relaxed           UMETA(DisplayName = "Relaxed"),
	Aiming            UMETA(DisplayName = "Aiming"),
	Crouching         UMETA(DisplayName = "Crouching"),
	CrouchAiming      UMETA(DisplayName = "Crouch + Aiming"),
	Shooting          UMETA(DisplayName = "Shooting"),
	CrouchShooting    UMETA(DisplayName = "Crouch + Shooting"),
	Reloading         UMETA(DisplayName = "Reloading"),
	CrouchReloading   UMETA(DisplayName = "Crouch + Reloading"),
	Sprinting         UMETA(DisplayName = "Sprinting"),
	MeleeAttacking    UMETA(DisplayName = "Melee Attacking"),
	Interacting       UMETA(DisplayName = "Interacting"),
	Throwing          UMETA(DisplayName = "Throwing"),
};

/** Per-state location + rotation adjustment for the left hand IK socket. */
USTRUCT(BlueprintType)
struct FAZ_LeftHandIKAdjustment
{
	GENERATED_BODY()

	/** Location offset applied on top of the grip socket position (in bone space). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK")
	FVector LocationOffset = FVector::ZeroVector;

	/** Rotation offset applied on top of the grip socket rotation (in bone space). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK")
	FRotator RotationOffset = FRotator::ZeroRotator;
};
