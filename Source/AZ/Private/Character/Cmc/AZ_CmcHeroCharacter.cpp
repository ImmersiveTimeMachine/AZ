// Copyright Artur. AZ project.

#include "Character/Cmc/AZ_CmcHeroCharacter.h"

#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/AZ_GA_HitReact.h"
#include "AbilitySystem/Abilities/AZ_GA_PlayerGrabbed.h"
#include "AZ_GameplayTags.h"
#include "Camera/CameraComponent.h"
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

	// --- CMC feel (starting point = v2 gait speeds + GASP-flavored accel; P1 tunes against the Mover
	// build side-by-side; further tuning belongs in the BP child's CharacterMovement panel, NOT here
	// once children exist — doctrine rule 1). ---
	// NOTE: MaxAcceleration / BrakingDecelerationWalking / GroundFriction / RotationRate are deliberately
	// NOT set here — ApplyMovementFeelParams() recomputes all four every frame and is their single owner.
	// Setting them here too would look authoritative in the BP details panel while being overwritten on
	// the first tick.
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->JumpZVelocity = 420.f;
	Move->GravityScale = 1.5f;
	Move->AirControl = 0.2f;
	Move->MaxStepHeight = 30.f;
	Move->SetWalkableFloorAngle(38.f);

	// --- Camera (P0 framing ≈ v2 explore stance; per-mode stances land in P2) ---
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

void AAZ_CmcHeroCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// GASP's PreCMCTick ordering: the derived-parameter pass must land BEFORE the movement component
	// consumes it, otherwise CMC spends the frame on last frame's braking/accel values and the feel lags
	// input by exactly one tick. Declaring the actor a prerequisite of CMC is what guarantees that.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
	}
	// The pose is evaluated from the post-move transform, so the mesh follows the actor as well.
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
	}
}

void AAZ_CmcHeroCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Order matters: the gait resolves this frame's MaxWalkSpeed, and the feel pass tapers acceleration
	// against the speed that gait implies.
	ResolveGaitAndStanceFromTags();
	UpdateSelectionGait();   // owns the stop-band latch that braking and pool selection both read
	ApplyMovementFeelParams();
}

void AAZ_CmcHeroCharacter::ResolveGaitAndStanceFromTags()
{
	const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();

	// Movement.* is domain state (granted by the movement abilities via ActivationOwnedTags); we read it
	// rather than Ability.State.*, which is ability-to-ability coordination. Same source as the v2 pawn.
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
		SetGait(DesiredGait);   // the one write point for MaxWalkSpeed
	}

	// Crouch is native here — no Mover mode, no custom input struct. Guarded on bIsCrouched so we issue
	// the request on the transition only; CMC handles the capsule resize and its replication.
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

	// Rotation mode drives which locomotion SET the graph searches (orient-to-movement forward clips vs
	// the 8-way directional strafe set), so it has to be resolved before OrientationIntent is meaningful.
	// Aiming outranks strafe: both face the camera, but aiming additionally locks the upper body.
	if (HasMatchingGameplayTag(AZTags.Ability_State_Aiming))
	{
		Out.RotationMode = EAZ_RotationMode::Aiming;
	}
	else if (HasMatchingGameplayTag(AZTags.Movement_Strafe))
	{
		Out.RotationMode = EAZ_RotationMode::Strafe;
	}

	// Raw desires, kept separate from the RESOLVED gait/stance the base already published. The graph
	// needs both: "sprinting" is what the body is doing, "wants to sprint" is what the player asked for,
	// and they differ for exactly as long as a transition takes — which is the window the start clips fill.
	Out.InputState.bWantsToSprint = HasMatchingGameplayTag(AZTags.Movement_Sprinting);
	Out.InputState.bWantsToWalk = !Out.InputState.bWantsToSprint && !HasMatchingGameplayTag(AZTags.Movement_Running);
	Out.InputState.bWantsToCrouch = HasMatchingGameplayTag(AZTags.Movement_Crouching);
	Out.InputState.bWantsToStrafe = HasMatchingGameplayTag(AZTags.Movement_Strafe);
	Out.InputState.bWantsToAim = HasMatchingGameplayTag(AZTags.Ability_State_Aiming);

	// Recompute now that RotationMode is known — the base filled it assuming orient-to-movement, which is
	// wrong the moment we are strafing (the body faces the camera, not the stick).
	if (Out.RotationMode == EAZ_RotationMode::OrientToMovement && !Out.InputAcceleration.IsNearlyZero())
	{
		Out.OrientationIntent = Out.InputAcceleration.Rotation();
	}
	else
	{
		Out.OrientationIntent = FRotator(0.f, Out.AimingRotation.Yaw, 0.f);
	}
}

