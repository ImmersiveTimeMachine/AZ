#include "Character/AZ_HeroPawn.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "Character/AZ_PawnCameraMovementComponent.h"
#include "Equipment/AZ_EquipmentManagerComponent.h"
#include "Player/AZ_PlayerState.h"

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "DefaultMovementSet/LayeredMoves/RootMotionAttributeLayeredMove.h"
#include "MoverDataModelTypes.h"
#include "MoveLibrary/BasedMovementUtils.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "MoverPoseSearchTrajectoryPredictor.h"
#include "Animation/AZ_AnimInstance.h"
#include "NetworkPredictionComponent.h"
#include "Components/AudioComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

// GASP DDCvar.ControlStyle — 0 = standard third-person, 1 = twin-stick.
// Read by Update_TwinStickMode each tick. Set via console (`DDCvar.ControlStyle 1`).
static TAutoConsoleVariable<int32> CVarDDControlStyle(
	TEXT("DDCvar.ControlStyle"),
	0,
	TEXT("GASP Control Style. 0 = standard third-person, 1 = twin-stick."),
	ECVF_Default);

// GASP DDCvar.StrafeStyle — selects sprint-while-strafing dot-product threshold.
// 0 → 0.5 (must move mostly toward orientation to sprint).
// 1 or 2 → -0.1 (sprint allowed unless moving sharply backward).
// Read by Get_Gait Strafe branch.
static TAutoConsoleVariable<int32> CVarDDStrafeStyle(
	TEXT("DDCvar.StrafeStyle"),
	0,
	TEXT("GASP Strafe Style. Selects sprint dot-threshold: 0 = 0.5, 1/2 = -0.1."),
	ECVF_Default);

// GASP DDCvar.AnalogInputStyle — controls default-gait selection (no sprint, no walk).
// 0 → Run (keyboard default).
// 1 → Analog: IA_Move stick magnitude > 0.8 → Run else Walk.
// Read by Get_Gait fallthrough branch.
static TAutoConsoleVariable<int32> CVarDDAnalogInputStyle(
	TEXT("DDCvar.AnalogInputStyle"),
	0,
	TEXT("GASP Analog Input Style. 0 = keyboard (default Run), 1 = analog (deflection-controlled Run/Walk)."),
	ECVF_Default);

AAZ_HeroPawn::AAZ_HeroPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// --- Tick ---
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	// --- Replication ---
	bReplicates = true;
	SetReplicatingMovement(false); // Mover handles movement replication

	// --- Rotation: Mover handles orientation via OrientationIntent, not controller ---
	// false = character faces movement direction (OrientToMovement), camera orbits freely
	// This enables turn-in-place when idle (FutureFacingDelta builds up as camera rotates)
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// --- Collision root (capsule) — match old: radius 25, half-height 90 ---
	UCapsuleComponent* CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCapsule"));
	CapsuleComp->InitCapsuleSize(25.f, 90.f);
	CapsuleComp->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	CapsuleComp->SetCanEverAffectNavigation(true);
	CapsuleComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CapsuleComp->SetGenerateOverlapEvents(false);
	SetRootComponent(CapsuleComp);

	// --- Skeletal Mesh — match old: offset -90 (half-height), collision channels ---
	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CapsuleComp);
	MeshComponent->SetRelativeLocation(FVector(0.f, 0.f, -92.f));
	MeshComponent->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	MeshComponent->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	MeshComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	MeshComponent->SetGenerateOverlapEvents(true);
	MeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// --- Mover Component (replaces CharacterMovementComponent) ---
	CharacterMoverComponent = CreateDefaultSubobject<UCharacterMoverComponent>(TEXT("CharacterMoverComponent"));
	CharacterMoverComponent->SetUpdatedComponent(CapsuleComp);
	CharacterMoverComponent->SetHandleJump(true);
	CharacterMoverComponent->SetHandleStanceChanges(true);
	CharacterMoverComponent->PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// --- Network Prediction (required for networked Mover replication) ---
	NetworkPredictionComponent = CreateDefaultSubobject<UNetworkPredictionComponent>(TEXT("NetworkPredictionComponent"));

	// --- Camera — match old character values ---
	ThirdPersonCameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("ThirdPersonCameraBoom"));
	ThirdPersonCameraBoom->SetupAttachment(CapsuleComp);
	ThirdPersonCameraBoom->TargetArmLength = 220.f;
	ThirdPersonCameraBoom->SocketOffset = FVector(0.f, 70.f, 0.f);
	ThirdPersonCameraBoom->bUsePawnControlRotation = true;
	ThirdPersonCameraBoom->bEnableCameraLag = true;
	ThirdPersonCameraBoom->CameraLagSpeed = 8.f;
	ThirdPersonCameraBoom->CameraLagMaxDistance = 50.f;
	ThirdPersonCameraBoom->bEnableCameraRotationLag = true;
	ThirdPersonCameraBoom->CameraRotationLagSpeed = 10.f;
	ThirdPersonCameraBoom->bDoCollisionTest = true;
	ThirdPersonCameraBoom->ProbeSize = 12.f;
	ThirdPersonCameraBoom->ProbeChannel = ECC_Camera;

	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
	ThirdPersonCamera->SetupAttachment(ThirdPersonCameraBoom, USpringArmComponent::SocketName);
	ThirdPersonCamera->bUsePawnControlRotation = false;

	// --- Motion Matching Trajectory (Mover-native, replaces CharacterTrajectoryComponent) ---
	MoverTrajectoryPredictor = CreateDefaultSubobject<UMoverTrajectoryPredictor>(TEXT("MoverTrajectoryPredictor"));

	// --- Equipment ---
	EquipmentManagerComponent = CreateDefaultSubobject<UAZ_EquipmentManagerComponent>(TEXT("EquipmentManager"));

	// --- Camera + Movement State Manager ---
	CameraMovementComponent = CreateDefaultSubobject<UAZ_PawnCameraMovementComponent>(TEXT("CameraMovementComponent"));

	// --- Default attribute set class ---
	DefaultAttributeSetClass = nullptr;

	// --- MovementModeMap (GASP parity) ---
	// Mover plugin mode FNames → AZ enum. BP can override in editor.
	MovementModeMap.Add(FName(TEXT("Walking")), EAZ_MovementMode::OnGround);
	MovementModeMap.Add(FName(TEXT("Falling")), EAZ_MovementMode::InAir);
	MovementModeMap.Add(FName(TEXT("Sliding")), EAZ_MovementMode::Slide);
	MovementModeMap.Add(FName(TEXT("Flying")),  EAZ_MovementMode::Traversing);
}

// ============================================================
// Lifecycle
// ============================================================

void AAZ_HeroPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// The Mover component may have been added in BP; find it if not created in constructor
	if (!CharacterMoverComponent)
	{
		CharacterMoverComponent = FindComponentByClass<UCharacterMoverComponent>();
	}
}

