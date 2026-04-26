#include "Animation/AZ_AnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AZ_GameplayTags.h"
#include "Camera/CameraComponent.h"
#include "Character/AZ_HeroCharacter.h"
#include "Character/AZ_HeroPawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/BlueprintSpringMathLibrary.h"
#include "Weapon/AZ_Weapon.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "PoseSearch/PoseSearchTrajectoryPredictor.h"
#include "CharacterTrajectoryComponent.h"
#include "MoverPoseSearchTrajectoryPredictor.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNodeReference.h"
#include "BlendStack/BlendStackAnimNodeLibrary.h"
#include "AnimationWarpingLibrary.h"
#include "BoneControllers/AnimNode_OrientationWarping.h"
#include "PoseSearch/MotionMatchingAnimNodeLibrary.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "PoseSearch/PoseSearchResult.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "ChooserFunctionLibrary.h"
#include "Chooser.h"
#include "AlphaBlend.h"

namespace
{
	/** GASP 5-variable state tracking. On change: capture old duration as
	 *  LastStateTime, reset Time, latch LastFrame into Recent for RecentTimeLimit
	 *  seconds. After timer expires, Recent reverts to current. */
	template <typename TEnum>
	void UpdateStateTracking(TEnum& Recent, float& RecentTimer, float& Time, float& LastStateTime,
	                         TEnum Current, TEnum LastFrame, float DeltaSeconds, float RecentTimeLimit)
	{
		if (Current != LastFrame)
		{
			LastStateTime = Time;
			Time = 0.f;
			Recent = LastFrame;
			RecentTimer = RecentTimeLimit;
		}
		else
		{
			Time += DeltaSeconds;
			if (RecentTimer > 0.f)
			{
				RecentTimer -= DeltaSeconds;
				if (RecentTimer <= 0.f)
				{
					Recent = Current;
				}
			}
		}
	}
}

void UAZ_AnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	APawn* PawnOwner = TryGetPawnOwner();

	// Try new Mover pawn first, fall back to old ACharacter
	OwningHeroPawn = Cast<AAZ_HeroPawn>(PawnOwner);
	if (!OwningHeroPawn)
	{
		OwningCharacter = Cast<ACharacter>(PawnOwner);
		if (OwningCharacter)
		{
			MovementComponent = OwningCharacter->GetCharacterMovement();
		}
	}

	InitializeMoverPredictor();
}

// ========================================
// GASP UPDATE PIPELINE
// Split NativeUpdateAnimation into named functions matching the GASP
// SandboxCharacter_Mover_ABP graph layout. Phases 3–8 replace each in turn.
// ========================================

void UAZ_AnimInstance::InitializeMoverPredictor()
{
	if (OwningHeroPawn)
	{
		Predictor = OwningHeroPawn->MoverTrajectoryPredictor;
		Mover     = OwningHeroPawn->GetMoverComponent();
		bHasMover = (Mover != nullptr);
	}
	bHasOwningActor = (OwningHeroPawn != nullptr) || (OwningCharacter != nullptr);
}

void UAZ_AnimInstance::Update_CVarDrivenVariables()
{
	// Stub. LocomotionSetup / MMDatabaseLOD CVar reads land in a later phase.
}

void UAZ_AnimInstance::Update_PropertiesFromCharacter()
{
	// Movement flags + ground info from the pawn's thread-safe proxy (Mover) or CMC (legacy).
	if (OwningHeroPawn)
	{
		const FAZ_MoverStateProxy& MoverState = OwningHeroPawn->GetMoverStateSafe();
		Velocity    = MoverState.Velocity;
		GroundSpeed = MoverState.GroundSpeed;

		const bool bCurrentlyFalling = MoverState.bIsFalling;
		if (bWasFalling && !bCurrentlyFalling) bIsJumping = false;
		bIsFalling  = bCurrentlyFalling;
		bWasFalling = bCurrentlyFalling;

		bIsCrouching = MoverState.bIsCrouching;
		bIsAiming    = MoverState.bIsAiming;

		CharacterProperties.GroundNormal   = MoverState.GroundNormal;
		CharacterProperties.GroundLocation = MoverState.GroundLocation;
	}
	else if (OwningCharacter && MovementComponent)
	{
		Velocity    = MovementComponent->Velocity;
		GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.f).Size();

		const bool bCurrentlyFalling = MovementComponent->IsFalling();
		if (bWasFalling && !bCurrentlyFalling) bIsJumping = false;
		bIsFalling  = bCurrentlyFalling;
		bWasFalling = bCurrentlyFalling;

		bIsCrouching = MovementComponent->IsCrouching();

		const FFindFloorResult& Floor = MovementComponent->CurrentFloor;
		if (Floor.bBlockingHit)
		{
			CharacterProperties.GroundNormal   = Floor.HitResult.ImpactNormal;
			CharacterProperties.GroundLocation = Floor.HitResult.ImpactPoint;
		}
		else
		{
			CharacterProperties.GroundNormal   = FVector::UpVector;
			CharacterProperties.GroundLocation = FVector::ZeroVector;
		}
	}

	CharacterProperties.MovementDirection = MovementDirection;

	AActor* OwnerActor = OwningHeroPawn
		? static_cast<AActor*>(OwningHeroPawn.Get())
		: static_cast<AActor*>(OwningCharacter.Get());
	if (OwnerActor)
	{
		if (const APawn* Pawn = Cast<APawn>(OwnerActor))
		{
			if (const AController* PC = Pawn->GetController())
			{
				CharacterProperties.AimingRotation = PC->GetControlRotation();
			}
		}
	}

	if (bForceFootPlacementReset) bForceFootPlacementReset = false;
}

void UAZ_AnimInstance::Update_Trajectory(float DeltaSeconds)
{
	// Generate 15 history + 15 future samples with deceleration prediction (Mover) or
	// copy the legacy CharacterTrajectoryComponent buffer into our Trajectory field.
	if (OwningHeroPawn)
	{
		if (UMoverTrajectoryPredictor* PawnPredictor = OwningHeroPawn->MoverTrajectoryPredictor)
		{
			TScriptInterface<IPoseSearchTrajectoryPredictorInterface> PredictorInterface;
			PredictorInterface.SetObject(PawnPredictor);
			PredictorInterface.SetInterface(Cast<IPoseSearchTrajectoryPredictorInterface>(PawnPredictor));

			UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectoryWithPredictor(
				PredictorInterface,
				DeltaSeconds,
				Trajectory,
				PreviousDesiredControllerYaw,
				Trajectory,
				/*HistorySamplingInterval*/ 0.033f,
				/*HistoryCount*/ 15,
				/*PredictionSamplingInterval*/ 0.1f,
				/*PredictionCount*/ 15);
		}
	}
	else if (OwningCharacter)
	{
		if (const AAZ_HeroCharacter* Hero = Cast<AAZ_HeroCharacter>(OwningCharacter))
		{
			if (UCharacterTrajectoryComponent* TrajComp = Hero->CharacterTrajectory)
			{
				FProperty* Prop = TrajComp->GetClass()->FindPropertyByName(TEXT("Trajectory"));
				if (Prop)
				{
					Prop->CopyCompleteValue(&Trajectory, Prop->ContainerPtrToValuePtr<void>(TrajComp));
				}
			}
		}
	}

	if (Trajectory.Samples.Num() <= 0) return;

	UPoseSearchTrajectoryLibrary::GetTransformTrajectoryVelocity(Trajectory,  0.4f,  0.5f, Trj_FutureVelocity,     false);
	UPoseSearchTrajectoryLibrary::GetTransformTrajectoryVelocity(Trajectory,  0.1f,  0.2f, Trj_NearFutureVelocity, false);
	UPoseSearchTrajectoryLibrary::GetTransformTrajectoryVelocity(Trajectory, -0.3f, -0.2f, Trj_PastVelocity,       false);

	FTransformTrajectorySample FutureSample;
	UPoseSearchTrajectoryLibrary::GetTransformTrajectorySampleAtTime(Trajectory, 1.5f, FutureSample, false);
	Trj_FutureFacing = FutureSample.Facing.Rotator();

	// Cumulative facing delta across the GASP sample set, via Get_TotalFacingDelta helper.
	static const TArray<float> FacingSampleTimes = { 0.f, 0.25f, 0.75f, 1.5f };
	FutureFacingDelta = Get_TotalFacingDelta(FacingSampleTimes);

	FVector PastAngVel = FVector::ZeroVector;
	FVector CurAngVel  = FVector::ZeroVector;
	UPoseSearchTrajectoryLibrary::GetTransformTrajectoryAngularVelocity(Trajectory, -0.3f, -0.2f, PastAngVel, false);
	UPoseSearchTrajectoryLibrary::GetTransformTrajectoryAngularVelocity(Trajectory,  0.0f,  0.1f, CurAngVel,  false);
	Trj_PastAngularVelocity    = PastAngVel;
	Trj_CurrentAngularVelocity = CurAngVel;
	Trj_TurnAngle = FutureFacingDelta;

	Trj_IsCircling   = FMath::Abs(PastAngVel.Z) > 200.f && FMath::Abs(CurAngVel.Z) > 200.f;
	Trj_CirclingTime = Trj_IsCircling ? (Trj_CirclingTime + DeltaSeconds) : 0.f;
}

