#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemInterface.h"
#include "MoverSimulationTypes.h"
#include "AZ_GameplayTags.h"
#include "Interaction/AZ_CombatInterface.h"
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
class AZ_API AAZ_HeroPawn : public APawn, public IAbilitySystemInterface, public IAZ_CombatInterface, public IMoverInputProducerInterface
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Input")
	float LookRateYaw = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Input")
	float LookRatePitch = 1.f;

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
};
