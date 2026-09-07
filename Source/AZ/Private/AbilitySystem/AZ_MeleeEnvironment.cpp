// Copyright Artur. AZ project.
#include "AbilitySystem/AZ_MeleeEnvironment.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BonePose.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "MotionWarpingComponent.h"

namespace
{
	void IgnoreSelfAndAttachments(const AActor& Avatar, const AActor* IgnoreActor, FCollisionQueryParams& Params)
	{
		Params.AddIgnoredActor(&Avatar);
		if (IgnoreActor) Params.AddIgnoredActor(IgnoreActor);
		TArray<AActor*> Attached;
		Avatar.GetAttachedActors(Attached, true, true);
		Params.AddIgnoredActors(Attached);
	}

	FCollisionObjectQueryParams SceneryObjects()
	{
		FCollisionObjectQueryParams Objects(FCollisionObjectQueryParams::AllObjects);
		Objects.RemoveObjectTypesToQuery(ECC_Pawn);
		return Objects;
	}

	bool IsSolidScenery(const UPrimitiveComponent* Component)
	{
		return Component && Component->IsQueryCollisionEnabled()
			&& Component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block
			&& !Cast<APawn>(Component->GetOwner());
	}

	bool IsSupportingFloor(const AActor& Avatar, const FHitResult& Hit)
	{
		const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Avatar.GetRootComponent());
		if (!Capsule || Hit.ImpactNormal.Z < 0.65f) return false;
		const float FeetZ = Capsule->GetComponentLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
		return Hit.ImpactPoint.Z <= FeetZ + 4.f;
	}

	bool SweepShape(const AActor& Avatar, const FVector& Start, const FVector& End,
		const FQuat& Rotation, const FCollisionShape& Shape, FHitResult& OutHit,
		bool bIgnoreFloor, const AActor* IgnoreActor)
	{
		OutHit = FHitResult();
		const UWorld* World = Avatar.GetWorld();
		if (!World) return false;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AZMeleeEnvironment), false);
		IgnoreSelfAndAttachments(Avatar, IgnoreActor, Params);
		TArray<FHitResult> Hits;
		World->SweepMultiByObjectType(Hits, Start, End, Rotation, SceneryObjects(), Shape, Params);
		bool bBlocked = false;
		for (const FHitResult& Hit : Hits)
		{
			if (!IsSolidScenery(Hit.GetComponent()) || (bIgnoreFloor && IsSupportingFloor(Avatar, Hit))) continue;
			if (!bBlocked || Hit.Time < OutHit.Time)
			{
				OutHit = Hit;
				bBlocked = true;
			}
		}
		return bBlocked;
	}
}

bool FAZ_MeleeEnvironment::SweepEnvironment(const AActor& Avatar, const FVector& Start, const FVector& End,
	float Radius, FHitResult& OutHit, bool bIgnoreFloor, const AActor* IgnoreActor)
{
	return SweepShape(Avatar, Start, End, FQuat::Identity,
		FCollisionShape::MakeSphere(FMath::Max(0.1f, Radius)), OutHit, bIgnoreFloor, IgnoreActor);
}

bool FAZ_MeleeEnvironment::IsCapsulePathClear(const AActor& Avatar, const FVector& Start, const FVector& End,
	FHitResult& OutHit, const AActor* IgnoreActor)
{
	OutHit = FHitResult();
	const UWorld* World = Avatar.GetWorld();
	const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Avatar.GetRootComponent());
	if (!World || !Capsule) return false;
	// A tiny vertical inset avoids treating the support surface as an occupied destination. Preserve
	// full horizontal radius; this is clearance checking, not permission to squeeze through walls.
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = FMath::Max(Radius, Capsule->GetScaledCapsuleHalfHeight() - 0.5f);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	const FQuat Rotation = Capsule->GetComponentQuat();
	if (SweepShape(Avatar, Start, End, Rotation, Shape, OutHit, true, IgnoreActor)) return false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AZMeleeDestination), false);
	IgnoreSelfAndAttachments(Avatar, IgnoreActor, Params);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, End, Rotation, SceneryObjects(), Shape, Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (IsSolidScenery(Overlap.GetComponent()))
		{
			OutHit = FHitResult(Overlap.GetActor(), Overlap.GetComponent(), End, FVector::ZeroVector);
			OutHit.bBlockingHit = true;
			OutHit.bStartPenetrating = true;
			OutHit.Time = 1.f;
			return false;
		}
	}
	return true;
}

