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
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "MoverDataModelTypes.h"
#include "Animation/AZ_WeaponAnimationProfile.h"
#include "Weapon/AZ_Weapon.h"

namespace AZVisualMotion
{
	// Geometry shared by independently reset motion consumers. The foot-pin
	// reference/quiet/serial history is deliberately not owned by these helpers.
	FTransform SupportWorldAffineDelta(const FTransform& PreviousBase, const FTransform& CurrentBase)
	{
		FTransform Delta = PreviousBase.Inverse() * CurrentBase;
		Delta.NormalizeRotation();
		return Delta;
	}

	FVector ProjectHeading(const FVector& Heading, const FVector& Up)
	{
		return FVector::VectorPlaneProject(Heading, Up).GetSafeNormal();
	}

	double SignedHeadingAngleDeg(const FVector& From, const FVector& To, const FVector& Up)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Up, FVector::CrossProduct(From, To)),
			FVector::DotProduct(From, To)));
	}

	double OwnPlanarSpeed(const FVector& CurrentLocation, const FVector& PreviousLocation,
		const FTransform& BasedWorldDelta, const FVector& Up, float DeltaSeconds)
	{
		const FVector PreviousBasedLocation = BasedWorldDelta.TransformPosition(PreviousLocation);
		return FVector::VectorPlaneProject(CurrentLocation - PreviousBasedLocation, Up).Size() / DeltaSeconds;
	}
}

void UAZ_MoverAnimInstance::ResetVisualMotionState()
{
	VisualMotionSample = FVisualMotionSample{};
	VisualMotionHistory = FVisualMotionHistory{};
	AppliedAimStepDemand = FAppliedAimStepDemand{};
	ResetAppliedAimStepPlayback();
}

