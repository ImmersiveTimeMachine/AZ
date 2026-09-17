// Copyright Artur. AZ project.

#include "Throwables/AZ_ThrowLaunchSolver.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/GameplayStaticsTypes.h"
#include "Throwables/AZ_ThrowPresentationProfile.h"
#include "Throwables/AZ_ThrowableDefinition.h"

namespace
{
	/** The additive profile the runtime sphere uses. Names the contract in one place for both sides. */
	const FName ThrowableProfileName(TEXT("ThrowableProjectile"));

	/**
	 * ★ COLLISION IS BILATERAL, and this is the whole reason this helper exists.
	 *
	 * A bare channel sweep only asks "does the thing I hit block channel X". The real moving sphere ALSO has
	 * its own response container and ignore rules, and Unreal resolves a contact from BOTH sides. So
	 * "object type Projectile, therefore trace Ch2, therefore identical blockers" is false in general.
	 *
	 * Concrete case: this profile ignores Ability objects. An Ability object that blocks Ch2 would stop a
	 * bare Ch2 prediction sweep while the actual throwable sails straight through it — the preview would
	 * draw a contact that never happens.
	 *
	 * PredictProjectilePath cannot express this: its channel sweep passes query params but no per-projectile
	 * response container (GameplayStatics.cpp, the sweep around :2887), unlike PrimitiveComponent's
	 * InitSweepCollisionParams (:3201), which copies the moving body's responses, ignored actors/components,
	 * complexity and ignore mask. So the ballistic samples are taken WITHOUT native tracing and each segment
	 * is swept here with the profile's real channel and response container.
	 */
	bool GetThrowContract(ECollisionChannel& OutChannel, FCollisionResponseParams& OutResponse)
	{
		return UCollisionProfile::GetChannelAndResponseParams(ThrowableProfileName, OutChannel, OutResponse);
	}
}

FAZ_ThrowLaunchSolution UAZ_ThrowLaunchSolver::BuildSolution(
	const APawn* Thrower,
	const USkeletalMeshComponent* Mesh,
	const UAZ_ThrowableDefinition* Definition,
	const UAZ_ThrowPresentationProfile* Profile,
	const EAZ_ThrowArc Arc,
	const float AimDistance,
	const FRotator& AimRotation,
	const bool bUseLiveGrip)
{
	FAZ_ThrowLaunchSolution Out;
	Out.Arc = Arc;
	if (!Thrower || !Mesh || !Definition || !Profile)
	{
		return Out;   // Status stays NoDefinition.
	}
	const UWorld* World = Thrower->GetWorld();
	if (!World)
	{
		return Out;
	}

	Out.Radius   = Definition->CollisionRadius;
	Out.GravityZ = World->GetGravityZ() * Definition->GravityScale;

	// ---- Origin ---------------------------------------------------------------------------------
	// While aiming, the arm has not thrown yet, so the live hand is in the Loop pose and is NOT where the
	// object will leave. The calibrated anchor is a full transform in MESH COMPONENT space (the space the
	// pose was sampled in), composed through Mesh-to-World and then the item's grip offset — a component
	// space XYZ is not an actor-space origin and must never be used as one directly.
	// At the cue the live bone is authoritative and the geometry is rechecked.
	const FTransform GripToWorld = (bUseLiveGrip && Mesh->DoesSocketExist(Profile->GripBone))
		? Mesh->GetSocketTransform(Profile->GripBone)
		: Profile->GetReleaseAnchor(Arc) * Mesh->GetComponentTransform();
	const FTransform ReleaseToWorld = Profile->GripOffset * GripToWorld;
	// Per-arc vertical lift, in WORLD up. With one throw shape serving both arcs this is what differentiates
	// a close toss from a long one alongside speed: the motion is identical, the object simply leaves a
	// little higher so the slower, shorter arc still clears the thrower's own body.
	// Both the lift and the speed below vary CONTINUOUSLY with the aim distance. Stepping between two fixed
	// values made a band of distances unreachable: range goes as v-squared, so the far speed lands ~3x
	// further than the close one at the same angle, and nothing could be placed in the gap.
	Out.Origin = ReleaseToWorld.GetLocation() + FVector(0.f, 0.f, Definition->GetOriginLiftForAim(AimDistance));

	// ---- Velocity -------------------------------------------------------------------------------
	// Fixed speed along the accepted aim direction. Deliberately no target solve and no charge: holding the
	// button aims, it does not add power, so an unreachable target shows an honest short arc rather than a
	// silently boosted one.
	const FVector Direction = AimRotation.Vector().GetSafeNormal();
	Out.Velocity = Direction * Definition->GetSpeedForAim(AimDistance)
		+ Thrower->GetVelocity() * FMath::Clamp(Definition->VelocityInheritance, 0.f, 1.f);

	// ---- Launch clearance -----------------------------------------------------------------------
	// A shoulder camera routinely sees over cover the hand cannot clear. Sweep from the capsule's interior
	// axis at the release height out to the release point: if that is blocked the hand is behind geometry,
	// and the throw is refused BEFORE anything is spent — never "fixed" by spawning past the wall.
	// Same guard the firearm uses for a barrel poking through a wall (AZ_GA_FirearmFire.cpp:103), and it
	// uses the same bilateral contract as the flight prediction so the two agree about what blocks.
	ECollisionChannel Channel;
	FCollisionResponseParams Response;
	if (!GetThrowContract(Channel, Response))
	{
		return Out;
	}

	// The capsule is the pawn's ROOT component here; the hero is a Mover APawn and has no
	// ACharacter::GetCapsuleComponent(). Same resolution the firearm clearance check uses.
	FVector SafeOrigin = Thrower->GetActorLocation();
	if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Thrower->GetRootComponent()))
	{
		const FVector Axis = Capsule->GetUpVector();
		const float AxisExtent = FMath::Max(0.f,
			Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius());
		SafeOrigin = Capsule->GetComponentLocation() + Axis * FMath::Clamp(
			FVector::DotProduct(Out.Origin - Capsule->GetComponentLocation(), Axis), -AxisExtent, AxisExtent);
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AZThrowClearance), false);
	Params.AddIgnoredActor(Thrower);
	FHitResult Blocked;
	if (World->SweepSingleByChannel(Blocked, SafeOrigin, Out.Origin, FQuat::Identity, Channel,
		FCollisionShape::MakeSphere(FMath::Max(1.f, Out.Radius)), Params, Response))
	{
		Out.Status = EAZ_ThrowSolutionStatus::BlockedAtHand;
		return Out;
	}

	Out.Status = EAZ_ThrowSolutionStatus::Valid;
	return Out;
}

