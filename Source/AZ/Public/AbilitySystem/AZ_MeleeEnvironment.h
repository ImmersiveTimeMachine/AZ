// Copyright Artur. AZ project.
#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"

class AActor;
class UAnimMontage;
class USkeletalMeshComponent;

struct FAZ_MeleeAttackTrajectorySample
{
	float Time = 0.f;
	/** Authored local root displacement from the requested StartTime, respecting enabled root motion. */
	FTransform RootMotion = FTransform::Identity;
	/** Root-motion-extracted slot pose, on the active mesh's required bones, including socket offsets. */
	TArray<FVector> ComponentSocketLocations;
	/** Actor transform if the authored root motion plays without warping, collision or movement input. */
	FTransform ActorTransform = FTransform::Identity;
};

struct AZ_API FAZ_MeleeAttackTrajectory
{
	FTransform StartActorTransform = FTransform::Identity;
	FTransform MeshToActor = FTransform::Identity;
	TArray<FName> SocketNames;
	TArray<FVector> InitialWorldSocketLocations;
	TArray<FAZ_MeleeAttackTrajectorySample> Samples;

	/** Use a supplied planned actor transform when checking a warp; no fixed mesh yaw/scale assumptions. */
	FVector GetSocketWorldLocation(const FAZ_MeleeAttackTrajectorySample& Sample, int32 SocketIndex,
		const FTransform& PlannedActorTransform) const;
};

/** Native combat environment queries; no object instance or reflection is required. */
struct AZ_API FAZ_MeleeEnvironment
{
	/** First solid scenery contact, ordered by sweep time. Pawns, attachments and triggers are excluded.
	 * bIgnoreFloor only excludes upward-facing support near the avatar's feet, not table tops. */
	static bool SweepEnvironment(const AActor& Avatar, const FVector& Start, const FVector& End,
		float Radius, FHitResult& OutHit, bool bIgnoreFloor = false, const AActor* IgnoreActor = nullptr);

	/** Validate a straight path and destination using the avatar's root capsule. Start/End are world
	 * capsule centres. Returns false if the actor has no capsule or world (clearance is unknown). */
	static bool IsCapsulePathClear(const AActor& Avatar, const FVector& Start, const FVector& End,
		FHitResult& OutHit, const AActor* IgnoreActor = nullptr);

	/** Game-thread preflight sampling of a single non-additive montage slot. Uses the active mesh's bone
	 * container and the engine montage-track extractor, including retargeting and socket offsets.
	 * Does not evaluate montage blending, linked layers or postprocess/control rigs. Failure is explicit;
	 * callers must not treat missing bones, invalid tracks, or unsupported additive/multi-slot clips as clear.
	 * Root-motion delta is separate from the extracted pose so callers can validate warped body paths. */
	static bool SampleAttackTrajectory(USkeletalMeshComponent& Mesh, const UAnimMontage& Montage,
		TConstArrayView<FName> SocketNames, float StartTime, float EndTime, FAZ_MeleeAttackTrajectory& OutTrajectory,
		float SampleInterval = 1.f / 60.f);

	/** Sweep the sampled limbs and body-to-limb corridors in chronological order. Empty PlannedActors
	 * uses authored root travel; otherwise supply one actor transform for every sample (e.g. a warp).
	 * Checks the initial displayed limb -> first authored pose as a conservative blend-in corridor.
	 * This is a finite-sample preflight, not a replacement for runtime contact resolution. */
	static bool IsAttackTrajectoryClear(const AActor& Avatar, const FAZ_MeleeAttackTrajectory& Trajectory,
		TConstArrayView<FTransform> PlannedActors, float Radius, FHitResult& OutHit, float& OutMontageTime,
		const AActor* IgnoreActor = nullptr, bool bIgnoreFloor = true);
};
