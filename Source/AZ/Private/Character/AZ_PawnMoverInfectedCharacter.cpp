// Copyright Artur. AZ project.

#include "Character/AZ_PawnMoverInfectedCharacter.h"

#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/Abilities/AZ_GA_Death.h"
#include "AbilitySystem/Abilities/AZ_GA_MeleeAttack.h"   // FindAnimSetMontage (flinch)
#include "AbilitySystem/Abilities/AZ_GA_ZombieMelee.h"
#include "Animation/AnimMontage.h"
#include "AZ_GameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AI/AZ_InfectedAIController.h"
#include "Animation/AnimInstance.h"          // mid-turn commitment read (temp reflection glue)
#include "Animation/AZ_LocomotionTypes.h"   // FAZ_MoverCustomInputs, EAZ_Gait, EAZ_RotationMode
#include "Character/AZ_MovementDirectionCapabilityComponent.h"
#include "Character/AZ_PawnMoverComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DefaultMovementSet/NavMoverComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameplayTagContainer.h"
#include "MoverDataModelTypes.h"            // FCharacterDefaultInputs, EMoveInputType
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
	// Pawns must NOT affect navigation: a nav-relevant capsule carves a hole in the navmesh under the pawn —
	// the Chalkie's own path queries then start "off-navmesh" inside its own carve and ALL moves fail. Found
	// the hard way (Phase 0 smoke test); agents use avoidance, not carving.
	Capsule->SetCanEverAffectNavigation(false);
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

	// "Where can I move" clearance clamp (no tick — ProduceInput calls ConstrainIntent).
	MovementCapability = CreateDefaultSubobject<UAZ_MovementDirectionCapabilityComponent>(TEXT("MovementCapability"));

	// Nav<->Mover bridge: PathFollowing auto-discovers it (INavMovementInterface); it finds our MoverComponent in
	// its InitializeComponent. Agent extents mirror the capsule (25 radius / 2x90 half-height) so RecastNavMesh
	// generation (project nav settings use the same numbers) and per-agent queries agree.
	NavMoverComponent = CreateDefaultSubobject<UNavMoverComponent>(TEXT("NavMoverComponent"));
	NavMoverComponent->NavAgentProps.AgentRadius = 25.f;
	NavMoverComponent->NavAgentProps.AgentHeight = 180.f;

	// --- Own ASC (NPC pattern). Minimal replication: AI is server-authoritative. ---
	AbilitySystemComponent = CreateDefaultSubobject<UAZ_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// Combat vitals (S1): owner-subobject → the ASC picks it up in InitAbilityActorInfo. Two hero
	// punches (2×25) kill a standard Chalkie; per-variant HP moves to DA_ChalkieConfig later.
	VitalsAttributeSet = CreateDefaultSubobject<UAZ_VitalsAttributeSet>(TEXT("VitalsAttributeSet"));
	VitalsAttributeSet->InitMaxHealth(50.f);
	VitalsAttributeSet->InitHealth(50.f);

	// Faction must be valid BEFORE possession: for placed pawns SpawnDefaultController runs from
	// PostInitializeComponents (pre-BeginPlay), and the AI controller adopts the pawn's team in OnPossess.
	// Leaving this to BeginPlay handed the controller NoTeam → every Chalkie read every OTHER Chalkie as
	// Hostile (1 != NoTeam) and the horde fought itself. BeginPlay re-applies with any BP override.
	TeamId = FGenericTeamId(DefaultTeamId);
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

	// Re-apply faction with the BP-overridable default (ctor already set the native default pre-possession).
	TeamId = FGenericTeamId(DefaultTeamId);

	// Bind ASC actor info now that the pawn (owner + avatar) exists. Idempotent — safe alongside PossessedBy.
	InitAbilitySystem();

	// Flinch listener: Vitals Health drops (but survives) -> variant hit-react. BeginPlay runs once
	// per instance, so this binds exactly once.
	if (AbilitySystemComponent && VitalsAttributeSet)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(VitalsAttributeSet->GetHealthAttribute())
			.AddUObject(this, &AAZ_PawnMoverInfectedCharacter::HandleHealthChanged);
	}
}

