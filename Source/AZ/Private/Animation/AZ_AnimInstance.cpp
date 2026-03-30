#include "Animation/AZ_AnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AZ_GameplayTags.h"
#include "Camera/CameraComponent.h"
#include "Character/AZ_HeroCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Weapon/AZ_Weapon.h"
#include "CharacterTrajectoryComponent.h"

void UAZ_AnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwningCharacter = Cast<ACharacter>(TryGetPawnOwner());
	if (OwningCharacter)
	{
		MovementComponent = OwningCharacter->GetCharacterMovement();
	}
}

void UAZ_AnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!MovementComponent || !OwningCharacter)
	{
		return;
	}

	// --- Ground Speed ---
	const FVector Velocity = MovementComponent->Velocity;
	GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.f).Size();

	// --- Falling / Landing detection ---
	const bool bCurrentlyFalling = MovementComponent->IsFalling();

	// Detect landing: was falling last frame, not falling now
	if (bWasFalling && !bCurrentlyFalling)
	{
		bIsJumping = false;
	}

	bIsFalling = bCurrentlyFalling;
	bWasFalling = bCurrentlyFalling;

	// --- Crouch state ---
	bIsCrouching = MovementComponent->IsCrouching();

	// --- Combined aim pose (aiming or shooting) ---
	bWantsAimPose = bIsAiming || bIsShooting;

	// --- Character Trajectory (for Motion Matching) ---
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
	if (!CachedASC && OwningCharacter)
	{
		CachedASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningCharacter);
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


	// --- Camera interpolation (stance-dependent: stand / aim / crouch / crouch+aim) ---
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
	if (bWantsAimPose)
	{
		if (const AAZ_HeroCharacter* Hero = Cast<AAZ_HeroCharacter>(OwningCharacter))
		{
			const UCameraComponent* Camera = Hero->bIsFirstPersonPerspective
				? Hero->FirstPersonCamera
				: Hero->ThirdPersonCamera;
			if (Camera)
			{
				const FVector TraceStart = Camera->GetComponentLocation();
				const FVector TraceEnd = TraceStart + Camera->GetForwardVector() * AimTraceDistance;

				FHitResult HitResult;
				FCollisionQueryParams Params;
				Params.AddIgnoredActor(OwningCharacter);

				if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, Params))
				{
					AimTarget = HitResult.ImpactPoint;
				}
				else
				{
					AimTarget = TraceEnd;
				}

				// Compute aim pitch/yaw relative to character facing
				const FVector CharLocation = OwningCharacter->GetActorLocation();
				const FVector DirToTarget = (AimTarget - CharLocation).GetSafeNormal();
				const FRotator AimRotation = DirToTarget.Rotation();
				const FRotator CharRotation = OwningCharacter->GetActorRotation();
				const FRotator Delta = (AimRotation - CharRotation).GetNormalized();
				AimYaw = Delta.Yaw;
				AimPitch = Delta.Pitch;
			}
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
			FTransform TargetTransform;
			if (Weapon->GetLeftHandSocket(OwningCharacter->GetMesh(), WeaponAttachSocket, CurrentWeaponPoseState, TargetTransform))
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
	if (MaxGroundSpeed > UE_KINDA_SMALL_NUMBER && GroundSpeed > UE_KINDA_SMALL_NUMBER)
	{
		const FVector GroundVelocity(Velocity.X, Velocity.Y, 0.f);
		const FVector LocalVelocity = OwningCharacter->GetActorRotation().UnrotateVector(GroundVelocity);

		NormalizedWalkForwardSpeed = FMath::Clamp(LocalVelocity.X / MaxGroundSpeed, -1.f, 1.f);
		NormalizedWalkRightSpeed = FMath::Clamp(LocalVelocity.Y / MaxGroundSpeed, -1.f, 1.f);
	}
	else
	{
		NormalizedWalkForwardSpeed = 0.f;
		NormalizedWalkRightSpeed = 0.f;
	}
}

AAZ_Weapon* UAZ_AnimInstance::GetPrimaryWeapon() const
{
	if (!OwningCharacter) return nullptr;

	const FName PrimaryTag = FAZ_GameplayTags::Get().Weapon_Slot_Primary.GetTagName();
	TArray<AActor*> AttachedActors;
	OwningCharacter->GetAttachedActors(AttachedActors);
	for (AActor* Attached : AttachedActors)
	{
		if (Attached->ActorHasTag(PrimaryTag))
		{
			return Cast<AAZ_Weapon>(Attached);
		}
	}

	return nullptr;
}
