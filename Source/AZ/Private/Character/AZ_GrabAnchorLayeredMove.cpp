// Copyright Artur. AZ project.

#include "Character/AZ_GrabAnchorLayeredMove.h"

#include "AZ_GameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"   // grabber actor location for the facing turn
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"

FLayeredMove_AZ_GrabAnchor::FLayeredMove_AZ_GrabAnchor()
{
	// ONE OWNER for the held body — position AND facing (2026-09-02). This used to be OverrideVelocity,
	// leaving facing to the walking mode's spring via GA_PlayerGrabbed's OrientationIntent. That spring
	// starts AFTER the paired catch clips are already playing and turns at its own leisurely rate, so a
	// hero caught 60° off the line was still turning while the catch section played out — the Chalkie
	// behind him, both facing the same way (measured: hero 60°/4°/28° off the line at three catches,
	// Chalkie 8-9° every time). The PSI search already places the pair along the actors' line; the one
	// thing nothing guaranteed was the victim's yaw before frame one. Now this move turns him too, capped
	// fast enough to finish inside the grabber's close-in.
	MixMode = EMoveMixMode::OverrideAll;
	// Backstop only — the ability cancels by tag on every exit. Must outlive the grabber's MaxHoldSeconds.
	DurationMs = 20000.f;
}

bool FLayeredMove_AZ_GrabAnchor::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep,
	const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	if (!AnchorMesh || !GrabbedMesh)
	{
		return false;
	}

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;
	if (DeltaSeconds <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	// Where the holding hand IS, and where the held point IS. Both read live off the posed meshes, so the
	// lock tracks the grabber's arm animation instead of a frozen capsule-to-capsule offset.
	const FTransform AnchorXf = AnchorMesh->GetSocketTransform(AnchorSocket, RTS_World);
	const FVector DesiredSocketLoc = AnchorXf.TransformPosition(AnchorSpaceOffset);
	const FVector CurrentSocketLoc = GrabbedMesh->GetSocketTransform(GrabbedSocket, RTS_World).GetLocation();

	// Z is dropped unconditionally: Walking mode floor-snaps the capsule every tick, so proposing vertical
	// here is work the sim throws away. The lift lives on the mesh (see the header note).
	FVector Gap = DesiredSocketLoc - CurrentSocketLoc;
	Gap.Z = 0.f;

	// The mesh rides the capsule rigidly: moving the capsule by Gap moves the held socket by Gap.
	FVector Velocity = Gap / DeltaSeconds;
	if (MaxCorrectionSpeed > 0.f)
	{
		Velocity = Velocity.GetClampedToMaxSize(MaxCorrectionSpeed);
	}

	// FACING: turn the held body toward the grabber's capsule, capped. Read from the sync state, not the
	// actor, so a replay sees the same numbers. GrabFaceTurnRateDeg is a file constant for now — this ships
	// through Live Coding, and a new member on a USTRUCT would change its layout; promote it to a UPROPERTY
	// at the next closed-editor build.
	constexpr float GrabFaceTurnRateDeg = 720.f;   // 60° in 0.08s, 180° in 0.25s — inside GrabCloseSeconds for the common case
	float YawErrorDeg = 0.f;
	if (const FMoverDefaultSyncState* Sync = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
	{
		if (const AActor* Grabber = AnchorMesh->GetOwner())
		{
			const FVector ToGrabber = (Grabber->GetActorLocation() - Sync->GetLocation_WorldSpace()).GetSafeNormal2D();
			if (!ToGrabber.IsNearlyZero())
			{
				YawErrorDeg = FRotator::NormalizeAxis(
					static_cast<float>(ToGrabber.Rotation().Yaw - Sync->GetOrientation_WorldSpace().Yaw));
				const float YawRate = FMath::Clamp(YawErrorDeg / DeltaSeconds, -GrabFaceTurnRateDeg, GrabFaceTurnRateDeg);
				OutProposedMove.AngularVelocityDegrees = FVector(0.f, 0.f, YawRate);
			}
		}
	}

	// Instrumentation: the error we start with (and how long the cap needs), then a single sample at 0.3s —
	// the catch section is well under way by then, so this is "was the body aligned when it mattered".
	{
		const double SinceStartMs = TimeStep.BaseSimTimeMs - StartSimTimeMs;
		const bool bFirstTick = SinceStartMs < TimeStep.StepMs * 0.5;
		const bool bCross300 = SinceStartMs < 300.0 && (SinceStartMs + TimeStep.StepMs) >= 300.0;
		if (bFirstTick)
		{
			UE_LOG(LogTemp, Display, TEXT("[Grab] face %s: start err=%+.0fdeg -> aligned in %.2fs at %.0fdeg/s"),
				*GetNameSafe(GrabbedMesh->GetOwner()), YawErrorDeg, FMath::Abs(YawErrorDeg) / GrabFaceTurnRateDeg, GrabFaceTurnRateDeg);
		}
		else if (bCross300)
		{
			UE_LOG(LogTemp, Display, TEXT("[Grab] face %s: @0.3s err=%+.0fdeg (pass: |err| < 5)"),
				*GetNameSafe(GrabbedMesh->GetOwner()), YawErrorDeg);
		}
	}

	OutProposedMove.MixMode = MixMode;
	OutProposedMove.LinearVelocity = Velocity;
	return true;
}

bool FLayeredMove_AZ_GrabAnchor::IsFinished(double CurrentSimTimeMs) const
{
	// A grabber that got destroyed mid-hold must not leave a live move proposing motion toward a stale
	// transform — free the victim's capsule immediately, the ability's own release path handles the rest.
	if (!AnchorMesh || !GrabbedMesh)
	{
		return true;
	}
	return FLayeredMoveBase::IsFinished(CurrentSimTimeMs);
}

bool FLayeredMove_AZ_GrabAnchor::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	const FGameplayTag& AnchorTag = FAZ_GameplayTags::Get().Mover_GrabAnchor;
	return bExactMatch ? TagToFind.MatchesTagExact(AnchorTag) : AnchorTag.MatchesTag(TagToFind);
}

void FLayeredMove_AZ_GrabAnchor::GetGameplayTags(FGameplayTagContainer& InOutTags) const
{
	InOutTags.AddTag(FAZ_GameplayTags::Get().Mover_GrabAnchor);
}

FLayeredMoveBase* FLayeredMove_AZ_GrabAnchor::Clone() const
{
	return new FLayeredMove_AZ_GrabAnchor(*this);
}

void FLayeredMove_AZ_GrabAnchor::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	Ar << AnchorMesh;
	Ar << AnchorSocket;
	Ar << GrabbedMesh;
	Ar << GrabbedSocket;
	Ar << AnchorSpaceOffset;
	Ar << MaxCorrectionSpeed;
}

UScriptStruct* FLayeredMove_AZ_GrabAnchor::GetScriptStruct() const
{
	return FLayeredMove_AZ_GrabAnchor::StaticStruct();
}

FString FLayeredMove_AZ_GrabAnchor::ToSimpleString() const
{
	return FString::Printf(TEXT("AZ Grab Anchor (%s -> %s)"), *GrabbedSocket.ToString(), *AnchorSocket.ToString());
}

void FLayeredMove_AZ_GrabAnchor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(AnchorMesh);
	Collector.AddReferencedObject(GrabbedMesh);
}
