// Copyright Artur. AZ project.
#include "Animation/AZ_MoverAnimInstance.h"

#include "Animation/AZ_PairedHandContactComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "AZ_GameplayTags.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Character/AZ_PawnMoverHeroCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/IConsoleManager.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "MoverDataModelTypes.h"

void UAZ_MoverAnimInstance::ResetProceduralAnimationState()
{
	ProceduralFeetAlpha = ProceduralInteractionAlpha = InteractionLeftHandAlpha = 0.f;
	InteractionRightHandAlpha = 0.0;
	InteractionLeftHandTargetCS = InteractionRightHandTargetCS = FTransform::Identity;
	ProceduralGroundNormal = FVector::UpVector;
	ProceduralBasedMovementDelta = FTransform::Identity;
	bProceduralFeetRaycast = bProceduralFootPinning = bProceduralSlopeWarping = false;
	bProceduralHasTeleported = false;
	bProceduralFeetReset = true;
	PreviousProceduralMesh.Reset();
	PreviousProceduralMeshComponent.Reset();
	PreviousProceduralOwner.Reset();
	PreviousProceduralController.Reset();
	PreviousProceduralBase.Reset();
	PreviousProceduralBaseBone = NAME_None;
	PreviousProceduralBaseTransform = FTransform::Identity;
	PreviousProceduralLocation = PreviousProceduralVelocity = FVector::ZeroVector;
	PreviousProceduralMeshLocation = FVector::ZeroVector;
	bProceduralStateInitialized = bPreviousProceduralFeetEligible = bPreviousProceduralBaseValid = false;
	bProceduralFootBonesValid = false;
}

void UAZ_MoverAnimInstance::OnProceduralFeetBecameRelevant(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	// This only writes this animation instance's evaluation-owned reset pulse.
	bProceduralFeetReset = true;
}

void UAZ_MoverAnimInstance::NativePostEvaluateAnimation()
{
	Super::NativePostEvaluateAnimation();
	// Do not clear in Update: the rig must see the pulse during its evaluation.
	bProceduralFeetReset = false;
	bProceduralHasTeleported = false;
}