void UAZ_AnimInstance::Update_EssentialValues(float DeltaSeconds)
{
	// LastFrame snapshots BEFORE any update.
	Velocity_LastFrame           = Velocity;
	Acceleration_LastFrame       = Acceleration;
	CharacterTransform_LastFrame = CharacterTransform;
	FutureFacingDelta_LastFrame  = FutureFacingDelta;
	Trj_PreviousFutureVelocity   = Trj_FutureVelocity;

	AActor* OwnerActor = OwningHeroPawn
		? static_cast<AActor*>(OwningHeroPawn.Get())
		: static_cast<AActor*>(OwningCharacter.Get());

	// HeroPawn Velocity is set in Update_PropertiesFromCharacter from the Mover proxy.
	// Legacy Character path reads Velocity from the actor (GetVelocity() == 0 for Mover pawns).
	if (!OwningHeroPawn)
	{
		Velocity = OwnerActor ? OwnerActor->GetVelocity() : FVector::ZeroVector;
	}
	Speed2D      = FVector(Velocity.X, Velocity.Y, 0.f).Size();
	bHasVelocity = Speed2D > 10.f;
	if (bHasVelocity) LastNonZeroVelocity = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();

	if (DeltaSeconds > UE_KINDA_SMALL_NUMBER)
	{
		VelocityAcceleration = (Velocity - Velocity_LastFrame) / DeltaSeconds;
	}

	// Acceleration = player intent rotated to world (GASP parity), NOT dv/dt. The derivative
	// trends to 0 at steady-state (V_n ≈ V_n−1 at MaxSpeed) which would flip MovementState
	// to Idle while W is still held. Intent keeps bAccelerating=true across the whole hold.
	// VelocityAcceleration (the derivative) is still exposed separately for AdditiveLean.
	if (OwningHeroPawn)
	{
		const FVector LocalIntent = OwningHeroPawn->GetCachedMoveInputIntent();
		if (LocalIntent.SizeSquared() > UE_KINDA_SMALL_NUMBER)
		{
			const AController* Controller = OwningHeroPawn->GetController();
			const FRotator ControlRot = Controller ? Controller->GetControlRotation() : FRotator::ZeroRotator;
			const FVector WorldIntent = ControlRot.RotateVector(LocalIntent);
			float MaxAccel = 4000.f;
			if (const UCharacterMoverComponent* MoverComp = OwningHeroPawn->GetMoverComponent())
			{
				if (const UCommonLegacyMovementSettings* Settings = MoverComp->FindSharedSettings<UCommonLegacyMovementSettings>())
				{
					MaxAccel = Settings->Acceleration;
				}
			}
			Acceleration = WorldIntent * MaxAccel;
		}
		else
		{
			Acceleration = FVector::ZeroVector;
		}
	}
	else if (MovementComponent)
	{
		Acceleration = MovementComponent->GetCurrentAcceleration();
	}
	else
	{
		Acceleration = VelocityAcceleration;
	}
	AccelerationAmount = Acceleration.Size();
	bHasAcceleration   = AccelerationAmount > 10.f;

	if (OwnerActor)
	{
		CharacterTransform   = OwnerActor->GetActorTransform();
		RelativeAcceleration = CharacterTransform.InverseTransformVectorNoScale(Acceleration);
	}
}

void UAZ_AnimInstance::Update_States(float DeltaSeconds)
{
	MovementState_LastFrame     = MovementState;
	MovementMode_LastFrame      = MovementMode;
	Gait_LastFrame              = Gait;
	Stance_LastFrame            = Stance;
	MovementDirection_LastFrame = MovementDirection;
	RotationMode_LastFrame      = RotationMode;

	MovementMode = bIsFalling  ? EAZ_MovementMode::InAir    : EAZ_MovementMode::OnGround;
	Stance       = bIsCrouching ? EAZ_Stance::Crouching     : EAZ_Stance::Standing;

	MovementState = IsMoving() ? EAZ_MovementState::Moving : EAZ_MovementState::Idle;

	if      (Speed2D < 200.f) Gait = EAZ_Gait::Walk;
	else if (Speed2D < 500.f) Gait = EAZ_Gait::Run;
	else                      Gait = EAZ_Gait::Sprint;

	AActor* OwnerActor = OwningHeroPawn
		? static_cast<AActor*>(OwningHeroPawn.Get())
		: static_cast<AActor*>(OwningCharacter.Get());
	if (bHasVelocity && OwnerActor)
	{
		const FVector Forward = OwnerActor->GetActorForwardVector();
		const FVector Right   = OwnerActor->GetActorRightVector();
		const FVector Vel2D   = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
		const float ForwardDot = FVector::DotProduct(Forward, Vel2D);
		const float RightDot   = FVector::DotProduct(Right,   Vel2D);
		if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
		{
			MovementDirection = ForwardDot >= 0.f ? EAZ_MovementDirection::F : EAZ_MovementDirection::B;
		}
		else
		{
			MovementDirection = RightDot >= 0.f ? EAZ_MovementDirection::RR : EAZ_MovementDirection::LL;
		}
	}

	UpdateStateTracking(MovementState_Recent,     MovementState_RecentTimer,     MovementState_Time,     MovementState_LastStateTime,     MovementState,     MovementState_LastFrame,     DeltaSeconds, 0.1f);
	UpdateStateTracking(MovementMode_Recent,      MovementMode_RecentTimer,      MovementMode_Time,      MovementMode_LastStateTime,      MovementMode,      MovementMode_LastFrame,      DeltaSeconds, 0.2f);
	UpdateStateTracking(Gait_Recent,              Gait_RecentTimer,              Gait_Time,              Gait_LastStateTime,              Gait,              Gait_LastFrame,              DeltaSeconds, 0.1f);
	UpdateStateTracking(Stance_Recent,            Stance_RecentTimer,            Stance_Time,            Stance_LastStateTime,            Stance,            Stance_LastFrame,            DeltaSeconds, 0.1f);
	UpdateStateTracking(MovementDirection_Recent, MovementDirection_RecentTimer, MovementDirection_Time, MovementDirection_LastStateTime, MovementDirection, MovementDirection_LastFrame, DeltaSeconds, 0.1f);
	UpdateStateTracking(RotationMode_Recent,      RotationMode_RecentTimer,      RotationMode_Time,      RotationMode_LastStateTime,      RotationMode,      RotationMode_LastFrame,      DeltaSeconds, 0.1f);
}

