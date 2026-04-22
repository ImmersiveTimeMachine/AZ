#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemInterface.h"
#include "MoverSimulationTypes.h"
#include "MoverDataModelTypes.h"
#include "AZ_GameplayTags.h"
#include "Animation/AZ_LocomotionTypes.h"
#include "Interaction/AZ_CombatInterface.h"
#include "Character/IAZ_SandboxCharacterPawn.h"
#include "AZ_HeroPawn.generated.h"

class UCharacterMoverComponent;
class UCharacterTrajectoryComponent;
class UMoverTrajectoryPredictor;
class UNetworkPredictionComponent;
class USpringArmComponent;
class UCameraComponent;
class UAZ_AbilitySystemComponent;
class UAZ_EquipmentManagerComponent;
class UAZ_PawnCameraMovementComponent;
class AAZ_Weapon;
class AAZ_PlayerState;
class UInputAction;
struct FInputActionValue;

/**
 * FAZ_MoverStateProxy: Thread-safe snapshot of Mover state for AnimInstance access.
 * Populated on the game thread during Tick, safe to read from animation worker threads.
 */
USTRUCT(BlueprintType)
struct AZ_API FAZ_MoverStateProxy
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Mover")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Mover")
	float GroundSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Mover")
	bool bIsOnGround = true;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Mover")
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Mover")
	bool bIsCrouching = false;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Mover")
	bool bIsSprinting = false;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Mover")
	bool bIsAiming = false;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Mover")
	FName MovementModeName = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Mover")
	FVector GroundNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Mover")
	FVector GroundLocation = FVector::ZeroVector;
};

/**
 * AAZ_HeroPawn: APawn-based hero using UCharacterMoverComponent (Mover plugin)
 * instead of ACharacter/CharacterMovementComponent.
 *
 * Key differences from the old AAZ_HeroCharacter:
 *  - No capsule component — collision is on the UpdatedComponent (set via Mover)
 *  - Movement is driven by UCharacterMoverComponent (replaces UCharacterMovementComponent)
 *  - Input is produced via IMoverInputProducerInterface::ProduceInput
 *  - GAS tags are read each frame and mapped to FAZ_MoverInputCmd flags
 *  - PrimaryActorTick.TickGroup = TG_PrePhysics (matching AnimBP tick)
 */
UCLASS(config = Game, BlueprintType)
class AZ_API AAZ_HeroPawn : public APawn, public IAbilitySystemInterface, public IAZ_CombatInterface, public IMoverInputProducerInterface, public IAZ_SandboxCharacterPawn
{
	GENERATED_BODY()

public:
	AAZ_HeroPawn(const FObjectInitializer& ObjectInitializer);

	// ========================================
	// APawn overrides
	// ========================================
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;

	/** Align the controller's control rotation with the actor's facing yaw.
	 *  Called from PossessedBy (primary — guaranteed controller) and from BeginPlay
	 *  (fallback — covers pawns already possessed before BeginPlay fires, such as
	 *  default player pawns in PIE). Idempotent; safe to call multiple times.
	 *  Prevents the spawn-time FutureFacingDelta spike that makes
	 *  ShouldTurnInPlace() fire while SM=IdleLoop → chooser has no matching row → A-pose. */
	void AlignControllerWithActor();
	virtual void OnRep_PlayerState() override;
	virtual void PostInitializeComponents() override;