void AAZ_HeroPawn::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->PlayerCameraManager->ViewPitchMax = 89.f;
		PC->PlayerCameraManager->ViewPitchMin = -89.f;
	}

	// Fallback: align controller yaw with actor if the pawn was already possessed
	// before BeginPlay fired (PIE default pawn, streaming cases). The primary path
	// is PossessedBy — this just covers the race where possession preceded BeginPlay.
	AlignControllerWithActor();

	if (CharacterMoverComponent)
	{
		// Register this pawn as the Mover's input producer
		CharacterMoverComponent->InputProducer = this;

		// GASP parity: force Mover to tick BEFORE this pawn so GetLastInputCmd()
		// returns valid data in our Tick's CacheInputsFromMover() call.
		// UE component-before-owner ordering usually gives this implicitly, but
		// the explicit prerequisite matches GASP's BeginPlay and makes the
		// dependency robust against future tick-group changes.
		AddTickPrerequisiteComponent(CharacterMoverComponent);

		// Lazy-create the predictor if the BP CDO nulled it out (UCLASS EditInlineNew
		// makes BP serialize this property as an instanced subobject, which can
		// override the C++ CreateDefaultSubobject default with null).
		if (!MoverTrajectoryPredictor)
		{
			MoverTrajectoryPredictor = NewObject<UMoverTrajectoryPredictor>(this, TEXT("MoverTrajectoryPredictor_Runtime"));
		}

		// Wire up Mover trajectory predictor for PoseSearch
		if (MoverTrajectoryPredictor)
		{
			MoverTrajectoryPredictor->Setup(CharacterMoverComponent);
		}

		// Phase 6: bind movement-mode change delegate. Initialize current mode
		// so the first observed change has a sensible "previous" value.
		CharacterMoverComponent->OnMovementModeChanged.AddDynamic(this, &AAZ_HeroPawn::HandleMovementModeChanged);
		CurrentMovementModeName  = CharacterMoverComponent->GetMovementModeName();
		PreviousMovementModeName = CurrentMovementModeName;
	}
}

void AAZ_HeroPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Populate thread-safe state proxy for AnimInstance (game thread only)
	if (CharacterMoverComponent)
	{
		MoverStateProxy.Velocity = CharacterMoverComponent->GetVelocity();
		MoverStateProxy.GroundSpeed = FVector(MoverStateProxy.Velocity.X, MoverStateProxy.Velocity.Y, 0.f).Size();
		MoverStateProxy.bIsOnGround = CharacterMoverComponent->IsOnGround();
		MoverStateProxy.bIsFalling = CharacterMoverComponent->IsFalling();
		MoverStateProxy.bIsCrouching = CharacterMoverComponent->IsCrouching();

		// GASP Update_FloorValues parity: cache impact normal+point for systems that
		// need floor info (foot IK / slope warping / VFX). Fallback to mesh world
		// location + world-up when no floor hit (matches GASP).
		FHitResult FloorHit;
		if (CharacterMoverComponent->TryGetFloorCheckHitResult(FloorHit))
		{
			FloorNormal = FloorHit.ImpactNormal;
			FloorLocation = FloorHit.ImpactPoint;
		}
		else
		{
			FloorNormal = FVector::UpVector;
			FloorLocation = MeshComponent ? MeshComponent->GetComponentLocation() : FVector::ZeroVector;
		}

		// Mirror onto MoverStateProxy so AnimInstance worker-thread reads stay valid.
		MoverStateProxy.GroundNormal = FloorNormal;
		MoverStateProxy.GroundLocation = FloorLocation;
	}

	if (AZ_AbilitySystemComponent)
	{
		const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();
		MoverStateProxy.bIsSprinting = AZ_AbilitySystemComponent->HasMatchingGameplayTag(AZTags.Ability_State_Sprinting);
		MoverStateProxy.bIsAiming = AZ_AbilitySystemComponent->HasMatchingGameplayTag(AZTags.Ability_State_Aiming);
	}

	// Phase 5: per-tick control rotation rate (for camera-whip detection / GAS).
	Update_ControlRotationRate(DeltaTime);

	// GASP parity: drive Slide.Loop audio's `Speed` parameter from current velocity
	// (no-op while SlidingAudioComponent is null — i.e. not sliding or no AC_FoleyEvents yet).
	Update_SlidingAudio();

	// GASP parity: targeting + twin-stick aim mode (drive Get_RotationMode /
	// Get_AimingRotation Tier-1/Tier-2 paths). Both no-op when their inputs are
	// inactive (empty TargetableActors, ControlStyle != 1).
	// Order matches GASP EventGraph Tick: Update_TargetedActor → Update_TwinStickMode.
	Update_TargetedActor();
	Update_TwinStickMode();

	// GASP parity for content, deliberate ORDER deviation: GASP calls
	// CacheInputsFromMover at the START of Tick (using prior-frame Mover output).
	// AZ calls it at the END so newly-packed PreSim cmds from this frame's
	// OnProduceInput → Mover sim are reflected in PostSim before next Tick.
	// AddTickPrerequisiteComponent on the Mover keeps the data valid here either way.
	CacheInputsFromMover();

	// Phase 7: optional on-screen debug overlay (gated by bShowPawnDebug).
	DebugDraws();
}

void AAZ_HeroPawn::CacheInputsFromMover()
{
	if (!CharacterMoverComponent) return;

	const FMoverInputCmdContext& LastCmd = CharacterMoverComponent->GetLastInputCmd();

	if (const FCharacterDefaultInputs* CharInputs = LastCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>())
	{
		MoverDefaultInputs_PostSim = *CharInputs;
	}
	if (const FAZ_MoverCustomInputs* CustomInputs = LastCmd.InputCollection.FindDataByType<FAZ_MoverCustomInputs>())
	{
		MoverCustomInputs_PostSim = *CustomInputs;
	}
}

// ============================================================
// GASP Derivation Getters (Phase 2)
// ============================================================
// Each mirrors a GASP SandboxCharacter_Mover Get_* function. Consumed by
// OnPreSimulateTick (Phase 3) to pack MoverDefaultInputs_PreSim and
// MoverCustomInputs_PreSim before feeding the Mover.

FVector AAZ_HeroPawn::Get_MoveInput() const
{
	// GASP player-path: yaw-only rotation of local stick, clamped to unit, normalized.
	// AI NavMovement branch deferred until AZ adds AI pawns from this class.
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return FVector::ZeroVector;

	FVector Clamped = CachedMoveInputIntent;
	if (Clamped.SizeSquared() > 1.f) Clamped.Normalize();

	const FRotator YawOnly(0.f, PC->GetControlRotation().Yaw, 0.f);
	return YawOnly.RotateVector(Clamped).GetSafeNormal();
}

