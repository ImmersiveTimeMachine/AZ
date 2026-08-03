// Copyright Artur. AZ project.

#include "Character/AZ_PawnMoverInfectedCharacter.h"

#include "AbilitySystem/AZ_AbilitySystemComponent.h"
#include "AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/Abilities/AZ_GA_ChalkieGrab.h"
#include "AbilitySystem/Abilities/AZ_GA_Death.h"
#include "AbilitySystem/Abilities/AZ_GA_HitReact.h"
#include "AbilitySystemBlueprintLibrary.h"   // SendGameplayEventToActor (HitReact / StepBack triggers)
#include "AbilitySystem/Abilities/AZ_GA_MeleeAttack.h"   // FindAnimSetMontage (flinch)
#include "AbilitySystem/Abilities/AZ_GA_ZombieMelee.h"
#include "Animation/AnimMontage.h"
#include "AZ_GameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AI/AZ_InfectedAIController.h"
#include "AIController.h"
#include "GameplayEffectExtension.h"   // FGameplayEffectModCallbackData (scream causer)
#include "Perception/AISense_Hearing.h"
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
#include "MotionWarpingComponent.h"
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
	// Pawns are never floors: Mover's step-up honors this (GroundMovementUtils::CanStepUpOnHitSurface),
	// so a chasing Chalkie slides along the hero's capsule instead of climbing on top of them (and
	// nobody stands on corpses' capsules). Blocking is unaffected — they still can't pass through.
	Capsule->CanCharacterStepUpOn = ECB_No;
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

	// Motion warping. Self-wiring: UMoverComponent::BeginPlay does FindComponentByClass on this and creates a
	// UMotionWarpingMoverAdapter for it — nothing here or in the Mover config needs to reference the other.
	// Idle unless a montage carries a MotionWarping notify AND gameplay has registered a matching named
	// target, so simply owning it costs nothing per frame.
	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));

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
	// RETIRED as the reaction path (2026-07-21): direct attribute sets (the vitals set's SetHealth in
	// PostGameplayEffectExecute) fire this delegate with GEModData = NULL, so the effect CAUSER is
	// unavailable here — the damage lock and scream silently no-op'd every hit. All on-hit reaction
	// now runs through HandleDamaged(), called by the vitals set WITH the causer. Kept bound for UI.
}

void AAZ_PawnMoverInfectedCharacter::HandleDamaged(AActor* Causer, float Damage)
{
	if (MoverComponent && !MoverComponent->IsActive())
	{
		return;   // already a corpse
	}
	UE_LOG(LogTemp, Display, TEXT("[Vitals] %s HandleDamaged by %s (lock+scream+stagger path)"),
		*GetName(), *GetNameSafe(Causer));

	// RULE 2 (fight rulebook): being hit = instant unconditional lock on a hostile attacker —
	// strongest stimulus, no cone, no range, no alert beat; fires on EVERY damaging hit.
	if (AAZ_InfectedAIController* Chalkie = Cast<AAZ_InfectedAIController>(GetController()))
	{
		Chalkie->NotifyDamagedBy(Cast<APawn>(Causer));
	}

	// Being hit ALWAYS interrupts the zombie's own attack + halts pathing + SCREAMS — independent of
	// whether a flinch asset exists (audit rules-finding #10: three rules were gated behind one
	// optional DA field).
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		// Child-safe cancel (BP-first grants): match the melee by IsChildOf, not exact class — the
		// granted spec may be BP_GA_ZombieMelee. Collect first, cancel after (cancel mutates the list).
		TArray<FGameplayAbilitySpecHandle> MeleeHandles;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->GetClass()->IsChildOf(UAZ_GA_ZombieMelee::StaticClass()))
			{
				MeleeHandles.Add(Spec.Handle);
			}
		}
		for (const FGameplayAbilitySpecHandle& Handle : MeleeHandles)
		{
			ASC->CancelAbilityHandle(Handle);
		}
	}
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
	}
	// STAGGER SCREAM — the loudest beacon in the alley: pack hearing converges on a screaming
	// pack-mate. Attributed to the ATTACKER (pack hearing filters friendly sources) at THIS location.
	if (Causer)
	{
		// Loudness MULTIPLIES the listener's HearingRange: tunable per-PAWN (BP vars on BP_AZ_Chalkie,
		// per-variant later via DA_ChalkieConfig — a Screamer variant carries across the block).
		UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(),
			UAZ_GA_MeleeAttack::ReadConfigFloat(this, TEXT("ScreamLoudness"), 2.f), Causer,
			UAZ_GA_MeleeAttack::ReadConfigFloat(this, TEXT("ScreamMaxRange"), 1400.f), FName("Scream"));
	}

	// FULL STAGGER, EVERY HIT (user direction 2026-07-21) — arch step A: one event, and GA_HitReact owns
	// the whole reaction (montage via task, capsule via generation-scoped RM, Staggered gate via
	// ActivationOwnedTags, beat ended by the montage's own BeatEnd notify). The GRAB CARVE-OUT (rule 8)
	// that used to be an early return here is now the ability's ActivationBlockedTags=Grabbing — the
	// event is sent regardless and simply doesn't activate, which is exactly the design: everything
	// ABOVE this line (lock, melee-cancel, scream) fires for a grabbing Chalkie too; only the flinch
	// MOTION is armored away. Payload Instigator = causer (directional variants later).
	FGameplayEventData Payload;
	Payload.Instigator = Causer;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		this, FAZ_GameplayTags::Get().Event_Combat_HitReact, Payload);
}