	// ========================================
	// IAbilitySystemInterface
	// ========================================
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "AZ|Abilities")
	UAbilitySystemComponent* GetASC() const;

	// ========================================
	// IAZ_CombatInterface
	// ========================================
	virtual FOnASCRegistered& GetOnASCRegisteredDelegate() override;
	FOnASCRegistered OnAscRegistered;

	// ========================================
	// IMoverInputProducerInterface
	// ========================================
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;

	// ========================================
	// Components — Public Accessors
	// ========================================

	UFUNCTION(BlueprintPure, Category = "AZ|Mover")
	UCharacterMoverComponent* GetMoverComponent() const { return CharacterMoverComponent; }

	UFUNCTION(BlueprintPure, Category = "AZ|Mesh")
	USkeletalMeshComponent* GetMainMesh() const { return MeshComponent; }

	UFUNCTION(BlueprintPure, Category = "AZ|Equipment")
	UAZ_EquipmentManagerComponent* GetEquipmentManagerComponent() const { return EquipmentManagerComponent; }

	UFUNCTION(BlueprintPure, Category = "AZ|Camera")
	UAZ_PawnCameraMovementComponent* GetCameraMovementComponent() const { return CameraMovementComponent; }

	/** Current local move input (before world-space rotation). Used by camera offset system. */
	UFUNCTION(BlueprintPure, Category = "AZ|Input")
	const FVector& GetCachedMoveInputIntent() const { return CachedMoveInputIntent; }

	/** Thread-safe snapshot of Mover state. Populated during Tick on game thread, safe for AnimInstance worker thread reads. */
	UFUNCTION(BlueprintPure, Category = "AZ|Mover")
	const FAZ_MoverStateProxy& GetMoverStateSafe() const { return MoverStateProxy; }

	// ========================================
	// Components — Visible
	// ========================================

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "AZ|Mesh")
	TObjectPtr<USkeletalMeshComponent> MeshComponent;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "AZ|Camera")
	TObjectPtr<USpringArmComponent> ThirdPersonCameraBoom;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "AZ|Camera")
	TObjectPtr<UCameraComponent> ThirdPersonCamera;

	/** Mover-native trajectory predictor for PoseSearch Motion Matching. Replaces UCharacterTrajectoryComponent (ACharacter-only). */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "AZ|MotionMatching")
	TObjectPtr<UMoverTrajectoryPredictor> MoverTrajectoryPredictor;

	// ========================================
	// Input Actions (assigned in editor)
	// ========================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputAction> MoveInputAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputAction> LookInputAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputAction> JumpInputAction;

	/** Hold to sprint (GASP IA_Sprint). Sets bWantsToSprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputAction> SprintInputAction;

	/** Hold to walk slower than default run (GASP IA_Walk). Sets bWantsToWalk. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputAction> WalkInputAction;

	/** Hold to strafe (body faces camera, move sideways) (GASP IA_Strafe). Sets bWantsToStrafe. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Input")
	TObjectPtr<UInputAction> StrafeInputAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Input")
	float LookRateYaw = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Input")
	float LookRatePitch = 1.f;

	// ========================================
	// Player Input State (GASP S_PlayerInputState parity)
	// ========================================

	/** Holds raw player intent flags (Sprint/Walk/Strafe/Aim/Crouch).
	 *  GASP pattern: Enhanced Input handlers toggle fields here, OnProduceInput
	 *  and derived functions (Get_Gait / Get_RotationMode) read them.
	 *  Sprint/Walk/Strafe are set by this pawn's bindings.
	 *  Aim/Crouch are driven by GAS abilities (left untouched here). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Input")
	FAZ_PlayerInputState PlayerInputState;

	// ========================================
	// Abilities
	// ========================================

	UPROPERTY(EditAnywhere, Category = "AZ|Abilities|Startup")
	TArray<TSubclassOf<class UAZ_GameplayAbility>> StartupAbilities;

	UPROPERTY(EditAnywhere, Category = "AZ|Abilities|Passive")
	TArray<TSubclassOf<class UAZ_GameplayAbility>> StartupPassiveAbilities;

	UPROPERTY(EditAnywhere, Category = "AZ|Abilities|Input")
	TArray<TSubclassOf<class UAZ_GameplayAbility>> CharacterInputAbilities;

	// ========================================
	// GAS Attribute Init
	// ========================================

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Attributes")
	TSubclassOf<UGameplayEffect> DefaultPrimaryAttributes;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Attributes")
	TSubclassOf<UGameplayEffect> DefaultSecondaryAttributes;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Attributes")
	TSubclassOf<UGameplayEffect> DefaultVitalAttributes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Attributes")
	TSubclassOf<UAttributeSet> DefaultAttributeSetClass;

protected:
	// ========================================
	// Mover Input Production
	// ========================================

	/** Override in native subclasses to extend input. Call Super. */
	virtual void OnProduceInput(float DeltaMs, FMoverInputCmdContext& InputCmdResult);

	// ========================================
	// Components — Protected
	// ========================================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Movement", Transient)
	TObjectPtr<UCharacterMoverComponent> CharacterMoverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Movement", Transient)
	TObjectPtr<UNetworkPredictionComponent> NetworkPredictionComponent;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "AZ|Equipment")
	TObjectPtr<UAZ_EquipmentManagerComponent> EquipmentManagerComponent;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "AZ|Camera")
	TObjectPtr<UAZ_PawnCameraMovementComponent> CameraMovementComponent;

	/** Game-thread snapshot of Mover state — updated in Tick, read by AnimInstance. */
	FAZ_MoverStateProxy MoverStateProxy;

	// ========================================
	// GASP Pre/Post Sim Input State (matches GASP SandboxCharacter_Mover pattern)
	// ========================================
	// _PreSim copies are built locally each frame, NOT replicated, fed to Mover.
	// _PostSim copies are pulled FROM Mover after sim (via CacheInputsFromMover)
	// and ARE the replicated state anim/camera should read for network correctness.

	/** Local frame's CharacterDefaultInputs built before Mover simulates. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Mover")
	FCharacterDefaultInputs MoverDefaultInputs_PreSim;

	/** Post-sim CharacterDefaultInputs pulled from Mover.GetLastInputCmd.
	 *  Replicated by Mover — anim / camera should read THIS, not _PreSim. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Mover")
	FCharacterDefaultInputs MoverDefaultInputs_PostSim;

	/** Local frame's custom (AZ-specific) inputs before Mover simulates. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Mover")
	FAZ_MoverCustomInputs MoverCustomInputs_PreSim;

	/** Post-sim custom inputs pulled from Mover. Replicated. Read for anim/camera. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Mover")
	FAZ_MoverCustomInputs MoverCustomInputs_PostSim;

	/** Pull last-simulated input cmd from Mover and store into _PostSim copies.
	 *  Called each Tick AFTER Mover has simulated the frame (see EventGraph
	 *  tick order in GASP's SandboxCharacter_Mover — Mover ticks first via
	 *  AddTickPrerequisiteComponent on BeginPlay). */
	void CacheInputsFromMover();

	/** AZ-specific idle-TIP accumulator — speed-independent commit on 60° of
	 *  cumulative mouse-yaw motion. Replaces GASP's raw |delta| ≥ 60° check.
	 *  Called from OnProduceInput before Get_OrientationIntent reads bIdleTurnInProgress. */
	void Update_IdleTIPAccumulator();

	/** Tracks controller yaw rate (deg/sec). GASP Update_ControlRotationRate.
	 *  Packed into MoverCustomInputs_PreSim.ControlRotationRate; consumed by
	 *  rotation-mode logic / camera systems for "is the player whipping the camera?" checks. */
	void Update_ControlRotationRate(float DeltaSeconds);

	// ========================================
	// Phase 6 — Movement mode change delegate
	// ========================================

	/** Bound to UMoverComponent::OnMovementModeChanged in BeginPlay. */
	UFUNCTION()
	void HandleMovementModeChanged(const FName& PreviousMode, const FName& NewMode);

	/** Last mode name observed by the change handler. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Mover")
	FName PreviousMovementModeName;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Mover")
	FName CurrentMovementModeName;

	// ========================================
	// Phase 7 — Side systems (minimal)
	// ========================================

	/** Trace inputs for traversal / vault detection. Defaults sensible for human-scale pawn. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Mover")
	FAZ_TraversalCheckInputs Get_TraversalCheckInputs() const;

	/** On-screen overlay (gait, mode, direction, TIP). Toggled by `bShowAZDebug` cvar/flag. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Mover")
	void DebugDraws() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Mover")
	bool bShowPawnDebug = false;

	// ========================================
	// Phase 8 — IAZ_SandboxCharacterPawn implementation
	// ========================================

	virtual FAZ_CharacterPropertiesForAnimation GetPropertiesForAnimation_Implementation() const override;
	virtual FAZ_CharacterPropertiesForCamera    GetPropertiesForCamera_Implementation() const override;
	virtual FAZ_CharacterPropertiesForTraversal GetPropertiesForTraversal_Implementation() const override;
	virtual void                                SetCharacterInputState_Implementation(const FAZ_PlayerInputState& NewState) override;

	// ========================================
	// GASP Derivation Getters (Phase 2 of port)
	// ========================================
	// Pure-ish functions: return derived state from PlayerInputState / Mover /
	// CachedMoveInputIntent / TargetedActor / TwinStickMode. Mirror GASP's
	// SandboxCharacter_Mover Get_* functions; consumed by OnPreSimulateTick
	// (Phase 3) to pack MoverDefaultInputs_PreSim + MoverCustomInputs_PreSim.

	/** World-space movement intent (normalized), rotated by camera yaw.
	 *  GASP Get_MoveInput — player-path only (AI NavMovement branch not yet ported). */
	UFUNCTION(BlueprintPure, Category = "AZ|Mover")
	FVector Get_MoveInput() const;

	/** Camera/aim rotation. GASP Get_AimingRotation — three-way:
	 *  TargetedActor look-at → TwinStickAimRotation → ControlRotation.
	 *  TargetedActor/TwinStickMode paths defer to Phase 5 (Update_* methods). */
	UFUNCTION(BlueprintPure, Category = "AZ|Mover")
	FRotator Get_AimingRotation() const;

	/** Ground speed — horizontal magnitude of Mover velocity. */
	UFUNCTION(BlueprintPure, Category = "AZ|Mover")
	double Get_Speed() const;

	/** Current movement mode — maps Mover FName (e.g. "Walking") through MovementModeMap. */
	UFUNCTION(BlueprintPure, Category = "AZ|Mover")
	EAZ_MovementMode Get_CurrentMovementMode() const;

	/** Rotation mode — GASP Get_RotationMode. Three-tier:
	 *  (1) TargetedActor valid: Aim if bWantsToAim else Strafe.
	 *  (2) TwinStickMode with right stick deflected: Aim/Strafe.
	 *  (3) Default: bWantsToAim → Aim, bWantsToStrafe → Strafe, else OrientToMovement. */
	UFUNCTION(BlueprintPure, Category = "AZ|Mover")
	EAZ_RotationMode Get_RotationMode() const;

	/** Gait — GASP Get_Gait. Uses PlayerInputState bWantsToWalk/bWantsToSprint,
	 *  MoverDefaultInputs_PreSim (for strafe dot-test), rotation mode. */
	UFUNCTION(BlueprintPure, Category = "AZ|Mover")
	EAZ_Gait Get_Gait() const;

	/** Per-mode orientation intent matrix. GASP Get_OrientationIntent.
	 *  AZ divergence: OnGround+Idle+Aim uses our speed-independent accumulator
	 *  (bIdleTurnInProgress) instead of GASP's raw |delta| ≥ 60° check. */
	UFUNCTION(BlueprintPure, Category = "AZ|Mover")
	FVector Get_OrientationIntent() const;

	/** Mover movement-mode FName → AZ enum. Mirrors GASP MovementModeMap.
	 *  Default populated in ctor: Walking→OnGround, Falling→InAir,
	 *  Sliding→Slide, Flying→Traversing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Mover")
	TMap<FName, EAZ_MovementMode> MovementModeMap;

	// ========================================
	// MovementDirection helpers (Phase 4)
	// ========================================

	/** Angle thresholds (degrees) defining F/B/L/R quadrants. Defaults from struct. */
	UFUNCTION(BlueprintPure, Category = "AZ|Mover")
	FAZ_MovementDirectionThresholds Get_MovementDirectionThresholds() const;

	/** Bucket a signed angle (deg, +CW from forward) into AZ's 6-value direction enum.
	 *  Foot-phase (LL/LR/RL/RR) selected by carrying prior frame's foot-bias for hysteresis. */
	UFUNCTION(BlueprintPure, Category = "AZ|Mover")
	EAZ_MovementDirection Get_MovementDirectionFromAngle(const FAZ_MovementDirectionThresholds& Thresholds,
	                                                     double SignedAngleDegrees) const;

	/** Combined: returns current movement direction + residual rotation offset.
	 *  Offset = angle from canonical direction heading to actual move heading,
	 *  used by anim to rotate strafe loops to match velocity yaw. */
	UFUNCTION(BlueprintCallable, Category = "AZ|Mover")
	void Get_MovementDirectionAndOffset(EAZ_MovementDirection& OutDirection, double& OutRotationOffset) const;

	// ========================================
	// GAS
	// ========================================

	UPROPERTY()
	TObjectPtr<UAZ_AbilitySystemComponent> AZ_AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UAttributeSet> AttributeSet;

	void InitDefaultAttributes();
	void ApplyEffectToSelf(const TSubclassOf<UGameplayEffect>& GameplayEffectClass, float Level) const;
	void InitDefaultAbilities() const;

	// ========================================
	// Equipment Socket Map
	// ========================================

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Equipment")
	TMap<FGameplayTag, FName> EquipmentSocketMap;

	// ========================================
	// Cached Input State
	// ========================================

	FVector CachedMoveInputIntent = FVector::ZeroVector;
	bool bIsJumpJustPressed = false;
	bool bIsJumpPressed = false;

	/** Cached last-valid idle orientation target (camera forward on XY plane). */
	FVector LastIdleOrientationTarget = FVector::ZeroVector;

	/** Accumulated absolute mouse rotation (degrees) since last TIP commit.
	 *  Speed-independent trigger: any 60° of cumulative camera yaw movement
	 *  fires TIP, regardless of how fast or slow the mouse moved. */
	float AccumulatedYawSinceCommit = 0.f;
	float LastObservedControllerYaw = 0.f;
	bool bAccumYawInitialized = false;

	/** True between commit and alignment — single source of truth for "turn in progress".
	 *  AnimInstance reads this to drive bIsTurning / TIP. OrientationIntent is only
	 *  emitted when this is true (otherwise body stays put). */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Mover")
	bool bIdleTurnInProgress = false;

	/** Controller yaw rate (degrees/second) — updated each Tick by Update_ControlRotationRate. */
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Mover")
	double ControlRotationRate = 0.0;

	/** Last frame's controller yaw — used to compute ControlRotationRate. */
	double LastControlRotationYaw = 0.0;
	bool bControlRotationRateInitialized = false;