void UAZ_MoverAnimInstance::UpdateVisualMotionSample(float DeltaSeconds)
{
	check(IsInGameThread());
	FVisualMotionSample& Sample = VisualMotionSample;
	FVisualMotionHistory& History = VisualMotionHistory;
	Sample = FVisualMotionSample{};
	Sample.RawDeltaSeconds = DeltaSeconds;
	USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	USkeletalMesh* MeshAsset = IsValid(Mesh) ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const UWorld* World = GetWorld();
	if (!IsValid(Cached_Pawn) || !IsValid(Cached_MoverComponent)
		|| !IsValid(Cached_CharacterMoverComponent) || !IsValid(MeshAsset)
		|| Mesh->GetOwner() != Cached_Pawn || !World)
	{
		History = FVisualMotionHistory{};
		Sample.Continuity = EVisualMotionContinuity::SourcesInvalid;
		return;
	}
	Sample.bSourcesValid = true;
	Sample.WorldTimeSeconds = World->GetTimeSeconds();
	Sample.MeshTransform = Mesh->GetComponentTransform();
	const FVector UpDirection = Cached_MoverComponent->GetUpDirection();
	Sample.Up = UpDirection.GetSafeNormal();
	Sample.bGrounded = Cached_CharacterMoverComponent->IsOnGround();
	UPrimitiveComponent* Base = Sample.bGrounded ? Cached_MoverComponent->GetMovementBase() : nullptr;
	const FName BaseBone = Base ? Cached_MoverComponent->GetMovementBaseBoneName() : NAME_None;
	FVector BaseLocation = FVector::ZeroVector;
	FQuat BaseRotation = FQuat::Identity;
	const bool bBaseValid = IsValid(Base)
		&& UBasedMovementUtils::GetMovementBaseTransform(Base, BaseBone, BaseLocation, BaseRotation);
	const FTransform BaseTransform(BaseRotation, BaseLocation);
	Sample.bSourceChanged = History.bHistoryValid && (History.MeshAsset.Get() != MeshAsset
		|| History.MeshComponent.Get() != Mesh || History.Owner.Get() != Cached_Pawn
		|| History.Controller.Get() != Cached_Pawn->GetController()
		|| History.Mover.Get() != Cached_MoverComponent || History.CharacterMover.Get() != Cached_CharacterMoverComponent);
	Sample.bBaseChanged = History.bHistoryValid && (History.Base.Get() != Base
		|| History.BaseBone != BaseBone || History.bBaseValid != bBaseValid);
	if (Base && !bBaseValid)
	{
		// A known support with an unknown transform is not a static-world basis.
		// Retry without motion events; recovery must first seed fresh anchors.
		History.bHistoryValid = false;
		Sample.Continuity = EVisualMotionContinuity::BaseUnavailable;
		return;
	}
	if (!Sample.MeshTransform.IsValid() || !BaseTransform.IsValid()
		|| UpDirection.ContainsNaN() || Sample.Up.IsNearlyZero())
	{
		History.bHistoryValid = false;
		Sample.Continuity = EVisualMotionContinuity::InvalidTransform;
		return;
	}
	if (History.bHistoryValid && bBaseValid && !Sample.bBaseChanged && !Sample.bSourceChanged)
	{
		Sample.SupportWorldDelta = AZVisualMotion::SupportWorldAffineDelta(History.BaseTransform, BaseTransform);
	}
	if (!Sample.SupportWorldDelta.IsValid())
	{
		History.bHistoryValid = false;
		Sample.Continuity = EVisualMotionContinuity::InvalidTransform;
		return;
	}
	const FQuat SupportRotation = Sample.SupportWorldDelta.GetRotation();
	// Mover Up is the measurement/gravity plane, not an axis attached to the
	// support. A pitching/rolling base alone must not invalidate this plane.
	const bool bUpChanged = History.bHistoryValid && !History.Up.Equals(Sample.Up, KINDA_SMALL_NUMBER);
	const FMoverDefaultSyncState* Sync = Cached_MoverComponent->GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const bool bSkipInterpolation = Sync && Sync->bSkipInterpolation != 0;
	const double Elapsed = History.bHistoryValid ? Sample.WorldTimeSeconds - History.WorldTimeSeconds : 0.0;
	Sample.ElapsedSeconds = Elapsed;
	const FVector PreviousBasedLocation = Sample.SupportWorldDelta.TransformPosition(History.MeshLocation);
	Sample.SupportRelativeDistance = History.bHistoryValid
		? FVector::Dist(Sample.MeshTransform.GetLocation(), PreviousBasedLocation) : 0.0;

	EVisualMotionContinuity Reason = !History.bHistoryValid ? EVisualMotionContinuity::Initial
		: Sample.bSourceChanged ? EVisualMotionContinuity::SourceChanged
		: Sample.bBaseChanged ? EVisualMotionContinuity::BaseChanged
		: bUpChanged ? EVisualMotionContinuity::UpChanged
		: bSkipInterpolation ? EVisualMotionContinuity::SkipInterpolation
		: DeltaSeconds > VisualMotionMaxGapSeconds || Elapsed > VisualMotionMaxGapSeconds ? EVisualMotionContinuity::TimeGap
		: Sample.SupportRelativeDistance > VisualMotionDiscontinuityDistanceCm ? EVisualMotionContinuity::DiscontinuousTravel
		: EVisualMotionContinuity::Continuous;
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.f || !FMath::IsFinite(Sample.WorldTimeSeconds)
		|| (History.bHistoryValid && (!FMath::IsFinite(Elapsed) || Elapsed < 0.0)))
	{
		History.bHistoryValid = false;
		Sample.Continuity = EVisualMotionContinuity::InvalidTime;
		return;
	}
	const bool bReseed = Reason != EVisualMotionContinuity::Continuous;
	if (bReseed) History.bHistoryValid = false;
	const FQuat MeshRotation = Sample.MeshTransform.GetRotation();
	FVector HeadingAxis = History.bUseSecondaryHeadingAxis ? MeshRotation.GetAxisY() : MeshRotation.GetAxisX();
	if (HeadingAxis.ContainsNaN() || !HeadingAxis.IsNormalized())
	{
		History.bHistoryValid = false;
		Sample.Continuity = EVisualMotionContinuity::InvalidHeading;
		return;
	}
	Sample.Heading = AZVisualMotion::ProjectHeading(HeadingAxis, Sample.Up);
	if (!History.bHistoryValid && Sample.Heading.IsNearlyZero())
	{
		History.bUseSecondaryHeadingAxis = !History.bUseSecondaryHeadingAxis;
		HeadingAxis = History.bUseSecondaryHeadingAxis ? MeshRotation.GetAxisY() : MeshRotation.GetAxisX();
		Sample.Heading = AZVisualMotion::ProjectHeading(HeadingAxis, Sample.Up);
	}
	if (HeadingAxis.ContainsNaN() || !HeadingAxis.IsNormalized()
		|| Sample.Heading.ContainsNaN() || Sample.Heading.IsNearlyZero())
	{
		History.bHistoryValid = false;
		Sample.Continuity = EVisualMotionContinuity::InvalidHeading;
		return;
	}
	Sample.bGeometryValid = true;
	if (DeltaSeconds == 0.f || (History.bHistoryValid && Elapsed <= 0.0))
	{
		// Hold all anchors (including the support transform), so the next elapsed
		// sample still measures own motion instead of adopting a paused pose.
		Sample.Continuity = bReseed ? Reason : DeltaSeconds == 0.f
			? EVisualMotionContinuity::ZeroDeltaTime : EVisualMotionContinuity::TimeNotAdvanced;
		return;
	}
	if (!bReseed)
	{
		// Carry the full axis before projecting. Carrying a previously flattened
		// heading would invent own yaw when a support pitches or rolls.
		if (History.HeadingAxis.ContainsNaN() || !History.HeadingAxis.IsNormalized())
		{
			History.bHistoryValid = false;
			Sample.Continuity = EVisualMotionContinuity::InvalidHeading;
			return;
		}
		const FVector PreviousHeading = AZVisualMotion::ProjectHeading(
			SupportRotation.RotateVector(History.HeadingAxis), Sample.Up);
		if (PreviousHeading.IsNearlyZero())
		{
			History.bHistoryValid = false;
			Sample.Continuity = EVisualMotionContinuity::InvalidHeading;
			return;
		}
		Sample.OwnPlanarDisplacement = FVector::VectorPlaneProject(
			Sample.MeshTransform.GetLocation() - PreviousBasedLocation, Sample.Up);
		Sample.OwnPlanarSpeed = AZVisualMotion::OwnPlanarSpeed(Sample.MeshTransform.GetLocation(),
			History.MeshLocation, Sample.SupportWorldDelta, Sample.Up, static_cast<float>(Elapsed));
		Sample.OwnYawDeltaDeg = AZVisualMotion::SignedHeadingAngleDeg(PreviousHeading, Sample.Heading, Sample.Up);
		if (!FMath::IsFinite(Sample.OwnPlanarSpeed) || !FMath::IsFinite(Sample.OwnYawDeltaDeg))
		{
			History.bHistoryValid = false;
			Sample.Continuity = EVisualMotionContinuity::InvalidTransform;
			return;
		}
		Sample.bQualified = true;
	}
	Sample.bReseeded = bReseed;
	Sample.Continuity = Reason;
	History.MeshAsset = MeshAsset;
	History.MeshComponent = Mesh;
	History.Owner = Cached_Pawn;
	History.Controller = Cached_Pawn->GetController();
	History.Mover = Cached_MoverComponent;
	History.CharacterMover = Cached_CharacterMoverComponent;
	History.Base = Base;
	History.BaseBone = BaseBone;
	History.BaseTransform = BaseTransform;
	History.bBaseValid = bBaseValid;
	History.MeshLocation = Sample.MeshTransform.GetLocation();
	History.HeadingAxis = HeadingAxis;
	History.Up = Sample.Up;
	History.WorldTimeSeconds = Sample.WorldTimeSeconds;
	History.bHistoryValid = true;
}

