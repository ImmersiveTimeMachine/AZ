#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimInstance.h"
#include "AZ_AnimInstance.generated.h"

class UCharacterMovementComponent;
class UAbilitySystemComponent;

UCLASS()
class AZ_API UAZ_AnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativePostEvaluateAnimation() override;

	// ========================================
	// BLENDSPACE INPUTS
	// ========================================

	/** Normalized forward/backward speed (-1 to 1) for the 2D locomotion blendspace. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement")
	float NormalizedWalkForwardSpeed;

	/** Normalized right/left speed (-1 to 1) for the 2D locomotion blendspace. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement")
	float NormalizedWalkRightSpeed;

	/** Raw ground speed (XY plane magnitude) in cm/s. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement")
	float GroundSpeed;

	/** The maximum ground speed used to normalize blendspace inputs.
	 *  This should cover the full speed range (walk + sprint). Default 500. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|Movement")
	float MaxGroundSpeed = 500.f;

	// ========================================
	// WEAPON STATE (driven by GAS tags)
	// ========================================

	/** Current weapon gameplay tag from the ASC. Used by ABP to select the correct locomotion state machine. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Weapon")
	FGameplayTag CurrentWeaponTag;

	/** Index for Blend Poses by int: 0=Unarmed, 1=Rifle, 2=Pistol, 3=Shotgun, 4=Melee, etc. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Weapon")
	int32 WeaponAnimIndex = 0;

	/** World-space transform of the weapon's left hand grip socket. Used by Hand IK in the AnimBP. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Weapon")
	FTransform LeftHandIKTransform;

	/** Whether left hand IK is enabled (set in editor). Runtime: true only if enabled AND weapon has grip socket. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|Weapon")
	bool bEnableLeftHandIK = true;

	/** Runtime: true when IK is enabled AND weapon grip socket was found this frame. Read by ABP. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Weapon")
	bool bUseLeftHandIK = false;

	/** Socket name on the weapon mesh for the left hand grip (relaxed pose). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|Weapon")
	FName LeftHandGripSocket{TEXT("LeftHandGrip")};

	/** Socket name on the weapon mesh for the left hand grip (aim pose). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|Weapon")
	FName LeftHandGripAimSocket{TEXT("LeftHandGripAim")};

	/** Offset from the grip socket to the palm center. Tweak in editor per character. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|Weapon")
	FVector LeftHandIKOffset{FVector::ZeroVector};

	/** Multiplier applied to blendspace speed inputs when a weapon is equipped.
	 *  Increase to make weapon locomotion animations play faster (e.g. 1.3). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|Weapon")
	float WeaponAnimSpeedMultiplier = 1.0f;

	/** Speed at which the weapon interpolates between relaxed and aim positions. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|Weapon|Positioning")
	float WeaponPoseInterpSpeed = 15.f;


	/** Socket on character mesh where weapon is always attached. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AZ|Weapon|Positioning")
	FName WeaponAttachSocket{TEXT("hand_r")};

	// ========================================
	// JUMP / FALL STATE
	// ========================================

	/** True while the character movement component reports falling (covers both jump and walk-off-edge). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement")
	bool bIsFalling;

	/** True if the character actively jumped (as opposed to walking off a ledge). Set by GA_Jump, cleared on land. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|Movement")
	bool bIsJumping;

	/** True while the character is crouching. Set by GA_Crouch, cleared on uncrouch. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement")
	bool bIsCrouching;

	/** True while the character is aiming (ADS). Set/cleared by GA_Aim toggle. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement")
	bool bIsAiming;

protected:

	UPROPERTY()
	TObjectPtr<ACharacter> OwningCharacter;

	UPROPERTY()
	TObjectPtr<UCharacterMovementComponent> MovementComponent;

	UPROPERTY()
	TWeakObjectPtr<AActor> CachedPrimaryWeapon;

	/** Current interpolated relative transform for weapon positioning. */
	FTransform CurrentWeaponRelativeTransform;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> CachedASC;

private:

	/** Tracks whether we were falling last frame so we can detect landing. */
	bool bWasFalling = false;
};
