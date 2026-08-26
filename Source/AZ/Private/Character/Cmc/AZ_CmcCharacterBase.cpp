
#include "Character/Cmc/AZ_CmcCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "TimerManager.h"

AAZ_CmcCharacterBase::AAZ_CmcCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetCapsuleComponent()->InitCapsuleSize(25.f, 90.f);
	GetCapsuleComponent()->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	GetCapsuleComponent()->SetCanEverAffectNavigation(false);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->CanCharacterStepUpOn = ECB_No;

	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -92.f));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw   = false;
	bUseControllerRotationRoll  = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));

	UCharacterMovementComponent* CMC = GetCharacterMovement();
	CMC->MinAnalogWalkSpeed              = 150.f;
	CMC->BrakingFrictionFactor           = 0.f;
	CMC->PerchRadiusThreshold            = 20.f;
	CMC->bUseFlatBaseForFloorChecks      = true;
	CMC->SetCrouchedHalfHeight(60.f);
	CMC->NavAgentProps.bCanCrouch        = true;
	CMC->bCanWalkOffLedgesWhenCrouching  = true;
	if (FNavMovementProperties* NavProps = CMC->GetNavMovementProperties())
	{
		NavProps->bUseAccelerationForPaths = true;
	}
}

void AAZ_CmcCharacterBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	TeamId = FGenericTeamId(DefaultTeamId);
	GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
	SetGait(CurrentGait);

	WireModularMeshFollowers();
}

void AAZ_CmcCharacterBase::WireModularMeshFollowers()
{
	USkeletalMeshComponent* LeaderMesh = GetMesh();
	if (!LeaderMesh)
	{
		return;
	}

	TArray<USceneComponent*> Descendants;
	LeaderMesh->GetChildrenComponents( true, Descendants);

	int32 WiredCount = 0;
	int32 ExcludedCount = 0;
	for (USceneComponent* Child : Descendants)
	{
		USkeletalMeshComponent* Follower = Cast<USkeletalMeshComponent>(Child);
		if (!Follower || Follower == LeaderMesh || !Follower->GetSkeletalMeshAsset())
		{
			continue;
		}

		if (ModularFollowerExclusions.Contains(Follower->GetFName()))
		{
			++ExcludedCount;
			continue;
		}

		if (Follower->GetAnimClass())
		{
			Follower->SetAnimInstanceClass(nullptr);
		}

		Follower->SetLeaderPoseComponent(LeaderMesh,  true);
		++WiredCount;
	}

	UE_LOG(LogTemp, Display, TEXT("[CmcMesh] %s wired %d modular follower mesh(es) to %s (%d excluded)"),
		*GetName(), WiredCount, *LeaderMesh->GetName(), ExcludedCount);
}

void AAZ_CmcCharacterBase::Landed(const FHitResult& Hit)
{
	LandVelocity = GetCharacterMovement()->Velocity;

	Super::Landed(Hit);

	bJustLanded = true;
	GetWorldTimerManager().SetTimer(JustLandedTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bJustLanded = false;
	}), JustLandedDuration,  false);
}

FRotator AAZ_CmcCharacterBase::GetAimRotation() const
{
	return IsLocallyControlled() ? GetControlRotation() : GetBaseAimRotation();
}

namespace AZ::CmcStop
{
	static constexpr float StoppedSpeedTolerance = 2.f;

	static constexpr float TimeoutFactor = 2.5f;

	static constexpr float MinStopTime = 0.05f;
}

float AAZ_CmcCharacterBase::GetStopRemainingDistance() const
{
	if (!bStopActive)
	{
		return 0.f;
	}

	const float Travelled = FVector::DotProduct(GetActorLocation() - StopStartLocation, StopDirection);
	return FMath::Max(0.f, StopPlannedDistance - Travelled);
}

float AAZ_CmcCharacterBase::GetStopProgress() const
{
	if (!bStopActive || StopPlannedDistance <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	return FMath::Clamp(1.f - GetStopRemainingDistance() / StopPlannedDistance, 0.f, 1.f);
}

EAZ_Gait AAZ_CmcCharacterBase::BandForSpeed(float Speed2D) const
{
	if (Speed2D > (RunSpeed + SprintSpeed) * 0.5f)  { return EAZ_Gait::Sprint; }
	if (Speed2D > (WalkSpeed + RunSpeed) * 0.5f)    { return EAZ_Gait::Run; }
	return EAZ_Gait::Walk;
}

void AAZ_CmcCharacterBase::UpdateSelectionGait()
{
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move)
	{
		SelectionGait = CurrentGait;
		return;
	}

	const float Speed2D = Move->Velocity.Size2D();
	const bool bHasInput = !Move->GetCurrentAcceleration().IsNearlyZero();
	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;

	if (!bHasInput)
	{
		if (!bStopBandLatched)
		{
			bStopBandLatched = true;
			LatchedStopBand = BandForSpeed(Speed2D);

			bStopActive = true;
			StopEntrySpeed = Speed2D;
			StopDirection = Move->Velocity.GetSafeNormal2D();
			StopStartLocation = GetActorLocation();
			StopElapsed = 0.f;

			bStopIsAnimated = (Speed2D >= StopAnimEnterSpeed) && !StopDirection.IsNearlyZero();

			const float TargetStopTime =
				FMath::Max(bStopIsAnimated ? StopTimeSeconds : StopFloorTimeSeconds, AZ::CmcStop::MinStopTime);
			StopBrakingDecel = StopEntrySpeed / TargetStopTime;
			StopPlannedDistance = StopEntrySpeed * TargetStopTime * 0.5f;
		}

		if (bStopActive)
		{
			StopElapsed += DeltaSeconds;

			const bool bHalted = Speed2D <= AZ::CmcStop::StoppedSpeedTolerance;
			const bool bOverran = StopElapsed > (StopTimeSeconds * AZ::CmcStop::TimeoutFactor);
			if (bHalted || bOverran)
			{
				if (bStopIsAnimated)
				{
					const float Travelled = FVector::DotProduct(GetActorLocation() - StopStartLocation, StopDirection);
					UE_LOG(LogTemp, Display,
						TEXT("[CmcStop] entry=%.0f -> %.2fs (target %.2f) | travelled %.0f cm, planned %.0f, err %+.0f | braking=%.0f%s"),
						StopEntrySpeed, StopElapsed, StopTimeSeconds, Travelled, StopPlannedDistance,
						Travelled - StopPlannedDistance, StopBrakingDecel, bOverran ? TEXT("  TIMEOUT") : TEXT(""));
				}
				bStopActive = false;
			}
		}
	}
	else
	{
		bStopBandLatched = false;
		bStopActive = false;
		bStopIsAnimated = false;
	}

	if (bStopBandLatched && bStopActive)
	{
		SelectionGait = LatchedStopBand;
	}
	else
	{
		const EAZ_Gait SpeedBand = BandForSpeed(Speed2D);
		SelectionGait = (static_cast<uint8>(SpeedBand) > static_cast<uint8>(CurrentGait)) ? SpeedBand : CurrentGait;
	}
}

