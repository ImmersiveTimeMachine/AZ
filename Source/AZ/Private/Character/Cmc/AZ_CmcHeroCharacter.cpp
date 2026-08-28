
#include "Character/Cmc/AZ_CmcHeroCharacter.h"
#include "Animation/AnimMontage.h"

#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/AZ_GA_HitReact.h"
#include "AbilitySystem/Abilities/AZ_GA_PlayerGrabbed.h"
#include "Animation/AZ_CmcAnimInstance.h"
#include "Chooser.h"
#include "ChooserFunctionLibrary.h"
#include "Animation/AZ_LocomotionStateMachine.h"
#include "AZ_GameplayTags.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "MotionWarpingComponent.h"
#include "RootMotionModifier_SkewWarp.h"
#include "Player/AZ_PlayerState.h"

AAZ_CmcHeroCharacter::AAZ_CmcHeroCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->JumpZVelocity = 420.f;
	Move->GravityScale = 1.5f;
	Move->AirControl = 0.2f;
	Move->MaxStepHeight = 30.f;
	Move->SetWalkableFloorAngle(38.f);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 220.f;
	CameraBoom->SocketOffset = FVector(0.f, 70.f, 0.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraLagSpeed = 8.f;
	CameraBoom->CameraRotationLagSpeed = 10.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom);
	Camera->SetFieldOfView(80.f);
}

namespace AZ::RmMontage
{
	static constexpr float ReleaseFraction   = 0.60f;

	static constexpr float StopReleaseFraction = 0.92f;

	static bool bStopEdgePending = false;
	static constexpr float InterruptAngleDeg = 60.f;
	static constexpr float BlendOutSeconds   = 0.15f;

	static bool    bActive = false;
	static double  LaunchTime = -1.0;
	static bool    bIsStop = false;
	static FVector LaunchDir = FVector::ZeroVector;

	// How stale LastMoveInputTime may be and still count as "the stick is held". Matches the existing
	// bInputHeld idiom in Tick; named here so the turn-exit hand-back and that check cannot drift apart.
	static constexpr float InputHeldWindow = 0.1f;
}

void AAZ_CmcHeroCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
	}
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
	}
}

void AAZ_CmcHeroCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ResolveGaitAndStanceFromTags();
	TickTurnMontage();
	UpdateSelectionGait();
	if (bStopActive && !bStopActive_LastFrameHero)
	{
		AZ::RmMontage::bStopEdgePending = true;
	}
	bStopActive_LastFrameHero = bStopActive;

	if (!bStopActive)
	{
		AZ::RmMontage::bStopEdgePending = false;
	}
	else if (AZ::RmMontage::bStopEdgePending)
	{
		const UAnimInstance* EdgeAnim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
		if (!EdgeAnim || !EdgeAnim->IsAnyMontagePlaying())
		{
			TryPlayRootMotionStop();
			AZ::RmMontage::bStopEdgePending = false;
		}
	}

	if (AZ::RmMontage::bActive)
	{
		USkeletalMeshComponent* RmMesh = GetMesh();
		UAnimInstance* RmAnim = RmMesh ? RmMesh->GetAnimInstance() : nullptr;
		UAnimMontage* Mtg = RmAnim ? RmAnim->GetCurrentActiveMontage() : nullptr;
		if (!RmAnim || !Mtg)
		{
			AZ::RmMontage::bActive = false;
		}
		else
		{
			const float Len = Mtg->GetPlayLength();
			const float Frac = (Len > KINDA_SMALL_NUMBER) ? (RmAnim->Montage_GetPosition(Mtg) / Len) : 1.f;
			const float Now2 = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
			const bool bInputHeld = ((Now2 - LastMoveInputTime) <= 0.1f) && !LastMoveInputDir.IsNearlyZero();

			const TCHAR* Why = nullptr;
			const float ReleaseAt = AZ::RmMontage::bIsStop
				? AZ::RmMontage::StopReleaseFraction : AZ::RmMontage::ReleaseFraction;
			if (Frac >= ReleaseAt)
			{
				Why = TEXT("delivered");
			}
			else if (!AZ::RmMontage::bIsStop && !bInputHeld)
			{
				Why = TEXT("input released");
			}
			else if (bInputHeld && !AZ::RmMontage::LaunchDir.IsNearlyZero()
				&& FMath::Abs(FRotator::NormalizeAxis(static_cast<float>(
					LastMoveInputDir.Rotation().Yaw - AZ::RmMontage::LaunchDir.Rotation().Yaw)))
					> AZ::RmMontage::InterruptAngleDeg)
			{
				Why = TEXT("redirected");
			}
			else if (AZ::RmMontage::bIsStop && bInputHeld
				&& LastMoveInputTime > (AZ::RmMontage::LaunchTime + 0.05))
			{
				Why = TEXT("stop cancelled");
			}

			if (Why)
			{
				FMontageBlendSettings StopSettings;
				StopSettings.Blend.BlendTime = AZ::RmMontage::BlendOutSeconds;
				StopSettings.BlendMode = EMontageBlendMode::Inertialization;
				RmAnim->Montage_StopWithBlendSettings(StopSettings, Mtg);
				AZ::RmMontage::bActive = false;
				bRootMotionStopActive = false;
				UE_LOG(LogTemp, Display, TEXT("[CmcRmEnd] %s at %.0f%% vel=%.0f"), Why, Frac * 100.f,
					GetCharacterMovement() ? GetCharacterMovement()->Velocity.Size2D() : -1.f);
			}
		}
	}

	if (bRootMotionStopActive)
	{
		const UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
		if (!Anim || !Anim->IsAnyMontagePlaying())
		{
			bRootMotionStopActive = false;
		}
	}

	if (const USkeletalMeshComponent* DbgMesh = GetMesh())
	{
		if (UAnimInstance* DbgAnim = DbgMesh->GetAnimInstance())
		{
			if (UAnimMontage* Mtg = DbgAnim->GetCurrentActiveMontage())
			{
				const UCharacterMovementComponent* M = GetCharacterMovement();
				UE_LOG(LogTemp, Display,
					TEXT("[CmcRmDbg] %s pos=%.2f mtgHasRM=%d charHasRM=%d rmParams=%d rmDelta=%.2f vel=%.0f mode=%d"),
					*GetNameSafe(Mtg), DbgAnim->Montage_GetPosition(Mtg),
					Mtg->HasRootMotion() ? 1 : 0,
					IsPlayingRootMotion() ? 1 : 0,
					M ? (M->RootMotionParams.bHasRootMotion ? 1 : 0) : -1,
					M ? M->RootMotionParams.GetRootMotionTransform().GetTranslation().Size2D() : -1.f,
					M ? M->Velocity.Size2D() : -1.f,
					static_cast<int32>(DbgAnim->RootMotionMode));
			}
		}
	}

	ApplyMovementFeelParams(DeltaSeconds);
	UpdateTurnInPlaceLock(DeltaSeconds);
}

