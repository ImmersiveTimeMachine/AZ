
#include "Animation/AZ_CmcAnimInstance.h"

#include "Character/Cmc/AZ_CmcCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "AZ_GameplayTags.h"
#include "Animation/AZ_LocomotionStateMachine.h"
#include "Animation/AZ_MoverAnimInstance.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequence.h"
#include "Character/AZ_ObstacleSensorComponent.h"
#include "Chooser.h"
#include "Character/Cmc/AZ_CmcHeroCharacter.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "BlendStack/BlendStackAnimNodeLibrary.h"
#include "AnimationWarpingLibrary.h"
#include "PoseSearch/MotionMatchingAnimNodeLibrary.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "BoneControllers/AnimNode_OffsetRootBone.h"
#include "Animation/AnimClassInterface.h"

namespace AZ::CmcAnim
{
	static float SignedYawTo(const FVector& WorldDir, float BaseYawDeg)
	{
		const FVector Dir = WorldDir.GetSafeNormal2D();
		if (Dir.IsNearlyZero())
		{
			return 0.f;
		}
		const FRotationMatrix Basis(FRotator(0.f, BaseYawDeg, 0.f));
		return FMath::RadiansToDegrees(FMath::Atan2(
			FVector::DotProduct(Dir, Basis.GetUnitAxis(EAxis::Y)),
			FVector::DotProduct(Dir, Basis.GetUnitAxis(EAxis::X))));
	}
}

UAZ_CmcAnimInstance::UAZ_CmcAnimInstance()
{
	RootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
}

void UAZ_CmcAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	Cached_Character = Cast<AAZ_CmcCharacterBase>(TryGetPawnOwner());

	StateMachine = NewObject<UAZ_LocomotionStateMachine>(this);

	PendingBlendOut            = 0.f;
	PendingStartAngleDeg       = 0.f;
	TransitionSerial           = 0;
	LastPushedTransitionSerial = 0;
	LastPushedSMState          = EAZ_StateMachineState::IdleLoop;
	LatchedReaction            = EAZ_ObstacleReaction::None;
	RatioSamplesRaw.Reset();
	RatioSamplesEff.Reset();

	if (RootMotionMode != ERootMotionMode::RootMotionFromMontagesOnly)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CmcAnim] %s has RootMotionMode=%d, expected RootMotionFromMontagesOnly(%d). ")
			TEXT("Locomotion root motion will be applied twice on CMC. Fix it in the ABP Class Defaults."),
			*GetClass()->GetName(), static_cast<int32>(RootMotionMode),
			static_cast<int32>(ERootMotionMode::RootMotionFromMontagesOnly));
	}

	if (!bLoggedInit)
	{
		bLoggedInit = true;
		const USkeletalMeshComponent* MeshComp = GetSkelMeshComponent();
		UE_LOG(LogTemp, Display,
			TEXT("[CmcAnim] %s init | pawn=%s | mesh=%s | collisions=%s"),
			*GetClass()->GetName(), *GetNameSafe(Cached_Character),
			*GetNameSafe(MeshComp ? MeshComp->GetSkeletalMeshAsset() : nullptr),
			bHandleTrajectoryCollisions ? TEXT("on") : TEXT("off"));
	}
}


void UAZ_CmcAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!StateMachine)
	{
		StateMachine = NewObject<UAZ_LocomotionStateMachine>(this);
	}

	if (!Cached_Character)
	{
		Cached_Character = Cast<AAZ_CmcCharacterBase>(TryGetPawnOwner());
		if (!Cached_Character)
		{
			return;
		}
	}

	Cached_Character->FillAnimContract(CharacterProperties);

	{
		const bool bMontageNow = (GetCurrentActiveMontage() != nullptr);
		bMontageJustReleased_GT = (bMontageActive_GT && !bMontageNow);
		if (bMontageJustReleased_GT)
		{
			UE_LOG(LogTemp, Display, TEXT("[CmcRmRelease] invalidate | spd=%.0f state=%d"),
				Speed2D, static_cast<int32>(MovementState));
		}
		bMontageActive_GT = bMontageNow;

		const AAZ_CmcHeroCharacter* RmHero = Cast<AAZ_CmcHeroCharacter>(Cached_Character);
		bRmOwnsStarts_GT = RmHero && RmHero->OwnsRootMotionStarts(CharacterProperties.SelectionGait);

		if (!Cached_ObstacleSensor.IsValid())
		{
			Cached_ObstacleSensor = Cached_Character->FindComponentByClass<UAZ_ObstacleSensorComponent>();
		}
		SensorReaction_GT = Cached_ObstacleSensor.IsValid()
			? Cached_ObstacleSensor->CurrentReaction
			: EAZ_ObstacleReaction::None;

		bCombatReady = CharacterProperties.OwnedTags.HasTag(FAZ_GameplayTags::Get().Combat_Ready);
		CombatReadyAlpha = FMath::FInterpTo(CombatReadyAlpha, bCombatReady ? 1.f : 0.f, DeltaSeconds,
			bCombatReady ? CombatReadyBlendInSpeed : CombatReadyBlendOutSpeed);
	}

	Update_GrabIK(DeltaSeconds);

	{
		static const FName StopsTag(TEXT("Stops"));
		bStopClipSelected_GT = CurrentDatabaseTags.Contains(StopsTag);

		StopClipSpeed_GT = 0.f;
		StopClipSampleTime_GT = 0.f;
		if (bStopClipSelected_GT)
		{
			if (const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(CurrentSelectedAnim.Get()))
			{
				StopClipSampleTime_GT = CurrentSelectedTime;
				StopClipSpeed_GT = Sequence->EvaluateCurveData(MoveDataSpeedCurve, CurrentSelectedTime);
			}
		}

		static const FName TurnInPlaceSnapshotTag(TEXT("TurnInPlace"));
		bTipClipSelected_GT = CurrentDatabaseTags.Contains(TurnInPlaceSnapshotTag);
		TipClipFraction_GT = 0.f;
		if (bTipClipSelected_GT)
		{
			if (const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(CurrentSelectedAnim.Get()))
			{
				const float Len = Sequence->GetPlayLength();
				TipClipFraction_GT = (Len > KINDA_SMALL_NUMBER) ? (CurrentSelectedTime / Len) : 0.f;
			}
		}
		TipRootYaw_GT = static_cast<float>(RootTransform.Rotator().Yaw);
	}

	if (bDebugAnim)
	{
		DrawDebugAnimOverlay();
		LogMovementFeelOncePerSecond(DeltaSeconds);
	}
}

void UAZ_CmcAnimInstance::LogMovementFeelOncePerSecond(float DeltaSeconds) const
{
	const UWorld* FeelWorld = GetWorld();
	if (!FeelWorld || !FeelWorld->IsGameWorld())
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(Cached_Character);
	const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Move)
	{
		return;
	}

	static float FeelLogAccumulator = 0.f;
	FeelLogAccumulator += DeltaSeconds;
	if (FeelLogAccumulator < 1.f)
	{
		return;
	}
	FeelLogAccumulator = 0.f;

	const float LiveSpeed2D = Move->Velocity.Size2D();
	const float TimeToSpeed = (Move->MaxAcceleration > 0.f) ? (LiveSpeed2D / Move->MaxAcceleration) : 0.f;

	UE_LOG(LogTemp, Display,
		TEXT("[CmcFeel] spd=%.0f | friction=%.2f maxAccel=%.0f braking=%.0f rotYaw=%.0f | ")
		TEXT("timeToSpeed=%.2fs dirTau=%.2fs | orientToMove=%d ctrlDesired=%d"),
		LiveSpeed2D, Move->GroundFriction, Move->MaxAcceleration, Move->BrakingDecelerationWalking,
		Move->RotationRate.Yaw, TimeToSpeed,
		(Move->GroundFriction > 0.f) ? (1.f / Move->GroundFriction) : 0.f,
		Move->bOrientRotationToMovement ? 1 : 0, Move->bUseControllerDesiredRotation ? 1 : 0);
}