void AAZ_PawnMoverInfectedCharacter::PlayStepBackReaction(UAnimMontage* Montage, float HoldSeconds)
{
	if (MoverComponent && !MoverComponent->IsActive())
	{
		return;   // already a corpse (same guard as HandleDamaged — don't animate recoils on the dead)
	}
	if (!Montage)
	{
		return;
	}
	// Arch step A: thin shim — the horde call site stays put, GA_HitReact owns the reaction. Payload
	// carries the clip and the crowd's beat (EventMagnitude; 0 = the clip's own beat/BeatEnd notify —
	// the caller may SHORTEN the beat, never lengthen it past the authored notify). Two triggers into
	// one ability keep flinch and step-back on the same Staggered gate with last-writer-wins preemption
	// (retrigger), matching how they already preempted each other on the montage slot.
	FGameplayEventData Payload;
	Payload.OptionalObject = Montage;
	Payload.EventMagnitude = HoldSeconds;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		this, FAZ_GameplayTags::Get().Event_Combat_StepBack, Payload);
}

bool AAZ_PawnMoverInfectedCharacter::IsStaggerReactionPlaying() const
{
	// A tag query, nothing else. The old form asked the ANIM SYSTEM (Montage_IsPlaying on the flinch
	// resolved by reflection + the last step-back pointer), which made AI pacing a side effect of blend
	// timing — cutting a montage 0.4s earlier silently made the whole pack swing sooner. The tag is
	// GA_HitReact's ActivationOwnedTags: set and cleared by the ability's lifecycle, no timer to desync.
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	return ASC && ASC->HasMatchingGameplayTag(FAZ_GameplayTags::Get().State_Combat_Staggered);
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

	// Publish the corpse's true state BEFORE detaching (audit: the Aggressive tag lived on replicated
	// state forever): dead things are Dormant.
	if (UAZ_AbilitySystemComponent* ASC = Cast<UAZ_AbilitySystemComponent>(AbilitySystemComponent))
	{
		const FAZ_GameplayTags& AZTags = FAZ_GameplayTags::Get();
		ASC->RemoveStateTag(AZTags.State_Infected_Aggressive);
		ASC->RemoveStateTag(AZTags.State_Infected_Alerted);
		ASC->AddStateTag(AZTags.State_Infected_Dormant);
	}

	// Brain off (controller detaches + BT stops); movement sim off — but DEFERRED (audit finding,
	// both agents): GA_Death queues the collapse root-motion move in this same call stack; an
	// immediate Deactivate discards it and the body collapses in place instead of where the clip
	// says. RagdollDelay = 0.6 x montage length, so /0.6 recovers the full clip duration.
	DetachFromControllerPendingDestroy();
	if (MoverComponent)
	{
		const float DeactivateDelay = (RagdollDelay > 0.f) ? (RagdollDelay / 0.6f) + 0.1f : 0.f;
		if (DeactivateDelay > 0.f)
		{
			FTimerHandle DeactivateTimer;
			GetWorldTimerManager().SetTimer(DeactivateTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (MoverComponent)
				{
					MoverComponent->Deactivate();
				}
			}), DeactivateDelay, false);
		}
		else
		{
			MoverComponent->Deactivate();
		}
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
		// EDITOR-ASSIGNED grants (user rule 2026-07-24: no hardcoded asset paths in C++): the pawn BP
		// points these at the BP tuning children; unset falls back to the native class. The CDO patches
		// (triggers / activation tags) go to the ASSIGNED class — a BP child's CDO does NOT inherit
		// runtime patches made to the native CDO.
		UClass* DeathClass    = *DeathAbilityClass    ? *DeathAbilityClass    : UAZ_GA_Death::StaticClass();
		UClass* MeleeClass    = *MeleeAbilityClass    ? *MeleeAbilityClass    : UAZ_GA_ZombieMelee::StaticClass();
		UClass* GrabClass     = *GrabAbilityClass     ? *GrabAbilityClass     : UAZ_GA_ChalkieGrab::StaticClass();
		UClass* HitReactClass = *HitReactAbilityClass ? *HitReactAbilityClass : UAZ_GA_HitReact::StaticClass();
		UAZ_GA_Death::ConfigureTriggerOnCDO(DeathClass);
		UAZ_GA_ChalkieGrab::ConfigureCDO(GrabClass);
		UAZ_GA_HitReact::ConfigureOnCDO(HitReactClass);
		// (No melee CDO patch: "can't swing while Staggered" is a CanActivateAbility override on
		//  UAZ_GA_ZombieMelee — ActivationBlockedTags is read off the ability INSTANCE, which does not
		//  inherit a CDO patched at runtime. Same trap that silently disabled the Staggered gate itself.)
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(DeathClass, 1, INDEX_NONE, this));
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(MeleeClass, 1, INDEX_NONE, this));
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(GrabClass, 1, INDEX_NONE, this));
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(HitReactClass, 1, INDEX_NONE, this));
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