void AAZ_CmcHeroCharacter::ResolveGaitAndStanceFromTags()
{
	const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();

	EAZ_Gait DesiredGait = EAZ_Gait::Walk;
	if (HasMatchingGameplayTag(AZTags.Movement_Sprinting))
	{
		DesiredGait = EAZ_Gait::Sprint;
	}
	else if (HasMatchingGameplayTag(AZTags.Movement_Running))
	{
		DesiredGait = EAZ_Gait::Run;
	}

	if (DesiredGait != GetCurrentGait())
	{
		SetGait(DesiredGait);
	}

	const bool bWantsCrouch = HasMatchingGameplayTag(AZTags.Movement_Crouching);
	if (bWantsCrouch && !bIsCrouched)
	{
		Crouch();
	}
	else if (!bWantsCrouch && bIsCrouched)
	{
		UnCrouch();
	}
}

void AAZ_CmcHeroCharacter::FillAnimContract(FAZ_CmcAnimContract& Out) const
{
	Super::FillAnimContract(Out);

	const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();

	if (HasMatchingGameplayTag(AZTags.Ability_State_Aiming))
	{
		Out.RotationMode = EAZ_RotationMode::Aiming;
	}
	else if (HasMatchingGameplayTag(AZTags.Movement_Strafe))
	{
		Out.RotationMode = EAZ_RotationMode::Strafe;
	}

	Out.InputState.bWantsToSprint = HasMatchingGameplayTag(AZTags.Movement_Sprinting);
	Out.InputState.bWantsToWalk = !Out.InputState.bWantsToSprint && !HasMatchingGameplayTag(AZTags.Movement_Running);
	Out.InputState.bWantsToCrouch = HasMatchingGameplayTag(AZTags.Movement_Crouching);
	Out.InputState.bWantsToStrafe = HasMatchingGameplayTag(AZTags.Movement_Strafe);
	Out.InputState.bWantsToAim = HasMatchingGameplayTag(AZTags.Ability_State_Aiming);

	if (Out.RotationMode == EAZ_RotationMode::OrientToMovement && !Out.InputAcceleration.IsNearlyZero())
	{
		Out.OrientationIntent = Out.InputAcceleration.Rotation();
	}
	else
	{
		Out.OrientationIntent = FRotator(0.f, Out.AimingRotation.Yaw, 0.f);
	}

	Out.bTurnInPlaceActive = bTipLockActive;
	Out.TurnInPlaceTargetYaw = TipTargetYaw;
	if (bTipLockActive)
	{
		Out.OrientationIntent = FRotator(0.f, TipTargetYaw, 0.f);
	}
}