void UAZ_CmcAnimInstance::DrawDebugAnimOverlay() const
{
	if (!GEngine)
	{
		return;
	}

	static constexpr int32 KeyBase = 0x415A00;

	const UEnum* DirEnum   = StaticEnum<EAZ_MovementDirection>();
	const UEnum* GaitEnum  = StaticEnum<EAZ_Gait>();
	const UEnum* StateEnum = StaticEnum<EAZ_MovementState>();
	auto EnumName = [](const UEnum* E, int64 V) -> FString
	{
		return E ? E->GetNameStringByValue(V) : TEXT("?");
	};

	const FString AnimName = GetNameSafe(CurrentSelectedAnim);
	const FString DbName   = GetNameSafe(CurrentSelectedDatabase);

	const float RootOffsetYaw = static_cast<float>(
		(CharacterTransform.Rotator() - RootTransform.Rotator()).GetNormalized().Yaw);

	FString GateNames;
	for (const FName& GateLabel : MatchedGateLabels)
	{
		if (!GateNames.IsEmpty()) { GateNames += TEXT(","); }
		GateNames += GateLabel.ToString();
	}

	const FVector2D Lean = Get_LeanAmount();

	GEngine->AddOnScreenDebugMessage(KeyBase + 0, 0.f, FColor::Yellow,
		FString::Printf(TEXT("ANIM  %s"), *AnimName));
	GEngine->AddOnScreenDebugMessage(KeyBase + 1, 0.f, FColor::Orange,
		FString::Printf(TEXT("DB    %s   cost %.1f   loop %d   tags %d"),
			*DbName, SearchCost, bCurrentAssetLooping ? 1 : 0, CurrentDatabaseTags.Num()));
	GEngine->AddOnScreenDebugMessage(KeyBase + 2, 0.f, FColor::Silver,
		FString::Printf(TEXT("gates [%s]"), *GateNames));
	GEngine->AddOnScreenDebugMessage(KeyBase + 3, 0.f, FColor::Green,
		FString::Printf(TEXT("dir   %s  ang %+.0f    gait %s   state %s"),
			*EnumName(DirEnum,  static_cast<int64>(MovementDirection)), MovementDirectionAngle,
			*EnumName(GaitEnum, static_cast<int64>(Gait)),
			*EnumName(StateEnum, static_cast<int64>(MovementState))));
	GEngine->AddOnScreenDebugMessage(KeyBase + 4, 0.f, FColor::Cyan,
		FString::Printf(TEXT("spd   %.0f / %.0f    trj fut %.0f   turnAng %+.0f"),
			Speed2D, CharacterProperties.CurrentMaxSpeed,
			Trj_FutureVelocity.Size2D(), Get_TrajectoryTurnAngle()));
	GEngine->AddOnScreenDebugMessage(KeyBase + 5, 0.f,
		FMath::Abs(RootOffsetYaw) > 60.f ? FColor::Red : FColor::White,
		FString::Printf(TEXT("yaw   capsule %+.0f   root %+.0f   ROOTOFF %+.0f"),
			CharacterTransform.Rotator().Yaw, RootTransform.Rotator().Yaw, RootOffsetYaw));
	GEngine->AddOnScreenDebugMessage(KeyBase + 6, 0.f, FColor::Magenta,
		FString::Printf(TEXT("lean  %+.2f    AOyaw %+.0f    pivot %d  tip %d  starting %d"),
			Lean.X, Get_AO_Yaw(), IsPivoting() ? 1 : 0,
			ShouldTurnInPlace() ? 1 : 0, IsStarting() ? 1 : 0));

	if (Speed2D > 40.f)
	{
		UE_LOG(LogTemp, Display,
			TEXT("[CmcLean] lean=%+.2f relAccY=%+.2f spd=%.0f rootOff=%+.0f capsuleYaw=%+.0f"),
			Lean.X, CalculateRelativeAccelerationAmount().Y, Speed2D, RootOffsetYaw,
			CharacterTransform.Rotator().Yaw);
	}
}

void UAZ_CmcAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	if (!Cached_Character)
	{
		return;
	}

	Update_Logic(DeltaSeconds);
}

void UAZ_CmcAnimInstance::SetOffsetRootTransform(const FTransform& InOffsetRootTransform)
{
	OffsetRootTransform = InOffsetRootTransform;
	bHasOffsetRootTransform = true;
}

FAnimNode_OffsetRootBone* UAZ_CmcAnimInstance::FindOffsetRootBoneNode()
{
	const IAnimClassInterface* AnimClass = IAnimClassInterface::GetFromClass(GetClass());
	if (!AnimClass)
	{
		return nullptr;
	}

	for (const FStructProperty* NodeProp : AnimClass->GetAnimNodeProperties())
	{
		if (NodeProp && NodeProp->Struct == FAnimNode_OffsetRootBone::StaticStruct())
		{
			return NodeProp->ContainerPtrToValuePtr<FAnimNode_OffsetRootBone>(this);
		}
	}
	return nullptr;
}


void UAZ_CmcAnimInstance::Update_Logic(float DeltaSeconds)
{
	Update_Trajectory(DeltaSeconds);
	Update_EssentialValues(DeltaSeconds);
	Update_States();

	Update_MovementDirection();

	Update_LocomotionStateMachine(DeltaSeconds);

	if (bDebugAnim)
	{
		if (const UWorld* DebugWorld = GetWorld())
		{
			if (DebugWorld->IsGameWorld())
			{
				SecondsSinceSelectionChange += DeltaSeconds;

				static constexpr float RatioMinSpeed = 20.f;

				float DepictedSpeed = 0.f;
				const UAnimSequenceBase* SelectedSeq = Cast<UAnimSequenceBase>(CurrentSelectedAnim.Get());
				if (SelectedSeq)
				{
					DepictedSpeed = SelectedSeq->EvaluateCurveData(MoveDataSpeedCurve, CurrentSelectedTime);
				}
				if (DepictedSpeed > DynamicPlayRateMinDepictedSpeed && Speed2D > RatioMinSpeed)
				{
					const float Raw  = Speed2D / DepictedSpeed;
					const float Rate = static_cast<float>(ComputeDynamicPlayRate(SelectedSeq, CurrentSelectedTime));
					RatioSamplesRaw.Add(Raw);
					RatioSamplesEff.Add(Raw / FMath::Max(Rate, 0.01f));
				}

				if (CurrentSelectedAnim.Get() != LastLoggedSelectedAnim.Get())
				{
					if (RatioSamplesRaw.Num() > 0)
					{
						RatioSamplesRaw.Sort();
						RatioSamplesEff.Sort();
						const int32 N      = RatioSamplesRaw.Num();
						const int32 IdxMed = N / 2;
						const int32 IdxP90 = FMath::Min(N - 1, FMath::FloorToInt(static_cast<float>(N) * 0.9f));
						const float EffMed = RatioSamplesEff[IdxMed];
						const TCHAR* Band =
							(EffMed >= 0.85f && EffMed <= 1.15f) ? TEXT("GREEN") :
							(EffMed >= 0.75f && EffMed <= 1.25f) ? TEXT("YELLOW") : TEXT("RED");
						UE_LOG(LogTemp, Display,
							TEXT("[CmcRatio] %-32s n=%3d raw med=%.2f p90=%.2f | eff med=%.2f p90=%.2f  %s"),
							*GetNameSafe(LastLoggedSelectedAnim.Get()), N,
							RatioSamplesRaw[IdxMed], RatioSamplesRaw[IdxP90],
							EffMed, RatioSamplesEff[IdxP90], Band);
					}
					RatioSamplesRaw.Reset();
					RatioSamplesEff.Reset();

					FString ChangeGates;
					for (const FName& GateLabel : MatchedGateLabels)
					{
						if (!ChangeGates.IsEmpty()) ChangeGates += TEXT(",");
						ChangeGates += GateLabel.ToString();
					}
					const UEnum* GaitEnum = StaticEnum<EAZ_Gait>();
					const FString CmdGait = GaitEnum
						? GaitEnum->GetNameStringByValue(static_cast<int64>(CharacterProperties.Gait))
						: FString::FromInt(static_cast<int32>(CharacterProperties.Gait));
					const FString SelGait = GaitEnum
						? GaitEnum->GetNameStringByValue(static_cast<int64>(CharacterProperties.SelectionGait))
						: FString::FromInt(static_cast<int32>(CharacterProperties.SelectionGait));

					const UEnum* SMEnum = StaticEnum<EAZ_StateMachineState>();
					const FString SMName = SMEnum
						? SMEnum->GetNameStringByValue(static_cast<int64>(ChooserContext.SMState))
						: FString::FromInt(static_cast<int32>(ChooserContext.SMState));

					UE_LOG(LogTemp, Display,
						TEXT("[CmcSel:%s|mtg=%d] #%d dt=%.0fms %s -> %s | SM=%s startDir=%d moveTrans=%d ")
						TEXT("| cost=%+.2f spd=%.0f turn=%.0f accel=%.2f ")
						TEXT("moving=%d pivot=%d tip=%d | cmd=%s sel=%s wantSprint=%d wantWalk=%d ")
						TEXT("| db=%s gates=[%s]"),
						*GetNameSafe(TryGetPawnOwner()), bMontageActive_GT ? 1 : 0,
						++SelectionChangeIndex,
						SecondsSinceSelectionChange * 1000.f,
						*GetNameSafe(LastLoggedSelectedAnim.Get()), *GetNameSafe(CurrentSelectedAnim),
						*SMName, static_cast<int32>(ChooserContext.StartDirection),
						ChooserContext.bMovingTransition ? 1 : 0,
						SearchCost, Speed2D, Get_TrajectoryTurnAngle(), AccelerationAmount,
						IsMoving(), IsPivoting(), ShouldTurnInPlace(),
						*CmdGait, *SelGait,
						CharacterProperties.InputState.bWantsToSprint ? 1 : 0,
						CharacterProperties.InputState.bWantsToWalk ? 1 : 0,
						*GetNameSafe(CurrentSelectedDatabase), *ChangeGates);

					LastLoggedSelectedAnim = CurrentSelectedAnim.Get();
					SecondsSinceSelectionChange = 0.f;
				}
			}
		}

		DebugAccumulator += DeltaSeconds;
		if (DebugAccumulator >= 1.f)
		{
			DebugAccumulator = 0.f;
			const UEnum* DirEnum = StaticEnum<EAZ_MovementDirection>();
			FString GateNames;
			for (const FName& GateLabel : MatchedGateLabels)
			{
				if (!GateNames.IsEmpty()) GateNames += TEXT(",");
				GateNames += GateLabel.ToString();
			}
			UE_LOG(LogTemp, Display,
				TEXT("[CmcAnim] SM=%d intentMove=%d | spd=%.0f/%.0f moving=%d pivot=%d tip=%d | accel=%.2f | dir=%s ang=%.0f Lfoot=%d ")
				TEXT("| trj past=%.0f cur=%.0f fut=%.0f turn=%.0f | land=%.2fs @ %.0f | samples=%d ")
				TEXT("| gates=[%s] | yaw cap=%.0f root=%.0f ROOTOFF=%.0f ")
				TEXT("| MM db=%s cost=%.1f loop=%d tags=%d anim=%s"),
				static_cast<int32>(ChooserContext.SMState), ChooserContext.bIsMoving ? 1 : 0,
				Speed2D, CharacterProperties.CurrentMaxSpeed, IsMoving(), IsPivoting(), ShouldTurnInPlace(),
				AccelerationAmount,
				DirEnum ? *DirEnum->GetNameStringByValue(static_cast<int64>(MovementDirection)) : TEXT("?"),
				MovementDirectionAngle, bLeftFootDown,
				Trj_PastVelocity.Size2D(), Trj_CurrentVelocity.Size2D(), Trj_FutureVelocity.Size2D(),
				Get_TrajectoryTurnAngle(),
				TrajectoryCollision.TimeToLand, TrajectoryCollision.LandSpeed, Trajectory.Samples.Num(),
				*GateNames,
				CharacterTransform.Rotator().Yaw, RootTransform.Rotator().Yaw,
				FRotator::NormalizeAxis(RootTransform.Rotator().Yaw - CharacterTransform.Rotator().Yaw),
				*GetNameSafe(CurrentSelectedDatabase), SearchCost, bCurrentAssetLooping,
				CurrentDatabaseTags.Num(), *GetNameSafe(CurrentSelectedAnim));
		}
	}
}


