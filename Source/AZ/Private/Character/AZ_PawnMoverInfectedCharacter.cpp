// Copyright Artur. AZ project.

#include "Character/AZ_PawnMoverInfectedCharacter.h"

#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AI/AZ_InfectedAIController.h"
#include "Animation/AZ_LocomotionTypes.h"   // FAZ_MoverCustomInputs, EAZ_Gait, EAZ_RotationMode
#include "Character/AZ_MovementDirectionCapabilityComponent.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameplayTagContainer.h"
#include "MoverDataModelTypes.h"            // FCharacterDefaultInputs, EMoveInputType
#include "MoverPoseSearchTrajectoryPredictor.h"
#include "NetworkPredictionComponent.h"

AAZ_PawnMoverInfectedCharacter::AAZ_PawnMoverInfectedCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// No per-frame actor tick: there's no camera to interp and the AI drive lives on the controller. The Mover
	// component ticks itself (TG_PrePhysics). Keeps a horde of these cheap.
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicatingMovement(false);   // Mover handles movement replication

	// Body rotation is driven by the Mover mode (OrientationIntent target). Controller rotation never drives the
	// actor body directly.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw   = false;
	bUseControllerRotationRoll  = false;

	// Auto-possess by our AI controller whether the pawn is placed in the level or spawned at runtime.
	AIControllerClass = AAZ_InfectedAIController::StaticClass();
	AutoPossessAI     = EAutoPossessAI::PlacedInWorldOrSpawned;

	// --- Capsule (collision root) --- mirrors the hero so the same skeletal mesh aligns and the Mover floor
	// queries behave identically.
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(25.f, 90.f);
	Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	Capsule->SetCanEverAffectNavigation(true);
	Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Capsule->SetGenerateOverlapEvents(false);
	SetRootComponent(Capsule);

	// --- Skeletal Mesh --- AnimBlueprint mode; the BP assigns the NPC's own ABP (parent UAZ_InfectedAnimInstance).
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Capsule);
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, -92.f));
	Mesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Mesh->SetGenerateOverlapEvents(true);
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// --- Mover --- modes (Walking / Falling / RMAction) self-register in UAZ_PawnMoverComponent::OnRegister, so
	// no BP wiring is needed for movement to work.
	MoverComponent = CreateDefaultSubobject<UAZ_PawnMoverComponent>(TEXT("MoverComponent"));
	MoverComponent->SetUpdatedComponent(Capsule);
	MoverComponent->SetHandleJump(true);
	MoverComponent->SetHandleStanceChanges(true);
	MoverComponent->PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// Required for Mover replication / NetworkPrediction (AI is server-authoritative; this carries corrections).
	NetworkPredictionComponent = CreateDefaultSubobject<UNetworkPredictionComponent>(TEXT("NetworkPredictionComponent"));

	// PoseSearch trajectory predictor (Mover-native). The infected AnimInstance reads it for motion matching.
	TrajectoryPredictor = CreateDefaultSubobject<UMoverTrajectoryPredictor>(TEXT("TrajectoryPredictor"));

	// "Where can I move" clearance clamp (no tick — ProduceInput calls ConstrainIntent).
	MovementCapability = CreateDefaultSubobject<UAZ_MovementDirectionCapabilityComponent>(TEXT("MovementCapability"));

	// --- Own ASC (NPC pattern). Minimal replication: AI is server-authoritative. ---
	AbilitySystemComponent = CreateDefaultSubobject<UAZ_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
}

void AAZ_PawnMoverInfectedCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Archetype-proof the Mover flags (a BP that serialized false can't override). Mirrors the hero.
	if (MoverComponent)
	{
		MoverComponent->SetHandleJump(true);
		MoverComponent->SetHandleStanceChanges(true);
	}

	// Wire the trajectory predictor to the Mover component. Lazy-create if a BP subclass CDO nulled the instanced
	// subobject (same guard the hero needs — UMoverTrajectoryPredictor is EditInlineNew).
	if (!TrajectoryPredictor)
	{
		TrajectoryPredictor = NewObject<UMoverTrajectoryPredictor>(this, TEXT("TrajectoryPredictor_Runtime"));
	}
	if (TrajectoryPredictor)
	{
		TrajectoryPredictor->Setup(GetMoverComponent());
	}

	// Apply default faction once (subclasses / spawners can override via SetGenericTeamId).
	TeamId = FGenericTeamId(DefaultTeamId);

	// Bind ASC actor info now that the pawn (owner + avatar) exists. Idempotent — safe alongside PossessedBy.
	InitAbilitySystem();
}

void AAZ_PawnMoverInfectedCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	// Re-bind ASC actor info on (re)possession — harmless if BeginPlay already did it.
	InitAbilitySystem();
}