FRotator AAZ_HeroPawn::Get_AimingRotation() const
{
	// GASP three-tier dispatch (parity with SandboxCharacter_Mover.Get_AimingRotation):
	// Tier 1 — TargetedActor: rotation from self → target.
	// Tier 2 — TwinStickMode: cached TwinStickAimRotation.
	// Tier 3 — default: control rotation (camera).
	if (TargetedActor)
	{
		const FVector ToTarget = TargetedActor->GetActorLocation() - GetActorLocation();
		return UKismetMathLibrary::MakeRotFromX(ToTarget);
	}

	if (TwinStickMode)
	{
		return TwinStickAimRotation;
	}

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		return PC->GetControlRotation();
	}
	return FRotator::ZeroRotator;
}

FVector2D AAZ_HeroPawn::GetTwinStickAimDirection() const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !TwinStickAimDirectionInputAction) return FVector2D::ZeroVector;

	const UEnhancedPlayerInput* EnhancedInput = Cast<UEnhancedPlayerInput>(PC->PlayerInput);
	if (!EnhancedInput) return FVector2D::ZeroVector;

	return EnhancedInput->GetActionValue(TwinStickAimDirectionInputAction).Get<FVector2D>();
}

double AAZ_HeroPawn::Get_Speed() const
{
	if (!CharacterMoverComponent) return 0.0;
	const FVector V = CharacterMoverComponent->GetVelocity();
	return FVector(V.X, V.Y, 0.f).Size();
}

EAZ_MovementMode AAZ_HeroPawn::Get_CurrentMovementMode() const
{
	if (!CharacterMoverComponent) return EAZ_MovementMode::OnGround;
	const FName ModeName = CharacterMoverComponent->GetMovementModeName();
	if (const EAZ_MovementMode* Found = MovementModeMap.Find(ModeName))
	{
		return *Found;
	}
	return EAZ_MovementMode::OnGround;
}

EAZ_RotationMode AAZ_HeroPawn::Get_RotationMode() const
{
	// GASP three-tier dispatch (parity with SandboxCharacter_Mover.Get_RotationMode):
	// Tier 1 — TargetedActor valid: force Aim if WantsToAim else Strafe.
	// Tier 2 — TwinStickMode AND right stick deflected: force Aim if WantsToAim else Strafe.
	//          Right-stick centered while in TwinStickMode → OrientToMovement (free move).
	// Tier 3 — default: PlayerInputState bools (Aim wins over Strafe).
	if (TargetedActor)
	{
		return PlayerInputState.bWantsToAim ? EAZ_RotationMode::Aiming
		                                    : EAZ_RotationMode::Strafe;
	}

	if (TwinStickMode)
	{
		if (!GetTwinStickAimDirection().IsNearlyZero())
		{
			return PlayerInputState.bWantsToAim ? EAZ_RotationMode::Aiming
			                                    : EAZ_RotationMode::Strafe;
		}
		return EAZ_RotationMode::OrientToMovement;
	}

	if (PlayerInputState.bWantsToAim)    return EAZ_RotationMode::Aiming;
	if (PlayerInputState.bWantsToStrafe) return EAZ_RotationMode::Strafe;
	return EAZ_RotationMode::OrientToMovement;
}

EAZ_Gait AAZ_HeroPawn::Get_Gait() const
{
	// GASP-parity Get_Gait (full body, including DDCvar branches).
	//
	// WantsToSprint:
	//   OrientToMovement → Sprint
	//   Strafe → DotProduct(MoveInput, OrientationIntent) > Threshold → Sprint else Run
	//            Threshold from `DDCvar.StrafeStyle` Select: 0 → 0.5, else → -0.1.
	//   Aiming → Run (sprint cap)
	// !WantsToSprint, WantsToWalk → Walk
	// !WantsToSprint, !WantsToWalk → check `DDCvar.AnalogInputStyle`:
	//   0 → Run (keyboard default — GASP baseline)
	//   1 → if IA_Move stick magnitude > 0.8 → Run else Walk

	if (PlayerInputState.bWantsToSprint)
	{
		const EAZ_RotationMode RotMode = Get_RotationMode();
		switch (RotMode)
		{
		case EAZ_RotationMode::OrientToMovement:
			return EAZ_Gait::Sprint;

		case EAZ_RotationMode::Strafe:
		{
			const FVector MoveDir = MoverDefaultInputs_PreSim.GetMoveInput().GetSafeNormal();
			const FVector OrientDir = MoverDefaultInputs_PreSim.OrientationIntent.GetSafeNormal();
			const double Dot = FVector::DotProduct(MoveDir, OrientDir);
			// GASP Select: StrafeStyle 0 → 0.5; StrafeStyle 1 or 2 (or anything else) → -0.1.
			const int32 StrafeStyle = CVarDDStrafeStyle.GetValueOnGameThread();
			const double Threshold = (StrafeStyle == 0) ? 0.5 : -0.1;
			return (Dot > Threshold) ? EAZ_Gait::Sprint : EAZ_Gait::Run;
		}

		case EAZ_RotationMode::Aiming:
			// Sprint cap when aiming.
			return EAZ_Gait::Run;

		default:
			return EAZ_Gait::Run;
		}
	}

	if (PlayerInputState.bWantsToWalk)
	{
		return EAZ_Gait::Walk;
	}

	// Fallthrough: GASP DDCvar.AnalogInputStyle decides default gait.
	const int32 AnalogStyle = CVarDDAnalogInputStyle.GetValueOnGameThread();
	if (AnalogStyle == 1)
	{
		// Analog branch: read IA_Move stick magnitude, > 0.8 → Run else Walk.
		if (const APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (const UEnhancedPlayerInput* EPI = Cast<UEnhancedPlayerInput>(PC->PlayerInput))
			{
				if (MoveInputAction)
				{
					const FVector2D Stick = EPI->GetActionValue(MoveInputAction).Get<FVector2D>();
					return (Stick.Size() > 0.8) ? EAZ_Gait::Run : EAZ_Gait::Walk;
				}
			}
		}
		return EAZ_Gait::Walk; // Fallback if no PC/IA wired.
	}

	// AZ keyboard default: Walk (inverted from GASP — Shift held → Sprint, otherwise Walk).
	// Reasoning: project_input_stack_rt_mirror.md — "Get_Gait inverted (Walk=default, Shift=Sprint)".
	return EAZ_Gait::Walk;
}

