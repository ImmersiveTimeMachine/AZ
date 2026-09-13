// Copyright Artur. AZ project.
#include "Character/AZ_PawnMoverHeroCharacter.h"

#include "AbilitySystemComponent.h"
#include "AZ_GameplayTags.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "MoverDataModelTypes.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "MoveLibrary/MoverBlackboard.h"

namespace
{
	// Camera-frame samples must not consume the simulation-frame batched base
	// delegate. Weak ownership also makes this body-only correction safe to load
	// after the user's build; it can move into the pawn at a later layout build.
	struct FCameraBaseFrame
	{
		TWeakObjectPtr<UPrimitiveComponent> Base;
		FName Bone;
		FTransform AppliedTransform = FTransform::Identity;
		int32 LastTeleportFrame = INDEX_NONE;
		bool bHasTransform = false;
		bool bInitialized = false;
	};
	TMap<TWeakObjectPtr<AAZ_PawnMoverHeroCharacter>, FCameraBaseFrame> CameraBaseFrames;

	struct FCameraStepTrace
	{
		double Until = 0.0;
		uint64 InputFrame = MAX_uint64;
		uint64 LastOutputFrame = MAX_uint64;
		float PreviousActorZ = 0.f;
		float PreviousCameraZ = 0.f;
		float PreviousViewZ = 0.f;
		bool bHasFinalSample = false;
	};
	TMap<TWeakObjectPtr<AAZ_PawnMoverHeroCharacter>, FCameraStepTrace> CameraStepTraces;
	FDelegateHandle CameraStepEndFrameHandle;

	bool IsCameraStepTracingEnabled()
	{
		const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("az.Cam.Debug"));
		return CVar && CVar->GetInt() != 0;
	}

	void TraceCameraStepEndFrame()
	{
		const bool bEnabled = IsCameraStepTracingEnabled();
		for (auto It = CameraStepTraces.CreateIterator(); It; ++It)
		{
			AAZ_PawnMoverHeroCharacter* Pawn = It.Key().Get();
			if (!bEnabled || !IsValid(Pawn) || !Pawn->IsLocallyControlled() || !Pawn->GetWorld()
				|| !Pawn->GetWorld()->IsGameWorld() || !Pawn->GetCamera() || !Pawn->GetCameraBoom())
			{
				It.RemoveCurrent();
				continue;
			}
			FCameraStepTrace& Trace = It.Value();
			if (Trace.LastOutputFrame == Trace.InputFrame) continue;
			const USpringArmComponent* Boom = Pawn->GetCameraBoom();
			const float ActorZ = Pawn->GetActorLocation().Z;
			const float CameraZ = Pawn->GetCamera()->GetComponentLocation().Z;
			const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
			const APlayerCameraManager* Manager = PC ? PC->PlayerCameraManager : nullptr;
			const float ViewZ = Manager ? Manager->GetCameraCacheView().Location.Z : CameraZ;
			if (Pawn->GetWorld()->GetTimeSeconds() <= Trace.Until)
			{
				// EndFrame observes the published bone buffer. NativePostEvaluate
				// runs before that buffer flips and would report the previous pose.
				const USkeletalMeshComponent* Mesh = Pawn->GetMesh();
				const float HeadZ = Mesh ? Mesh->GetSocketLocation(TEXT("head")).Z : ActorZ;
				UE_LOG(LogTemp, Display, TEXT("[CameraStepEnd] pawn=%s f=%llu end=%llu actorZ=%.2f dActor=%+.2f headZ=%.2f camZ=%.2f dCam=%+.2f viewZ=%.2f dView=%+.2f cacheT=%.3f desiredZ=%.2f boomCollision=%d targetZ=%.2f pivotZ=%.2f rootZ=%.2f pelvisZ=%.2f footLZ=%.2f footRZ=%.2f"),
					*Pawn->GetName(), Trace.InputFrame, GFrameCounter, ActorZ,
					Trace.bHasFinalSample ? ActorZ - Trace.PreviousActorZ : 0.f, HeadZ, CameraZ,
					Trace.bHasFinalSample ? CameraZ - Trace.PreviousCameraZ : 0.f, ViewZ,
					Trace.bHasFinalSample ? ViewZ - Trace.PreviousViewZ : 0.f, Manager ? Manager->GetCameraCacheTime() : -1.f,
					Boom->GetUnfixedCameraPosition().Z, Boom->IsCollisionFixApplied(), Boom->TargetOffset.Z,
					Boom->GetComponentLocation().Z + Boom->TargetOffset.Z,
					Mesh ? Mesh->GetSocketLocation(TEXT("root")).Z : ActorZ,
					Mesh ? Mesh->GetSocketLocation(TEXT("pelvis")).Z : ActorZ,
					Mesh ? Mesh->GetSocketLocation(TEXT("foot_l")).Z : ActorZ,
					Mesh ? Mesh->GetSocketLocation(TEXT("foot_r")).Z : ActorZ);
			}
			Trace.PreviousActorZ = ActorZ;
			Trace.PreviousCameraZ = CameraZ;
			Trace.PreviousViewZ = ViewZ;
			Trace.bHasFinalSample = true;
			Trace.LastOutputFrame = Trace.InputFrame;
		}
		if (CameraStepTraces.IsEmpty())
		{
			FCoreDelegates::OnEndFrame.Remove(CameraStepEndFrameHandle);
			CameraStepEndFrameHandle.Reset();
		}
	}
}