FVector FAZ_MeleeAttackTrajectory::GetSocketWorldLocation(const FAZ_MeleeAttackTrajectorySample& Sample, int32 SocketIndex,
	const FTransform& PlannedActorTransform) const
{
	check(Sample.ComponentSocketLocations.IsValidIndex(SocketIndex));
	return (MeshToActor * PlannedActorTransform).TransformPosition(Sample.ComponentSocketLocations[SocketIndex]);
}

bool FAZ_MeleeEnvironment::SampleAttackTrajectory(USkeletalMeshComponent& Mesh, const UAnimMontage& Montage,
	TConstArrayView<FName> SocketNames, float StartTime, float EndTime, FAZ_MeleeAttackTrajectory& OutTrajectory,
	float SampleInterval)
{
	OutTrajectory = FAZ_MeleeAttackTrajectory();
	const AActor* Avatar = Mesh.GetOwner();
	const UAnimInstance* Anim = Mesh.GetAnimInstance();
	if (!IsInGameThread() || !Avatar || !Anim || !Mesh.GetSkeletalMeshAsset() || SocketNames.IsEmpty()
		|| Montage.SlotAnimTracks.Num() != 1 || !FMath::IsFinite(StartTime) || !FMath::IsFinite(EndTime)
		|| !FMath::IsFinite(SampleInterval) || SampleInterval <= 0.f || EndTime < StartTime) return false;
	const FBoneContainer& Bones = Anim->GetRequiredBones();
	if (!Bones.IsValid()) return false;
	const FAnimTrack& Track = Montage.SlotAnimTracks[0].AnimTrack;
	if (Track.IsAdditive()) return false;
	StartTime = FMath::Clamp(StartTime, 0.f, Montage.GetPlayLength());
	EndTime = FMath::Clamp(EndTime, StartTime, Montage.GetPlayLength());
	const int32 Intervals = FMath::CeilToInt((EndTime - StartTime) / SampleInterval);
	if (Intervals > 600) return false; // malformed input must not stall the gameplay thread

	TArray<FCompactPoseBoneIndex> BoneIndices;
	TArray<FVector> SocketOffsets;
	for (const FName SocketName : SocketNames)
	{
		if (!Mesh.DoesSocketExist(SocketName)) return false;
		const USkeletalMeshSocket* Socket = Mesh.GetSocketByName(SocketName);
		const FName BoneName = Socket ? Socket->BoneName : SocketName;
		const int32 MeshBoneIndex = Mesh.GetBoneIndex(BoneName);
		if (MeshBoneIndex == INDEX_NONE) return false;
		const FCompactPoseBoneIndex BoneIndex = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshBoneIndex));
		if (BoneIndex == INDEX_NONE) return false;
		BoneIndices.Add(BoneIndex);
		SocketOffsets.Add(Socket ? Socket->RelativeLocation : FVector::ZeroVector);
	}

	FAZ_MeleeAttackTrajectory Result;
	Result.StartActorTransform = Avatar->GetActorTransform();
	const FTransform MeshToWorld = Mesh.GetComponentTransform();
	Result.MeshToActor = MeshToWorld.GetRelativeTransform(Result.StartActorTransform);
	Result.SocketNames.Append(SocketNames.GetData(), SocketNames.Num());
	for (const FName SocketName : SocketNames) Result.InitialWorldSocketLocations.Add(Mesh.GetSocketLocation(SocketName));
	const FTransform ActorInMesh = Result.StartActorTransform.GetRelativeTransform(MeshToWorld);
	Result.Samples.Reserve(Intervals + 1);
	for (int32 Index = 0; Index <= Intervals; ++Index)
	{
		const float Time = Index == Intervals ? EndTime : FMath::Min(EndTime, StartTime + Index * SampleInterval);
		const FAnimSegment* Segment = Track.GetSegmentAtTime(Time);
		if (!Segment || !Segment->IsValid() || !Segment->GetAnimReference()) return false;
		FMemMark Mark(FMemStack::Get());
		FCSPose<FCompactPose> Pose;
		UMotionWarpingUtilities::ExtractComponentSpacePose(&Montage, Bones, Time, true, Pose);
		FAZ_MeleeAttackTrajectorySample& Sample = Result.Samples.AddDefaulted_GetRef();
		Sample.Time = Time;
		Sample.RootMotion = Montage.ExtractRootMotionFromTrackRange(StartTime, Time, FAnimExtractContext());
		Sample.ActorTransform = ActorInMesh * (Sample.RootMotion * MeshToWorld);
		for (int32 SocketIndex = 0; SocketIndex < BoneIndices.Num(); ++SocketIndex)
		{
			const FTransform& BoneTransform = Pose.GetComponentSpaceTransform(BoneIndices[SocketIndex]);
			const FVector Location = BoneTransform.TransformPosition(SocketOffsets[SocketIndex]);
			if (Location.ContainsNaN()) return false;
			Sample.ComponentSocketLocations.Add(Location);
		}
	}
	OutTrajectory = MoveTemp(Result);
	return true;
}