void UAZ_AnimInstance::Update_AimOffset(float DeltaSeconds)
{
	// GASP parity — smooth a worldspace aim target, take the delta from root rotation as
	// an AO Vector2D (X=Yaw, Y=Pitch). Gate AO enable on rotation mode, montage slot
	// weight, and angle thresholds. If gates fail, snap SmoothedAimTarget to target so
	// the next frame's spring starts fresh (no pull-back jitter).
	AO_AimTarget = CharacterProperties.AimingRotation;

	UBlueprintSpringMathLibrary::CriticalSpringDampRotator(
		SmoothedAimTarget, InOutAngularVelocity, AO_AimTarget, DeltaSeconds, AimTargetSmoothingTime);

	Previous_AO = AO;
	const FRotator RootRot = RootTransform.Rotator();
	const FRotator DeltaSmoothed = UKismetMathLibrary::NormalizedDeltaRotator(SmoothedAimTarget, RootRot);
	AO = FVector2D(DeltaSmoothed.Yaw, DeltaSmoothed.Pitch);

	// EnableAO gates:
	//  1. abs yaw delta root→aim target <= (180 if Idle, else 110)
	//  2. RotationMode == Strafe OR Aiming
	//  3. DefaultSlot local weight < 0.5 (no strong montage)
	//  4. abs(AO.X - Previous_AO.X) < 135 (prevents snap re-enable)
	const FRotator DeltaRaw = UKismetMathLibrary::NormalizedDeltaRotator(AO_AimTarget, RootRot);
	const float MaxYaw = (MovementState == EAZ_MovementState::Idle) ? 180.f : 110.f;
	const bool bYawOK   = FMath::Abs(DeltaRaw.Yaw) <= MaxYaw;
	const bool bModeOK  = (RotationMode == EAZ_RotationMode::Strafe) || (RotationMode == EAZ_RotationMode::Aiming);
	const bool bSlotOK  = GetSlotMontageLocalWeight(FName("DefaultSlot")) < 0.5f;
	const bool bDeltaOK = FMath::Abs(AO.X - Previous_AO.X) < 135.f;

	EnableAO = bYawOK && bModeOK && bSlotOK && bDeltaOK;

	if (!EnableAO)
	{
		SmoothedAimTarget = AO_AimTarget;
	}
}

void UAZ_AnimInstance::Update_AdditiveLean(float /*DeltaSeconds*/)
{
	// GASP parity — VelocityAcceleration rotated into velocity-aligned local space,
	// Y component is the lateral accel signal. Normalize by a speed-dependent divisor
	// into [-1..1], then pack into LeanAmount Vector2D according to MovementDirection:
	//   F      → (lat,  0)
	//   B      → (-lat, 0)look 
	//   LL/LR  → (0, -lat)
	//   RL/RR  → (0,  lat)
	const FRotator VelocityRot = UKismetMathLibrary::Conv_VectorToRotator(Velocity);
	const FVector  LocalAccel  = VelocityRot.UnrotateVector(VelocityAcceleration);

	const float Divisor = FMath::GetMappedRangeValueClamped(FVector2D(200.f, 320.f), FVector2D(500.f, 800.f), Speed2D);
	LateralAccelerationAmount = FMath::Clamp(LocalAccel.Y / Divisor, -1.f, 1.f);

	switch (MovementDirection)
	{
	case EAZ_MovementDirection::F:
		LeanAmount = FVector2D(LateralAccelerationAmount, 0.f);
		break;
	case EAZ_MovementDirection::B:
		LeanAmount = FVector2D(-LateralAccelerationAmount, 0.f);
		break;
	case EAZ_MovementDirection::LL:
	case EAZ_MovementDirection::LR:
		LeanAmount = FVector2D(0.f, -LateralAccelerationAmount);
		break;
	case EAZ_MovementDirection::RL:
	case EAZ_MovementDirection::RR:
		LeanAmount = FVector2D(0.f, LateralAccelerationAmount);
		break;
	}
}

void UAZ_AnimInstance::Update_Logic(float DeltaSeconds)
{
	Update_Trajectory(DeltaSeconds);
	Update_EssentialValues(DeltaSeconds);
	Update_States(DeltaSeconds);
	Update_AimOffset(DeltaSeconds);
	Update_AdditiveLean(DeltaSeconds);
}