void UAZ_MoverAnimInstance::UpdateAppliedAimStepDemand(bool bEligible,
	UAZ_WeaponAnimationProfile* Profile, AAZ_Weapon* Weapon)
{
	check(IsInGameThread());
	FAppliedAimStepDemand& Demand = AppliedAimStepDemand;
	Demand.bReset = Demand.bFrozen = false;
	const FVisualMotionSample& Motion = VisualMotionSample;
	const bool bFrozen = Motion.Continuity == EVisualMotionContinuity::ZeroDeltaTime
		|| Motion.Continuity == EVisualMotionContinuity::TimeNotAdvanced;
	const bool bEpisodeChanged = Demand.bEpisodeValid && (Demand.Profile.Get() != Profile
		|| Demand.Weapon.Get() != Weapon || Demand.Stance != ChooserContext.Stance);
	if (!bEligible || bEpisodeChanged || (!Motion.bQualified && !bFrozen))
	{
		Demand = FAppliedAimStepDemand{};
		Demand.bReset = true;
		return;
	}
	if (bFrozen)
	{
		Demand.bFrozen = true;
		return; // no angle, quiet-time, stationary or episode progress on paused/unqualified time
	}
	const auto FiniteOr = [](float Value, float Default) { return FMath::IsFinite(Value) ? Value : Default; };
	const double EnterSpeed = FMath::Max(0.f, FiniteOr(AimStepStationaryEnterSpeed, 2.f));
	const double ExitSpeed = FMath::Max(EnterSpeed + UE_DOUBLE_SMALL_NUMBER,
		static_cast<double>(FiniteOr(AimStepStationaryExitSpeed, 5.f)));
	const bool bStationary = Demand.bStationary ? Motion.OwnPlanarSpeed < ExitSpeed : Motion.OwnPlanarSpeed <= EnterSpeed;
	if (!bStationary)
	{
		Demand = FAppliedAimStepDemand{};
		Demand.bReset = true;
		return;
	}
	if (!Demand.bEpisodeValid)
	{
		Demand.Profile = Profile;
		Demand.Weapon = Weapon;
		Demand.Stance = ChooserContext.Stance;
		Demand.bEpisodeValid = Demand.bStationary = true;
		Demand.bReset = true;
		return; // eligibility starts a new episode; never inherit the preceding pose's turn
	}
	Demand.bStationary = true;
	const double EntryThreshold = FMath::Max(.001f, FiniteOr(AimStepEntryAngleDeg, 5.f));
	const double ReversalThreshold = FMath::Max(.001f, FiniteOr(AimStepReversalAngleDeg, 5.f));
	const double QuietDuration = FMath::Max(0.f, FiniteOr(AimStepQuietSeconds, .15f));
	const double QuietRange = FMath::Max(0.f, FiniteOr(AimStepQuietRangeDeg, .25f));
	const double Delta = Motion.OwnYawDeltaDeg;
	if (!Demand.bActive && !Demand.bPlaybackBlocked)
	{
		// Do not re-anchor during quiet windows: arbitrarily slow applied turns
		// still accumulate a meaningful angle instead of disappearing below a rate gate.
		Demand.EntryAngleDeg += Delta;
		if (FMath::Abs(Demand.EntryAngleDeg) >= EntryThreshold)
		{
			Demand.bActive = true;
			Demand.Direction = Demand.EntryAngleDeg > 0.0 ? 1 : -1;
			Demand.EntryAngleDeg = Demand.OppositeAngleDeg = 0.0;
			Demand.QuietHeadingDeg = Demand.QuietMinDeg = Demand.QuietMaxDeg = Demand.QuietSeconds = 0.0;
		}
		return;
	}
	if (Demand.bActive)
	{
		Demand.OppositeAngleDeg = FMath::Max(0.0, Demand.OppositeAngleDeg - Demand.Direction * Delta);
		if (Demand.OppositeAngleDeg >= ReversalThreshold)
		{
			Demand.Direction = -Demand.Direction;
			Demand.OppositeAngleDeg = 0.0; // pending direction; the SM owns the committed step boundary
		}
	}
	Demand.QuietHeadingDeg += Delta;
	Demand.QuietMinDeg = FMath::Min(Demand.QuietMinDeg, Demand.QuietHeadingDeg);
	Demand.QuietMaxDeg = FMath::Max(Demand.QuietMaxDeg, Demand.QuietHeadingDeg);
	if (Demand.QuietMaxDeg - Demand.QuietMinDeg > QuietRange)
	{
		Demand.QuietHeadingDeg = Demand.QuietMinDeg = Demand.QuietMaxDeg = Demand.QuietSeconds = 0.0;
	}
	else
	{
		Demand.QuietSeconds += Motion.ElapsedSeconds;
		if (Demand.QuietSeconds >= QuietDuration)
		{
			Demand.bActive = false;
			Demand.bPlaybackBlocked = false;
			Demand.Direction = 0;
			Demand.EntryAngleDeg = Demand.OppositeAngleDeg = 0.0;
			Demand.QuietHeadingDeg = Demand.QuietMinDeg = Demand.QuietMaxDeg = Demand.QuietSeconds = 0.0;
		}
	}
}

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
	ProceduralFootTurn = FProceduralFootTurnState{};
	bProceduralFootTurnHistoryResetPending = false;
	// Keep ProceduralFootPinReleaseSerial: an unevaluated rig must not lose its pending release request.
	bProceduralStateInitialized = bPreviousProceduralFeetEligible = bPreviousProceduralBaseValid = false;
	bProceduralFootBonesValid = false;
}

