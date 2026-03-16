#include "AbilitySystem/AttributeSets/AZ_WeaponAttributeSet.h"

#include "AZ_GameplayTags.h"
#include "Net/UnrealNetwork.h"

/*
    FGameplayTag Weapon_None;
    FGameplayTag Weapon_Pistol;
    FGameplayTag Weapon_Rifle;
    FGameplayTag Weapon_RocketLauncher;
    FGameplayTag Weapon_Shotgun;
    FGameplayTag Weapon_SMG;
    FGameplayTag Weapon_Melee;
 */

UAZ_WeaponAttributeSet::UAZ_WeaponAttributeSet()
{
    // ... your constructor logic if any ...
}

FGameplayAttribute UAZ_WeaponAttributeSet::GetMaxReserveAmmoAttributeFromTag(const FGameplayTag& WeaponTag) const
{
    // Get a reference to your static tag library for easy access
    const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();

    if (WeaponTag == Tags.Weapon_Pistol)
    {
        return GetMaxPistolReserveAmmoAttribute();
    }
    if (WeaponTag == Tags.Weapon_Rifle)
    {
        return GetMaxRifleReserveAmmoAttribute();
    }
    if (WeaponTag == Tags.Weapon_Shotgun)
    {
        return GetMaxShotgunReserveAmmoAttribute();
    }
    if (WeaponTag == Tags.Weapon_SMG)
    {
        return GetMaxSMGReserveAmmoAttribute();
    }
    if (WeaponTag == Tags.Weapon_RocketLauncher)
    {
        return GetMaxRocketReserveAmmoAttribute();
    }

    // If the tag doesn't match any ammo type (e.g., Weapon_Melee), return an invalid attribute.
    return FGameplayAttribute();
}

FGameplayAttribute UAZ_WeaponAttributeSet::GetReserveAmmoAttributeFromTag(const FGameplayTag& WeaponTag) const
{
    // Get a reference to your static tag library for easy access
    const FAZ_GameplayTags& Tags = FAZ_GameplayTags::Get();

    if (WeaponTag == Tags.Weapon_Pistol)
    {
        return GetPistolReserveAmmoAttribute();
    }
    if (WeaponTag == Tags.Weapon_Rifle)
    {
        return GetRifleReserveAmmoAttribute();
    }
    if (WeaponTag == Tags.Weapon_Shotgun)
    {
        return GetShotgunReserveAmmoAttribute();
    }
    if (WeaponTag == Tags.Weapon_SMG)
    {
        return GetSMGReserveAmmoAttribute();
    }
    if (WeaponTag == Tags.Weapon_RocketLauncher)
    {
        return GetRocketReserveAmmoAttribute();
    }

    // If the tag doesn't match any ammo type (e.g., Weapon_Melee), return an invalid attribute.
    return FGameplayAttribute();
}

void UAZ_WeaponAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Clip Ammo
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, PistolClipAmmo, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, RifleClipAmmo, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, ShotgunClipAmmo, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, SMGClipAmmo, COND_None, REPNOTIFY_Always);

    // Reserve Ammo
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, PistolReserveAmmo, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, RifleReserveAmmo, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, ShotgunReserveAmmo, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, SMGReserveAmmo, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, RocketReserveAmmo, COND_None, REPNOTIFY_Always);

    // Max Clip Ammo
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, MaxPistolClipAmmo, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, MaxRifleClipAmmo, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, MaxShotgunClipAmmo, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, MaxSMGClipAmmo, COND_None, REPNOTIFY_Always);

    // Max Reserve Ammo
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, MaxPistolReserveAmmo, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, MaxRifleReserveAmmo, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, MaxShotgunReserveAmmo, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, MaxSMGReserveAmmo, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, MaxRocketReserveAmmo, COND_None, REPNOTIFY_Always);

    // Weapon Stats
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, BaseDamage, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, FireRate, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, ReloadTime, COND_None, REPNOTIFY_Always);

    // Status Effects
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, BurnChance, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, BurnDuration, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, BurnDamage, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, FreezeChance, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, FreezeDuration, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, ShockChance, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, ShockDamage, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, ShockDuration, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, ExplosionChance, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, ExplosionRadius, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, ExplosionDamage, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, KnockbackStrength, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, SpreadModifier, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, RicochetChance, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, PierceCount, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, HomingStrength, COND_None, REPNOTIFY_Always);

    // Melee
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, StaminaCost, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, Durability, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAZ_WeaponAttributeSet, MaxDurability, COND_None, REPNOTIFY_Always);

}


