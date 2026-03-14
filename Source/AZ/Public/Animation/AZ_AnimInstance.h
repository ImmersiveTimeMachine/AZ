#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "AZ_AnimInstance.generated.h"

class UCharacterMovementComponent;

UCLASS()
class AZ_API UAZ_AnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

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
	// JUMP / FALL STATE
	// ========================================

	/** True while the character movement component reports falling (covers both jump and walk-off-edge). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Movement")
	bool bIsFalling;

	/** True if the character actively jumped (as opposed to walking off a ledge). Set by Jump ability, cleared on land. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|Movement")
	bool bIsJumping;

	/** Set by anim notify at the end of the JumpStart animation to transition into the FallLoop. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|Movement")
	bool bIsStartJumpLoop;

	/** Set by anim notify at the end of the Landing animation to transition back to locomotion. */
	UPROPERTY(BlueprintReadWrite, Category = "AZ|Movement")
	bool bIsEndJumpLanding;

protected:

	UPROPERTY()
	TObjectPtr<ACharacter> OwningCharacter;

	UPROPERTY()
	TObjectPtr<UCharacterMovementComponent> MovementComponent;

private:

	/** Tracks whether we were falling last frame so we can detect landing. */
	bool bWasFalling = false;
};