void AAZ_CmcHeroCharacter::ApplyMovementFeelParams(float DeltaSeconds)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move)
	{
		return;
	}

	const float Speed2D = Move->Velocity.Size2D();
	const bool bHasInput = !Move->GetCurrentAcceleration().IsNearlyZero();

	float NoInputBraking = BrakingDecelNoInput;

	bool bCurveDriven = false;
	// ★ A turn montage owns velocity outright. bTurnMontageActive is OUR flag, not an engine query:
	// the earlier guard used ACharacter::IsPlayingRootMotion(), which measurably did NOT report true
	// for a PlaySlotAnimationAsDynamicMontage clip — the stop curve still engaged 800 ms into a turn
	// (`curve ENGAGED body=341 clip=257 step=-84 braking=840`) and wrote the STOP clip's velocity over
	// the turn's root motion. That double-write is the spike on the transition to idle.
	if (bTurnMontageActive)
	{
		bStopCurveEngaged = false;
	}
	else if (bStopCurveBraking && bStopActive && bStopIsAnimated && DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		if (const UAZ_CmcAnimInstance* CmcAnim =
			GetMesh() ? Cast<UAZ_CmcAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr)
		{
			const float ClipSpeed = CmcAnim->GetStopClipDepictedSpeed();

			if (!bStopCurveEngaged && !bStopCurveRejected && CmcAnim->IsStopClipSelected())
			{
				const float HandoverStep = FMath::Abs(ClipSpeed - Speed2D);
				const float ClipTime = CmcAnim->GetStopClipSampleTime();
				const bool bAgrees = ClipSpeed > KINDA_SMALL_NUMBER
					&& HandoverStep <= StopCurveMaxHandoverStep
					&& ClipTime <= StopCurveMaxHandoverClipTime;

				if (!bAgrees)
				{
					bStopCurveRejected = true;
					UE_LOG(LogTemp, Display,
						TEXT("[CmcStop] curve REJECTED: body=%.0f clip=%.0f step=%.0f clipTime=%.2fs ")
						TEXT("-> falling back to v0/T (clip is describing a different stop)"),
						Speed2D, ClipSpeed, HandoverStep, ClipTime);
				}
			}

			const bool bCurveUsable = ClipSpeed > KINDA_SMALL_NUMBER || bStopCurveEngaged;
			if (!bStopCurveRejected && CmcAnim->IsStopClipSelected() && bCurveUsable)
			{
				const float ConvergeOver = FMath::Max(StopCurveConvergenceTime, DeltaSeconds);
				const float Needed = (Speed2D - ClipSpeed) / ConvergeOver;
				NoInputBraking = FMath::Clamp(Needed, 0.f, StopCurveMaxBraking);
				bCurveDriven = true;

				if (!bStopCurveEngaged)
				{
					bStopCurveEngaged = true;
					UE_LOG(LogTemp, Display,
						TEXT("[CmcStop] curve ENGAGED: body=%.0f clip=%.0f step=%+.0f cm/s braking=%.0f  clipTime=%.2fs"),
						Speed2D, ClipSpeed, ClipSpeed - Speed2D, NoInputBraking,
						CmcAnim->GetStopClipSampleTime());
				}
			}
		}
	}

	if (!bStopActive)
	{
		bStopCurveEngaged = false;
		bStopCurveRejected = false;
	}

	if (bRootMotionStopActive)
	{
		NoInputBraking = 0.f;
	}
	else if (!bCurveDriven && bStopTimeBraking && bStopActive)
	{
		NoInputBraking = StopBrakingDecel;
	}
	else if (bGaitScaledBraking)
	{
		switch (SelectionGait)
		{
		case EAZ_Gait::Sprint: NoInputBraking = SprintBrakingDecel; break;
		case EAZ_Gait::Run:    NoInputBraking = RunBrakingDecel;    break;
		default:               NoInputBraking = WalkBrakingDecel;   break;
		}
	}
	Move->BrakingDecelerationWalking = bHasInput ? BrakingDecelWithInput : NoInputBraking;

	const float AccelAlpha = FMath::Clamp(
		(Speed2D - AccelTaperSpeedMin) / FMath::Max(1.f, AccelTaperSpeedMax - AccelTaperSpeedMin), 0.f, 1.f);
	Move->MaxAcceleration = FMath::Lerp(MaxAccelerationBase, MaxAccelerationAtTopSpeed, AccelAlpha);

	const float FrictionAlpha = FMath::Clamp(Speed2D / FMath::Max(1.f, FrictionTaperSpeedMax), 0.f, 1.f);
	Move->GroundFriction = FMath::Lerp(GroundFrictionMax, GroundFrictionMin, FrictionAlpha);

	float NewYawRate = 0.f;
	bool  bFacingTimeApplied = false;

	if (bFacingTimeRotation && !Move->IsFalling())
	{
		const FVector Accel = Move->GetCurrentAcceleration();
		bool  bHasTarget  = false;
		float DesiredYaw  = 0.f;

		if (Move->bOrientRotationToMovement)
		{
			if (!Accel.IsNearlyZero())
			{
				DesiredYaw = static_cast<float>(Accel.Rotation().Yaw);
				bHasTarget = true;
			}
		}
		else if (Move->bUseControllerDesiredRotation && Controller)
		{
			DesiredYaw = static_cast<float>(Controller->GetControlRotation().Yaw);
			bHasTarget = true;
		}

		if (bHasTarget)
		{
			const float DeltaYaw = FMath::Abs(FRotator::NormalizeAxis(
				DesiredYaw - static_cast<float>(GetActorRotation().Yaw)));

			float Tau = bHasInput
				? static_cast<float>(FMath::GetMappedRangeValueClamped(
					FVector2D(RunSpeed, SprintSpeed),
					FVector2D(WalkRunFacingTime, SprintFacingTime), Speed2D))
				: IdleFacingTime;

			Tau -= static_cast<float>(FMath::GetMappedRangeValueClamped(
				FVector2D(CameraSnapShortenStartAngle, CameraSnapShortenFullAngle),
				FVector2D(0.f, CameraSnapShortenMaxSeconds), DeltaYaw));

			Tau = FMath::Max(Tau, FMath::Max(DeltaSeconds, KINDA_SMALL_NUMBER));

			NewYawRate = FMath::Clamp(DeltaYaw / Tau, 0.f, MaxFacingRotationRateYaw);
			bFacingTimeApplied = true;
		}
	}

	if (!bFacingTimeApplied)
	{
		float GroundedYawRate = GroundedRotationRateYaw;
		if (bGaitScaledRotationRate)
		{
			switch (CurrentGait)
			{
			case EAZ_Gait::Run:    GroundedYawRate = RunRotationRateYaw;    break;
			case EAZ_Gait::Sprint: GroundedYawRate = SprintRotationRateYaw; break;
			default:               break;
			}
		}
		NewYawRate = Move->IsFalling() ? FallingRotationRateYaw : GroundedYawRate;

		if (!FMath::IsNearlyEqual(Move->RotationRate.Yaw, NewYawRate))
		{
			UE_LOG(LogTemp, Display, TEXT("[CmcRot] RotationRate.Yaw %.0f -> %.0f | gaitScaled=%d gait=%d falling=%d"),
				Move->RotationRate.Yaw, NewYawRate, bGaitScaledRotationRate ? 1 : 0,
				static_cast<int32>(CurrentGait), Move->IsFalling() ? 1 : 0);
		}
	}

	Move->RotationRate = FRotator(0.f, NewYawRate, 0.f);

	{
		const FVector V = Move->Velocity;
		const FVector A = Move->GetCurrentAcceleration();
		const float   Sp = V.Size2D();
		if (Sp > 40.f && !A.IsNearlyZero())
		{
			const float Ang = FMath::Abs(FRotator::NormalizeAxis(
				static_cast<float>(A.Rotation().Yaw - V.Rotation().Yaw)));
			if (Ang > 25.f)
			{
				UE_LOG(LogTemp, Display,
					TEXT("[CmcTurn] ang=%5.1f spd=%6.1f rot=%5.0f%s fric=%4.1f accel=%5.0f maxspd=%5.0f capsuleYaw=%7.1f"),
					Ang, Sp, Move->RotationRate.Yaw, bFacingTimeApplied ? TEXT("(tau)") : TEXT("(flat)"),
					Move->GroundFriction, Move->MaxAcceleration, Move->GetMaxSpeed(),
					GetActorRotation().Yaw);
			}
		}
	}
}

