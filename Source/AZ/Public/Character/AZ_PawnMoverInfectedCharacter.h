// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagAssetInterface.h"
#include "GenericTeamAgentInterface.h"
#include "MoverSimulationTypes.h"               // IMoverInputProducerInterface, FMoverInputCmdContext
#include "Animation/AZ_LocomotionTypes.h"       // EAZ_Gait
#include "AZ_PawnMoverInfectedCharacter.generated.h"

class UAbilitySystemComponent;
class UAZ_AbilitySystemComponent;
class UAZ_PawnMoverComponent;
class UAZ_MovementDirectionCapabilityComponent;
class UNavMoverComponent;
class UNetworkPredictionComponent;
class UCapsuleComponent;
class USkeletalMeshComponent;
class UMotionWarpingComponent;

/**
 * AAZ_PawnMoverInfectedCharacter — the "Chalkie" infected enemy (v2 Mover NPC pawn).
 *
 * A STANDALONE sibling of AAZ_PawnMoverHeroCharacter (NOT a subclass): same v2 Mover stack, but WITHOUT the
 * player's camera + Enhanced Input, and with its OWN AbilitySystemComponent (NPCs have no PlayerState). It is
 * animated by its OWN AnimInstance, UAZ_InfectedAnimInstance — a slim CLASSIC locomotion driver (Option B:
 * state-machine / blendspace, NOT Motion Matching; the hero's UAZ_MoverAnimInstance is left untouched). That
 * driver casts to THIS pawn and reads GetMoverComponent()->GetVelocity() (physics -> anim); no trajectory
 * prediction (an MM-only artifact) is needed — hence this pawn no longer carries a trajectory predictor.
 *
 * How it is driven: TWO inputs feed ProduceInput, nav first, intent surface as fallback.
 *  1. NavMesh path-follow — the AIController's MoveTo/MoveToActor (later issued by a BehaviorTree) routes through
 *     the engine UNavMoverComponent (INavMovementInterface, auto-discovered by PathFollowing); ProduceInput
 *     consumes its cached request via ConsumeNavMovementData and collapses it to a unit direction (speed stays
 *     gait-driven — the walking mode is the single speed authority).
 *  2. The raw AI intent surface (SetMoveIntentWorld / SetDesiredFacingWorld / SetGait) — direct steering with no
 *     pathing (scripted nudges, dormant twitch-turns), used only when no nav move is in flight.
 * Either way ProduceInput turns it into the deterministic Mover InputCmd. No PlayerController, so facing comes
 * from the AI's desired heading (face-target / face-movement), which the walking mode honours via OrientationIntent.
 *
 * Interfaces (mirror the hero so every system treats both pawns uniformly):
 *  - IAbilitySystemInterface      — returns this pawn's OWN ASC (created here).
 *  - IMoverInputProducerInterface — AI ProduceInput (world-space intent in, deterministic InputCmd out).
 *  - IGameplayTagAssetInterface   — tag queries route through the ASC.
 *  - IGenericTeamAgentInterface   — faction for AI perception; defaults to the player's enemy team.
 *
 * Foundation scope: pawn + own ASC + team + AI intent plumbing + Mover stack + NavMesh path-follow bridge.
 * Health/attributes, melee abilities, perception and the BehaviorTree brain are later steps.
 */
UCLASS(config = Game, BlueprintType)
class AZ_API AAZ_PawnMoverInfectedCharacter
	: public APawn
	, public IAbilitySystemInterface
	, public IMoverInputProducerInterface
	, public IGameplayTagAssetInterface
	, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	AAZ_PawnMoverInfectedCharacter(const FObjectInitializer& ObjectInitializer);

	// ========================================
	// APawn
	// ========================================
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;

	// ========================================
	// IAbilitySystemInterface — this pawn owns its ASC (no PlayerState for NPCs).
	// ========================================
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ========================================
	// IMoverInputProducerInterface — AI input producer (no PlayerController / camera).
	// ========================================
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;

	// ========================================
	// IGameplayTagAssetInterface — route through the ASC.
	// ========================================
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;

	// ========================================
	// IGenericTeamAgentInterface — AI perception / faction.
	// ========================================
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamId) override;
	virtual FGenericTeamId GetGenericTeamId() const override;

	// ========================================
	// AI intent surface — written by the AIController / BehaviorTree on the SERVER, read by ProduceInput.
	// ========================================

	/** Set the desired WORLD-space move intent (direction * 0..1 magnitude). Zero = stand still. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AI|Intent")
	void SetMoveIntentWorld(const FVector& WorldIntent);

	/** Set an explicit WORLD-space facing direction (e.g. face the target). Zero = face the move direction. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AI|Intent")
	void SetDesiredFacingWorld(const FVector& WorldFacing);

	/** Set the gait (Walk / Run / Sprint) the walking mode resolves to a speed. */
	UFUNCTION(BlueprintCallable, Category = "AZ|AI|Intent")
	void SetGait(EAZ_Gait NewGait);

	// ========================================
	// Component accessors (used by UAZ_InfectedAnimInstance, mirroring the hero)
	// ========================================
	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	UAZ_PawnMoverComponent* GetMoverComponent() const { return MoverComponent; }

	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	USkeletalMeshComponent* GetMesh() const { return Mesh; }

	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	UNavMoverComponent* GetNavMoverComponent() const { return NavMoverComponent; }

	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	UMotionWarpingComponent* GetMotionWarpingComponent() const { return MotionWarpingComponent; }

	// ========================================
	// Components (public like the hero so the BP details panel exposes them)
	// ========================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Pawn")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Pawn")
	TObjectPtr<USkeletalMeshComponent> Mesh;

