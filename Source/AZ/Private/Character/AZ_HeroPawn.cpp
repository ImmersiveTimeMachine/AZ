#include "Character/AZ_HeroPawn.h"

#include "AbilitySystemComponent.h"
#include "Character/AZ_MoverInputCmd.h"
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
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"

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

		// Floor hit for GroundNormal / GroundLocation (used by foot IK slope warping)
		FHitResult FloorHit;
		if (CharacterMoverComponent->TryGetFloorCheckHitResult(FloorHit))
		{
			MoverStateProxy.GroundNormal = FloorHit.ImpactNormal;
			MoverStateProxy.GroundLocation = FloorHit.ImpactPoint;
		}
		else
		{
			MoverStateProxy.GroundNormal = FVector::UpVector;
			MoverStateProxy.GroundLocation = FVector::ZeroVector;
		}
	}

	if (AZ_AbilitySystemComponent)
	{
		const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();
		MoverStateProxy.bIsSprinting = AZ_AbilitySystemComponent->HasMatchingGameplayTag(AZTags.Ability_State_Sprinting);
		MoverStateProxy.bIsAiming = AZ_AbilitySystemComponent->HasMatchingGameplayTag(AZTags.Ability_State_Aiming);
	}

	// Phase 5: per-tick control rotation rate (for camera-whip detection / GAS).
	Update_ControlRotationRate(DeltaTime);

	// GASP parity: after Mover has simulated this frame, copy its last input cmd
	// into our _PostSim snapshots. Anim/camera read these (they're the replicated
	// state). Mover's tick runs before ours via AddTickPrerequisiteComponent
	// (set up in BeginPlay), guaranteeing GetLastInputCmd is valid here.
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
	// GASP priority: TargetedActor look-at → TwinStickAimRotation → control rotation.
	// TargetedActor / TwinStickMode come in Phase 5 (Update_TargetedActor, Update_TwinStickMode).
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		return PC->GetControlRotation();
	}
	return FRotator::ZeroRotator;
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
	// GASP three-tier dispatch. TargetedActor / TwinStickMode branches deferred —
	// Phase 5 populates those state fields. For now: default branch only.
	if (PlayerInputState.bWantsToAim)    return EAZ_RotationMode::Aiming;
	if (PlayerInputState.bWantsToStrafe) return EAZ_RotationMode::Strafe;
	return EAZ_RotationMode::OrientToMovement;
}

