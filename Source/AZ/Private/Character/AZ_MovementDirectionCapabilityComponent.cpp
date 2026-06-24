// Copyright Artur. AZ project.

#include "Character/AZ_MovementDirectionCapabilityComponent.h"

#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MoverComponent.h"
#include "MoveLibrary/MovementUtils.h"        // UMovementUtils::ComputeSlideDelta
#include "MoveLibrary/MovementUtilsTypes.h"   // FMovingComponentSet

UAZ_MovementDirectionCapabilityComponent::UAZ_MovementDirectionCapabilityComponent()
{
	// Pure query component — driven by the pawn's ProduceInput, never ticks on its own.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UAZ_MovementDirectionCapabilityComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		MoverComp = Owner->FindComponentByClass<UMoverComponent>();
		Capsule   = Owner->FindComponentByClass<UCapsuleComponent>();
	}
}

bool UAZ_MovementDirectionCapabilityComponent::SweepForWall(const FVector& WorldDir, FHitResult& OutHit) const
{
	const UWorld* World = GetWorld();
	const UCapsuleComponent* Cap = Capsule.Get();
	const AActor* Owner = GetOwner();
	if (!World || !Cap || !Owner)
	{
		return false;
	}

	// Sweep capsule = the pawn capsule from StepUpClearance upward (so walkable steps / curbs below it aren't
	// treated as walls), slightly inset so resting contact doesn't false-trigger. The TOP stays at the head, so it
	// hits exactly what would block the pawn EXCEPT the steppable bottom slice.
	const float FullR  = Cap->GetScaledCapsuleRadius();
	const float FullHH = Cap->GetScaledCapsuleHalfHeight();
	const float R      = FMath::Max(1.f, FullR - CapsuleInset);
	const float HalfHt = FMath::Max(R, FullHH - StepUpClearance * 0.5f);

	const FVector Start = Cap->GetComponentLocation() + FVector(0.f, 0.f, StepUpClearance * 0.5f);
	const FVector End   = Start + WorldDir * ProbeDistance;
	const FQuat   Rot   = Cap->GetComponentQuat();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AZ_MovementCapability), /*bTraceComplex*/ false, Owner);
	Params.AddIgnoredActor(Owner);

	FHitResult Hit;
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(R, HalfHt);
	const bool bHit = bUseCapsuleProfile
		? World->SweepSingleByProfile(Hit, Start, End, Rot, Cap->GetCollisionProfileName(), Shape, Params)
		: World->SweepSingleByChannel(Hit, Start, End, Rot, TraceChannel.GetValue(), Shape, Params);

	if (bDrawDebug)
	{
		DrawDebugCapsule(World, End, HalfHt, R, Rot, bHit ? FColor::Red : FColor::Green, false, 0.f);
		DrawDebugLine(World, Start, End, FColor::White, false, 0.f, 0, 0.5f);
	}

	// A wall = a mostly-vertical face that OPPOSES the move (rejects floors / ramps and back-facing geometry).
	if (bHit && Hit.bBlockingHit
		&& FMath::Abs(Hit.Normal.Z) <= MaxWallNormalZ
		&& FVector::DotProduct(Hit.Normal, WorldDir) < 0.f)
	{
		OutHit = Hit;
		return true;
	}
	return false;
}

FVector UAZ_MovementDirectionCapabilityComponent::ConstrainIntent(const FVector& WorldIntent)
{
	bBlockedThisQuery  = false;
	LastBlockingNormal = FVector::ZeroVector;

	FVector Intent = WorldIntent;
	Intent.Z = 0.f;
	if (Intent.SizeSquared() <= KINDA_SMALL_NUMBER)
	{
		return WorldIntent;   // no intent -> nothing to clamp
	}

	const FVector Dir = Intent.GetSafeNormal();
	FHitResult Hit;
	if (!SweepForWall(Dir, Hit))
	{
		return WorldIntent;   // clear -> pass through unchanged (full predictive intent)
	}

	// Flatten the wall normal to the ground plane — we only constrain horizontal movement (the floor handles Z).
	FVector N = Hit.Normal;
	N.Z = 0.f;
	if (!N.Normalize())
	{
		return WorldIntent;   // degenerate normal -> don't fight it
	}

	bBlockedThisQuery  = true;
	LastBlockingNormal = N;

	// Mover's EXACT slide projection, so the clamped intent matches how the real walking move resolves this wall.
	const FMovingComponentSet MovingComps(MoverComp.Get());
	FVector Slid = UMovementUtils::ComputeSlideDelta(MovingComps, Intent, 1.f, N, Hit);
	Slid.Z = 0.f;

	if (bDrawDebug && GetOwner())
	{
		const FVector Base = GetOwner()->GetActorLocation();
		DrawDebugLine(GetWorld(), Base, Base + Slid.GetSafeNormal() * 60.f, FColor::Cyan, false, 0.f, 0, 2.f);
	}

	// Facing (near) straight into the wall -> no usable tangent -> cancel the intent so the pawn idles instead of
	// running in place. Otherwise slide along the wall.
	if (Slid.SizeSquared() <= KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}
	if (FVector::DotProduct(Slid.GetSafeNormal(), Dir) < MinSlideAlignment)
	{
		return FVector::ZeroVector;
	}
	return Slid;
}

bool UAZ_MovementDirectionCapabilityComponent::IsDirectionClear(const FVector& WorldDir) const
{
	FVector D = WorldDir;
	D.Z = 0.f;
	if (!D.Normalize())
	{
		return true;
	}
	FHitResult Hit;
	return !SweepForWall(D, Hit);
}