// --- OnRep Function Implementations ---

void UAZ_WeaponAttributeSet::OnRep_PistolClipAmmo(const FGameplayAttributeData& OldPistolClipAmmo)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, PistolClipAmmo, OldPistolClipAmmo);
}

void UAZ_WeaponAttributeSet::OnRep_RifleClipAmmo(const FGameplayAttributeData& OldRifleClipAmmo)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, RifleClipAmmo, OldRifleClipAmmo);
}

void UAZ_WeaponAttributeSet::OnRep_ShotgunClipAmmo(const FGameplayAttributeData& OldShotgunClipAmmo)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, ShotgunClipAmmo, OldShotgunClipAmmo);
}

void UAZ_WeaponAttributeSet::OnRep_SMGClipAmmo(const FGameplayAttributeData& OldSMGClipAmmo)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, SMGClipAmmo, OldSMGClipAmmo);
}

void UAZ_WeaponAttributeSet::OnRep_PistolReserveAmmo(const FGameplayAttributeData& OldPistolReserveAmmo)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, PistolReserveAmmo, OldPistolReserveAmmo);
}

void UAZ_WeaponAttributeSet::OnRep_RifleReserveAmmo(const FGameplayAttributeData& OldRifleReserveAmmo)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, RifleReserveAmmo, OldRifleReserveAmmo);
}

void UAZ_WeaponAttributeSet::OnRep_ShotgunReserveAmmo(const FGameplayAttributeData& OldShotgunReserveAmmo)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, ShotgunReserveAmmo, OldShotgunReserveAmmo);
}

void UAZ_WeaponAttributeSet::OnRep_SMGReserveAmmo(const FGameplayAttributeData& OldSMGReserveAmmo)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, SMGReserveAmmo, OldSMGReserveAmmo);
}

void UAZ_WeaponAttributeSet::OnRep_RocketReserveAmmo(const FGameplayAttributeData& OldRocketReserveAmmo)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, RocketReserveAmmo, OldRocketReserveAmmo);
}

void UAZ_WeaponAttributeSet::OnRep_MaxPistolClipAmmo(const FGameplayAttributeData& OldMaxPistolClipAmmo)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, MaxPistolClipAmmo, OldMaxPistolClipAmmo);
}

void UAZ_WeaponAttributeSet::OnRep_MaxPistolReserveAmmo(const FGameplayAttributeData& OldMaxPistolReserveAmmo)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, MaxPistolReserveAmmo, OldMaxPistolReserveAmmo);
}

void UAZ_WeaponAttributeSet::OnRep_MaxRifleClipAmmo(const FGameplayAttributeData& OldMaxRifleClipAmmo)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, MaxRifleClipAmmo, OldMaxRifleClipAmmo);
}

void UAZ_WeaponAttributeSet::OnRep_MaxRifleReserveAmmo(const FGameplayAttributeData& OldMaxRifleReserveAmmo)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, MaxRifleReserveAmmo, OldMaxRifleReserveAmmo);
}

void UAZ_WeaponAttributeSet::OnRep_MaxShotgunClipAmmo(const FGameplayAttributeData& OldMaxShotgunClipAmmo)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, MaxShotgunClipAmmo, OldMaxShotgunClipAmmo);
}

