#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/AttributeSets/AZ_HeroAttributeSet.h"
#include "AZ/Public/Character/AZ_CharacterBase.h"
#include "AZ/Public/Inventory/AZ_InventoryComponent.h"
#include "Equipment/AZ_EquipmentManagerComponent.h"
#include "AZ_HeroCharacter.generated.h"

// Forward declarations
class UAZ_AbilitySystemComponent;
class AAZ_Weapon;
class AAZ_PlayerState;
class AAZ_PlayerController;
class USpringArmComponent;
class UCameraComponent;

USTRUCT()
struct AZ_API FEchoHeroInventory
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<AAZ_Weapon*> Weapons;

	// TODO: Add consumable items, passive items (armor), door keys, etc.
};

USTRUCT()
struct AZ_API FHUDInitializationData
{
	GENERATED_BODY()

	const AAZ_PlayerController* PlayerController = nullptr;
	const AAZ_PlayerState* PlayerState = nullptr;
	const UAbilitySystemComponent* AbilitySystemComponent = nullptr;
	const UAttributeSet* AttributeSet = nullptr;
};

UCLASS(config=Game, BlueprintType)
class AZ_API AAZ_HeroCharacter : public AAZ_CharacterBase
{
	GENERATED_BODY()

public:
	AAZ_HeroCharacter();

