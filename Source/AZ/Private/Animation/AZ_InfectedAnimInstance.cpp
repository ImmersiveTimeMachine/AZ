// Copyright Artur. AZ project.

#include "Animation/AZ_InfectedAnimInstance.h"

#include "Character/AZ_PawnMoverComponent.h"
#include "Character/AZ_PawnMoverInfectedCharacter.h"

void UAZ_InfectedAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	Cached_Pawn = Cast<AAZ_PawnMoverInfectedCharacter>(TryGetPawnOwner());
	if (Cached_Pawn)
	{
		Cached_MoverComponent = Cached_Pawn->GetMoverComponent();
	}
}

void UAZ_InfectedAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Re-resolve lazily: the pawn / Mover may not exist on the first init (editor preview) or right after spawn.
	if (!Cached_Pawn)
	{
		Cached_Pawn = Cast<AAZ_PawnMoverInfectedCharacter>(TryGetPawnOwner());
	}
	if (Cached_Pawn && !Cached_MoverComponent)
	{
		Cached_MoverComponent = Cached_Pawn->GetMoverComponent();
	}

	if (!Cached_MoverComponent)
	{
		GroundSpeed = 0.f;
		bIsMoving   = false;
		return;
	}

	// Physics -> anim: the Mover owns translation; the blendspace just visualizes the resulting ground speed.
	const FVector Velocity = Cached_MoverComponent->GetVelocity();
	GroundSpeed = Velocity.Size2D();
	bIsMoving   = GroundSpeed > MoveSpeedThreshold;
}
