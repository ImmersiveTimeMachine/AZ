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
 *    Input surface, gait semantics, the grab seams (now behind IAZ_CombatAvatar).
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

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	// ========================================
	// IAbilitySystemInterface — the player ASC lives on the PlayerState (persists across pawn switch).
	// ========================================
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

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