void AAZ_PawnMoverInfectedCharacter::InitAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// NPC ASC: owner AND avatar are this pawn (no PlayerState). Safe to call repeatedly.
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilitySystemComponent->AbilityActorInfoSet();

	// Startup attributes / abilities (health, melee, etc.) are granted in the combat/health step — server-only,
	// guarded by HasAuthority — and intentionally omitted from this foundation.
}

// ========================================
// AI intent surface
// ========================================

void AAZ_PawnMoverInfectedCharacter::SetMoveIntentWorld(const FVector& WorldIntent)
{
	CachedAIMoveIntentWorld = WorldIntent;
}

void AAZ_PawnMoverInfectedCharacter::SetDesiredFacingWorld(const FVector& WorldFacing)
{
	CachedAIDesiredFacingWorld = WorldFacing;
}

void AAZ_PawnMoverInfectedCharacter::SetGait(EAZ_Gait NewGait)
{
	CachedAIGait = NewGait;
}

// ========================================
// IMoverInputProducerInterface
// ========================================

void AAZ_PawnMoverInfectedCharacter::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	// AI input producer — the server-authoritative counterpart of the hero's ProduceInput. Same single-producer
	// rule: this is the one place per sim tick that turns cached intent into the deterministic InputCmd the Mover
	// modes consume (NetworkPrediction replays it identically). No PlayerController / camera is read.
	FCharacterDefaultInputs& Defaults =
		InputCmdResult.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	FAZ_MoverCustomInputs& Custom =
		InputCmdResult.InputCollection.FindOrAddMutableDataByType<FAZ_MoverCustomInputs>();

	// Move intent is already WORLD-space (the controller / BT computes it toward the goal). Keep it on the ground plane.
	FVector WorldMove = CachedAIMoveIntentWorld;
	WorldMove.Z = 0.f;

	// "Where can I move" — same intent-pure clamp the hero uses, so a Chalkie pinned against a wall idles instead of
	// running in place (the anim stays intent-driven; Mover's collision is the movement backstop).
	if (MovementCapability)
	{
		WorldMove = MovementCapability->ConstrainIntent(WorldMove);
	}

	// Facing: explicit desired facing if set (face the target), else face the move direction, else HOLD the current
	// facing when fully idle (no spin).
	FVector Facing = CachedAIDesiredFacingWorld;
	Facing.Z = 0.f;
	if (Facing.IsNearlyZero())
	{
		Facing = WorldMove;
	}
	if (Facing.IsNearlyZero())
	{
		Facing = GetActorForwardVector();
	}
	Facing = Facing.GetSafeNormal();
	const FRotator FacingRot = Facing.Rotation();

	// Where the AI is "looking". Stored on the InputCmd so modes / anim read a deterministic heading (the
	// AnimInstance separately reads the controller's control rotation for AO/TIP — the controller keeps that in
	// sync with this facing).
	Defaults.ControlRotation = FacingRot;

	// DirectionalIntent: a unit-ish vector toward the desired motion. World-space (bUsingMovementBase=false).
	Defaults.SetMoveInput(EMoveInputType::DirectionalIntent, WorldMove);

	// Body facing target. The walking mode rotates toward this (OrientToMovement spring); for AI it is the goal/move
	// heading, not a camera.
	Defaults.OrientationIntent = Facing;

	// Infected don't jump yet. Explicitly clear so a recycled cmd context can't carry a stale impulse.
	Defaults.bIsJumpPressed        = false;
	Defaults.bIsJumpJustPressed    = false;
	Defaults.SuggestedMovementMode = NAME_None;
	Defaults.bUsingMovementBase    = false;

	// Gait → speed (Walk/Run/Sprint resolved by the walking mode). RotationMode = OrientToMovement (no camera
	// strafe/aim for AI). Crouch off for the foundation.
	Custom.Gait           = CachedAIGait;
	Custom.bWantsToCrouch = false;
	Custom.RotationMode   = EAZ_RotationMode::OrientToMovement;
	Custom.RotationOffset = 0.0;
}

// ========================================
// IAbilitySystemInterface
// ========================================

UAbilitySystemComponent* AAZ_PawnMoverInfectedCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

// ========================================
// IGameplayTagAssetInterface — route through the ASC.
// ========================================

void AAZ_PawnMoverInfectedCharacter::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->GetOwnedGameplayTags(TagContainer);
	}
}

bool AAZ_PawnMoverInfectedCharacter::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		return ASC->HasMatchingGameplayTag(TagToCheck);
	}
	return false;
}

bool AAZ_PawnMoverInfectedCharacter::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		return ASC->HasAllMatchingGameplayTags(TagContainer);
	}
	return false;
}

bool AAZ_PawnMoverInfectedCharacter::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
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

void AAZ_PawnMoverInfectedCharacter::SetGenericTeamId(const FGenericTeamId& NewTeamId)
{
	TeamId = NewTeamId;
}

FGenericTeamId AAZ_PawnMoverInfectedCharacter::GetGenericTeamId() const
{
	return TeamId;
}
