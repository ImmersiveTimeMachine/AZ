// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagAssetInterface.h"
#include "GenericTeamAgentInterface.h"
#include "Animation/AZ_LocomotionTypes.h"   // EAZ_Gait
#include "Character/AZ_CombatAvatar.h"
#include "Character/AZ_JumpRequester.h"
#include "AZ_CmcCharacterBase.generated.h"

class UAbilitySystemComponent;
class UContextualAnimSceneActorComponent;
class UMotionWarpingComponent;

/**
 * AAZ_CmcCharacterBase — shared base of the CMC (v3) character generation. [SPIKE: spike/cmc-backport]
 *
 * ACharacter-based, built from scratch per the CMC back-port spike plan: the movement/root-motion/warping
 * glue the Mover generation had to hand-build (RM-attribute bridge, DriveRootMotion, grab anchor, custom
 * crouch/jump modes) is ENGINE-NATIVE here, so this class owns only what the engine doesn't:
 *  - GAS access + tag routing (hero resolves the PlayerState ASC, infected owns its own — virtual accessor).
 *  - Team identity for AI perception.
 *  - Gait → MaxWalkSpeed (the ONE write point for gait speed — one owner per fact).
 *  - The combat-avatar seam (IAZ_CombatAvatar) abilities talk to instead of concrete pawn casts.
 *  - CAS readiness: MotionWarping + ContextualAnimSceneActor components live HERE because both fighting
 *    actors warp and join contextual scenes (committed direction: paired moments = CAS + warping, both
 *    native on ACharacter — see project_contextual_anim_mover_assessment).
 *
 * Interfaces mirror the v2 pawns so every system treats all generations uniformly.
 */
UCLASS(Abstract, config = Game, BlueprintType)
class AZ_API AAZ_CmcCharacterBase
	: public ACharacter
	, public IAbilitySystemInterface
	, public IGameplayTagAssetInterface
	, public IGenericTeamAgentInterface
	, public IAZ_JumpRequester
	, public IAZ_CombatAvatar
{
	GENERATED_BODY()

public:
	AAZ_CmcCharacterBase(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;

	// ========================================
	// IAbilitySystemInterface — children resolve their ASC (hero: PlayerState; infected: own component).
	// ========================================
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return nullptr; }

	// ========================================
	// IGameplayTagAssetInterface — tag queries route through the ASC (mirrors v2).
	// ========================================
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;

	// ========================================
	// IGenericTeamAgentInterface — AI perception / faction.
	// ========================================
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamId) override { TeamId = NewTeamId; }
	virtual FGenericTeamId GetGenericTeamId() const override { return TeamId; }

	// ========================================
	// IAZ_JumpRequester — GAS gates the request, CMC executes natively.
	// ========================================
	virtual void SetJumpPressed(bool bPressed) override;
	virtual bool CanRequestJump() const override { return CanJump(); }

	// ========================================
	// IAZ_CombatAvatar
	// ========================================
	virtual USkeletalMeshComponent* GetCombatMesh() const override { return GetMesh(); }

	// ========================================
	// Gait — the ONE write point for movement speed (one owner per fact). BT / input / abilities call
	// this; nothing else touches MaxWalkSpeed.
	// ========================================
	UFUNCTION(BlueprintCallable, Category = "AZ|Movement")
	void SetGait(EAZ_Gait NewGait);

	UFUNCTION(BlueprintPure, Category = "AZ|Movement")
	EAZ_Gait GetCurrentGait() const { return CurrentGait; }

	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	UMotionWarpingComponent* GetMotionWarpingComponent() const { return MotionWarpingComponent; }

protected:
	// ---- Gait speeds (cm/s) — v2 semantics carried over (Walk 165 / Run 375 / Sprint 585). Get the
	// C++ defaults right BEFORE creating BP children: a BP child serializes its own copy and later C++
	// default changes will NOT reach it (doctrine rule 1 — the GrabHoldDistance lesson). ----
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Speeds", meta = (ForceUnits = "cm/s"))
	float WalkSpeed = 165.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Speeds", meta = (ForceUnits = "cm/s"))
	float RunSpeed = 375.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Speeds", meta = (ForceUnits = "cm/s"))
	float SprintSpeed = 585.f;

	/** Crouched speed — applied to CMC MaxWalkSpeedCrouched (native crouch owns the stance). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Speeds", meta = (ForceUnits = "cm/s"))
	float CrouchSpeed = 90.f;

	/** Live gait, written only by SetGait. */
	EAZ_Gait CurrentGait = EAZ_Gait::Run;

	// ---- CAS readiness (committed direction: paired/contextual moments = CAS scenes + warping) ----

	/** Motion warping — native on ACharacter: warp windows on montages deform root motion with zero
	 *  adapter glue (the Mover generation needed the RM-attribute bridge for this). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Movement")
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

	/** Contextual Anim scene membership — CAS's component requires an ACharacter owner + a
	 *  MotionWarpingComponent (ContextualAnimSceneActorComponent.cpp:208), both satisfied here. Idle
	 *  until a scene binds this actor, so owning it costs nothing per frame. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|ContextualAnim")
	TObjectPtr<UContextualAnimSceneActorComponent> ContextualAnimComponent;

	// ---- Team ----

	/** Default team id for this class: hero 0, infected 1 (ctor of each child). Resolved into TeamId in
	 *  PostInitializeComponents so a BP-child override of the default is honored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|AI")
	uint8 DefaultTeamId = 0;

	FGenericTeamId TeamId = FGenericTeamId::NoTeam;
};