void AAZ_CmcHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}

	ensureMsgf(DefaultMappingContext,
		TEXT("%s: DefaultMappingContext is null. Set it in the BP child defaults — no IAs will trigger."),
		*GetName());
	ensureMsgf(MoveInputAction,
		TEXT("%s: MoveInputAction is null. Set it in the BP child defaults — movement won't work."),
		*GetName());
	ensureMsgf(LookInputAction,
		TEXT("%s: LookInputAction is null. Set it in the BP child defaults — look won't work."),
		*GetName());

	if (MoveInputAction)
	{
		Input->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AAZ_CmcHeroCharacter::OnMoveTriggered);
	}
	if (LookInputAction)
	{
		Input->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AAZ_CmcHeroCharacter::OnLookTriggered);
	}
}

void AAZ_CmcHeroCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->RemoveMappingContext(DefaultMappingContext);
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void AAZ_CmcHeroCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitAbilitySystem();
}

void AAZ_CmcHeroCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitAbilitySystem();
}

UAbilitySystemComponent* AAZ_CmcHeroCharacter::GetAbilitySystemComponent() const
{
	const AAZ_PlayerState* PS = GetPlayerState<AAZ_PlayerState>();
	return PS ? PS->GetAbilitySystemComponent() : nullptr;
}

void AAZ_CmcHeroCharacter::InitAbilitySystem()
{
	AAZ_PlayerState* PS = GetPlayerState<AAZ_PlayerState>();
	if (!PS)
	{
		return;
	}

	UAZ_AbilitySystemComponent* ASC = Cast<UAZ_AbilitySystemComponent>(PS->GetAbilitySystemComponent());
	if (!ASC)
	{
		return;
	}

	ASC->InitAbilityActorInfo(PS, this);

	if (HasAuthority())
	{
		ASC->GrantAbilitiesWithInputTag(StartupAbilities);

		UClass* GrabbedClass = *GrabbedAbilityClass ? *GrabbedAbilityClass : UAZ_GA_PlayerGrabbed::StaticClass();
		if (!ASC->FindAbilitySpecFromClass(GrabbedClass))
		{
			UAZ_GA_PlayerGrabbed::ConfigureCDO(GrabbedClass);
			FGameplayAbilitySpec GrabbedSpec(GrabbedClass, 1, INDEX_NONE, this);
			GrabbedSpec.GetDynamicSpecSourceTags().AddTag(FAZ_GameplayTags::Get().Input_Action_Interact);
			ASC->GiveAbility(GrabbedSpec);
			UE_LOG(LogTemp, Display, TEXT("[CmcHero] %s granted to hero ASC"), *GrabbedClass->GetName());
		}

		UClass* HitReactClass = *HitReactAbilityClass ? *HitReactAbilityClass : UAZ_GA_HitReact::StaticClass();
		if (!ASC->FindAbilitySpecFromClass(HitReactClass))
		{
			UAZ_GA_HitReact::ConfigureOnCDO(HitReactClass);
			ASC->GiveAbility(FGameplayAbilitySpec(HitReactClass, 1, INDEX_NONE, this));
			UE_LOG(LogTemp, Display, TEXT("[CmcHero] %s granted to hero ASC"), *HitReactClass->GetName());
		}
	}
}


namespace AZ::InputRamp
{

	static double RampStartTime   = -1.0;
	static double LastInputTime   = -1.0;
	static FVector LastInputDir   = FVector::ZeroVector;
	static float  LastLoggedScale = 1.f;
}

