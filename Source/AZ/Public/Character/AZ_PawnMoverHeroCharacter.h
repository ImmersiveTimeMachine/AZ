// Copyright Artur. AZ project.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemInterface.h"
#include "Character/AZ_JumpRequester.h"
#include "Character/AZ_PawnCameraMovementComponent.h"   // FAZ_CameraStanceConfig (per-mode camera framing)
#include "GameplayTagAssetInterface.h"
#include "GenericTeamAgentInterface.h"
#include "MoverSimulationTypes.h"
#include "AZ_PawnMoverHeroCharacter.generated.h"

class UAbilitySystemComponent;
class UAZ_GameplayAbility;
class UAZ_PawnMoverComponent;
class UNetworkPredictionComponent;
class UMoverTrajectoryPredictor;
class UCapsuleComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * AAZ_PawnMoverHeroCharacter — v2 hero pawn (foundation skeleton).
 *
 * APawn-based, Mover-driven. Lean foundation per the v2 architecture:
 * components only at this stage; per-frame state derivation, GASP Get_* functions,
 * and chooser context are owned by the AnimInstance (UAZ_PawnMoverAnimInstance).
 * Rotation policy and RM bridging belong to the Mover mode (added in a later step).
 *
 * Interfaces:
 *  - IAbilitySystemInterface       — GAS bridge (returns PlayerState ASC; nullptr until Step 4).
 *  - IMoverInputProducerInterface  — Mover input producer (zero-input stub until Step 2).
 *  - IGameplayTagAssetInterface    — tag queries route through the ASC; lets any system
 *                                    (Niagara, AI perception, UMG, gameplay code) check
 *                                    the pawn's tag state without knowing about GAS.
 *  - IGenericTeamAgentInterface    — AI perception / faction. Player default team id;
 *                                    AI subclass overrides at spawn.
 *
 * INavAgentInterface is inherited from APawn — no override needed.
 *
 * Implementation order (architecture doc):
 *  Step 1 — this class: components + lifecycle stubs + interface stubs.
 *  Step 2 — input wiring (Enhanced Input → ProduceInput → Mover).
 *  Step 3 — UAZ_PawnMoverSmoothWalkingMode with ResolveRotationTarget() virtual.
 *  Step 4 — GAS link (ASC accessor → PlayerState), tag taxonomy.
 *  Step 5+ — per-state work (Idle → Start → Locomotion → ...).
 */
