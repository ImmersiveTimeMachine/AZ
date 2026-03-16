#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AZ_WeaponAttributeSet.generated.h"

// A macro to create boilerplate getter/setter functions for an attribute.
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Holds attributes related to weapons, primarily ammunition counts and weapon stats.
 * Lives on the player's ASC (via PlayerState) so weapon state persists across equip/unequip cycles.
 * On equip: WeaponStateFragment pushes values here. On unequip: values are saved back to the fragment.
 */
UCLASS()
class AZ_API UAZ_WeaponAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UAZ_WeaponAttributeSet();

	FGameplayAttribute GetMaxReserveAmmoAttributeFromTag(const FGameplayTag& WeaponTag) const;
	FGameplayAttribute GetReserveAmmoAttributeFromTag(const FGameplayTag& WeaponTag) const;

	// --- Clip/Magazine Ammo ---
	// The current amount of ammo loaded in the weapon's magazine or clip.

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Ammo|Pistol", ReplicatedUsing = OnRep_PistolClipAmmo)
	FGameplayAttributeData PistolClipAmmo;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, PistolClipAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Ammo|Rifle", ReplicatedUsing = OnRep_RifleClipAmmo)
	FGameplayAttributeData RifleClipAmmo;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, RifleClipAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Ammo|Shotgun", ReplicatedUsing = OnRep_ShotgunClipAmmo)
	FGameplayAttributeData ShotgunClipAmmo;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, ShotgunClipAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Ammo|SMG", ReplicatedUsing = OnRep_SMGClipAmmo)
	FGameplayAttributeData SMGClipAmmo;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, SMGClipAmmo);

	// --- Reserve Ammo ---
	// The total amount of ammo the player is carrying for this weapon type, but not currently in the clip.

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Ammo|Pistol", ReplicatedUsing = OnRep_PistolReserveAmmo)
	FGameplayAttributeData PistolReserveAmmo;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, PistolReserveAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Ammo|Rifle", ReplicatedUsing = OnRep_RifleReserveAmmo)
	FGameplayAttributeData RifleReserveAmmo;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, RifleReserveAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Ammo|Shotgun", ReplicatedUsing = OnRep_ShotgunReserveAmmo)
	FGameplayAttributeData ShotgunReserveAmmo;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, ShotgunReserveAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Ammo|SMG", ReplicatedUsing = OnRep_SMGReserveAmmo)
	FGameplayAttributeData SMGReserveAmmo;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, SMGReserveAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Ammo|Rocket", ReplicatedUsing = OnRep_RocketReserveAmmo)
	FGameplayAttributeData RocketReserveAmmo;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, RocketReserveAmmo);

	// --- Max Ammo Capacities ---
	// The maximum values for the attributes above. Useful for UI and pickup logic.

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Ammo|Pistol", ReplicatedUsing = OnRep_MaxPistolClipAmmo)
	FGameplayAttributeData MaxPistolClipAmmo;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, MaxPistolClipAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Ammo|Pistol", ReplicatedUsing = OnRep_MaxPistolReserveAmmo)
	FGameplayAttributeData MaxPistolReserveAmmo;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, MaxPistolReserveAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Ammo|Rifle", ReplicatedUsing = OnRep_MaxRifleClipAmmo)
	FGameplayAttributeData MaxRifleClipAmmo;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, MaxRifleClipAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Ammo|Rifle", ReplicatedUsing = OnRep_MaxRifleReserveAmmo)
	FGameplayAttributeData MaxRifleReserveAmmo;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, MaxRifleReserveAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Ammo|Shotgun", ReplicatedUsing = OnRep_MaxShotgunClipAmmo)
	FGameplayAttributeData MaxShotgunClipAmmo;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, MaxShotgunClipAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Ammo|Shotgun", ReplicatedUsing = OnRep_MaxShotgunReserveAmmo)
	FGameplayAttributeData MaxShotgunReserveAmmo;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, MaxShotgunReserveAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Ammo|SMG", ReplicatedUsing = OnRep_MaxSMGClipAmmo)
	FGameplayAttributeData MaxSMGClipAmmo;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, MaxSMGClipAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Ammo|SMG", ReplicatedUsing = OnRep_MaxSMGReserveAmmo)
	FGameplayAttributeData MaxSMGReserveAmmo;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, MaxSMGReserveAmmo);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Ammo|Rocket", ReplicatedUsing = OnRep_MaxRocketReserveAmmo)
	FGameplayAttributeData MaxRocketReserveAmmo;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, MaxRocketReserveAmmo);

	// --- Weapon-Specific Stats ---
	// These could also live here if they are fundamental to the weapon's gameplay.
	// Alternatively, they could be simple float properties on the AAZ_Weapon class itself.
	// Putting them here allows them to be modified by GameplayEffects.

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Weapon Stats", ReplicatedUsing = OnRep_BaseDamage)
	FGameplayAttributeData BaseDamage;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, BaseDamage);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Weapon Stats", ReplicatedUsing = OnRep_FireRate)
	FGameplayAttributeData FireRate; // e.g., Shots per second
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, FireRate);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Weapon Stats", ReplicatedUsing = OnRep_ReloadTime)
	FGameplayAttributeData ReloadTime; // In seconds
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, ReloadTime);


	// --- Melee Weapon Stats (for Weapon_Melee) ---
	UPROPERTY(BlueprintReadOnly, Category = "AZ|Melee Stats", ReplicatedUsing = OnRep_StaminaCost)
	FGameplayAttributeData StaminaCost; // Stamina consumed per swing
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, StaminaCost);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Melee Stats", ReplicatedUsing = OnRep_Durability)
	FGameplayAttributeData Durability; // Current durability of the weapon
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, Durability);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|Melee Stats", ReplicatedUsing = OnRep_MaxDurability)
	FGameplayAttributeData MaxDurability; // Maximum durability
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, MaxDurability);

	// --- Optional Attributes ---

	UPROPERTY(BlueprintReadOnly, Category = "AZ|AmmoAttribute", ReplicatedUsing = OnRep_BurnChance)
	FGameplayAttributeData BurnChance;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, BurnChance);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|AmmoAttribute", ReplicatedUsing = OnRep_BurnDuration)
	FGameplayAttributeData BurnDuration;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, BurnDuration);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|AmmoAttribute", ReplicatedUsing = OnRep_BurnDamage)
	FGameplayAttributeData BurnDamage;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, BurnDamage);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|AmmoAttribute", ReplicatedUsing = OnRep_FreezeChance)
	FGameplayAttributeData FreezeChance;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, FreezeChance);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|AmmoAttribute", ReplicatedUsing = OnRep_FreezeDuration)
	FGameplayAttributeData FreezeDuration;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, FreezeDuration);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|AmmoAttribute", ReplicatedUsing = OnRep_ShockChance)
	FGameplayAttributeData ShockChance;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, ShockChance);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|AmmoAttribute", ReplicatedUsing = OnRep_ShockDamage)
	FGameplayAttributeData ShockDamage;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, ShockDamage);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|AmmoAttribute", ReplicatedUsing = OnRep_ShockDuration)
	FGameplayAttributeData ShockDuration;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, ShockDuration);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|AmmoAttribute", ReplicatedUsing = OnRep_ExplosionChance)
	FGameplayAttributeData ExplosionChance;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, ExplosionChance);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|AmmoAttribute", ReplicatedUsing = OnRep_ExplosionRadius)
	FGameplayAttributeData ExplosionRadius;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, ExplosionRadius);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|AmmoAttribute", ReplicatedUsing = OnRep_ExplosionDamage)
	FGameplayAttributeData ExplosionDamage;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, ExplosionDamage);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|AmmoAttribute", ReplicatedUsing = OnRep_KnockbackStrength)
	FGameplayAttributeData KnockbackStrength;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, KnockbackStrength);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|AmmoAttribute", ReplicatedUsing = OnRep_SpreadModifier)
	FGameplayAttributeData SpreadModifier;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, SpreadModifier);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|AmmoAttribute", ReplicatedUsing = OnRep_RicochetChance)
	FGameplayAttributeData RicochetChance;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, RicochetChance);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|AmmoAttribute", ReplicatedUsing = OnRep_PierceCount)
	FGameplayAttributeData PierceCount;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, PierceCount);

	UPROPERTY(BlueprintReadOnly, Category = "AZ|AmmoAttribute", ReplicatedUsing = OnRep_HomingStrength)
	FGameplayAttributeData HomingStrength;
	ATTRIBUTE_ACCESSORS(UAZ_WeaponAttributeSet, HomingStrength);

