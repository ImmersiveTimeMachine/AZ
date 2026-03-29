// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/AttributeSets/AZ_WeaponAttributeSet.h"
#include "GameFramework/Actor.h"
#include "Items/AZ_Item.h"
#include "Weapon/AZ_WeaponTypes.h"
#include "AZ_Weapon.generated.h"

class UAZ_InventoryComponent;
class UInputMappingContext;
class UPaperSprite;
class UAZ_GameplayAbility;
class AAZ_GATA_SphereTrace;
class AAZ_GATA_LineTrace;
enum class EAZ_AbilityInputID : uint8;
class AAZ_HeroCharacter;
class UAZ_AbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWeaponAmmoChangedDelegate, int32, OldValue, int32, NewValue);

UCLASS()
class AZ_API AAZ_Weapon : public AAZ_Item, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AAZ_Weapon();

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Whether or not to spawn this weapon with collision enabled (pickup mode).
	// Set to false when spawning directly into a player's inventory or true when spawning into the world in pickup mode.
	UPROPERTY(BlueprintReadWrite)
	bool bSpawnWithCollision;

	// This tag is primarily used by the first person Animation Blueprint to determine which animations to play
	// (Rifle vs Rocket Launcher)
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "AZ|Weapon")
	FGameplayTag WeaponTag;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "AZ|Weapon")
	FGameplayTagContainer RestrictedPickupTags;
	
	// UI HUD Primary Icon when equipped. Using Sprites because of the texture atlas from ShooterGame.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|UI")
	UPaperSprite* PrimaryIcon;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|UI")
	UPaperSprite* SecondaryIcon;

	// UI HUD Primary Clip Icon when equipped
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|UI")
	UPaperSprite* PrimaryClipIcon;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|UI")
	UPaperSprite* SecondaryClipIcon;

	UPROPERTY(BlueprintReadWrite, VisibleInstanceOnly, Category = "AZ|Weapon")
	FGameplayTag FireMode;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "AZ|Weapon")
	FGameplayTag PrimaryAmmoType;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "AZ|Weapon")
	FGameplayTag SecondaryAmmoType; 

	// Things like fire mode for rifle
	UPROPERTY(BlueprintReadWrite, VisibleInstanceOnly, Category = "AZ|Weapon")
	FText StatusText;

	UPROPERTY(BlueprintAssignable, Category = "AZ|Weapon")
	FWeaponAmmoChangedDelegate OnPrimaryClipAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category = "AZ|Weapon")
	FWeaponAmmoChangedDelegate OnMaxPrimaryClipAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category = "AZ|Weapon")
	FWeaponAmmoChangedDelegate OnSecondaryClipAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category = "AZ|Weapon")
	FWeaponAmmoChangedDelegate OnMaxSecondaryClipAmmoChanged;

	UPROPERTY(EditAnywhere, Category = "AZ|Weapon|Sockets")
	FName CarrySocketName{ TEXT("BackRifleSocket") };

	UPROPERTY(EditAnywhere, Category = "AZ|Weapon|Sockets")
	FName RelaxedSocketName{ TEXT("RightHandRifleSocketRelaxed") };

	UPROPERTY(EditAnywhere, Category = "AZ|Weapon|Sockets")
	FName AimSocketName{ TEXT("RightHandRifleSocketAim") };

	/** Socket on weapon mesh for left hand grip. */
	UPROPERTY(EditAnywhere, Category = "AZ|Weapon|Sockets")
	FName LeftHandGripSocket{ TEXT("LeftHandGrip") };

	/**
	 * Per-pose IK adjustments. Pre-populated with all pose states.
	 * Tweak offsets per weapon to adjust left hand grip for each animation state.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AZ|Weapon|IK")
	TMap<EAZ_WeaponPoseState, FAZ_LeftHandIKAdjustment> LeftHandIKAdjustments;

	/**
	 * Get the left hand IK target in bone space relative to a given bone.
	 * Reads the grip socket from the weapon mesh, converts to bone space via TransformToBoneSpace,
	 * then applies the adjustment for the given pose state.
	 * @param CharMesh       The character's skeletal mesh to transform into bone space.
	 * @param BoneName       The bone to compute relative to (typically hand_r).
	 * @param PoseState      The current weapon pose state resolved by the AnimInstance.
	 * @param OutTransform   The resulting bone-space transform for the IK effector.
	 * @return true if a valid socket was found and transform was computed.
	 */
	UFUNCTION(BlueprintCallable, Category = "AZ|Weapon")
	bool GetLeftHandSocket(USkeletalMeshComponent* CharMesh, FName BoneName, EAZ_WeaponPoseState PoseState, FTransform& OutTransform) const;

	// Implement the interface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    
	// Getter for the attribute set
	UAZ_WeaponAttributeSet* GetAttributeSet() const { return AttributeSet; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AZ|Weapon")
	virtual USkeletalMeshComponent* GetWeaponMesh1P() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AZ|Weapon")
	virtual USkeletalMeshComponent* GetWeaponMesh3P() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;

	void SetOwningCharacter(AAZ_HeroCharacter* InOwningCharacter);

	// Pickup on touch
	virtual void NotifyActorBeginOverlap(class AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(class AActor* OtherActor) override;

	/** Sets this weapon up as a cosmetic-only prop for prediction. */
	UFUNCTION()
	void MakeCosmetic();

	/*UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& OverlapInfo);

	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
	*/


	// Called when the player equips this weapon
	virtual void Equip();

	// Called when the player unequips this weapon
	virtual void UnEquip();

	virtual void AddAbilities();

	virtual void RemoveAbilities();

	virtual int32 GetAbilityLevel(EAZ_AbilityInputID AbilityID) { return 1; };

	// Resets things like fire mode to default
	UFUNCTION(BlueprintCallable, Category = "AZ|Weapon")
	virtual void ResetWeapon();

	UFUNCTION(NetMulticast, Reliable)
	void OnDropped(FVector NewLocation);
	virtual void OnDropped_Implementation(FVector NewLocation);
	virtual bool OnDropped_Validate(FVector NewLocation);

	UFUNCTION(BlueprintCallable, Category = "AZ|Weapon")
	virtual int32 GetPrimaryClipAmmo() const;

	UFUNCTION(BlueprintCallable, Category = "AZ|Weapon")
	virtual int32 GetMaxPrimaryClipAmmo() const;

	UFUNCTION(BlueprintCallable, Category = "AZ|Weapon")
	virtual int32 GetSecondaryClipAmmo() const;

	UFUNCTION(BlueprintCallable, Category = "AZ|Weapon")
	virtual int32 GetMaxSecondaryClipAmmo() const;

	UFUNCTION(BlueprintCallable, Category = "AZ|Weapon")
	virtual void SetPrimaryClipAmmo(int32 NewPrimaryClipAmmo);

	UFUNCTION(BlueprintCallable, Category = "AZ|Weapon")
	virtual void SetMaxPrimaryClipAmmo(int32 NewMaxPrimaryClipAmmo);

	UFUNCTION(BlueprintCallable, Category = "AZ|Weapon")
	virtual void SetSecondaryClipAmmo(int32 NewSecondaryClipAmmo);

	UFUNCTION(BlueprintCallable, Category = "AZ|Weapon")
	virtual void SetMaxSecondaryClipAmmo(int32 NewMaxSecondaryClipAmmo);

	/*UFUNCTION(BlueprintCallable, Category = "AZ|Weapon")
	TSubclassOf<class UEchoHUDReticle> GetPrimaryHUDReticleClass() const;*/

	UFUNCTION(BlueprintCallable, Category = "AZ|Weapon")
	virtual bool HasInfiniteAmmo() const;

	UFUNCTION(BlueprintCallable, Category = "AZ|Animation")
	UAnimMontage* GetEquip1PMontage() const;

	UFUNCTION(BlueprintCallable, Category = "AZ|Animation")
	UAnimMontage* GetEquip3PMontage() const;
	
	UFUNCTION(BlueprintCallable, Category = "AZ|Audio")
	class USoundCue* GetPickupSound() const;

	UFUNCTION(BlueprintCallable, Category = "AZ|Weapon")
	FText GetDefaultStatusText() const;

	// Getter for LineTraceTargetActor. Spawns it if it doesn't exist yet.
	UFUNCTION(BlueprintCallable, Category = "AZ|Targeting")
	AAZ_GATA_LineTrace* GetLineTraceTargetActor();

	// Getter for SphereTraceTargetActor. Spawns it if it doesn't exist yet.
	UFUNCTION(BlueprintCallable, Category = "AZ|Targeting")
	AAZ_GATA_SphereTrace* GetSphereTraceTargetActor();

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Input")
	UInputMappingContext* FireMappingContext;

protected:
	/*UFUNCTION()
	void PickUpWeapon(AEchoHero* PickUpCharacter);*/
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Use a descriptive variable name, not the class name.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Abilities")
	TObjectPtr<UAZ_AbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AZ|Abilities")
	TObjectPtr<UAZ_WeaponAttributeSet> AttributeSet;

	// How much ammo in the clip the gun starts with
	UPROPERTY(BlueprintReadOnly, EditAnywhere, ReplicatedUsing = OnRep_PrimaryClipAmmo, Category = "AZ|Weapon|Ammo")
	int32 PrimaryClipAmmo;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, ReplicatedUsing = OnRep_MaxPrimaryClipAmmo, Category = "AZ|Weapon|Ammo")
	int32 MaxPrimaryClipAmmo;

	// How much ammo in the clip the gun starts with. Used for things like rifle grenades.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, ReplicatedUsing = OnRep_SecondaryClipAmmo, Category = "AZ|Weapon|Ammo")
	int32 SecondaryClipAmmo;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, ReplicatedUsing = OnRep_MaxSecondaryClipAmmo, Category = "AZ|Weapon|Ammo")
	int32 MaxSecondaryClipAmmo;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Weapon|Ammo")
	bool bInfiniteAmmo;

	/*UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|UI")
	TSubclassOf<class UEchoHUDReticle> PrimaryHUDReticleClass;*/

	UPROPERTY()
	AAZ_GATA_LineTrace* LineTraceTargetActor;

	UPROPERTY()
	AAZ_GATA_SphereTrace* SphereTraceTargetActor;

	// Collision box for when weapon is in pickup mode
	UPROPERTY(VisibleAnywhere, Category = "AZ|Weapon")
	class UBoxComponent* CollisionComp;

	UPROPERTY(VisibleAnywhere, Category = "AZ|Weapon")
	USkeletalMeshComponent* WeaponMesh1P;

	UPROPERTY(VisibleAnywhere, Category = "AZ|Weapon")
	USkeletalMeshComponent* WeaponMesh3P;
	
	// Relative Location of weapon 3P Mesh when in pickup mode
	// 1P weapon mesh is invisible so it doesn't need one
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Weapon")
	FVector WeaponMesh3PickupRelativeLocation;

	// Relative Location of weapon 1P Mesh when equipped
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Weapon")
	FVector WeaponMesh1PEquippedRelativeLocation;

	// Relative Location of weapon 3P Mesh when equipped
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Weapon")
	FVector WeaponMesh3PEquippedRelativeLocation;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "AZ|Weapon")
	AAZ_HeroCharacter* OwningCharacter;

	UPROPERTY(EditAnywhere, Category = "AZ|Weapon")
	TArray<TSubclassOf<UAZ_GameplayAbility>> Abilities;

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Weapon")
	TArray<FGameplayAbilitySpecHandle> AbilitySpecHandles;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AZ|Weapon")
	FGameplayTag DefaultFireMode;

	// Things like fire mode for rifle
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Weapon")
	FText DefaultStatusText;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Animation")
	UAnimMontage* Equip1PMontage;

	UPROPERTY(BlueprintReadonly, EditAnywhere, Category = "AZ|Animation")
	UAnimMontage* Equip3PMontage;

	// Sound played when player picks it up
	UPROPERTY(EditDefaultsOnly, Category = "AZ|Audio")
	class USoundCue* PickupSound;

	// Cache tags
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "AZ|Weapon")
	FGameplayTag WeaponPrimaryInstantAbilityTag;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "AZ|Weapon")
	FGameplayTag WeaponSecondaryInstantAbilityTag;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "AZ|Weapon")
	FGameplayTag WeaponAlternateInstantAbilityTag;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "AZ|Weapon")
	FGameplayTag WeaponIsFiringTag;
	
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	// Called when the player picks up this weapon
	virtual void PickUpOnTouch(AAZ_HeroCharacter* InCharacter);

	UFUNCTION()
	virtual void OnRep_PrimaryClipAmmo(int32 OldPrimaryClipAmmo);

	UFUNCTION()
	virtual void OnRep_MaxPrimaryClipAmmo(int32 OldMaxPrimaryClipAmmo);

	UFUNCTION()
	virtual void OnRep_SecondaryClipAmmo(int32 OldSecondaryClipAmmo);

	UFUNCTION()
	virtual void OnRep_MaxSecondaryClipAmmo(int32 OldMaxSecondaryClipAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Weapon")
	bool bIsCosmeticOnly = false;
	
};