	// ========================================
	// CAMERA — COMPONENTS
	// ========================================

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "AZ|Camera")
	class USpringArmComponent* ThirdPersonCameraBoom;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "AZ|Camera")
	class UCameraComponent* ThirdPersonCamera;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "AZ|Camera")
	class UCameraComponent* FirstPersonCamera;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Camera")
	bool bIsFirstPersonPerspective{false};

	// ========================================
	// CAMERA — BOOM / POSITION
	// ========================================

	/** Distance from character to camera. RE-style ≈ 200-300. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Boom")
	float CameraBoomArmLength = 250.f;

	/** Right offset for over-the-shoulder view. Positive = right shoulder. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Boom")
	float CameraBoomSocketOffsetY = 55.f;

	/** Vertical offset. Roughly head height above root. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Boom")
	float CameraBoomSocketOffsetZ = 60.f;

	// ========================================
	// CAMERA — LAG / SMOOTHING
	// ========================================

	/** Translation lag speed. Lower = more floaty/cinematic. 8-12 = responsive, 4-6 = heavy. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Lag")
	float CameraLagSpeed = 10.f;

	/** Rotation lag speed. Lower = more delay on turning. 8-12 = snappy, 4-6 = smooth. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Lag")
	float CameraRotationLagSpeed = 12.f;

	// ========================================
	// CAMERA — FOV
	// ========================================

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|FOV")
	float Default1PFOV;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|FOV")
	float Default3PFOV;

	// ========================================
	// CAMERA — AIM (ADS)
	// ========================================

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Aim")
	float AimFOV = 55.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Aim")
	float AimBoomLength = 120.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Aim")
	float AimSocketOffsetY = 70.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Aim")
	float AimSocketOffsetZ = 45.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Aim")
	float CameraAimInterpSpeed = 10.f;

	// ========================================
	// CAMERA — CROUCH
	// ========================================

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Crouch")
	float CrouchBoomLength = 200.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Crouch")
	float CrouchSocketOffsetY = 55.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Crouch")
	float CrouchSocketOffsetZ = 30.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Crouch")
	float CameraCrouchInterpSpeed = 10.f;

	// ========================================
	// CAMERA — MOVEMENT OFFSET
	// ========================================

	// ========================================
	// CAMERA — MOVEMENT OFFSET (DEFAULT / STAND)
	// ========================================

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Move Offset|Default")
	FVector2D CameraOffsetForward{0.f, 5.f};

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Move Offset|Default")
	FVector2D CameraOffsetBackward{0.f, 0.f};

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Move Offset|Default")
	FVector2D CameraOffsetLeft{-15.f, 0.f};

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Move Offset|Default")
	FVector2D CameraOffsetRight{15.f, 0.f};

	// ========================================
	// CAMERA — MOVEMENT OFFSET (AIM)
	// ========================================

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Move Offset|Aim")
	FVector2D CameraOffsetAimForward{0.f, 3.f};

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Move Offset|Aim")
	FVector2D CameraOffsetAimBackward{0.f, 0.f};

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Move Offset|Aim")
	FVector2D CameraOffsetAimLeft{-10.f, 0.f};

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Move Offset|Aim")
	FVector2D CameraOffsetAimRight{10.f, 0.f};

	// ========================================
	// CAMERA — MOVEMENT OFFSET (CROUCH)
	// ========================================

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Move Offset|Crouch")
	FVector2D CameraOffsetCrouchForward{0.f, 3.f};

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Move Offset|Crouch")
	FVector2D CameraOffsetCrouchBackward{0.f, 0.f};

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Move Offset|Crouch")
	FVector2D CameraOffsetCrouchLeft{-12.f, 0.f};

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Move Offset|Crouch")
	FVector2D CameraOffsetCrouchRight{12.f, 0.f};

	/** Speed of movement offset interpolation. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera|Move Offset")
	float CameraMoveOffsetInterpSpeed = 8.f;

	// ========================================
	// MOVEMENT — SPEEDS
	// ========================================

	/** Brisk walk speed in cm/s (~7 km/h). Sprint is handled via GAS ability. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Movement|Speed")
	float WalkSpeed = 195.f;

	/** Crouched movement speed in cm/s (~3 km/h). */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Movement|Speed")
	float CrouchSpeed = 90.f;

	/** Sprint speed multiplier applied on top of WalkSpeed via GAS ability. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Movement|Speed")
	float SprintSpeedMultiplier = 2.15f;

	// ========================================
	// MOVEMENT — ACCELERATION / DECELERATION
	// ========================================

	/** How fast the character reaches walk speed. 450 ≈ 0.43s from standstill. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Movement|Accel")
	float Acceleration = 450.f;

	/** Deceleration force when releasing input. Lower = longer slide. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Movement|Accel")
	float BrakingDeceleration = 150.f;

	/** Ground friction during normal movement. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Movement|Accel")
	float MovementGroundFriction = 5.f;

	/** Friction multiplier applied during braking (0-2). */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Movement|Accel")
	float MovementBrakingFrictionFactor = 0.8f;

	// ========================================
	// MOVEMENT — JUMP / AIR
	// ========================================

	/** Initial upward velocity for a standing jump. 420 ≈ ~45cm peak at GravityScale 1.5. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Movement|Jump")
	float JumpVelocity = 420.f;

	/** Gravity multiplier. >1 = heavier, grounded feel. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Movement|Jump")
	float JumpGravityScale = 1.5f;

	/** Mid-air directional control (0 = none, 1 = full). Realistic ≈ 0.08. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Movement|Jump")
	float JumpAirControl = 0.08f;

	/** Air deceleration when no input. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Movement|Jump")
	float AirBrakingDeceleration = 100.f;

	/** Lateral air friction (subtle air resistance). */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Movement|Jump")
	float AirLateralFriction = 0.2f;

	// ========================================
	// MOVEMENT — TERRAIN
	// ========================================

	/** Max height the character can step up (cm). Stair step ≈ 20-30cm. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Movement|Terrain")
	float StepHeight = 30.f;

	/** Max walkable slope angle in degrees. Comfortable human limit ≈ 35-40°. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Movement|Terrain")
	float WalkableAngle = 38.f;

	// ========================================
	// CHARACTER PROPERTIES
	// ========================================

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	USkeletalMeshComponent* FirstPersonMesh;

	// ========================================
	// PUBLIC METHODS
	// ========================================

	// Initialization & Progress
	void LoadProgress();

	// Attribute System
	UAZ_HeroAttributeSet* GetHeroAttributeSet() const;

	// Mesh & Components
	UFUNCTION(BlueprintCallable, Category = "AZ|Echo|HeroCharacter")
	USkeletalMeshComponent* GetThirdPersonMesh() const;



	// Character State
	bool IsHeroAlive() const;
	bool IsInFirstPersonPerspective() const { return bIsFirstPersonPerspective; }

	// Weapon Management
	UFUNCTION(BlueprintCallable, Category = "AZ|Echo|Inventory")
	AAZ_Weapon* GetCurrentWeapon() const { return CurrentWeapon; }

	bool DoesWeaponExistInInventory(AAZ_Weapon* InWeapon);

	UFUNCTION(BlueprintCallable, Category = "AZ|GASShooter|Inventory")
	void EquipWeapon(AAZ_Weapon* NewWeapon);

	UFUNCTION(BlueprintCallable, Category = "AZ|Inventory")
	bool AddWeaponToInventory(AAZ_Weapon* NewWeapon);

	UFUNCTION(BlueprintCallable, Category = "AZ|Inventory")
	bool EquipWeaponFromSlot(int InSlot);

	UFUNCTION()
	UAZ_EquipmentManagerComponent* GetEquipmentManagerComponent() const { return EquipmentManagerComponent; }

	UFUNCTION(BlueprintPure, Category = "AZ|Weapon")
	AAZ_Weapon* GetEquippedWeapon() const { return EquippedWeapon; }

	UFUNCTION()
	void AttachItem(AAZ_Item* Item);

	UFUNCTION(Server, Reliable)
	void ServerAttachItem(AAZ_Item* Item);

protected:

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "AZ|Camera")
	class UAZ_EquipmentManagerComponent* EquipmentManagerComponent;
	
	// ========================================
	// PROTECTED PROPERTIES
	// ========================================

	// Ability System Component
	UPROPERTY()
	TObjectPtr<UAZ_AbilitySystemComponent> AZ_AbilitySystemComponent;

	// Gameplay Tags (Cached for performance)
	FGameplayTag NoWeaponTag;
	FGameplayTag WeaponChangingDelayReplicationTag;
	FGameplayTag WeaponAmmoTypeNoneTag;
	FGameplayTag WeaponAbilityTag;
	FGameplayTag KnockedDownTag;
	FGameplayTag InteractingTag;
	FGameplayTag CurrentWeaponTag;

	// This pointer holds the weapon that is currently spawned and in the player's hands.
	// We add "ReplicatedUsing" to tell the engine to call a function when this changes on the client.
	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_EquippedWeapon, Category = "AZ|Combat")
	TObjectPtr<AAZ_Weapon> EquippedWeapon;

	// This is the function that will be called on clients.
	UFUNCTION()
	void OnRep_EquippedWeapon();

	// You might also have a pointer for the previously equipped weapon to handle detachment
	UPROPERTY()
	TObjectPtr<AAZ_Weapon> PreviousWeapon;

	// Weapon Management
	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon)
	AAZ_Weapon* CurrentWeapon;

	bool bChangedWeaponLocally;

	// Inventory (Legacy - TODO: Refactor to use proper inventory component)
	UPROPERTY(ReplicatedUsing = OnRep_Inventory)
	FEchoHeroInventory OldImplInventory;

	// ========================================
	// PROTECTED METHODS
	// ========================================

	// Network Replication
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Controller & PlayerState Events
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	void LinkInventoryAndEquipment() const;

	// Weapon Management (Internal)
	void SetCurrentWeapon(AAZ_Weapon* NewWeapon, AAZ_Weapon* LastWeapon);

	// Network RPCs for Weapon Sync
	UFUNCTION(Server, Reliable)
	void ServerEquipWeapon(AAZ_Weapon* NewWeapon);
	void ServerEquipWeapon_Implementation(AAZ_Weapon* NewWeapon);
	bool ServerEquipWeapon_Validate(AAZ_Weapon* NewWeapon);

	UFUNCTION(Server, Reliable)
	void ServerSyncCurrentWeapon();
	void ServerSyncCurrentWeapon_Implementation();
	bool ServerSyncCurrentWeapon_Validate();

	UFUNCTION(Client, Reliable)
	void ClientSyncCurrentWeapon(AAZ_Weapon* InWeapon);
	void ClientSyncCurrentWeapon_Implementation(AAZ_Weapon* InWeapon);
	bool ClientSyncCurrentWeapon_Validate(AAZ_Weapon* InWeapon);

	// Replication Callbacks
	UFUNCTION()
	void OnRep_CurrentWeapon(AAZ_Weapon* LastWeapon);

	UFUNCTION()
	void OnRep_Inventory();

	// Attribute System
	void ApplyEffectToDefaultAttributes() const;

	

private:
	// ========================================
	// PRIVATE METHODS
	// ========================================

	void CheckAndPrintAttributes() const;
	void InitializeHUD();
};