void AAZ_CmcHeroCharacter::OnMoveTriggered(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator YawRotation(0.f, Controller ? Controller->GetControlRotation().Yaw : 0.f, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	LastMoveInputDir = (Forward * Axis.Y + Right * Axis.X).GetSafeNormal2D();
	LastMoveInputTime = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : -1.f;

	if (bTipLockActive)
	{
		return;
	}

	if (bTurnInPlaceLock && !LastMoveInputDir.IsNearlyZero())
	{
		const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();
		const UCharacterMovementComponent* Move = GetCharacterMovement();
		const bool bEligible =
			Move && Move->IsMovingOnGround()
			&& Move->Velocity.Size2D() < TurnInPlaceEnterMaxSpeed
			&& !HasMatchingGameplayTag(AZTags.Ability_State_Aiming)
			&& !HasMatchingGameplayTag(AZTags.Movement_Strafe);

		const float InputYaw = static_cast<float>(LastMoveInputDir.Rotation().Yaw);
		const float EnterDelta = FMath::Abs(FRotator::NormalizeAxis(
			InputYaw - static_cast<float>(GetActorRotation().Yaw)));

		if (bEligible && EnterDelta >= TurnInPlaceEnterAngle)
		{
			if (++TipEnterCandidateFrames >= TurnInPlaceMinHoldFrames)
			{
				bTipLockActive = true;
				TipTargetYaw = InputYaw;
				TipLockElapsed = 0.f;
				TipEnterCandidateFrames = 0;
				UE_LOG(LogTemp, Display,
					TEXT("[CmcTip] LATCH target=%.0f delta=%.0f spd=%.0f"),
					TipTargetYaw, EnterDelta, Move->Velocity.Size2D());
			}
			return;
		}
		TipEnterCandidateFrames = 0;
	}

	float RampScale = 1.f;
	if (bInputRampEnabled && GetWorld())
	{
		const double Now = GetWorld()->GetTimeSeconds();
		const FVector Dir = (Forward * Axis.Y + Right * Axis.X).GetSafeNormal2D();

		const bool bFreshPress = (AZ::InputRamp::LastInputTime < 0.0)
			|| ((Now - AZ::InputRamp::LastInputTime) > InputRampReleaseSeconds);
		const bool bHardTurn = !AZ::InputRamp::LastInputDir.IsNearlyZero() && !Dir.IsNearlyZero()
			&& FMath::Abs(FRotator::NormalizeAxis(static_cast<float>(
				Dir.Rotation().Yaw - AZ::InputRamp::LastInputDir.Rotation().Yaw))) > InputRampRetriggerAngle;

		if (bFreshPress || bHardTurn || AZ::InputRamp::RampStartTime < 0.0)
		{
			AZ::InputRamp::RampStartTime = Now;
		}

		const float Elapsed = static_cast<float>(Now - AZ::InputRamp::RampStartTime);
		const float Alpha = FMath::Clamp(Elapsed / FMath::Max(InputRampSeconds, 0.01f), 0.f, 1.f);
		RampScale = FMath::Lerp(InputRampStartScale, 1.f, Alpha);

		AZ::InputRamp::LastInputTime = Now;
		AZ::InputRamp::LastInputDir = Dir;

		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			Move->MinAnalogWalkSpeed = InputRampBaseMinAnalog * RampScale;
		}

		if (FMath::Abs(RampScale - AZ::InputRamp::LastLoggedScale) > 0.15f)
		{
			AZ::InputRamp::LastLoggedScale = RampScale;
			UE_LOG(LogTemp, Display, TEXT("[CmcRamp] scale=%.2f t=%.2f fresh=%d hardTurn=%d spd=%.0f"),
				RampScale, Elapsed, bFreshPress ? 1 : 0, bHardTurn ? 1 : 0,
				GetCharacterMovement() ? GetCharacterMovement()->Velocity.Size2D() : -1.f);
		}
	}

	const FVector WorldInputDir = (Forward * Axis.Y + Right * Axis.X).GetSafeNormal2D();

	// A turn montage owns the capsule while it runs: its root motion supplies rotation AND travel, so
	// adding movement input on top would fight it. Input is still READ every frame (this handler runs),
	// it is simply not applied — which is what makes the exit contract "complete, then snap" rather than
	// "complete while drifting toward the new stick".
	if (bTurnMontageActive)
	{
		return;
	}
	if (TryPlayTurnMontage(WorldInputDir))
	{
		return;
	}

	if (TryPlayRootMotionStart(WorldInputDir))
	{
		return;
	}

	AddMovementInput(Forward, Axis.Y * RampScale);
	AddMovementInput(Right, Axis.X * RampScale);
}

bool AAZ_CmcHeroCharacter::OwnsRootMotionStarts(EAZ_Gait InGait) const
{
	if (!bRootMotionStarts)
	{
		return false;
	}
	return RootMotionStartClips.ContainsByPredicate(
		[InGait](const FAZ_RootMotionStartClip& Row)
		{
			return Row.Clip != nullptr && Row.Gait == InGait;
		});
}

bool AAZ_CmcHeroCharacter::TryPlayRootMotionStart(const FVector& WorldInputDir)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!bRootMotionStarts || !Move || !MeshComp || WorldInputDir.IsNearlyZero())
	{
		return false;
	}

	if (!Move->IsMovingOnGround() || Move->Velocity.Size2D() > RootMotionStartMaxSpeed)
	{
		return false;
	}

	UAnimInstance* Anim = MeshComp->GetAnimInstance();
	if (!Anim)
	{
		return false;
	}

	if (Anim->IsAnyMontagePlaying())
	{
		return false;
	}

	const float Now = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
	if (LastRootMotionStartTime >= 0.f && (Now - LastRootMotionStartTime) < RootMotionStartCooldown)
	{
		return false;
	}

	const float SignedAngle = static_cast<float>(FRotator::NormalizeAxis(
		WorldInputDir.Rotation().Yaw - GetActorRotation().Yaw));
	const EAZ_StartDirection Direction = UAZ_LocomotionStateMachine::BucketStartDirection(SignedAngle);
	const EAZ_Gait Gait = GetCurrentGait();

	ChooserDirection = Direction;
	UAnimSequence* Clip = EvaluateLocomotionChooser(RootMotionStartChooser);
	if (!Clip)
	{
		const FAZ_RootMotionStartClip* Row = RootMotionStartClips.FindByPredicate(
			[Gait, Direction](const FAZ_RootMotionStartClip& Entry)
			{
				return Entry.Gait == Gait && Entry.Direction == Direction && Entry.Clip != nullptr;
			});
		if (Row) { Clip = Row->Clip; }
	}

	if (!Clip)
	{
		return false;
	}

	UAnimMontage* Played = Anim->PlaySlotAnimationAsDynamicMontage(
		Clip, RootMotionStartSlot,
		RootMotionStartBlendIn, RootMotionStartBlendOut,
		 1.f,  1);

	if (!Played)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CmcRmStart] FAILED to play %s on slot %s"),
			*GetNameSafe(Clip), *RootMotionStartSlot.ToString());
		return false;
	}

	LastRootMotionStartTime = Now;
	AZ::RmMontage::bActive = true;
	AZ::RmMontage::LaunchTime = Now;
	AZ::RmMontage::bIsStop = false;
	AZ::RmMontage::LaunchDir = WorldInputDir;

	const UEnum* DirEnum = StaticEnum<EAZ_StartDirection>();
	UE_LOG(LogTemp, Display,
		TEXT("[CmcRmStart] %s | angle=%+.0f bucket=%s gait=%d spd=%.0f dur=%.2fs"),
		*GetNameSafe(Clip), SignedAngle,
		DirEnum ? *DirEnum->GetNameStringByValue(static_cast<int64>(Direction)) : TEXT("?"),
		static_cast<int32>(Gait), Move->Velocity.Size2D(), Clip->GetPlayLength());

	return true;
}