void UAZ_AnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Lazy init: retry if NativeInitializeAnimation ran before the pawn was ready.
	if (!OwningHeroPawn && !OwningCharacter)
	{
		APawn* PawnOwner = TryGetPawnOwner();
		OwningHeroPawn = Cast<AAZ_HeroPawn>(PawnOwner);
		if (!OwningHeroPawn)
		{
			OwningCharacter = Cast<ACharacter>(PawnOwner);
			if (OwningCharacter)
			{
				MovementComponent = OwningCharacter->GetCharacterMovement();
			}
		}
		InitializeMoverPredictor();
	}

	if (!OwningHeroPawn && !OwningCharacter) return;

	// GASP pipeline.
	Update_CVarDrivenVariables();
	Update_PropertiesFromCharacter();
	Update_Logic(DeltaSeconds);

	// --- Resolve weapon pose state from animation bools (priority order) ---
	if (bIsShooting && bIsCrouching)
		CurrentWeaponPoseState = EAZ_WeaponPoseState::CrouchShooting;
	else if (bIsShooting)
		CurrentWeaponPoseState = EAZ_WeaponPoseState::Shooting;
	else if (bIsAiming && bIsCrouching)
		CurrentWeaponPoseState = EAZ_WeaponPoseState::CrouchAiming;
	else if (bIsAiming)
		CurrentWeaponPoseState = EAZ_WeaponPoseState::Aiming;
	else if (bIsCrouching)
		CurrentWeaponPoseState = EAZ_WeaponPoseState::Crouching;
	else
		CurrentWeaponPoseState = EAZ_WeaponPoseState::Relaxed;

	// --- Cache ASC (may not be available until PlayerState replicates) ---
	AActor* OwningActor = OwningHeroPawn ? static_cast<AActor*>(OwningHeroPawn.Get()) : static_cast<AActor*>(OwningCharacter.Get());
	if (!CachedASC && OwningActor)
	{
		CachedASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningActor);
	}

	// --- Weapon state from ASC tags ---
	if (CachedASC)
	{
		FGameplayTagContainer OwnedTags;
		CachedASC->GetOwnedGameplayTags(OwnedTags);

		// Find the first tag under Item.Type.Weapon parent tag
		const FGameplayTag WeaponParent = FGameplayTag::RequestGameplayTag(FName("Item.Type.Weapon"), false);
		CurrentWeaponTag = FGameplayTag::EmptyTag;
		if (WeaponParent.IsValid())
		{
			for (const FGameplayTag& Tag : OwnedTags)
			{
				if (Tag.MatchesTag(WeaponParent))
				{
					CurrentWeaponTag = Tag;
					break;
				}
			}
		}
	}

	// --- Weapon anim index for Blend Poses by int ---
	// 0=Unarmed, 1=Rifle, 2=Pistol, 3=Shotgun, 4=SMG, 5=Melee
	{
		const auto& Tags = FAZ_GameplayTags::Get();
		if (!CurrentWeaponTag.IsValid())
			WeaponAnimIndex = 0;
		else if (CurrentWeaponTag.MatchesTagExact(Tags.Item_Type_Weapon_Rifle))
			WeaponAnimIndex = 1;
		else if (CurrentWeaponTag.MatchesTagExact(Tags.Item_Type_Weapon_Pistol))
			WeaponAnimIndex = 2;
		else if (CurrentWeaponTag.MatchesTagExact(Tags.Item_Type_Weapon_Shotgun))
			WeaponAnimIndex = 3;
		else if (CurrentWeaponTag.MatchesTagExact(Tags.Item_Type_Weapon_SMG))
			WeaponAnimIndex = 4;
		else if (CurrentWeaponTag.MatchesTagExact(Tags.Item_Type_Weapon_Melee))
			WeaponAnimIndex = 5;
		else
			WeaponAnimIndex = 0;
	}


	// --- Camera interpolation (old character only — HeroPawn uses UAZ_PawnCameraMovementComponent) ---
	if (!OwningHeroPawn && OwningCharacter)
	if (AAZ_HeroCharacter* Hero = Cast<AAZ_HeroCharacter>(OwningCharacter))
	{
		// Determine target values based on current stance
		float TargetFOV;
		float TargetBoomLength;
		float TargetOffsetY;
		float TargetOffsetZ;
		float InterpSpeed;

		if (bIsAiming)
		{
			TargetFOV = Hero->AimFOV;
			TargetBoomLength = bIsCrouching ? FMath::Min(Hero->AimBoomLength, Hero->CrouchBoomLength) : Hero->AimBoomLength;
			TargetOffsetY = Hero->AimSocketOffsetY;
			TargetOffsetZ = bIsCrouching ? Hero->CrouchSocketOffsetZ : Hero->AimSocketOffsetZ;
			InterpSpeed = Hero->CameraAimInterpSpeed;
		}
		else if (bIsCrouching)
		{
			TargetFOV = Hero->Default3PFOV;
			TargetBoomLength = Hero->CrouchBoomLength;
			TargetOffsetY = Hero->CrouchSocketOffsetY;
			TargetOffsetZ = Hero->CrouchSocketOffsetZ;
			InterpSpeed = Hero->CameraCrouchInterpSpeed;
		}
		else
		{
			TargetFOV = Hero->Default3PFOV;
			TargetBoomLength = Hero->CameraBoomArmLength;
			TargetOffsetY = Hero->CameraBoomSocketOffsetY;
			TargetOffsetZ = Hero->CameraBoomSocketOffsetZ;
			InterpSpeed = Hero->CameraAimInterpSpeed;
		}

		// FOV
		UCameraComponent* Camera = Hero->bIsFirstPersonPerspective
			? Hero->FirstPersonCamera
			: Hero->ThirdPersonCamera;
		if (Camera)
		{
			Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetFOV, DeltaSeconds, InterpSpeed));
		}

		// Spring arm
		if (USpringArmComponent* Boom = Hero->ThirdPersonCameraBoom)
		{
			// Compensate for capsule height change — boom is on root, root drops instantly
			const float StandingHalfHeight = Hero->GetDefaultHalfHeight();
			const float CrouchedHalfHeight = MovementComponent->GetCrouchedHalfHeight();
			const float CapsuleDelta = bIsCrouching ? (StandingHalfHeight - CrouchedHalfHeight) : 0.f;
			// Add capsule delta to target Z so the camera stays at the same world height
			// Our interp then smoothly removes it
			TargetOffsetZ += CapsuleDelta;

			Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, TargetBoomLength, DeltaSeconds, InterpSpeed);

			// Select directional offsets based on stance
			FVector2D OffFwd, OffBwd, OffLt, OffRt;
			if (bIsAiming)
			{
				OffFwd = Hero->CameraOffsetAimForward;
				OffBwd = Hero->CameraOffsetAimBackward;
				OffLt  = Hero->CameraOffsetAimLeft;
				OffRt  = Hero->CameraOffsetAimRight;
			}
			else if (bIsCrouching)
			{
				OffFwd = Hero->CameraOffsetCrouchForward;
				OffBwd = Hero->CameraOffsetCrouchBackward;
				OffLt  = Hero->CameraOffsetCrouchLeft;
				OffRt  = Hero->CameraOffsetCrouchRight;
			}
			else
			{
				OffFwd = Hero->CameraOffsetForward;
				OffBwd = Hero->CameraOffsetBackward;
				OffLt  = Hero->CameraOffsetLeft;
				OffRt  = Hero->CameraOffsetRight;
			}

			// Blend per-direction offsets by movement speed
			FVector2D MoveOffset(0.f, 0.f);

			if (NormalizedWalkRightSpeed > 0.f)
			{
				MoveOffset += OffRt * NormalizedWalkRightSpeed;
			}
			else if (NormalizedWalkRightSpeed < 0.f)
			{
				MoveOffset += OffLt * FMath::Abs(NormalizedWalkRightSpeed);
			}

			if (NormalizedWalkForwardSpeed > 0.f)
			{
				MoveOffset += OffFwd * NormalizedWalkForwardSpeed;
			}
			else if (NormalizedWalkForwardSpeed < 0.f)
			{
				MoveOffset += OffBwd * FMath::Abs(NormalizedWalkForwardSpeed);
			}

			// Interp base stance offset at stance speed, directional offset at move speed
			FVector CurrentOffset = Boom->SocketOffset;

			// Base stance target (without movement)
			const float BaseY = FMath::FInterpTo(CurrentOffset.Y - MoveOffset.X, TargetOffsetY, DeltaSeconds, InterpSpeed) + MoveOffset.X;
			const float BaseZ = FMath::FInterpTo(CurrentOffset.Z - MoveOffset.Y, TargetOffsetZ, DeltaSeconds, InterpSpeed) + MoveOffset.Y;

			CurrentOffset.Y = BaseY;
			CurrentOffset.Z = BaseZ;
			Boom->SocketOffset = CurrentOffset;
		}
	}

	// --- Weapon aim positioning (DISABLED — weapon stays on RelaxedSocket) ---
	// TODO: Re-enable socket transition when aim offset + IK setup is finalized
	/*
	if ((bIsAiming || bIsShooting) && CurrentWeaponTag.IsValid())
	{
		if (!CachedPrimaryWeapon.IsValid())
		{
			const FName PrimaryTag = FAZ_GameplayTags::Get().Weapon_Slot_Primary.GetTagName();
			TArray<AActor*> AttachedActors;
			OwningCharacter->GetAttachedActors(AttachedActors);
			for (AActor* Attached : AttachedActors)
			{
				if (Attached->ActorHasTag(PrimaryTag))
				{
					CachedPrimaryWeapon = Attached;
					break;
				}
			}
		}

		if (AAZ_Weapon* Weapon = Cast<AAZ_Weapon>(CachedPrimaryWeapon.Get()))
		{
			USceneComponent* WeaponRoot = Weapon->GetRootComponent();
			USkeletalMeshComponent* CharMesh = OwningCharacter->GetMesh();

			if (WeaponRoot && CharMesh)
			{
				const FTransform AimWorld = CharMesh->GetSocketTransform(Weapon->AimSocketName, RTS_World);
				const FTransform AttachWorld = WeaponRoot->GetAttachParent()
					? WeaponRoot->GetAttachParent()->GetSocketTransform(WeaponRoot->GetAttachSocketName())
					: CharMesh->GetSocketTransform(Weapon->RelaxedSocketName, RTS_World);
				const FTransform TargetRelative = AimWorld.GetRelativeTransform(AttachWorld);

				if (bIsShooting)
				{
					CurrentWeaponRelativeTransform = TargetRelative;
				}
				else
				{
					const float Alpha = FMath::Clamp(DeltaSeconds * WeaponPoseInterpSpeed, 0.f, 1.f);
					CurrentWeaponRelativeTransform.BlendWith(TargetRelative, Alpha);
				}

				WeaponRoot->SetRelativeTransform(CurrentWeaponRelativeTransform);
			}

		}
	}
	else if (CachedPrimaryWeapon.IsValid())
	{
		// Not aiming — smooth back to identity
		if (USceneComponent* WeaponRoot = CachedPrimaryWeapon->GetRootComponent())
		{
			const float Alpha = FMath::Clamp(DeltaSeconds * WeaponPoseInterpSpeed, 0.f, 1.f);
			CurrentWeaponRelativeTransform.BlendWith(FTransform::Identity, Alpha);
			WeaponRoot->SetRelativeTransform(CurrentWeaponRelativeTransform);

			if (CurrentWeaponRelativeTransform.GetLocation().IsNearlyZero(0.1f))
			{
				WeaponRoot->SetRelativeTransform(FTransform::Identity);
				CurrentWeaponRelativeTransform = FTransform::Identity;
				CachedPrimaryWeapon = nullptr;
			}
		}
	}
	*/

	// --- Aim Target (crosshair trace) ---
	if (bWantsAimPose && OwningActor)
	{
		// Find camera on either pawn type
		const UCameraComponent* AimCamera = nullptr;
		if (OwningHeroPawn)
		{
			AimCamera = OwningHeroPawn->ThirdPersonCamera;
		}
		else if (const AAZ_HeroCharacter* Hero = Cast<AAZ_HeroCharacter>(OwningCharacter))
		{
			AimCamera = Hero->bIsFirstPersonPerspective ? Hero->FirstPersonCamera : Hero->ThirdPersonCamera;
		}

		if (AimCamera)
		{
			const FVector TraceStart = AimCamera->GetComponentLocation();
			const FVector TraceEnd = TraceStart + AimCamera->GetForwardVector() * AimTraceDistance;

			FHitResult HitResult;
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(OwningActor);

			if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, Params))
			{
				AimTarget = HitResult.ImpactPoint;
			}
			else
			{
				AimTarget = TraceEnd;
			}

			const FVector CharLocation = OwningActor->GetActorLocation();
			const FVector DirToTarget = (AimTarget - CharLocation).GetSafeNormal();
			const FRotator AimRotation = DirToTarget.Rotation();
			const FRotator CharRotation = OwningActor->GetActorRotation();
			const FRotator Delta = (AimRotation - CharRotation).GetNormalized();
			AimYaw = Delta.Yaw;
			AimPitch = Delta.Pitch;
		}
	}

	// --- Left Hand IK ---
	bUseLeftHandIK = false;
	if (bEnableLeftHandIK && CurrentWeaponTag.IsValid())
	{
		AAZ_Weapon* Weapon = Cast<AAZ_Weapon>(CachedPrimaryWeapon.IsValid() ? CachedPrimaryWeapon.Get() : nullptr);
		if (!Weapon)
		{
			Weapon = GetPrimaryWeapon();
		}

		if (Weapon)
		{
			USkeletalMeshComponent* CharMesh = OwningHeroPawn ? OwningHeroPawn->GetMainMesh() : (OwningCharacter ? OwningCharacter->GetMesh() : nullptr);
			FTransform TargetTransform;
			if (CharMesh && Weapon->GetLeftHandSocket(CharMesh, WeaponAttachSocket, CurrentWeaponPoseState, TargetTransform))
			{
				// Only interpolate on state change
				if (CurrentWeaponPoseState != LastIKPoseState)
				{
					LastIKPoseState = CurrentWeaponPoseState;
					bIsIKBlending = true;
				}

				if (bIsIKBlending)
				{
					const float Alpha = FMath::Clamp(DeltaSeconds * LeftHandIKInterpSpeed, 0.f, 1.f);
					LeftHandIKTransform.BlendWith(TargetTransform, Alpha);

					if (LeftHandIKTransform.GetLocation().Equals(TargetTransform.GetLocation(), 0.1f))
					{
						LeftHandIKTransform = TargetTransform;
						bIsIKBlending = false;
					}
				}
				else
				{
					LeftHandIKTransform = TargetTransform;
				}

				bUseLeftHandIK = true;
			}
		}
	}

	// --- Normalized blendspace inputs ---
	// RE-style: character faces camera direction (bUseControllerRotationYaw).
	// Use velocity transformed to local space so animation follows actual movement,
	// including deceleration slide when input is released.
	if (MaxGroundSpeed > UE_KINDA_SMALL_NUMBER && GroundSpeed > UE_KINDA_SMALL_NUMBER && OwningActor)
	{
		const FVector GroundVelocity(Velocity.X, Velocity.Y, 0.f);
		const FVector LocalVelocity = OwningActor->GetActorRotation().UnrotateVector(GroundVelocity);

		NormalizedWalkForwardSpeed = FMath::Clamp(LocalVelocity.X / MaxGroundSpeed, -1.f, 1.f);
		NormalizedWalkRightSpeed = FMath::Clamp(LocalVelocity.Y / MaxGroundSpeed, -1.f, 1.f);
	}
	else
	{
		NormalizedWalkForwardSpeed = 0.f;
		NormalizedWalkRightSpeed = 0.f;
	}

	// --- Chooser state tracking ---
	// Update direction reversal BEFORE updating last frame values
	bDirectionReversed = (NormalizedWalkForwardSpeed * LastNormalizedForwardSpeed < -0.1f);
	LastNormalizedForwardSpeed = NormalizedWalkForwardSpeed;
	// bWasMovingLastFrame is read by WasMoving() THEN updated at end of frame
	// So during this frame, WasMoving() returns last frame's state
	bWasMovingLastFrame = GroundSpeed > 10.f;

	// Cache turning state as a bool property for Chooser BoolColumn bindings
	// (equivalent to GASP's S_CharacterPropertiesForAnimation.bIsTurning).
	bIsTurning = ShouldTurnInPlace();

	// TEMP DEBUG — remove after testing
	if (GEngine && OwningHeroPawn)
	{
		const FString AnimName = BlendStackInputs.Anim ? BlendStackInputs.Anim->GetName() : TEXT("None");
		FString TagsStr;
		for (const FName& T : BlendStackInputs.Tags) { if (!TagsStr.IsEmpty()) TagsStr += TEXT(","); TagsStr += T.ToString(); }
		static const TCHAR* SMNames[] = {
			TEXT("IdleLp"), TEXT("TransIdle"), TEXT("LocoLp"), TEXT("TransLoco"),
			TEXT("AirLp"), TEXT("TransAir"), TEXT("IdleBrk"), TEXT("TransSlide"), TEXT("SlideLp")
		};
		const int32 SMIdx = FMath::Clamp((int32)StateMachineState, 0, 8);

		FString ChooserAnimName;
		for (const FName& T : ChooserOutputs.Tags) { if (!ChooserAnimName.IsEmpty()) ChooserAnimName += TEXT(","); ChooserAnimName += T.ToString(); }
		const FString ChooserInfo = FString::Printf(TEXT("BT=%.2f BP=%s MM=%d"),
			ChooserOutputs.BlendTime, *ChooserOutputs.BlendProfile.ToString(), ChooserOutputs.bUseMM);

		// Compute CurrentDelta for overlay (same expression ShouldTurnInPlace uses)
		const float DbgCurrentDelta = FMath::Abs(FRotator::NormalizeAxis(
			CharacterProperties.AimingRotation.Yaw - CharacterTransform.Rotator().Yaw));

		GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Yellow,
			FString::Printf(TEXT("SM=%s | Spd=%.0f | Turn=%d | CurDelta=%.1f | FutDelta=%.1f | Tags=%s"),
				SMNames[SMIdx], Speed2D, bIsTurning, DbgCurrentDelta, FutureFacingDelta, *TagsStr));
		GEngine->AddOnScreenDebugMessage(-2, 0.f, FColor::Cyan,
			FString::Printf(TEXT("Anim=%s"), *AnimName));
		GEngine->AddOnScreenDebugMessage(-3, 0.f, FColor::Green,
			FString::Printf(TEXT("CHT: %s | OutTags=%s"), *ChooserInfo, *ChooserAnimName));
		GEngine->AddOnScreenDebugMessage(-4, 0.f, FColor::Orange,
			FString::Printf(TEXT("Delta=%.1f | Dir=%d | MoveState=%d | NoAnim=%d | bLoop=%d | LST=%.2f"),
				FutureFacingDelta,
				(int32)MovementDirection, (int32)MovementState,
				bNoValidAnim, BlendStackInputs.bLoop, MovementState_LastStateTime));
	}

	// Detect locomotion database change — pulse true for exactly one frame
	const bool bDatabaseActuallyChanged = (CurrentSelectedDatabase != PreviousSelectedDatabase);
	PreviousSelectedDatabase = CurrentSelectedDatabase;
	bLocomotionDatabaseChanged = bDatabaseActuallyChanged;

}