FVector AAZ_HeroPawn::Get_OrientationIntent() const
{
	// GASP per-mode matrix (OnGround/InAir/Slide/Traversing × moving/idle × RotationMode).
	// "Pass-through last value" uses MoverDefaultInputs_PreSim.OrientationIntent —
	// the last OrientationIntent we packed (cache hysteresis in GASP's TIP pattern).
	const FVector LastOrient = MoverDefaultInputs_PreSim.OrientationIntent;
	const FRotator Aiming = Get_AimingRotation();
	const FVector AimingFwd = FRotator(0.f, Aiming.Yaw, 0.f).Vector();
	const FVector MoveVec = Get_MoveInput();
	const bool bMoving = !MoveVec.IsNearlyZero();
	const EAZ_MovementMode MoveMode = Get_CurrentMovementMode();
	const EAZ_RotationMode RotMode = Get_RotationMode();

	switch (MoveMode)
	{
	case EAZ_MovementMode::OnGround:
		if (bMoving)
		{
			// Moving on ground.
			switch (RotMode)
			{
			case EAZ_RotationMode::OrientToMovement: return MoveVec;
			case EAZ_RotationMode::Strafe:           return AimingFwd;
			case EAZ_RotationMode::Aiming:           return AimingFwd;
			default:                                 return MoveVec;
			}
		}
		else
		{
			// Idle on ground — GASP threshold-cache pattern (Get_OrientationIntent comment
			// nodes: "WITHOUT movement input + OrientToMovement/Strafe → keep last frame";
			// "WITHOUT movement input + Aim → switch to AimingRotation when 60° offset".
			switch (RotMode)
			{
			case EAZ_RotationMode::OrientToMovement:
				return LastOrient;
			case EAZ_RotationMode::Strafe:
				return LastOrient;
			case EAZ_RotationMode::Aiming:
			{
				const FRotator ActorRot = GetActorRotation();
				const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(Aiming, ActorRot);
				return (FMath::Abs(Delta.Yaw) > 60.0) ? AimingFwd : LastOrient;
			}
			default:
				return LastOrient;
			}
		}

	case EAZ_MovementMode::InAir:
		switch (RotMode)
		{
		case EAZ_RotationMode::OrientToMovement: return LastOrient;
		case EAZ_RotationMode::Strafe:           return AimingFwd;
		case EAZ_RotationMode::Aiming:           return AimingFwd;
		default:                                 return LastOrient;
		}

	case EAZ_MovementMode::Slide:
		if (RotMode == EAZ_RotationMode::OrientToMovement)
		{
			if (CharacterMoverComponent)
			{
				const FVector V = CharacterMoverComponent->GetVelocity();
				return FVector(V.X, V.Y, 0.f).GetSafeNormal();
			}
			return LastOrient;
		}
		return AimingFwd;

	case EAZ_MovementMode::Traversing:
		return GetActorForwardVector();

	default:
		return LastOrient;
	}
}

// ============================================================
// Controller / PlayerState — GAS Init
// ============================================================

void AAZ_HeroPawn::AlignControllerWithActor()
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetControlRotation(GetActorRotation());
	}
}

void AAZ_HeroPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Primary: align control rotation now that a controller is guaranteed.
	// Prevents trajectory predictor seeing a spawn-time delta that makes
	// ShouldTurnInPlace() fire while SM=IdleLoop → A-pose.
	AlignControllerWithActor();

	// Server-side: grab ASC from PlayerState
	if (AAZ_PlayerState* PS = GetPlayerState<AAZ_PlayerState>())
	{
		AZ_AbilitySystemComponent = Cast<UAZ_AbilitySystemComponent>(PS->GetAbilitySystemComponent());

		if (AZ_AbilitySystemComponent)
		{
			AZ_AbilitySystemComponent->InitAbilityActorInfo(PS, this);

			// Create attribute set
			if (DefaultAttributeSetClass)
			{
				AttributeSet = NewObject<UAttributeSet>(this, DefaultAttributeSetClass);
				AZ_AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());
			}

			InitDefaultAttributes();
			InitDefaultAbilities();

			OnAscRegistered.Broadcast(AZ_AbilitySystemComponent.Get());
		}
	}
}

void AAZ_HeroPawn::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Client-side: grab ASC from PlayerState
	if (AAZ_PlayerState* PS = GetPlayerState<AAZ_PlayerState>())
	{
		AZ_AbilitySystemComponent = Cast<UAZ_AbilitySystemComponent>(PS->GetAbilitySystemComponent());

		if (AZ_AbilitySystemComponent)
		{
			AZ_AbilitySystemComponent->InitAbilityActorInfo(PS, this);

			if (DefaultAttributeSetClass && !AttributeSet)
			{
				AttributeSet = NewObject<UAttributeSet>(this, DefaultAttributeSetClass);
				AZ_AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());
			}

			OnAscRegistered.Broadcast(AZ_AbilitySystemComponent.Get());
		}
	}
}

// ============================================================
// IAbilitySystemInterface / CombatInterface
// ============================================================

UAbilitySystemComponent* AAZ_HeroPawn::GetAbilitySystemComponent() const
{
	return AZ_AbilitySystemComponent.Get();
}

UAbilitySystemComponent* AAZ_HeroPawn::GetASC() const
{
	return AZ_AbilitySystemComponent.Get();
}

FOnASCRegistered& AAZ_HeroPawn::GetOnASCRegisteredDelegate()
{
	return OnAscRegistered;
}

// ============================================================
// GAS Helpers
// ============================================================

void AAZ_HeroPawn::InitDefaultAttributes()
{
	ApplyEffectToSelf(DefaultPrimaryAttributes, 1.f);
	ApplyEffectToSelf(DefaultSecondaryAttributes, 1.f);
	ApplyEffectToSelf(DefaultVitalAttributes, 1.f);
}

void AAZ_HeroPawn::ApplyEffectToSelf(const TSubclassOf<UGameplayEffect>& GameplayEffectClass, float Level) const
{
	if (!GameplayEffectClass || !AZ_AbilitySystemComponent)
	{
		return;
	}

	FGameplayEffectContextHandle ContextHandle = AZ_AbilitySystemComponent->MakeEffectContext();
	ContextHandle.AddSourceObject(this);
	const FGameplayEffectSpecHandle SpecHandle = AZ_AbilitySystemComponent->MakeOutgoingSpec(GameplayEffectClass, Level, ContextHandle);
	AZ_AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void AAZ_HeroPawn::InitDefaultAbilities() const
{
	if (!AZ_AbilitySystemComponent || !HasAuthority())
	{
		return;
	}

	UObject* SourceObject = const_cast<AAZ_HeroPawn*>(this);

	for (const TSubclassOf<UAZ_GameplayAbility>& AbilityClass : StartupAbilities)
	{
		FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, SourceObject);
		AZ_AbilitySystemComponent->GiveAbility(Spec);
	}

	for (const TSubclassOf<UAZ_GameplayAbility>& AbilityClass : StartupPassiveAbilities)
	{
		FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, SourceObject);
		AZ_AbilitySystemComponent->GiveAbility(Spec);
	}

	// Input-bound abilities — grant with InputTag so controller can bind them
	AZ_AbilitySystemComponent->GrantAbilitiesWithInputTag(CharacterInputAbilities);
}

