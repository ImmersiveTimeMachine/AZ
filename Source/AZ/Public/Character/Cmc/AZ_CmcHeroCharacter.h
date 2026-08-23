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
	void ApplyMovementFeelParams(float DeltaSeconds);

	// ---- Curve-driven stop braking: the capsule tracks what the clip depicts ----

	/** Make the capsule follow the selected stop clip's own MoveData_Speed curve instead of a solved
	 *  constant. Zero slide by construction at any release speed, and stopping DISTANCE becomes a
	 *  property of the content rather than a tuned number.
	 *
	 *  ⚠ This INVERTS the ownership doctrine for stops: everywhere else CMC drives and animation
	 *  follows (which is why RootMotionMode is montages-only). Legitimate — many shipped games do
	 *  exactly this for stops — but it is a real doctrine change and will interact with network
	 *  prediction later, so it is a flag rather than a silent default.
	 *
	 *  Falls back to the latched v0/T contract whenever the curve reads non-positive: no stop selected,
	 *  the clip carries no MoveData_Speed, or the clip has reached its plant. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Stop")
	bool bStopCurveBraking = true;

	/** Ceiling on the per-frame correction. The frame a stop is selected the body's speed need not equal
	 *  the clip's speed at the chosen entry frame, and an unclamped (Speed - ClipSpeed)/dt would apply
	 *  that whole gap in one tick as a visible velocity step. MM picks the entry by trajectory so they
	 *  should be close; this bounds the case where they are not. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Stop")
	float StopCurveMaxBraking = 4000.f;

	/**
	 * Handover guards — the clip may only drive the capsule if the two AGREE when it takes over.
	 *
	 * Curve braking assumes the clip's phase corresponds to this stop's progress. That is false when the
	 * selection is a CONTINUING POSE inherited from an earlier stop: tapping the stick repeatedly latches
	 * a fresh contract at full walk speed while the clip is still at 0.78s depicting 25 cm/s. Measured
	 * 2026-08-23 — twenty such handovers in five seconds, every one pinned at StopCurveMaxBraking, each a
	 * visible snap. Trusting the clip there is trusting a clip that is describing a different stop.
	 *
	 * Both guards must pass. Speed agreement alone is not enough (a spent clip and a slow body can agree
	 * by coincidence), and phase alone is not enough (an early frame of the wrong gait's clip is still wrong).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Stop", meta = (ForceUnits = "cm/s"))
	float StopCurveMaxHandoverStep = 120.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Stop", meta = (ForceUnits = "s"))
	float StopCurveMaxHandoverClipTime = 0.45f;

	/**
	 * How long the capsule takes to converge onto the clip's speed curve.
	 *
	 * The first cut divided by DeltaSeconds, i.e. closed the whole gap in ONE frame. That made every
	 * disagreement a velocity step, which is why the handover gate had to be tight enough to reject
	 * two thirds of stops. But most of those disagreements are legitimate: with two stop variants per
	 * gait the foot phase can be a quarter-cycle off, so Motion Matching enters mid-clip where the
	 * phase matches and the clip has already decelerated (measured 2026-08-23: entering at 0.27s with
	 * the clip depicting 117 while the body was at 148 — a real 31 cm/s gap, and a correct entry).
	 *
	 * Converging over a window turns that gap from a snap into a lean: 31 cm/s over 0.1s is 310 cm/s²,
	 * which reads as weight. The gate stays as a backstop for genuinely absurd handovers rather than
	 * being the thing that decides most stops.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Stop", meta = (ForceUnits = "s"))
	float StopCurveConvergenceTime = 0.1f;

	/** Latches the "curve engaged" log to once per stop. Cleared when the stop contract releases. */
	bool bStopCurveEngaged = false;

	/** Set when a stop's handover was REJECTED, so the decision holds for the whole stop instead of being
	 *  retested every frame — the clip advances and the body slows, so a rejected stop would otherwise
	 *  flicker into curve driving partway through and produce the very step the guard exists to prevent. */
	bool bStopCurveRejected = false;

	/**
	 * Translates MOVEMENT-domain GAS tags into CMC state, mirroring exactly what the v2 pawn packs into
	 * its Mover InputCmd (AZ_PawnMoverHeroCharacter.cpp:847). The abilities are unchanged and stay
	 * movement-agnostic — they only ever grant Movement.* tags; this pawn is what turns those into a gait
	 * and into native Crouch()/UnCrouch(). That keeps GA_Run/GA_Crouch working on BOTH generations
	 * without a Mover reference anywhere in them.
	 */
	void ResolveGaitAndStanceFromTags();

	// ---- Derived-parameter tuning (GASP 5.8 SandboxCharacter_CMC reference values) ----

	/** Braking while the stick is HELD. The released-stick value is chosen per gait below. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	float BrakingDecelWithInput = 500.f;

	/** Released-stick braking when bGaitScaledBraking is OFF. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	float BrakingDecelNoInput = 500.f;

	/**
	 * Released-stick braking, per gait.
	 *
	 * MEASURED, not chosen: foot slide on a stop is exactly
	 *     clipStopTime - (GaitSpeed / braking)
	 * so the only real decision is WHERE inside the clip's ~0.9s the character decelerates. Sampling the
	 * stop clips' own root-motion speed (2026-08-23) gives the deceleration each ANIMATION depicts:
	 *   WalkFwdStop_LU  147 cm/s -> 0 at 0.92s = 160    WalkFwdStop_RU  162 -> 0 at 0.86s = 188
	 *   RunFwdStop_LU   353 cm/s -> 0 at 0.95s = 372    RunFwdStop_RU   388 -> 0 at 1.03s = 377
	 * Each default below is gait speed / ~0.95s. One shared value cannot serve three gaits: at a single
	 * 500 the walk stop still slid 0.59s while sprint OVERSHOT by 0.22s (capsule still gliding after the
	 * feet had planted). Sprint reuses the run stop clips, so its number is derived, not measured.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel", meta = (EditCondition = "bGaitScaledBraking"))
	float WalkBrakingDecel = 190.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel", meta = (EditCondition = "bGaitScaledBraking"))
	float RunBrakingDecel = 375.f;

	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel", meta = (EditCondition = "bGaitScaledBraking"))
	float SprintBrakingDecel = 615.f;

	/** Off = every gait uses BrakingDecelNoInput (the single-value behaviour before 2026-08-23). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	bool bGaitScaledBraking = true;

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

	/** Grounded yaw rate at WALK gait, and the fallback whenever gait-specific turning is off.
	 *
	 *  MEASURED, not guessed: the capsule has to turn at the rate the sustained-turn content was authored
	 *  at, or motion matching cannot match a held curve and reaches for the violent turn clips instead
	 *  (pivots at 186 deg/s, 180-degree starts at 208) — which is what "it over-leans in a turn" looks
	 *  like. AnimPro arc loops: walk 180 deg/s, run 93-116, and sprint has no arc content at all.
	 *
	 *  NEGATIVE means instant in CMC. GASP turns instantly and lets OffsetRootBone carry the visual
	 *  smoothing — but our AnimGraph has no OffsetRootBone yet (Stage C), so instant reads as a snap
	 *  AND spikes the trajectory. Keep these finite until that node lands. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel", meta = (ForceUnits = "deg/s"))
	float GroundedRotationRateYaw = 180.f;

	/** Run gait yaw rate — matches RunArchLoop_L/R (93 / 116 deg/s). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel", meta = (ForceUnits = "deg/s"))
	float RunRotationRateYaw = 115.f;

	/** Sprint gait yaw rate. Sprint owns no arc content, so this is a design choice rather than a
	 *  measurement: slowest of the three, so a sprint turn carves wide instead of pivoting on the spot. */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel", meta = (ForceUnits = "deg/s"))
	float SprintRotationRateYaw = 90.f;

	/** Off = every grounded gait uses GroundedRotationRateYaw (the pre-2026-08-19 single-rate behaviour). */
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Movement|Feel")
	bool bGaitScaledRotationRate = true;

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
