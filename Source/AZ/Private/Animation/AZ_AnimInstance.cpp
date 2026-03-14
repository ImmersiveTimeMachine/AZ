#include "Animation/AZ_AnimInstance.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

void UAZ_AnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwningCharacter = Cast<ACharacter>(TryGetPawnOwner());
	if (OwningCharacter)
	{
		MovementComponent = OwningCharacter->GetCharacterMovement();
	}
}

void UAZ_AnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!MovementComponent || !OwningCharacter)
	{
		return;
	}

	// --- Ground Speed ---
	const FVector Velocity = MovementComponent->Velocity;
	GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.f).Size();

	// --- Falling / Landing detection ---
	const bool bCurrentlyFalling = MovementComponent->IsFalling();

	// Detect landing: was falling last frame, not falling now
	if (bWasFalling && !bCurrentlyFalling)
	{
		bIsJumping = false;
		bIsStartJumpLoop = false;
		// Note: bIsEndJumpLanding is set by the anim notify on the Landing animation,
		// NOT here. We only clear jump state on land.
	}

	bIsFalling = bCurrentlyFalling;
	bWasFalling = bCurrentlyFalling;

	// --- Normalized blendspace inputs ---
	// RE-style: character faces camera direction (bUseControllerRotationYaw).
	// Use velocity transformed to local space so animation follows actual movement,
	// including deceleration slide when input is released.
	if (MaxGroundSpeed > UE_KINDA_SMALL_NUMBER && GroundSpeed > UE_KINDA_SMALL_NUMBER)
	{
		const FVector GroundVelocity(Velocity.X, Velocity.Y, 0.f);
		const FVector LocalVelocity = OwningCharacter->GetActorRotation().UnrotateVector(GroundVelocity);

		NormalizedWalkForwardSpeed = FMath::Clamp(LocalVelocity.X / MaxGroundSpeed, -1.f, 1.f);
		NormalizedWalkRightSpeed = FMath::Clamp(LocalVelocity.Y / MaxGroundSpeed, -1.f, 1.f);
	}
	else
	{
		NormalizedWalkForwardSpeed = 0.f;
		NormalizedWalkRightSpeed = 0.f;
	}
}
