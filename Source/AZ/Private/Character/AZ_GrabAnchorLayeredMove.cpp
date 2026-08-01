// Copyright Artur. AZ project.

#include "Character/AZ_GrabAnchorLayeredMove.h"

#include "AZ_GameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"

FLayeredMove_AZ_GrabAnchor::FLayeredMove_AZ_GrabAnchor()
{
	// Replace the mode's own velocity, keep its angular: the victim must still turn to face the grabber
	// (GA_PlayerGrabbed drives OrientationIntent) while we own where the body goes.
	MixMode = EMoveMixMode::OverrideVelocity;
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