int32 UAZ_AnimInstance::GetGait() const
{
	if (GroundSpeed < 10.f) return 0;       // Idle
	if (GroundSpeed < 200.f) return 1;      // Walk
	if (GroundSpeed < 500.f) return 2;      // Run
	return 3;                                // Sprint
}

AAZ_Weapon* UAZ_AnimInstance::GetPrimaryWeapon() const
{
	AActor* Owner = OwningHeroPawn ? static_cast<AActor*>(OwningHeroPawn.Get()) : static_cast<AActor*>(OwningCharacter.Get());
	if (!Owner) return nullptr;

	const FName PrimaryTag = FAZ_GameplayTags::Get().Weapon_Slot_Primary.GetTagName();
	TArray<AActor*> AttachedActors;
	Owner->GetAttachedActors(AttachedActors);
	for (AActor* Attached : AttachedActors)
	{
		if (Attached->ActorHasTag(PrimaryTag))
		{
			return Cast<AAZ_Weapon>(Attached);
		}
	}

	return nullptr;
}

// ========================================
// TRANSITION CONDITIONS
// ========================================

bool UAZ_AnimInstance::IsStarting() const
{
	// Character is starting to move: has future velocity + acceleration, not currently a pivot
	const float FutureSpeed = FVector(Trj_FutureVelocity.X, Trj_FutureVelocity.Y, 0.f).Size();
	return bHasVelocity && FutureSpeed >= Speed2D + 100.f
		&& !CurrentDatabaseTags.Contains(FName("Pivots"))
		&& Speed2D < 100.f;
}

bool UAZ_AnimInstance::IsPivoting() const
{
	// Direction reversal while moving
	return MovementState == EAZ_MovementState::Moving && bDirectionReversed;
}