bool FAZ_MeleeEnvironment::IsAttackTrajectoryClear(const AActor& Avatar, const FAZ_MeleeAttackTrajectory& Trajectory,
	TConstArrayView<FTransform> PlannedActors, float Radius, FHitResult& OutHit, float& OutMontageTime,
	const AActor* IgnoreActor, bool bIgnoreFloor)
{
	OutHit = FHitResult();
	OutMontageTime = 0.f;
	if (!Avatar.GetWorld() || Trajectory.Samples.IsEmpty() || Trajectory.SocketNames.IsEmpty()
		|| Trajectory.InitialWorldSocketLocations.Num() != Trajectory.SocketNames.Num()
		|| (!PlannedActors.IsEmpty() && PlannedActors.Num() != Trajectory.Samples.Num())) return false;
	TArray<FVector> Previous = Trajectory.InitialWorldSocketLocations;
	float PreviousTime = Trajectory.Samples[0].Time;
	for (int32 SampleIndex = 0; SampleIndex < Trajectory.Samples.Num(); ++SampleIndex)
	{
		const FAZ_MeleeAttackTrajectorySample& Sample = Trajectory.Samples[SampleIndex];
		if (Sample.ComponentSocketLocations.Num() != Previous.Num()) return false;
		const FTransform& ActorTransform = PlannedActors.IsEmpty() ? Sample.ActorTransform : PlannedActors[SampleIndex];
		bool bBlocked = false;
		float EarliestTime = TNumericLimits<float>::Max();
		for (int32 SocketIndex = 0; SocketIndex < Previous.Num(); ++SocketIndex)
		{
			const FVector Current = Trajectory.GetSocketWorldLocation(Sample, SocketIndex, ActorTransform);
			FHitResult Hit;
			if (SweepEnvironment(Avatar, Previous[SocketIndex], Current, Radius, Hit, bIgnoreFloor, IgnoreActor))
			{
				const float ContactTime = FMath::Lerp(PreviousTime, Sample.Time, Hit.Time);
				if (ContactTime < EarliestTime) { EarliestTime = ContactTime; OutHit = Hit; bBlocked = true; }
			}
			FVector Body = ActorTransform.GetLocation();
			Body.Z = Current.Z;
			if (SweepEnvironment(Avatar, Body, Current, Radius, Hit, bIgnoreFloor, IgnoreActor)
				&& Sample.Time < EarliestTime)
			{
				EarliestTime = Sample.Time;
				OutHit = Hit;
				bBlocked = true;
			}
			Previous[SocketIndex] = Current;
		}
		if (bBlocked) { OutMontageTime = EarliestTime; return false; }
		PreviousTime = Sample.Time;
	}
	return true;
}