void AAZ_PawnMoverInfectedCharacter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue >= Data.OldValue || Data.NewValue <= 0.f)
	{
		return;   // heals and killing blows don't flinch (death owns the killing blow)
	}
	if (MoverComponent && !MoverComponent->IsActive())
	{
		return;   // already a corpse
	}
	UAnimMontage* Flinch = UAZ_GA_MeleeAttack::FindAnimSetMontage(this, TEXT("HitReactMontage"));
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!Flinch || !AnimInstance || AnimInstance->Montage_IsPlaying(Flinch))
	{
		return;   // no double-flinch: a landed hit during a flinch just deals damage
	}
	// A flinch on the shared slot also interrupts a mid-swing attack montage — punching a Chalkie
	// out of its claw is deliberate (player defense). The KnockBack source clips run 3-7s with a full
	// recovery tail, so play only a WINDOW: stop with a blend after ~1.1s.
	AnimInstance->Montage_Play(Flinch);
	FTimerHandle FlinchTimer;
	GetWorldTimerManager().SetTimer(FlinchTimer, FTimerDelegate::CreateWeakLambda(this, [this, Flinch]()
	{
		if (UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr)
		{
			if (Anim->Montage_IsPlaying(Flinch))
			{
				Anim->Montage_Stop(0.3f, Flinch);
			}
		}
	}), 1.1f, false);
}

void AAZ_PawnMoverInfectedCharacter::BeginCorpse(float RagdollDelay)
{
	// Death latch: only death deactivates the Mover (corpses are PERMANENT now — no lifespan to latch on).
	if (MoverComponent && !MoverComponent->IsActive())
	{
		return;
	}

	// MINIMAL anim-only death (user decision 2026-07-21): the montage holds its last frame and that IS
	// the corpse. No ragdoll, capsule collision UNTOUCHED, mesh stays attached and Mover-managed (the
	// visual-component null + collision changes were ragdoll-era plumbing — nulling the visual comp
	// mid-game can leave a stale smoothing offset baked into the mesh transform). RagdollCorpse() stays
	// unused for the day corpse physics is wanted (explosions).
	// NEVER DestroyComponent a live Mover: the NetworkPrediction backend keeps ticking the registered
	// simulation -> use-after-free crash in WalkingMode::SimulationTick (learned 2026-07-21).
	(void)RagdollDelay;

	// Brain off (controller detaches + BT stops); movement sim off. NO despawn — corpses stay as set
	// dressing (user decision 2026-07-21). Freeze the anim once the death clip has fully played out
	// (longest death clip is ~2.2s) so a field of corpses costs zero anim evaluation.
	DetachFromControllerPendingDestroy();
	if (MoverComponent)
	{
		MoverComponent->Deactivate();
	}
	FTimerHandle FreezeTimer;
	GetWorldTimerManager().SetTimer(FreezeTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (Mesh)
		{
			Mesh->bPauseAnims = true;
		}
	}), 4.f, false);
	UE_LOG(LogTemp, Display, TEXT("[Vitals] %s DIED"), *GetName());
}