protected:
	/** Bind ASC ActorInfo (owner = avatar = this pawn). Server grants startup content in a later step. */
	void InitAbilitySystem();

public:
	/** Vitals Health decreased but not to zero -> variant flinch (anim-set HitReactMontage, played as a
	 *  ~1.1s window of the long KnockBack clip). Cosmetic SP glue — becomes a GameplayCue in the batch. */
	void HandleHealthChanged(const struct FOnAttributeChangeData& Data);

	/** THE on-hit entry point (survivable hits), called by UAZ_VitalsAttributeSet::PostGameplayEffectExecute
	 *  with the REAL effect causer (the attribute-change delegate loses it — GEModData is null for direct
	 *  attribute sets). Fires the damage-lock rule, the stagger scream, and the full-stagger flinch. */
	void HandleDamaged(AActor* Causer, float Damage);

	/** PACK STEP-BACK: the recoil beat when a packmate seizes the prey (horde subsystem calls this on
	 *  every OTHER Chalkie engaged on that prey). Arch step A: a thin shim — sends Event.Combat.StepBack
	 *  (payload: Montage + HoldSeconds as the crowd's beat; <= 0 = the clip's own beat) and GA_HitReact
	 *  does the rest: montage task, generation-scoped capsule RM, the Staggered gate. */
	void PlayStepBackReaction(UAnimMontage* Montage, float HoldSeconds);

	/** True while a stagger-class reaction owns the body — hit-react flinch OR the pack step-back. The
	 *  attack BT task refuses to swing on this, so a new claw can't cancel the recoil's root motion.
	 *  A State.Combat.Staggered tag query — the tag is GA_HitReact's ActivationOwnedTags, set/cleared by
	 *  the ability lifecycle (beat ended by the montage's BeatEnd notify) — NOT Montage_IsPlaying, whose
	 *  blend timing used to set AI pacing by accident. Wrapper kept so the BT call site never changes. */
	bool IsStaggerReactionPlaying() const;

	/**
	 * ★ THE ONE OWNER of State.Combat.Staggered's lifetime for causes that are NOT a hit reaction.
	 *
	 * The tag means one thing — "this body is reeling and may not act" — but it now has two causes: a hit
	 * reaction (owned by UAZ_GA_HitReact for its montage beat) and being shoved off a grab. Letting each
	 * cause set and clear the tag independently is the counted-tag trap in a new costume: the grab's
	 * clear-timer would wipe a stagger that a punch applied a moment later, silently handing the player's
	 * hit back to the AI.
	 *
	 * So non-ability causes funnel through here, LAST WRITER WINS on a single shared timer — the
	 * semantics this project already settled on once (the arch-step-A′ SetStaggeredFor, removed when
	 * GA_HitReact briefly became the only source). Re-extending simply pushes the deadline out; it never
	 * shortens a longer hold that is already running.
	 *
	 * GA_HitReact still owns its own window through its ability lifetime. If it is active when this
	 * expires the tag stays up, because the ability re-asserts it — the clear here only releases what
	 * THIS call put on.
	 */
	void SetStaggeredFor(float Seconds);

private:
	/** Shared deadline behind SetStaggeredFor. One timer, so two causes cannot each hold a clear. */
	FTimerHandle StaggerHoldTimer;
	double StaggerHoldEndTime = 0.0;

	/** True while a reaction ability (asset tag Ability.Combat.HitReact) is running. Asked before this
	 *  pawn's own hold clears the tag, so a timer started by a shove cannot cut short a reaction that
	 *  began afterwards. Matched by IDENTITY TAG rather than class so a BP tuning child still counts. */
	bool IsStaggerReactionAbilityActive() const;