// ============================================================
// Enhanced Input
// ============================================================

void AAZ_HeroPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		Input->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AAZ_HeroPawn::OnMoveTriggered);
		Input->BindAction(MoveInputAction, ETriggerEvent::Completed, this, &AAZ_HeroPawn::OnMoveCompleted);
		Input->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AAZ_HeroPawn::OnLookTriggered);
		Input->BindAction(LookInputAction, ETriggerEvent::Completed, this, &AAZ_HeroPawn::OnLookCompleted);
		// GASP IA_Look_Gamepad — rate-based look (multiplied by world delta-seconds).
		if (LookGamepadInputAction)
		{
			Input->BindAction(LookGamepadInputAction, ETriggerEvent::Triggered, this, &AAZ_HeroPawn::OnLookGamepadTriggered);
		}
		Input->BindAction(JumpInputAction, ETriggerEvent::Started, this, &AAZ_HeroPawn::OnJumpStarted);
		Input->BindAction(JumpInputAction, ETriggerEvent::Completed, this, &AAZ_HeroPawn::OnJumpReleased);

		// GASP state-bool toggles — held-action pattern. Started=press, Completed=release.
		// bWantsToX flags are consumed by OnProduceInput to derive Gait / RotationMode.
		// (Crouch/Aim intentionally stay on GAS; not bound here.)
		if (SprintInputAction)
		{
			Input->BindAction(SprintInputAction, ETriggerEvent::Started,   this, &AAZ_HeroPawn::OnSprintStarted);
			Input->BindAction(SprintInputAction, ETriggerEvent::Completed, this, &AAZ_HeroPawn::OnSprintReleased);
		}
		if (WalkInputAction)
		{
			Input->BindAction(WalkInputAction,   ETriggerEvent::Started,   this, &AAZ_HeroPawn::OnWalkStarted);
			Input->BindAction(WalkInputAction,   ETriggerEvent::Completed, this, &AAZ_HeroPawn::OnWalkReleased);
		}
		if (StrafeInputAction)
		{
			Input->BindAction(StrafeInputAction, ETriggerEvent::Started,   this, &AAZ_HeroPawn::OnStrafeStarted);
			Input->BindAction(StrafeInputAction, ETriggerEvent::Completed, this, &AAZ_HeroPawn::OnStrafeReleased);
		}
	}
}

void AAZ_HeroPawn::OnMoveTriggered(const FInputActionValue& Value)
{
	// Enhanced Input IA_Move: X = Right/Left (A/D), Y = Forward/Back (W/S)
	// RotateVector expects: X = Forward, Y = Right
	const FVector2D Input2D = Value.Get<FVector2D>();
	CachedMoveInputIntent.X = FMath::Clamp(Input2D.Y, -1.f, 1.f);  // Forward/Back
	CachedMoveInputIntent.Y = FMath::Clamp(Input2D.X, -1.f, 1.f);  // Right/Left
	CachedMoveInputIntent.Z = 0.f;

}

void AAZ_HeroPawn::OnMoveCompleted(const FInputActionValue& Value)
{
	CachedMoveInputIntent = FVector::ZeroVector;
}

void AAZ_HeroPawn::OnLookTriggered(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookVector.X * LookRateYaw);
	AddControllerPitchInput(-LookVector.Y * LookRatePitch);
}

void AAZ_HeroPawn::OnLookCompleted(const FInputActionValue& Value)
{
	// No-op — look is applied immediately in OnLookTriggered
}

void AAZ_HeroPawn::OnLookGamepadTriggered(const FInputActionValue& Value)
{
	// GASP IA_Look_Gamepad parity:
	//   if (!TwinStickMode) → AddControllerYawInput(Stick.X * DeltaSeconds);
	//                         AddControllerPitchInput(Stick.Y * DeltaSeconds);
	// Pitch is NOT negated — GASP relies on the IA's `Negate Y` input modifier
	// to flip the axis upstream. Mirror the same modifier on AZ's IA asset.
	if (TwinStickMode)
	{
		return;
	}

	const FVector2D Stick = Value.Get<FVector2D>();
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float DeltaSeconds = World->GetDeltaSeconds();
	AddControllerYawInput(Stick.X * DeltaSeconds);
	AddControllerPitchInput(Stick.Y * DeltaSeconds);
}

void AAZ_HeroPawn::OnJumpStarted(const FInputActionValue& Value)
{
	bIsJumpJustPressed = !bIsJumpPressed;
	bIsJumpPressed = true;
}

void AAZ_HeroPawn::OnJumpReleased(const FInputActionValue& Value)
{
	bIsJumpPressed = false;
	bIsJumpJustPressed = false;
}

// ============================================================
// IMoverInputProducerInterface — ProduceInput
// ============================================================

void AAZ_HeroPawn::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	OnProduceInput(static_cast<float>(SimTimeMs), InputCmdResult);
}