void UAZ_WeaponAttributeSet::OnRep_MaxShotgunReserveAmmo(const FGameplayAttributeData& OldMaxShotgunReserveAmmo)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, MaxShotgunReserveAmmo, OldMaxShotgunReserveAmmo);
}

void UAZ_WeaponAttributeSet::OnRep_MaxSMGClipAmmo(const FGameplayAttributeData& OldMaxSMGClipAmmo)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, MaxSMGClipAmmo, OldMaxSMGClipAmmo);
}

void UAZ_WeaponAttributeSet::OnRep_MaxSMGReserveAmmo(const FGameplayAttributeData& OldMaxSMGReserveAmmo)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, MaxSMGReserveAmmo, OldMaxSMGReserveAmmo);
}

void UAZ_WeaponAttributeSet::OnRep_MaxRocketReserveAmmo(const FGameplayAttributeData& OldMaxRocketReserveAmmo)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, MaxRocketReserveAmmo, OldMaxRocketReserveAmmo);
}

void UAZ_WeaponAttributeSet::OnRep_BaseDamage(const FGameplayAttributeData& OldBaseDamage)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, BaseDamage, OldBaseDamage);
}

void UAZ_WeaponAttributeSet::OnRep_FireRate(const FGameplayAttributeData& OldFireRate)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, FireRate, OldFireRate);
}

void UAZ_WeaponAttributeSet::OnRep_ReloadTime(const FGameplayAttributeData& OldReloadTime)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, ReloadTime, OldReloadTime);
}

void UAZ_WeaponAttributeSet::OnRep_StaminaCost(const FGameplayAttributeData& OldStaminaCost)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, StaminaCost, OldStaminaCost);
}

void UAZ_WeaponAttributeSet::OnRep_Durability(const FGameplayAttributeData& OldDurability)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, Durability, OldDurability);
}

void UAZ_WeaponAttributeSet::OnRep_MaxDurability(const FGameplayAttributeData& OldMaxDurability)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, MaxDurability, OldMaxDurability);
}

void UAZ_WeaponAttributeSet::OnRep_BurnChance(const FGameplayAttributeData& OldValue) 
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, BurnChance, OldValue);
}

void UAZ_WeaponAttributeSet::OnRep_BurnDuration(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, BurnDuration, OldValue);
}

void UAZ_WeaponAttributeSet::OnRep_BurnDamage(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, BurnDamage, OldValue);
}

void UAZ_WeaponAttributeSet::OnRep_FreezeChance(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, FreezeChance, OldValue);
}

void UAZ_WeaponAttributeSet::OnRep_FreezeDuration(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, FreezeDuration, OldValue);
}

void UAZ_WeaponAttributeSet::OnRep_ShockChance(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, ShockChance, OldValue);
}

void UAZ_WeaponAttributeSet::OnRep_ShockDamage(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, ShockDamage, OldValue);
}

void UAZ_WeaponAttributeSet::OnRep_ShockDuration(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, ShockDuration, OldValue);
}

void UAZ_WeaponAttributeSet::OnRep_ExplosionChance(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, ExplosionChance, OldValue);
}

void UAZ_WeaponAttributeSet::OnRep_ExplosionRadius(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, ExplosionRadius, OldValue);
}

void UAZ_WeaponAttributeSet::OnRep_ExplosionDamage(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, ExplosionDamage, OldValue);
}

void UAZ_WeaponAttributeSet::OnRep_KnockbackStrength(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, KnockbackStrength, OldValue);
}

void UAZ_WeaponAttributeSet::OnRep_SpreadModifier(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, SpreadModifier, OldValue);
}

void UAZ_WeaponAttributeSet::OnRep_RicochetChance(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, RicochetChance, OldValue);
}

void UAZ_WeaponAttributeSet::OnRep_PierceCount(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, PierceCount, OldValue);
}

void UAZ_WeaponAttributeSet::OnRep_HomingStrength(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAZ_WeaponAttributeSet, HomingStrength, OldValue);
}



