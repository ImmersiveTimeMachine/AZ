#pragma once

#include "CoreMinimal.h"
#include "AZ_WeaponTypes.generated.h"

/** Per-weapon accepted-shot recoil tuning. Spread radius is a cone HALF-angle in degrees. */
USTRUCT(BlueprintType)
struct AZ_API FAZ_FirearmRecoilSettings
{
	GENERATED_BODY()

	/** Disables both added shot spread and camera kick; the weapon's baseline spread remains. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil")
	bool bEnabled = true;

	/** Extra cone radius added after each accepted shot, affecting following shots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Spread", meta=(ClampMin="0", ClampMax="90", ForceUnits="deg", EditCondition="bEnabled"))
	float SpreadRadiusPerShotDegrees = 0.08f;

	/** Maximum added HALF-angle above SpreadAim / 2; zero disables spread growth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Spread", meta=(ClampMin="0", ClampMax="90", ForceUnits="deg", EditCondition="bEnabled"))
	float MaxAdditionalSpreadRadiusDegrees = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Spread", meta=(ClampMin="0", ClampMax="60", ForceUnits="s", EditCondition="bEnabled"))
	float SpreadRecoveryDelaySeconds = 0.15f;

	/** Linear radius recovery. The positive minimum ensures every burst eventually settles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Spread", meta=(ClampMin="0.01", ClampMax="360", ForceUnits="deg/s", EditCondition="bEnabled"))
	float SpreadRecoverySpeedDegreesPerSecond = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Camera", meta=(EditCondition="bEnabled"))
	bool bCameraKickEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Camera", meta=(ClampMin="0", ClampMax="89", ForceUnits="deg", EditCondition="bEnabled && bCameraKickEnabled"))
	float CameraPitchKickDegrees = 0.55f;

	/** Random left/right kick in [-radius, +radius] for each accepted shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Camera", meta=(ClampMin="0", ClampMax="89", ForceUnits="deg", EditCondition="bEnabled && bCameraKickEnabled"))
	float CameraYawKickRadiusDegrees = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Camera", meta=(ClampMin="0", ClampMax="89", ForceUnits="deg", EditCondition="bEnabled && bCameraKickEnabled"))
	float MaxCameraPitchDegrees = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Camera", meta=(ClampMin="0", ClampMax="89", ForceUnits="deg", EditCondition="bEnabled && bCameraKickEnabled"))
	float MaxCameraYawDegrees = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Camera", meta=(ClampMin="0", ClampMax="60", ForceUnits="s", EditCondition="bEnabled && bCameraKickEnabled"))
	float CameraRecoveryDelaySeconds = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recoil|Camera", meta=(ClampMin="0.01", ClampMax="360", ForceUnits="deg/s", EditCondition="bEnabled && bCameraKickEnabled"))
	float CameraRecoverySpeedDegreesPerSecond = 7.f;
};

/** Runtime-only accepted-shot receipt. Recovery is evaluated from server time, without ticking authority. */
USTRUCT()
struct FAZ_FirearmSpreadState
{
	GENERATED_BODY()

	UPROPERTY()
	float PeakExtraRadiusDegrees = 0.f;

	UPROPERTY()
	double LastShotServerTime = 0.0;

	UPROPERTY()
	float RecoveryDelaySeconds = 0.f;

	UPROPERTY()
	float RecoverySpeedDegreesPerSecond = 1.25f;
};

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