void AAZ_CmcCharacterBase::FillAnimContract(FAZ_CmcAnimContract& Out) const
{
	Out.ActorTransform = GetActorTransform();
	Out.AimingRotation = GetAimRotation();
	Out.Gait = CurrentGait;
	Out.SelectionGait = SelectionGait;

	Out.bStopActive = bStopActive;
	Out.bStopIsAnimated = bStopIsAnimated;
	Out.StopEntrySpeed = StopEntrySpeed;
	Out.StopRemainingDistance = GetStopRemainingDistance();
	Out.StopPlannedDistance = StopPlannedDistance;
	Out.StopProgress = GetStopProgress();
	Out.WalkSpeed = WalkSpeed;
	Out.RunSpeed = RunSpeed;
	Out.SprintSpeed = SprintSpeed;
	Out.Stance = bIsCrouched ? EAZ_Stance::Crouching : EAZ_Stance::Standing;
	Out.bJustLanded = bJustLanded;
	Out.LandVelocity = LandVelocity;
	GetOwnedGameplayTags(Out.OwnedTags);

	Out.RotationMode = EAZ_RotationMode::OrientToMovement;

	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move)
	{
		return;
	}

	Out.Velocity = Move->Velocity;
	Out.InputAcceleration = Move->GetCurrentAcceleration();
	Out.CurrentMaxAcceleration = Move->GetMaxAcceleration();
	Out.CurrentMaxDeceleration = Move->GetMaxBrakingDeceleration();
	Out.CurrentMaxSpeed = Move->GetMaxSpeed();
	Out.MovementMode = Move->IsFalling() ? EAZ_MovementMode::InAir : EAZ_MovementMode::OnGround;

	if (Move->CurrentFloor.IsWalkableFloor())
	{
		Out.GroundLocation = Move->CurrentFloor.HitResult.ImpactPoint;
		Out.GroundNormal = Move->CurrentFloor.HitResult.ImpactNormal;
	}
	else
	{
		Out.GroundLocation = GetActorLocation();
		Out.GroundNormal = FVector::UpVector;
	}

	if (const UWorld* World = GetWorld())
	{
		Out.BasedMovementDelta = MovementBaseUtility::GetMovementBaseVelocity(
			GetMovementBaseInterfaceData(), BasedMovement.BoneName) * World->GetDeltaSeconds();
	}

	if (Out.RotationMode == EAZ_RotationMode::OrientToMovement && !Out.InputAcceleration.IsNearlyZero())
	{
		Out.OrientationIntent = Out.InputAcceleration.Rotation();
	}
	else
	{
		Out.OrientationIntent = FRotator(0.f, Out.AimingRotation.Yaw, 0.f);
	}
}

void AAZ_CmcCharacterBase::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->GetOwnedGameplayTags(TagContainer);
	}
}

bool AAZ_CmcCharacterBase::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(TagToCheck);
}

bool AAZ_CmcCharacterBase::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return ASC && ASC->HasAllMatchingGameplayTags(TagContainer);
}

bool AAZ_CmcCharacterBase::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return ASC && ASC->HasAnyMatchingGameplayTags(TagContainer);
}

void AAZ_CmcCharacterBase::SetJumpPressed(bool bPressed)
{
	if (bPressed)
	{
		Jump();
	}
	else
	{
		StopJumping();
	}
}

void AAZ_CmcCharacterBase::SetGait(EAZ_Gait NewGait)
{
	CurrentGait = NewGait;
	float Speed = RunSpeed;
	switch (NewGait)
	{
	case EAZ_Gait::Walk:   Speed = WalkSpeed;   break;
	case EAZ_Gait::Run:    Speed = RunSpeed;    break;
	case EAZ_Gait::Sprint: Speed = SprintSpeed; break;
	}
	GetCharacterMovement()->MaxWalkSpeed = Speed;
}