public:

	/** How long the hit-react clip is allowed to drive the CAPSULE, in seconds. 0 = the whole montage.
	 *
	 *  The Zombie_*_KnockBack_Chase clips are ROUND TRIPS, not knockbacks: they recoil, hold, then walk
	 *  back in — the re-approach is authored into the animation. Measured root paths:
	 *      Zombie_Chase_2_KnockBack_Chase (3.10s): out to 118cm by 1.39s (peak 150 cm/s), hold to 1.86s,
	 *                                              then returns, ending 25cm from where it started.
	 *      Zombie_Chase_5_KnockBack_Chase (4.03s): out to  85cm by 1.81s, hold to 2.02s, then returns.
	 *  Both peak at ~45% of clip length. Driving the capsule for the FULL clip makes a punched Chalkie
	 *  stumble back and then stroll straight back at you ~1.5s later, which reads as a delayed reaction
	 *  rather than a knockback.
	 *
	 *  Cutting the bridge at the top of the recoil keeps the whole 118cm and stops there. The MONTAGE is
	 *  cut at the same beat (0.4s blend), and the stagger gate is the State.Combat.Staggered tag held for
	 *  beat + blend — so how long a punched Chalkie stays unable to attack is an EXPLICIT number set here,
	 *  not a side effect of clip length. Default 1.5 ⇒ ~1.9s gate (was 3.1-4.0s when the gate was
	 *  Montage_IsPlaying on the full clip — that pacing change is deliberate: stun-lock is per-hit
	 *  re-triggered anyway, and pack pressure is the difficulty, per the fight rulebook).
	 *
	 *  FALLBACK ONLY since arch step B: when the anim set carries a "HitReact" FAZ_CombatMontage
	 *  descriptor, per-clip timing from THAT wins and this knob is ignored. This one per-pawn number
	 *  survives solely for variants whose descriptor isn't authored yet (1.5 cuts Chase_5 slightly
	 *  before its 1.81s peak — the exact mismatch the descriptor exists to fix). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Combat", meta = (ClampMin = "0", ForceUnits = "s"))
	float FlinchRootMotionSeconds = 1.5f;

	/** The prey we are currently grabbing, or null. Set by GA_ChalkieGrab at the catch and cleared on
	 *  every exit; UAZ_InfectedAnimInstance reads it to aim the grab hand-IK at the victim's grip sockets.
	 *  Mirror of the hero's GetGrabFacingTarget — the anim layer must not reach into GAS or the AI tree. */
	void SetGrabTarget(AActor* InTarget) { GrabTarget = InTarget; }
	AActor* GetGrabTarget() const { return GrabTarget.Get(); }

	/** Corpse-ification, called by UAZ_GA_Death after it starts the (replicated) death montage:
	 *  brain off, collision off, mover off, ragdoll at RagdollDelay (0 = instantly), despawn.
	 *  Idempotent — lifespan doubles as the death latch. */
	void BeginCorpse(float RagdollDelay);

	/** Montage->ragdoll hand-off at the fall's impact beat: the authored clip sells the hit, physics
	 *  settles the corpse against geometry (an animated fall ignores walls — bodies clipped through). */
	void RagdollCorpse();