bool UAZ_AnimInstance::ShouldTurnInPlace() const
{
	if (Speed2D >= 50.f || MovementState != EAZ_MovementState::Idle) return false;

	// Single source of truth: HeroPawn's bIdleTurnInProgress is set when the
	// accumulator commits a turn (60° of mouse motion) and clears when the body
	// finishes rotating to target. AnimInstance just mirrors it for SM/anim.
	if (OwningHeroPawn) return OwningHeroPawn->IsIdleTurnInProgress();
	return false;
}

bool UAZ_AnimInstance::ShouldReEnterTurnInPlace() const
{
	if (!ShouldTurnInPlace()) return false;

	// Only re-enter if spin direction needs to REVERSE (prevents every-frame re-fire).
	// "Spin_L" means currently turning left; if FutureFacingDelta > 45° we overshot and need right.
	// "Spin_R" means currently turning right; if FutureFacingDelta < -45° we overshot and need left.
	const bool bSpinL = BlendStackInputs.Tags.Contains(FName(TEXT("Spin_L"))) && FutureFacingDelta > 45.f;
	const bool bSpinR = BlendStackInputs.Tags.Contains(FName(TEXT("Spin_R"))) && FutureFacingDelta < -45.f;

	// Also allow initial entry when no spin is active yet (Tags empty)
	const bool bNoSpinYet = BlendStackInputs.Tags.Num() == 0;

	return bSpinL || bSpinR || bNoSpinYet;
}

bool UAZ_AnimInstance::ShouldSpinTransition() const
{
	// 180+ degree facing change while moving at speed
	return FMath::Abs(FutureFacingDelta) >= 130.f && Speed2D >= 150.f
		&& !CurrentDatabaseTags.Contains(FName("Pivots"));
}

bool UAZ_AnimInstance::JustLanded_Light() const
{
	return MovementMode == EAZ_MovementMode::OnGround
		&& MovementMode_LastFrame == EAZ_MovementMode::InAir
		&& Velocity_LastFrame.Z > HeavyLandSpeedThreshold;
}

bool UAZ_AnimInstance::JustLanded_Heavy() const
{
	return MovementMode == EAZ_MovementMode::OnGround
		&& MovementMode_LastFrame == EAZ_MovementMode::InAir
		&& Velocity_LastFrame.Z <= HeavyLandSpeedThreshold;
}

float UAZ_AnimInstance::Get_TotalFacingDelta(const TArray<float>& Times) const
{
	if (Trajectory.Samples.Num() <= 0) return 0.f;

	FTransformTrajectorySample FirstSample;
	UPoseSearchTrajectoryLibrary::GetTransformTrajectorySampleAtTime(Trajectory, 0.f, FirstSample, false);
	float PrevYaw = FirstSample.Facing.Rotator().Yaw;

	float TotalDelta = 0.f;
	for (const float T : Times)
	{
		FTransformTrajectorySample S;
		UPoseSearchTrajectoryLibrary::GetTransformTrajectorySampleAtTime(Trajectory, T, S, false);
		const float SampleYaw = S.Facing.Rotator().Yaw;
		TotalDelta += FRotator::NormalizeAxis(SampleYaw - PrevYaw);
		PrevYaw = SampleYaw;
	}
	return TotalDelta;
}

// ========================================
// Phase 9b — Warping / Procedural helpers
// ========================================

namespace
{
	/** Convert generic AnimNodeReference → typed BlendStack node ref. Returns invalid ref on failure. */
	FBlendStackAnimNodeReference ToBlendStackNode(const FAnimNodeReference& Node)
	{
		EAnimNodeReferenceConversionResult Result;
		return UBlendStackAnimNodeLibrary::ConvertToBlendStackNode(Node, Result);
	}

	/** Read (Anim, Time) from a BlendStack node ref. Anim is null if not currently playing a UAnimSequence. */
	void GetBlendStackAnimAndTime(const FAnimNodeReference& Node, UAnimSequence*& OutAnim, float& OutTime)
	{
		const FBlendStackAnimNodeReference BSNode = ToBlendStackNode(Node);
		UAnimationAsset* Asset = UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimAsset(BSNode);
		OutAnim = Cast<UAnimSequence>(Asset);
		OutTime = UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimAssetTime(BSNode);
	}
}

double UAZ_AnimInstance::Get_DynamicPlayRate(FAnimNodeReference BlendStackInput) const
{
	UAnimSequence* Anim = nullptr;
	float AnimTime = 0.f;
	GetBlendStackAnimAndTime(BlendStackInput, Anim, AnimTime);
	if (!Anim) return 1.0;

	float SpeedCurve = 0.f;
	UAnimationWarpingLibrary::GetCurveValueFromAnimation(Anim, FName("MoveData_Speed"), AnimTime, SpeedCurve);

	float AlphaCurve = 0.f;
	const bool bHasAlpha = UAnimationWarpingLibrary::GetCurveValueFromAnimation(Anim, FName("Enable_Warping"), AnimTime, AlphaCurve);
	if (!bHasAlpha) return 1.0;  // GASP: no Enable_Warping curve → no rate warping for this anim

	// Defaults match GASP node defaults (1.25 max, 1.00 min) when curves missing.
	float MaxRate = 1.25f;
	float MinRate = 1.0f;
	UAnimationWarpingLibrary::GetCurveValueFromAnimation(Anim, FName("MaxDynamicPlayRate"), AnimTime, MaxRate);
	UAnimationWarpingLibrary::GetCurveValueFromAnimation(Anim, FName("MinDynamicPlayRate"), AnimTime, MinRate);

	const double SafeRatio = FMath::IsNearlyZero(SpeedCurve) ? 1.0 : (double)Speed2D / (double)SpeedCurve;
	const double ClampedRatio = FMath::Clamp(SafeRatio, (double)MinRate, (double)MaxRate);
	const double WarpedRate = FMath::Lerp(1.0, ClampedRatio, (double)AlphaCurve);

	// Circling bonus — speeds up animation slightly when sustained turning.
	const double AbsAng = FMath::Abs(Trj_CurrentAngularVelocity.Z);
	const double CircularRate  = FMath::GetMappedRangeValueClamped(FVector2D(100.0, 400.0), FVector2D(1.0, 1.2), AbsAng);
	const double CircularAlpha = FMath::GetMappedRangeValueClamped(FVector2D(0.0, 0.5),     FVector2D(0.0, 1.0), Trj_CirclingTime);
	const double CirclingMul   = FMath::Lerp(1.0, CircularRate, CircularAlpha);

	return WarpedRate * CirclingMul;
}

FQuat UAZ_AnimInstance::Get_DesiredFacing(FAnimNodeReference Node) const
{
	UAnimSequence* Anim = nullptr;
	float AnimTime = 0.f;
	GetBlendStackAnimAndTime(Node, Anim, AnimTime);
	if (!Anim) return FQuat::Identity;

	float SteeringCurve = 0.f;
	UAnimationWarpingLibrary::GetCurveValueFromAnimation(Anim, FName("SteeringTargetTime"), AnimTime, SteeringCurve);

	const float T = FMath::GetMappedRangeValueClamped(FVector2D(0.f, 1.f), FVector2D(0.1f, 1.5f), SteeringCurve);

	FTransformTrajectorySample Sample;
	UPoseSearchTrajectoryLibrary::GetTransformTrajectorySampleAtTime(Trajectory, T, Sample, false);
	return Sample.Facing;
}

bool UAZ_AnimInstance::EnableSteering(FAnimNodeReference Node) const
{
	const FBlendStackAnimNodeReference BSNode = ToBlendStackNode(Node);
	const bool bAnimActive = UBlendStackAnimNodeLibrary::GetCurrentBlendStackAnimIsActive(BSNode);
	const bool bRelevant   = BlendStackInputs.bLoop || bAnimActive;

	const bool bMovingOrAirborne =
		IsMoving() ||
		(MovementMode == EAZ_MovementMode::InAir) ||
		(MovementMode == EAZ_MovementMode::Slide);

	return bRelevant && bMovingOrAirborne;
}

uint8 UAZ_AnimInstance::Get_OrientationWarpingWarpingSpace() const
{
	return bOffsetRootBoneEnabled
		? (uint8)EOrientationWarpingSpace::RootBoneTransform
		: (uint8)EOrientationWarpingSpace::ComponentTransform;
}

