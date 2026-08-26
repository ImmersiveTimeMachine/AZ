
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
	if (bStopCurveBraking && bStopActive && bStopIsAnimated && DeltaSeconds > KINDA_SMALL_NUMBER)
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