void UAZ_CmcAnimInstance::Update_Trajectory(float DeltaSeconds)
{
	const FPoseSearchTrajectoryData& TrajectoryData =
		(Speed2D > 0.f) ? TrajectoryGenerationData_Moving : TrajectoryGenerationData_Idle;

	FTransformTrajectory Generated;
	UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectory(
		this, TrajectoryData, DeltaSeconds,
		Trajectory, PreviousDesiredControllerYaw, Generated,
		TrajectoryHistorySamplingInterval, TrajectoryHistoryCount,
		TrajectoryPredictionSamplingInterval, TrajectoryPredictionCount);

	if (bHandleTrajectoryCollisions)
	{
		const TArray<AActor*> ActorsToIgnore;
		FTransformTrajectory Collided;
		UPoseSearchTrajectoryLibrary::HandleTransformTrajectoryWorldCollisions(
			this, this, Generated,
			bTrajectoryApplyGravity, FloorCollisionsOffset,
			Collided, TrajectoryCollision,
			TraceTypeQuery1,  false, ActorsToIgnore,
			EDrawDebugTrace::None,  true, MaxObstacleHeight);
		Trajectory = MoveTemp(Collided);
	}
	else
	{
		Trajectory = MoveTemp(Generated);
	}

	if (CharacterProperties.bTurnInPlaceActive)
	{
		const float DeltaYaw = static_cast<float>(FRotator::NormalizeAxis(
			CharacterProperties.TurnInPlaceTargetYaw
			- CharacterProperties.ActorTransform.Rotator().Yaw));
		const float ConvergeTime = FMath::Max(TurnInPlaceFacingConvergeTime, KINDA_SMALL_NUMBER);
		for (FTransformTrajectorySample& Sample : Trajectory.Samples)
		{
			if (Sample.TimeInSeconds > 0.f)
			{
				const float Alpha = FMath::Clamp(Sample.TimeInSeconds / ConvergeTime, 0.f, 1.f);
				const FQuat Delta(FVector::UpVector, FMath::DegreesToRadians(DeltaYaw * Alpha));
				Sample.Facing = Delta * Sample.Facing;
			}
		}
	}

	UPoseSearchTrajectoryLibrary::GetTransformTrajectoryVelocity(
		Trajectory, static_cast<float>(PastVelocityWindow.X), static_cast<float>(PastVelocityWindow.Y),
		Trj_PastVelocity,  false);
	UPoseSearchTrajectoryLibrary::GetTransformTrajectoryVelocity(
		Trajectory, static_cast<float>(CurrentVelocityWindow.X), static_cast<float>(CurrentVelocityWindow.Y),
		Trj_CurrentVelocity, false);
	UPoseSearchTrajectoryLibrary::GetTransformTrajectoryVelocity(
		Trajectory, static_cast<float>(FutureVelocityWindow.X), static_cast<float>(FutureVelocityWindow.Y),
		Trj_FutureVelocity, false);

	{
		const FVector PNow = Trajectory.GetSampleAtTime(0.f).Position;
		const FVector PFut = Trajectory.GetSampleAtTime(TrajectoryFutureLookahead).Position;
		PredictedFutureVelocity = (PFut - PNow) / FMath::Max(0.01f, TrajectoryFutureLookahead);
	}
}


void UAZ_CmcAnimInstance::Update_EssentialValues(float DeltaSeconds)
{
	CharacterTransform_LastFrame = CharacterTransform;
	CharacterTransform = CharacterProperties.ActorTransform;

	FAnimNode_OffsetRootBone* OffsetRootNode = bOffsetRootBoneEnabled ? FindOffsetRootBoneNode() : nullptr;
	if (OffsetRootNode)
	{
		FTransform OffsetRoot;
		OffsetRootNode->GetOffsetRootTransform(OffsetRoot);

		const FRotator OffsetRotation = OffsetRoot.Rotator();
		RootTransform = FTransform(
			FRotator(OffsetRotation.Pitch, OffsetRotation.Yaw + 90.f, OffsetRotation.Roll),
			OffsetRoot.GetTranslation(),
			FVector::OneVector);
	}
	else
	{
		RootTransform = CharacterTransform;
	}

	Acceleration_LastFrame = Acceleration;
	Acceleration = CharacterProperties.InputAcceleration;
	AccelerationAmount = (CharacterProperties.CurrentMaxAcceleration > UE_KINDA_SMALL_NUMBER)
		? Acceleration.Size() / CharacterProperties.CurrentMaxAcceleration
		: 0.f;
	bHasAcceleration = AccelerationAmount > 0.f;

	Velocity_LastFrame = Velocity;
	Velocity = CharacterProperties.Velocity;
	Speed2D = Velocity.Size2D();
	bHasVelocity = Speed2D > HasVelocityThreshold;


	const FVector RawVelocityAcceleration =
		(Velocity - Velocity_LastFrame) / FMath::Max(DeltaSeconds, 0.001f);
	VelocityAcceleration =
		FMath::VInterpTo(VelocityAcceleration, RawVelocityAcceleration, DeltaSeconds, LeanInterpSpeed);
	RelativeAcceleration = RootTransform.GetRotation().UnrotateVector(VelocityAcceleration);

	if (bHasVelocity)
	{
		LastNonZeroVelocity = Velocity;
	}
}