FVector UAZ_AnimInstance::Get_StrafeWarpDirection() const
{
	const float Alpha = FMath::GetMappedRangeValueClamped(
		FVector2D(20.f, 100.f),
		FVector2D(0.f, 1.f),
		FMath::Abs(Trj_CurrentAngularVelocity.Z));
	return FMath::Lerp(LastNonZeroVelocity, Trj_NearFutureVelocity, (double)Alpha);
}

double UAZ_AnimInstance::Get_StrideWarpAlpha(FAnimNodeReference Node) const
{
	UAnimSequence* Anim = nullptr;
	float AnimTime = 0.f;
	GetBlendStackAnimAndTime(Node, Anim, AnimTime);
	if (!Anim) return 0.0;

	float Warp = 0.f, Stride = 0.f;
	UAnimationWarpingLibrary::GetCurveValueFromAnimation(Anim, FName("Enable_Warping"),     AnimTime, Warp);
	// Note: GASP curve name has typo "Stide" (not "Stride") — matched verbatim for parity.
	UAnimationWarpingLibrary::GetCurveValueFromAnimation(Anim, FName("Enable_StideWarping"), AnimTime, Stride);
	return FMath::Clamp((double)Warp + (double)Stride, 0.0, 1.0);
}

double UAZ_AnimInstance::Get_StrafeWarpAlpha(FAnimNodeReference Node) const
{
	UAnimSequence* Anim = nullptr;
	float AnimTime = 0.f;
	GetBlendStackAnimAndTime(Node, Anim, AnimTime);
	if (!Anim) return 0.0;

	float Warp = 0.f, Strafe = 0.f;
	UAnimationWarpingLibrary::GetCurveValueFromAnimation(Anim, FName("Enable_Warping"),       AnimTime, Warp);
	UAnimationWarpingLibrary::GetCurveValueFromAnimation(Anim, FName("Enable_StrafeWarping"), AnimTime, Strafe);
	return FMath::Clamp((double)Warp + (double)Strafe, 0.0, 1.0);
}

double UAZ_AnimInstance::Get_ProceduralTargetTime(FAnimNodeReference Node) const
{
	UAnimSequence* Anim = nullptr;
	float AnimTime = 0.f;
	GetBlendStackAnimAndTime(Node, Anim, AnimTime);
	if (!Anim) return 0.4;  // GASP default when cast fails

	float SteeringTime = 0.f;
	UAnimationWarpingLibrary::GetCurveValueFromAnimation(Anim, FName("SteeringTargetTime"), AnimTime, SteeringTime);
	return FMath::GetMappedRangeValueClamped(FVector2D(0.0, 1.0), FVector2D(0.1, 0.3), SteeringTime);
}

void UAZ_AnimInstance::Biped_FootPlacement_OnBecomeRelevant(FAnimUpdateContext Context, FAnimNodeReference Node)
{
	bForceFootPlacementReset = true;
}

// ========================================
// Phase 9c — Motion Matching node callbacks
// ========================================

void UAZ_AnimInstance::Update_MotionMatching(FAnimUpdateContext Context, FAnimNodeReference Node)
{
	EAnimNodeReferenceConversionResult ConvResult;
	const FMotionMatchingAnimNodeReference MMNode =
		UMotionMatchingAnimNodeLibrary::ConvertToMotionMatchingNode(Node, ConvResult);
	if (ConvResult != EAnimNodeReferenceConversionResult::Succeeded) return;

	// Evaluate Chooser → ValidDatabases (mirrors GASP K2Node_EvaluateChooser2 with self-as-context).
	if (LocomotionDatabaseChooser)
	{
		const TArray<UObject*> ChosenObjects = UChooserFunctionLibrary::EvaluateChooserMulti(
			this, LocomotionDatabaseChooser, UPoseSearchDatabase::StaticClass());

		ValidDatabases.Reset(ChosenObjects.Num());
		for (UObject* Obj : ChosenObjects)
		{
			if (UPoseSearchDatabase* DB = Cast<UPoseSearchDatabase>(Obj))
			{
				ValidDatabases.Add(DB);
			}
		}
	}

	// Marshal TArray<TObjectPtr<UPoseSearchDatabase>> → TArray<UPoseSearchDatabase*> for the API.
	TArray<UPoseSearchDatabase*> RawDBs;
	RawDBs.Reserve(ValidDatabases.Num());
	for (const TObjectPtr<UPoseSearchDatabase>& DB : ValidDatabases)
	{
		RawDBs.Add(DB.Get());
	}

	UMotionMatchingAnimNodeLibrary::SetDatabasesToSearch(
		MMNode, RawDBs, (EPoseSearchInterruptMode)Get_MMInterruptMode());
}

void UAZ_AnimInstance::Update_MotionMatching_PostSelection(FAnimUpdateContext Context, FAnimNodeReference Node)
{
	EAnimNodeReferenceConversionResult ConvResult;
	const FMotionMatchingAnimNodeReference MMNode =
		UMotionMatchingAnimNodeLibrary::ConvertToMotionMatchingNode(Node, ConvResult);
	if (ConvResult != EAnimNodeReferenceConversionResult::Succeeded) return;

	FPoseSearchBlueprintResult SearchResult;
	bool bResultValid = false;
	UMotionMatchingAnimNodeLibrary::GetMotionMatchingSearchResult(MMNode, SearchResult, bResultValid);

	CurrentSelectedAnim     = SearchResult.SelectedAnim;
	CurrentSelectedDatabase = const_cast<UPoseSearchDatabase*>(SearchResult.SelectedDatabase.Get());
	MMSearchCost            = SearchResult.SearchCost;

	if (CurrentSelectedDatabase)
	{
		CurrentDatabaseTags.Reset();
		UPoseSearchLibrary::GetDatabaseTags(CurrentSelectedDatabase, CurrentDatabaseTags);
	}

	// Override the upcoming blend: HermiteCubic, 0.2s, no inertial blend (GASP defaults).
	// FMotionMatchingBlueprintBlendSettings's default ctor is declared in the PoseSearch
	// public header but NOT marked POSESEARCH_API, so its symbol isn't exported. We can't
	// construct it directly. Workaround: zero-init aligned storage and reinterpret —
	// equivalent to the engine's default ctor (all fields default to 0/null/false).
	alignas(FMotionMatchingBlueprintBlendSettings) uint8 SettingsStorage[sizeof(FMotionMatchingBlueprintBlendSettings)];
	FMemory::Memzero(SettingsStorage, sizeof(SettingsStorage));
	FMotionMatchingBlueprintBlendSettings& BlendSettings =
		*reinterpret_cast<FMotionMatchingBlueprintBlendSettings*>(SettingsStorage);
	BlendSettings.BlendTime         = 0.2f;
	BlendSettings.BlendOption       = EAlphaBlendOption::HermiteCubic;
	BlendSettings.bUseInertialBlend = false;

	bool bOverrideValid = false;
	UMotionMatchingAnimNodeLibrary::OverrideMotionMatchingBlendSettings(MMNode, BlendSettings, bOverrideValid);
}

// ========================================
// Phase 9d — SM State Controller
// ========================================