public:
	bool IsIdleTurnInProgress() const { return bIdleTurnInProgress; }

private:
	// Enhanced Input callbacks
	void OnMoveTriggered(const FInputActionValue& Value);
	void OnMoveCompleted(const FInputActionValue& Value);
	void OnLookTriggered(const FInputActionValue& Value);
	void OnLookCompleted(const FInputActionValue& Value);
	void OnJumpStarted(const FInputActionValue& Value);
	void OnJumpReleased(const FInputActionValue& Value);

	// GASP-style hold-to-toggle state inputs → FAZ_PlayerInputState
	void OnSprintStarted(const FInputActionValue& Value)  { PlayerInputState.bWantsToSprint = true; }
	void OnSprintReleased(const FInputActionValue& Value) { PlayerInputState.bWantsToSprint = false; }
	void OnWalkStarted(const FInputActionValue& Value)    { PlayerInputState.bWantsToWalk = true; }
	void OnWalkReleased(const FInputActionValue& Value)   { PlayerInputState.bWantsToWalk = false; }
	void OnStrafeStarted(const FInputActionValue& Value)  { PlayerInputState.bWantsToStrafe = true; }
	void OnStrafeReleased(const FInputActionValue& Value) { PlayerInputState.bWantsToStrafe = false; }
};
