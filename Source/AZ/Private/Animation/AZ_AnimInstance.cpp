#include "Animation/AZ_AnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AZ_GameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Weapon/AZ_Weapon.h"

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

	// --- Left Hand IK: find primary weapon's grip socket ---
	bUseLeftHandIK = false;
	if (bEnableLeftHandIK && CurrentWeaponTag.IsValid() && OwningCharacter)
	{
		const FName PrimaryTag = FAZ_GameplayTags::Get().Weapon_Slot_Primary.GetTagName();
		TArray<AActor*> AttachedActors;
		OwningCharacter->GetAttachedActors(AttachedActors);
		for (AActor* Attached : AttachedActors)
		{
			if (!Attached->ActorHasTag(PrimaryTag)) continue;

			if (AAZ_Weapon* Weapon = Cast<AAZ_Weapon>(Attached))
			{
				if (USkeletalMeshComponent* WeaponMesh = Weapon->GetWeaponMesh3P())
				{
					const FName GripSocket = bIsAiming ? LeftHandGripAimSocket : LeftHandGripSocket;
					if (WeaponMesh->DoesSocketExist(GripSocket))
					{
						FTransform SocketTransform = WeaponMesh->GetSocketTransform(GripSocket, RTS_World);
						SocketTransform.AddToTranslation(SocketTransform.GetRotation().RotateVector(LeftHandIKOffset));
						if (USkeletalMeshComponent* CharMesh = OwningCharacter->GetMesh())
						{
							LeftHandIKTransform = SocketTransform.GetRelativeTransform(CharMesh->GetComponentTransform());
						}
						bUseLeftHandIK = true;
					}
				}
				break;
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

void UAZ_AnimInstance::NativePostEvaluateAnimation()
{
	Super::NativePostEvaluateAnimation();

	if (!OwningCharacter) return;

	// --- Weapon aim positioning: copy AimSocket transform after animation is evaluated ---
	if (bIsAiming && CurrentWeaponTag.IsValid())
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

				// Smooth interpolation toward aim position
				const float DT = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
				const float Alpha = FMath::Clamp(DT * WeaponPoseInterpSpeed, 0.f, 1.f);
				CurrentWeaponRelativeTransform.BlendWith(TargetRelative, Alpha);

				WeaponRoot->SetRelativeTransform(CurrentWeaponRelativeTransform);
			}
		}
	}
	else if (CachedPrimaryWeapon.IsValid())
	{
		// Not aiming — smooth back to identity (RelaxedSocket)
		if (USceneComponent* WeaponRoot = CachedPrimaryWeapon->GetRootComponent())
		{
			const float DT = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
			const float Alpha = FMath::Clamp(DT * WeaponPoseInterpSpeed, 0.f, 1.f);
			CurrentWeaponRelativeTransform.BlendWith(FTransform::Identity, Alpha);

			WeaponRoot->SetRelativeTransform(CurrentWeaponRelativeTransform);

			// Once close enough to identity, snap and clear cache
			if (CurrentWeaponRelativeTransform.GetLocation().IsNearlyZero(0.1f))
			{
				WeaponRoot->SetRelativeTransform(FTransform::Identity);
				CurrentWeaponRelativeTransform = FTransform::Identity;
				CachedPrimaryWeapon = nullptr;
			}
		}
	}
}