void UAZ_AnimInstance::SetBlendStackAnimFromChooser(
	EAZ_StateMachineState State,
	bool bForceBlend,
	FAnimNodeReference BlendStackNode,
	UAnimationAsset* ChosenAnim,
	FAZ_ChooserOutputs ChooserOut)
{
	// Mirror GASP: state + previous-cache + reset transition bools.
	StateMachineState              = State;
	Previous_BlendStackInputs      = BlendStackInputs;
	bNotifyTransition_ReTransition = false;
	bNotifyTransition_ToLoop       = false;
	bNoValidAnim                   = false;

	// ABP supplies the chosen anim from CHT_MoverCharacterAnimations + the chooser output struct.
	ValidAnims.Reset();
	if (ChosenAnim) ValidAnims.Add(ChosenAnim);
	ChooserOutputs = ChooserOut;

	if (ValidAnims.Num() == 0)
	{
		bNoValidAnim = true;
		return;
	}

	if (ChooserOut.bUseMM)
	{
		// Single-frame MotionMatch over the chooser's valid anims; pick best by current pose+trajectory.
		TArray<UObject*> AssetsToSearch;
		AssetsToSearch.Reserve(ValidAnims.Num());
		for (UAnimationAsset* A : ValidAnims) AssetsToSearch.Add(A);

		FPoseSearchBlueprintResult MMResult;
		UPoseSearchLibrary::MotionMatch(
			this, AssetsToSearch, FName("PoseHistory"),
			FPoseSearchContinuingProperties(), FPoseSearchFutureProperties(),
			MMResult);

		UAnimationAsset* MMAnim = Cast<UAnimationAsset>(MMResult.SelectedAnim);
		if (!MMAnim)
		{
			bNoValidAnim = true;
			return;
		}

		// MMCostLimit: when > 0, MM result must beat that cost or we declare no valid anim.
		const bool bCostFail = (ChooserOut.MMCostLimit > 0.0) && (MMResult.SearchCost > ChooserOut.MMCostLimit);
		if (bCostFail)
		{
			bNoValidAnim = true;
			return;
		}

		bool bAssetLooping = false;
		UPoseSearchLibrary::IsAnimationAssetLooping(MMAnim, bAssetLooping);

		BlendStackInputs.Anim         = MMAnim;
		BlendStackInputs.bLoop        = bAssetLooping;
		BlendStackInputs.StartTime    = MMResult.SelectedTime;
		BlendStackInputs.BlendTime    = ChooserOut.BlendTime;
		BlendStackInputs.BlendProfile = const_cast<UBlendProfile*>(GetBlendProfileByName(ChooserOut.BlendProfile));
		BlendStackInputs.Tags         = ChooserOut.Tags;
		SearchCost                    = MMResult.SearchCost;
	}
	else
	{
		// Non-MM path: take the first valid anim and chooser-supplied timing/profile/tags.
		UAnimationAsset* FirstAnim = ValidAnims[0];
		bool bAssetLooping = false;
		UPoseSearchLibrary::IsAnimationAssetLooping(FirstAnim, bAssetLooping);

		BlendStackInputs.Anim         = FirstAnim;
		BlendStackInputs.bLoop        = bAssetLooping;
		BlendStackInputs.StartTime    = ChooserOut.StartTime;
		BlendStackInputs.BlendTime    = ChooserOut.BlendTime;
		BlendStackInputs.BlendProfile = const_cast<UBlendProfile*>(GetBlendProfileByName(ChooserOut.BlendProfile));
		BlendStackInputs.Tags         = ChooserOut.Tags;
	}

	// Force a fresh blend if the caller requested it AND we're not playing the same loop
	// (re-entering the same loop should not re-blend — GASP comment about pivot-failure case).
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

bool UAZ_AnimInstance::IsAnimationAlmostComplete(FAnimNodeReference BlendStackNode) const
{
	EAnimNodeReferenceConversionResult ConvResult;
	const FBlendStackAnimNodeReference BSNode =
		UBlendStackAnimNodeLibrary::ConvertToBlendStackNode(BlendStackNode, ConvResult);
	if (ConvResult != EAnimNodeReferenceConversionResult::Succeeded) return false;

	const bool  bLooping      = UBlendStackAnimNodeLibrary::IsCurrentAssetLooping(BSNode);
	const float TimeRemaining = UBlendStackAnimNodeLibrary::GetCurrentAssetTimeRemaining(BSNode);
	return !bLooping && TimeRemaining <= 0.75f;
}

bool UAZ_AnimInstance::JustTraversed() const
{
	// GASP: (MovingTraversal curve > 1) AND (DefaultSlot not driving pose).
	// Traversal montages set the curve while playing on a non-DefaultSlot; the slot
	// check prevents falsing during non-traversal montages that happen to use the curve.
	const bool bMovingTraversal = GetCurveValue(FName("MovingTraversal")) > 1.f;
	const bool bDefaultSlotInactive = const_cast<UAZ_AnimInstance*>(this)->GetSlotMontageLocalWeight(FName("DefaultSlot")) <= UE_KINDA_SMALL_NUMBER;
	return bMovingTraversal && bDefaultSlotInactive;
}

// ========================================
// ANIMGRAPH BINDINGS
// ========================================

uint8 UAZ_AnimInstance::Get_MMInterruptMode() const
{
	// 0=DoNotInterrupt, 1=InterruptOnDatabaseChange, 2=ForceInterrupt
	if (MovementModeChanged()) return 1;
	if (MovementStateChanged()) return 1;
	if (GaitChanged() && MovementState == EAZ_MovementState::Moving && IsOnGround()) return 1;
	if (StanceChanged() && MovementState == EAZ_MovementState::Moving && IsOnGround()) return 1;
	if (DirectionChanged() && MovementState == EAZ_MovementState::Moving) return 1;
	return 0; // DoNotInterrupt
}

float UAZ_AnimInstance::Get_MMBlendTime() const
{
	if (MovementMode == EAZ_MovementMode::InAir)
	{
		return (Velocity.Z > 100.f) ? 0.15f : 0.5f; // Just jumped vs falling
	}
	// On ground
	if (JustLanded_Light() || JustLanded_Heavy()) return 0.5f;
	return 0.2f;
}

uint8 UAZ_AnimInstance::Get_OffsetRootTranslationMode() const
{
	// EOffsetRootBoneMode: 0=Accumulate, 1=Interpolate, 2=LockOffsetAndConsumeAnimation,
	//                     3=LockOffsetIncreaseAndConsumeAnimation, 4=LockOffsetAndIgnoreAnimation, 5=Release
	if (MovementMode == EAZ_MovementMode::InAir) return 5; // Release
	if (MovementState == EAZ_MovementState::Moving) return 1; // Interpolate (GASP default while moving)
	return 5; // Release when idle (blend offset back out so mesh re-aligns with capsule)
}

uint8 UAZ_AnimInstance::Get_OffsetRootRotationMode() const
{
	// EOffsetRootBoneMode values same as above.
	return 0; // Accumulate (GASP default — mesh rotation counters capsule rotation for smoother visual turn)
}

float UAZ_AnimInstance::Get_OffsetRootTranslationHalfLife() const
{
	if (MovementState == EAZ_MovementState::Idle) return OffsetRootHalfLife_Idle;
	if (Gait == EAZ_Gait::Sprint) return OffsetRootHalfLife_Sprint;
	return OffsetRootHalfLife_Walk;
}

float UAZ_AnimInstance::Get_OffsetRootTranslationRadius() const
{
	return OffsetRootTranslationRadius;
}

bool UAZ_AnimInstance::AllowFootPinning() const
{
	return MovementMode == EAZ_MovementMode::OnGround && bFootPlacementEnabled;
}

bool UAZ_AnimInstance::AllowSlopeWarping() const
{
	return MovementMode == EAZ_MovementMode::OnGround && bFootPlacementEnabled;
}

bool UAZ_AnimInstance::JustTeleported() const
{
	const float DistSq = FVector::DistSquared(CharacterTransform.GetLocation(), CharacterTransform_LastFrame.GetLocation());
	return DistSq > FMath::Square(200.f);
}

FRotator UAZ_AnimInstance::Get_SlideSlopeRotation() const
{
	// Mirrors GASP SandboxCharacter_Mover_ABP::Get_SlideSlopeRotation
	const FRotator CharRot = CharacterTransform.Rotator();
	const FVector RightAxis = UKismetMathLibrary::GetRightVector(CharRot);
	const FVector UpAxis = UKismetMathLibrary::GetUpVector(CharRot);

	float OutPitch = 0.f;
	float OutRoll = 0.f;
	UKismetMathLibrary::GetSlopeDegreeAngles(RightAxis, SmoothedGroundNormal, UpAxis, OutPitch, OutRoll);

	// GASP negates both angles before building the rotator (Yaw stays 0)
	return FRotator(-OutPitch, 0.f, -OutRoll);
}

FVector UAZ_AnimInstance::Get_SlideSlopeOffset() const
{
	// Mirrors GASP SandboxCharacter_Mover_ABP::Get_SlideSlopeOffset
	const FVector RootLoc = RootTransform.GetLocation();
	const FVector ProjectedPoint = FVector::PointPlaneProject(RootLoc, CharacterProperties.GroundLocation, SmoothedGroundNormal);
	return ProjectedPoint - RootLoc;
}
