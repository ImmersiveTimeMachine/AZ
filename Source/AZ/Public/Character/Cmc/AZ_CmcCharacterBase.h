// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagAssetInterface.h"
#include "GenericTeamAgentInterface.h"
#include "Animation/AZ_LocomotionTypes.h"   // EAZ_Gait
#include "Animation/AZ_CmcAnimTypes.h"      // FAZ_CmcAnimContract (the anim seam)
#include "Character/AZ_CombatAvatar.h"
#include "Character/AZ_JumpRequester.h"
#include "AZ_CmcCharacterBase.generated.h"

class UAbilitySystemComponent;
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
 *  - Motion warping (native on ACharacter — the Mover generation needed an RM-attribute bridge for it).
 *  - Modular-mesh follower wiring, so MetaHuman/garment parts ride the main mesh.
 *
 * ContextualAnimation was REMOVED here (2026-08-16): GASP 5.8 shipped PoseSearch Interaction, CAS stayed
 * unchanged and moved deeper into Plugins/Experimental, and Epic's own sample uses it nowhere. Paired
 * moments (executions, grab v2) now target PSI instead — see project_gasp58_update_audit.
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

	// ========================================
	// Landing — GASP's JustLanded latch. The anim layer needs "did we land recently AND how hard", but
	// CMC zeroes the fall velocity the moment it lands, so a puller can never observe it. We latch both
	// at the landing event and hold them for JustLandedDuration. Re-landing inside the window restarts
	// the timer rather than stacking (retriggerable), so a stair-bounce reads as one landing.
	// ========================================
	virtual void Landed(const FHitResult& Hit) override;

	UFUNCTION(BlueprintPure, Category = "AZ|Movement")
	bool IsJustLanded() const { return bJustLanded; }

	/** Velocity captured AT the landing event, before CMC discards it. */
	UFUNCTION(BlueprintPure, Category = "AZ|Movement")
	FVector GetLandVelocity() const { return LandVelocity; }

	/**
	 * Aim rotation, correct for both ends of the wire: the local machine owns ControlRotation at full
	 * rate, while remote proxies only ever see the compressed replicated BaseAimRotation.
	 */
	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	FRotator GetAimRotation() const;

	// ========================================
	// The animation seam — ONE virtual, ONE struct, once per frame.
	// ========================================

	/**
	 * Publishes every raw fact the animation layer is allowed to know about this pawn.
	 *
	 * This is deliberately the ONLY channel. UAZ_CmcAnimInstance holds a pointer to this base class and
	 * calls nothing else on it, so anything able to fill the struct can drive the graph — a vehicle
	 * seat, a mounted turret, a cutscene proxy — without the anim instance growing a second pawn type
	 * to cast to. Three pawn generations of concrete casts (v1 refs still in 15 files, GAs still casting
	 * the v2 pawn) are what this exists to stop happening a fourth time.
	 *
	 * Raw facts only: nothing derived belongs here. Direction, foot phase, lean, aim offset and the
	 * trajectory are the anim instance's job, because they need curves, frame history, or both.
	 *
	 * Children EXTEND rather than replace — call Super first, then fill what only they can know (the
	 * hero's rotation mode and input flags come from its own tags).
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Anim")
	virtual void FillAnimContract(FAZ_CmcAnimContract& Out) const;

protected:
	/**
	 * Wires every anim-class-less skeletal mesh hanging off GetMesh() as a leader-pose follower.
	 *
	 * Modular characters (MetaHuman face + garments, the SurvivalMan outfit parts) attach extra skeletal
	 * meshes to the main mesh. Assigning LeaderPoseComponent on the BP template is NOT enough: the pose
	 * only propagates once SetLeaderPoseComponent() actually RUNS, because the setter is what registers
	 * the component in the leader's FollowerPoseComponents list and rebuilds the follower bone map.
	 * The setter also early-outs when the incoming value already matches the serialized one, so the
	 * template assignment forces us to pass bForceUpdate — otherwise this is a silent no-op.
	 *
	 * Selection is "every skeletal mesh child EXCEPT ModularFollowerExclusions". An earlier version keyed
	 * off "has no AnimClass", which breaks the moment someone assigns an ABP to a garment hoping it will
	 * animate itself — the exact thing this function exists to make unnecessary. Garments must NOT run
	 * their own graph: each ABP instance would run its own motion-matching search and drift out of sync
	 * with the body. Leader pose copies the body's final pose bone-for-bone instead, which is both exact
	 * and nearly free (a follower skips its own pose evaluation entirely).
	 *
	 * Cross-skeleton is fine here — UpdateLeaderBoneMap() maps purely by bone name and never consults
	 * CompatibleSkeletons, so SurvivalMan-skinned garments ride a MetaHuman body as long as names match.
	 */
	void WireModularMeshFollowers();

	/**
	 * Skeletal mesh children that keep their own anim graph and must never become followers.
	 * "Face" is the MetaHuman head: it is driven by ABP_Face + RigLogic on the face skeleton.
	 * Coupling to that name adds no new fragility — UMetaHumanComponentBase::GetSkelMeshComponentByName
	 * already resolves the face by this exact string, so the name is load-bearing either way.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Pawn")
	TArray<FName> ModularFollowerExclusions { FName("Face") };

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

	// ---- Landing latch ----

	/** How long IsJustLanded() stays true after touchdown (GASP uses 0.3s). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement", meta = (ForceUnits = "s"))
	float JustLandedDuration = 0.3f;

	bool bJustLanded = false;
	FVector LandVelocity = FVector::ZeroVector;
	FTimerHandle JustLandedTimer;

	// ---- CAS readiness (committed direction: paired/contextual moments = CAS scenes + warping) ----

	/** Motion warping — native on ACharacter: warp windows on montages deform root motion with zero
	 *  adapter glue (the Mover generation needed the RM-attribute bridge for this). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Movement")
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

	// ---- Team ----

	/** Default team id for this class: hero 0, infected 1 (ctor of each child). Resolved into TeamId in
	 *  PostInitializeComponents so a BP-child override of the default is honored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|AI")
	uint8 DefaultTeamId = 0;

	FGenericTeamId TeamId = FGenericTeamId::NoTeam;
};
