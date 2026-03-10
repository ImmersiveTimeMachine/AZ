// (Assuming this is in your AZ_GameplayTags.cpp)
#include "AZ/Public/AZ_GameplayTags.h" 
#include "AZ/AZ.h"
#include "GameplayTagsManager.h"

FAZ_GameplayTags FAZ_GameplayTags::GameplayTags; // Static instance definition

void FAZ_GameplayTags::InitializeNativeGameplayTags()
{
    UGameplayTagsManager& TagsManager = UGameplayTagsManager::Get();

    // -----------------------------------------------------------------------------------
    // Helper Lambda to reduce boilerplate
    // This handles adding the tag AND logging it in one go.
    // -----------------------------------------------------------------------------------
    auto AddTag = [&](FGameplayTag& OutTag, const FName& TagName, const FString& TagComment)
    {
        OutTag = TagsManager.AddNativeGameplayTag(TagName, TagComment);
        
        // Changed Log level to Verbose to avoid spamming the console, 
        // strictly adhere to 'Log' if you specifically need it visible always.
        UE_LOG(Log_AZ, Verbose, TEXT("Registering %s"), *TagName.ToString());
    };

	/*
		1. Strength (The Brawler)
		
			Melee Damage: Essential for saving ammo (Silent Hill/TLOU style).

			Carry Capacity: Vital for inventory management (Resident Evil style).

			Grapple Defense: How easily you can push off a zombie that grabs you.

		2. Agility (The Ghost)
		
			Stealth / Noise: How quietly you move (TLOU Clicker mechanics).

			Reload Speed: Crucial during tense boss fights.

			Weapon Swap Speed: Switching from empty shotgun to pistol before you die.

		3. Resilience (The Tank)
		
			Max Health: Surviving hits.

			Infection Resistance: (Theme specific) How long you can survive a bite or poison gas before needing a cure.

			Trauma / Sanity: (Silent Hill theme) Resistance to hallucinations or panic effects that mess up your aim.

		4. Expertise (The Survivor - Merged)
		
			Crafting Efficiency: Instead of "unlocking" recipes, high Expertise means you use fewer resources to craft the same item (e.g., 1 Alcohol + 1 Rag = 2 Molotovs instead of 1).

			Lockpicking / Hacking: Access to "optional" rooms with extra loot (RE style special keys).

			Critical Hit Chance: Knowing exactly where to stab a monster to kill it instantly.

			Healing Efficiency: A medkit heals 100 HP instead of 50 HP because you know how to dress a wound properly.
	 */
    // =========================================================
    // PRIMARY ATTRIBUTES
    // =========================================================
    AddTag(GameplayTags.Attributes_Primary_Strength,     FName("Attributes.Primary.Strength"),     TEXT("Primary Attribute: Strength - Affects melee damage, carrying capacity."));
    AddTag(GameplayTags.Attributes_Primary_Agility,      FName("Attributes.Primary.Agility"),      TEXT("Primary Attribute: Agility - Affects ranged, speed, evasion."));
    AddTag(GameplayTags.Attributes_Primary_Resilience,   FName("Attributes.Primary.Resilience"),   TEXT("Primary Attribute: Resilience - Affects health, armor."));
    AddTag(GameplayTags.Attributes_Primary_Intelligence, FName("Attributes.Primary.Intelligence"), TEXT("Primary Attribute: Intelligence - Affects crafting, hacking."));
    AddTag(GameplayTags.Attributes_Primary_Expertise,    FName("Attributes.Primary.Expertise"),    TEXT("Primary Attribute: Expertise - Affects crits, cooldowns."));

    // =========================================================
    // SECONDARY ATTRIBUTES - COMBAT
    // =========================================================
    AddTag(GameplayTags.Attributes_Secondary_Combat_MaxHealth,          FName("Attributes.Secondary.Combat.MaxHealth"),          TEXT("Combat Attribute: Maximum health capacity."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_HealthRegenRate,    FName("Attributes.Secondary.Combat.HealthRegenRate"),    TEXT("Combat Attribute: Rate of health regeneration."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_MeleeDamage,        FName("Attributes.Secondary.Combat.MeleeDamage"),        TEXT("Combat Attribute: Damage dealt by melee attacks."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_RangedDamage,       FName("Attributes.Secondary.Combat.RangedDamage"),       TEXT("Combat Attribute: Damage dealt by ranged attacks."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_Accuracy,           FName("Attributes.Secondary.Combat.Accuracy"),           TEXT("Combat Attribute: Precision of attacks."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_ReloadSpeed,        FName("Attributes.Secondary.Combat.ReloadSpeed"),        TEXT("Combat Attribute: Speed of weapon reloading."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_RecoilControl,      FName("Attributes.Secondary.Combat.RecoilControl"),      TEXT("Combat Attribute: Ability to control weapon recoil."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_CriticalHitChance,  FName("Attributes.Secondary.Combat.CriticalHitChance"),  TEXT("Combat Attribute: Chance to land a critical hit."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_CriticalHitDamage,  FName("Attributes.Secondary.Combat.CriticalHitDamage"),  TEXT("Combat Attribute: Damage multiplier for critical hits."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_AttackSpeed,        FName("Attributes.Secondary.Combat.AttackSpeed"),        TEXT("Combat Attribute: Speed of performing attacks."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_Armor,              FName("Attributes.Secondary.Combat.Armor"),              TEXT("Combat Attribute: Damage reduction from armor."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_PhysicalResistance, FName("Attributes.Secondary.Combat.PhysicalResistance"), TEXT("Combat Attribute: Resistance to physical damage."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_ExplosiveResistance,FName("Attributes.Secondary.Combat.ExplosiveResistance"),TEXT("Combat Attribute: Resistance to explosive damage."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_ElementalDamage,    FName("Attributes.Secondary.Combat.ElementalDamage"),    TEXT("Combat Attribute: Bonus damage from elemental sources."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_ArmorPenetration,   FName("Attributes.Secondary.Combat.ArmorPenetration"),   TEXT("Combat Attribute: Ability to penetrate enemy armor."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_KnockbackChance,    FName("Attributes.Secondary.Combat.KnockbackChance"),    TEXT("Combat Attribute: Chance to knock back enemies."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_HeavyWeaponHandling,FName("Attributes.Secondary.Combat.HeavyWeaponHandling"),TEXT("Combat Attribute: Proficiency with heavy weapons."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_EvasionChance,      FName("Attributes.Secondary.Combat.EvasionChance"),      TEXT("Combat Attribute: Chance to evade attacks."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_StaggerResistance,  FName("Attributes.Secondary.Combat.StaggerResistance"),  TEXT("Combat Attribute: Resistance to being staggered."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_WeakSpotDetection,  FName("Attributes.Secondary.Combat.WeakSpotDetection"),  TEXT("Combat Attribute: Ability to detect weak spots."));
    AddTag(GameplayTags.Attributes_Secondary_Combat_StatusEffectDuration,FName("Attributes.Secondary.Combat.StatusEffectDuration"),TEXT("Combat Attribute: Duration of inflicted status effects."));

    // =========================================================
    // SECONDARY ATTRIBUTES - SURVIVAL
    // =========================================================
    AddTag(GameplayTags.Attributes_Secondary_Survival_MaxStamina,             FName("Attributes.Secondary.Survival.MaxStamina"),             TEXT("Survival Attribute: Maximum stamina capacity."));
    AddTag(GameplayTags.Attributes_Secondary_Survival_StaminaRegenRate,       FName("Attributes.Secondary.Survival.StaminaRegenRate"),       TEXT("Survival Attribute: Rate of stamina regeneration."));
    AddTag(GameplayTags.Attributes_Secondary_Survival_CarryWeight,            FName("Attributes.Secondary.Survival.CarryWeight"),            TEXT("Survival Attribute: Maximum carrying capacity."));
    AddTag(GameplayTags.Attributes_Secondary_Survival_MovementSpeed,          FName("Attributes.Secondary.Survival.MovementSpeed"),          TEXT("Survival Attribute: Character movement speed."));
    AddTag(GameplayTags.Attributes_Secondary_Survival_DiseaseResistance,      FName("Attributes.Secondary.Survival.DiseaseResistance"),      TEXT("Survival Attribute: Resistance to diseases."));
    AddTag(GameplayTags.Attributes_Secondary_Survival_InfectionResistance,    FName("Attributes.Secondary.Survival.InfectionResistance"),    TEXT("Survival Attribute: Resistance to infections."));
    AddTag(GameplayTags.Attributes_Secondary_Survival_PoisonResistance,       FName("Attributes.Secondary.Survival.PoisonResistance"),       TEXT("Survival Attribute: Resistance to poison."));
    AddTag(GameplayTags.Attributes_Secondary_Survival_StatusEffectResistance, FName("Attributes.Secondary.Survival.StatusEffectResistance"), TEXT("Survival Attribute: General resistance to negative status effects."));
    AddTag(GameplayTags.Attributes_Secondary_Survival_HungerRate,             FName("Attributes.Secondary.Survival.HungerRate"),             TEXT("Survival Attribute: Rate at which hunger increases."));
    AddTag(GameplayTags.Attributes_Secondary_Survival_ThirstRate,             FName("Attributes.Secondary.Survival.ThirstRate"),             TEXT("Survival Attribute: Rate at which thirst increases."));

    // =========================================================
    // SECONDARY ATTRIBUTES - UTILITY
    // =========================================================
    AddTag(GameplayTags.Attributes_Secondary_Utility_Perception,          FName("Attributes.Secondary.Utility.Perception"),          TEXT("Utility Attribute: Ability to perceive environment."));
    AddTag(GameplayTags.Attributes_Secondary_Utility_CraftingSpeed,       FName("Attributes.Secondary.Utility.CraftingSpeed"),       TEXT("Utility Attribute: Speed of crafting items."));
    AddTag(GameplayTags.Attributes_Secondary_Utility_CraftingQuality,     FName("Attributes.Secondary.Utility.CraftingQuality"),     TEXT("Utility Attribute: Quality of crafted items."));
    AddTag(GameplayTags.Attributes_Secondary_Utility_RepairEfficiency,    FName("Attributes.Secondary.Utility.RepairEfficiency"),    TEXT("Utility Attribute: Efficiency of repairing items."));
    AddTag(GameplayTags.Attributes_Secondary_Utility_HackingSpeed,        FName("Attributes.Secondary.Utility.HackingSpeed"),        TEXT("Utility Attribute: Speed of hacking."));
    AddTag(GameplayTags.Attributes_Secondary_Utility_Lockpicking,         FName("Attributes.Secondary.Utility.Lockpicking"),         TEXT("Utility Attribute: Skill in lockpicking."));
    AddTag(GameplayTags.Attributes_Secondary_Utility_Stealth,             FName("Attributes.Secondary.Utility.Stealth"),             TEXT("Utility Attribute: Effectiveness of stealth."));
    AddTag(GameplayTags.Attributes_Secondary_Utility_Barter,              FName("Attributes.Secondary.Utility.Barter"),              TEXT("Utility Attribute: Effectiveness in bartering."));
    AddTag(GameplayTags.Attributes_Secondary_Utility_ExperienceGainRate,  FName("Attributes.Secondary.Utility.ExperienceGainRate"),  TEXT("Utility Attribute: Rate of experience point gain."));
    AddTag(GameplayTags.Attributes_Secondary_Utility_InteractionStrength, FName("Attributes.Secondary.Utility.InteractionStrength"), TEXT("Utility Attribute: Strength for interacting with objects."));
    AddTag(GameplayTags.Attributes_Secondary_Utility_CooldownReduction,   FName("Attributes.Secondary.Utility.CooldownReduction"),   TEXT("Utility Attribute: Reduction in ability cooldowns."));
    AddTag(GameplayTags.Attributes_Secondary_Utility_ResourcePool,        FName("Attributes.Secondary.Utility.ResourcePool"),        TEXT("Utility Attribute: Max capacity of special resources."));
    AddTag(GameplayTags.Attributes_Secondary_Utility_ResourceRegenRate,   FName("Attributes.Secondary.Utility.ResourceRegenRate"),   TEXT("Utility Attribute: Regen rate of special resources."));
    AddTag(GameplayTags.Attributes_Secondary_Utility_HealingEffectiveness,FName("Attributes.Secondary.Utility.HealingEffectiveness"),TEXT("Utility Attribute: Effectiveness of healing items."));

    // =========================================================
    // VITAL ATTRIBUTES (Dynamic)
    // =========================================================
    AddTag(GameplayTags.Attributes_Vital_CurrentHealth,  FName("Attributes.Vital.CurrentHealth"),  TEXT("Vital Attribute: Current health value."));
    AddTag(GameplayTags.Attributes_Vital_CurrentStamina, FName("Attributes.Vital.CurrentStamina"), TEXT("Vital Attribute: Current stamina value."));

    // =========================================================
    // NPC - INFECTED HUMANOID
    // =========================================================
    AddTag(GameplayTags.Infected_Humanoid_Health,        FName("NPC.Infected.Humanoid.Health"),        TEXT("Humanoid Infected: Current health."));
    AddTag(GameplayTags.Infected_Humanoid_MaxHealth,     FName("NPC.Infected.Humanoid.MaxHealth"),     TEXT("Humanoid Infected: Maximum health."));
    AddTag(GameplayTags.Infected_Humanoid_MovementSpeed, FName("NPC.Infected.Humanoid.MovementSpeed"), TEXT("Humanoid Infected: Movement speed."));
    AddTag(GameplayTags.Infected_Humanoid_Perception,    FName("NPC.Infected.Humanoid.Perception"),    TEXT("Humanoid Infected: Perception range."));
    AddTag(GameplayTags.Infected_Humanoid_Aggression,    FName("NPC.Infected.Humanoid.Aggression"),    TEXT("Humanoid Infected: Aggression level."));
    AddTag(GameplayTags.Infected_Humanoid_Armor,         FName("NPC.Infected.Humanoid.Armor"),         TEXT("Humanoid Infected: Armor value."));
    AddTag(GameplayTags.Infected_Humanoid_MeleeDamage,   FName("NPC.Infected.Humanoid.MeleeDamage"),   TEXT("Humanoid Infected: Melee damage."));
    AddTag(GameplayTags.Infected_Humanoid_RangedDamage,  FName("NPC.Infected.Humanoid.RangedDamage"),  TEXT("Humanoid Infected: Ranged damage."));
    AddTag(GameplayTags.Infected_Humanoid_AttackSpeed,   FName("NPC.Infected.Humanoid.AttackSpeed"),   TEXT("Humanoid Infected: Attack speed."));
    AddTag(GameplayTags.Infected_Humanoid_AttackRange,   FName("NPC.Infected.Humanoid.AttackRange"),   TEXT("Humanoid Infected: Attack range."));
    AddTag(GameplayTags.Infected_Humanoid_Accuracy,      FName("NPC.Infected.Humanoid.Accuracy"),      TEXT("Humanoid Infected: Accuracy."));
    AddTag(GameplayTags.Infected_Humanoid_Resistance,    FName("NPC.Infected.Humanoid.Resistance"),    TEXT("Humanoid Infected: Resistances."));

    // =========================================================
    // NPC - ANTAGONIST HUMAN
    // =========================================================
    AddTag(GameplayTags.Antagonist_Human_Health,        FName("NPC.Antagonist.Human.Health"),        TEXT("Human Antagonist: Current health."));
    AddTag(GameplayTags.Antagonist_Human_MaxHealth,     FName("NPC.Antagonist.Human.MaxHealth"),     TEXT("Human Antagonist: Maximum health."));
    AddTag(GameplayTags.Antagonist_Human_MovementSpeed, FName("NPC.Antagonist.Human.MovementSpeed"), TEXT("Human Antagonist: Movement speed."));
    AddTag(GameplayTags.Antagonist_Human_Perception,    FName("NPC.Antagonist.Human.Perception"),    TEXT("Human Antagonist: Perception."));
    AddTag(GameplayTags.Antagonist_Human_Aggression,    FName("NPC.Antagonist.Human.Aggression"),    TEXT("Human Antagonist: Aggression."));
    AddTag(GameplayTags.Antagonist_Human_Armor,         FName("NPC.Antagonist.Human.Armor"),         TEXT("Human Antagonist: Armor."));
    AddTag(GameplayTags.Antagonist_Human_MeleeDamage,   FName("NPC.Antagonist.Human.MeleeDamage"),   TEXT("Human Antagonist: Melee damage."));
    AddTag(GameplayTags.Antagonist_Human_RangedDamage,  FName("NPC.Antagonist.Human.RangedDamage"),  TEXT("Human Antagonist: Ranged damage."));
    AddTag(GameplayTags.Antagonist_Human_Accuracy,      FName("NPC.Antagonist.Human.Accuracy"),      TEXT("Human Antagonist: Accuracy."));
    AddTag(GameplayTags.Antagonist_Human_AttackSpeed,   FName("NPC.Antagonist.Human.AttackSpeed"),   TEXT("Human Antagonist: Attack speed."));
    AddTag(GameplayTags.Antagonist_Human_AttackRange,   FName("NPC.Antagonist.Human.AttackRange"),   TEXT("Human Antagonist: Attack range."));
    AddTag(GameplayTags.Antagonist_Human_Morale,        FName("NPC.Antagonist.Human.Morale"),        TEXT("Human Antagonist: Morale level."));

    // =========================================================
    // NPC - INFECTED ANIMAL
    // =========================================================
    AddTag(GameplayTags.Infected_Animal_Health,        FName("NPC.Infected.Animal.Health"),        TEXT("Animal Infected: Current health."));
    AddTag(GameplayTags.Infected_Animal_MaxHealth,     FName("NPC.Infected.Animal.MaxHealth"),     TEXT("Animal Infected: Maximum health."));
    AddTag(GameplayTags.Infected_Animal_MovementSpeed, FName("NPC.Infected.Animal.MovementSpeed"), TEXT("Animal Infected: Movement speed."));
    AddTag(GameplayTags.Infected_Animal_Perception,    FName("NPC.Infected.Animal.Perception"),    TEXT("Animal Infected: Perception."));
    AddTag(GameplayTags.Infected_Animal_Aggression,    FName("NPC.Infected.Animal.Aggression"),    TEXT("Animal Infected: Aggression."));
    AddTag(GameplayTags.Infected_Animal_Damage,        FName("NPC.Infected.Animal.Damage"),        TEXT("Animal Infected: Attack damage."));
    AddTag(GameplayTags.Infected_Animal_AttackSpeed,   FName("NPC.Infected.Animal.AttackSpeed"),   TEXT("Animal Infected: Attack speed."));
    AddTag(GameplayTags.Infected_Animal_AttackRange,   FName("NPC.Infected.Animal.AttackRange"),   TEXT("Animal Infected: Attack range."));
    AddTag(GameplayTags.Infected_Animal_Disease,       FName("NPC.Infected.Animal.Disease"),       TEXT("Animal Infected: Disease potency."));

    // =========================================================
    // BOSS ATTRIBUTES
    // =========================================================
    AddTag(GameplayTags.Boss_ChargeDamage,        FName("NPC.Boss.ChargeDamage"),        TEXT("Boss: Charge attack damage."));
    AddTag(GameplayTags.Boss_GroundSlamRadius,    FName("NPC.Boss.GroundSlamRadius"),    TEXT("Boss: Ground slam radius."));
    AddTag(GameplayTags.Boss_ToxicCloudDuration,  FName("NPC.Boss.ToxicCloudDuration"),  TEXT("Boss: Toxic cloud duration."));
    AddTag(GameplayTags.Boss_PsionicPower,        FName("NPC.Boss.PsionicPower"),        TEXT("Boss: Psionic strength."));
    AddTag(GameplayTags.Boss_SummoningCooldown,   FName("NPC.Boss.SummoningCooldown"),   TEXT("Boss: Summoning cooldown."));
    AddTag(GameplayTags.Boss_MutationLevel,       FName("NPC.Boss.MutationLevel"),       TEXT("Boss: Mutation level."));

    // =========================================================
    // GAMEPLAY EFFECTS
    // =========================================================
    AddTag(GameplayTags.GameplayEffect_Input_Magnitude,  FName("GameplayEffect.Input.Magnitude"), TEXT("Pass magnitude to GE from input context."));
    AddTag(GameplayTags.GameplayEffect_CharacterInitTag, FName("GameplayEffect.CharacterInit"),   TEXT("Character initialization effects."));

    // =========================================================
    // INPUT ACTIONS
    // =========================================================
    AddTag(GameplayTags.Input_Action_None,             FName("Input.Action.None"),             TEXT("Input: No action."));
    AddTag(GameplayTags.Input_Action_PrimaryAttack,    FName("Input.Action.PrimaryAttack"),    TEXT("Input: Primary attack."));
    AddTag(GameplayTags.Input_Action_SecondaryAttack,  FName("Input.Action.SecondaryAttack"),  TEXT("Input: Secondary attack."));
    AddTag(GameplayTags.Input_Action_Aim,              FName("Input.Action.Aim"),              TEXT("Input: Aim down sights."));
    AddTag(GameplayTags.Input_Action_Reload,           FName("Input.Action.Reload"),           TEXT("Input: Reload weapon."));
    AddTag(GameplayTags.Input_Action_MeleeAttack,      FName("Input.Action.MeleeAttack"),      TEXT("Input: Melee attack."));
    AddTag(GameplayTags.Input_Action_ThrowGrenade,     FName("Input.Action.ThrowGrenade"),     TEXT("Input: Throw grenade/tactical."));
    AddTag(GameplayTags.Input_Action_Jump,             FName("Input.Action.Jump"),             TEXT("Input: Jump."));
    AddTag(GameplayTags.Input_Action_Crouch,           FName("Input.Action.Crouch"),           TEXT("Input: Crouch (hold)."));
    AddTag(GameplayTags.Input_Action_ToggleCrouch,     FName("Input.Action.ToggleCrouch"),     TEXT("Input: Toggle crouch."));
    AddTag(GameplayTags.Input_Action_Sprint,           FName("Input.Action.Sprint"),           TEXT("Input: Sprint."));
    AddTag(GameplayTags.Input_Action_Dodge,            FName("Input.Action.Dodge"),            TEXT("Input: Dodge."));
    AddTag(GameplayTags.Input_Action_Move,             FName("Input.Action.Move"),             TEXT("Input: Move."));
    AddTag(GameplayTags.Input_Action_Interact,         FName("Input.Action.Interact"),         TEXT("Input: Interact."));
    AddTag(GameplayTags.Input_Action_OpenInventory,    FName("Input.Action.OpenInventory"),    TEXT("Input: Open inventory."));
    AddTag(GameplayTags.Input_Action_OpenMap,          FName("Input.Action.OpenMap"),          TEXT("Input: Open map."));
    AddTag(GameplayTags.Input_Action_OpenJournal,      FName("Input.Action.OpenJournal"),      TEXT("Input: Open journal."));
    AddTag(GameplayTags.Input_Action_PauseGame,        FName("Input.Action.PauseGame"),        TEXT("Input: Pause game."));
    AddTag(GameplayTags.Input_Action_UseAbility1,      FName("Input.Action.UseAbility1"),      TEXT("Input: Ability 1."));
    AddTag(GameplayTags.Input_Action_UseAbility2,      FName("Input.Action.UseAbility2"),      TEXT("Input: Ability 2."));
    AddTag(GameplayTags.Input_Action_UseAbility3,      FName("Input.Action.UseAbility3"),      TEXT("Input: Ability 3."));
    AddTag(GameplayTags.Input_Action_UseAbility4,      FName("Input.Action.UseAbility4"),      TEXT("Input: Ability 4."));
    AddTag(GameplayTags.Input_Action_NextWeapon,       FName("Input.Action.NextWeapon"),       TEXT("Input: Next weapon."));
    AddTag(GameplayTags.Input_Action_PreviousWeapon,   FName("Input.Action.PreviousWeapon"),   TEXT("Input: Previous weapon."));
    AddTag(GameplayTags.Input_Action_EquipWeaponSlot1, FName("Input.Action.EquipWeaponSlot1"), TEXT("Input: Equip Slot 1."));
    AddTag(GameplayTags.Input_Action_EquipWeaponSlot2, FName("Input.Action.EquipWeaponSlot2"), TEXT("Input: Equip Slot 2."));
    AddTag(GameplayTags.Input_Action_EquipWeaponSlot3, FName("Input.Action.EquipWeaponSlot3"), TEXT("Input: Equip Slot 3."));
    AddTag(GameplayTags.Input_Action_UseQuickHeal,     FName("Input.Action.UseQuickHeal"),     TEXT("Input: Quick heal."));
    AddTag(GameplayTags.Input_Action_UseQuickItem,     FName("Input.Action.UseQuickItem"),     TEXT("Input: Use quick item."));

    // =========================================================
    // ABILITY STATUS & TYPES
    // =========================================================
    AddTag(GameplayTags.Abilities_Status_Locked,    FName("Ability.Status.Locked"),           TEXT("Ability Status: Locked."));
    AddTag(GameplayTags.Abilities_Status_Eligible,  FName("Ability.Status.Eligible"),         TEXT("Ability Status: Eligible to learn."));
    AddTag(GameplayTags.Abilities_Status_Unlocked,  FName("Ability.Status.Unlocked"),         TEXT("Ability Status: Unlocked."));
    AddTag(GameplayTags.Abilities_Status_Equipped,  FName("Ability.Status.Equipped"),         TEXT("Ability Status: Equipped."));
    AddTag(GameplayTags.Abilities_Type_Offensive,   FName("Ability.Type.Active.Offensive"),   TEXT("Ability Type: Active Offensive."));
    AddTag(GameplayTags.Abilities_Type_Passive,     FName("Ability.Type.Passive"),            TEXT("Ability Type: Passive."));
    AddTag(GameplayTags.Abilities_Type_None,        FName("Ability.Type.None"),               TEXT("Ability Type: None."));

    // =========================================================
    // WEAPON TYPES
    // =========================================================
    AddTag(GameplayTags.Weapon_None,            FName("Weapon.None"),            TEXT("No weapon equipped"));
    AddTag(GameplayTags.Weapon_Pistol,          FName("Weapon.Pistol"),          TEXT("Pistol weapon type"));
    AddTag(GameplayTags.Weapon_Rifle,           FName("Weapon.Rifle"),           TEXT("Rifle weapon type"));
    AddTag(GameplayTags.Weapon_RocketLauncher,  FName("Weapon.RocketLauncher"),  TEXT("Rocket launcher weapon type"));
    AddTag(GameplayTags.Weapon_Shotgun,         FName("Weapon.Shotgun"),         TEXT("Shotgun weapon type"));
    AddTag(GameplayTags.Weapon_SMG,             FName("Weapon.SMG"),             TEXT("SMG weapon type"));
    AddTag(GameplayTags.Weapon_Melee,           FName("Weapon.Melee"),           TEXT("Melee weapon type"));

    // =========================================================
    // STATES (Movement, Combat, Character)
    // =========================================================
    AddTag(GameplayTags.Movement_Idle,          FName("Movement.Idle"),          TEXT("Character is idle"));
    AddTag(GameplayTags.Movement_Walking,       FName("Movement.Walking"),       TEXT("Character is walking"));
    AddTag(GameplayTags.Movement_Running,       FName("Movement.Running"),       TEXT("Character is running"));
    AddTag(GameplayTags.Movement_Sprinting,     FName("Movement.Sprinting"),     TEXT("Character is sprinting"));
    AddTag(GameplayTags.Movement_Crouching,     FName("Movement.Crouching"),     TEXT("Character is crouching"));
    AddTag(GameplayTags.Movement_Jumping,       FName("Movement.Jumping"),       TEXT("Character is jumping"));
    AddTag(GameplayTags.Movement_Falling,       FName("Movement.Falling"),       TEXT("Character is falling"));

    AddTag(GameplayTags.Combat_Aiming,          FName("Combat.Aiming"),          TEXT("Character is aiming"));
    AddTag(GameplayTags.Combat_Blocking,        FName("Combat.Blocking"),        TEXT("Character is blocking"));
    AddTag(GameplayTags.Combat_Attacking,       FName("Combat.Attacking"),       TEXT("Character is attacking"));
    AddTag(GameplayTags.Combat_Reloading,       FName("Combat.Reloading"),       TEXT("Character is reloading"));

    AddTag(GameplayTags.Character_Dead,         FName("Character.Dead"),         TEXT("Character is dead"));
    AddTag(GameplayTags.Character_Dying,        FName("Character.Dying"),        TEXT("Character is dying"));
    AddTag(GameplayTags.Character_Stunned,      FName("Character.Stunned"),      TEXT("Character is stunned"));
    AddTag(GameplayTags.Character_Interacting,  FName("Character.Interacting"),  TEXT("Character is interacting"));

    AddTag(GameplayTags.Animation_HipFire,      FName("Animation.HipFire"),      TEXT("Character is firing from hip"));
    AddTag(GameplayTags.Animation_ADS,          FName("Animation.ADS"),          TEXT("Character is aiming down sights"));

    // =========================================================
    // ABILITY ANIMATION STATES (Unarmed)
    // =========================================================
    AddTag(GameplayTags.Ability_Animation_Unarmed_Idle,             FName("Ability.Animation.Unarmed.Idle"),             TEXT("Anim: Unarmed idle"));
    AddTag(GameplayTags.Ability_Animation_Unarmed_Walking,          FName("Ability.Animation.Unarmed.Walking"),          TEXT("Anim: Unarmed walking"));
    AddTag(GameplayTags.Ability_Animation_Unarmed_Running,          FName("Ability.Animation.Unarmed.Running"),          TEXT("Anim: Unarmed running"));
    AddTag(GameplayTags.Ability_Animation_Unarmed_Sprinting,        FName("Ability.Animation.Unarmed.Sprinting"),        TEXT("Anim: Unarmed sprinting"));
    AddTag(GameplayTags.Ability_Animation_Unarmed_CrouchingIdle,    FName("Ability.Animation.Unarmed.CrouchingIdle"),    TEXT("Anim: Unarmed crouching idle"));
    AddTag(GameplayTags.Ability_Animation_Unarmed_CrouchingWalking, FName("Ability.Animation.Unarmed.CrouchingWalking"), TEXT("Anim: Unarmed crouching walking"));
    AddTag(GameplayTags.Ability_Animation_Unarmed_Attack_Light,     FName("Ability.Animation.Unarmed.Attack.Light"),     TEXT("Anim: Unarmed light attack"));
    AddTag(GameplayTags.Ability_Animation_Unarmed_Attack_Heavy,     FName("Ability.Animation.Unarmed.Attack.Heavy"),     TEXT("Anim: Unarmed heavy attack"));
    AddTag(GameplayTags.Ability_Animation_Unarmed_Jumping,          FName("Ability.Animation.Unarmed.Jumping"),          TEXT("Anim: Unarmed jumping"));
    AddTag(GameplayTags.Ability_Animation_Unarmed_Falling,          FName("Ability.Animation.Unarmed.Falling"),          TEXT("Anim: Unarmed falling"));

    // =========================================================
    // ABILITY ANIMATION STATES (Pistol)
    // =========================================================
    AddTag(GameplayTags.Ability_Animation_Pistol_Idle,              FName("Ability.Animation.Pistol.Idle"),              TEXT("Anim: Pistol idle"));
    AddTag(GameplayTags.Ability_Animation_Pistol_Walking,           FName("Ability.Animation.Pistol.Walking"),           TEXT("Anim: Pistol walking"));
    AddTag(GameplayTags.Ability_Animation_Pistol_Running,           FName("Ability.Animation.Pistol.Running"),           TEXT("Anim: Pistol running"));
    AddTag(GameplayTags.Ability_Animation_Pistol_Sprinting,         FName("Ability.Animation.Pistol.Sprinting"),         TEXT("Anim: Pistol sprinting"));
    AddTag(GameplayTags.Ability_Animation_Pistol_CrouchingIdle,     FName("Ability.Animation.Pistol.CrouchingIdle"),     TEXT("Anim: Pistol crouching idle"));
    AddTag(GameplayTags.Ability_Animation_Pistol_CrouchingWalking,  FName("Ability.Animation.Pistol.CrouchingWalking"),  TEXT("Anim: Pistol crouching walking"));
    AddTag(GameplayTags.Ability_Animation_Pistol_AimingIdle,        FName("Ability.Animation.Pistol.AimingIdle"),        TEXT("Anim: Pistol aiming idle"));
    AddTag(GameplayTags.Ability_Animation_Pistol_AimingWalking,     FName("Ability.Animation.Pistol.AimingWalking"),     TEXT("Anim: Pistol aiming walking"));
    AddTag(GameplayTags.Ability_Animation_Pistol_Fire,              FName("Ability.Animation.Pistol.Fire"),              TEXT("Anim: Pistol firing"));
    AddTag(GameplayTags.Ability_Animation_Pistol_AimingFire,        FName("Ability.Animation.Pistol.AimingFire"),        TEXT("Anim: Pistol aiming fire"));
    AddTag(GameplayTags.Ability_Animation_Pistol_Reloading,         FName("Ability.Animation.Pistol.Reloading"),         TEXT("Anim: Pistol reloading"));
    AddTag(GameplayTags.Ability_Animation_Pistol_Jumping,           FName("Ability.Animation.Pistol.Jumping"),           TEXT("Anim: Pistol jumping"));
    AddTag(GameplayTags.Ability_Animation_Pistol_Falling,           FName("Ability.Animation.Pistol.Falling"),           TEXT("Anim: Pistol falling"));

    // =========================================================
    // ABILITY ANIMATION STATES (Rifle)
    // =========================================================
    AddTag(GameplayTags.Ability_Animation_Rifle_Idle,               FName("Ability.Animation.Rifle.Idle"),               TEXT("Anim: Rifle idle"));
    AddTag(GameplayTags.Ability_Animation_Rifle_Walking,            FName("Ability.Animation.Rifle.Walking"),            TEXT("Anim: Rifle walking"));
    AddTag(GameplayTags.Ability_Animation_Rifle_Running,            FName("Ability.Animation.Rifle.Running"),            TEXT("Anim: Rifle running"));
    AddTag(GameplayTags.Ability_Animation_Rifle_Sprinting,          FName("Ability.Animation.Rifle.Sprinting"),          TEXT("Anim: Rifle sprinting"));
    AddTag(GameplayTags.Ability_Animation_Rifle_CrouchingIdle,      FName("Ability.Animation.Rifle.CrouchingIdle"),      TEXT("Anim: Rifle crouching idle"));
    AddTag(GameplayTags.Ability_Animation_Rifle_CrouchingWalking,   FName("Ability.Animation.Rifle.CrouchingWalking"),   TEXT("Anim: Rifle crouching walking"));
    AddTag(GameplayTags.Ability_Animation_Rifle_AimingIdle,         FName("Ability.Animation.Rifle.AimingIdle"),         TEXT("Anim: Rifle aiming idle"));
    AddTag(GameplayTags.Ability_Animation_Rifle_AimingWalking,      FName("Ability.Animation.Rifle.AimingWalking"),      TEXT("Anim: Rifle aiming walking"));
    AddTag(GameplayTags.Ability_Animation_Rifle_Fire,               FName("Ability.Animation.Rifle.Fire"),               TEXT("Anim: Rifle firing"));
    AddTag(GameplayTags.Ability_Animation_Rifle_AimingFire,         FName("Ability.Animation.Rifle.AimingFire"),         TEXT("Anim: Rifle aiming fire"));
    AddTag(GameplayTags.Ability_Animation_Rifle_Reloading,          FName("Ability.Animation.Rifle.Reloading"),          TEXT("Anim: Rifle reloading"));
    AddTag(GameplayTags.Ability_Animation_Rifle_Jumping,            FName("Ability.Animation.Rifle.Jumping"),            TEXT("Anim: Rifle jumping"));
    AddTag(GameplayTags.Ability_Animation_Rifle_Falling,            FName("Ability.Animation.Rifle.Falling"),            TEXT("Anim: Rifle falling"));

    // =========================================================
    // ABILITY ANIMATION STATES (Melee)
    // =========================================================
    AddTag(GameplayTags.Ability_Animation_Melee_Idle,               FName("Ability.Animation.Melee.Idle"),               TEXT("Anim: Melee idle"));
    AddTag(GameplayTags.Ability_Animation_Melee_Walking,            FName("Ability.Animation.Melee.Walking"),            TEXT("Anim: Melee walking"));
    AddTag(GameplayTags.Ability_Animation_Melee_Running,            FName("Ability.Animation.Melee.Running"),            TEXT("Anim: Melee running"));
    AddTag(GameplayTags.Ability_Animation_Melee_Sprinting,          FName("Ability.Animation.Melee.Sprinting"),          TEXT("Anim: Melee sprinting"));
    AddTag(GameplayTags.Ability_Animation_Melee_CrouchingIdle,      FName("Ability.Animation.Melee.CrouchingIdle"),      TEXT("Anim: Melee crouching idle"));
    AddTag(GameplayTags.Ability_Animation_Melee_CrouchingWalking,   FName("Ability.Animation.Melee.CrouchingWalking"),   TEXT("Anim: Melee crouching walking"));
    AddTag(GameplayTags.Ability_Animation_Melee_Attack_Light,       FName("Ability.Animation.Melee.Attack.Light"),       TEXT("Anim: Melee light attack"));
    AddTag(GameplayTags.Ability_Animation_Melee_Attack_Heavy,       FName("Ability.Animation.Melee.Attack.Heavy"),       TEXT("Anim: Melee heavy attack"));
    AddTag(GameplayTags.Ability_Animation_Melee_Block,              FName("Ability.Animation.Melee.Block"),              TEXT("Anim: Melee blocking"));
    AddTag(GameplayTags.Ability_Animation_Melee_Jumping,            FName("Ability.Animation.Melee.Jumping"),            TEXT("Anim: Melee jumping"));
    AddTag(GameplayTags.Ability_Animation_Melee_Falling,            FName("Ability.Animation.Melee.Falling"),            TEXT("Anim: Melee falling"));

    // =========================================================
    // GENERAL ABILITY ANIMATION STATES
    // =========================================================
    AddTag(GameplayTags.Ability_Animation_Character_Stunned,        FName("Ability.Animation.Character.Stunned"),        TEXT("Anim: Character stunned"));
    AddTag(GameplayTags.Ability_Animation_Character_Dying,          FName("Ability.Animation.Character.Dying"),          TEXT("Anim: Character dying"));
    AddTag(GameplayTags.Ability_Animation_Character_Dead,           FName("Ability.Animation.Character.Dead"),           TEXT("Anim: Character dead"));
    AddTag(GameplayTags.Ability_Animation_Character_Interacting,    FName("Ability.Animation.Character.Interacting"),    TEXT("Anim: Character interacting"));

    // =========================================================
    // ANIMATION STATE (Redundant duplicate block in original code?)
    // =========================================================
    // Note: You have two blocks of animation tags in your original code (Ability.Animation.* and Animation.State.*)
    // I have kept both below, but you should check if you need both.

    // Unarmed
    AddTag(GameplayTags.Animation_State_Unarmed_Idle,               FName("Animation.State.Unarmed.Idle"),               TEXT("State: Unarmed idle"));
    AddTag(GameplayTags.Animation_State_Unarmed_Walking,            FName("Animation.State.Unarmed.Walking"),            TEXT("State: Unarmed walking"));
    AddTag(GameplayTags.Animation_State_Unarmed_Running,            FName("Animation.State.Unarmed.Running"),            TEXT("State: Unarmed running"));
    AddTag(GameplayTags.Animation_State_Unarmed_Sprinting,          FName("Animation.State.Unarmed.Sprinting"),          TEXT("State: Unarmed sprinting"));
    AddTag(GameplayTags.Animation_State_Unarmed_CrouchingIdle,      FName("Animation.State.Unarmed.CrouchingIdle"),      TEXT("State: Unarmed crouching idle"));
    AddTag(GameplayTags.Animation_State_Unarmed_CrouchingWalking,   FName("Animation.State.Unarmed.CrouchingWalking"),   TEXT("State: Unarmed crouching walking"));
    AddTag(GameplayTags.Animation_State_Unarmed_Attack_Light,       FName("Animation.State.Unarmed.Attack.Light"),       TEXT("State: Unarmed light attack"));
    AddTag(GameplayTags.Animation_State_Unarmed_Attack_Heavy,       FName("Animation.State.Unarmed.Attack.Heavy"),       TEXT("State: Unarmed heavy attack"));
    AddTag(GameplayTags.Animation_State_Unarmed_Jumping,            FName("Animation.State.Unarmed.Jumping"),            TEXT("State: Unarmed jumping"));
    AddTag(GameplayTags.Animation_State_Unarmed_StartJumping,       FName("Animation.State.Unarmed.StartJumping"),       TEXT("State: Start unarmed jumping"));
    AddTag(GameplayTags.Animation_State_Unarmed_EndJumping,         FName("Animation.State.Unarmed.EndJumping"),         TEXT("State: End unarmed jumping"));
    AddTag(GameplayTags.Animation_State_Unarmed_Falling,            FName("Animation.State.Unarmed.Falling"),            TEXT("State: Unarmed falling"));

    // Pistol
    AddTag(GameplayTags.Animation_State_Pistol_Idle,                FName("Animation.State.Pistol.Idle"),                TEXT("State: Pistol idle"));
    AddTag(GameplayTags.Animation_State_Pistol_Walking,             FName("Animation.State.Pistol.Walking"),             TEXT("State: Pistol walking"));
    AddTag(GameplayTags.Animation_State_Pistol_Running,             FName("Animation.State.Pistol.Running"),             TEXT("State: Pistol running"));
    AddTag(GameplayTags.Animation_State_Pistol_Sprinting,           FName("Animation.State.Pistol.Sprinting"),           TEXT("State: Pistol sprinting"));
    AddTag(GameplayTags.Animation_State_Pistol_CrouchingIdle,       FName("Animation.State.Pistol.CrouchingIdle"),       TEXT("State: Pistol crouching idle"));
    AddTag(GameplayTags.Animation_State_Pistol_CrouchingWalking,    FName("Animation.State.Pistol.CrouchingWalking"),    TEXT("State: Pistol crouching walking"));
    AddTag(GameplayTags.Animation_State_Pistol_AimingIdle,          FName("Animation.State.Pistol.AimingIdle"),          TEXT("State: Pistol aiming idle"));
    AddTag(GameplayTags.Animation_State_Pistol_AimingWalking,       FName("Animation.State.Pistol.AimingWalking"),       TEXT("State: Pistol aiming walking"));
    AddTag(GameplayTags.Animation_State_Pistol_Fire,                FName("Animation.State.Pistol.Fire"),                TEXT("State: Pistol firing"));
    AddTag(GameplayTags.Animation_State_Pistol_AimingFire,          FName("Animation.State.Pistol.AimingFire"),          TEXT("State: Pistol aiming fire"));
    AddTag(GameplayTags.Animation_State_Pistol_Reloading,           FName("Animation.State.Pistol.Reloading"),           TEXT("State: Pistol reloading"));
    AddTag(GameplayTags.Animation_State_Pistol_Jumping,             FName("Animation.State.Pistol.Jumping"),             TEXT("State: Pistol jumping"));
    AddTag(GameplayTags.Animation_State_Pistol_Falling,             FName("Animation.State.Pistol.Falling"),             TEXT("State: Pistol falling"));

    // Rifle
    AddTag(GameplayTags.Animation_State_Rifle_Idle,                 FName("Animation.State.Rifle.Idle"),                 TEXT("State: Rifle idle"));
    AddTag(GameplayTags.Animation_State_Rifle_Walking,              FName("Animation.State.Rifle.Walking"),              TEXT("State: Rifle walking"));
    AddTag(GameplayTags.Animation_State_Rifle_Running,              FName("Animation.State.Rifle.Running"),              TEXT("State: Rifle running"));
    AddTag(GameplayTags.Animation_State_Rifle_Sprinting,            FName("Animation.State.Rifle.Sprinting"),            TEXT("State: Rifle sprinting"));
    AddTag(GameplayTags.Animation_State_Rifle_CrouchingIdle,        FName("Animation.State.Rifle.CrouchingIdle"),        TEXT("State: Rifle crouching idle"));
    AddTag(GameplayTags.Animation_State_Rifle_CrouchingWalking,     FName("Animation.State.Rifle.CrouchingWalking"),     TEXT("State: Rifle crouching walking"));
    AddTag(GameplayTags.Animation_State_Rifle_AimingIdle,           FName("Animation.State.Rifle.AimingIdle"),           TEXT("State: Rifle aiming idle"));
    AddTag(GameplayTags.Animation_State_Rifle_AimingWalking,        FName("Animation.State.Rifle.AimingWalking"),        TEXT("State: Rifle aiming walking"));
    AddTag(GameplayTags.Animation_State_Rifle_Fire,                 FName("Animation.State.Rifle.Fire"),                 TEXT("State: Rifle firing"));
    AddTag(GameplayTags.Animation_State_Rifle_AimingFire,           FName("Animation.State.Rifle.AimingFire"),           TEXT("State: Rifle aiming fire"));
    AddTag(GameplayTags.Animation_State_Rifle_Reloading,            FName("Animation.State.Rifle.Reloading"),            TEXT("State: Rifle reloading"));
    AddTag(GameplayTags.Animation_State_Rifle_Jumping,              FName("Animation.State.Rifle.Jumping"),              TEXT("State: Rifle jumping"));
    AddTag(GameplayTags.Animation_State_Rifle_Falling,              FName("Animation.State.Rifle.Falling"),              TEXT("State: Rifle falling"));

    // Melee
    AddTag(GameplayTags.Animation_State_Melee_Idle,                 FName("Animation.State.Melee.Idle"),                 TEXT("State: Melee idle"));
    AddTag(GameplayTags.Animation_State_Melee_Walking,              FName("Animation.State.Melee.Walking"),              TEXT("State: Melee walking"));
    AddTag(GameplayTags.Animation_State_Melee_Running,              FName("Animation.State.Melee.Running"),              TEXT("State: Melee running"));
    AddTag(GameplayTags.Animation_State_Melee_Sprinting,            FName("Animation.State.Melee.Sprinting"),            TEXT("State: Melee sprinting"));
    AddTag(GameplayTags.Animation_State_Melee_CrouchingIdle,        FName("Animation.State.Melee.CrouchingIdle"),        TEXT("State: Melee crouching idle"));
    AddTag(GameplayTags.Animation_State_Melee_CrouchingWalking,     FName("Animation.State.Melee.CrouchingWalking"),     TEXT("State: Melee crouching walking"));
    AddTag(GameplayTags.Animation_State_Melee_Attack_Light,         FName("Animation.State.Melee.Attack.Light"),         TEXT("State: Melee light attack"));
    AddTag(GameplayTags.Animation_State_Melee_Attack_Heavy,         FName("Animation.State.Melee.Attack.Heavy"),         TEXT("State: Melee heavy attack"));
    AddTag(GameplayTags.Animation_State_Melee_Block,                FName("Animation.State.Melee.Block"),                TEXT("State: Melee blocking"));
    AddTag(GameplayTags.Animation_State_Melee_Jumping,              FName("Animation.State.Melee.Jumping"),              TEXT("State: Melee jumping"));
    AddTag(GameplayTags.Animation_State_Melee_Falling,              FName("Animation.State.Melee.Falling"),              TEXT("State: Melee falling"));

    // General Character
    AddTag(GameplayTags.Animation_State_Character_Stunned,          FName("Animation.State.Character.Stunned"),          TEXT("State: Character stunned"));
    AddTag(GameplayTags.Animation_State_Character_Dying,            FName("Animation.State.Character.Dying"),            TEXT("State: Character dying"));
    AddTag(GameplayTags.Animation_State_Character_Dead,             FName("Animation.State.Character.Dead"),             TEXT("State: Character dead"));
    AddTag(GameplayTags.Animation_State_Character_Interacting,      FName("Animation.State.Character.Interacting"),      TEXT("State: Character interacting"));

    // =========================================================
    // INVENTORY: ITEM TYPES
    // =========================================================
    AddTag(GameplayTags.Item_Type_Weapon_Pistol,         FName("Item.Type.Weapon.Pistol"),         TEXT("Standard sidearm, moderate damage."));
    AddTag(GameplayTags.Item_Type_Weapon_Rifle,          FName("Item.Type.Weapon.Rifle"),          TEXT("High-accuracy ranged weapon."));
    AddTag(GameplayTags.Item_Type_Weapon_Shotgun,        FName("Item.Type.Weapon.Shotgun"),        TEXT("Close-range, high spread."));
    AddTag(GameplayTags.Item_Type_Weapon_SMG,            FName("Item.Type.Weapon.SMG"),            TEXT("High rate of fire, low damage."));
    AddTag(GameplayTags.Item_Type_Weapon_RocketLauncher, FName("Item.Type.Weapon.RocketLauncher"), TEXT("Heavy weapon with explosive ammo."));
    AddTag(GameplayTags.Item_Type_Weapon_Melee,          FName("Item.Type.Weapon.Melee"),          TEXT("Hand-to-hand weapon."));
    AddTag(GameplayTags.Item_Type_Weapon_Tool,           FName("Item.Type.Weapon.Tool"),           TEXT("Dual-purpose tools (crowbar, axe)."));

    AddTag(GameplayTags.Item_Type_Ammo_Pistol,           FName("Item.Type.Ammo.Pistol"),           TEXT("Standard pistol rounds."));
    AddTag(GameplayTags.Item_Type_Ammo_Rifle,            FName("Item.Type.Ammo.Rifle"),            TEXT("Rifle cartridges."));
    AddTag(GameplayTags.Item_Type_Ammo_Shotgun,          FName("Item.Type.Ammo.Shotgun"),          TEXT("Shotgun shells."));
    AddTag(GameplayTags.Item_Type_Ammo_SMG,              FName("Item.Type.Ammo.SMG"),              TEXT("Submachine gun ammunition."));
    AddTag(GameplayTags.Item_Type_Ammo_Rocket,           FName("Item.Type.Ammo.Rocket"),           TEXT("Rocket or grenade launcher ammo."));
    AddTag(GameplayTags.Item_Type_Ammo_Special,          FName("Item.Type.Ammo.Special"),          TEXT("Rare or elemental ammo."));

    AddTag(GameplayTags.Item_Type_Armor_Head,            FName("Item.Type.Armor.Head"),            TEXT("Helmet, mask, or head protection."));
    AddTag(GameplayTags.Item_Type_Armor_Chest,           FName("Item.Type.Armor.Chest"),           TEXT("Body armor or tactical vest."));
    AddTag(GameplayTags.Item_Type_Armor_Legs,            FName("Item.Type.Armor.Legs"),            TEXT("Leg protection gear."));
    AddTag(GameplayTags.Item_Type_Armor_Hands,           FName("Item.Type.Armor.Hands"),           TEXT("Gloves or protective handwear."));
    AddTag(GameplayTags.Item_Type_Armor_Feet,            FName("Item.Type.Armor.Feet"),            TEXT("Boots or foot armor."));
    AddTag(GameplayTags.Item_Type_Armor_Back,            FName("Item.Type.Armor.Back"),            TEXT("Backpacks or back-mounted protection."));
    AddTag(GameplayTags.Item_Type_Armor_FullBody,        FName("Item.Type.Armor.FullBody"),        TEXT("Complete suit (hazmat, etc)."));

    AddTag(GameplayTags.Item_Type_Accessory_Amulet,      FName("Item.Type.Accessory.Amulet"),      TEXT("Occult or protective neck item."));
    AddTag(GameplayTags.Item_Type_Accessory_Ring,        FName("Item.Type.Accessory.Ring"),        TEXT("Magical or status-enhancing ring."));
    AddTag(GameplayTags.Item_Type_Accessory_Belt,        FName("Item.Type.Accessory.Belt"),        TEXT("Utility belt or equipment modifier."));
    AddTag(GameplayTags.Item_Type_Accessory_Charm,       FName("Item.Type.Accessory.Charm"),       TEXT("Small item granting passive bonus."));
    AddTag(GameplayTags.Item_Type_Accessory_Cloak,       FName("Item.Type.Accessory.Cloak"),       TEXT("Magical or status-enhancing cloak."));
    AddTag(GameplayTags.Item_Type_Accessory_Mask,        FName("Item.Type.Accessory.Mask"),        TEXT("Magical or status-enhancing mask."));

    AddTag(GameplayTags.Item_Type_Consumable_Health,     FName("Item.Type.Consumable.Health"),     TEXT("Heals health (medkit, bandage)."));
    AddTag(GameplayTags.Item_Type_Consumable_Buff,       FName("Item.Type.Consumable.Buff"),       TEXT("Temporary stat boost."));
    AddTag(GameplayTags.Item_Type_Consumable_Throwable,  FName("Item.Type.Consumable.Throwable"),  TEXT("Usable item that can be thrown."));
    AddTag(GameplayTags.Item_Type_Consumable_Antidote,   FName("Item.Type.Consumable.Antidote"),   TEXT("Cures infection, toxin, etc."));
    AddTag(GameplayTags.Item_Type_Consumable_Food,       FName("Item.Type.Consumable.Food"),       TEXT("Restores partial health or stamina."));
    AddTag(GameplayTags.Item_Type_Consumable_Psych,      FName("Item.Type.Consumable.Psych"),      TEXT("Stabilizes sanity or reduces fear."));

    AddTag(GameplayTags.Item_Type_Attachment_Scope,      FName("Item.Type.Attachment.Scope"),      TEXT("Increases zoom or accuracy."));
    AddTag(GameplayTags.Item_Type_Attachment_Magazine,   FName("Item.Type.Attachment.Magazine"),   TEXT("Expands ammo capacity."));
    AddTag(GameplayTags.Item_Type_Attachment_Muzzle,     FName("Item.Type.Attachment.Muzzle"),     TEXT("Reduces recoil or suppresses sound."));
    AddTag(GameplayTags.Item_Type_Attachment_Grip,       FName("Item.Type.Attachment.Grip"),       TEXT("Improves handling or accuracy."));
    AddTag(GameplayTags.Item_Type_Attachment_Stock,      FName("Item.Type.Attachment.Stock"),      TEXT("Improves stability."));
    AddTag(GameplayTags.Item_Type_Attachment_Laser,      FName("Item.Type.Attachment.Laser"),      TEXT("Adds aiming laser or tracking."));
    AddTag(GameplayTags.Item_Type_Attachment_Light,      FName("Item.Type.Attachment.Light"),      TEXT("Adds flashlight or tactical light."));

    AddTag(GameplayTags.Item_Type_Material_Scrap,        FName("Item.Type.Material.Scrap"),        TEXT("Basic crafting metal and parts."));
    AddTag(GameplayTags.Item_Type_Material_Electronics,  FName("Item.Type.Material.Electronics"),  TEXT("Circuit boards and tech."));
    AddTag(GameplayTags.Item_Type_Material_Chemicals,    FName("Item.Type.Material.Chemicals"),    TEXT("Chemical reagents."));
    AddTag(GameplayTags.Item_Type_Material_Biological,   FName("Item.Type.Material.Biological"),   TEXT("Organic samples, tissue, spores."));
    AddTag(GameplayTags.Item_Type_Material_Powder,       FName("Item.Type.Material.Powder"),       TEXT("Gunpowder or powdered reagent."));
    AddTag(GameplayTags.Item_Type_Material_Fluid,        FName("Item.Type.Material.Fluid"),        TEXT("Solvent, fuel, acid, or oil."));

    AddTag(GameplayTags.Item_Type_Key,                   FName("Item.Type.Key"),                   TEXT("Traditional key."));
    AddTag(GameplayTags.Item_Type_KeyCard,               FName("Item.Type.KeyCard"),               TEXT("Electronic access card."));
    AddTag(GameplayTags.Item_Type_Document,              FName("Item.Type.Document"),              TEXT("Notes, journals, or letters."));
    AddTag(GameplayTags.Item_Type_PuzzleItem,            FName("Item.Type.PuzzleItem"),            TEXT("Artifacts for puzzles."));
    AddTag(GameplayTags.Item_Type_QuestItem,             FName("Item.Type.QuestItem"),             TEXT("Core item required for storyline."));

    AddTag(GameplayTags.Item_Type_Currency,              FName("Item.Type.Currency"),              TEXT("Money or tradable credits."));
    AddTag(GameplayTags.Item_Type_Junk,                  FName("Item.Type.Junk"),                  TEXT("Sellable trash."));
    AddTag(GameplayTags.Item_Type_Valuable,              FName("Item.Type.Valuable"),              TEXT("High-value items."));

    // =========================================================
    // INVENTORY: ITEM PROPERTIES & RARITY
    // =========================================================
    AddTag(GameplayTags.Item_Property_Stackable,  FName("Item.Property.Stackable"),  TEXT("Can stack multiple units."));
    AddTag(GameplayTags.Item_Property_Equippable, FName("Item.Property.Equippable"), TEXT("Can be equipped."));
    AddTag(GameplayTags.Item_Property_Consumable, FName("Item.Property.Consumable"), TEXT("Consumed on use."));
    AddTag(GameplayTags.Item_Property_Droppable,  FName("Item.Property.Droppable"),  TEXT("Can be dropped."));
    AddTag(GameplayTags.Item_Property_QuestItem,  FName("Item.Property.QuestItem"),  TEXT("Cannot be dropped/sold."));
    AddTag(GameplayTags.Item_Property_Tradeable,  FName("Item.Property.Tradeable"),  TEXT("Can be sold/exchanged."));
    AddTag(GameplayTags.Item_Property_Craftable,  FName("Item.Property.Craftable"),  TEXT("Used in crafting."));
    AddTag(GameplayTags.Item_Property_Breakable,  FName("Item.Property.Breakable"),  TEXT("Has durability."));
    AddTag(GameplayTags.Item_Property_Cursed,     FName("Item.Property.Cursed"),     TEXT("Has negative side effects."));
    AddTag(GameplayTags.Item_Property_Hidden,     FName("Item.Property.Hidden"),     TEXT("Hidden until revealed."));
    AddTag(GameplayTags.Item_Property_LimitedUse, FName("Item.Property.LimitedUse"), TEXT("Limited uses/charges."));
    AddTag(GameplayTags.Item_Property_Fragile,    FName("Item.Property.Fragile"),    TEXT("Easily breaks."));

    AddTag(GameplayTags.Item_Rarity_Common,       FName("Item.Rarity.Common"),       TEXT("Everyday item."));
    AddTag(GameplayTags.Item_Rarity_Uncommon,     FName("Item.Rarity.Uncommon"),     TEXT("Slightly better/rarer."));
    AddTag(GameplayTags.Item_Rarity_Rare,         FName("Item.Rarity.Rare"),         TEXT("Hard-to-find item."));
    AddTag(GameplayTags.Item_Rarity_Epic,         FName("Item.Rarity.Epic"),         TEXT("Unique/Story item."));
    AddTag(GameplayTags.Item_Rarity_Legendary,    FName("Item.Rarity.Legendary"),    TEXT("Iconic, extremely rare."));
    AddTag(GameplayTags.Item_Rarity_Prototype,    FName("Item.Rarity.Prototype"),    TEXT("Experimental/Prototype."));
    AddTag(GameplayTags.Item_Rarity_Cursed,       FName("Item.Rarity.Cursed"),       TEXT("Strong but dangerous."));
    AddTag(GameplayTags.Item_Rarity_Relic,        FName("Item.Rarity.Relic"),        TEXT("Ancient/Occult artifact."));

    // =========================================================
    // ITEM EFFECTS
    // =========================================================
    AddTag(GameplayTags.Item_Effect_FearReduction, FName("Item.Effect.FearReduction"), TEXT("Reduces fear/panic."));
    AddTag(GameplayTags.Item_Effect_SanityBoost,   FName("Item.Effect.SanityBoost"),   TEXT("Restores mental stability."));
    AddTag(GameplayTags.Item_Effect_NoiseMaker,    FName("Item.Effect.NoiseMaker"),    TEXT("Attracts enemies."));
    AddTag(GameplayTags.Item_Effect_LightSource,   FName("Item.Effect.LightSource"),   TEXT("Emits light."));
    AddTag(GameplayTags.Item_Effect_Holy,          FName("Item.Effect.Holy"),          TEXT("Repels supernatural entities."));
    AddTag(GameplayTags.Item_Effect_Tainted,       FName("Item.Effect.Tainted"),       TEXT("Attracts enemies or corrupts."));

    // =========================================================
    // EQUIPMENT STATES
    // =========================================================
    AddTag(GameplayTags.State_Equipped_Weapon_Primary,        FName("State.Equipped.Weapon.Primary"),        TEXT("State: Primary weapon equipped"));
    AddTag(GameplayTags.State_Equipped_Weapon_Secondary,      FName("State.Equipped.Weapon.Secondary"),      TEXT("State: Secondary weapon equipped"));
    AddTag(GameplayTags.State_Equipped_Weapon_Sidearm,        FName("State.Equipped.Weapon.Sidearm"),        TEXT("State: Sidearm weapon equipped"));
    AddTag(GameplayTags.State_Equipped_Weapon_Melee,          FName("State.Equipped.Weapon.Melee"),          TEXT("State: Melee weapon equipped"));
    AddTag(GameplayTags.State_Equipped_Armor_Head,            FName("State.Equipped.Armor.Head"),            TEXT("State: Head armor equipped"));
    AddTag(GameplayTags.State_Equipped_Armor_Chest,           FName("State.Equipped.Armor.Chest"),           TEXT("State: Chest armor equipped"));
    AddTag(GameplayTags.State_Equipped_Armor_Legs,            FName("State.Equipped.Armor.Legs"),            TEXT("State: Legs armor equipped"));
    AddTag(GameplayTags.State_Equipped_Armor_Hands,           FName("State.Equipped.Armor.Hands"),           TEXT("State: Hands armor equipped"));
    AddTag(GameplayTags.State_Equipped_Armor_Feet,            FName("State.Equipped.Armor.Feet"),            TEXT("State: Feet armor equipped"));
    AddTag(GameplayTags.State_Equipped_Consumable_QuickSlot1, FName("State.Equipped.Consumable.QuickSlot1"), TEXT("State: QuickSlot1 consumable equipped"));

    // =========================================================
    // EQUIPMENT SLOTS
    // =========================================================
    AddTag(GameplayTags.Equipment_Slot_PrimaryWeapon,   FName("Equipment.Slot.PrimaryWeapon"),   TEXT("Slot: Primary weapon"));
    AddTag(GameplayTags.Equipment_Slot_SecondaryWeapon, FName("Equipment.Slot.SecondaryWeapon"), TEXT("Slot: Secondary weapon"));
    AddTag(GameplayTags.Equipment_Slot_Sidearm,         FName("Equipment.Slot.Sidearm"),         TEXT("Slot: Sidearm weapon"));
    AddTag(GameplayTags.Equipment_Slot_Melee,           FName("Equipment.Slot.Melee"),           TEXT("Slot: Melee weapon"));

    AddTag(GameplayTags.Equipment_Slot_Head,            FName("Equipment.Slot.Head"),            TEXT("Slot: Head armor"));
    AddTag(GameplayTags.Equipment_Slot_Chest,           FName("Equipment.Slot.Chest"),           TEXT("Slot: Chest armor"));
    AddTag(GameplayTags.Equipment_Slot_Legs,            FName("Equipment.Slot.Legs"),            TEXT("Slot: Legs armor"));
    AddTag(GameplayTags.Equipment_Slot_Hands,           FName("Equipment.Slot.Hands"),           TEXT("Slot: Hands armor"));
    AddTag(GameplayTags.Equipment_Slot_Feet,            FName("Equipment.Slot.Feet"),            TEXT("Slot: Feet armor"));
    AddTag(GameplayTags.Equipment_Slot_Back,            FName("Equipment.Slot.Back"),            TEXT("Slot: Back (cape/backpack)"));

    AddTag(GameplayTags.Equipment_Slot_Amulet,          FName("Equipment.Slot.Amulet"),          TEXT("Slot: Amulet"));
    AddTag(GameplayTags.Equipment_Slot_Ring1,           FName("Equipment.Slot.Ring1"),           TEXT("Slot: Ring 1"));
    AddTag(GameplayTags.Equipment_Slot_Ring2,           FName("Equipment.Slot.Ring2"),           TEXT("Slot: Ring 2"));
    AddTag(GameplayTags.Equipment_Slot_Belt,            FName("Equipment.Slot.Belt"),            TEXT("Slot: Belt"));

    AddTag(GameplayTags.Equipment_Slot_QuickSlot1,      FName("Equipment.Slot.QuickSlot1"),      TEXT("Slot: Quick Slot 1"));
    AddTag(GameplayTags.Equipment_Slot_QuickSlot2,      FName("Equipment.Slot.QuickSlot2"),      TEXT("Slot: Quick Slot 2"));
    AddTag(GameplayTags.Equipment_Slot_QuickSlot3,      FName("Equipment.Slot.QuickSlot3"),      TEXT("Slot: Quick Slot 3"));
    AddTag(GameplayTags.Equipment_Slot_QuickSlot4,      FName("Equipment.Slot.QuickSlot4"),      TEXT("Slot: Quick Slot 4"));

    // =========================================================
    // ITEM FRAGMENTS
    // =========================================================
    AddTag(GameplayTags.Item_Fragment_Grid,             FName("Item.Fragment.Grid"),             TEXT("Fragment: Grid-based spatial inventory"));
    AddTag(GameplayTags.Item_Fragment_Stats,            FName("Item.Fragment.Stats"),            TEXT("Fragment: Item statistics"));
    AddTag(GameplayTags.Item_Fragment_Stats_Mode_1,     FName("Item.Fragment.Stats.Mode.1"),     TEXT("Fragment: Stats Mode 1"));
    AddTag(GameplayTags.Item_Fragment_Stats_Mode_2,     FName("Item.Fragment.Stats.Mode.2"),     TEXT("Fragment: Stats Mode 2"));
    AddTag(GameplayTags.Item_Fragment_Stats_Mode_3,     FName("Item.Fragment.Stats.Mode.3"),     TEXT("Fragment: Stats Mode 3"));
    AddTag(GameplayTags.Item_Fragment_Stats_Mode_4,     FName("Item.Fragment.Stats.Mode.4"),     TEXT("Fragment: Stats Mode 4"));
    AddTag(GameplayTags.Item_Fragment_Stats_Mode_5,     FName("Item.Fragment.Stats.Mode.5"),     TEXT("Fragment: Stats Mode 5"));
    AddTag(GameplayTags.Item_Fragment_PrimaryStats,     FName("Item.Fragment.PrimaryStats"),     TEXT("Fragment: Primary Stats"));
    AddTag(GameplayTags.Item_Fragment_SecondaryStats,   FName("Item.Fragment.SecondaryStats"),   TEXT("Fragment: Secondary Stats"));
    AddTag(GameplayTags.Item_Fragment_Visual,           FName("Item.Fragment.Visual"),           TEXT("Fragment: Visual/Display"));
    AddTag(GameplayTags.Item_Fragment_Icon,             FName("Item.Fragment.Icon"),             TEXT("Fragment: Icon"));
    AddTag(GameplayTags.Item_Fragment_Name,             FName("Item.Fragment.Name"),             TEXT("Fragment: Name"));
    AddTag(GameplayTags.Item_Fragment_Type,             FName("Item.Fragment.Type"),             TEXT("Fragment: Type"));
    AddTag(GameplayTags.Item_Fragment_Text,             FName("Item.Fragment.Text"),             TEXT("Fragment: Text"));
    AddTag(GameplayTags.Item_Fragment_Value,            FName("Item.Fragment.Value"),            TEXT("Fragment: Value"));
    AddTag(GameplayTags.Item_Fragment_Level,            FName("Item.Fragment.Level"),            TEXT("Fragment: Level"));
    AddTag(GameplayTags.Item_Fragment_Stackable,        FName("Item.Fragment.Stackable"),        TEXT("Fragment: Stackable"));
    AddTag(GameplayTags.Item_Fragment_Consumable,       FName("Item.Fragment.Consumable"),       TEXT("Fragment: Consumable"));
    AddTag(GameplayTags.Item_Fragment_Equipment,        FName("Item.Fragment.Equipment"),        TEXT("Fragment: Equipment properties"));
    AddTag(GameplayTags.Item_Fragment_Durability,       FName("Item.Fragment.Durability"),       TEXT("Fragment: Durability/Condition"));
    AddTag(GameplayTags.Item_Fragment_Modification,     FName("Item.Fragment.Modification"),     TEXT("Fragment: Modifications"));
    AddTag(GameplayTags.Item_Fragment_Metadata,         FName("Item.Fragment.Metadata"),         TEXT("Fragment: Metadata"));

    // Weapon detail tags (for item description UI)
    AddTag(GameplayTags.Item_Fragment_Description,     FName("Item.Fragment.Description"),     TEXT("Fragment: Description subtitle"));
    AddTag(GameplayTags.Item_Fragment_MagazineSize,    FName("Item.Fragment.MagazineSize"),    TEXT("Fragment: Magazine size"));
    AddTag(GameplayTags.Item_Fragment_Ammo,            FName("Item.Fragment.Ammo"),            TEXT("Fragment: Ammo count"));
    AddTag(GameplayTags.Item_Fragment_Ammo_Diff,       FName("Item.Fragment.Ammo.Diff"),       TEXT("Fragment: Ammo comparison diff"));
    AddTag(GameplayTags.Item_Fragment_Damage,          FName("Item.Fragment.Damage"),          TEXT("Fragment: Damage value"));
    AddTag(GameplayTags.Item_Fragment_Damage_Diff,     FName("Item.Fragment.Damage.Diff"),     TEXT("Fragment: Damage comparison diff"));
    AddTag(GameplayTags.Item_Fragment_Range,           FName("Item.Fragment.Range"),           TEXT("Fragment: Range value"));
    AddTag(GameplayTags.Item_Fragment_Range_Diff,      FName("Item.Fragment.Range.Diff"),      TEXT("Fragment: Range comparison diff"));
    AddTag(GameplayTags.Item_Fragment_Reload,          FName("Item.Fragment.Reload"),          TEXT("Fragment: Reload speed"));
    AddTag(GameplayTags.Item_Fragment_Reload_Diff,     FName("Item.Fragment.Reload.Diff"),     TEXT("Fragment: Reload comparison diff"));
    AddTag(GameplayTags.Item_Fragment_Condition,       FName("Item.Fragment.Condition"),       TEXT("Fragment: Condition percentage"));
    AddTag(GameplayTags.Item_Fragment_Accuracy,        FName("Item.Fragment.Accuracy"),        TEXT("Fragment: Accuracy rating"));
    AddTag(GameplayTags.Item_Fragment_Recoil,          FName("Item.Fragment.Recoil"),          TEXT("Fragment: Recoil rating"));
    AddTag(GameplayTags.Item_Fragment_Weight,          FName("Item.Fragment.Weight"),          TEXT("Fragment: Weight"));
}