void UAZ_CmcAnimInstance::Update_States()
{
	MovementMode_LastFrame = MovementMode;
	MovementMode = CharacterProperties.MovementMode;

	RotationMode_LastFrame = RotationMode;
	RotationMode = CharacterProperties.RotationMode;

	MovementState_LastFrame = MovementState;
	MovementState = IsMoving() ? EAZ_MovementState::Moving : EAZ_MovementState::Idle;

	Gait_LastFrame = Gait;

	Gait = CharacterProperties.SelectionGait;

	Stance_LastFrame = Stance;
	Stance = CharacterProperties.Stance;

	bStopActive_LastFrame = bStopActive;
	bStopActive = CharacterProperties.bStopActive;

	bTurnInPlaceActive_LastFrame = bTurnInPlaceActive;
	bTurnInPlaceActive = CharacterProperties.bTurnInPlaceActive;

}


void UAZ_CmcAnimInstance::Update_MovementDirection()
{
	const float ContactL = GetCurveValue(FootSpeedCurveL);
	const float ContactR = GetCurveValue(FootSpeedCurveR);
	if (ContactL > FootPlantedSpeedThreshold || ContactR > FootPlantedSpeedThreshold)
	{
		bLeftFootDown = (ContactL >= ContactR);
	}

	if (Speed2D > DirectionHoldSpeed)
	{
		MovementDirectionAngle = AZ::CmcAnim::SignedYawTo(Velocity, static_cast<float>(RootTransform.Rotator().Yaw));
	}

	const bool bLeftFootLeads = bInvertFootPhase ? bLeftFootDown : !bLeftFootDown;
	const float Angle = MovementDirectionAngle;

	const FAZ_MovementDirectionThresholds Thresholds = Get_MovementDirectionThresholds();

	if (Angle >= -Thresholds.FL && Angle <= Thresholds.FR)
	{
		MovementDirection = EAZ_MovementDirection::F;
	}
	else if (Angle > Thresholds.BR || Angle < -Thresholds.BL)
	{
		MovementDirection = EAZ_MovementDirection::B;
	}
	else if (Angle > 0.f)
	{
		MovementDirection = bLeftFootLeads ? EAZ_MovementDirection::RL : EAZ_MovementDirection::RR;
	}
	else
	{
		MovementDirection = bLeftFootLeads ? EAZ_MovementDirection::LL : EAZ_MovementDirection::LR;
	}
}


bool UAZ_CmcAnimInstance::IsMoving() const
{
	return !Velocity.Equals(FVector::ZeroVector, IsMovingVelocityTolerance);
}

bool UAZ_CmcAnimInstance::IsPivoting() const
{
	float Threshold = PivotAngleThreshold_OrientToMovement;
	switch (RotationMode)
	{
	case EAZ_RotationMode::Strafe:  Threshold = PivotAngleThreshold_Strafe;  break;
	case EAZ_RotationMode::Aiming:  Threshold = PivotAngleThreshold_Aiming;  break;
	default: break;
	}

	return (FMath::Abs(Get_TrajectoryTurnAngle()) >= Threshold) && IsMoving();
}

bool UAZ_CmcAnimInstance::ShouldTurnInPlace() const
{
	if (CharacterProperties.bTurnInPlaceActive)
	{
		return true;
	}

	const float YawDelta = static_cast<float>(FMath::Abs(
		(CharacterProperties.OrientationIntent - RootTransform.Rotator()).GetNormalized().Yaw));

	const bool bJustStopped = (MovementState == EAZ_MovementState::Idle)
		&& (MovementState_LastFrame == EAZ_MovementState::Moving);

	return (YawDelta >= TurnInPlaceAngleThreshold)
		&& (CharacterProperties.InputState.bWantsToAim || bJustStopped);
}

float UAZ_CmcAnimInstance::Get_TrajectoryTurnAngle() const
{
	return static_cast<float>((Acceleration.Rotation() - Velocity.Rotation()).GetNormalized().Yaw);
}

FVector UAZ_CmcAnimInstance::CalculateRelativeAccelerationAmount() const
{
	const float MaxAcceleration = CharacterProperties.CurrentMaxAcceleration;
	const float MaxDeceleration = CharacterProperties.CurrentMaxDeceleration;
	if (MaxAcceleration <= 0.f || MaxDeceleration <= 0.f)
	{
		return FVector::ZeroVector;
	}

	const FVector VelDir = Velocity.GetSafeNormal2D();
	if (VelDir.IsNearlyZero())
	{
		const float Budget = FMath::Max(MaxAcceleration, 1.f);
		return CharacterTransform.GetRotation().UnrotateVector(
			VelocityAcceleration.GetClampedToMaxSize(Budget) / Budget);
	}


	const float   LongMag  = static_cast<float>(FVector::DotProduct(VelocityAcceleration, VelDir));
	const FVector LatVec   = FVector(VelocityAcceleration.X, VelocityAcceleration.Y, 0.f) - VelDir * LongMag;

	const float LongBudget = FMath::Max(LongMag >= 0.f ? MaxAcceleration : MaxDeceleration, 1.f);
	const float TurnBudget = FMath::Max(
		Speed2D * FMath::DegreesToRadians(LeanTurnRateReference), 1.f);

	const FVector LongPart = VelDir * FMath::Clamp(LongMag / LongBudget, -1.f, 1.f);
	const FVector LatPart  = LatVec.GetSafeNormal2D()
		* FMath::Clamp(static_cast<float>(LatVec.Size2D()) / TurnBudget, 0.f, 1.f);

	const FVector Combined = (LongPart + LatPart).GetClampedToMaxSize(1.f);

	return CharacterTransform.GetRotation().UnrotateVector(Combined);
}

FVector2D UAZ_CmcAnimInstance::Get_LeanAmount() const
{
	const float RangeSpan = FMath::Max(static_cast<float>(LeanSpeedRangeIn.Y - LeanSpeedRangeIn.X), UE_KINDA_SMALL_NUMBER);
	const float Alpha = FMath::Clamp((Speed2D - static_cast<float>(LeanSpeedRangeIn.X)) / RangeSpan, 0.f, 1.f);
	const float SpeedScale = FMath::Lerp(static_cast<float>(LeanSpeedRangeOut.X),
	                                     static_cast<float>(LeanSpeedRangeOut.Y), Alpha);

	return FVector2D(CalculateRelativeAccelerationAmount().Y * SpeedScale, 0.f);
}

FVector2D UAZ_CmcAnimInstance::Get_AOValue() const
{
	const FRotator Delta =
		(CharacterProperties.AimingRotation - RootTransform.Rotator()).GetNormalized();

	const float DisableAlpha = FMath::Clamp(GetCurveValue(DisableAOCurve), 0.f, 1.f);
	return FMath::Lerp(FVector2D(Delta.Yaw, Delta.Pitch), FVector2D::ZeroVector, DisableAlpha);
}