bool AAZ_CmcHeroCharacter::TryPlayTurnMontage(const FVector& WorldInputDir)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	UCharacterMovementComponent* Move = GetCharacterMovement();
	UAnimInstance* Anim = MeshComp ? MeshComp->GetAnimInstance() : nullptr;

	// Rate-limited "why not" trace: this fires at most 2x/sec and names the gate that blocked, so a
	// single PIE answers it instead of another round of reasoning about which condition is false.
	auto Veto = [this](const TCHAR* Why) -> bool
	{
		static double LastLog = -1.0;
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		if (Now - LastLog > 0.5)
		{
			LastLog = Now;
			UE_LOG(LogTemp, Display, TEXT("[CmcTurnVeto] %s"), Why);
		}
		return false;
	};

	if (!bTurnMontages)                 { return Veto(TEXT("bTurnMontages is FALSE")); }
	if (!Anim)                          { return Veto(TEXT("no AnimInstance")); }
	if (!Move)                          { return Veto(TEXT("no CharacterMovement")); }
	if (!MotionWarpingComponent)        { return Veto(TEXT("no MotionWarpingComponent")); }
	if (WorldInputDir.IsNearlyZero())   { return Veto(TEXT("WorldInputDir is zero")); }
	if (bTurnMontageActive)             { return false; }
	if (Anim->IsAnyMontagePlaying())    { return Veto(TEXT("another montage is already playing")); }
	if (!Move->IsMovingOnGround())      { return Veto(TEXT("not moving on ground")); }

	const float Now = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
	if (LastTurnMontageTime >= 0.f && (Now - LastTurnMontageTime) < TurnMontageCooldown)
	{
		return false;
	}

	// Heading error the turn has to deliver. Latched here, at frame 0 of the clip, and never revised —
	// that latch plus frame-0 entry is what makes the delivered heading exact instead of "whatever was
	// left of the clip" (measured: mid-entry gives a 180 asset only ~100-140 deg).
	const float SignedAngle = static_cast<float>(FRotator::NormalizeAxis(
		WorldInputDir.Rotation().Yaw - GetActorRotation().Yaw));
	if (FMath::Abs(SignedAngle) < TurnMontageMinAngleDeg)
	{
		return false;   // ordinary: most frames are not a turn
	}

	const EAZ_StartDirection Bucket = UAZ_LocomotionStateMachine::BucketStartDirection(SignedAngle);
	const EAZ_Gait Gait = GetCurrentGait();

	bool bLeftFoot = true;
	if (const UAZ_CmcAnimInstance* CmcAnim = Cast<UAZ_CmcAnimInstance>(Anim))
	{
		bLeftFoot = CmcAnim->bLeftFootDown;
	}

	const FAZ_TurnMontageClip* Row = TurnMontageClips.FindByPredicate(
		[Gait, Bucket, bLeftFoot](const FAZ_TurnMontageClip& Entry)
		{
			return Entry.Clip && Entry.Gait == Gait && Entry.Direction == Bucket
				&& Entry.bLeftFootDown == bLeftFoot;
		});
	if (!Row)
	{
		// Foot variant is a refinement, not a requirement — take either foot rather than drop the turn.
		Row = TurnMontageClips.FindByPredicate(
			[Gait, Bucket](const FAZ_TurnMontageClip& Entry)
			{
				return Entry.Clip && Entry.Gait == Gait && Entry.Direction == Bucket;
			});
	}
	if (!Row || !Row->Clip)
	{
		const UEnum* DE = StaticEnum<EAZ_StartDirection>();
		UE_LOG(LogTemp, Warning,
			TEXT("[CmcTurnVeto] no clip row for gait=%d bucket=%s (angle %+.0f) — %d rows loaded"),
			static_cast<int32>(Gait),
			DE ? *DE->GetNameStringByValue(static_cast<int64>(Bucket)) : TEXT("?"),
			SignedAngle, TurnMontageClips.Num());
		return false;
	}

	UAnimSequence* Clip = Row->Clip;
	const float Length = Clip->GetPlayLength();

	// Warp target FIRST: the modifier resolves the target by name on its first tick.
	TurnMontageTargetYaw = static_cast<float>(FRotator::NormalizeAxis(GetActorRotation().Yaw + SignedAngle));
	MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
		TurnMontageWarpTarget, GetActorLocation(), FRotator(0.f, TurnMontageTargetYaw, 0.f));

	UAnimMontage* Played = Anim->PlaySlotAnimationAsDynamicMontage(
		Clip, RootMotionStartSlot, TurnMontageBlendIn, TurnMontageBlendOut, 1.f, 1);
	if (!Played)
	{
		MotionWarpingComponent->RemoveWarpTarget(TurnMontageWarpTarget);
		UE_LOG(LogTemp, Warning, TEXT("[CmcTurn] FAILED to play %s on slot %s"),
			*GetNameSafe(Clip), *RootMotionStartSlot.ToString());
		return false;
	}

	// ROTATION-ONLY warp over the whole clip. bWarpTranslation=false leaves the authored travel intact
	// (in-place turns stay in place, pivots keep their arc); RotationMethod::Scale lets a 180 asset serve
	// a smaller request; MaxRotationRate clamps the authored spike at the warp layer instead of letting
	// the capsule desync from the mesh.
	URootMotionModifier_SkewWarp::AddRootMotionModifierSkewWarp(
		MotionWarpingComponent, Clip, 0.f, Length,
		TurnMontageWarpTarget, EWarpPointAnimProvider::None, FTransform::Identity, NAME_None,
		/*bWarpTranslation*/ false, /*bIgnoreZAxis*/ true, /*bWarpRotation*/ true,
		EMotionWarpRotationType::Default, EMotionWarpRotationMethod::Scale,
		/*WarpRotationTimeMultiplier*/ 1.f, TurnMontageMaxRotationRate);

	bTurnMontageActive  = true;
	LastTurnMontageTime = Now;

	// CANCEL any stop already in flight. The stand-down in UpdateSelectionGait only stops a NEW stop
	// from latching; one that latched BEFORE the turn keeps writing velocity from its own clip curve
	// right through it (measured: `curve ENGAGED body=329 clip=246 step=-84 braking=835` mid-turn,
	// which dragged that turn's residual to -10.8 deg against ~0 for the clean ones).
	// One owner per fact: while the turn montage runs, the turn clip owns velocity.
	bStopActive         = false;
	bStopBandLatched    = false;
	bStopIsAnimated     = false;
	bStopCurveEngaged   = false;
	bStopCurveRejected  = false;
	bRootMotionStopActive = false;

	const UEnum* DirEnum = StaticEnum<EAZ_StartDirection>();
	UE_LOG(LogTemp, Display,
		TEXT("[CmcTurn] %s | angle=%+.0f bucket=%s gait=%d Lfoot=%d spd=%.0f dur=%.2fs -> target yaw %.0f"),
		*GetNameSafe(Clip), SignedAngle,
		DirEnum ? *DirEnum->GetNameStringByValue(static_cast<int64>(Bucket)) : TEXT("?"),
		static_cast<int32>(Gait), bLeftFoot ? 1 : 0, Move->Velocity.Size2D(), Length, TurnMontageTargetYaw);

	return true;
}

