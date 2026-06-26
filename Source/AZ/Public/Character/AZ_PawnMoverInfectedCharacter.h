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
class UNetworkPredictionComponent;
class UMoverTrajectoryPredictor;
class UCapsuleComponent;
class USkeletalMeshComponent;

/**
 * AAZ_PawnMoverInfectedCharacter — the "Chalkie" infected enemy (v2 Mover NPC pawn).
 *
 * A STANDALONE sibling of AAZ_PawnMoverHeroCharacter (NOT a subclass): same v2 Mover stack, but WITHOUT the
 * player's camera + Enhanced Input, and with its OWN AbilitySystemComponent (NPCs have no PlayerState). It is
 * animated by its OWN AnimInstance, UAZ_InfectedAnimInstance — an independent copy of the player's MM pipeline
 * (so the infected can diverge freely; the hero's UAZ_MoverAnimInstance is left untouched). That copy casts to
 * THIS pawn and pulls GetMoverComponent()/GetTrajectoryPredictor(), exactly as the hero AnimInstance does for the
 * hero pawn — hence this pawn keeps its own trajectory predictor.
 *
 * How it is driven: the AIController (AAZ_InfectedAIController) — and later a BehaviorTree / NavMesh path-follow —
 * writes the SERVER-side intent surface (SetMoveIntentWorld / SetDesiredFacingWorld / SetGait). ProduceInput turns
 * that into the deterministic Mover InputCmd. No PlayerController, so facing comes from the AI's desired heading
 * (face-target / face-movement), which the walking mode honours via OrientationIntent.
 *
 * Interfaces (mirror the hero so every system treats both pawns uniformly):
 *  - IAbilitySystemInterface      — returns this pawn's OWN ASC (created here).
 *  - IMoverInputProducerInterface — AI ProduceInput (world-space intent in, deterministic InputCmd out).
 *  - IGameplayTagAssetInterface   — tag queries route through the ASC.
 *  - IGenericTeamAgentInterface   — faction for AI perception; defaults to the player's enemy team.
 *
 * Foundation scope: pawn + own ASC + team + AI intent plumbing + Mover stack. Health/attributes, melee abilities,
 * perception, BehaviorTree and NavMesh path-following (via UNavMoverComponent) are later steps.
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
	UMoverTrajectoryPredictor* GetTrajectoryPredictor() const { return TrajectoryPredictor; }

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

	// ========================================
	// GAS — own ASC (NPC pattern; not on a PlayerState).
	// ========================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|GAS")
	TObjectPtr<UAZ_AbilitySystemComponent> AbilitySystemComponent;

	// ========================================
	// Mover stack
	// ========================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Mover", Transient)
	TObjectPtr<UAZ_PawnMoverComponent> MoverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Mover", Transient)
	TObjectPtr<UNetworkPredictionComponent> NetworkPredictionComponent;

	/** PoseSearch trajectory predictor (Mover-native). UAZ_InfectedAnimInstance pulls this each tick to build the
	 *  FTransformTrajectory for motion matching + intent-based IsMoving — same as the hero. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|MotionMatching")
	TObjectPtr<UMoverTrajectoryPredictor> TrajectoryPredictor;

	/** "Where can I move" clearance clamp — reused from the hero so the AI doesn't run-in-place into walls.
	 *  Intent-pure (clamps the move INTENT in ProduceInput), so the anim stays predictive. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Movement")
	TObjectPtr<UAZ_MovementDirectionCapabilityComponent> MovementCapability;

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