TArray<UPoseSearchDatabase*> UAZ_CmcAnimInstance::Get_DatabasesToSearch() const
{
	MatchedGateLabels.Reset();

	const EAZ_MovementState GateState =
		bMontageActive_GT ? EAZ_MovementState::Moving : MovementState;

	TArray<UPoseSearchDatabase*> Result;
	const FAZ_DatabaseGate* ExclusiveRow = nullptr;
	for (const FAZ_DatabaseGate& Gate : DatabaseGates)
	{
		if (!Gate.Matches(MovementMode, Stance, GateState, Gait,
			ChooserContext.SMState, ChooserContext.StartDirection, RotationMode))
		{
			continue;
		}

		if (Gate.bExclusive)
		{
			ExclusiveRow = &Gate;
			break;
		}

		MatchedGateLabels.Add(Gate.Label);
		for (const TObjectPtr<UPoseSearchDatabase>& Database : Gate.Databases)
		{
			if (Database)
			{
				Result.Add(Database);
			}
		}
	}

	if (ExclusiveRow)
	{
		Result.Reset();
		MatchedGateLabels.Reset();
		MatchedGateLabels.Add(ExclusiveRow->Label);
		for (const TObjectPtr<UPoseSearchDatabase>& Database : ExclusiveRow->Databases)
		{
			if (Database)
			{
				Result.Add(Database);
			}
		}
	}


	const EAZ_StateMachineState SMPhase = ChooserContext.SMState;

	const bool bLocoTransition  = (SMPhase == EAZ_StateMachineState::TransitionToLocomotion)
		&& !ChooserContext.bJustLanded;
	const bool bReversalBucket =
		ChooserContext.StartDirection == EAZ_StartDirection::L180 ||
		ChooserContext.StartDirection == EAZ_StartDirection::R180;
	const bool bPivotTransition = bLocoTransition && ChooserContext.bMovingTransition && bReversalBucket;
	const bool bStartTransition = bLocoTransition && !bPivotTransition;

	static const FName StartsTag(TEXT("Starts"));
	if (!bStartTransition && !CurrentDatabaseTags.Contains(StartsTag))
	{
		Result.RemoveAll([](const UPoseSearchDatabase* Db)
		{
			return Db && Db->Tags.Contains(StartsTag);
		});
	}

	if (!bPivotTransition && !CurrentDatabaseTags.Contains(PivotDatabaseTag))
	{
		Result.RemoveAll([this](const UPoseSearchDatabase* Db)
		{
			return Db && Db->Tags.Contains(PivotDatabaseTag);
		});
	}

	static const FName StopsTag(TEXT("Stops"));
	const bool bStopTransition = (SMPhase == EAZ_StateMachineState::TransitionToIdle)
		&& !ChooserContext.bJustLanded;
	const bool bSubFloorStop = CharacterProperties.bStopActive && !CharacterProperties.bStopIsAnimated;

	if ((!bStopTransition || bSubFloorStop) && !CurrentDatabaseTags.Contains(StopsTag))
	{
		Result.RemoveAll([](const UPoseSearchDatabase* Db)
		{
			return Db && Db->Tags.Contains(StopsTag);
		});
	}

	if (bPivotTransition)
	{
		UE_LOG(LogTemp, Display,
			TEXT("[CmcPivot] window OPEN | SM=%d startDir=%d turn=%.0f spd=%.0f poolSize=%d pivotsInPool=%d"),
			static_cast<int32>(SMPhase), static_cast<int32>(ChooserContext.StartDirection),
			Get_TrajectoryTurnAngle(), Speed2D, Result.Num(),
			Result.ContainsByPredicate([this](const UPoseSearchDatabase* Db)
				{ return Db && Db->Tags.Contains(PivotDatabaseTag); }) ? 1 : 0);
	}

	{
		static const FName RmStartsTag(TEXT("Starts"));
		static const FName RmStopsTag(TEXT("Stops"));

		if (bRmOwnsStarts_GT || bMontageActive_GT)
		{
			Result.RemoveAll([](const UPoseSearchDatabase* Db)
			{
				return Db && Db->Tags.Contains(RmStartsTag);
			});
		}

		if (bRmOwnsStarts_GT || bMontageActive_GT)
		{
			Result.RemoveAll([this](const UPoseSearchDatabase* Db)
			{
				return Db && Db->Tags.Contains(PivotDatabaseTag);
			});
		}

		if (bMontageActive_GT)
		{
			Result.RemoveAll([](const UPoseSearchDatabase* Db)
			{
				return Db && Db->Tags.Contains(RmStopsTag);
			});
		}
	}

	static const FName StanceTransTag(TEXT("StanceTrans"));
	if ((SMPhase != EAZ_StateMachineState::TransitionStance) && !CurrentDatabaseTags.Contains(StanceTransTag))
	{
		Result.RemoveAll([](const UPoseSearchDatabase* Db)
		{
			return Db && Db->Tags.Contains(StanceTransTag);
		});
	}

	static const FName TurnInPlaceTag(TEXT("TurnInPlace"));
	if (!ShouldTurnInPlace() && !CurrentDatabaseTags.Contains(TurnInPlaceTag))
	{
		Result.RemoveAll([](const UPoseSearchDatabase* Db)
		{
			return Db && Db->Tags.Contains(TurnInPlaceTag);
		});
	}

	return Result;
}

namespace AZ::CmcAnim
{
	static const FName StopsTagName(TEXT("Stops"));

	static constexpr float StoppedSpeedTolerance = 2.f;

	static constexpr float StopsMinPlayRate = 0.2f;

	static const FName IdlesTagName(TEXT("Idles"));


}

void UAZ_CmcAnimInstance::KeepPlayingOneShotSearchable(
	const FMotionMatchingAnimNodeReference& MotionMatchingNode, TArray<UPoseSearchDatabase*>& Pool) const
{
	if (bMontageActive_GT)
	{
		return;
	}

	FPoseSearchBlueprintResult Current;
	bool bIsResultValid = false;
	UMotionMatchingAnimNodeLibrary::GetMotionMatchingSearchResult(MotionMatchingNode, Current, bIsResultValid);
	if (!bIsResultValid || !Current.SelectedDatabase || Current.bLoop)
	{
		return;
	}

	const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(Current.SelectedAnim);
	if (!Sequence)
	{
		return;
	}

	const float PlayLength = Sequence->GetPlayLength();
	if (PlayLength <= 0.f)
	{
		return;
	}

	const bool bIsStop = CurrentDatabaseTags.Contains(AZ::CmcAnim::StopsTagName);

	if (!bIsStop && CharacterProperties.bStopActive)
	{
		return;
	}

	const float KeepAliveFraction =
		bIsStop ? StopKeepAliveFractionTunable : OneShotKeepAliveFractionTunable;

	if (Current.SelectedTime >= KeepAliveFraction * PlayLength)
	{
		return;
	}

	Pool.AddUnique(const_cast<UPoseSearchDatabase*>(Current.SelectedDatabase.Get()));

	if (bIsStop && Speed2D > AZ::CmcAnim::StoppedSpeedTolerance)
	{
		const bool bHasNonIdleAlternative = Pool.ContainsByPredicate([](const UPoseSearchDatabase* Db)
		{
			return Db && !Db->Tags.Contains(AZ::CmcAnim::IdlesTagName);
		});

		if (bHasNonIdleAlternative)
		{
			Pool.RemoveAll([](const UPoseSearchDatabase* Db)
			{
				return Db && Db->Tags.Contains(AZ::CmcAnim::IdlesTagName);
			});
		}
	}
}

void UAZ_CmcAnimInstance::Update_MotionMatching(const FAnimNodeReference& Node)
{
	EAnimNodeReferenceConversionResult Conversion = EAnimNodeReferenceConversionResult::Failed;
	const FMotionMatchingAnimNodeReference MotionMatchingNode =
		UMotionMatchingAnimNodeLibrary::ConvertToMotionMatchingNode(Node, Conversion);
	if (Conversion != EAnimNodeReferenceConversionResult::Succeeded)
	{
		return;
	}

	TArray<UPoseSearchDatabase*> Pool = Get_DatabasesToSearch();
	KeepPlayingOneShotSearchable(MotionMatchingNode, Pool);
	if (Pool.IsEmpty())
	{
		for (const FAZ_DatabaseGate& Gate : DatabaseGates)
		{
			if (!Gate.Matches(EAZ_MovementMode::OnGround, Stance, MovementState, Gait))
			{
				continue;
			}
			for (const TObjectPtr<UPoseSearchDatabase>& Database : Gate.Databases)
			{
				if (Database)
				{
					Pool.AddUnique(Database);
				}
			}
		}

		if (!bWarnedEmptyGateUnion)
		{
			bWarnedEmptyGateUnion = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[CmcAnim] DatabaseGates union is EMPTY for mode=%d stance=%d state=%d gait=%d — ")
				TEXT("fell back to the GROUNDED rows (%d db). Add a matching gate row."),
				static_cast<int32>(MovementMode), static_cast<int32>(Stance),
				static_cast<int32>(MovementState), static_cast<int32>(Gait), Pool.Num());
		}

		if (Pool.IsEmpty())
		{
			return;
		}
	}
	else
	{
		bWarnedEmptyGateUnion = false;
	}

	UMotionMatchingAnimNodeLibrary::SetDatabasesToSearch(
		MotionMatchingNode, Pool, Get_MMInterruptMode());
}