void AAZ_HeroPawn::OnProduceInput(float DeltaMs, FMoverInputCmdContext& InputCmdResult)
{
	// === GASP SandboxCharacter_Mover OnPreSimulateTick parity ===
	// Pack PreSim copies via derivation getters, apply movement-base transform,
	// then copy into the actual InputCmdResult. PostSim copies are pulled later
	// by CacheInputsFromMover in Tick (already wired in Phase 1).
	if (!GetController())
	{
		return;
	}

	// (1) Update idle-TIP accumulator BEFORE Get_OrientationIntent reads it.
	Update_IdleTIPAccumulator();

	// (2) Pack MoverDefaultInputs_PreSim via GASP getters.
	MoverDefaultInputs_PreSim.ControlRotation = Get_AimingRotation();
	MoverDefaultInputs_PreSim.SetMoveInput(EMoveInputType::DirectionalIntent, Get_MoveInput());
	MoverDefaultInputs_PreSim.OrientationIntent = Get_OrientationIntent();
	MoverDefaultInputs_PreSim.bIsJumpPressed = bIsJumpPressed;
	MoverDefaultInputs_PreSim.bIsJumpJustPressed = bIsJumpJustPressed;
	MoverDefaultInputs_PreSim.SuggestedMovementMode = NAME_None;
	MoverDefaultInputs_PreSim.bUsingMovementBase = false;

	// (3) Pack MoverCustomInputs_PreSim. RotationMode packed FIRST so Get_Gait
	//     (which derives RotationMode internally) produces a consistent answer.
	MoverCustomInputs_PreSim.RotationMode = Get_RotationMode();
	MoverCustomInputs_PreSim.Gait = Get_Gait();
	MoverCustomInputs_PreSim.bWantsToCrouch = PlayerInputState.bWantsToCrouch;

	// Phase 4: MovementDirection + RotationOffset.
	{
		EAZ_MovementDirection MD;
		double Offset = 0.0;
		Get_MovementDirectionAndOffset(MD, Offset);
		MoverCustomInputs_PreSim.MovementDirection = MD;
		MoverCustomInputs_PreSim.RotationOffset = Offset;
	}

	// Phase 5: ControlRotationRate (updated each Tick).
	MoverCustomInputs_PreSim.ControlRotationRate = ControlRotationRate;

	// (4) Convert MoveInput / OrientationIntent to base-relative if on a moving platform.
	if (CharacterMoverComponent)
	{
		if (UPrimitiveComponent* MovementBase = CharacterMoverComponent->GetMovementBase())
		{
			const FName BaseBoneName = CharacterMoverComponent->GetMovementBaseBoneName();
			FVector RelativeMove, RelativeOrient;

			UBasedMovementUtils::TransformWorldDirectionToBased(MovementBase, BaseBoneName, MoverDefaultInputs_PreSim.GetMoveInput(), RelativeMove);
			UBasedMovementUtils::TransformWorldDirectionToBased(MovementBase, BaseBoneName, MoverDefaultInputs_PreSim.OrientationIntent, RelativeOrient);

			MoverDefaultInputs_PreSim.SetMoveInput(MoverDefaultInputs_PreSim.GetMoveInputType(), RelativeMove);
			MoverDefaultInputs_PreSim.OrientationIntent = RelativeOrient;
			MoverDefaultInputs_PreSim.bUsingMovementBase = true;
			MoverDefaultInputs_PreSim.MovementBase = MovementBase;
			MoverDefaultInputs_PreSim.MovementBaseBoneName = BaseBoneName;
		}
	}

	// (5) Copy PreSim into the actual InputCmdResult that Mover consumes.
	FCharacterDefaultInputs& OutDefault = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	OutDefault = MoverDefaultInputs_PreSim;

	FAZ_MoverCustomInputs& OutCustom = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FAZ_MoverCustomInputs>();
	OutCustom = MoverCustomInputs_PreSim;

	// (6) Consume one-shot temporal flags.
	// (Legacy GAS-tag → FAZ_MoverInputCmd handshake removed in Phase 9 — no
	//  consumers; PlayerInputState + MoverCustomInputs_PreSim now carry all the
	//  intent flags Mover modes need. The FAZ_MoverInputCmd struct file is
	//  orphaned; safe to delete after a BP-asset audit.)
	bIsJumpJustPressed = false;
}

// ============================================================
// MovementDirection helpers (Phase 4)
// ============================================================

FAZ_MovementDirectionThresholds AAZ_HeroPawn::Get_MovementDirectionThresholds() const
{
	// Defaults from struct: FL/FR=55°, BL/BR=125°. Per-character overrides
	// could come from a UPROPERTY later; GASP's typical 70/110 is a tuning choice.
	return FAZ_MovementDirectionThresholds{};
}

EAZ_MovementDirection AAZ_HeroPawn::Get_MovementDirectionFromAngle(
	const FAZ_MovementDirectionThresholds& Thresholds, double SignedAngleDegrees) const
{
	const double Abs = FMath::Abs(SignedAngleDegrees);

	// Forward arc: |angle| ≤ FL (left) or FR (right).
	if (SignedAngleDegrees <= 0.0 && Abs <= Thresholds.FL) return EAZ_MovementDirection::F;
	if (SignedAngleDegrees >= 0.0 && Abs <= Thresholds.FR) return EAZ_MovementDirection::F;

	// Backward arc: |angle| ≥ BL (left) or BR (right).
	if (SignedAngleDegrees <= 0.0 && Abs >= Thresholds.BL) return EAZ_MovementDirection::B;
	if (SignedAngleDegrees >= 0.0 && Abs >= Thresholds.BR) return EAZ_MovementDirection::B;

	// Side arc — foot-phase chosen by carrying last frame's bucket via PostSim
	// (gives hysteresis so foot doesn't flip when crossing F/L or L/B boundaries).
	const EAZ_MovementDirection Prev = MoverCustomInputs_PostSim.MovementDirection;
	if (SignedAngleDegrees < 0.0)
	{
		// Left side. Keep prior LL/LR if already on the left, else default LL.
		return (Prev == EAZ_MovementDirection::LR) ? EAZ_MovementDirection::LR
		                                           : EAZ_MovementDirection::LL;
	}
	// Right side.
	return (Prev == EAZ_MovementDirection::RR) ? EAZ_MovementDirection::RR
	                                           : EAZ_MovementDirection::RL;
}

void AAZ_HeroPawn::Get_MovementDirectionAndOffset(EAZ_MovementDirection& OutDirection,
                                                  double& OutRotationOffset) const
{
	const FVector MoveDir = Get_MoveInput();
	if (MoveDir.IsNearlyZero())
	{
		// No movement — preserve last direction so anim doesn't snap mid-stop.
		OutDirection = MoverCustomInputs_PostSim.MovementDirection;
		OutRotationOffset = 0.0;
		return;
	}

	// Signed angle from body forward to move direction (+CW = right, -CCW = left).
	const FVector ActorFwd   = GetActorForwardVector();
	const FVector ActorRight = GetActorRightVector();
	const double  Forward = FVector::DotProduct(ActorFwd,   MoveDir);
	const double  Right   = FVector::DotProduct(ActorRight, MoveDir);
	const double  SignedAngle = FMath::RadiansToDegrees(FMath::Atan2(Right, Forward));

	OutDirection = Get_MovementDirectionFromAngle(Get_MovementDirectionThresholds(), SignedAngle);

	// Residual offset = (actual move heading) − (canonical heading of bucket).
	// Anim uses this to rotate the strafe loop so feet match true velocity yaw.
	const double CanonicalHeading = [&]() -> double
	{
		switch (OutDirection)
		{
		case EAZ_MovementDirection::F:  return 0.0;
		case EAZ_MovementDirection::B:
			// Choose ±180 closer to SignedAngle to keep offset small.
			return (SignedAngle >= 0.0) ? 180.0 : -180.0;
		case EAZ_MovementDirection::LL:
		case EAZ_MovementDirection::LR: return -90.0;
		case EAZ_MovementDirection::RL:
		case EAZ_MovementDirection::RR: return  90.0;
		default:                        return 0.0;
		}
	}();
	OutRotationOffset = FRotator::NormalizeAxis(SignedAngle - CanonicalHeading);
}

// ============================================================
// Phase 6 — Movement mode change handler
// ============================================================

