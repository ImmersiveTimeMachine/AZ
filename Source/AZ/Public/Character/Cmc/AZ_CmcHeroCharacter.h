// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "Character/Cmc/AZ_CmcCharacterBase.h"
#include "AZ_CmcHeroCharacter.generated.h"

class UAZ_GameplayAbility;
class UCameraComponent;
class UGameplayAbility;
class UInputAction;
class UInputMappingContext;
class USpringArmComponent;
struct FInputActionValue;

/**
 * AAZ_CmcHeroCharacter — the CMC (v3) hero. [SPIKE: spike/cmc-backport]
 *
 * From-scratch ACharacter hero, inspired by (not copied from) both predecessors:
 *  - v2 AZ_PawnMoverHeroCharacter: GAS grant sites (Configure-on-the-RESOLVED-class pattern), Enhanced
 *    Input surface, gait semantics, the grab seams (now behind IAZ_CombatAvatar)./sta
 *  - v1 AZ_HeroCharacter: the CMC layout itself — but tunables live directly ON the CMC (every CMC
 *    property is BP-editable already), not mirrored into pawn UPROPERTYs.
 *
 * What the engine now owns that v2 hand-built: root-motion → capsule (montages just work), motion
 * warping (native), crouch (Crouch()/UnCrouch()), jump (Jump(), gated by GA via IAZ_JumpRequester),
 * client prediction + replication (CMC), attachability (live pawns can be attached — the grab anchor
 * layered move has no reason to exist here).
 *
 * MM trajectory: NO trajectory component — UAZ_MoverAnimInstance's CMC branch generates the trajectory
 * itself via UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectory (the production 5.8
 * for-Character path, CMC-simulated prediction). One owner for trajectory; nothing to keep in sync.
 *
 * P0 scope: possessable, input moves the capsule, camera framed, GAS granted. Camera stances, strafe,
 * gait input toggles and the cinematic grab camera are P2.
 */
UCLASS(config = Game, BlueprintType)
class AZ_API AAZ_CmcHeroCharacter : public AAZ_CmcCharacterBase
{
	GENERATED_BODY()

public:
	AAZ_CmcHeroCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	// ========================================
	// IAbilitySystemInterface — the player ASC lives on the PlayerState (persists across pawn switch).
	// ========================================
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** Extends the base contract with the two facts only the hero has: which rotation mode the player is
	 *  in, and the raw input desires. Both come from the same Movement.* tags the gait/stance resolve
	 *  reads, so abilities stay movement-agnostic and unchanged across both pawn generations. */
	virtual void FillAnimContract(FAZ_CmcAnimContract& Out) const override;

	// ========================================
	// IAZ_CombatAvatar — grab-victim seams (P0: state storage; camera/mesh reactions land in P2).
	// ========================================
	virtual void SetGrabFacingTarget(const AActor* Target) override { GrabFacingTarget = Target; }
	virtual const AActor* GetGrabFacingTarget() const override { return GrabFacingTarget.Get(); }
	virtual void SetGrabOutcomeFraming(bool bInOutcomeFraming) override { bGrabOutcomeFraming = bInOutcomeFraming; }
	virtual void SetGrabIKReleased(bool bInReleased) override { bGrabIKReleased = bInReleased; }
	virtual bool IsGrabIKReleased() const override { return bGrabIKReleased; }

	/** PC pulls this on possess to push the pawn's IMC — kept name-compatible with the v2 pawn so the
	 *  PC stays pawn-agnostic. The pawn ALSO pushes it itself in PawnClientRestart (self-contained). */
	UFUNCTION(BlueprintPure, Category = "AZ|Input")
	UInputMappingContext* GetDefaultMappingContext() const { return DefaultMappingContext; }

	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	UCameraComponent* GetCamera() const { return Camera; }

protected:
	void InitAbilitySystem();

	void OnMoveTriggered(const FInputActionValue& Value);
	void OnLookTriggered(const FInputActionValue& Value);

