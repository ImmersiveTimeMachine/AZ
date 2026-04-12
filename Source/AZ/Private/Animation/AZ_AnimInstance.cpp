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
#include "CharacterTrajectoryComponent.h"
#include "MoverPoseSearchTrajectoryPredictor.h"

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

		// Build trajectory using mesh position + velocity
		if (USkeletalMeshComponent* Mesh = OwningHeroPawn->GetMainMesh())
		{
			const int32 NumHistory = 3;
			const int32 NumFuture = 3;
			const int32 TotalSamples = NumHistory + 1 + NumFuture;
			const float Interval = 0.2f;

			CharacterTrajectory.Samples.SetNum(TotalSamples);

			// Use mesh component transform — PoseSearch indexes relative to mesh facing
			const FVector MeshLocation = Mesh->GetComponentLocation();
			const FQuat MeshFacing = Mesh->GetComponentQuat();

			for (int32 i = 0; i < TotalSamples; ++i)
			{
				const float Time = (i - NumHistory) * Interval;
				FTransformTrajectorySample& Sample = CharacterTrajectory.Samples[i];
				Sample.TimeInSeconds = Time;
				Sample.Position = MeshLocation + Velocity * Time;
				Sample.Facing = MeshFacing;
			}
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

	// --- Essential Values (GASP pattern) ---
	// Save last frame BEFORE updating current
	Velocity_LastFrame = Velocity;
	Acceleration_LastFrame = Acceleration;
	CharacterTransform_LastFrame = CharacterTransform;
	FutureFacingDelta_LastFrame = FutureFacingDelta;
	Trj_PreviousFutureVelocity = Trj_FutureVelocity;

	// Update current essential values
	{
		AActor* Owner = OwningHeroPawn ? static_cast<AActor*>(OwningHeroPawn.Get()) : static_cast<AActor*>(OwningCharacter.Get());
		// For HeroPawn (Mover), Velocity was already set from MoverStateProxy above.
		// For old Character, read from actor. Don't override Mover velocity with GetVelocity() (returns zero for Mover pawns).
		if (!OwningHeroPawn)
		{
			Velocity = Owner ? Owner->GetVelocity() : FVector::ZeroVector;
		}
		Speed2D = FVector(Velocity.X, Velocity.Y, 0.f).Size();
		bHasVelocity = Speed2D > 10.f;
		if (bHasVelocity) LastNonZeroVelocity = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();

		// Acceleration = velocity change rate
		if (DeltaSeconds > UE_KINDA_SMALL_NUMBER)
		{
			VelocityAcceleration = (Velocity - Velocity_LastFrame) / DeltaSeconds;
		}
		Acceleration = VelocityAcceleration;
		AccelerationAmount = Acceleration.Size();
		bHasAcceleration = AccelerationAmount > 10.f;

		// Relative acceleration (in character local space)
		if (Owner)
		{
			CharacterTransform = Owner->GetActorTransform();
			RelativeAcceleration = CharacterTransform.InverseTransformVectorNoScale(Acceleration);
		}
	}

	// --- State tracking (GASP pattern) ---
	// Save ALL state _LastFrame values BEFORE updating
	MovementState_LastFrame = MovementState;
	MovementMode_LastFrame = MovementMode;
	Gait_LastFrame = Gait;
	Stance_LastFrame = Stance;
	MovementDirection_LastFrame = MovementDirection;
	RotationMode_LastFrame = RotationMode;

	// Update current states
	MovementMode = bIsFalling ? EAZ_MovementMode::InAir : EAZ_MovementMode::OnGround;
	MovementState = bHasVelocity ? EAZ_MovementState::Moving : EAZ_MovementState::Idle;
	Stance = bIsCrouching ? EAZ_Stance::Crouching : EAZ_Stance::Standing;

	// --- Character Properties for Procedural systems (foot IK, aim offset, etc.) ---
	{
		AActor* Owner = OwningHeroPawn ? static_cast<AActor*>(OwningHeroPawn.Get()) : static_cast<AActor*>(OwningCharacter.Get());

		CharacterProperties.MovementDirection = MovementDirection;

		// Ground normal from Mover proxy or CMC floor hit
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

		// Aiming rotation — from controller aim (camera direction)
		if (Owner)
		{
			if (const APawn* Pawn = Cast<APawn>(Owner))
			{
				if (const AController* PC = Pawn->GetController())
				{
					CharacterProperties.AimingRotation = PC->GetControlRotation();
				}
			}
		}

		// Auto-clear force reset after one frame
		if (bForceFootPlacementReset)
		{
			bForceFootPlacementReset = false;
		}
	}

	// Gait
	if (Speed2D < 200.f)
		Gait = EAZ_Gait::Walk;
	else if (Speed2D < 500.f)
		Gait = EAZ_Gait::Run;
	else
		Gait = EAZ_Gait::Sprint;

	// --- Movement Direction (matches GASP F/B/LL/LR/RL/RR) ---
	// 6-way classification: forward/back use F/B directly, side moves decompose
	// to LL/LR/RL/RR based on direction bias (which foot is leading). Until foot
	// phase is tracked, we default side moves to LL and RR and let strafe animations
	// cycle feet via MM trajectory matching.
	if (bHasVelocity)
	{
		AActor* Owner = OwningHeroPawn ? static_cast<AActor*>(OwningHeroPawn.Get()) : static_cast<AActor*>(OwningCharacter.Get());
		if (Owner)
		{
			const FVector Forward = Owner->GetActorForwardVector();
			const FVector Right = Owner->GetActorRightVector();
			const FVector Vel2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
			const float ForwardDot = FVector::DotProduct(Forward, Vel2D);
			const float RightDot = FVector::DotProduct(Right, Vel2D);

			if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
			{
				MovementDirection = ForwardDot >= 0.f ? EAZ_MovementDirection::F : EAZ_MovementDirection::B;
			}
			else
			{
				// Side movement: default to LL/RR (left-foot for left, right-foot for right).
				// LR/RL variants are used by foot-phase-aware databases once foot tracking lands.
				MovementDirection = RightDot >= 0.f ? EAZ_MovementDirection::RR : EAZ_MovementDirection::LL;
			}
		}
	}

	// --- Trajectory-derived values ---
	if (CharacterTrajectory.Samples.Num() > 0)
	{
		AActor* Owner = OwningHeroPawn ? static_cast<AActor*>(OwningHeroPawn.Get()) : static_cast<AActor*>(OwningCharacter.Get());
		const FQuat ActorFacing = Owner ? Owner->GetActorQuat() : FQuat::Identity;

		// Find future samples (TimeInSeconds > 0)
		FVector FutureVelocity = FVector::ZeroVector;
		FVector NearFutureVelocity = FVector::ZeroVector;
		FQuat FutureFacingQuat = ActorFacing;
		float FutureTime = 0.f;
		float NearFutureTime = 0.f;

		for (const FTransformTrajectorySample& Sample : CharacterTrajectory.Samples)
		{
			if (Sample.TimeInSeconds > 0.f)
			{
				// First future sample = near future
				if (NearFutureTime == 0.f)
				{
					NearFutureVelocity = (Sample.Position - (Owner ? Owner->GetActorLocation() : FVector::ZeroVector)) / FMath::Max(Sample.TimeInSeconds, 0.01f);
					NearFutureTime = Sample.TimeInSeconds;
				}
				// Last future sample = far future
				FutureVelocity = (Sample.Position - (Owner ? Owner->GetActorLocation() : FVector::ZeroVector)) / FMath::Max(Sample.TimeInSeconds, 0.01f);
				FutureFacingQuat = Sample.Facing;
				FutureTime = Sample.TimeInSeconds;
			}
		}

		Trj_FutureVelocity = FutureVelocity;
		Trj_NearFutureVelocity = NearFutureVelocity;

		// Future facing delta = angle between current facing and predicted future facing
		// When idle (no velocity), future facing should match current facing -> delta = 0
		if (bHasVelocity)
		{
			const FRotator CurrentFacingRot = ActorFacing.Rotator();
			Trj_FutureFacing = FutureFacingQuat.Rotator();
			FutureFacingDelta = FRotator::NormalizeAxis(Trj_FutureFacing.Yaw - CurrentFacingRot.Yaw);
		}
		else
		{
			Trj_FutureFacing = ActorFacing.Rotator();
			FutureFacingDelta = 0.f;
		}

		// Angular velocity (yaw change rate)
		if (DeltaSeconds > UE_KINDA_SMALL_NUMBER)
		{
			const float YawDelta = FRotator::NormalizeAxis(ActorFacing.Rotator().Yaw - CharacterTransform_LastFrame.Rotator().Yaw);
			Trj_CurrentAngularVelocity = YawDelta / DeltaSeconds;
		}

		// IsCircling: moving + turning significantly + not a sharp pivot
		Trj_IsCircling = bHasVelocity
			&& FMath::Abs(Trj_CurrentAngularVelocity) > 30.f
			&& FMath::Abs(FutureFacingDelta) < 90.f;

		// CirclingTime accumulator
		if (Trj_IsCircling)
		{
			Trj_CirclingTime += DeltaSeconds;
		}
		else
		{
			Trj_CirclingTime = 0.f;
		}
	}

	// --- MovementMode_Recent (delayed tracking, holds previous mode briefly) ---
	{
		static constexpr float ModeRecentDelay = 0.2f;
		if (MovementMode != MovementMode_LastFrame)
		{
			MovementModeRecentTimer = ModeRecentDelay;
		}
		if (MovementModeRecentTimer > 0.f)
		{
			MovementModeRecentTimer -= DeltaSeconds;
		}
		else
		{
			MovementMode_Recent = MovementMode;
		}
	}

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
	// Large facing delta while standing still
	return FMath::Abs(FutureFacingDelta) >= 50.f && Speed2D < 50.f
		&& MovementState == EAZ_MovementState::Idle;
}

bool UAZ_AnimInstance::ShouldReEnterTurnInPlace() const
{
	if (!ShouldTurnInPlace()) return false;

	// Check spin direction from BlendStack Tags (matching GASP logic)
	bool bSpinL = (BlendStackInputs.Tags == FName(TEXT("Spin_L"))) && FutureFacingDelta > 45.f;
	bool bSpinR = (BlendStackInputs.Tags == FName(TEXT("Spin_R"))) && FutureFacingDelta < -45.f;

	return bSpinL || bSpinR;
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
	// 0=Interpolate, 1=Accumulate, 2=Release
	if (MovementMode == EAZ_MovementMode::InAir) return 2; // Release
	if (MovementState == EAZ_MovementState::Moving) return 0; // Interpolate
	return 2; // Release when idle
}

uint8 UAZ_AnimInstance::Get_OffsetRootRotationMode() const
{
	// 0=Interpolate, 1=Accumulate, 2=Release
	return 1; // Accumulate (GASP default)
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