EAZ_Gait AAZ_HeroPawn::Get_Gait() const
{
	// AZ semantics (inverted from GASP):
	//   Default (no modifier) → Walk
	//   Sprint button (Shift) → Sprint  (Run when in Aim mode)
	//   Walk button           → Walk    (no-op since default is already Walk;
	//                                    kept for external "force walk" overrides
	//                                    e.g. heavy-load / low-stamina ability)
	//
	// GASP defaults to Run as the baseline, with Walk explicit and Sprint via
	// modifier. AZ inverts because slow/quiet movement is the natural baseline
	// for our game and Shift=fast matches mainstream FPS/TPS feel.

	// Sprint takes priority — Shift always means "go fast".
	if (PlayerInputState.bWantsToSprint)
	{
		const EAZ_RotationMode RotMode = Get_RotationMode();
		switch (RotMode)
		{
		case EAZ_RotationMode::OrientToMovement:
			return EAZ_Gait::Sprint;

		case EAZ_RotationMode::Strafe:
		{
			// GASP dot-test retained: sprint only when moving roughly along orientation.
			const FVector MoveDir = MoverDefaultInputs_PreSim.GetMoveInput().GetSafeNormal();
			const FVector OrientDir = MoverDefaultInputs_PreSim.OrientationIntent.GetSafeNormal();
			const float Dot = static_cast<float>(FVector::DotProduct(MoveDir, OrientDir));
			constexpr float StrafeSprintThreshold = 0.5f;
			return (Dot > StrafeSprintThreshold) ? EAZ_Gait::Sprint : EAZ_Gait::Run;
		}

		case EAZ_RotationMode::Aiming:
			// In Aim mode, Sprint caps at Run.
			return EAZ_Gait::Run;

		default:
			return EAZ_Gait::Run;
		}
	}

	// Walk button: explicit, but identical to default in AZ (kept for overrides).
	if (PlayerInputState.bWantsToWalk)
	{
		return EAZ_Gait::Walk;
	}

	// Default: Walk (AZ inversion).
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
			// Idle on ground.
			// AZ divergence (broader than GASP): the accumulator-based TIP fires
			// for ALL rotation modes, not just Aim. Reason: until Phase 9 wires
			// GAS, bWantsToAim/bWantsToStrafe stay false, so Get_RotationMode
			// always returns OrientToMovement — strict GASP would give "no idle
			// TIP" everywhere and regress the working baseline (commit b5c076e1).
			// When GAS Aim lands, narrow this back to the Aim-only case.
			return bIdleTurnInProgress ? LastIdleOrientationTarget : LastOrient;
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

	// (6) Legacy GAS-tag handshake — populates FAZ_MoverInputCmd that walking-mode
	//     etc. read for sprint/aim/crouch/dash speed adjustments. Phase 9 will
	//     migrate these into PlayerInputState and remove this block.
	if (AZ_AbilitySystemComponent)
	{
		FAZ_MoverInputCmd& AZInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FAZ_MoverInputCmd>();
		const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();
		AZInputs.bIsSprinting = AZ_AbilitySystemComponent->HasMatchingGameplayTag(AZTags.Ability_State_Sprinting);
		AZInputs.bIsAiming    = AZ_AbilitySystemComponent->HasMatchingGameplayTag(AZTags.Ability_State_Aiming);
		AZInputs.bIsCrouching = AZ_AbilitySystemComponent->HasMatchingGameplayTag(AZTags.Movement_Crouching);
		AZInputs.bIsDashing   = AZ_AbilitySystemComponent->HasMatchingGameplayTag(AZTags.Ability_State_Dashing);
	}

	// (7) Consume one-shot temporal flags.
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
	PreviousMovementModeName = PreviousMode;
	CurrentMovementModeName  = NewMode;
	// Hooks for slide audio / one-shot effects can attach here later (Phase 7+).
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

	const double Yaw = PC->GetControlRotation().Yaw;
	if (!bControlRotationRateInitialized)
	{
		LastControlRotationYaw = Yaw;
		bControlRotationRateInitialized = true;
		ControlRotationRate = 0.0;
		return;
	}

	const double Delta = FRotator::NormalizeAxis(Yaw - LastControlRotationYaw);
	ControlRotationRate = FMath::Abs(Delta) / DeltaSeconds;
	LastControlRotationYaw = Yaw;
}

void AAZ_HeroPawn::Update_IdleTIPAccumulator()
{
	// Speed-independent: tracks absolute mouse-yaw motion since last commit.
	// 60° fires bIdleTurnInProgress; release on body-aligned (dot ≥ 0.998 ≈ 4°).
	// Replaces GASP's raw |delta| ≥ 60° check (which couples to mouse speed).
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	const float ControllerYaw = PC->GetControlRotation().Yaw;

	// Reset state when moving — body faces movement direction, no TIP needed.
	// Re-init LastObservedControllerYaw so the first idle frame doesn't see
	// a huge stale delta from before the move started.
	const FVector MoveVec = Get_MoveInput();
	if (!MoveVec.IsNearlyZero())
	{
		bIdleTurnInProgress = false;
		AccumulatedYawSinceCommit = 0.f;
		LastObservedControllerYaw = ControllerYaw;
		bAccumYawInitialized = true;
		LastIdleOrientationTarget = FVector::ZeroVector;
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

	// Commit fresh turn when accumulator hits 60°.
	if (!bIdleTurnInProgress && AccumulatedYawSinceCommit >= 60.f)
	{
		bIdleTurnInProgress = true;
		AccumulatedYawSinceCommit = 0.f;
		LastIdleOrientationTarget = FRotator(0.f, ControllerYaw, 0.f).Vector();
	}

	// While in-progress, track camera (player can adjust mid-turn) and check
	// alignment release condition.
	if (bIdleTurnInProgress)
	{
		LastIdleOrientationTarget = FRotator(0.f, ControllerYaw, 0.f).Vector();

		const FVector ActorFwd2D = FVector(GetActorForwardVector().X, GetActorForwardVector().Y, 0.f).GetSafeNormal();
		if (FVector::DotProduct(ActorFwd2D, LastIdleOrientationTarget) >= 0.998f)
		{
			bIdleTurnInProgress = false;
			AccumulatedYawSinceCommit = 0.f;
		}
	}
}