void UAZ_MoverAnimInstance::UpdateProceduralAnimation(float DeltaSeconds)
{
	check(IsInGameThread());
	USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	USkeletalMesh* MeshAsset = IsValid(Mesh) ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!IsValid(Cached_Pawn) || !IsValid(Cached_MoverComponent)
		|| !IsValid(Cached_CharacterMoverComponent) || !IsValid(MeshAsset) || Mesh->GetOwner() != Cached_Pawn)
	{
		ResetProceduralAnimationState();
		return;
	}
	const float Dt = FMath::IsFinite(DeltaSeconds) ? FMath::Max(0.f, DeltaSeconds) : 0.f;
	const bool bMeshChanged = PreviousProceduralMesh.Get() != MeshAsset || PreviousProceduralMeshComponent.Get() != Mesh
		|| PreviousProceduralOwner.Get() != Cached_Pawn || PreviousProceduralController.Get() != Cached_Pawn->GetController();
	if (bMeshChanged)
	{
		bProceduralFootBonesValid = true;
		for (const FName Bone : {FName(TEXT("root")), FName(TEXT("pelvis")),
			FName(TEXT("thigh_l")), FName(TEXT("calf_l")), FName(TEXT("foot_l")), FName(TEXT("ball_l")),
			FName(TEXT("thigh_r")), FName(TEXT("calf_r")), FName(TEXT("foot_r")), FName(TEXT("ball_r"))})
		{
			bProceduralFootBonesValid &= Mesh->GetBoneIndex(Bone) != INDEX_NONE;
		}
		ProceduralFeetAlpha = ProceduralInteractionAlpha = 0.f;
		if (!bProceduralFootBonesValid)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ProceduralFeet] Bypassing unsupported mesh %s: required leg/foot bones missing"), *GetNameSafe(MeshAsset));
		}
	}

	// Use finalized Mover floor/base data on the game thread. Rig inputs contain
	// values only: animation workers never query inventory, movement or other actors.
	const bool bGrounded = Cached_CharacterMoverComponent->IsOnGround();
	UPrimitiveComponent* Base = bGrounded ? Cached_MoverComponent->GetMovementBase() : nullptr;
	const FName BaseBone = Base ? Cached_MoverComponent->GetMovementBaseBoneName() : NAME_None;
	FVector BaseLocation = FVector::ZeroVector;
	FQuat BaseRotation = FQuat::Identity;
	const bool bBaseValid = IsValid(Base) && UBasedMovementUtils::GetMovementBaseTransform(Base, BaseBone, BaseLocation, BaseRotation);
	const FTransform BaseTransform(BaseRotation, BaseLocation);
	const bool bBaseChanged = PreviousProceduralBase.Get() != Base || PreviousProceduralBaseBone != BaseBone
		|| bPreviousProceduralBaseValid != bBaseValid;
	// Crossing an ordinary curb can change a stationary Movable floor base to
	// nullptr (static geometry). Keep the world-space root/pelvis/floor dampers
	// across continuous grounded handoffs; Reset would snap them to capsule Z.
	// Release the old foot pins below so they do not follow the new platform.
	const bool bGroundedBaseHandoff = bProceduralStateInitialized && bBaseChanged
		&& bGrounded && bPreviousProceduralFeetEligible;
	ProceduralBasedMovementDelta = FTransform::Identity;
	FTransform BasedWorldAffineDelta = FTransform::Identity;
	if (bProceduralStateInitialized && bBaseValid && !bBaseChanged)
	{
		// Previous world point -> previous base-local point -> current world point.
		BasedWorldAffineDelta = PreviousProceduralBaseTransform.Inverse() * BaseTransform;
		BasedWorldAffineDelta.NormalizeRotation();
		// The source rig rotates pinned points around its PREVIOUS MESH origin,
		// then adds this translation. Supply displacement at that origin, not the
		// affine transform's translation (which is relative to world zero).
		ProceduralBasedMovementDelta = FTransform(BasedWorldAffineDelta.GetRotation(),
			BasedWorldAffineDelta.TransformPosition(PreviousProceduralMeshLocation) - PreviousProceduralMeshLocation);
	}

	const FVector Location = Cached_Pawn->GetActorLocation();
	const FVector Velocity = Cached_Pawn->GetVelocity();
	const FVector PreviousBasedLocation = BasedWorldAffineDelta.TransformPosition(PreviousProceduralLocation);
	const float TravelAllowance = 2.f * FMath::Max(Velocity.Size(), PreviousProceduralVelocity.Size()) * Dt;
	const bool bUnexpectedDisplacement = bProceduralStateInitialized
		&& FVector::Dist(Location, PreviousBasedLocation) > FMath::Max(1.f, ProceduralTeleportThresholdCm) + TravelAllowance;
	const FMoverDefaultSyncState* Sync = Cached_MoverComponent->GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	bProceduralHasTeleported = bUnexpectedDisplacement || (Sync && Sync->bSkipInterpolation != 0);
	const bool bStanceChanged = bProceduralStateInitialized && PreviousProceduralStance != ChooserContext.Stance;
	if (!bProceduralStateInitialized || bMeshChanged || (bBaseChanged && !bGroundedBaseHandoff)
		|| bStanceChanged || bProceduralHasTeleported)
	{
		bProceduralFeetReset = true;
	}

	const FVector Up = Cached_MoverComponent->GetUpDirection();
	FHitResult FloorHit;
	const bool bValidFloor = bGrounded && Cached_MoverComponent->TryGetFloorCheckHitResult(FloorHit)
		&& FloorHit.bBlockingHit && !FloorHit.ImpactNormal.ContainsNaN() && !FloorHit.ImpactNormal.IsNearlyZero();
	const FVector DesiredNormal = bValidFloor ? FloorHit.ImpactNormal.GetSafeNormal() : Up;
	ProceduralGroundNormal = bProceduralFeetReset ? DesiredNormal
		: FMath::VInterpTo(ProceduralGroundNormal, DesiredNormal, Dt, 1.f / FMath::Max(.01f, ProceduralGroundNormalSmoothingSeconds)).GetSafeNormal();

	bool bFullBodyMontage = GetSlotMontageGlobalWeight(TEXT("FullBody")) > KINDA_SMALL_NUMBER;
	if (const UAnimMontage* Montage = GetCurrentActiveMontage())
	{
		for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
		{
			bFullBodyMontage |= Track.SlotName == TEXT("FullBody");
		}
	}
	const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();
	const bool bBlockedPose = Mesh->IsSimulatingPhysics() || bFullBodyMontage
		|| ChooserContext.OwnedTags.HasTag(Tags.Character_Dead)
		|| ChooserContext.OwnedTags.HasTag(Tags.State_Grabbed)
		|| ChooserContext.Reaction != EAZ_ObstacleReaction::None;
	// Simulated observers may have no local Mover floor-blackboard entry. The
	// rig performs its own ground traces; use up as their initial normal fallback.
	const bool bFeetEligible = bEnableProceduralFeet && bProceduralFootBonesValid && bGrounded && !bBlockedPose
		&& Mesh->GetPredictedLODLevel() <= ProceduralFeetMaxLOD;
	if (bFeetEligible && !bPreviousProceduralFeetEligible) bProceduralFeetReset = true;
	ProceduralFeetAlpha = ProceduralFeetBlendSeconds <= KINDA_SMALL_NUMBER ? (bFeetEligible ? 1.f : 0.f)
		: FMath::FInterpConstantTo(ProceduralFeetAlpha, bFeetEligible ? 1.f : 0.f, Dt, 1.f / ProceduralFeetBlendSeconds);
	bProceduralFeetRaycast = bFeetEligible;
	bProceduralSlopeWarping = bFeetEligible;
	const UAnimSequenceBase* SelectedSequence = Cast<UAnimSequenceBase>(BlendStackInputs.Anim);
	const bool bContactCurvesPresent = SelectedSequence && SelectedSequence->HasCurveData(TEXT("contact_l"))
		&& SelectedSequence->HasCurveData(TEXT("contact_r"));
	// The rig also evaluates the actual input-pose contact values; presence alone
	// never locks a foot. Missing and conservatively all-zero curves release pins.
	bProceduralFootPinning = bFeetEligible && bEnableProceduralFootPinning && bContactCurvesPresent
		&& !bProceduralHasTeleported && !bBaseChanged;
	// Disabling pinning for a handoff lets the rig fade the old lock and capture
	// a fresh contact. It preserves its existing smooth unlock/relock behavior,
	// while teleports and identity/relevance changes still take the hard reset.
	const IConsoleVariable* StepDebug = IConsoleManager::Get().FindConsoleVariable(TEXT("az.Cam.Debug"));
	if (StepDebug && StepDebug->GetInt() != 0 && Cached_Pawn->IsLocallyControlled())
	{
		UE_LOG(LogTemp, Display, TEXT("[ProceduralFeetTrace] pawn=%s f=%llu dt=%.4f ground=%d eligible=%d/%d alpha=%.3f reset=%d init=%d meshChanged=%d baseChanged=%d handoff=%d stanceChanged=%d teleport=%d skip=%d unexpected=%d base=%s>%s rawDz=%+.2f basedDz=%+.2f pin=%d raycast=%d slope=%d anim=%s"),
			*Cached_Pawn->GetName(), GFrameCounter, Dt, bGrounded, bPreviousProceduralFeetEligible, bFeetEligible,
			ProceduralFeetAlpha, bProceduralFeetReset, !bProceduralStateInitialized, bMeshChanged, bBaseChanged,
			bGroundedBaseHandoff, bStanceChanged, bProceduralHasTeleported, Sync && Sync->bSkipInterpolation != 0,
			bUnexpectedDisplacement, *GetNameSafe(PreviousProceduralBase.Get()), *GetNameSafe(Base),
			Location.Z - PreviousProceduralLocation.Z, PreviousBasedLocation.Z - PreviousProceduralLocation.Z,
			bProceduralFootPinning, bProceduralFeetRaycast, bProceduralSlopeWarping, *GetNameSafe(SelectedSequence));
	}

	FAZ_PairedHandContactSnapshot Hands;
	if (UAZ_PairedHandContactComponent* Contacts = Cached_Pawn->FindComponentByClass<UAZ_PairedHandContactComponent>())
	{
		Hands = Contacts->EvaluateHandContacts(Mesh, Dt);
	}
	InteractionLeftHandTargetCS = Hands.LeftTargetCS;
	InteractionRightHandTargetCS = Hands.RightTargetCS;
	InteractionLeftHandAlpha = Hands.LeftWeight;
	InteractionRightHandAlpha = static_cast<double>(Hands.RightWeight);
	const bool bHandsEligible = bEnableProceduralInteractionIK && !Mesh->IsSimulatingPhysics()
		&& !ChooserContext.OwnedTags.HasTag(Tags.Character_Dead) && Mesh->GetPredictedLODLevel() <= ProceduralInteractionMaxLOD
		&& (Hands.LeftWeight > KINDA_SMALL_NUMBER || Hands.RightWeight > KINDA_SMALL_NUMBER);
	// Per-hand contact weights own their ramp and release. This gate additionally
	// eases explicit enable/LOD changes without requiring the ended action's GUID.
	ProceduralInteractionAlpha = FMath::FInterpConstantTo(ProceduralInteractionAlpha,
		bHandsEligible ? 1.f : 0.f, Dt, bHandsEligible ? 10.f : 5.f);

	PreviousProceduralMesh = MeshAsset;
	PreviousProceduralMeshComponent = Mesh;
	PreviousProceduralOwner = Cached_Pawn;
	PreviousProceduralController = Cached_Pawn->GetController();
	PreviousProceduralBase = Base;
	PreviousProceduralBaseBone = BaseBone;
	PreviousProceduralBaseTransform = BaseTransform;
	bPreviousProceduralBaseValid = bBaseValid;
	PreviousProceduralLocation = Location;
	PreviousProceduralMeshLocation = Mesh->GetComponentLocation();
	PreviousProceduralVelocity = Velocity;
	PreviousProceduralStance = ChooserContext.Stance;
	bPreviousProceduralFeetEligible = bFeetEligible;
	bProceduralStateInitialized = true;
}