void UAZ_CmcAnimInstance::Update_MotionMatching_PostSelection(const FAnimNodeReference& Node)
{
	EAnimNodeReferenceConversionResult Conversion = EAnimNodeReferenceConversionResult::Failed;
	const FMotionMatchingAnimNodeReference MotionMatchingNode =
		UMotionMatchingAnimNodeLibrary::ConvertToMotionMatchingNode(Node, Conversion);
	if (Conversion != EAnimNodeReferenceConversionResult::Succeeded)
	{
		return;
	}

	FPoseSearchBlueprintResult Result;
	bool bIsResultValid = false;
	UMotionMatchingAnimNodeLibrary::GetMotionMatchingSearchResult(MotionMatchingNode, Result, bIsResultValid);

	if (bIsResultValid)
	{
		PublishSelection(Result.SelectedAnim, Result.SelectedDatabase, Result.SelectedTime,
			Result.bLoop, Result.SearchCost, TArray<FName>());
	}

}

bool UAZ_CmcAnimInstance::EnableSteering(const FAnimNodeReference& Node) const
{
	const bool bGoingSomewhere = (MovementState == EAZ_MovementState::Moving)
		|| (MovementMode == EAZ_MovementMode::InAir);

	return bGoingSomewhere && UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimIsActive(Node);
}


bool UAZ_CmcAnimInstance::IsStarting() const
{
	return IsMoving()
		&& (Trj_FutureVelocity.Size2D() >= Velocity.Size2D() + StartingFutureSpeedMargin)
		&& !CurrentDatabaseTags.Contains(PivotDatabaseTag);
}

bool UAZ_CmcAnimInstance::ShouldSpinTransition() const
{
	const float OffsetYaw = FMath::Abs(static_cast<float>(
		(CharacterTransform.Rotator() - RootTransform.Rotator()).GetNormalized().Yaw));

	return (OffsetYaw >= SpinTransitionAngleThreshold)
		&& (Speed2D >= SpinTransitionMinSpeed)
		&& !CurrentDatabaseTags.Contains(PivotDatabaseTag);
}

FQuat UAZ_CmcAnimInstance::Get_DesiredFacing() const
{
	FTransformTrajectorySample Sample;
	UPoseSearchTrajectoryLibrary::GetTransformTrajectorySampleAtTime(
		Trajectory, DesiredFacingSampleTime, Sample,  false);
	return Sample.Facing;
}

float UAZ_CmcAnimInstance::Get_AO_Yaw() const
{
	return (RotationMode == EAZ_RotationMode::Strafe) ? static_cast<float>(Get_AOValue().X) : 0.f;
}

FAZ_MovementDirectionThresholds UAZ_CmcAnimInstance::Get_MovementDirectionThresholds() const
{
	if (MovementDirection == EAZ_MovementDirection::F || MovementDirection == EAZ_MovementDirection::B)
	{
		return DirectionThresholds_Cardinal;
	}

	if (IsPivoting())
	{
		return DirectionThresholds_Cardinal;
	}

	if (bCurrentAssetLooping && !CharacterProperties.InputState.bWantsToAim)
	{
		return DirectionThresholds_SideLoop;
	}

	return DirectionThresholds_SideTight;
}


float UAZ_CmcAnimInstance::Get_MMBlendTime() const
{
	if (MovementMode == EAZ_MovementMode::InAir)
	{
		return (Velocity.Z > MMRisingVelocityZ) ? MMBlendTime_Rising : MMBlendTime_Falling;
	}

	return (MovementMode_LastFrame == EAZ_MovementMode::InAir) ? MMBlendTime_JustLanded : MMBlendTime_Ground;
}

float UAZ_CmcAnimInstance::Get_MMNotifyRecencyTimeOut() const
{
	switch (Gait)
	{
	case EAZ_Gait::Sprint: return MMNotifyRecency_Sprint;
	case EAZ_Gait::Run:    return MMNotifyRecency_Run;
	default:               return MMNotifyRecency_Walk;
	}
}

EPoseSearchInterruptMode UAZ_CmcAnimInstance::Get_MMInterruptMode() const
{
	const bool bModeChanged = (MovementMode != MovementMode_LastFrame);

	const bool bGroundedStateChanged =
		((MovementState != MovementState_LastFrame)
			|| ((Gait != Gait_LastFrame) && (MovementState == EAZ_MovementState::Moving))
			|| (Stance != Stance_LastFrame))
		&& (MovementMode == EAZ_MovementMode::OnGround);

	if (bMontageJustReleased_GT)
	{
		bMontageJustReleased_GT = false;
		return EPoseSearchInterruptMode::InterruptOnDatabaseChangeAndInvalidateContinuingPose;
	}

	if (bStopActive && !bStopActive_LastFrame)
	{
		return EPoseSearchInterruptMode::InterruptOnDatabaseChangeAndInvalidateContinuingPose;
	}

	if (bTurnInPlaceActive && !bTurnInPlaceActive_LastFrame)
	{
		return EPoseSearchInterruptMode::InterruptOnDatabaseChangeAndInvalidateContinuingPose;
	}

	return (bModeChanged || bGroundedStateChanged)
		? EPoseSearchInterruptMode::InterruptOnDatabaseChange
		: EPoseSearchInterruptMode::DoNotInterrupt;
}

EOffsetRootBoneMode UAZ_CmcAnimInstance::Get_OffsetRootRotationMode() const
{
	return IsSlotActive(MontageSlotName) ? EOffsetRootBoneMode::Release : EOffsetRootBoneMode::Accumulate;
}

EOffsetRootBoneMode UAZ_CmcAnimInstance::Get_OffsetRootTranslationMode() const
{
	if (IsSlotActive(MontageSlotName))
	{
		return EOffsetRootBoneMode::Release;
	}
	if (MovementMode != EAZ_MovementMode::OnGround)
	{
		return EOffsetRootBoneMode::Release;
	}
	return IsMoving() ? EOffsetRootBoneMode::Interpolate : EOffsetRootBoneMode::Release;
}

float UAZ_CmcAnimInstance::Get_OffsetRootTranslationHalfLife() const
{
	return (MovementState == EAZ_MovementState::Idle) ? OffsetRootHalfLife_Idle : OffsetRootHalfLife_Moving;
}

EOrientationWarpingSpace UAZ_CmcAnimInstance::Get_OrientationWarpingWarpingSpace() const
{
	return bOffsetRootBoneEnabled
		? EOrientationWarpingSpace::RootBoneTransform
		: EOrientationWarpingSpace::ComponentTransform;
}

bool UAZ_CmcAnimInstance::AllowFootPinning() const
{
	return (MovementMode == EAZ_MovementMode::OnGround) && IsMoving();
}

double UAZ_CmcAnimInstance::ComputeDynamicPlayRate(const UAnimSequenceBase* Anim, float AnimTime) const
{
	if (!Anim)
	{
		return 1.0;
	}

	float AlphaCurve = 0.f;
	if (!UAnimationWarpingLibrary::GetCurveValueFromAnimation(Anim, PlayRateWarpingCurve, AnimTime, AlphaCurve))
	{
		return 1.0;
	}

	float SpeedCurve = 0.f;
	UAnimationWarpingLibrary::GetCurveValueFromAnimation(Anim, MoveDataSpeedCurve, AnimTime, SpeedCurve);
	if (SpeedCurve <= DynamicPlayRateMinDepictedSpeed)
	{
		return 1.0;
	}

	const bool bStopSelected = Anim == CurrentSelectedAnim.Get()
		&& CurrentDatabaseTags.Contains(AZ::CmcAnim::StopsTagName);
	if (bStopSelected)
	{
		if (Speed2D <= AZ::CmcAnim::StoppedSpeedTolerance)
		{
			return 1.0;
		}
		if (CharacterProperties.bStopActive && CharacterProperties.bStopIsAnimated)
		{
			return 1.0;
		}
	}

	const double MinRate = bStopSelected
		? static_cast<double>(AZ::CmcAnim::StopsMinPlayRate)
		: static_cast<double>(DynamicPlayRateMin);
	const double Ratio = FMath::Clamp(
		static_cast<double>(Speed2D) / static_cast<double>(SpeedCurve),
		MinRate, static_cast<double>(DynamicPlayRateMax));
	return FMath::Lerp(1.0, Ratio, static_cast<double>(FMath::Clamp(AlphaCurve, 0.f, 1.f)));
}