bool AAZ_CmcHeroCharacter::IsAnimDrivingMovement() const
{
	return Super::IsAnimDrivingMovement() || bTurnMontageActive;
}

void AAZ_CmcHeroCharacter::TickTurnMontage()
{
	if (!bTurnMontageActive)
	{
		return;
	}

	const USkeletalMeshComponent* MeshComp = GetMesh();
	const UAnimInstance* Anim = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (Anim && Anim->IsAnyMontagePlaying())
	{
		return;   // COMPLETE: no redirect interrupt, by contract.
	}

	// SNAP: warping closes most of the gap, but blend-out and the rate clamp can leave a few degrees.
	// Land exactly on the latched target so the heading error cannot accumulate across turns.
	const FRotator Current = GetActorRotation();
	const float Residual = static_cast<float>(FRotator::NormalizeAxis(TurnMontageTargetYaw - Current.Yaw));
	SetActorRotation(FRotator(Current.Pitch, TurnMontageTargetYaw, Current.Roll));

	if (MotionWarpingComponent)
	{
		MotionWarpingComponent->RemoveWarpTarget(TurnMontageWarpTarget);
	}
	bTurnMontageActive = false;

	// ★ HAND THE STICK BACK ON THE SAME FRAME — this closes the "stop clip stabbed in on turn exit" bug.
	// OnMoveTriggered early-returns while the turn owns the capsule, so on the frame this flag clears
	// there is no AddMovementInput yet: CMC reports acceleration 0 while the anim instance's
	// bMontageActive_GT has already gone false. ChooserContext.bIsMoving is pure input intent with no
	// hysteresis (AZ_CmcAnimInstance.cpp:1401), so it reads that single frame as "the player let go":
	// the SM drops LocomotionLoop -> TransitionToIdle (AZ_LocomotionStateMachine.cpp:264), the Stops
	// pool opens, and MM correctly picks a stop. Next frame input returns and it bounces straight back
	// (:244). Measured in AZ.log 2026-08-27: [CmcTurn] complete at frames 209 / 317 / 921 -> a stop
	// selected at frames 210 / 318 / 922, each one returning to the loop 23-190 ms later.
	// Re-applying the still-held stick here means no frame exists in which the character is neither
	// turning nor accelerating. Double-adding is harmless if OnMoveTriggered also runs this frame —
	// ScaleInputAcceleration clamps the accumulated input vector to unit size.
	const UWorld* TurnWorld = GetWorld();
	const float TurnNow = TurnWorld ? static_cast<float>(TurnWorld->GetTimeSeconds()) : 0.f;
	const bool bStickStillHeld = !LastMoveInputDir.IsNearlyZero()
		&& (TurnNow - LastMoveInputTime) <= AZ::RmMontage::InputHeldWindow;
	if (bStickStillHeld)
	{
		AddMovementInput(LastMoveInputDir, 1.f);
	}

	UE_LOG(LogTemp, Display, TEXT("[CmcTurn] complete | residual snapped %+.1f deg | handback=%d"),
		Residual, bStickStillHeld ? 1 : 0);
}

UAnimSequence* AAZ_CmcHeroCharacter::EvaluateLocomotionChooser(UChooserTable* Table)
{
	if (!Table)
	{
		return nullptr;
	}

	ChooserGait = GetCurrentGait();
	ChooserStance = bIsCrouched ? EAZ_Stance::Crouching : EAZ_Stance::Standing;
	ChooserSpeed = GetCharacterMovement() ? GetCharacterMovement()->Velocity.Size2D() : 0.f;

	const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();
	ChooserRotationMode = HasMatchingGameplayTag(AZTags.Ability_State_Aiming) ? EAZ_RotationMode::Aiming
		: (HasMatchingGameplayTag(AZTags.Movement_Strafe) ? EAZ_RotationMode::Strafe
		                                                  : EAZ_RotationMode::OrientToMovement);

	if (const UAZ_CmcAnimInstance* CmcAnim =
		GetMesh() ? Cast<UAZ_CmcAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr)
	{
		bChooserLeftFootDown = CmcAnim->bLeftFootDown;
	}

	UObject* Result = UChooserFunctionLibrary::EvaluateChooser(this, Table, UAnimSequence::StaticClass());
	return Cast<UAnimSequence>(Result);
}

