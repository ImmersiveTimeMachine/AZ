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
}

void UAZ_AnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// --- Lazy init: retry if NativeInitializeAnimation ran before pawn was ready ---
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
	}

	// --- Read movement data from either Mover proxy (new) or CMC (old) ---
	// Note: Velocity is now a class member, not local

	if (OwningHeroPawn)
	{
		// New Mover pawn: read thread-safe proxy
		const FAZ_MoverStateProxy& MoverState = OwningHeroPawn->GetMoverStateSafe();
		Velocity = MoverState.Velocity;
		GroundSpeed = MoverState.GroundSpeed;


		const bool bCurrentlyFalling = MoverState.bIsFalling;
		if (bWasFalling && !bCurrentlyFalling)
		{
			bIsJumping = false;
		}
		bIsFalling = bCurrentlyFalling;
		bWasFalling = bCurrentlyFalling;

		bIsCrouching = MoverState.bIsCrouching;
		bIsAiming = MoverState.bIsAiming;

		// Generate trajectory via Mover predictor (15 history + 15 future samples
		// with proper deceleration prediction). GASP path — replaces the linear
		// MeshLocation+Velocity*Time loop, which can't predict stops.
		if (UMoverTrajectoryPredictor* Predictor = OwningHeroPawn->MoverTrajectoryPredictor)
		{
			TScriptInterface<IPoseSearchTrajectoryPredictorInterface> PredictorInterface;
			PredictorInterface.SetObject(Predictor);
			PredictorInterface.SetInterface(Cast<IPoseSearchTrajectoryPredictorInterface>(Predictor));

			UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectoryWithPredictor(
				PredictorInterface,
				DeltaSeconds,
				CharacterTrajectory,
				PreviousDesiredControllerYaw,
				CharacterTrajectory,
				/*HistorySamplingInterval*/ 0.033f,
				/*HistoryCount*/ 15,
				/*PredictionSamplingInterval*/ 0.1f,
				/*PredictionCount*/ 15);
		}
	}
	else if (OwningCharacter && MovementComponent)
	{
		// Old ACharacter: read from CMC
		Velocity = MovementComponent->Velocity;
		GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.f).Size();

		const bool bCurrentlyFalling = MovementComponent->IsFalling();
		if (bWasFalling && !bCurrentlyFalling)
		{
			bIsJumping = false;
		}
		bIsFalling = bCurrentlyFalling;
		bWasFalling = bCurrentlyFalling;

		bIsCrouching = MovementComponent->IsCrouching();

		// Trajectory (old character)
		if (const AAZ_HeroCharacter* Hero = Cast<AAZ_HeroCharacter>(OwningCharacter))
		{
			if (UCharacterTrajectoryComponent* TrajComp = Hero->CharacterTrajectory)
			{
				FProperty* Prop = TrajComp->GetClass()->FindPropertyByName(TEXT("Trajectory"));
				if (Prop)
				{
					Prop->CopyCompleteValue(&CharacterTrajectory, Prop->ContainerPtrToValuePtr<void>(TrajComp));
				}
			}
		}
	}
	else
	{
		return;
	}

	// --- Update_EssentialValues (GASP parity) ---
	// Save last-frame snapshots BEFORE updating
	Velocity_LastFrame = Velocity;
	Acceleration_LastFrame = Acceleration;
	CharacterTransform_LastFrame = CharacterTransform;
	FutureFacingDelta_LastFrame = FutureFacingDelta;
	Trj_PreviousFutureVelocity = Trj_FutureVelocity;

	AActor* OwnerActor = OwningHeroPawn ? static_cast<AActor*>(OwningHeroPawn.Get()) : static_cast<AActor*>(OwningCharacter.Get());

	{
		// For HeroPawn (Mover), Velocity was set from MoverStateProxy above.
		// For old Character, read from actor (GetVelocity() returns zero for Mover pawns).
		if (!OwningHeroPawn)
		{
			Velocity = OwnerActor ? OwnerActor->GetVelocity() : FVector::ZeroVector;
		}
		Speed2D = FVector(Velocity.X, Velocity.Y, 0.f).Size();
		bHasVelocity = Speed2D > 10.f;
		if (bHasVelocity) LastNonZeroVelocity = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();

		if (DeltaSeconds > UE_KINDA_SMALL_NUMBER)
		{
			VelocityAcceleration = (Velocity - Velocity_LastFrame) / DeltaSeconds;
		}

		// Acceleration mirrors GASP: it's the player's INPUT INTENT (rotated to world),
		// not d(velocity)/dt. The derivative goes to ~0 during steady-state motion (V_n ≈ V_n-1
		// at MaxSpeed) which would flip MovementState to Idle while W is still held.
		// Using intent keeps bAccelerating=true the whole time the player is pressing input.
		// VelocityAcceleration (the derivative) is kept separately for Update_AdditiveLean.
		if (OwningHeroPawn)
		{
			const FVector LocalIntent = OwningHeroPawn->GetCachedMoveInputIntent();
			if (LocalIntent.SizeSquared() > UE_KINDA_SMALL_NUMBER)
			{
				const AController* Controller = OwningHeroPawn->GetController();
				const FRotator ControlRot = Controller ? Controller->GetControlRotation() : FRotator::ZeroRotator;
				const FVector WorldIntent = ControlRot.RotateVector(LocalIntent);
				// Pull the actual configured max acceleration from the Mover's shared settings
				// so RelativeAcceleration matches the real units the Mover applies each tick.
				// Falls back to 4000 (the engine default) if settings aren't reachable.
				float MaxAccel = 4000.f;
				if (const UCharacterMoverComponent* Mover = OwningHeroPawn->GetMoverComponent())
				{
					if (const UCommonLegacyMovementSettings* Settings = Mover->FindSharedSettings<UCommonLegacyMovementSettings>())
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
		bHasAcceleration = AccelerationAmount > 10.f;

		if (OwnerActor)
		{
			CharacterTransform = OwnerActor->GetActorTransform();
			RelativeAcceleration = CharacterTransform.InverseTransformVectorNoScale(Acceleration);
		}
	}

	// --- Character Properties for Procedural systems ---
	{
		CharacterProperties.MovementDirection = MovementDirection;

		if (OwningHeroPawn)
		{
			const FAZ_MoverStateProxy& MoverState = OwningHeroPawn->GetMoverStateSafe();
			CharacterProperties.GroundNormal = MoverState.GroundNormal;
			CharacterProperties.GroundLocation = MoverState.GroundLocation;
		}
		else if (MovementComponent)
		{
			const FFindFloorResult& Floor = MovementComponent->CurrentFloor;
			if (Floor.bBlockingHit)
			{
				CharacterProperties.GroundNormal = Floor.HitResult.ImpactNormal;
				CharacterProperties.GroundLocation = Floor.HitResult.ImpactPoint;
			}
			else
			{
				CharacterProperties.GroundNormal = FVector::UpVector;
				CharacterProperties.GroundLocation = FVector::ZeroVector;
			}
		}

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

		if (bForceFootPlacementReset)
		{
			bForceFootPlacementReset = false;
		}
	}

	// --- Update_Trajectory (GASP parity) — derive Trj_* values from CharacterTrajectory ---
	// CharacterTrajectory was generated above (HeroPawn path uses Mover predictor with
	// 15+15 samples and proper deceleration prediction; old character path uses
	// CharacterTrajectoryComponent). All derived values pull from the trajectory via
	// PoseSearchTrajectoryLibrary helpers — the same APIs GASP calls in BP.
	if (CharacterTrajectory.Samples.Num() > 0)
	{
		// Future velocities at GASP-exact time windows.
		UPoseSearchTrajectoryLibrary::GetTransformTrajectoryVelocity(CharacterTrajectory,  0.4f,  0.5f, Trj_FutureVelocity,     false);
		UPoseSearchTrajectoryLibrary::GetTransformTrajectoryVelocity(CharacterTrajectory,  0.1f,  0.2f, Trj_NearFutureVelocity, false);
		UPoseSearchTrajectoryLibrary::GetTransformTrajectoryVelocity(CharacterTrajectory, -0.3f, -0.2f, Trj_PastVelocity,       false);

		// Predicted facing 1.5s ahead.
		FTransformTrajectorySample FutureSample;
		UPoseSearchTrajectoryLibrary::GetTransformTrajectorySampleAtTime(CharacterTrajectory, 1.5f, FutureSample, false);
		Trj_FutureFacing = FutureSample.Facing.Rotator();

		// Cumulative facing delta across multiple sample points (GASP Get_TotalFacingDelta).
		// Sums CONSECUTIVE signed yaw deltas — telescopes to (last − first) but keeps
		// NormalizeAxis on each step so multi-turn rotations (>180°) don't wrap.
		// CRITICAL: initialize PrevYaw from the first trajectory sample, NOT from
		// actor yaw. Trajectory samples are in mesh-component space, which carries
		// the SK_SurvivalMan-baked -90° offset from actor forward. Comparing
		// sample[0] to actor yaw would inject that offset as a permanent -90° bias
		// into FutureFacingDelta, falsely triggering ShouldTurnInPlace forever.
		{
			const float SampleTimes[] = { 0.f, 0.25f, 0.75f, 1.5f };
			float TotalDelta = 0.f;
			FTransformTrajectorySample FirstSample;
			UPoseSearchTrajectoryLibrary::GetTransformTrajectorySampleAtTime(CharacterTrajectory, 0.f, FirstSample, false);
			float PrevYaw = FirstSample.Facing.Rotator().Yaw;
			for (const float T : SampleTimes)
			{
				FTransformTrajectorySample S;
				UPoseSearchTrajectoryLibrary::GetTransformTrajectorySampleAtTime(CharacterTrajectory, T, S, false);
				const float SampleYaw = S.Facing.Rotator().Yaw;
				TotalDelta += FRotator::NormalizeAxis(SampleYaw - PrevYaw);
				PrevYaw = SampleYaw;
			}
			FutureFacingDelta = TotalDelta;
		}

		// Angular velocities (Z component is yaw rate; circling test uses |Z|).
		FVector PastAngVel = FVector::ZeroVector;
		FVector CurAngVel  = FVector::ZeroVector;
		UPoseSearchTrajectoryLibrary::GetTransformTrajectoryAngularVelocity(CharacterTrajectory, -0.3f, -0.2f, PastAngVel, false);
		UPoseSearchTrajectoryLibrary::GetTransformTrajectoryAngularVelocity(CharacterTrajectory,  0.0f,  0.1f, CurAngVel,  false);
		Trj_PastAngularVelocity    = PastAngVel.Z;
		Trj_CurrentAngularVelocity = CurAngVel.Z;
		Trj_TurnAngle = FutureFacingDelta;

		// IsCircling: sustained turn (past + current both above 200 deg/s) — matches GASP.
		Trj_IsCircling = FMath::Abs(PastAngVel.Z) > 200.f && FMath::Abs(CurAngVel.Z) > 200.f;
		Trj_CirclingTime = Trj_IsCircling ? (Trj_CirclingTime + DeltaSeconds) : 0.f;
	}

	// --- Update_States (GASP parity) — snapshot LastFrame, compute new, run state tracking ---
	MovementState_LastFrame     = MovementState;
	MovementMode_LastFrame      = MovementMode;
	Gait_LastFrame              = Gait;
	Stance_LastFrame            = Stance;
	MovementDirection_LastFrame = MovementDirection;
	RotationMode_LastFrame      = RotationMode;

	// MovementMode + Stance — direct from movement bits.
	MovementMode = bIsFalling ? EAZ_MovementMode::InAir : EAZ_MovementMode::OnGround;
	Stance       = bIsCrouching ? EAZ_Stance::Crouching : EAZ_Stance::Standing;

	// MovementState — GASP IsMoving: future velocity != 0 (tol 10) AND acceleration != 0.
	// Fires Idle IMMEDIATELY on input release while Speed2D may still be high — this
	// gives Walk Stops chooser rows (speed 10..200 + SM=TransIdle) a chance to match.
	{
		const FVector FutureVel2D(Trj_FutureVelocity.X, Trj_FutureVelocity.Y, 0.f);
		const FVector Accel2D(Acceleration.X, Acceleration.Y, 0.f);
		const bool bFutureMoving = FutureVel2D.SizeSquared() > 100.f;  // tol 10 → 10^2
		const bool bAccelerating = Accel2D.SizeSquared() > 1.f;
		MovementState = (bFutureMoving && bAccelerating)
			? EAZ_MovementState::Moving
			: EAZ_MovementState::Idle;
	}

	// Gait — speed-banded.
	if (Speed2D < 200.f)      Gait = EAZ_Gait::Walk;
	else if (Speed2D < 500.f) Gait = EAZ_Gait::Run;
	else                      Gait = EAZ_Gait::Sprint;

	// MovementDirection — F/B/LL/LR/RL/RR. Side moves default LL/RR until foot phase tracked.
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

	// Apply 5-variable tracking pattern (X_Recent, X_RecentTimer, X_Time, X_LastStateTime).
	// RecentTimeLimit per GASP: MovementMode 0.2s, others 0.1s.
	UpdateStateTracking(MovementState_Recent,     MovementState_RecentTimer,     MovementState_Time,     MovementState_LastStateTime,     MovementState,     MovementState_LastFrame,     DeltaSeconds, 0.1f);
	UpdateStateTracking(MovementMode_Recent,      MovementMode_RecentTimer,      MovementMode_Time,      MovementMode_LastStateTime,      MovementMode,      MovementMode_LastFrame,      DeltaSeconds, 0.2f);
	UpdateStateTracking(Gait_Recent,              Gait_RecentTimer,              Gait_Time,              Gait_LastStateTime,              Gait,              Gait_LastFrame,              DeltaSeconds, 0.1f);
	UpdateStateTracking(Stance_Recent,            Stance_RecentTimer,            Stance_Time,            Stance_LastStateTime,            Stance,            Stance_LastFrame,            DeltaSeconds, 0.1f);
	UpdateStateTracking(MovementDirection_Recent, MovementDirection_RecentTimer, MovementDirection_Time, MovementDirection_LastStateTime, MovementDirection, MovementDirection_LastFrame, DeltaSeconds, 0.1f);
	UpdateStateTracking(RotationMode_Recent,      RotationMode_RecentTimer,      RotationMode_Time,      RotationMode_LastStateTime,      RotationMode,      RotationMode_LastFrame,      DeltaSeconds, 0.1f);

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
		const FString TagsStr = BlendStackInputs.Tags.IsNone() ? TEXT("") : BlendStackInputs.Tags.ToString();
		static const TCHAR* SMNames[] = {
			TEXT("IdleLp"), TEXT("TransIdle"), TEXT("LocoLp"), TEXT("TransLoco"),
			TEXT("AirLp"), TEXT("TransAir"), TEXT("IdleBrk"), TEXT("TransSlide"), TEXT("SlideLp")
		};
		const int32 SMIdx = FMath::Clamp((int32)StateMachineState, 0, 8);

		const FString ChooserAnimName = ChooserOutputs.Tags.IsNone() ? TEXT("") : ChooserOutputs.Tags.ToString();
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
	const bool bDatabaseActuallyChanged = (CurrentLocomotionDatabase != PreviousLocomotionDatabase);
	PreviousLocomotionDatabase = CurrentLocomotionDatabase;
	bLocomotionDatabaseChanged = bDatabaseActuallyChanged;

	// --- Update_AdditiveLean (mirrors GASP SandboxCharacter_Mover_ABP::Update_AdditiveLean) ---
	{
		// Convert VelocityAcceleration into the velocity-aligned local space, take Y (lateral)
		const FRotator VelocityRot = UKismetMathLibrary::Conv_VectorToRotator(Velocity);
		const FVector LocalAccel = VelocityRot.UnrotateVector(VelocityAcceleration);

		// Map current 2D speed (200..320) to a divisor (500..800), then normalize lateral accel into -1..1
		const float Divisor = FMath::GetMappedRangeValueClamped(FVector2D(200.f, 320.f), FVector2D(500.f, 800.f), Speed2D);
		LateralAccelerationAmount = FMath::Clamp(LocalAccel.Y / Divisor, -1.f, 1.f);

		// Pack into a Vector2D suited for the lean blendspace based on movement direction.
		// F/B use X axis lean, side moves (LL/LR/RL/RR) use Y axis lean with sign per side.
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

	// --- Update_AimOffset (mirrors GASP SandboxCharacter_Mover_ABP::Update_AimOffset) ---
	// Smooth a worldspace aim target, then compute AO as delta from root rotation.
	// Gate AO enable on rotation mode, montage slot weight, and angle thresholds.
	{
		AO_AimTarget = CharacterProperties.AimingRotation;

		// Critical spring damp SmoothedAimTarget toward AO_AimTarget
		UBlueprintSpringMathLibrary::CriticalSpringDampRotator(
			SmoothedAimTarget, InOutAngularVelocity, AO_AimTarget, DeltaSeconds, AimTargetSmoothingTime);

		// Save previous AO, compute new AO = (SmoothedAimTarget - RootRotation) → Vector2D(Yaw, Pitch)
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
		const bool bYawOK = FMath::Abs(DeltaRaw.Yaw) <= MaxYaw;

		const bool bModeOK = (RotationMode == EAZ_RotationMode::Strafe) || (RotationMode == EAZ_RotationMode::Aiming);
		const bool bSlotOK = GetSlotMontageLocalWeight(FName("DefaultSlot")) < 0.5f;
		const bool bDeltaOK = FMath::Abs(AO.X - Previous_AO.X) < 135.f;

		EnableAO = bYawOK && bModeOK && bSlotOK && bDeltaOK;

		// If AO not enabled this frame, snap SmoothedAimTarget to current AO_AimTarget
		// so next frame's smoothing starts fresh (prevents pull-back jitter).
		if (!EnableAO)
		{
			SmoothedAimTarget = AO_AimTarget;
		}
	}
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
	const bool bSpinL = (BlendStackInputs.Tags == FName(TEXT("Spin_L"))) && FutureFacingDelta > 45.f;
	const bool bSpinR = (BlendStackInputs.Tags == FName(TEXT("Spin_R"))) && FutureFacingDelta < -45.f;

	// Also allow initial entry when no spin is active yet (Tags empty)
	const bool bNoSpinYet = BlendStackInputs.Tags == NAME_None || BlendStackInputs.Tags.IsNone();

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