void AAZ_PawnMoverHeroCharacter::ResetCameraHeightSmoothing()
{
	CameraBaseFrames.Remove(TWeakObjectPtr<AAZ_PawnMoverHeroCharacter>(this));
	CameraStepTraces.Remove(TWeakObjectPtr<AAZ_PawnMoverHeroCharacter>(this));
	if (bCameraHeightInitialized && CameraHeightBoom.IsValid())
	{
		// Remove only our terrain contribution when this camera stops being local.
		CameraHeightBoom->TargetOffset = CrouchCameraOffset;
	}
	CameraStepOffset = 0.f;
	CameraPendingBaseDisplacement = FVector::ZeroVector;
	bCameraHeightInitialized = false;
	bCameraPreviouslyGrounded = false;
	bCameraPreviousSkipInterpolation = false;
	CameraHeightController.Reset();
	CameraHeightBoom.Reset();
}

void AAZ_PawnMoverHeroCharacter::OnCameraBasedMovementApplied(const FTransform& TransformDelta, const FMoverTimeStep& TimeStep)
{
	// Retain the reflected callback for the already-loaded class. Base transforms
	// are now sampled with the camera, rather than consuming batched sim events.
}

void AAZ_PawnMoverHeroCharacter::UpdateCameraHeightSmoothing(float DeltaTime, float StanceInterpSpeed, bool bCinematicFraming)
{
	const AAZ_PawnMoverHeroCharacter* Defaults = GetClass()->GetDefaultObject<AAZ_PawnMoverHeroCharacter>();
	if (!Capsule || !MoverComponent || !GetWorld() || !CameraBoom || CameraBoom->GetAttachParent() != Capsule
		|| !Defaults || !Defaults->Capsule || !Defaults->CameraBoom)
	{
		ResetCameraHeightSmoothing();
		return;
	}
	const float Dt = FMath::IsFinite(DeltaTime) ? FMath::Max(0.f, DeltaTime) : 0.f;
	const double Now = GetWorld()->GetTimeSeconds();
	const FVector Up = MoverComponent->GetUpDirection().GetSafeNormal();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector FootPoint = Capsule->GetComponentLocation() - Up * HalfHeight;
	// Unlike capsule displacement, this velocity excludes StepUp, StepDown and
	// floor-snap substeps. Ramp travel remains in it, so smooth ramps stay direct.
	const FVector Velocity = MoverComponent->GetVelocity();
	const bool bGrounded = MoverComponent->IsOnGround();
	for (auto It = CameraBaseFrames.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}
	FCameraBaseFrame& BaseFrame = CameraBaseFrames.FindOrAdd(TWeakObjectPtr<AAZ_PawnMoverHeroCharacter>(this));
	UPrimitiveComponent* Base = MoverComponent->GetMovementBase();
	const FName BaseBone = Base ? MoverComponent->GetMovementBaseBoneName() : NAME_None;
	FRelativeBaseInfo AppliedBase;
	const UMoverBlackboard* Blackboard = MoverComponent->GetSimBlackboard();
	const bool bAppliedBaseValid = Base && Blackboard
		&& Blackboard->TryGet(CommonBlackboard::LastFoundDynamicMovementBase, AppliedBase)
		&& AppliedBase.MovementBase.Get() == Base && AppliedBase.BoneName == BaseBone;
	const FTransform AppliedBaseTransform(AppliedBase.Rotation, AppliedBase.Location);
	FVector PreviousFootOnCurrentBase = CameraPreviousFootPoint;
	bool bCanMeasureTerrain = BaseFrame.bInitialized;
	bool bStationaryHandoff = false;
	if (BaseFrame.bHasTransform)
	{
		FTransform CurrentOldBaseTransform;
		bool bHasCurrentOldBase = false;
		if (BaseFrame.Base.Get() == Base && BaseFrame.Bone == BaseBone && bAppliedBaseValid)
		{
			// This snapshot is updated whenever the base actually moves the pawn,
			// so it remains aligned even when physics base work runs PostPhysics.
			CurrentOldBaseTransform = AppliedBaseTransform;
			bHasCurrentOldBase = true;
		}
		else if (const UPrimitiveComponent* PreviousBase = BaseFrame.Base.Get())
		{
			// A normal step can change support from the floor to a block (or to
			// no tracked base). Do not discard that height change merely because
			// the component identity changed. If the old support stayed still,
			// there is no base motion/imparted velocity to subtract from the step.
			FVector OldLocation, LinearVelocity, PointVelocity;
			FQuat OldRotation;
			bStationaryHandoff = UBasedMovementUtils::GetMovementBaseTransform(PreviousBase, BaseFrame.Bone, OldLocation, OldRotation)
				&& OldLocation.Equals(BaseFrame.AppliedTransform.GetLocation(), .01)
				&& OldRotation.Equals(BaseFrame.AppliedTransform.GetRotation(), 0.000001)
				&& UBasedMovementUtils::GetMovementBaseVelocity(PreviousBase, LinearVelocity) && LinearVelocity.IsNearlyZero(.1)
				&& UBasedMovementUtils::GetMovementBaseVelocityAtPoint(PreviousBase, CameraPreviousFootPoint, PointVelocity)
				&& PointVelocity.IsNearlyZero(.1);
			if (bStationaryHandoff)
			{
				CurrentOldBaseTransform = BaseFrame.AppliedTransform;
				bHasCurrentOldBase = true;
			}
		}
		// Genuine moving-base departures still refresh their baseline without
		// adding an impulse: Mover can also impart that base's velocity.
		bCanMeasureTerrain &= bHasCurrentOldBase;
		if (bHasCurrentOldBase)
		{
			const FTransform Delta = BaseFrame.AppliedTransform.Inverse() * CurrentOldBaseTransform;
			PreviousFootOnCurrentBase = Delta.TransformPosition(CameraPreviousFootPoint);
		}
	}
	else if (BaseFrame.Base.IsValid()) bCanMeasureTerrain = false;
	const FMoverDefaultSyncState* Sync = MoverComponent->GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const bool bSkipInterpolation = Sync && Sync->bSkipInterpolation != 0;
	const bool bStanceRecenter = bCameraHeightInitialized && !FMath::IsNearlyEqual(HalfHeight, CameraPreviousHalfHeight, .1f);
	const int32 SimFrame = MoverComponent->GetLastTimeStep().ServerFrame;
	const bool bTeleportEvent = bSkipInterpolation && SimFrame != BaseFrame.LastTeleportFrame && !bStanceRecenter;
	if (bSkipInterpolation) BaseFrame.LastTeleportFrame = SimFrame;
	const float TravelAllowance = 2.f * static_cast<float>(FMath::Max(Velocity.Size(), CameraPreviousVelocity.Size())) * Dt;
	const FVector Travel = FootPoint - PreviousFootOnCurrentBase;
	const bool bLargeJump = bCameraHeightInitialized && Travel.Size() > 200.f + TravelAllowance;
	const bool bReset = !bCameraHeightInitialized || !BaseFrame.bInitialized || CameraHeightController.Get() != GetController()
		|| CameraHeightBoom.Get() != CameraBoom || FVector::DotProduct(CameraPreviousUp, Up) < .999
		|| Now < CameraHeightLastUpdateTime || Now - CameraHeightLastUpdateTime > .25
		|| bTeleportEvent || bLargeJump;
	const float OffsetIn = CameraStepOffset;
	const FVector ExpectedTravel = (Velocity + CameraPreviousVelocity) * (0.5f * Dt);
	const float RawVerticalTravel = FVector::DotProduct(FootPoint - CameraPreviousFootPoint, Up);
	const float VerticalTravel = FVector::DotProduct(Travel, Up);
	const float ExpectedVerticalTravel = FVector::DotProduct(ExpectedTravel, Up);
	const float Step = FVector::DotProduct(Travel - ExpectedTravel, Up);
	const UCommonLegacyMovementSettings* Settings = MoverComponent->FindSharedSettings<UCommonLegacyMovementSettings>();
	const float MaxStep = Settings ? Settings->MaxStepHeight : 40.f;
	const float StepLimit = FMath::Max(1.f, MaxStep) * 1.5f + 2.f;
	const TCHAR* StepGate = TEXT("accepted");
	float OffsetProposed = OffsetIn;

	// Preserve the existing crouch glide in its own state. A stair correction
	// must never be fed back through the crouch interpolation a second time.
	const float StandingHeight = Defaults->Capsule->GetUnscaledCapsuleHalfHeight()
		* static_cast<float>(Capsule->GetComponentTransform().GetScale3D().Z);
	const FVector HeightCorrection = Up * (StandingHeight - HalfHeight);
	const FVector DesiredCrouchOffset = Defaults->CameraBoom->TargetOffset - HeightCorrection;
	CameraBoom->SetRelativeLocation(Defaults->CameraBoom->GetRelativeLocation()
		+ Capsule->GetComponentTransform().InverseTransformVector(HeightCorrection));
	CrouchCameraOffset = bReset ? DesiredCrouchOffset
		: FMath::VInterpTo(CrouchCameraOffset, DesiredCrouchOffset, Dt, StanceInterpSpeed);

	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	const bool bBlocked = bCinematicFraming || (Mesh && Mesh->IsSimulatingPhysics())
		|| (ASC && ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Character_Dead));
	const float MaxOffset = FMath::IsFinite(CameraStepMaxOffset) ? FMath::Max(0.f, CameraStepMaxOffset) : 40.f;
	if (bReset || !bSmoothCameraSteps || bBlocked)
	{
		StepGate = bReset ? TEXT("reset") : (!bSmoothCameraSteps ? TEXT("disabled") : TEXT("blocked"));
		CameraStepOffset = 0.f;
		OffsetProposed = CameraStepOffset;
	}
	else
	{
		if (bGrounded && bCameraPreviouslyGrounded && bCanMeasureTerrain && Dt > SMALL_NUMBER)
		{
			if (FMath::IsFinite(Step) && FMath::Abs(Step) >= .5f && FMath::Abs(Step) <= StepLimit)
			{
				CameraStepOffset = FMath::Clamp(CameraStepOffset - Step, -MaxOffset, MaxOffset);
				UE_LOG(LogTemp, Display, TEXT("[CameraStep] step=%+.2f offset=%+.2f footZ=%.2f halfLife=%.3f"),
					Step, CameraStepOffset, FootPoint.Z, CameraStepSmoothingHalfLife);
			}
			else StepGate = !FMath::IsFinite(Step) ? TEXT("nonfinite") : (FMath::Abs(Step) < .5f ? TEXT("small") : TEXT("tooLarge"));
		}
		else StepGate = !bGrounded ? TEXT("air") : (!bCameraPreviouslyGrounded ? TEXT("wasAir") : (!bCanMeasureTerrain ? TEXT("baseHandoff") : TEXT("zeroDt")));
		OffsetProposed = CameraStepOffset;
		// The tail can finish in air; jumps/falls themselves never add lag.
		const float HalfLife = FMath::IsFinite(CameraStepSmoothingHalfLife) ? FMath::Max(.001f, CameraStepSmoothingHalfLife) : .08f;
		CameraStepOffset *= FMath::Exp(-0.69314718f * Dt / HalfLife);
	}
	CameraStepOffset = FMath::Clamp(CameraStepOffset, -MaxOffset, MaxOffset);
	const float OffsetDecayed = CameraStepOffset;

	// The offset belongs before the spring-arm collision solve. Keep its sweep
	// origin above the floor, then check the short vertical pivot shift itself so
	// a low ceiling cannot contain the origin before the ordinary arm sweep runs.
	const FVector Pivot = CameraBoom->GetComponentLocation() + CrouchCameraOffset;
	const float ProbeRadius = FMath::Max(0.f, CameraBoom->ProbeSize);
	const float MinimumClearance = ProbeRadius + 2.f;
	const float MaxDown = FMath::Max(0.f, static_cast<float>(FVector::DotProduct(Pivot - FootPoint, Up)) - MinimumClearance);
	CameraStepOffset = FMath::Max(CameraStepOffset, -MaxDown);
	const float OffsetAfterFloor = CameraStepOffset;
	bool bPivotHit = false;
	FHitResult PivotHit;
	if (CameraBoom->bDoCollisionTest && FMath::Abs(CameraStepOffset) > .05f)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CameraStepPivot), false, this);
		if (GetWorld()->SweepSingleByChannel(Hit, Pivot, Pivot + Up * CameraStepOffset, FQuat::Identity,
			CameraBoom->ProbeChannel, FCollisionShape::MakeSphere(FMath::Max(1.f, ProbeRadius)), Params))
		{
			bPivotHit = true;
			PivotHit = Hit;
			CameraStepOffset *= Hit.bStartPenetrating ? 0.f : FMath::Clamp(Hit.Time - .02f, 0.f, 1.f);
		}
	}
	if (FMath::Abs(CameraStepOffset) < .05f) CameraStepOffset = 0.f;
	CameraBoom->TargetOffset = CrouchCameraOffset + Up * CameraStepOffset;

	if (IsCameraStepTracingEnabled())
	{
		FCameraStepTrace& Trace = CameraStepTraces.FindOrAdd(TWeakObjectPtr<AAZ_PawnMoverHeroCharacter>(this));
		if (Trace.InputFrame == MAX_uint64 || (bCameraHeightInitialized && FMath::Abs(RawVerticalTravel) > .25f)
			|| FMath::Abs(OffsetIn) > .05f || FMath::Abs(CameraStepOffset) > .05f || bGrounded != bCameraPreviouslyGrounded)
		{
			Trace.Until = Now + .6;
		}
		Trace.InputFrame = GFrameCounter;
		if (!CameraStepEndFrameHandle.IsValid()) CameraStepEndFrameHandle = FCoreDelegates::OnEndFrame.AddStatic(&TraceCameraStepEndFrame);
		if (Now <= Trace.Until)
		{
			const uint32 ResetBits = (!bCameraHeightInitialized ? 1u : 0u) | (!BaseFrame.bInitialized ? 2u : 0u)
				| (CameraHeightController.Get() != GetController() ? 4u : 0u) | (CameraHeightBoom.Get() != CameraBoom ? 8u : 0u)
				| (FVector::DotProduct(CameraPreviousUp, Up) < .999 ? 16u : 0u) | (Now < CameraHeightLastUpdateTime ? 32u : 0u)
				| (Now - CameraHeightLastUpdateTime > .25 ? 64u : 0u) | (bTeleportEvent ? 128u : 0u) | (bLargeJump ? 256u : 0u);
			UE_LOG(LogTemp, Display, TEXT("[CameraStepTrace] v=3 pawn=%s f=%llu sim=%d dt=%.4f mode=%s footZ=%.2f rawDz=%+.2f basedDz=%+.2f expectedDz=%+.2f residual=%+.2f limit=%.2f velZ=%+.2f ground=%d/%d measure=%d base=%s applied=%d stationaryHandoff=%d skip=%d stance=%d reset=0x%x gate=%s enabled=%d halfLife=%.3f offset=%.2f>%.2f>%.2f>%.2f>%.2f pivotHit=%d penetrating=%d hitTime=%.3f hitActor=%s hitComp=%s crouchZ=%.2f targetZ=%.2f"),
				*GetName(), GFrameCounter, SimFrame, Dt, *MoverComponent->GetMovementModeName().ToString(), FootPoint.Z,
				RawVerticalTravel, VerticalTravel, ExpectedVerticalTravel, Step, StepLimit, Velocity.Z,
				bGrounded, bCameraPreviouslyGrounded, bCanMeasureTerrain, *GetNameSafe(Base), bAppliedBaseValid, bStationaryHandoff,
				bSkipInterpolation, bStanceRecenter, ResetBits, StepGate, bSmoothCameraSteps, CameraStepSmoothingHalfLife,
				OffsetIn, OffsetProposed, OffsetDecayed, OffsetAfterFloor, CameraStepOffset, bPivotHit,
				bPivotHit && PivotHit.bStartPenetrating, bPivotHit ? PivotHit.Time : 1.f,
				*GetNameSafe(PivotHit.GetActor()), *GetNameSafe(PivotHit.GetComponent()), CrouchCameraOffset.Z, CameraBoom->TargetOffset.Z);
		}
	}

	CameraPendingBaseDisplacement = FVector::ZeroVector;
	BaseFrame.Base = Base;
	BaseFrame.Bone = BaseBone;
	BaseFrame.AppliedTransform = AppliedBaseTransform;
	BaseFrame.bHasTransform = bAppliedBaseValid;
	BaseFrame.bInitialized = true;
	CameraPreviousFootPoint = FootPoint;
	CameraPreviousVelocity = Velocity;
	CameraPreviousUp = Up;
	CameraPreviousHalfHeight = HalfHeight;
	CameraHeightLastUpdateTime = Now;
	CameraHeightController = GetController();
	CameraHeightBoom = CameraBoom;
	bCameraPreviouslyGrounded = bGrounded;
	bCameraPreviousSkipInterpolation = bSkipInterpolation;
	bCameraHeightInitialized = true;
}
