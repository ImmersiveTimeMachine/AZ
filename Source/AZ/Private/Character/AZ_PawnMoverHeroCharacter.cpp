// Copyright Artur. AZ project.

#include "Character/AZ_PawnMoverHeroCharacter.h"

#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Animation/AZ_LocomotionTypes.h"   // FAZ_MoverCustomInputs, EAZ_Gait
#include "AZ_GameplayTags.h"                 // FAZ_GameplayTags::Get()
#include "Engine/CollisionProfile.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayTagContainer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "MoverDataModelTypes.h"
#include "MoverPoseSearchTrajectoryPredictor.h"
#include "NetworkPredictionComponent.h"
#include "Player/AZ_PlayerState.h"

AAZ_PawnMoverHeroCharacter::AAZ_PawnMoverHeroCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	bReplicates = true;
	SetReplicatingMovement(false); // Mover handles movement replication

	// Body rotation is driven by the Mover mode (always-back-to-camera target via
	// ResolveRotationTarget() — implemented in Step 3). Controller rotation never
	// drives the actor directly; the camera orbits independently of the body.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw   = false;
	bUseControllerRotationRoll  = false;

	// --- Capsule (collision root) ---
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(25.f, 90.f);
	Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	Capsule->SetCanEverAffectNavigation(true);
	Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Capsule->SetGenerateOverlapEvents(false);
	SetRootComponent(Capsule);

	// --- Skeletal Mesh ---
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Capsule);
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, -92.f));
	Mesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Mesh->SetGenerateOverlapEvents(true);
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// --- Camera Boom + Camera (third-person; 1p ADS hybrid added later) ---
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Capsule);
	CameraBoom->TargetArmLength = 220.f;
	CameraBoom->SocketOffset = FVector(0.f, 70.f, 0.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 8.f;
	CameraBoom->CameraLagMaxDistance = 50.f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 10.f;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->ProbeSize = 12.f;
	CameraBoom->ProbeChannel = ECC_Camera;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	// --- Mover ---
	MoverComponent = CreateDefaultSubobject<UAZ_PawnMoverComponent>(TEXT("MoverComponent"));
	MoverComponent->SetUpdatedComponent(Capsule);
	MoverComponent->SetHandleJump(true);    // Physics jump: the engine Walking mode applies the launch impulse
	                                        // and transitions Walking -> Falling (gravity + floor-contact land),
	                                        // so the arc adapts to terrain height. Re-enforced in BeginPlay.
	MoverComponent->SetHandleStanceChanges(true);   // Engine handles crouch stance: the base OnMoverPreSimulationTick
	                                                // queues/cancels the engine FStanceModifier from bWantsToCrouch.
	                                                // The component's input bridge feeds bWantsToCrouch from the
	                                                // Movement.Crouching GAS tag. Re-enforced in BeginPlay.
	MoverComponent->PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// Required for Mover replication.
	NetworkPredictionComponent = CreateDefaultSubobject<UNetworkPredictionComponent>(TEXT("NetworkPredictionComponent"));

	// --- PoseSearch trajectory predictor (Mover-native) ---
	TrajectoryPredictor = CreateDefaultSubobject<UMoverTrajectoryPredictor>(TEXT("TrajectoryPredictor"));
}