double UAZ_CmcAnimInstance::Get_DynamicPlayRate(FAnimNodeReference BlendStackInput) const
{
	UAnimationAsset* Asset = UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimAsset(BlendStackInput);
	const float AnimTime = UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimAssetTime(BlendStackInput);
	return ComputeDynamicPlayRate(Cast<UAnimSequenceBase>(Asset), AnimTime);
}

void UAZ_CmcAnimInstance::PublishSelection(UObject* InAnim, const UPoseSearchDatabase* InDatabase,
	float InTime, bool bInLoop, float InCost, const TArray<FName>& InFallbackTags)
{
	CurrentSelectedAnim     = InAnim;
	CurrentSelectedDatabase = InDatabase;
	CurrentSelectedTime     = InTime;
	bCurrentAssetLooping    = bInLoop;
	SearchCost              = InCost;

	CurrentDatabaseTags.Reset();
	if (InDatabase)
	{
		UPoseSearchLibrary::GetDatabaseTags(InDatabase, CurrentDatabaseTags);
	}
	else
	{
		CurrentDatabaseTags = InFallbackTags;
	}
}

void UAZ_CmcAnimInstance::Update_GrabIK(float DeltaSeconds)
{
	float TargetAlpha = 0.f;

	if (Cached_Character && !Cached_Character->IsGrabIKReleased())
	{
		if (const AActor* Grabber = Cached_Character->GetGrabFacingTarget())
		{
			const UAbilitySystemComponent* ASC = Cached_Character->GetAbilitySystemComponent();
			if (ASC && ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().State_Grabbed))
			{
				if (const USkeletalMeshComponent* GrabberMesh = Grabber->FindComponentByClass<USkeletalMeshComponent>())
				{
					TargetAlpha = 1.f;

					const FVector NewR = UAZ_MoverAnimInstance::ResolveGrabIKTarget(
						GetSkelMeshComponent(), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
						GrabberMesh, GrabberMesh->GetSocketLocation(GrabIKGrabberBoneForHandR), GrabIKReachScale);
					const FVector NewL = UAZ_MoverAnimInstance::ResolveGrabIKTarget(
						GetSkelMeshComponent(), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
						GrabberMesh, GrabberMesh->GetSocketLocation(GrabIKGrabberBoneForHandL), GrabIKReachScale);

					const bool bSnap = GrabIKAlpha < 0.05f;
					GrabIKTarget_HandR = bSnap ? NewR
						: FMath::VInterpTo(GrabIKTarget_HandR, NewR, DeltaSeconds, GrabIKTargetInterpSpeed);
					GrabIKTarget_HandL = bSnap ? NewL
						: FMath::VInterpTo(GrabIKTarget_HandL, NewL, DeltaSeconds, GrabIKTargetInterpSpeed);
				}
			}
		}
	}

	GrabIKAlpha = FMath::FInterpTo(GrabIKAlpha, TargetAlpha, DeltaSeconds, GrabIKBlendSpeed);

	if (GrabIKAlpha > KINDA_SMALL_NUMBER)
	{
		const UWorld* ShakeWorld = GetWorld();
		const float Now = ShakeWorld ? static_cast<float>(ShakeWorld->GetTimeSeconds()) : 0.f;

		const float TBody = Now * GrabBodyShakeFrequency;
		GrabBodyShakeRot = FRotator(
			FMath::PerlinNoise1D(TBody) * GrabBodyShakeAmplitudeDeg,
			FMath::PerlinNoise1D(TBody + 49.3f) * GrabBodyShakeAmplitudeDeg * 0.6f,
			FMath::PerlinNoise1D(TBody + 151.7f) * GrabBodyShakeAmplitudeDeg * 0.6f) * GrabIKAlpha;

		const float THead = Now * GrabHeadShakeFrequency;
		GrabHeadShakeRot = FRotator(
			FMath::PerlinNoise1D(THead + 293.1f) * GrabHeadShakeAmplitudeDeg,
			FMath::PerlinNoise1D(THead + 397.7f) * GrabHeadShakeAmplitudeDeg * 0.7f,
			FMath::PerlinNoise1D(THead + 509.3f) * GrabHeadShakeAmplitudeDeg * 0.5f) * GrabIKAlpha;
	}
	else
	{
		GrabBodyShakeRot = FRotator::ZeroRotator;
		GrabHeadShakeRot = FRotator::ZeroRotator;
	}
}

void UAZ_CmcAnimInstance::Update_LocomotionStateMachine(float DeltaSeconds)
{
	const EAZ_StateMachineState PreviousSMState = ChooserContext.SMState;

	ChooserContext.Speed2D           = Speed2D;
	ChooserContext.Gait              = Gait;
	ChooserContext.Stance            = Stance;
	ChooserContext.MovementMode      = MovementMode;
	ChooserContext.MovementDirection = MovementDirection;
	ChooserContext.bLeftFootDown     = bLeftFootDown;
	ChooserContext.OwnedTags         = CharacterProperties.OwnedTags;
	ChooserContext.AimingRotation    = CharacterProperties.AimingRotation;

	ChooserContext.bStrafe = (RotationMode != EAZ_RotationMode::OrientToMovement);

	ChooserContext.RotationOffset = FRotator::NormalizeAxis(
		CharacterProperties.AimingRotation.Yaw - CharacterTransform.Rotator().Yaw);

	ChooserContext.bIsMoving = (AccelerationAmount > MoveIntentDeadzone);

	if (ChooserContext.bIsMoving)
	{
		PendingStartAngleDeg = AZ::CmcAnim::SignedYawTo(
			Acceleration, static_cast<float>(RootTransform.Rotator().Yaw));
	}

	const UWorld* World = GetWorld();
	const float Now = World ? static_cast<float>(World->GetTimeSeconds()) : 0.f;
	{
		if (SensorReaction_GT != EAZ_ObstacleReaction::None)
		{
			LatchedReaction = SensorReaction_GT;
		}
		else if (!(LatchedReaction != EAZ_ObstacleReaction::None
			&& StateMachine && StateMachine->IsReactionActive(Now)))
		{
			LatchedReaction = EAZ_ObstacleReaction::None;
		}
		ChooserContext.Reaction = LatchedReaction;
	}

	FAZ_LocoSMInputs SMIn;
	SMIn.WorldNow             = Now;
	SMIn.bIsMoving            = ChooserContext.bIsMoving;
	SMIn.MovementMode         = MovementMode;
	SMIn.bSuppressLocomotion  = bSuppressLocomotion;
	SMIn.Stance               = Stance;
	SMIn.bHoldTakeoffPhase    = false;
	SMIn.PendingStartAngleDeg = PendingStartAngleDeg;
	SMIn.bStrafe              = ChooserContext.bStrafe;
	SMIn.MovementDirection    = ChooserContext.MovementDirection;
	SMIn.bObstacleReacting    = (ChooserContext.Reaction != EAZ_ObstacleReaction::None);
	SMIn.bStopOnAbortedStart  = true;
	SMIn.IdleBreakMinTime     = IdleBreakMinTime;
	SMIn.IdleBreakMaxTime     = IdleBreakMaxTime;

	if (!StateMachine)
	{
		return;
	}

	const FAZ_LocoSMOutputs SMOut = StateMachine->Tick(SMIn);
	ChooserContext.SMState           = SMOut.State;
	ChooserContext.StartDirection    = SMOut.StartDirection;
	ChooserContext.bMovingTransition = SMOut.bMovingTransition;
	ChooserContext.bJustLanded       = SMOut.bJustLanded;

	ChooserContext.FromStance = StateMachine->GetSettledStance();

	if (ChooserContext.SMState != PreviousSMState)
	{
		++TransitionSerial;
	}

	{
		const bool bAirState =
			ChooserContext.SMState == EAZ_StateMachineState::TransitionToInAir ||
			ChooserContext.SMState == EAZ_StateMachineState::InAirLoop;
		if (!bAirState)
		{
			LastGroundedLeftFootDown = ChooserContext.bLeftFootDown;
			LastGroundedIsMoving     = ChooserContext.bIsMoving;
		}
		else
		{
			ChooserContext.bLeftFootDown = LastGroundedLeftFootDown;
			ChooserContext.bIsMoving     = LastGroundedIsMoving;
		}
	}

	LeanAmount = Get_LeanAmount();
	const bool bForwardLean = ChooserContext.bIsMoving
		&& ChooserContext.MovementDirection == EAZ_MovementDirection::F
		&& ChooserContext.SMState == EAZ_StateMachineState::LocomotionLoop;
	LeanAlpha = FMath::FInterpTo(LeanAlpha, bForwardLean ? 1.f : 0.f, DeltaSeconds, 10.f);
}

