
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

namespace AZ::CmcJump
{
	// ★ CONTENT-DERIVED. Authored apex height of each takeoff clip, measured 2026-08-27 at 120 Hz from
	// the clip's root-Z peak (AnimationLibrary.get_bone_pose_for_time(seq,"root",t,true)):
	//   AnimPro_JumpIdleStart    apex 0.467 s -> 100.1 cm
	//   AnimPro_JumpWalkStart_*  apex 0.367 s ->  60.0 cm
	//   AnimPro_JumpRunStart_LU  apex 0.433 s ->  70.1 cm   (_RU apex 0.333 s, same height)
	// We store the HEIGHT, not the launch velocity, and solve v = sqrt(2*g*h) against LIVE gravity — so
	// the capsule keeps matching the animation even if GravityScale is retuned. Re-measure if the clips
	// are re-authored; see project_cmc_jump_build_order.
	static constexpr float ApexIdle = 100.1f;
	static constexpr float ApexWalk =  60.0f;
	static constexpr float ApexRun  =  70.1f;

	/** Below this ground speed the idle takeoff clip is the one that plays, so use its arc. */
	static constexpr float IdleJumpMaxSpeed = 50.f;
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

bool AAZ_CmcCharacterBase::IsAnimDrivingMovement() const
{
	return IsPlayingRootMotion();
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
	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;

	// ★ Root motion owns movement -> the stop contract must stand down.
	// A turn montage deliberately suppresses AddMovementInput while it plays, which makes
	// GetCurrentAcceleration() zero — indistinguishable from the player releasing the stick. Without
	// this guard the stop contract engaged mid-turn and wrote velocity from the STOP clip's curve on
	// top of the turn's root motion (measured 2026-08-27: `curve ENGAGED body=342 clip=390 step=+48`
	// 83 ms before the turn started, then `entry=365 -> 0.53s ... err -81`). Two owners writing one
	// velocity is the spike on the transition to idle.
	// The player releasing DURING a turn is not lost: acceleration is still zero when the montage
	// ends, so the stop latches on the very next frame — which is also the exit contract, complete
	// the turn, then stop.
	// ★ MEASURED 2026-08-27: this guard was INERT for the turn montage. IsPlayingRootMotion() does not
	// report a clip started with PlaySlotAnimationAsDynamicMontage, which is exactly how the turn plays,
	// so bAnimDrivesMovement stayed false, the stop latched mid-turn, and SelectionGait was forced to
	// LatchedStopBand (:228) — dropping a running character to the WALK databases at ~230 cm/s:
	//   #13 RunFwdLoop -> WalkFwdLoop | spd=230 mtg=1 | cmd=Run sel=Walk | gates=[WalkMove]
	//   #14 back to RunFwdLoop 162 ms later
	// That is the "strange anims during the rotation". IsAnimDrivingMovement() is the one owner of
	// "an animation drives the capsule" and the hero override includes its turn montage.
	const bool bAnimDrivesMovement = IsAnimDrivingMovement();
	const bool bHasInput = !Move->GetCurrentAcceleration().IsNearlyZero() || bAnimDrivesMovement;

	// ★ GROUND GUARD — the stop contract is a GROUNDED contract. Without this, a jump with no stick
	// input reads exactly like a release: bStopActive latches mid-air, SelectionGait is forced to
	// LatchedStopBand, and the hero's curve braking writes velocity while the capsule is in Falling —
	// two owners on one velocity, airborne. Same failure class as the turn bugs fixed 2026-08-27
	// (see project_cmc_input_gap_doctrine): "no input" is not "the player stopped".
	// Airborne, SelectionGait falls through to the speed-band path below, which keeps the horizontal
	// speed the character took off with — so a run-jump still lands on run-gait land clips.
	if (!Move->IsMovingOnGround())
	{
		bStopBandLatched = false;
		bStopActive      = false;
		bStopIsAnimated  = false;
	}
	else if (!bHasInput)
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
	if (!bPressed)
	{
		// ★ DO NOT clear a press the movement tick has not consumed yet — this is why space did nothing.
		// UAZ_GA_PawnJump ends through WaitInputRelease(bTestAlreadyReleased=true), which can fire the
		// release in the SAME frame as the press. StopJumping() sets bPressedJump=false outright
		// (Character.cpp), so the press was destroyed before CheckJumpInput ever saw it and the
		// character silently never left the ground — measured 2026-08-28: 16x
		// "[CmcJump] SetJumpPressed(1) canJump=1 jumpAllowed=1 onGround=1" with no jump.
		// The Mover hero hit exactly this and worked around it with a one-shot latch: "clearing on
		// release silently dropped those jumps (audit P2-18)".
		// bPressedJump still true => not yet consumed, so leave it alone. Nothing leaks: the engine
		// clears it itself in ClearJumpInput once JumpKeyHoldTime >= JumpMaxHoldTime (0 here), and
		// landing calls ResetJumpState(), so JumpCurrentCount still recovers for the next jump.
		if (!bPressedJump)
		{
			StopJumping();
		}
		return;
	}

	// ★ ONE OWNER for jump height. Derive it from the SAME state that will select the takeoff clip, at
	// the moment of the jump, so the physics arc and the authored arc agree BY CONSTRUCTION.
	// Measured 2026-08-27: GravityScale is 1.5 (effective g = 1470 cm/s^2), and the flat
	// JumpZVelocity = 420 produced a 60 cm apex — exactly right for the WALK clip and 40 cm short for
	// idle, so the body played a big jump while the capsule did a small one. Solving for the authored
	// apex removes that mismatch and, with it, the main argument for an RM-driven rise: no montage root
	// motion has to fight gravity for ownership of the capsule.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		const float Speed2D = Move->Velocity.Size2D();
		const float Apex = (Speed2D <= AZ::CmcJump::IdleJumpMaxSpeed)
			? AZ::CmcJump::ApexIdle
			: ((SelectionGait == EAZ_Gait::Walk) ? AZ::CmcJump::ApexWalk : AZ::CmcJump::ApexRun);

		// GetGravityZ() on CMC already folds in GravityScale (CharacterMovementComponent.cpp:3582-3585).
		const float GravityZ = FMath::Abs(Move->GetGravityZ());
		if (GravityZ > KINDA_SMALL_NUMBER && Apex > 0.f)
		{
			Move->JumpZVelocity = FMath::Sqrt(2.f * GravityZ * Apex);
		}

		// DIAGNOSTIC: this line is the split point for "space does nothing". Every static link was
		// verified correct (SpaceBar -> AZ_IA_RT_Jump -> Input.Action.Jump -> BP_AZ_GA_PawnJump, granted
		// in StartupAbilities, JumpMaxCount=1, NavAgentProps.bCanJump=true) — so the failure is runtime.
		//   line PRESENT  -> the GA reached the pawn; the problem is CanJump()/CMC below.
		//   line ABSENT   -> input never became an ability activation; the problem is upstream
		//                    (IMC not applied, PC not binding, or the ASC never granted the spec).
		UE_LOG(LogTemp, Warning,
			TEXT("[CmcJump] SetJumpPressed(1) | canJump=%d jumpAllowed=%d onGround=%d falling=%d "
			     "crouched=%d maxCount=%d apex=%.1f jumpZ=%.1f"),
			CanJump() ? 1 : 0, Move->IsJumpAllowed() ? 1 : 0,
			Move->IsMovingOnGround() ? 1 : 0, Move->IsFalling() ? 1 : 0,
			bIsCrouched ? 1 : 0, JumpMaxCount, Apex, Move->JumpZVelocity);
	}

	Jump();
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