void UAZ_MoverAnimInstance::OnProceduralFeetBecameRelevant(const FAnimUpdateContext& Context, const FAnimNodeReference& Node)
{
	// This only writes this animation instance's evaluation-owned reset pulse.
	bProceduralFeetReset = true;
	bProceduralFootTurnHistoryResetPending = true;
}

void UAZ_MoverAnimInstance::RequestProceduralFootPinRelease()
{
	// Defined wrap; never overflow a signed int32 or return to the initial zero generation.
	ProceduralFootPinReleaseSerial = ProceduralFootPinReleaseSerial >= MAX_int32 || ProceduralFootPinReleaseSerial < 0
		? 1 : ProceduralFootPinReleaseSerial + 1;
}

void UAZ_MoverAnimInstance::UpdateProceduralFootTurnState(const FTransform& MeshTransform,
	const FTransform& BasedWorldAffineDelta, const FVector& UpDirection, float DeltaSeconds,
	bool bFeetEligible, bool bResetHistory, bool bBaseChanged)
{
	check(IsInGameThread());
	FProceduralFootTurnState& Turn = ProceduralFootTurn;
	if (!bFeetEligible)
	{
		Turn = FProceduralFootTurnState{};
		return;
	}
	const auto FiniteOr = [](float Value, float Fallback) { return FMath::IsFinite(Value) ? Value : Fallback; };
	const double EnterSpeed = FMath::Max(0.f, FiniteOr(ProceduralFootTurnStationaryEnterSpeed, 2.f));
	const double ExitSpeed = FMath::Max(EnterSpeed + UE_DOUBLE_SMALL_NUMBER,
		static_cast<double>(FiniteOr(ProceduralFootTurnStationaryExitSpeed, 5.f)));
	const double ReleaseAngle = FMath::Clamp(FiniteOr(ProceduralFootTurnReleaseAngleDeg, 1.f), .001f, 180.f);
	const double QuietDuration = FMath::Max(0.f, FiniteOr(ProceduralFootTurnQuietSeconds, .2f));
	const double QuietRange = FMath::Max(0.f, FiniteOr(ProceduralFootTurnQuietRangeDeg, .05f));
	const double ContinuityLimit = FMath::Max(.001f, FiniteOr(ProceduralFootTurnContinuitySeconds, .25f));
	const auto ResetQuiet = [&Turn](const FVector& Heading)
	{
		Turn.QuietReferenceHeading = Heading;
		Turn.QuietMinAngleDeg = Turn.QuietMaxAngleDeg = Turn.QuietSeconds = 0.0;
	};
	const auto MarkInvalid = [this, &Turn, &ResetQuiet]()
	{
		if (!Turn.bInvalidEpisode) RequestProceduralFootPinRelease();
		Turn.bInvalidEpisode = true;
		Turn.bSuppressed = true;
		Turn.bHistoryValid = Turn.bStationary = false;
		Turn.OwnPlanarSpeed = Turn.HeadingExcursionDeg = 0.0;
		ResetQuiet(FVector::ZeroVector);
	};
	const FVector Up = UpDirection.GetSafeNormal();
	if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.f || DeltaSeconds > ContinuityLimit
		|| UpDirection.ContainsNaN() || Up.IsNearlyZero() || !MeshTransform.IsValid()
		|| !BasedWorldAffineDelta.IsValid())
	{
		MarkInvalid();
		return;
	}
	const bool bHasElapsedTime = DeltaSeconds > 0.f;
	const bool bRecovered = bHasElapsedTime && Turn.bInvalidEpisode;
	if (bResetHistory || bBaseChanged || bRecovered) Turn.bHistoryValid = false;
	const auto ProjectHeading = [&Up](const FVector& Heading)
	{
		return AZVisualMotion::ProjectHeading(Heading, Up);
	};
	const FQuat MeshRotation = MeshTransform.GetRotation();
	FVector Heading = ProjectHeading(Turn.bUseSecondaryHeadingAxis ? MeshRotation.GetAxisY() : MeshRotation.GetAxisX());
	if (!Turn.bHistoryValid && Heading.IsNearlyZero())
	{
		Turn.bUseSecondaryHeadingAxis = !Turn.bUseSecondaryHeadingAxis;
		Heading = ProjectHeading(Turn.bUseSecondaryHeadingAxis ? MeshRotation.GetAxisY() : MeshRotation.GetAxisX());
	}
	if (Heading.ContainsNaN() || Heading.IsNearlyZero())
	{
		MarkInvalid();
		return;
	}
	if (!Turn.bHistoryValid)
	{
		if (bHasElapsedTime) Turn.bInvalidEpisode = false;
		// Identity/stance/teleport/relevance/base handoffs cannot become synthetic angular spikes.
		// Preserve suppression and the delivery serial; only the local measurement history is reseeded.
		Turn.PreviousHeading = Turn.PinReferenceHeading = Heading;
		ResetQuiet(Heading);
		// No qualified relative-motion sample yet; seed history without guessing that the body has stopped.
		Turn.OwnPlanarSpeed = -1.0;
		Turn.HeadingExcursionDeg = 0.0;
		Turn.bStationary = false;
		Turn.bHistoryValid = true;
		return;
	}
	const FQuat BaseRotationDelta = BasedWorldAffineDelta.GetRotation();
	Turn.PreviousHeading = ProjectHeading(BaseRotationDelta.RotateVector(Turn.PreviousHeading));
	Turn.PinReferenceHeading = ProjectHeading(BaseRotationDelta.RotateVector(Turn.PinReferenceHeading));
	Turn.QuietReferenceHeading = ProjectHeading(BaseRotationDelta.RotateVector(Turn.QuietReferenceHeading));
	if (Turn.PreviousHeading.IsNearlyZero() || Turn.PinReferenceHeading.IsNearlyZero()
		|| Turn.QuietReferenceHeading.IsNearlyZero())
	{
		MarkInvalid();
		return;
	}
	const auto AngleDeg = [&Up](const FVector& From, const FVector& To)
	{
		return AZVisualMotion::SignedHeadingAngleDeg(From, To, Up);
	};
	if (!bHasElapsedTime)
	{
		// A zero-dt sample neither advances settling nor proves a stop. Keep the carried references;
		// the next elapsed sample still sees any own-body rotation rather than silently adopting it.
		const double QuietAngle = AngleDeg(Turn.QuietReferenceHeading, Heading);
		Turn.QuietMinAngleDeg = FMath::Min(Turn.QuietMinAngleDeg, QuietAngle);
		Turn.QuietMaxAngleDeg = FMath::Max(Turn.QuietMaxAngleDeg, QuietAngle);
		if (Turn.QuietMaxAngleDeg - Turn.QuietMinAngleDeg > QuietRange) ResetQuiet(Heading);
		return;
	}
	Turn.OwnPlanarSpeed = AZVisualMotion::OwnPlanarSpeed(MeshTransform.GetLocation(),
		PreviousProceduralMeshLocation, BasedWorldAffineDelta, Up, DeltaSeconds);
	if (!FMath::IsFinite(Turn.OwnPlanarSpeed))
	{
		MarkInvalid();
		return;
	}
	Turn.bInvalidEpisode = false;
	const bool bWasStationary = Turn.bStationary;
	Turn.bStationary = bWasStationary
		? Turn.OwnPlanarSpeed < ExitSpeed
		: Turn.OwnPlanarSpeed <= EnterSpeed;
	if (!Turn.bStationary)
	{
		// Preserve normal moving/stop planting. This detector does not retroactively diagnose a pin
		// corrupted entirely during a moving turn before the body became stationary.
		// The pending serial, not an elapsed wall-clock fade, guarantees fresh capture on the next rig evaluation.
		Turn.bSuppressed = false;
		Turn.PinReferenceHeading = Turn.PreviousHeading = Heading;
		Turn.HeadingExcursionDeg = 0.0;
		ResetQuiet(Heading);
		return;
	}
	if (!bWasStationary) Turn.PinReferenceHeading = Turn.PreviousHeading;
	Turn.HeadingExcursionDeg = AngleDeg(Turn.PinReferenceHeading, Heading);
	Turn.PreviousHeading = Heading;
	if (!Turn.bSuppressed)
	{
		// Do not re-anchor this reference during quiet windows: a slow continuous turn must accumulate.
		if (FMath::Abs(Turn.HeadingExcursionDeg) >= ReleaseAngle)
		{
			RequestProceduralFootPinRelease();
			Turn.bSuppressed = true;
			ResetQuiet(Heading);
		}
		return;
	}
	const double QuietAngle = AngleDeg(Turn.QuietReferenceHeading, Heading);
	Turn.QuietMinAngleDeg = FMath::Min(Turn.QuietMinAngleDeg, QuietAngle);
	Turn.QuietMaxAngleDeg = FMath::Max(Turn.QuietMaxAngleDeg, QuietAngle);
	if (Turn.QuietMaxAngleDeg - Turn.QuietMinAngleDeg > QuietRange)
	{
		// Track angular range, so a reversal cannot cancel movement into a false quiet endpoint.
		ResetQuiet(Heading);
	}
	else
	{
		Turn.QuietSeconds += DeltaSeconds;
		if (Turn.QuietSeconds >= QuietDuration)
		{
			Turn.bSuppressed = false;
			Turn.PinReferenceHeading = Heading;
			Turn.HeadingExcursionDeg = 0.0;
			ResetQuiet(Heading);
		}
	}
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
		BasedWorldAffineDelta = AZVisualMotion::SupportWorldAffineDelta(PreviousProceduralBaseTransform, BaseTransform);
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
	const bool bResetTurnHistory = bProceduralFeetReset || bProceduralFootTurnHistoryResetPending;
	bProceduralFootTurnHistoryResetPending = false;
	UpdateProceduralFootTurnState(Mesh->GetComponentTransform(), BasedWorldAffineDelta, Up,
		DeltaSeconds, bFeetEligible, bResetTurnHistory, bBaseChanged);
	// The rig also evaluates the actual input-pose contact values; presence alone
	// never locks a foot. Missing and conservatively all-zero curves release pins.
	bProceduralFootPinning = bFeetEligible && bEnableProceduralFootPinning && bContactCurvesPresent
		&& !bProceduralHasTeleported && !bBaseChanged && !ProceduralFootTurn.bSuppressed;
	// Disabling pinning for a handoff lets the rig fade the old lock and capture
	// a fresh contact. It preserves its existing smooth unlock/relock behavior,
	// while teleports and identity/relevance changes still take the hard reset.
	const IConsoleVariable* StepDebug = IConsoleManager::Get().FindConsoleVariable(TEXT("az.Cam.Debug"));
	if (StepDebug && StepDebug->GetInt() != 0 && Cached_Pawn->IsLocallyControlled())
	{
		UE_LOG(LogTemp, Display, TEXT("[ProceduralFeetTrace] pawn=%s f=%llu dt=%.4f ground=%d eligible=%d/%d alpha=%.3f reset=%d init=%d meshChanged=%d baseChanged=%d handoff=%d stanceChanged=%d teleport=%d skip=%d unexpected=%d base=%s>%s rawDz=%+.2f basedDz=%+.2f pin=%d raycast=%d slope=%d anim=%s turnSuppressed=%d releaseSerial=%d ownSpeed=%.2f headingDeg=%+.3f quietSeconds=%.3f turnInvalid=%d"),
			*Cached_Pawn->GetName(), GFrameCounter, Dt, bGrounded, bPreviousProceduralFeetEligible, bFeetEligible,
			ProceduralFeetAlpha, bProceduralFeetReset, !bProceduralStateInitialized, bMeshChanged, bBaseChanged,
			bGroundedBaseHandoff, bStanceChanged, bProceduralHasTeleported, Sync && Sync->bSkipInterpolation != 0,
			bUnexpectedDisplacement, *GetNameSafe(PreviousProceduralBase.Get()), *GetNameSafe(Base),
			Location.Z - PreviousProceduralLocation.Z, PreviousBasedLocation.Z - PreviousProceduralLocation.Z,
			bProceduralFootPinning, bProceduralFeetRaycast, bProceduralSlopeWarping, *GetNameSafe(SelectedSequence),
			ProceduralFootTurn.bSuppressed, ProceduralFootPinReleaseSerial, ProceduralFootTurn.OwnPlanarSpeed,
			ProceduralFootTurn.HeadingExcursionDeg, ProceduralFootTurn.QuietSeconds, ProceduralFootTurn.bInvalidEpisode);
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