void AAZ_HeroPawn::HandleMovementModeChanged(const FName& PreviousMode, const FName& NewMode)
{
	// GASP parity (full body of SandboxCharacter_Mover.OnMovementModeChanged).
	// Resolve incoming FNames → enum values via MovementModeMap (GASP holds these
	// as function-local vars; AZ uses stack locals for the same effect).
	EAZ_MovementMode PrevEnum = EAZ_MovementMode::OnGround;
	EAZ_MovementMode NewEnum  = EAZ_MovementMode::OnGround;
	if (const EAZ_MovementMode* P = MovementModeMap.Find(PreviousMode)) PrevEnum = *P;
	if (const EAZ_MovementMode* N = MovementModeMap.Find(NewMode))      NewEnum  = *N;

	// Track raw FName values for any AZ-side consumers (DebugDraws, future wiring).
	PreviousMovementModeName = PreviousMode;
	CurrentMovementModeName  = NewMode;

	// ===== Sequence then_0: NEW mode entry handling =====
	switch (NewEnum)
	{
	case EAZ_MovementMode::OnGround:  // Walking
		// Land foley fires only when PREV was Falling. Volume comes from impact Z velocity:
		// fall speed -500 → vol 0.5, -900 → vol 1.5 (clamped, mapped linearly).
		if (PrevEnum == EAZ_MovementMode::InAir && CharacterMoverComponent)
		{
			const double ZVel = CharacterMoverComponent->GetVelocity().Z;
			const float Volume = static_cast<float>(FMath::GetMappedRangeValueClamped(
				FVector2D(-500.0, -900.0), FVector2D(0.5, 1.5), ZVel));
			PlayFoleyEvent_Stub(TEXT("Foley.Event.Land"), Volume);
		}
		break;

	case EAZ_MovementMode::InAir:  // Falling
	{
		// Jump foley fires only when PREV was Walking AND bIsJumpJustPressed.
		// Volume from XY speed: 0 → vol 0.5, 375 → vol 1.0.
		if (PrevEnum == EAZ_MovementMode::OnGround
			&& MoverDefaultInputs_PreSim.bIsJumpJustPressed
			&& CharacterMoverComponent)
		{
			const double XYSpeed = CharacterMoverComponent->GetVelocity().Size2D();
			const float Volume = static_cast<float>(FMath::GetMappedRangeValueClamped(
				FVector2D(0.0, 375.0), FVector2D(0.5, 1.0), XYSpeed));
			PlayFoleyEvent_Stub(TEXT("Foley.Event.Jump"), Volume);
		}

		// Auto-uncrouch when transitioning to Falling — no crouching in air.
		// GASP also calls CharacterMover->UnCrouch() directly. AZ uses the
		// input-intent path (flag clear) which Mover honors via the next
		// OnProduceInput-built FAZ_MoverCustomInputs, avoiding direct stance API.
		// Note: crouch in AZ lives on the CUSTOM-inputs struct, not FCharacterDefaultInputs.
		if (CharacterMoverComponent && CharacterMoverComponent->IsCrouching())
		{
			PlayerInputState.bWantsToCrouch = false;
			MoverCustomInputs_PreSim.bWantsToCrouch = false;
		}
		break;
	}

	case EAZ_MovementMode::Slide:  // Sliding
		// Slide.Loop foley — stash returned audio component so Update_SlidingAudio
		// can drive its `Speed` parameter and the cleanup pass below can fade it out.
		SlidingAudioComponent = PlayFoleyEvent_Stub(TEXT("Foley.Event.Slide.Loop"), 1.0f);
		break;

	default:
		break;
	}

	// ===== Sequence then_1: PREV mode exit handling (cleanup) =====
	// Leaving Sliding → fade out the loop audio over 0.5s.
	if (PrevEnum == EAZ_MovementMode::Slide && SlidingAudioComponent)
	{
		SlidingAudioComponent->FadeOut(0.5f, 0.f, EAudioFaderCurve::Linear);
		SlidingAudioComponent = nullptr;
	}

	// GASP slide-exit-while-sprinting → clear queued crouch intent.
	// Without this, a sprint-slide-release sequence leaves the pawn stuck crouched.
	// (GASP exec source for this branch traced via Reviewer A: it's chained AFTER
	// the Sliding-cleanup FadeOut, gated by bWantsToSprint.)
	if (PrevEnum == EAZ_MovementMode::Slide && PlayerInputState.bWantsToSprint)
	{
		PlayerInputState.bWantsToCrouch = false;
		MoverCustomInputs_PreSim.bWantsToCrouch = false;
	}
}

UAudioComponent* AAZ_HeroPawn::PlayFoleyEvent_Stub(FName EventName, float Volume)
{
	// TODO(foley): wire to AZ's AC_FoleyEvents-equivalent actor component when it
	// lands. GASP signature: `AC_FoleyEvents.PlayFoleyEvent(GameplayTag Event,
	// S_FoleyEventParams { Volume }) → UAudioComponent*`. Until then this is a
	// no-op so OnMovementModeChanged keeps full GASP-parity structure with no audio.
	(void)EventName;
	(void)Volume;
	return nullptr;
}

// ============================================================
// Phase 7 — Side systems
// ============================================================

FAZ_TraversalCheckInputs AAZ_HeroPawn::Get_TraversalCheckInputs() const
{
	FAZ_TraversalCheckInputs Out;
	Out.TraceForwardDirection = GetActorForwardVector();
	// Other fields keep their struct defaults (200 fwd, 30 radius, 60 half-height).
	return Out;
}

void AAZ_HeroPawn::DebugDraws() const
{
	if (!bShowPawnDebug) return;
	if (!GEngine) return;

	const FString Line = FString::Printf(
		TEXT("[AZ HeroPawn] Mode=%s  Speed=%.0f  Gait=%d  Dir=%d  TIPInProg=%d  CRR=%.0f"),
		*CurrentMovementModeName.ToString(),
		Get_Speed(),
		static_cast<int32>(Get_Gait()),
		static_cast<int32>(MoverCustomInputs_PostSim.MovementDirection),
		bIdleTurnInProgress ? 1 : 0,
		ControlRotationRate);
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(reinterpret_cast<UPTRINT>(this)),
	                                 0.f, FColor::Yellow, Line);
}

// ============================================================
// Phase 8 — IAZ_SandboxCharacterPawn implementation
// ============================================================

FAZ_CharacterPropertiesForAnimation AAZ_HeroPawn::GetPropertiesForAnimation_Implementation() const
{
	FAZ_CharacterPropertiesForAnimation Out;
	Out.MovementDirection = MoverCustomInputs_PostSim.MovementDirection;
	Out.AimingRotation    = Get_AimingRotation();
	Out.OrientationIntent = MoverDefaultInputs_PostSim.OrientationIntent.Rotation();
	Out.SteeringTime      = 0.0; // Filled by mover-feedback integration later.
	Out.GroundNormal      = MoverStateProxy.GroundNormal;
	Out.GroundLocation    = MoverStateProxy.GroundLocation;
	Out.bJustLanded       = false; // Set by Mover landed delegate later.
	Out.LandVelocity      = FVector::ZeroVector;
	return Out;
}