void AAZ_PawnMoverInfectedCharacter::RagdollCorpse()
{
	if (!Mesh)
	{
		return;
	}
	if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(0.f);
	}
	// Un-parent from the capsule FIRST: anything still nudging the capsule (Mover's network-prediction
	// backend keeps writing transforms even deactivated) would drag the simulated mesh along and spam
	// "Attempting to move a fully simulated skeletal mesh... use the Teleport flag" every tick. The
	// mesh stays owned by the actor, so the lifespan despawn still cleans it up.
	Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	// EXPLICIT ragdoll collision (this project's ini redefines the profile list WITHOUT the engine's
	// "Ragdoll" profile — SetCollisionProfileName silently yields no collision -> corpse through the
	// floor). Block the world, ignore live pawns walking over and the camera spring arm.
	Mesh->SetCollisionObjectType(ECC_PhysicsBody);
	Mesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Mesh->SetSimulatePhysics(true);
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

	// Native startup grants (S3): the claw swipe (BT-activated) + the death ability (Event.Death-triggered).
	// Grant-time is the earliest point the native tag registry is guaranteed ready, hence the CDO trigger
	// patch here rather than in the ability constructor.
	if (!bStartupAbilitiesGranted && HasAuthority())
	{
		bStartupAbilitiesGranted = true;
		UAZ_GA_Death::ConfigureTriggerOnCDO();
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UAZ_GA_Death::StaticClass(), 1, INDEX_NONE, this));
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UAZ_GA_ZombieMelee::StaticClass(), 1, INDEX_NONE, this));
	}
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

	// NavMesh path-follow first: MoveTo/MoveToActor requests land in the NavMoverComponent (RequestPathMove caches
	// a unit-ish intent; RequestDirectMove caches a full velocity). Both collapse to a unit DIRECTION here — the
	// gait remains the single speed authority (our walking modes read GetMoveInput_WorldSpace as 0..1 intent), so
	// nav never fights the gait table. Consume writes the outs ONLY when a move is in flight (returns false and
	// leaves them untouched otherwise — hence the zero-init locals).
	FVector NavIntent   = FVector::ZeroVector;
	FVector NavVelocity = FVector::ZeroVector;
	const bool bHasNavMove =
		NavMoverComponent && NavMoverComponent->ConsumeNavMovementData(NavIntent, NavVelocity);

	// Nav data wins over the raw AI intent cache; the cache stays the no-pathing fallback (scripted nudges,
	// dormant twitch-turns). Favor the velocity request when both are present (mirrors MoverExamplesCharacter).
	FVector WorldMove = bHasNavMove
		? (NavVelocity.IsNearlyZero() ? NavIntent : NavVelocity)
		: CachedAIMoveIntentWorld;
	WorldMove.Z = 0.f;
	if (bHasNavMove)
	{
		WorldMove = WorldMove.GetSafeNormal();
	}

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
	const bool bFacingFromMove = Facing.IsNearlyZero();
	if (bFacingFromMove)
	{
		Facing = WorldMove;
	}
	if (Facing.IsNearlyZero())
	{
		Facing = GetActorForwardVector();
	}
	Facing = Facing.GetSafeNormal();
	const FRotator FacingRot = Facing.Rotation();

	// TLOU turn-then-move + anim intent-commitment: a move goal far off the current heading is a TURN first,
	// then a walk. Two hold conditions, translation parked while EITHER is true:
	//  1. Misaligned (> 60 deg off the goal): keeps GroundSpeed ~0 so the ABP's stationary turn-in-place
	//     detection fires (simultaneous rotate+accelerate reads as a walk to the anim layer and skates).
	//  2. The anim layer is MID-TURN (ABP 'Turning' latched until its EndRotation notify): the committed turn
	//     clip finishes before the walk starts — no mid-clip pop when the blend gate releases.
	// Gameplay preemption is untouched: BT decorator aborts (spotted a target) re-task instantly regardless.
	// Only applies when facing DERIVES from the move (an explicit facing override owns the body heading).
	// WALK GAIT ONLY — politeness is for the calm branches (investigate/wander toward STATIC points, where the
	// spring settles and the turn completes). A chasing (Run) Chalkie skips both holds: its goal MOVES, so the
	// spring never aligns and the hold deadlocks into stand-and-turn-forever while prey circles it. Aggression
	// interrupts politeness — it wheels and sprints, and an in-flight turn clip just gets blended over.
	// The BP-var reflection read is TEMPORARY glue — turn detection moves into UAZ_InfectedAnimInstance C++ at
	// the next full build (60 deg + the var promoted to real UPROPERTYs then).
	if (bFacingFromMove && !WorldMove.IsNearlyZero() && CachedAIGait == EAZ_Gait::Walk)
	{
		bool bAnimMidTurn = false;
		if (const UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr)
		{
			if (const FBoolProperty* TurningProperty =
					CastField<FBoolProperty>(AnimInstance->GetClass()->FindPropertyByName(TEXT("Turning"))))
			{
				bAnimMidTurn = TurningProperty->GetPropertyValue_InContainer(AnimInstance);
			}
		}

		const float CosToGoal =
			FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), WorldMove.GetSafeNormal());
		if (bAnimMidTurn || CosToGoal < FMath::Cos(FMath::DegreesToRadians(60.f)))
		{
			WorldMove = FVector::ZeroVector;
		}
	}

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