void AAZ_PawnMoverHeroCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Physics-driven jump: the engine Walking mode (bHandleJump) consumes bIsJumpJustPressed (packed by the GA
	// jump via IAZ_JumpRequester), applies the launch impulse, and transitions Walking -> Falling; the engine
	// Falling mode does gravity / air-control and plants back into Walking on real floor contact — so the jump
	// adapts to terrain height instead of a baked flat-ground RM arc. Set here as well as the ctor so a BP
	// archetype that serialized bHandleJump=false can't override it. The SM air phase follows MovementMode
	// (InAir) in UAZ_LocomotionStateMachine; the anim is cosmetic. RMAction is kept for future vault/mantle.
	if (MoverComponent)
	{
		MoverComponent->SetHandleJump(true);
		MoverComponent->SetHandleStanceChanges(true);   // archetype-proof: a BP that serialized false can't override
	}

	// NOTE: RM bridge (capsule side) deliberately NOT queued here yet. FLayeredMove_RootMotionAttribute
	// uses MixMode=OverrideAll and contributes whenever the "RootMotionDelta" mesh attribute is present
	// — which RootMotionFromEverything makes true EVERY frame, including in-place idle/walk loops (≈zero
	// delta). OverrideAll then forces capsule velocity to ~zero, so velocity-driven locomotion can't even
	// start moving (W deadlocks in idle). It must only be active for transition clips that actually carry
	// RM. Re-add in the stops/starts iteration, gated by the Mover.SkipAnimRootMotion tag during loops
	// (the layered move early-outs on that tag — see RootMotionAttributeLayeredMove.cpp:137-141).
	// See project_root_motion_mode.

	// Wire the PoseSearch trajectory predictor to the Mover component. The v2 AnimInstance generates an
	// FTransformTrajectory each tick from it (PoseSearchGenerateTransformTrajectoryWithPredictor) — the
	// single source for both motion matching (PoseHistory node) and intent-based IsMoving.
	//
	// Lazy-create if the BP CDO nulled it out: UMoverTrajectoryPredictor is UCLASS(EditInlineNew), so a
	// Blueprint subclass CDO serializes this instanced-subobject property and clobbers the C++
	// CreateDefaultSubobject default with null (confirmed — AZ_BP_PawnMoverHeroCharacter's CDO has
	// TrajectoryPredictor=None). Without this guard GetTrajectoryPredictor() returns null at runtime and
	// the AnimInstance never builds the trajectory (samples=0). Mirrors v1 AAZ_HeroPawn::BeginPlay.
	if (!TrajectoryPredictor)
	{
		TrajectoryPredictor = NewObject<UMoverTrajectoryPredictor>(this, TEXT("TrajectoryPredictor_Runtime"));
	}
	if (TrajectoryPredictor)
	{
		TrajectoryPredictor->Setup(GetMoverComponent());
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->PlayerCameraManager->ViewPitchMax = 89.f;
		PC->PlayerCameraManager->ViewPitchMin = -89.f;
	}

	// Apply default team id once. Subclasses (AI) can override DefaultTeamId in their ctor
	// or call SetGenericTeamId at spawn for runtime faction switching.
	TeamId = FGenericTeamId(DefaultTeamId);
}

void AAZ_PawnMoverHeroCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// ========================================
// Possession + GAS init
// ========================================
//
// Two entry points because GAS ActorInfo must be initialized on both ends:
//   - Server: PossessedBy (also grants StartupAbilities — server is authority).
//   - Client: OnRep_PlayerState (granting is server-only; client just inits ActorInfo).
// Pawn-switch (entering a vehicle) re-fires PossessedBy on the new pawn — ActorInfo
// rebinds the AvatarActor to the new pawn while OwnerActor (PlayerState) stays the
// same, so player abilities continue to function but target the new avatar.

void AAZ_PawnMoverHeroCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitAbilitySystem();
}

void AAZ_PawnMoverHeroCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitAbilitySystem();
}

void AAZ_PawnMoverHeroCharacter::InitAbilitySystem()
{
	AAZ_PlayerState* PS = GetPlayerState<AAZ_PlayerState>();
	if (!PS)
	{
		// On clients, PlayerState replicates in a few frames after possess —
		// OnRep_PlayerState fires when it arrives and we retry.
		return;
	}

	UAZ_AbilitySystemComponent* ASC = Cast<UAZ_AbilitySystemComponent>(PS->GetAbilitySystemComponent());
	if (!ASC)
	{
		return;
	}

	// Rebinds ActorInfo on every (re)possession. Safe to call repeatedly — GAS
	// resets actor refs and re-binds delegates. Required after pawn-switch so
	// the player ASC targets the new avatar.
	ASC->InitAbilityActorInfo(PS, this);

	// Server-only granting. GrantAbilitiesWithInputTag has an idempotency guard
	// (bStartupAbilitiesGiven) — re-possession or vehicle exit returns is a no-op.
	if (HasAuthority())
	{
		ASC->GrantAbilitiesWithInputTag(StartupAbilities);
	}
}