FAZ_CharacterPropertiesForCamera AAZ_HeroPawn::GetPropertiesForCamera_Implementation() const
{
	FAZ_CharacterPropertiesForCamera Out;
	Out.CameraMode = 0;
	Out.Stance     = MoverStateProxy.bIsCrouching ? EAZ_Stance::Crouching : EAZ_Stance::Standing;
	Out.Gait       = Get_Gait();
	return Out;
}

FAZ_CharacterPropertiesForTraversal AAZ_HeroPawn::GetPropertiesForTraversal_Implementation() const
{
	FAZ_CharacterPropertiesForTraversal Out;
	Out.Mesh        = MeshComponent;
	Out.Capsule     = Cast<UCapsuleComponent>(GetRootComponent());
	Out.MovementMode= Get_CurrentMovementMode();
	Out.Gait        = Get_Gait();
	Out.Speed       = Get_Speed();
	return Out;
}

void AAZ_HeroPawn::SetCharacterInputState_Implementation(const FAZ_PlayerInputState& NewState)
{
	PlayerInputState = NewState;
}

void AAZ_HeroPawn::Update_ControlRotationRate(float DeltaSeconds)
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		ControlRotationRate = 0.0;
		return;
	}

	const FRotator CurrentControlRot = PC->GetControlRotation();
	if (!bControlRotationRateInitialized)
	{
		LastControlRotation = CurrentControlRot;
		bControlRotationRateInitialized = true;
		ControlRotationRate = 0.0;
		return;
	}

	const double Delta = FRotator::NormalizeAxis(CurrentControlRot.Yaw - LastControlRotation.Yaw);
	ControlRotationRate = FMath::Abs(Delta) / DeltaSeconds;
	LastControlRotation = CurrentControlRot;
}

void AAZ_HeroPawn::Update_TwinStickMode()
{
	// GASP gate: only active when DDCvar.ControlStyle == 1. False branch is empty
	// in GASP — TwinStickMode is a one-way latch (once true, stays true even if
	// the cvar is later cleared). Matched here for behavioral parity.
	if (CVarDDControlStyle.GetValueOnGameThread() != 1)
	{
		return;
	}

	TwinStickMode = true;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// GASP zeros control rotation each tick — camera follows, no manual look in twin-stick.
	PC->SetControlRotation(FRotator::ZeroRotator);

	// Write TwinStickAimRotation from right-stick deflection (Atan2 of Y-flipped, X)
	// or fall back to actor rotation when stick is centered.
	const FVector2D Stick = GetTwinStickAimDirection();
	constexpr float Tolerance = 0.1f;
	if (!Stick.IsNearlyZero(Tolerance))
	{
		const double YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(-Stick.Y, Stick.X));
		TwinStickAimRotation = FRotator(0.0, YawDegrees, 0.0);
	}
	else
	{
		TwinStickAimRotation = GetActorRotation();
	}
}

void AAZ_HeroPawn::Update_SlidingAudio()
{
	// GASP parity: only set the Speed param when the audio component is valid.
	// Component is created in OnMovementModeChanged when entering Slide and
	// faded-out + nulled when leaving. So this is naturally gated to Slide mode.
	if (!SlidingAudioComponent || !CharacterMoverComponent)
	{
		return;
	}

	const float Speed = static_cast<float>(CharacterMoverComponent->GetVelocity().Size());
	SlidingAudioComponent->SetFloatParameter(FName(TEXT("Speed")), Speed);
}

void AAZ_HeroPawn::Update_TargetedActor()
{
	if (TargetableActors.Num() > 0)
	{
		// GASP FindNearestActor — picks closest by 3D distance from actor location.
		// TArray<TObjectPtr<AActor>> needs flattening to TArray<AActor*> for the API.
		TArray<AActor*> Candidates;
		Candidates.Reserve(TargetableActors.Num());
		for (const TObjectPtr<AActor>& A : TargetableActors)
		{
			if (A)
			{
				Candidates.Add(A.Get());
			}
		}
		float Distance = 0.f;
		TargetedActor = UGameplayStatics::FindNearestActor(GetActorLocation(), Candidates, Distance);
	}
	else
	{
		TargetedActor = nullptr;
	}
	// GASP debug cone skipped — Phase 0 ledger marks all debug overlays as out of scope.
}

void AAZ_HeroPawn::Update_IdleTIPAccumulator()
{
	// GASP parity: idle TIP signal fires only in Aiming. In OrientToMovement and
	// Strafe, GASP returns LastOrient when idle (Get_OrientationIntent comment),
	// so no body rotation → no TIP. Aiming uses the 60° threshold cache.
	// AnimInstance reads `bIdleTurnInProgress` via IsIdleTurnInProgress() to
	// drive its TIP state in the SM; this signal is the AZ-side anim helper.
	const EAZ_RotationMode Mode = Get_RotationMode();
	if (Mode != EAZ_RotationMode::Aiming)
	{
		bIdleTurnInProgress = false;
		AccumulatedYawSinceCommit = 0.f;
		bAccumYawInitialized = false;
		return;
	}

	// Speed-independent: tracks absolute mouse-yaw motion since last commit.
	// 60° fires bIdleTurnInProgress; release on body-aligned (dot ≥ 0.998 ≈ 4°).
	// Get_OrientationIntent now drives actual rotation via raw GASP threshold-cache;
	// this accumulator only signals "TIP in progress" for anim consumption.
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	const float ControllerYaw = PC->GetControlRotation().Yaw;

	// Reset state when moving.
	if (!Get_MoveInput().IsNearlyZero())
	{
		bIdleTurnInProgress = false;
		AccumulatedYawSinceCommit = 0.f;
		LastObservedControllerYaw = ControllerYaw;
		bAccumYawInitialized = true;
		return;
	}

	if (!bAccumYawInitialized)
	{
		LastObservedControllerYaw = ControllerYaw;
		AccumulatedYawSinceCommit = 0.f;
		bAccumYawInitialized = true;
	}

	const float YawDelta = FRotator::NormalizeAxis(ControllerYaw - LastObservedControllerYaw);
	AccumulatedYawSinceCommit += FMath::Abs(YawDelta);
	LastObservedControllerYaw = ControllerYaw;

	if (!bIdleTurnInProgress && AccumulatedYawSinceCommit >= 60.f)
	{
		bIdleTurnInProgress = true;
		AccumulatedYawSinceCommit = 0.f;
	}
	else if (bIdleTurnInProgress)
	{
		// Release when actor has rotated to face camera (Mover sim has caught up).
		const FVector ActorFwd2D = FVector(GetActorForwardVector().X, GetActorForwardVector().Y, 0.f).GetSafeNormal();
		const FVector CamFwd2D   = FRotator(0.f, ControllerYaw, 0.f).Vector();
		if (FVector::DotProduct(ActorFwd2D, CamFwd2D) >= 0.998f)
		{
			bIdleTurnInProgress = false;
			AccumulatedYawSinceCommit = 0.f;
		}
	}
}