float UAZ_ThrowLaunchSolver::MeasureAimDistance(
	const UObject* WorldContext,
	const FVector& ViewLocation,
	const FRotator& AimRotation,
	const FVector& MeasureFrom,
	const AActor* IgnoredActor,
	const float MaxRange)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const float Range = FMath::Max(1.f, MaxRange);
	if (!World)
	{
		return Range;
	}
	// Visibility, not the throwable contract: this asks "what is the player looking at", which is a
	// question about the camera's line of sight, not about what a thrown object would collide with.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AZThrowAim), false);
	if (IgnoredActor)
	{
		Params.AddIgnoredActor(IgnoredActor);
	}
	FHitResult Hit;
	const FVector End = ViewLocation + AimRotation.Vector().GetSafeNormal() * Range;
	if (!World->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params))
	{
		return Range;   // open space IS the far case
	}
	return FMath::Min(Range, static_cast<float>(FVector::Dist(MeasureFrom, Hit.ImpactPoint)));
}

FAZ_ThrowPreviewResult UAZ_ThrowLaunchSolver::Predict(
	const UObject* WorldContext,
	const FAZ_ThrowLaunchSolution& Solution,
	const AActor* IgnoredActor,
	const float Horizon,
	const float Frequency)
{
	FAZ_ThrowPreviewResult Out;
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || !Solution.IsValid())
	{
		return Out;
	}
	ECollisionChannel Channel;
	FCollisionResponseParams Response;
	if (!GetThrowContract(Channel, Response))
	{
		return Out;
	}

	// ---- 1. Ballistic samples, no native tracing ------------------------------------------------
	// Collision is resolved below instead, because the predictor's own sweep cannot carry the projectile's
	// response container and would therefore disagree with the real flight (see GetThrowContract).
	FPredictProjectilePathParams Params;
	Params.StartLocation = Solution.Origin;
	Params.LaunchVelocity = Solution.Velocity;
	Params.ProjectileRadius = FMath::Max(1.f, Solution.Radius);
	Params.MaxSimTime = FMath::Max(0.05f, Horizon);
	// Set explicitly: the C++ default is 20Hz while the Blueprint wrappers default to 15, and inheriting
	// either silently would make the displayed arc depend on which entry point happened to be used.
	Params.SimFrequency = FMath::Max(1.f, Frequency);
	Params.bTraceWithCollision = false;
	// Passed through explicitly because the predictor reads OverrideGravityZ == 0 as "use world gravity",
	// so a genuine zero can never be expressed. Solutions therefore never carry 0 here.
	Params.OverrideGravityZ = Solution.GravityZ;

	FPredictProjectilePathResult Result;
	UGameplayStatics::PredictProjectilePath(WorldContext, Params, Result);
	if (Result.PathData.Num() == 0)
	{
		return Out;
	}

	// ---- 2. Sweep each segment with the REAL bilateral contract ----------------------------------
	FCollisionQueryParams Query(SCENE_QUERY_STAT(AZThrowPredict), false);
	if (IgnoredActor)
	{
		Query.AddIgnoredActor(IgnoredActor);
	}
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(FMath::Max(1.f, Solution.Radius));

	Out.Points.Reserve(Result.PathData.Num());
	Out.Points.Add(Result.PathData[0].Location);
	for (int32 Index = 1; Index < Result.PathData.Num(); ++Index)
	{
		const FPredictProjectilePathPointData& Prev = Result.PathData[Index - 1];
		const FPredictProjectilePathPointData& Next = Result.PathData[Index];

		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, Prev.Location, Next.Location, FQuat::Identity, Channel,
			Sphere, Query, Response))
		{
			// First blocking contact ends the displayed arc. Deliberately no bounce or rest prediction:
			// the shared flight contract stops here, and advertising a resting place from it would be a
			// lie — a first bounce is not where a grenade detonates.
			Out.Points.Add(Hit.Location);
			Out.bHitBlocking = true;
			// ImpactPoint, not Hit.Location: the latter is the swept SPHERE CENTRE, which sits one radius
			// off the surface and would hang the contact marker in the air in front of a wall.
			Out.ImpactPoint = Hit.ImpactPoint;
			Out.ImpactNormal = Hit.ImpactNormal;
			Out.HitComponent = Hit.GetComponent();
			// Contact time interpolated across the segment the hit fell in, so it stays comparable to the
			// sample times regardless of the chosen simulation frequency.
			Out.TimeToImpact = FMath::Lerp(Prev.Time, Next.Time, FMath::Clamp(Hit.Time, 0.f, 1.f));
			return Out;
		}
		Out.Points.Add(Next.Location);
	}

	// Nothing struck inside the horizon. That is an ordinary throw into open space, NOT a refusal and not a
	// reason to fabricate a marker: the caller fades the line end and draws nothing.
	return Out;
}