// ========================================
// Enhanced Input
// ========================================
//
// Pawn-side binding per the v2 multi-pawn design: each pawn class owns its input
// surface. The PC stays pawn-agnostic — on possess it queries GetDefaultMappingContext()
// and pushes it into the local player's EnhancedInput subsystem; on unpossess it pops.
//
// Handlers cache to member fields; the Mover-side ProduceInput_Implementation reads
// those fields and writes the deterministic InputCmd consumed by Mover modes. Caching
// keeps input on the game thread and avoids touching FCharacterDefaultInputs from
// callbacks (Mover wants a single producer point per sim tick).

void AAZ_PawnMoverHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		return;
	}

	// BP-defaults sanity checks. Silent skip → "input doesn't work" with no error.
	// `ensureMsgf` warns loudly in dev (red log) without crashing; ship builds still run.
	ensureMsgf(DefaultMappingContext,
		TEXT("%s: DefaultMappingContext is null. Set it in the pawn BP defaults — without it, "
		     "the PC has no per-pawn IMC to push on possess and no IAs will trigger."),
		*GetName());
	ensureMsgf(MoveInputAction,
		TEXT("%s: MoveInputAction is null. Set it in the pawn BP defaults — movement won't work."),
		*GetName());
	ensureMsgf(LookInputAction,
		TEXT("%s: LookInputAction is null. Set it in the pawn BP defaults — look won't work."),
		*GetName());

	if (MoveInputAction)
	{
		Input->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &AAZ_PawnMoverHeroCharacter::OnMoveTriggered);
		Input->BindAction(MoveInputAction, ETriggerEvent::Completed, this, &AAZ_PawnMoverHeroCharacter::OnMoveCompleted);
	}
	if (LookInputAction)
	{
		Input->BindAction(LookInputAction, ETriggerEvent::Triggered, this, &AAZ_PawnMoverHeroCharacter::OnLookTriggered);
	}
	// Jump is GAS-routed via UAZ_GA_PawnJump (InputTag Input.Action.Jump) — not
	// bound here. The PC's InputConfig DA maps the Jump IA to the input tag,
	// the GA fires, the GA calls IAZ_JumpRequester::SetJumpPressed on this pawn.
}

void AAZ_PawnMoverHeroCharacter::OnMoveTriggered(const FInputActionValue& Value)
{
	// IA_Move axis convention: X = Right/Left (A/D), Y = Forward/Back (W/S).
	// CachedMoveInputIntent uses pawn-local convention: X = Forward, Y = Right.
	// The Mover input producer rotates this by ControlRotation.Yaw to get world-space.
	const FVector2D Input2D = Value.Get<FVector2D>();
	CachedMoveInputIntent.X = FMath::Clamp(Input2D.Y, -1.f, 1.f);
	CachedMoveInputIntent.Y = FMath::Clamp(Input2D.X, -1.f, 1.f);
	CachedMoveInputIntent.Z = 0.f;
}

void AAZ_PawnMoverHeroCharacter::OnMoveCompleted(const FInputActionValue& Value)
{
	CachedMoveInputIntent = FVector::ZeroVector;
}

void AAZ_PawnMoverHeroCharacter::OnLookTriggered(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookVector.X * LookRateYaw);
	AddControllerPitchInput(-LookVector.Y * LookRatePitch);
}

// ========================================
// IAZ_JumpRequester
// ========================================

void AAZ_PawnMoverHeroCharacter::SetJumpPressed(bool bPressed)
{
	// One-shot edge-detect: bIsJumpJustPressed = press-this-tick, consumed ONLY by
	// ProduceInput (both the normal path and the no-PC early-return consume it).
	// Release deliberately does NOT clear the edge — a press+release inside one game
	// frame (fast tap / low FPS) must still deliver its one-shot to the next sim tick;
	// clearing on release silently dropped those jumps (audit P2-18).
	if (bPressed && !bIsJumpPressed)
	{
		bIsJumpJustPressed = true;
	}
	bIsJumpPressed = bPressed;
}