bool AAZ_CmcHeroCharacter::TryPlayRootMotionStop()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!bRootMotionStops || !Move || !MeshComp)
	{
		return false;
	}

	UAnimInstance* Anim = MeshComp->GetAnimInstance();
	if (!Anim || Anim->IsAnyMontagePlaying() || !Move->IsMovingOnGround())
	{
		return false;
	}

	const float Now = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
	if (LastRootMotionStopTime >= 0.f && (Now - LastRootMotionStopTime) < RootMotionStartCooldown)
	{
		return false;
	}

	const float EntrySpeed = Move->Velocity.Size2D();

	bool bLeftDown = true;
	if (const UAZ_CmcAnimInstance* FootAnim =
		GetMesh() ? Cast<UAZ_CmcAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr)
	{
		bLeftDown = FootAnim->bLeftFootDown;
	}
	bChooserLeftFootDown = bLeftDown;

	UAnimSequence* Clip = nullptr;

	ChooserDirection = EAZ_StartDirection::Fwd;
	if (UAnimSequence* Chosen = EvaluateLocomotionChooser(RootMotionStopChooser))
	{
		Clip = Chosen;
	}
	else
	{
		const EAZ_Gait Gait = GetCurrentGait();
		const FAZ_RootMotionStopClip* Row = RootMotionStopClips.FindByPredicate(
			[Gait, bLeftDown, EntrySpeed](const FAZ_RootMotionStopClip& E)
			{
				return E.Clip && E.Gait == Gait && E.bLeftFootDown == bLeftDown
					&& EntrySpeed >= E.MinEntrySpeed && EntrySpeed <= E.MaxEntrySpeed;
			});
		if (Row) { Clip = Row->Clip; }
	}

	if (!Clip)
	{
		return false;
	}

	UAnimMontage* Played = Anim->PlaySlotAnimationAsDynamicMontage(
		Clip, RootMotionStartSlot, RootMotionStartBlendIn, RootMotionStartBlendOut, 1.f, 1);
	if (!Played)
	{
		return false;
	}

	LastRootMotionStopTime = Now;
	bRootMotionStopActive = true;
	AZ::RmMontage::bActive = true;
	AZ::RmMontage::LaunchTime = Now;
	AZ::RmMontage::bIsStop = true;
	AZ::RmMontage::LaunchDir = FVector::ZeroVector;

	UE_LOG(LogTemp, Display,
		TEXT("[CmcRmStop] %s | entry=%.0f leftFoot=%d gait=%d dur=%.2fs"),
		*GetNameSafe(Clip), EntrySpeed, bLeftDown ? 1 : 0,
		static_cast<int32>(GetCurrentGait()), Clip->GetPlayLength());
	return true;
}

void AAZ_CmcHeroCharacter::UpdateTurnInPlaceLock(float DeltaSeconds)
{
	if (!bTipLockActive)
	{
		return;
	}
	TipLockElapsed += DeltaSeconds;

	const float Now = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
	if (Now - LastMoveInputTime > 0.1f)
	{
		ReleaseTurnInPlaceLock(TEXT("input released"),  false);
		return;
	}

	const float LiveYaw = static_cast<float>(LastMoveInputDir.Rotation().Yaw);
	if (FMath::Abs(FRotator::NormalizeAxis(LiveYaw - TipTargetYaw)) > TurnInPlaceRetargetAngle)
	{
		UE_LOG(LogTemp, Display, TEXT("[CmcTip] RELATCH %.0f -> %.0f (t=%.2f)"),
			TipTargetYaw, LiveYaw, TipLockElapsed);
		TipTargetYaw = LiveYaw;
		TipLockElapsed = 0.f;
	}

	const UAZ_CmcAnimInstance* CmcAnim =
		GetMesh() ? Cast<UAZ_CmcAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr;
	if (!CmcAnim)
	{
		ReleaseTurnInPlaceLock(TEXT("no anim instance"), false);
		return;
	}

	const float RemainingYaw = FMath::Abs(FRotator::NormalizeAxis(TipTargetYaw - CmcAnim->GetTipRootYaw()));
	const bool bTipSelected = CmcAnim->IsTurnInPlaceClipSelected();

	if (RemainingYaw <= TurnInPlaceExitAngle)
	{
		ReleaseTurnInPlaceLock(TEXT("root arrived"), true);
	}
	else if (bTipSelected && CmcAnim->GetTurnInPlaceClipFraction() >= 0.7f)
	{
		ReleaseTurnInPlaceLock(TEXT("clip done"), true);
	}
	else if (!bTipSelected && TipLockElapsed > 0.35f)
	{
		ReleaseTurnInPlaceLock(TEXT("no selection"), false);
	}
	else if (TipLockElapsed >= TurnInPlaceTimeout)
	{
		ReleaseTurnInPlaceLock(TEXT("WATCHDOG"), true);
	}
}

void AAZ_CmcHeroCharacter::ReleaseTurnInPlaceLock(const TCHAR* Reason, bool bSnapCapsule)
{
	const UAZ_CmcAnimInstance* CmcAnim =
		GetMesh() ? Cast<UAZ_CmcAnimInstance>(GetMesh()->GetAnimInstance()) : nullptr;

	if (bSnapCapsule && bTurnInPlaceSnapCapsuleOnRelease && CmcAnim)
	{
		SetActorRotation(FRotator(0.f, CmcAnim->GetTipRootYaw(), 0.f));
	}

	UE_LOG(LogTemp, Display,
		TEXT("[CmcTip] RELEASE (%s) t=%.2f remaining=%.0f clipFrac=%.2f snap=%d"),
		Reason, TipLockElapsed,
		CmcAnim ? FMath::Abs(FRotator::NormalizeAxis(TipTargetYaw - CmcAnim->GetTipRootYaw())) : -1.f,
		CmcAnim ? CmcAnim->GetTurnInPlaceClipFraction() : -1.f,
		bSnapCapsule ? 1 : 0);

	bTipLockActive = false;
	TipLockElapsed = 0.f;
	TipEnterCandidateFrames = 0;
}

void AAZ_CmcHeroCharacter::OnLookTriggered(const FInputActionValue& Value)
{
	if (HasMatchingGameplayTag(FAZ_GameplayTags::Get().State_Grabbed))
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X * LookRateYaw);
	AddControllerPitchInput(-Axis.Y * LookRatePitch);
}