UCLASS(config = Game, BlueprintType)
class AZ_API AAZ_PawnMoverHeroCharacter
	: public APawn
	, public IAbilitySystemInterface
	, public IMoverInputProducerInterface
	, public IGameplayTagAssetInterface
	, public IGenericTeamAgentInterface
	, public IAZ_JumpRequester
{
	GENERATED_BODY()

public:
	AAZ_PawnMoverHeroCharacter(const FObjectInitializer& ObjectInitializer);

	// ========================================
	// APawn
	// ========================================
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	/** PC pulls this on OnPossess to push the pawn's IMC into the local player's
	 *  EnhancedInput subsystem — keeps the PC pawn-agnostic across hero/vehicle classes. */
	UFUNCTION(BlueprintPure, Category = "AZ|Input")
	UInputMappingContext* GetDefaultMappingContext() const { return DefaultMappingContext; }

	// ========================================
	// IAbilitySystemInterface (Step 1 stub — returns nullptr; PlayerState in Step 4)
	// ========================================
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ========================================
	// IMoverInputProducerInterface (Step 1 stub — emits zero input; Step 2 wires real input)
	// ========================================
	virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;

	// ========================================
	// IGameplayTagAssetInterface — routes queries through the ASC.
	// Anything that needs to ask "is this pawn aiming / sprinting / slowed?" goes here.
	// ========================================
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;

	// ========================================
	// IGenericTeamAgentInterface — AI perception / faction.
	// Default team is configurable per pawn class; AI subclass sets enemy team at spawn.
	// ========================================
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamId) override;
	virtual FGenericTeamId GetGenericTeamId() const override;

	// ========================================
	// Component accessors
	// ========================================
	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	UAZ_PawnMoverComponent* GetMoverComponent() const { return MoverComponent; }

	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	USkeletalMeshComponent* GetMesh() const { return Mesh; }

	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	UCameraComponent* GetCamera() const { return Camera; }

	UFUNCTION(BlueprintPure, Category = "AZ|Pawn")
	UMoverTrajectoryPredictor* GetTrajectoryPredictor() const { return TrajectoryPredictor; }

	// ========================================
	// Components
	// ========================================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Pawn")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Pawn")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Pawn")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Pawn")
	TObjectPtr<UCameraComponent> Camera;

	// ========================================
	// Camera — per rotation-mode framing (Explore / Strafe / Aiming)
	// ========================================
	// The boom (TargetArmLength + SocketOffset) and FOV are interpolated each Tick toward the config for the
	// CURRENT rotation mode (resolved from GAS tags), so switching mode glides the framing instead of snapping.
	// "Mode" mirrors ProduceInput's RotationMode pick: Aiming > Strafe > Explore. Set per-mode SocketOffset (etc.)
	// in the details panel; leave fields equal across modes to make that field not change between them.

	/** OrientToMovement (explore) — the default framing. Defaults match the boom set up in the constructor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Camera|Modes")
	FAZ_CameraStanceConfig CameraExplore;

	/** Strafe (combat-ready) — e.g. shifted over-the-shoulder for an aim-able framing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Camera|Modes")
	FAZ_CameraStanceConfig CameraStrafe;

	/** Aiming (ADS) — close + narrow. Used once the Aiming rotation mode is wired (ProduceInput sets it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Camera|Modes")
	FAZ_CameraStanceConfig CameraAiming;

protected:
	// ========================================
	// Input (Enhanced Input)
	// ========================================

	/** IMC pushed onto the local player's EnhancedInput subsystem when the PC possesses
	 *  this pawn. Pawn-owned so each pawn class declares its own input surface. */
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

	// Cached input state — written on the game thread (Enhanced Input handlers for
	// movement, IAZ_JumpRequester for jump), read by ProduceInput_Implementation to
	// fill the deterministic Mover InputCmd.
	FVector CachedMoveInputIntent = FVector::ZeroVector;
	bool bIsJumpPressed = false;
	bool bIsJumpJustPressed = false;

	// Strafe: at idle the body HOLDS its facing (no camera follow); a move aligns it to the camera. This latch
	// keeps the align going until the body reaches the camera even if the move was a brief tap, so one tap aligns
	// fully instead of freezing part-way. While set (or move held), face the align target; else (idle) hold current facing.
	bool bStrafeAligning = false;
	// The yaw the post-release align finishes to: the camera yaw CAPTURED at the moment input was released (kept
	// current while a move is held). The latch drives to THIS frozen heading, not the live camera, so turning the
	// camera after you stop does not drag the idle body around (that was the "idle still adjusts to camera" bug).
	float StrafeAlignTargetYaw = 0.f;

	/** Interp the camera boom (arm length + socket offset) and FOV toward the current rotation-mode config.
	 *  Local-viewer only; called every Tick. */
	void UpdateCameraForMode(float DeltaTime);

	void OnMoveTriggered(const FInputActionValue& Value);
	void OnMoveCompleted(const FInputActionValue& Value);
	void OnLookTriggered(const FInputActionValue& Value);

public:
	// ========================================
	// IAZ_JumpRequester — flag-flipper called by UAZ_GA_PawnJump (GAS gates,
	// Mover executes). Mover reads bIsJumpPressed from the cached flag during its
	// next sim tick; the deterministic InputCmd carries it through NetworkPrediction.
	// ========================================
	virtual void SetJumpPressed(bool bPressed) override;

protected:

	// ========================================
	// GAS
	// ========================================

	/** Abilities granted to the player ASC on first possession. Granting happens on
	 *  the server in PossessedBy; the player ASC lives on PlayerState so abilities
	 *  persist across pawn-switch (vehicle entry / respawn). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|GAS")
	TArray<TSubclassOf<UAZ_GameplayAbility>> StartupAbilities;

	/** Init the ASC's ActorInfo with (PlayerState owner, this pawn avatar) on possess.
	 *  On server also grants StartupAbilities (idempotent — re-possession is a no-op). */
	void InitAbilitySystem();

	// ========================================
	// Mover
	// ========================================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Mover", Transient)
	TObjectPtr<UAZ_PawnMoverComponent> MoverComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|Mover", Transient)
	TObjectPtr<UNetworkPredictionComponent> NetworkPredictionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AZ|MotionMatching")
	TObjectPtr<UMoverTrajectoryPredictor> TrajectoryPredictor;

	/** Default team id for this pawn class. Player pawns default to 0; AI subclasses
	 *  set their team in the constructor or at spawn. Read by AI perception. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|AI")
	uint8 DefaultTeamId = 0;

	/** Live team id, overridable at runtime via SetGenericTeamId (e.g. faction switch). */
	FGenericTeamId TeamId = FGenericTeamId::NoTeam;
};
