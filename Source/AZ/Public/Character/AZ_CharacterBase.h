#pragma once

#include <CoreMinimal.h>
#include <GameFramework/Character.h>

#include "AbilitySystemInterface.h"
#include "AZ_GameplayTags.h"
#include "AbilitySystem/Abilities/AZ_GameplayAbility.h"
#include "Interaction/AZ_CombatInterface.h"
#include "AZ_CharacterBase.generated.h"

class AAZ_Weapon;
class UAZ_AbilitySystemComponent;

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	WT_None         UMETA(DisplayName = "None"),
	WT_Pistol       UMETA(DisplayName = "Pistol"),
	WT_Rifle        UMETA(DisplayName = "Rifle"),
	WT_RocketLauncher UMETA(DisplayName = "Rocket Launcher"),
	WT_Shotgun      UMETA(DisplayName = "Shotgun"),
	WT_SMG          UMETA(DisplayName = "SMG"),
	WT_Sniper       UMETA(DisplayName = "Sniper"),
	WT_Melee        UMETA(DisplayName = "Melee"),
	WT_Max          UMETA(Hidden)
};

UENUM(BlueprintType)
enum ECharacterState : uint8 
{
	CS_Idle,
	CS_Walking,
	CS_Running,
	CS_Jumping,
	CS_Falling,
	CS_Dead,
	CS_Dying,
	CS_Attacking,
	CS_Reloading,
	CS_Sprinting,
	CS_Crouching,
	CS_Swimming,
	CS_Climbing,
	CS_Max  
};

UCLASS(Abstract)
class AZ_API AAZ_CharacterBase : public ACharacter, public IAZ_CombatInterface, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AAZ_CharacterBase();

	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AZ|Combat")
	TObjectPtr<USkeletalMeshComponent> Weapon;

	UPROPERTY(EditAnywhere, Category = "AZ|Sockets")
	FName WeaponTipSocketName{TEXT("WeaponTipSocket")};

	UPROPERTY(EditAnywhere, Category = "AZ|Sockets")
	FName LeftHandSocketName{TEXT("LeftHandSocket")};

	UPROPERTY(EditAnywhere, Category = "AZ|Sockets")
	FName RightHandSocketName{TEXT("WeaponHandSocket")};

	UPROPERTY(EditAnywhere, Category = "AZ|Sockets")
	FName TailSocketName{TEXT("TailSocket")};

	UFUNCTION(BlueprintPure, Category = "AZ|Abilities")
	UAbilitySystemComponent* GetASC() const;

	/** Combat Interface */
	virtual FOnASCRegistered& GetOnASCRegisteredDelegate() override;

	/** end Combat Interface */
	
	FOnASCRegistered OnAscRegistered;

	// Implement IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** Returns the map that links equipment slot tags to mesh socket names. */
	const TMap<FGameplayTag, FName>& GetEquipmentSocketMap() const;

	
protected:
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UAttributeSet* GetAttributeSet() const;
	
	UPROPERTY()
	TObjectPtr<UAttributeSet> AttributeSet;

	// This property will hold the class of AttributeSet to create for this character.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Attributes")
	TSubclassOf<UAttributeSet> DefaultAttributeSetClass;

	virtual void InitDefaultAttributes();
	
	void ApplyEffectToSelf(const TSubclassOf<UGameplayEffect>& GameplayEffectClass, float Level) const;
	void InitDefaultAbilities() const;

	void InitInputAbilities(const TArray<TSubclassOf<UAZ_GameplayAbility>>& InInputAbilities) const;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Attributes")
	TSubclassOf<UGameplayEffect> DefaultPrimaryAttributes;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Attributes")
	TSubclassOf<UGameplayEffect> DefaultSecondaryAttributes;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Attributes")
	TSubclassOf<UGameplayEffect> DefaultVitalAttributes;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "AZ|Attributes")
	TSubclassOf<UGameplayEffect> DefaultTagsToActivate;
	

	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	UPROPERTY(EditAnywhere, Category = "AZ|Abilities|Startup")
	TArray<TSubclassOf<UAZ_GameplayAbility>> StartupAbilities;

	UPROPERTY(EditAnywhere, Category = "AZ|Abilities|Passive")
	TArray<TSubclassOf<UAZ_GameplayAbility>> StartupPassiveAbilities;

	UPROPERTY(EditAnywhere, Category = "AZ|Abilities|Input")
	TArray<TSubclassOf<UAZ_GameplayAbility>> CharacterInputAbilities;

	/** A map that links an Equipment Slot Tag to a specific FName socket on this character's mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AZ|Equipment")
	TMap<FGameplayTag, FName> EquipmentSocketMap;

	FGameplayTag CurrentMovementStateTag{FAZ_GameplayTags::Get().Movement_Idle};
	FGameplayTag CurrentWeaponStateTag{FAZ_GameplayTags::Get().Weapon_None};

	FGameplayTag DetermineMovementStateFromSpeed(float Speed) const;

	EWeaponType ActiveWeaponType;

	virtual void SetWeaponAndTag(EWeaponType WeaponType);

	void SwitchToWeaponInSlot(int32 SlotIndex);
};