protected:

	// ========================================
	// GAS — own ASC (NPC pattern; not on a PlayerState).
	// ========================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|GAS")
	TObjectPtr<UAZ_AbilitySystemComponent> AbilitySystemComponent;

	/** Shared combat vitals (S1 damage spine) — owner-subobject, auto-registered with the ASC.
	 *  Defaults set in the ctor (infected are 2-punch kills at the spine's 25 default). */
	UPROPERTY()
	TObjectPtr<class UAZ_VitalsAttributeSet> VitalsAttributeSet;

	/** One-shot guard for the native startup grants (InitAbilitySystem is re-entrant). */
	bool bStartupAbilitiesGranted = false;

	// (Arch step A: the reaction timers that lived here — StepBackCutTimer, FlinchCutTimer,
	//  StaggerTagTimer/SetStaggeredFor — are GONE. GA_HitReact owns the whole reaction lifecycle; the
	//  beat is ended by the montage's own Event.Combat.BeatEnd notify, and the Staggered gate is the
	//  ability's ActivationOwnedTags. Events drive, timers guard — and the guards live in the ability.)

	/** Prey held by the current grab (see SetGrabTarget). Weak: the victim can die or be destroyed
	 *  mid-hold, and the anim layer must simply stop reaching rather than chase a dangling pointer. */
	TWeakObjectPtr<AActor> GrabTarget;

	// ========================================
	// Mover stack
	// ========================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Mover", Transient)
	TObjectPtr<UAZ_PawnMoverComponent> MoverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Mover", Transient)
	TObjectPtr<UNetworkPredictionComponent> NetworkPredictionComponent;

	/** "Where can I move" clearance clamp — reused from the hero so the AI doesn't run-in-place into walls.
	 *  Intent-pure (clamps the move INTENT in ProduceInput), so the anim stays predictive. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Movement")
	TObjectPtr<UAZ_MovementDirectionCapabilityComponent> MovementCapability;

	/** Nav<->Mover bridge (engine): implements INavMovementInterface so the AIController's PathFollowingComponent
	 *  auto-discovers it. Path-follow requests (BT MoveTo / MoveToActor) are cached here and consumed by
	 *  ProduceInput each sim tick — nav data wins over the raw AI intent cache when a move is in flight.
	 *  Also carries the RVO avoidance surface for Phase-5 hordes (Detour Crowd / avoidance masks). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Mover")
	TObjectPtr<UNavMoverComponent> NavMoverComponent;

	/** Motion warping (engine): deforms the root-motion delta of an attack montage so the swing lands on a
	 *  moving target. Its ONLY wiring cost is existing — UMoverComponent::BeginPlay finds it by class and
	 *  builds a UMotionWarpingMoverAdapter, which binds ProcessLocalRootMotionDelegate. From then on our
	 *  existing RM route (FLayeredMove_RootMotionAttribute -> ConvertLocalRootMotionToWorld) routes every
	 *  root-motion montage through the warp modifiers. No component = delegate unbound = zero cost. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Mover")
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

	// ========================================
	// AI intent cache (server-written by the controller/BT; read by ProduceInput each sim tick)
	// ========================================
	FVector CachedAIMoveIntentWorld = FVector::ZeroVector;
	FVector CachedAIDesiredFacingWorld = FVector::ZeroVector;
	EAZ_Gait CachedAIGait = EAZ_Gait::Walk;

	// ========================================
	// Team
	// ========================================
	/** Default faction. 1 = enemy of the player (team 0). Read by AI perception. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|AI")
	uint8 DefaultTeamId = 1;

	/** Live team id, overridable at runtime via SetGenericTeamId. */
	FGenericTeamId TeamId = FGenericTeamId::NoTeam;

public:
	// ========================================
	// Crowd (coordination group — distinct from Team, which is friend/foe for perception)
	// ========================================
	/** Which crowd this Chalkie coordinates with. Chalkies sharing a CrowdId form ONE crowd: the horde
	 *  brain gives each crowd its own attacker budget, ring circle and turn cadence. Set this per PLACED
	 *  instance to split a level into distinct packs; the default "Default" keeps everyone in one crowd
	 *  (identical to the pre-crowd behavior). EditAnywhere so it is authorable on each level actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Crowd")
	FName CrowdId = TEXT("Default");

	/** Starting aggression for THIS Chalkie's crowd, 1 (wary) .. 5 (frenzy). The FIRST member of a crowd
	 *  to spawn seeds the crowd's level; thereafter change it live via UAZ_HordeSubsystem::SetCrowdIntensity
	 *  (or the az.Crowd.Intensity test override). Members of the same crowd should share this value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Crowd", meta = (ClampMin = "1", ClampMax = "5"))
	int32 CrowdIntensity = 3;

	// ========================================
	// Startup abilities — EDITOR-ASSIGNED (no hardcoded /Game/ paths in C++, user rule 2026-07-24).
	// Point these at the BP tuning children (BP_GA_Death / BP_GA_ZombieMelee / BP_GA_ChalkieGrab) in
	// the pawn BP; native classes are only the ctor fallback. The grant site patches the CDO of the
	// ASSIGNED class (triggers/tags), and the BT attack task resolves to the granted concrete class.
	// ========================================
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Abilities")
	TSubclassOf<class UGameplayAbility> DeathAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Abilities")
	TSubclassOf<class UGameplayAbility> MeleeAbilityClass;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Abilities")
	TSubclassOf<class UGameplayAbility> GrabAbilityClass;

	/** The stagger-class reaction (flinch + pack step-back). BP child overrides for tuning; native
	 *  UAZ_GA_HitReact when unset. Event-triggered — see its ConfigureOnCDO. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Abilities")
	TSubclassOf<class UGameplayAbility> HitReactAbilityClass;
};