void AAZ_CmcHeroCharacter::ApplyMovementFeelParams()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move)
	{
		return;
	}

	const float Speed2D = Move->Velocity.Size2D();
	const bool bHasInput = !Move->GetCurrentAcceleration().IsNearlyZero();

	// Released-stick braking follows GAIT, because the stop CLIPS disagree by gait and they are what the
	// stop has to match. Foot slide on a stop is exactly clipStopTime - (Speed / braking), so a single
	// shared value cannot serve three gaits: measured 2026-08-23 at a shared 500, walk still slid 0.59s
	// while sprint OVERSHOT by 0.22s. Each per-gait default is that gait's speed / the clip's own ~0.95s
	// stop time. (The old "released brakes 4x harder than held" framing is gone: at 2000 the capsule
	// stopped in 0.19s from a run against a clip depicting 0.95s, which is what the 0.76s slide WAS.)
	// Keyed off ACTUAL SPEED, not CurrentGait. Gait comes from gameplay tags
	// (ResolveGaitAndStanceFromTags), so releasing the sprint input drops the tag to Walk on that frame
	// while the body is still travelling at sprint speed — measured 2026-08-23: spd=558 with
	// braking=190 (the walk value), a 2.9s stop. Rotation rate legitimately follows INTENT, which is why
	// bGaitScaledRotationRate keys off the gait; braking must follow MOMENTUM, which only speed knows.
	float NoInputBraking = BrakingDecelNoInput;
	if (bGaitScaledBraking)
	{
		// SelectionGait, not Speed2D directly: it is latched at the instant the stop began, so the value
		// stays CONSTANT for the whole stop. Recomputing per-frame from the current speed made the
		// deceleration decay as the character slowed (615 -> 375 -> 190 in one stop from sprint = 493cm
		// over 2.15s against a 167cm / 0.95s clip). The stop clips decelerate linearly; so must we.
		switch (SelectionGait)
		{
		case EAZ_Gait::Sprint: NoInputBraking = SprintBrakingDecel; break;
		case EAZ_Gait::Run:    NoInputBraking = RunBrakingDecel;    break;
		default:               NoInputBraking = WalkBrakingDecel;   break;
		}
	}
	Move->BrakingDecelerationWalking = bHasInput ? BrakingDecelWithInput : NoInputBraking;

	// Acceleration and friction both fall off with speed, so the last stretch to top speed is gradual.
	const float AccelAlpha = FMath::Clamp(
		(Speed2D - AccelTaperSpeedMin) / FMath::Max(1.f, AccelTaperSpeedMax - AccelTaperSpeedMin), 0.f, 1.f);
	Move->MaxAcceleration = FMath::Lerp(MaxAccelerationBase, MaxAccelerationAtTopSpeed, AccelAlpha);

	const float FrictionAlpha = FMath::Clamp(Speed2D / FMath::Max(1.f, FrictionTaperSpeedMax), 0.f, 1.f);
	Move->GroundFriction = FMath::Lerp(GroundFrictionMax, GroundFrictionMin, FrictionAlpha);

	// Turn rate follows GAIT, because the arc loops disagree by gait and they are what a held turn has to
	// match: walk arcs carve at 180 deg/s, run arcs at 93-116. One rate cannot serve both — at the walk
	// rate a running turn outruns its arcs and the search falls through to pivots and 180-degree starts;
	// at the run rate walking crawls (a 180 takes 1.6s). Sprint has no arc content, so it gets the widest
	// carve by choice. Airborne stays finite regardless or the turn reads as a snap.
	float GroundedYawRate = GroundedRotationRateYaw;
	if (bGaitScaledRotationRate)
	{
		switch (CurrentGait)
		{
		case EAZ_Gait::Run:    GroundedYawRate = RunRotationRateYaw;    break;
		case EAZ_Gait::Sprint: GroundedYawRate = SprintRotationRateYaw; break;
		default:               break;   // Walk keeps GroundedRotationRateYaw
		}
	}
	Move->RotationRate = FRotator(0.f, Move->IsFalling() ? FallingRotationRateYaw : GroundedYawRate, 0.f);
}

void AAZ_CmcHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}

	// BP-defaults sanity checks (v2 lesson): a silent skip reads as "input doesn't work" with no error.
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
	// Jump stays GAS-routed (InputConfig maps the Jump IA to Input.Action.Jump; UAZ_GA_PawnJump calls
	// IAZ_JumpRequester::SetJumpPressed, which the base forwards to native Jump()). Not bound here.
}

void AAZ_CmcHeroCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	// Self-contained IMC push: the pawn owns its input surface and doesn't depend on the PC knowing this
	// pawn generation. Remove-then-add keeps the call idempotent across restarts.
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
	// Mirrors the v2 grant site verbatim — the pattern is proven and doctrine-compliant (grant the
	// RESOLVED class, patch ITS CDO, idempotent by spec lookup).
	AAZ_PlayerState* PS = GetPlayerState<AAZ_PlayerState>();
	if (!PS)
	{
		return;   // clients retry from OnRep_PlayerState when it replicates in
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

void AAZ_CmcHeroCharacter::OnMoveTriggered(const FInputActionValue& Value)
{
	// IA_Move axis convention (same asset as v2): X = Right/Left, Y = Forward/Back. Camera-relative.
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator YawRotation(0.f, Controller ? Controller->GetControlRotation().Yaw : 0.f, 0.f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Axis.X);
}

void AAZ_CmcHeroCharacter::OnLookTriggered(const FInputActionValue& Value)
{
	// Grabbed = camera locked (TLOU-style hold; the grab camera owns the shot). The IMC stays pushed —
	// removing it would also kill the E-mash IA.
	if (HasMatchingGameplayTag(FAZ_GameplayTags::Get().State_Grabbed))
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X * LookRateYaw);
	AddControllerPitchInput(-Axis.Y * LookRatePitch);
}