protected:
	// --- OnRep Functions ---

	UFUNCTION()
	void OnRep_PistolClipAmmo(const FGameplayAttributeData& OldPistolClipAmmo);
	UFUNCTION()
	void OnRep_RifleClipAmmo(const FGameplayAttributeData& OldRifleClipAmmo);
	UFUNCTION()
	void OnRep_ShotgunClipAmmo(const FGameplayAttributeData& OldShotgunClipAmmo);
	UFUNCTION()
	void OnRep_SMGClipAmmo(const FGameplayAttributeData& OldSMGClipAmmo);

	UFUNCTION()
	void OnRep_PistolReserveAmmo(const FGameplayAttributeData& OldPistolReserveAmmo);
	UFUNCTION()
	void OnRep_RifleReserveAmmo(const FGameplayAttributeData& OldRifleReserveAmmo);
	UFUNCTION()
	void OnRep_ShotgunReserveAmmo(const FGameplayAttributeData& OldShotgunReserveAmmo);
	UFUNCTION()
	void OnRep_SMGReserveAmmo(const FGameplayAttributeData& OldSMGReserveAmmo);
	UFUNCTION()
	void OnRep_RocketReserveAmmo(const FGameplayAttributeData& OldRocketReserveAmmo);

	UFUNCTION()
	void OnRep_MaxPistolClipAmmo(const FGameplayAttributeData& OldMaxPistolClipAmmo);
	UFUNCTION()
	void OnRep_MaxPistolReserveAmmo(const FGameplayAttributeData& OldMaxPistolReserveAmmo);
	UFUNCTION()
	void OnRep_MaxRifleClipAmmo(const FGameplayAttributeData& OldMaxRifleClipAmmo);
	UFUNCTION()
	void OnRep_MaxRifleReserveAmmo(const FGameplayAttributeData& OldMaxRifleReserveAmmo);
	UFUNCTION()
	void OnRep_MaxShotgunClipAmmo(const FGameplayAttributeData& OldMaxShotgunClipAmmo);
	UFUNCTION()
	void OnRep_MaxShotgunReserveAmmo(const FGameplayAttributeData& OldMaxShotgunReserveAmmo);
	UFUNCTION()
	void OnRep_MaxSMGClipAmmo(const FGameplayAttributeData& OldMaxSMGClipAmmo);
	UFUNCTION()
	void OnRep_MaxSMGReserveAmmo(const FGameplayAttributeData& OldMaxSMGReserveAmmo);
	UFUNCTION()
	void OnRep_MaxRocketReserveAmmo(const FGameplayAttributeData& OldMaxRocketReserveAmmo);

	UFUNCTION()
	void OnRep_BaseDamage(const FGameplayAttributeData& OldBaseDamage);
	UFUNCTION()
	void OnRep_FireRate(const FGameplayAttributeData& OldFireRate);
	UFUNCTION()
	void OnRep_ReloadTime(const FGameplayAttributeData& OldReloadTime);

	UFUNCTION()
	void OnRep_StaminaCost(const FGameplayAttributeData& OldStaminaCost);
	UFUNCTION()
	void OnRep_Durability(const FGameplayAttributeData& OldDurability);
	UFUNCTION()
	void OnRep_MaxDurability(const FGameplayAttributeData& OldMaxDurability);


	// Optional Attributes OnRep Functions

	UFUNCTION()
	void OnRep_BurnChance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_BurnDuration(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_BurnDamage(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_FreezeChance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_FreezeDuration(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ShockChance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ShockDamage(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ShockDuration(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ExplosionChance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ExplosionRadius(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_ExplosionDamage(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_KnockbackStrength(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_SpreadModifier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_RicochetChance(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_PierceCount(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_HomingStrength(const FGameplayAttributeData& OldValue);
};