void UAZ_CmcAnimInstance::SetBlendStackAnimFromChooser(
	bool bForceBlend,
	FAnimNodeReference BlendStackNode,
	FAZ_ChooserOutputs ChooserOut,
	UAnimationAsset* ChosenAnim,
	const TArray<UObject*>& Candidates)
{
	ChooserOutputs = ChooserOut;

	if (!ChosenAnim && Candidates.Num() == 0)
	{
		return;
	}

	const bool bInTransition =
		ChooserContext.SMState == EAZ_StateMachineState::TransitionToIdle ||
		ChooserContext.SMState == EAZ_StateMachineState::TransitionToLocomotion ||
		ChooserContext.SMState == EAZ_StateMachineState::TransitionToInAir ||
		ChooserContext.SMState == EAZ_StateMachineState::TransitionStance;

	if (bInTransition && TransitionSerial == LastPushedTransitionSerial && !bForceBlend)
	{
		return;
	}

	const bool bSelectionChanged =
		ChooserContext.SMState           != LastPushedSMState ||
		ChooserContext.Stance            != LastPushedStance ||
		ChooserContext.Gait              != LastPushedGait ||
		ChooserContext.MovementDirection != LastPushedDir ||
		ChooserContext.bLeftFootDown     != LastPushedLeftFootDown ||
		ChooserContext.Reaction          != LastPushedReaction;

	if (!bSelectionChanged && !bForceBlend && !ChooserOut.bUseMM && BlendStackInputs.Anim != nullptr)
	{
		return;
	}


	bool bAssetLooping = false;

	UObject* PublishAnim = nullptr;
	const UPoseSearchDatabase* PublishDb = nullptr;
	float PublishTime = 0.f;
	float PublishCost = 0.f;

	const float Crossfade = (PendingBlendOut > 0.f)
		? PendingBlendOut
		: static_cast<float>(ChooserOut.BlendTime);

	if (ChooserOut.bUseMM)
	{
		TArray<UObject*> AssetsToSearch = Candidates;
		if (AssetsToSearch.Num() == 0 && ChosenAnim)
		{
			AssetsToSearch.Add(ChosenAnim);
		}

		if (AssetsToSearch.Num() == 0)
		{
			for (UPoseSearchDatabase* Database : Get_DatabasesToSearch())
			{
				if (Database)
				{
					AssetsToSearch.Add(Database);
				}
			}
		}
		if (AssetsToSearch.Num() == 0)
		{
			return;
		}

		FPoseSearchBlueprintResult MMResult;
		UPoseSearchLibrary::MotionMatch(
			this, AssetsToSearch, PoseHistoryTag,
			FPoseSearchContinuingProperties(), FPoseSearchFutureProperties(),
			MMResult);

		UAnimationAsset* MMAnim = Cast<UAnimationAsset>(MMResult.SelectedAnim);
		double MMStartTime = MMResult.SelectedTime;
		if (!MMAnim)
		{
			MMAnim = ChosenAnim ? ChosenAnim : (Candidates.Num() > 0 ? Cast<UAnimationAsset>(Candidates[0]) : nullptr);
			MMStartTime = 0.0;
			if (!MMAnim)
			{
				return;
			}
		}
		else if (ChooserOut.MMCostLimit > 0.0 && MMResult.SearchCost > ChooserOut.MMCostLimit)
		{
			return;
		}

		UPoseSearchLibrary::IsAnimationAssetLooping(MMAnim, bAssetLooping);

		PublishAnim = MMAnim;
		PublishDb   = MMResult.SelectedDatabase.Get();
		PublishTime = static_cast<float>(MMStartTime);
		PublishCost = MMResult.SearchCost;

		BlendStackInputs.Anim         = MMAnim;
		BlendStackInputs.bLoop        = bAssetLooping;
		BlendStackInputs.StartTime    = MMStartTime;
		BlendStackInputs.BlendTime    = Crossfade;
		BlendStackInputs.BlendProfile = const_cast<UBlendProfile*>(GetBlendProfileByName(ChooserOut.BlendProfile));
		BlendStackInputs.Tags         = ChooserOut.Tags;
	}
	else
	{
		UAnimationAsset* DirectAnim = ChosenAnim
			? ChosenAnim
			: (Candidates.Num() > 0 ? Cast<UAnimationAsset>(Candidates[0]) : nullptr);
		if (!DirectAnim)
		{
			return;
		}
		UPoseSearchLibrary::IsAnimationAssetLooping(DirectAnim, bAssetLooping);

		PublishAnim = DirectAnim;
		PublishDb   = nullptr;
		PublishTime = static_cast<float>(ChooserOut.StartTime);
		PublishCost = 0.f;

		BlendStackInputs.Anim         = DirectAnim;
		BlendStackInputs.bLoop        = bAssetLooping;
		BlendStackInputs.StartTime    = ChooserOut.StartTime;
		BlendStackInputs.BlendTime    = Crossfade;
		BlendStackInputs.BlendProfile = const_cast<UBlendProfile*>(GetBlendProfileByName(ChooserOut.BlendProfile));
		BlendStackInputs.Tags         = ChooserOut.Tags;
	}


	PublishSelection(PublishAnim, PublishDb, PublishTime, bAssetLooping, PublishCost, ChooserOut.Tags);

	LastPushedSMState          = ChooserContext.SMState;
	LastPushedStance           = ChooserContext.Stance;
	LastPushedGait             = ChooserContext.Gait;
	LastPushedDir              = ChooserContext.MovementDirection;
	LastPushedLeftFootDown     = ChooserContext.bLeftFootDown;
	LastPushedReaction         = ChooserContext.Reaction;
	LastPushedTransitionSerial = TransitionSerial;

	PendingBlendOut = static_cast<float>(ChooserOut.BlendOut);

	const UWorld* World = GetWorld();
	const float Now = World ? static_cast<float>(World->GetTimeSeconds()) : 0.f;

	auto RemainingSeconds = [this]() -> float
	{
		if (const UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(BlendStackInputs.Anim))
		{
			return FMath::Max(0.05f, Seq->GetPlayLength() - static_cast<float>(BlendStackInputs.StartTime));
		}
		return -1.f;
	};

	if (ChooserContext.SMState == EAZ_StateMachineState::IdleBreak)
	{
		const float Remaining = RemainingSeconds();
		if (Remaining > 0.f && StateMachine)
		{
			StateMachine->NotifyIdleBreakClipPushed(Now, Remaining, IdleBreakAlmostCompleteThreshold);
		}
	}

	if (ChooserContext.Reaction != EAZ_ObstacleReaction::None
		&& ChooserContext.SMState == EAZ_StateMachineState::LocomotionLoop)
	{
		const float Remaining = RemainingSeconds();
		if (Remaining > 0.f && StateMachine)
		{
			StateMachine->NotifyReactionClipPushed(Now, Remaining, TransitionAlmostCompleteThreshold);
		}
	}

	if (bInTransition)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Verbose, TEXT("[CmcTrans] entry: SM=%d startDir=%d moveTrans=%d justLanded=%d ")
			TEXT("Lfoot=%d Dir=%d Gait=%d -> chosen=%s"),
			static_cast<int32>(ChooserContext.SMState), static_cast<int32>(ChooserContext.StartDirection),
			ChooserContext.bMovingTransition ? 1 : 0, ChooserContext.bJustLanded ? 1 : 0,
			ChooserContext.bLeftFootDown ? 1 : 0,
			static_cast<int32>(ChooserContext.MovementDirection),
			static_cast<int32>(ChooserContext.Gait), *GetNameSafe(BlendStackInputs.Anim));
#endif
		const float Remaining = RemainingSeconds();
		if (Remaining > 0.f && StateMachine)
		{
			StateMachine->NotifyTransitionClipPushed(Now, Remaining, TransitionAlmostCompleteThreshold);
		}

	}

	if (bForceBlend && !BlendStackInputs.bLoop)
	{
		EAnimNodeReferenceConversionResult ConvResult;
		const FBlendStackAnimNodeReference BSNode =
			UBlendStackAnimNodeLibrary::ConvertToBlendStackNode(BlendStackNode, ConvResult);
		if (ConvResult == EAnimNodeReferenceConversionResult::Succeeded)
		{
			UBlendStackAnimNodeLibrary::ForceBlendNextUpdate(BSNode);
		}
	}
}
