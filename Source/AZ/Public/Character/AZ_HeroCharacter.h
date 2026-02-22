#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/AttributeSets/AZ_HeroAttributeSet.h"
#include "AZ/Public/Character/AZ_CharacterBase.h"
#include "AZ/Public/Inventory/AZ_InventoryComponent.h"
#include "Inventory/AZ_EquipmentManagerComponent.h"
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
	// CAMERA PROPERTIES
	// ========================================

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera")
	float Default1PFOV;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera")
	float Default3PFOV;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "AZ|Camera")
	class USpringArmComponent* ThirdPersonCameraBoom;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "AZ|Camera")
	class UCameraComponent* ThirdPersonCamera;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "AZ|Camera")
	class UCameraComponent* FirstPersonCamera;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera")
	float CameraBoomLocationX = 0.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera")
	float CameraBoomLocationY = 50.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Camera")
	float CameraBoomLocationZ = 68.492264f;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Camera")
	bool bIsFirstPersonPerspective{false};

	// ========================================
	// CHARACTER PROPERTIES
	// ========================================

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Character|Movement")
	float RunSpeedMultiplayer{2.0f};

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Character")
	FName WeaponAttachPoint;

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
	UFUNCTION(BlueprintCallable, Category = "Echo|HeroCharacter")
	USkeletalMeshComponent* GetThirdPersonMesh() const;

	FName GetWeaponAttachPoint() { return WeaponAttachPoint; }

	// Character State
	bool IsHeroAlive() const;
	bool IsInFirstPersonPerspective() const { return bIsFirstPersonPerspective; }

	// Weapon Management
	UFUNCTION(BlueprintCallable, Category = "Echo|Inventory")
	AAZ_Weapon* GetCurrentWeapon() const { return CurrentWeapon; }

	bool DoesWeaponExistInInventory(AAZ_Weapon* InWeapon);

	UFUNCTION(BlueprintCallable, Category = "GASShooter|Inventory")
	void EquipWeapon(AAZ_Weapon* NewWeapon);

	UFUNCTION(BlueprintCallable, Category = "AZ|Inventory")
	bool AddWeaponToInventory(AAZ_Weapon* NewWeapon);

	UFUNCTION(BlueprintCallable, Category = "AZ|Inventory")
	bool EquipWeaponFromSlot(int InSlot);

	UFUNCTION()
	UAZ_EquipmentManagerComponent* GetEquipmentManagerComponent() const { return EquipmentManagerComponent; }

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
	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_EquippedWeapon, Category = "Combat")
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
