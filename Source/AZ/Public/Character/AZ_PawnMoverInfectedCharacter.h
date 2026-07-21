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
};