	/**
	 * Per-frame derived CMC parameters — GASP's "PreCMCTick" pass, ported.
	 *
	 * GASP does not treat braking/acceleration/friction/turn rate as constants: it recomputes them every
	 * frame from current speed and whether the stick is held, which is most of why its locomotion reads
	 * heavy at speed and crisp at a standstill. This runs BEFORE the movement component ticks (see the
	 * prerequisite set up in PostInitializeComponents) so CMC consumes this frame's values, not last
	 * frame's. It is the ONE writer of these four properties; the constructor no longer sets them.
	 *
	 * Hero-only on purpose: the same tuning applied to the infected would silently change Chalkie chase
	 * feel and its BT rotation tracking, which this spike never asked for.
	 */
	void ApplyMovementFeelParams();

	/**
	 * Translates MOVEMENT-domain GAS tags into CMC state, mirroring exactly what the v2 pawn packs into
	 * its Mover InputCmd (AZ_PawnMoverHeroCharacter.cpp:847). The abilities are unchanged and stay
	 * movement-agnostic — they only ever grant Movement.* tags; this pawn is what turns those into a gait
	 * and into native Crouch()/UnCrouch(). That keeps GA_Run/GA_Crouch working on BOTH generations
	 * without a Mover reference anywhere in them.
	 */
	void ResolveGaitAndStanceFromTags();

	// ---- Derived-parameter tuning (GASP 5.8 SandboxCharacter_CMC reference values) ----

	/** Braking while the stick is held vs released — the released value is what stops the slide. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	float BrakingDecelWithInput = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	float BrakingDecelNoInput = 2000.f;

	/** Acceleration tapers from Base down to AtTopSpeed across the speed window below, so top speed is
	 *  approached rather than snapped to. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	float MaxAccelerationBase = 800.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	float MaxAccelerationAtTopSpeed = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel", meta = (ForceUnits = "cm/s"))
	float AccelTaperSpeedMin = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel", meta = (ForceUnits = "cm/s"))
	float AccelTaperSpeedMax = 700.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	float GroundFrictionMax = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	float GroundFrictionMin = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel", meta = (ForceUnits = "cm/s"))
	float FrictionTaperSpeedMax = 500.f;

	/** Grounded yaw rate. NEGATIVE means instant in CMC — GASP turns the capsule instantly and lets the
	 *  ABP's OffsetRootBone carry the visual smoothing (our stack has that node, so the pairing holds).
	 *  If turning reads too snappy, this is the first dial: try 500 to restore the pre-spike behaviour. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	float GroundedRotationRateYaw = -1.f;

	/** In air the capsule turns at a finite rate — instant mid-air rotation reads as a glitch. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	float FallingRotationRateYaw = 200.f;

	// ========================================
	// Components
	// ========================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Pawn")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Pawn")
	TObjectPtr<UCameraComponent> Camera;

	// ========================================
	// Input (Enhanced Input) — EDITOR-ASSIGNED in the BP child (no /Game/ paths in C++).
	// ========================================
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputAction> MoveInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputAction> LookInputAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	float LookRateYaw = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Input")
	float LookRatePitch = 1.f;

	// ========================================
	// GAS — grant sites mirror v2 (grant the RESOLVED class, patch ITS CDO; BP children do not inherit
	// runtime patches to the native CDO — doctrine rule 2).
	// ========================================

	/** Abilities granted to the player ASC on first possession (server-side, idempotent). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|GAS")
	TArray<TSubclassOf<UAZ_GameplayAbility>> StartupAbilities;

	/** Grab-victim ability — point at BP_GA_PlayerGrabbed in the BP child; unset = native fallback. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Abilities")
	TSubclassOf<UGameplayAbility> GrabbedAbilityClass;

	/** The hero's stagger-class on-hit reaction — BP_GA_HitReact_Hero; unset = native fallback. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Abilities")
	TSubclassOf<UGameplayAbility> HitReactAbilityClass;

	// ---- Grab-victim state (read by the anim/camera layers via IAZ_CombatAvatar) ----
	TWeakObjectPtr<const AActor> GrabFacingTarget;
	bool bGrabOutcomeFraming = false;
	bool bGrabIKReleased = false;
};