// ========================================
// IAbilitySystemInterface
// ========================================

UAbilitySystemComponent* AAZ_PawnMoverHeroCharacter::GetAbilitySystemComponent() const
{
	// Player ASC lives on PlayerState (cross-pawn persistence — survives possession
	// changes / vehicle entry / respawn). Direct query each call; no cache. If this
	// becomes a measured hot path, cache in PossessedBy/OnRep_PlayerState.
	if (const AAZ_PlayerState* PS = GetPlayerState<AAZ_PlayerState>())
	{
		return PS->GetAbilitySystemComponent();
	}
	return nullptr;
}

// ========================================
// IMoverInputProducerInterface
// ========================================

void AAZ_PawnMoverHeroCharacter::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	// Mover input producer. Single point per sim tick that converts cached game-thread
	// input state into the deterministic InputCmd Mover modes consume. NetworkPrediction
	// replays this on the server with the same inputs, so all input must flow through
	// here (never directly poke FCharacterDefaultInputs from a callback).
	//
	// Only FCharacterDefaultInputs is shipped — the v2 design routes all non-physics
	// intent (crouch, sprint, aim, weapon swap, etc.) through GAS tags on the player
	// ASC, which GAS predicts+replicates. Mover modes read those tags via OwnedTagsRef.
	// Gait / MovementDirection / RotationOffset are derived later in the pipeline
	// (AnimInstance / chooser / Mover mode's ResolveRotationTarget), not pre-computed
	// here — keeps the Mover InputCmd small and the producer free of derived state.
	FCharacterDefaultInputs& CharacterDefaultInputs =
		InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();

	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		// AI / unpossessed: zero the defaults AND the custom inputs, and consume the jump
		// one-shot — a press just before unpossession must not stay latched and fire on the
		// next possession, and a recycled cmd context must not carry stale gait/crouch
		// (audit P1 §ProduceInput no-PC path).
		CharacterDefaultInputs = FCharacterDefaultInputs();
		FAZ_MoverCustomInputs& NoPCCustom = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FAZ_MoverCustomInputs>();
		NoPCCustom = FAZ_MoverCustomInputs();
		bIsJumpJustPressed = false;
		return;
	}

	const FRotator ControlRot = PC->GetControlRotation();

	// World-space move = camera-yaw-relative WASD. Pitch/roll discarded — character
	// moves on the ground plane regardless of where the camera tilts.
	const FRotator YawOnly(0.f, ControlRot.Yaw, 0.f);
	FVector WorldMove = FRotationMatrix(YawOnly).TransformVector(CachedMoveInputIntent);

	// Where the camera is pointing this sim tick. Mover stores it on the
	// SyncState so modes / animation can read "look direction" deterministically
	// (don't query PlayerCameraManager from inside a mode — it isn't replayed).
	CharacterDefaultInputs.ControlRotation = ControlRot;
	
	// Attack owns movement: drop locomotion intent mid-melee so the capsule doesn't
	// glide under the full-body punch.
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())   // you implement IAbilitySystemInterface
			if (ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().Ability_State_MeleeAttacking))
				WorldMove = FVector::ZeroVector;

	// The move command itself. DirectionalIntent = "a unit-ish vector pointing
	// where I want to go" (vs. Velocity = "an exact velocity vector"). World-space
	// because bUsingMovementBase is false; if true, this would be base-relative.
	CharacterDefaultInputs.SetMoveInput(EMoveInputType::DirectionalIntent, WorldMove);

	// Where the body should face. Mover modes use this as the rotation target unless
	// a mode overrides via ResolveRotationTarget(). Step 2 baseline: face movement
	// direction (Orient-to-Movement). Step 3's UAZ_PawnMoverSmoothWalkingMode replaces
	// this with always-back-to-camera + idle-TIP via the virtual seam.
	CharacterDefaultInputs.OrientationIntent = WorldMove;

	// ---- Gait → Mover (v2: movement-domain GAS tags bridge to the Mover sim) ----
	// The walking mode's ResolveGait reads FAZ_MoverCustomInputs.Gait and maps it to
	// WalkSpeed/RunSpeed/SprintSpeed (+ accel, turn, facing). We translate the player's
	// MOVEMENT-domain tags into the gait HERE — on the game thread, where the ASC is
	// reachable — and ship the resolved enum in the deterministic InputCmd, because a Mover
	// mode runs in the prediction replay and cannot query the ASC. Tags are granted by
	// movement abilities (BP_GA_Run → Movement.Running via ActivationOwnedTags). We read
	// Movement.* (domain state), NOT Ability.State.* (which is ability-to-ability coordination).
	FAZ_MoverCustomInputs& CustomInputs = InputCmdResult.InputCollection.FindOrAddMutableDataByType<FAZ_MoverCustomInputs>();
	{
		const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();
		if (HasMatchingGameplayTag(AZTags.Movement_Sprinting))
		{
			CustomInputs.Gait = EAZ_Gait::Sprint;
		}
		else if (HasMatchingGameplayTag(AZTags.Movement_Running))
		{
			CustomInputs.Gait = EAZ_Gait::Run;
		}
		else 
		{
			CustomInputs.Gait = EAZ_Gait::Walk;
		}
		
		CustomInputs.bWantsToCrouch = HasMatchingGameplayTag(AZTags.Movement_Crouching);
	}

	// "Held" jump flag — true the whole time the player is holding Space, false
	// on release. Mover's falling-mode air-control / coyote-time reads this each tick.
	CharacterDefaultInputs.bIsJumpPressed = bIsJumpPressed;

	// One-shot edge — true ONLY on the sim tick where press transitioned 0→1. Used
	// by the walking mode to fire the initial jump impulse exactly once. Consumed
	// at the bottom of this function (we set bIsJumpJustPressed = false there).
	CharacterDefaultInputs.bIsJumpJustPressed = bIsJumpJustPressed;

	// Optional movement-mode override. NAME_None = "let Mover pick the mode from
	// state" (walking, falling, swimming via the transitions registered on the
	// MoverComponent). Set this to a specific mode name to force a transition —
	// e.g. teleport into Flying mode, scripted slide, etc.
	CharacterDefaultInputs.SuggestedMovementMode = NAME_None;

	// false = inputs are world-space (the simple case). Set true + fill MovementBase
	// + MovementBaseBoneName when standing on a moving primitive (elevator, ship
	// deck) so input is inherited along with the platform's motion. Mover detects
	// the base via floor query; the conversion uses UBasedMovementUtils.
	CharacterDefaultInputs.bUsingMovementBase = false;

	// Consume one-shot.
	bIsJumpJustPressed = false;
}

// ========================================
// IGameplayTagAssetInterface — route through the ASC.
// Until Step 4 wires the ASC, all queries return empty/false (safe default).
// ========================================

void AAZ_PawnMoverHeroCharacter::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->GetOwnedGameplayTags(TagContainer);
	}
}

bool AAZ_PawnMoverHeroCharacter::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		return ASC->HasMatchingGameplayTag(TagToCheck);
	}
	return false;
}

bool AAZ_PawnMoverHeroCharacter::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		return ASC->HasAllMatchingGameplayTags(TagContainer);
	}
	return false;
}

bool AAZ_PawnMoverHeroCharacter::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		return ASC->HasAnyMatchingGameplayTags(TagContainer);
	}
	return false;
}

// ========================================
// IGenericTeamAgentInterface
// ========================================

void AAZ_PawnMoverHeroCharacter::SetGenericTeamId(const FGenericTeamId& NewTeamId)
{
	TeamId = NewTeamId;
}

FGenericTeamId AAZ_PawnMoverHeroCharacter::GetGenericTeamId() const
{
	return TeamId;
}
