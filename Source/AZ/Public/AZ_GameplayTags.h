#pragma once
#include <GameplayTagContainer.h>

/**
 * */
struct AZ_API FAZ_GameplayTags
{
    static const FAZ_GameplayTags& Get() { return GameplayTags; }
    static void InitializeNativeGameplayTags();

    // Primary Attributes 
    FGameplayTag Attributes_Primary_Strength;
    FGameplayTag Attributes_Primary_Agility;       // Assuming Agility from previous discussion
    FGameplayTag Attributes_Primary_Resilience;
    FGameplayTag Attributes_Primary_Intelligence;
    FGameplayTag Attributes_Primary_Expertise;

    // Secondary Attributes - Combat
    FGameplayTag Attributes_Secondary_Combat_MaxHealth;
    FGameplayTag Attributes_Secondary_Combat_HealthRegenRate;
    FGameplayTag Attributes_Secondary_Combat_MeleeDamage;
    FGameplayTag Attributes_Secondary_Combat_RangedDamage;
    FGameplayTag Attributes_Secondary_Combat_Accuracy;
    FGameplayTag Attributes_Secondary_Combat_ReloadSpeed;
    FGameplayTag Attributes_Secondary_Combat_RecoilControl;
    FGameplayTag Attributes_Secondary_Combat_CriticalHitChance;
    FGameplayTag Attributes_Secondary_Combat_CriticalHitDamage;
    FGameplayTag Attributes_Secondary_Combat_AttackSpeed;
    FGameplayTag Attributes_Secondary_Combat_Armor;
    FGameplayTag Attributes_Secondary_Combat_PhysicalResistance;
    FGameplayTag Attributes_Secondary_Combat_ExplosiveResistance;
    FGameplayTag Attributes_Secondary_Combat_ElementalDamage;         // ADDED
    FGameplayTag Attributes_Secondary_Combat_ArmorPenetration;       // ADDED
    FGameplayTag Attributes_Secondary_Combat_KnockbackChance;        // ADDED
    FGameplayTag Attributes_Secondary_Combat_HeavyWeaponHandling;    // ADDED
    FGameplayTag Attributes_Secondary_Combat_EvasionChance;          // ADDED
    FGameplayTag Attributes_Secondary_Combat_StaggerResistance;       // ADDED
    FGameplayTag Attributes_Secondary_Combat_WeakSpotDetection;       // ADDED
    FGameplayTag Attributes_Secondary_Combat_StatusEffectDuration;   // ADDED (duration of effects player inflicts)


    // Secondary Attributes - Survival
    FGameplayTag Attributes_Secondary_Survival_MaxStamina;
    FGameplayTag Attributes_Secondary_Survival_StaminaRegenRate;
    FGameplayTag Attributes_Secondary_Survival_CarryWeight;
    FGameplayTag Attributes_Secondary_Survival_MovementSpeed;
    FGameplayTag Attributes_Secondary_Survival_DiseaseResistance;
    FGameplayTag Attributes_Secondary_Survival_InfectionResistance;
    FGameplayTag Attributes_Secondary_Survival_PoisonResistance;
    FGameplayTag Attributes_Secondary_Survival_StatusEffectResistance; // ADDED (general resistance to effects on player)
    FGameplayTag Attributes_Secondary_Survival_HungerRate;
    FGameplayTag Attributes_Secondary_Survival_ThirstRate;

    // Secondary Attributes - Utility
    FGameplayTag Attributes_Secondary_Utility_Perception;
    FGameplayTag Attributes_Secondary_Utility_CraftingSpeed;
    FGameplayTag Attributes_Secondary_Utility_CraftingQuality;
    FGameplayTag Attributes_Secondary_Utility_RepairEfficiency;
    FGameplayTag Attributes_Secondary_Utility_HackingSpeed;
    FGameplayTag Attributes_Secondary_Utility_Lockpicking;
    FGameplayTag Attributes_Secondary_Utility_Stealth;
    FGameplayTag Attributes_Secondary_Utility_Barter;
    FGameplayTag Attributes_Secondary_Utility_ExperienceGainRate;
    FGameplayTag Attributes_Secondary_Utility_InteractionStrength;    // ADDED
    FGameplayTag Attributes_Secondary_Utility_CooldownReduction;      // ADDED (for abilities/gadgets)
    FGameplayTag Attributes_Secondary_Utility_ResourcePool;           // ADDED (e.g., Focus/Energy Max)
    FGameplayTag Attributes_Secondary_Utility_ResourceRegenRate;     // ADDED (e.g., Focus/Energy Regen)
    FGameplayTag Attributes_Secondary_Utility_HealingEffectiveness;   // ADDED

    // Vital Attributes
    FGameplayTag Attributes_Vital_CurrentHealth;
    FGameplayTag Attributes_Vital_CurrentStamina;
    // Consider adding FGameplayTag Attributes_Vital_CurrentResource; if you have a ResourcePool

    // Tags for Humanoid Infected Attributes
    FGameplayTag Infected_Humanoid_Health;
    FGameplayTag Infected_Humanoid_MaxHealth;
    FGameplayTag Infected_Humanoid_MovementSpeed;
    FGameplayTag Infected_Humanoid_Perception;
    FGameplayTag Infected_Humanoid_Aggression;
    FGameplayTag Infected_Humanoid_Armor;
    FGameplayTag Infected_Humanoid_MeleeDamage;
    FGameplayTag Infected_Humanoid_RangedDamage;    // If applicable
    FGameplayTag Infected_Humanoid_AttackSpeed;
    FGameplayTag Infected_Humanoid_AttackRange;
    FGameplayTag Infected_Humanoid_Accuracy;        // If applicable
    FGameplayTag Infected_Humanoid_Resistance;      // General or specific
    
    // Tags for Human Antagonist Attributes
    FGameplayTag Antagonist_Human_Health;
    FGameplayTag Antagonist_Human_MaxHealth;
    FGameplayTag Antagonist_Human_MovementSpeed;
    FGameplayTag Antagonist_Human_Perception;
    FGameplayTag Antagonist_Human_Aggression;
    FGameplayTag Antagonist_Human_Armor;
    FGameplayTag Antagonist_Human_MeleeDamage;
    FGameplayTag Antagonist_Human_RangedDamage;
    FGameplayTag Antagonist_Human_Accuracy;
    FGameplayTag Antagonist_Human_AttackSpeed;
    FGameplayTag Antagonist_Human_AttackRange;
    FGameplayTag Antagonist_Human_Morale;

    // Tags for Animal Infected Attributes
    FGameplayTag Infected_Animal_Health;
    FGameplayTag Infected_Animal_MaxHealth;
    FGameplayTag Infected_Animal_MovementSpeed;
    FGameplayTag Infected_Animal_Perception;
    FGameplayTag Infected_Animal_Aggression;
    FGameplayTag Infected_Animal_Damage;           // General or specific (e.g., MeleeDamage)
    FGameplayTag Infected_Animal_AttackSpeed;
    FGameplayTag Infected_Animal_AttackRange;
    FGameplayTag Infected_Animal_Disease;          // Or Resistance if it's about player resisting them

    // Tags for Boss Attributes
    FGameplayTag Boss_ChargeDamage;
    FGameplayTag Boss_GroundSlamRadius;
    FGameplayTag Boss_ToxicCloudDuration;
    FGameplayTag Boss_PsionicPower;
    FGameplayTag Boss_SummoningCooldown;
    FGameplayTag Boss_MutationLevel;

    // ... other Gameplay Tags ...
    FGameplayTag GameplayEffect_Input_Magnitude; // This seems like a generic tag for effect magnitude
    FGameplayTag GameplayEffect_CharacterInitTag; 

    // Input Actions (Replaces "Input Attributes")
    FGameplayTag Input_Action_None; // For no specific input, or default

    // --- Core Combat Actions ---
    FGameplayTag Input_Action_PrimaryAttack;    // e.g., Fire weapon, main melee swing
    FGameplayTag Input_Action_SecondaryAttack;   // e.g., Alt-fire, heavy melee attack, block
    FGameplayTag Input_Action_Aim;             // Aim down sights or focus
    FGameplayTag Input_Action_Reload;          // (You had this, it's good)
    FGameplayTag Input_Action_MeleeAttack;     // If a dedicated melee button distinct from primary/secondary
    FGameplayTag Input_Action_ThrowGrenade;    // Or use tactical equipment

    // --- Movement Actions ---
    FGameplayTag Input_Action_Jump;            // Replaces InputTag_SpaceBar
    FGameplayTag Input_Action_Crouch;
    FGameplayTag Input_Action_ToggleCrouch;   // If you have both hold and toggle
	FGameplayTag Input_Action_Run;          // Hold or toggle
    FGameplayTag Input_Action_Sprint;          // Hold or toggle
    FGameplayTag Input_Action_Dodge;           // If applicable
    FGameplayTag Input_Action_Move;           // If applicable

    // --- Interaction & UI ---
    FGameplayTag Input_Action_Interact;        // Use, open door, talk, loot
    FGameplayTag Input_Action_OpenInventory;
    FGameplayTag Input_Action_OpenMap;
    FGameplayTag Input_Action_OpenJournal;     // Or quests, codex, etc.
    FGameplayTag Input_Action_PauseGame;       // Open main menu

    // --- Ability / Item / Weapon Management ---
    FGameplayTag Input_Action_UseAbility1;     // Replaces InputTag_1
    FGameplayTag Input_Action_UseAbility2;     // Replaces InputTag_2
    FGameplayTag Input_Action_UseAbility3;     // Replaces InputTag_3
    FGameplayTag Input_Action_UseAbility4;     // Replaces InputTag_4
    // Add more if needed: Input_Action_UseAbility5, etc.

    FGameplayTag Input_Action_NextWeapon;
    FGameplayTag Input_Action_PreviousWeapon;
    FGameplayTag Input_Action_EquipWeaponSlot1; // For direct weapon selection
    FGameplayTag Input_Action_EquipWeaponSlot2;
    FGameplayTag Input_Action_EquipWeaponSlot3; 
    // ... etc. for direct weapon slots if you have more than next/prev

    FGameplayTag Input_Action_UseQuickHeal;    // If you have a dedicated heal button
    FGameplayTag Input_Action_UseQuickItem;    // For a generic "use selected quickslot item"

    // --- Vehicle Actions (If Applicable) ---
    // FGameplayTag Input_Action_EnterExitVehicle;
    // FGameplayTag Input_Action_AccelerateVehicle;
    // FGameplayTag Input_Action_BrakeVehicle;
    // FGameplayTag Input_Action_SteerLeftVehicle;
    // FGameplayTag Input_Action_SteerRightVehicle;

    // ... other game-specific actions ...

    // Player Fight Status (Consider renaming to Ability_Status_*)
    FGameplayTag Abilities_Status_Locked;
    FGameplayTag Abilities_Status_Eligible;
    FGameplayTag Abilities_Status_Unlocked;
    FGameplayTag Abilities_Status_Equipped;

    // Attack Type (Consider renaming to Ability_Activation_*)
    FGameplayTag Abilities_Type_Offensive; // Or Ability_Type_Active_Offensive
    FGameplayTag Abilities_Type_Passive;
    FGameplayTag Abilities_Type_None;

    // Ability State Tags (ActivationOwnedTags — on ASC while ability is active)
    FGameplayTag Ability_State_Shooting;
    FGameplayTag Ability_State_Reloading;
    FGameplayTag Ability_State_Aiming;
    FGameplayTag Ability_State_Sprinting;
    FGameplayTag Ability_State_MeleeAttacking;
    FGameplayTag Ability_State_Interacting;
    FGameplayTag Ability_State_Throwing;
    FGameplayTag Ability_State_Dashing;

    /** Authored at the first frame of each grab OUTCOME section (Push/Kick). The outcome is QUEUED at
     *  the wrestle loop boundary, so it begins up to a full cycle after it is chosen — the clip itself is
     *  the only thing that knows when it actually started, and the shove's root motion must be driven
     *  from exactly there. */
    FGameplayTag Event_Grab_OutcomeBegin;

    // ---- ATTACK PHASING (recovery cancel) ----
    // Startup and Active are committed; RECOVERY is cancellable. Published by a notify state on the clip
    // so the boundary is authored next to the animation, and mirrored onto the ASC as a loose tag so the
    // input layer and the movement-intent layer can both read it without knowing about the ability.
    FGameplayTag Event_Combat_CancelOpen;
    FGameplayTag Event_Combat_CancelClose;
    FGameplayTag State_Combat_CancelWindow;   // ASC mirror: "this attack may be cancelled right now"

    // ---- ABILITY IDENTITY (asset tags) ----
    // What an ability IS, so other abilities can address it in Block/CancelAbilitiesWithTag — those
    // match against the TARGET ability's ASSET tags, so an ability with no asset tag can never be
    // blocked or cancelled by tag (which is why every cancel in this project used to be hand-rolled by
    // class). Asset tags are read from the CDO via the spec, so they are authored in the BP tuning child
    // or patched at grant time — NOT in DeclareAbilityTags (see UAZ_GameplayAbility for the split).
    FGameplayTag Ability_Combat_Melee;      // hero punch + Chalkie claw (shared UAZ_GA_MeleeAttack rail)
    FGameplayTag Ability_Combat_Grab;       // Chalkie's grab commit
    FGameplayTag Ability_Combat_Grabbed;    // the victim's side of a grab
    FGameplayTag Ability_Combat_HitReact;   // stagger-class reaction
    FGameplayTag Ability_Combat_Death;      // terminal
    FGameplayTag Ability_Movement_Jump;
    FGameplayTag Ability_Movement_Crouch;

    // Ability Cooldown Tags (granted by cooldown GEs, checked by ActivationBlockedTags)
    FGameplayTag Ability_Cooldown_Shoot;
    FGameplayTag Ability_Cooldown_Reload;
    FGameplayTag Ability_Cooldown_Melee;
    FGameplayTag Ability_Cooldown_Interact;
    FGameplayTag Ability_Cooldown_Dash;
    FGameplayTag Ability_Cooldown_Grenade;
    FGameplayTag Ability_Cooldown_Sprint;

    // Weapon Type Tags
    FGameplayTag Weapon_None;
	FGameplayTag Weapon_Fist;
    FGameplayTag Weapon_Pistol;
    FGameplayTag Weapon_Rifle;
    FGameplayTag Weapon_RocketLauncher;
    FGameplayTag Weapon_Shotgun;
    FGameplayTag Weapon_SMG;
    FGameplayTag Weapon_Melee;

    // Weapon Slot Tags (on the weapon actor itself)
    FGameplayTag Weapon_Slot_Primary;
    FGameplayTag Weapon_Slot_Secondary;
    FGameplayTag Weapon_Slot_Sidearm;
    FGameplayTag Weapon_Slot_Melee;

    // Movement State Tags
    FGameplayTag Movement_Idle;
    FGameplayTag Movement_Walking;
    FGameplayTag Movement_Running;
    FGameplayTag Movement_Sprinting;
    FGameplayTag Movement_Crouching;
    FGameplayTag Movement_Jumping;
    FGameplayTag Movement_Falling;
    FGameplayTag Movement_Aiming;
    FGameplayTag Movement_Swimming;
    FGameplayTag Movement_Climbing;
    FGameplayTag Movement_Strafe;   // combat-ready rotation mode: body faces target, locomotion is directional
    FGameplayTag Combat_Ready;      // upper-body fists-up stance; TIMED + auto-clears. Independent of Movement.Strafe

    // Combat State Tags
    FGameplayTag Combat_Aiming;
    FGameplayTag Combat_Blocking;
    FGameplayTag Combat_Attacking;
    FGameplayTag Combat_Reloading;
    FGameplayTag Combat_Firing;
    FGameplayTag Combat_MeleeSwing;

    // Character State Tags
    FGameplayTag Character_Dead;
    FGameplayTag Character_Dying;
    FGameplayTag Character_Stunned;
    FGameplayTag Character_Downed;
    FGameplayTag Character_Interacting;

    // Infected AI phase (published by AAZ_InfectedAIController to the NPC's OWN ASC via AddStateTag, so it
    // REPLICATES — client-side Chalkie AnimInstances read the phase from the ASC in co-op, where AI
    // controllers exist only on the server. Exactly one of the three is present at a time.)
    FGameplayTag State_Infected_Dormant;
    FGameplayTag State_Infected_Alerted;
    FGameplayTag State_Infected_Aggressive;

    // Crowd-brain COMBAT ROLES (rule 7 — published by UAZ_HordeSubsystem, the SOLE writer; replicated
    // via AddStateTag so client AnimInstances can read the menace posture in co-op. Phase = knowledge
    // (State.Infected.*), role = permission: at most MaxAttackersPerPrey Chalkies hold Active per prey,
    // every other engaged Chalkie is Passive and holds the ring.)
    FGameplayTag State_Combat_Engaged_Active;    // holds an attack slot: press the attack
    FGameplayTag State_Combat_Engaged_Passive;   // live target, no slot: ring at RingDistance, menace, wait

    FGameplayTag State_Combat_Staggered;  // a stagger-class reaction owns the body (hit-react flinch OR the
                                          //  pack step-back). EXPLICIT gate with an explicit duration, set
                                          //  where the reaction starts — replaces inferring "staggered" from
                                          //  Montage_IsPlaying, where blend timing silently set AI pacing.
                                          //  (A′ loose-tag form today; becomes GA_HitReact's
                                          //  ActivationOwnedTags at arch step A.)

    // --- GRAB / grapple (TLOU-style "caught") ---
    FGameplayTag State_Combat_Grabbing;   // Chalkie: mid-grab commit — holds its Active slot, plays the grab
                                          //  pose, and is exempt from the rule-8 flinch-cancel while grabbing
    FGameplayTag State_Grabbed;           // Player: caught — movement + camera frozen (checked in ProduceInput
                                          //  and OnLookTriggered); struggle-mash the only available action
    FGameplayTag Mover_GrabAnchor;        // MOVER FEATURE tag (not an ASC tag): reported by
                                          //  FLayeredMove_AZ_GrabAnchor so GA_PlayerGrabbed can end the
                                          //  socket lock with CancelFeaturesWithTag on every exit path

    // Grab STAGE (paired-montage grab). Published by the LEADER (GA_ChalkieGrab) on its own ASC as it
    // moves the shared montage between sections — stages inside a paired grab are montage sections, not
    // separate abilities, so this is how the BT / crowd brain / damage rules see which stage is running
    // without any extra ability plumbing. Exactly one is present at a time while the grab is live.
    FGameplayTag State_Grab_Catching;     // the catch clip is playing (contact already made)
    FGameplayTag State_Grab_Wrestling;    // the hold loop: mash window is open, outcome undecided
    FGameplayTag State_Grab_Resolving;    // outcome section queued/running — no longer interruptible

    // --- Reactions (arch step A: event-driven, "events drive, timers guard") ---
    FGameplayTag Event_Combat_HitReact;     // HandleDamaged -> GA_HitReact: survivable hit, play the variant flinch
    FGameplayTag Event_Combat_StepBack;     // horde -> GA_HitReact: pack recoil beat (payload: montage + optional shorter beat)
    FGameplayTag Event_Combat_GrabShoved;   // GA_ChalkieGrab -> GA_HitReact (SELF): the player broke the grab.
                                            //  Third trigger on the SAME reaction ability rather than a new one:
                                            //  the shove needs exactly what a hit reaction already owns — a clip
                                            //  with an authored beat, a root-motion window cut at the recoil peak,
                                            //  and the Staggered gate held for the ability's lifetime. A separate
                                            //  ability would be a SECOND absolute writer to that counted tag, and a
                                            //  punch landing mid-shove would run both concurrently.
    FGameplayTag Event_Combat_BeatEnd;      // AUTHORED ON THE MONTAGE at the measured beat — the timeline itself
                                            //  ends the reaction/bite (rate-scale/interrupt-safe); timers only guard

    // --- Damage spine (S1): montage hit windows, death, SetByCaller magnitudes ---
    FGameplayTag Event_Montage_Melee_Hit;   // RETIRED as the damage trigger (kept for old data): the single
                                            //  hand-placed contact frame was the source of every timing bug
    FGameplayTag Event_Montage_Melee_WindowBegin;   // strike phase opens: GA_MeleeAttack starts the SOCKET
                                                    //  SWEEP — the frame the fist actually touches a pawn IS
                                                    //  the hit; contact timing is physics, not authored data
    FGameplayTag Event_Montage_Melee_WindowEnd;     // strike phase closes: sweep stops; anything not touched
                                                    //  was a legitimate whiff
    FGameplayTag Event_Death;               // sent to the dying actor's ASC when Vitals Health hits 0
    FGameplayTag Event_Grabbed;             // sent to the PLAYER's ASC to trigger GA_PlayerGrabbed (like Event_Death->GA_Death)
    FGameplayTag Event_GrabEscaped;         // player -> grabber ASC: mash won, shove me off (GA_ChalkieGrab plays GrabEscapeMontage)
    FGameplayTag Event_GrabTimeout;         // player -> grabber ASC: window elapsed, attack lands (GrabEndMontage + damage chunk)
    FGameplayTag Event_GrabRelease;         // grabber -> player ASC: grab died for another reason (cancel/death) — free the player
    FGameplayTag Event_Grab_Shove;          // hero's escape montage -> its own ASC, forwarded to the grabber: the shove
                                            //  CONNECTS on this frame. Measured: both NAAT escape clips sit still for
                                            //  1.35s and only then depart 39.6cm, so the contact is late in the clip and
                                            //  firing the knockback at the section start would throw the whole wind-up
                                            //  away. The notify lives on the hero's own standalone montage — a montage
                                            //  FOLLOWER gets SetPosition every frame and can skip notifies entirely,
                                            //  which is why the outcome is not synced at all.
    FGameplayTag SetByCaller_Damage;        // magnitude channel on GE_Damage specs

    // Equipment State Tags
    FGameplayTag State_Equipped_Weapon_Primary;
    FGameplayTag State_Equipped_Weapon_Secondary;
    FGameplayTag State_Equipped_Weapon_Sidearm;
    FGameplayTag State_Equipped_Weapon_Melee;
    FGameplayTag State_Equipped_Armor_Head;
    FGameplayTag State_Equipped_Armor_Chest;
    FGameplayTag State_Equipped_Armor_Legs;
    FGameplayTag State_Equipped_Armor_Hands;
    FGameplayTag State_Equipped_Armor_Feet;
    FGameplayTag State_Equipped_Consumable_QuickSlot1;

    // Animation Modifier Tags
    FGameplayTag Animation_HipFire;
    FGameplayTag Animation_ADS; // Aim Down Sights
    
    // --- Animation State Tags ---
    // These tags are granted by passive abilities to represent the final, unambiguous
    // animation state for the AnimBP to read. Each corresponds to one UGA_AnimationState child.

    // --- Animation State Tags ---
    // These tags are granted by passive abilities to represent the final, unambiguous
    // animation state for the AnimBP to read.
   
    // Unarmed States (Weapon.None)
    FGameplayTag Ability_Animation_Unarmed_Idle;
    FGameplayTag Ability_Animation_Unarmed_Walking;
    FGameplayTag Ability_Animation_Unarmed_Running;
    FGameplayTag Ability_Animation_Unarmed_Sprinting;
    FGameplayTag Ability_Animation_Unarmed_CrouchingIdle;
    FGameplayTag Ability_Animation_Unarmed_CrouchingWalking;
    FGameplayTag Ability_Animation_Unarmed_Attack_Light;
    FGameplayTag Ability_Animation_Unarmed_Attack_Heavy;
    FGameplayTag Ability_Animation_Unarmed_Jumping;
    FGameplayTag Ability_Animation_Unarmed_Falling;

    // Pistol States (Weapon.Pistol)
    FGameplayTag Ability_Animation_Pistol_Idle;
    FGameplayTag Ability_Animation_Pistol_Walking;
    FGameplayTag Ability_Animation_Pistol_Running;
    FGameplayTag Ability_Animation_Pistol_Sprinting;
    FGameplayTag Ability_Animation_Pistol_CrouchingIdle;
    FGameplayTag Ability_Animation_Pistol_CrouchingWalking;
    FGameplayTag Ability_Animation_Pistol_AimingIdle;
    FGameplayTag Ability_Animation_Pistol_AimingWalking;
    FGameplayTag Ability_Animation_Pistol_Fire;
    FGameplayTag Ability_Animation_Pistol_AimingFire;
    FGameplayTag Ability_Animation_Pistol_Reloading;
    FGameplayTag Ability_Animation_Pistol_Jumping;
    FGameplayTag Ability_Animation_Pistol_Falling;

    // Rifle States (Weapon.Rifle, Weapon.SMG, etc.)
    FGameplayTag Ability_Animation_Rifle_Idle;
    FGameplayTag Ability_Animation_Rifle_Walking;
    FGameplayTag Ability_Animation_Rifle_Running;
    FGameplayTag Ability_Animation_Rifle_Sprinting;
    FGameplayTag Ability_Animation_Rifle_CrouchingIdle;
    FGameplayTag Ability_Animation_Rifle_CrouchingWalking;
    FGameplayTag Ability_Animation_Rifle_AimingIdle;
    FGameplayTag Ability_Animation_Rifle_AimingWalking;
    FGameplayTag Ability_Animation_Rifle_Fire;
    FGameplayTag Ability_Animation_Rifle_AimingFire;
    FGameplayTag Ability_Animation_Rifle_Reloading;
    FGameplayTag Ability_Animation_Rifle_Jumping;
    FGameplayTag Ability_Animation_Rifle_Falling;

    // Melee Weapon States (Weapon.Melee)
    FGameplayTag Ability_Animation_Melee_Idle;
    FGameplayTag Ability_Animation_Melee_Walking;
    FGameplayTag Ability_Animation_Melee_Running;
    FGameplayTag Ability_Animation_Melee_Sprinting;
    FGameplayTag Ability_Animation_Melee_CrouchingIdle;
    FGameplayTag Ability_Animation_Melee_CrouchingWalking;
    FGameplayTag Ability_Animation_Melee_Attack_Light;
    FGameplayTag Ability_Animation_Melee_Attack_Heavy;
    FGameplayTag Ability_Animation_Melee_Block;
    FGameplayTag Ability_Animation_Melee_Jumping;
    FGameplayTag Ability_Animation_Melee_Falling;

    // General Character States (Overrides)
    FGameplayTag Ability_Animation_Character_Stunned;
    FGameplayTag Ability_Animation_Character_Dying;
    FGameplayTag Ability_Animation_Character_Dead;
    FGameplayTag Ability_Animation_Character_Interacting;

    // In your FGameplayTags.h file, inside the struct FGameplayTags definition

    // --- Animation State Tags ---
    // These tags are granted by passive abilities to represent the final, unambiguous
    // animation state for the AnimBP to read.

    // Unarmed States
    FGameplayTag Animation_State_Unarmed_Idle;
    FGameplayTag Animation_State_Unarmed_Walking;
    FGameplayTag Animation_State_Unarmed_Running;
    FGameplayTag Animation_State_Unarmed_Sprinting;
    FGameplayTag Animation_State_Unarmed_CrouchingIdle;
    FGameplayTag Animation_State_Unarmed_CrouchingWalking;
    FGameplayTag Animation_State_Unarmed_Attack_Light;
    FGameplayTag Animation_State_Unarmed_Attack_Heavy;
    FGameplayTag Animation_State_Unarmed_StartJumping;
    FGameplayTag Animation_State_Unarmed_EndJumping;
    FGameplayTag Animation_State_Unarmed_Jumping;
    FGameplayTag Animation_State_Unarmed_Falling;

    // Pistol States
    FGameplayTag Animation_State_Pistol_Idle;
    FGameplayTag Animation_State_Pistol_Walking;
    FGameplayTag Animation_State_Pistol_Running;
    FGameplayTag Animation_State_Pistol_Sprinting;
    FGameplayTag Animation_State_Pistol_CrouchingIdle;
    FGameplayTag Animation_State_Pistol_CrouchingWalking;
    FGameplayTag Animation_State_Pistol_AimingIdle;
    FGameplayTag Animation_State_Pistol_AimingWalking;
    FGameplayTag Animation_State_Pistol_Fire;
    FGameplayTag Animation_State_Pistol_AimingFire;
    FGameplayTag Animation_State_Pistol_Reloading;
    FGameplayTag Animation_State_Pistol_Jumping;
    FGameplayTag Animation_State_Pistol_Falling;

    // Rifle States (can be reused for SMG, Shotgun etc. or you can make unique ones)
    FGameplayTag Animation_State_Rifle_Idle;
    FGameplayTag Animation_State_Rifle_Walking;
    FGameplayTag Animation_State_Rifle_Running;
    FGameplayTag Animation_State_Rifle_Sprinting;
    FGameplayTag Animation_State_Rifle_CrouchingIdle;
    FGameplayTag Animation_State_Rifle_CrouchingWalking;
    FGameplayTag Animation_State_Rifle_AimingIdle;
    FGameplayTag Animation_State_Rifle_AimingWalking;
    FGameplayTag Animation_State_Rifle_Fire;
    FGameplayTag Animation_State_Rifle_AimingFire;
    FGameplayTag Animation_State_Rifle_Reloading;
    FGameplayTag Animation_State_Rifle_Jumping;
    FGameplayTag Animation_State_Rifle_Falling;

    // Melee Weapon States
    FGameplayTag Animation_State_Melee_Idle;
    FGameplayTag Animation_State_Melee_Walking;
    FGameplayTag Animation_State_Melee_Running;
    FGameplayTag Animation_State_Melee_Sprinting;
    FGameplayTag Animation_State_Melee_CrouchingIdle;
    FGameplayTag Animation_State_Melee_CrouchingWalking;
    FGameplayTag Animation_State_Melee_Attack_Light;
    FGameplayTag Animation_State_Melee_Attack_Heavy;
    FGameplayTag Animation_State_Melee_Block;
    FGameplayTag Animation_State_Melee_Jumping;
    FGameplayTag Animation_State_Melee_Falling;

    // General Character States (Overrides)
    FGameplayTag Animation_State_Character_Stunned;
    FGameplayTag Animation_State_Character_Dying;
    FGameplayTag Animation_State_Character_Dead;
    FGameplayTag Animation_State_Character_Interacting;

    // --- Equipment Slot Tags ---
    // These tags represent the specific slots on a character where items can be equipped.

    // Weapon Slots
    FGameplayTag Equipment_Slot_PrimaryWeapon;    // e.g., for Rifles, Shotguns
    FGameplayTag Equipment_Slot_SecondaryWeapon;  // e.g., for SMGs, smaller rifles
    FGameplayTag Equipment_Slot_Sidearm;          // e.g., for Pistols
    FGameplayTag Equipment_Slot_Melee;            // e.g., for Knives, Swords

    // Armor Slots
    FGameplayTag Equipment_Slot_Head;
    FGameplayTag Equipment_Slot_Chest;
    FGameplayTag Equipment_Slot_Legs;
    FGameplayTag Equipment_Slot_Hands;
    FGameplayTag Equipment_Slot_Feet;
    FGameplayTag Equipment_Slot_Back;             // e.g., for Capes or Backpacks

    // Accessory Slots
    FGameplayTag Equipment_Slot_Amulet;
    FGameplayTag Equipment_Slot_Ring1;
    FGameplayTag Equipment_Slot_Ring2;
    FGameplayTag Equipment_Slot_Belt;

    // Quick Slots (for consumables or usable items)
    FGameplayTag Equipment_Slot_QuickSlot1;
    FGameplayTag Equipment_Slot_QuickSlot2;
    FGameplayTag Equipment_Slot_QuickSlot3;
    FGameplayTag Equipment_Slot_QuickSlot4;

    // ============================================================
    // ITEM TYPE TAGS
    // Defines what the item fundamentally is.
    // ============================================================

    // --- Weapon Types ---
    FGameplayTag Item_Type_Weapon_Pistol; // Standard sidearm, moderate damage, common ammo.
    FGameplayTag Item_Type_Weapon_Rifle; // High-accuracy ranged weapon, higher damage.
    FGameplayTag Item_Type_Weapon_Shotgun; // Close-range, high spread and stopping power.
    FGameplayTag Item_Type_Weapon_SMG; // High rate of fire, low damage per shot.
    FGameplayTag Item_Type_Weapon_RocketLauncher; // Heavy weapon with explosive ammo.
    FGameplayTag Item_Type_Weapon_Melee; // Hand-to-hand weapon, low range, no ammo needed.
    FGameplayTag Item_Type_Weapon_Tool; // Dual-purpose tools (crowbar, axe) usable as melee or for puzzles.

    // --- Ammunition Types ---
    FGameplayTag Item_Type_Ammo_Pistol; // Standard pistol rounds.
    FGameplayTag Item_Type_Ammo_Rifle; // Rifle cartridges.
    FGameplayTag Item_Type_Ammo_Shotgun; // Shotgun shells.
    FGameplayTag Item_Type_Ammo_SMG; // Submachine gun ammunition.
    FGameplayTag Item_Type_Ammo_Rocket; // Rocket or grenade launcher ammo.
    FGameplayTag Item_Type_Ammo_Special; // Rare or elemental ammo (silver, incendiary, etc).

    // --- Armor / Equipment Types ---
    FGameplayTag Item_Type_Armor_Head; // Helmet, mask, or head protection.
    FGameplayTag Item_Type_Armor_Chest; // Body armor or tactical vest.
    FGameplayTag Item_Type_Armor_Legs; // Leg protection gear.
    FGameplayTag Item_Type_Armor_Hands; // Gloves or protective handwear.
    FGameplayTag Item_Type_Armor_Feet; // Boots or foot armor.
    FGameplayTag Item_Type_Armor_Back; // Backpacks or back-mounted protection.
    FGameplayTag Item_Type_Armor_FullBody; // Complete suit (hazmat, tactical, or bio suit).

    // --- Accessory Types ---
    FGameplayTag Item_Type_Accessory_Amulet; // Occult or protective item worn on neck.
    FGameplayTag Item_Type_Accessory_Ring; // Magical or status-enhancing ring.
    FGameplayTag Item_Type_Accessory_Belt; // Utility belt or wearable equipment modifier.
    FGameplayTag Item_Type_Accessory_Charm; // Small item granting passive bonus or curse.
	FGameplayTag Item_Type_Accessory_Cloak;
	FGameplayTag Item_Type_Accessory_Mask;

    // --- Consumable Types ---
    FGameplayTag Item_Type_Consumable_Health; // Heals health (medkit, bandage).
    FGameplayTag Item_Type_Consumable_Buff; // Temporary stat boost (adrenaline, stimpack).
    FGameplayTag Item_Type_Consumable_Throwable; // Usable item that can be thrown (grenade, molotov).
    FGameplayTag Item_Type_Consumable_Antidote; // Cures infection, toxin, or corruption effects.
    FGameplayTag Item_Type_Consumable_Food; // Restores partial health or stamina.
    FGameplayTag Item_Type_Consumable_Psych; // Stabilizes sanity or reduces fear.

    // --- Weapon Attachment Types ---
    FGameplayTag Item_Type_Attachment_Scope; // Increases zoom or accuracy.
    FGameplayTag Item_Type_Attachment_Magazine; // Expands ammo capacity.
    FGameplayTag Item_Type_Attachment_Muzzle; // Reduces recoil or suppresses sound.
    FGameplayTag Item_Type_Attachment_Grip; // Improves handling or accuracy.
    FGameplayTag Item_Type_Attachment_Stock; // Improves stability.
    FGameplayTag Item_Type_Attachment_Laser; // Adds aiming laser or tracking.
    FGameplayTag Item_Type_Attachment_Light; // Adds flashlight or tactical light source.

    // --- Crafting & Resource Types ---
    FGameplayTag Item_Type_Material_Scrap; // Basic crafting metal and parts.
    FGameplayTag Item_Type_Material_Electronics; // Circuit boards, wires, and tech components.
    FGameplayTag Item_Type_Material_Chemicals; // Chemical reagents for crafting ammo or meds.
    FGameplayTag Item_Type_Material_Biological; // Organic samples, tissue, spores.
    FGameplayTag Item_Type_Material_Powder; // Gunpowder or other powdered reagent.
    FGameplayTag Item_Type_Material_Fluid; // Solvent, fuel, acid, or oil.

    // --- Keys, Notes & Quest Items ---
    FGameplayTag Item_Type_Key; // Traditional key for doors or locks.
    FGameplayTag Item_Type_KeyCard; // Electronic or magnetic access card.
    FGameplayTag Item_Type_Document; // Notes, journals, or letters revealing lore or clues.
    FGameplayTag Item_Type_PuzzleItem; // Artifacts or emblems used to unlock puzzle mechanisms.
    FGameplayTag Item_Type_QuestItem; // Core item required for progression or storyline.

    // --- Currency & Valuables ---
    FGameplayTag Item_Type_Currency; // Money or tradable credits.
    FGameplayTag Item_Type_Junk; // Sellable trash or broken parts.
    FGameplayTag Item_Type_Valuable; // High-value items (watches, gems, relics).

    // Defines how the item behaves in systems (inventory, trade, combat, etc.).
    FGameplayTag Item_Property_Stackable;           // Can stack multiple units in one slot.
    FGameplayTag Item_Property_Equippable;          // Can be equipped in gear or weapon slot.
    FGameplayTag Item_Property_Consumable;          // Consumed on use (disappears after activation).
    FGameplayTag Item_Property_Droppable;           // Can be dropped from inventory.
    FGameplayTag Item_Property_QuestItem;           // Cannot be dropped, sold, or destroyed.
    FGameplayTag Item_Property_Tradeable;           // Can be sold or exchanged with NPCs.
    FGameplayTag Item_Property_Craftable;           // Can be used in crafting recipes.
    FGameplayTag Item_Property_Breakable;           // Has durability and can break on use.
    FGameplayTag Item_Property_Cursed;              // Has negative or supernatural side effects.
    FGameplayTag Item_Property_Hidden;              // Hidden or inactive until revealed.
    FGameplayTag Item_Property_LimitedUse;          // Has limited number of uses or charges.
    FGameplayTag Item_Property_Fragile;             // Easily breaks or loses effectiveness.

    // Defines quality, drop rate, and thematic importance.
    FGameplayTag Item_Rarity_Common;                // Everyday or easily found item.
    FGameplayTag Item_Rarity_Uncommon;              // Slightly better or rarer item.
    FGameplayTag Item_Rarity_Rare;                  // Hard-to-find, more powerful item.
    FGameplayTag Item_Rarity_Epic;                  // Unique or story-related item.
    FGameplayTag Item_Rarity_Legendary;             // Iconic, extremely rare item.
    FGameplayTag Item_Rarity_Prototype;             // Experimental or prototype gear.
    FGameplayTag Item_Rarity_Cursed;                // Strong but dangerous or cursed item.
    FGameplayTag Item_Rarity_Relic;                 // Ancient or occult artifact.

    // Special tags for fear, sanity, and atmosphere systems — ideal for survival horror mechanics.
    FGameplayTag Item_Effect_FearReduction;         // Reduces character fear or panic level.
    FGameplayTag Item_Effect_SanityBoost;           // Restores mental stability or focus.
    FGameplayTag Item_Effect_NoiseMaker;            // Attracts nearby enemies when used.
    FGameplayTag Item_Effect_LightSource;           // Emits light (flashlight, candle, flare).
    FGameplayTag Item_Effect_Holy;                  // Repels or damages supernatural entities.
    FGameplayTag Item_Effect_Tainted;               // Attracts enemies or corrupts user over time.

    // ============================================================
    // ITEM FRAGMENT TAGS
    // Defines fragment types for inventory items.
    // ============================================================
    FGameplayTag Item_Fragment_Grid;
    FGameplayTag Item_Fragment_Stats;
	FGameplayTag Item_Fragment_Stats_Mode_1;
	FGameplayTag Item_Fragment_Stats_Mode_2;
	FGameplayTag Item_Fragment_Stats_Mode_3;
	FGameplayTag Item_Fragment_Stats_Mode_4;
	FGameplayTag Item_Fragment_Stats_Mode_5;
	FGameplayTag Item_Fragment_PrimaryStats;
	FGameplayTag Item_Fragment_SecondaryStats;
    FGameplayTag Item_Fragment_Visual;     
    FGameplayTag Item_Fragment_Icon;
	FGameplayTag Item_Fragment_Name;
	FGameplayTag Item_Fragment_Name_StaticText;
	FGameplayTag Item_Fragment_Type;
	FGameplayTag Item_Fragment_Text;
	FGameplayTag Item_Fragment_Value;
	FGameplayTag Item_Fragment_Level;
    FGameplayTag Item_Fragment_Stackable;
    FGameplayTag Item_Fragment_Consumable;
	FGameplayTag Item_Fragment_Equipment;
    FGameplayTag Item_Fragment_Durability;
    FGameplayTag Item_Fragment_Modification;
    FGameplayTag Item_Fragment_Metadata;
	FGameplayTag Item_Fragment_Ammo_Primary_Name;
	FGameplayTag Item_Fragment_Ammo_Secondary_Name;

	// Weapon detail tags (for item description UI)
	FGameplayTag Item_Fragment_Description;

	FGameplayTag Item_Fragment_MagazineSize;
	FGameplayTag Item_Fragment_MagazineSize_Text;
	FGameplayTag Item_Fragment_MagazineSize_Value;
	FGameplayTag Item_Fragment_MagazineSize_MaxValue;
	FGameplayTag Item_Fragment_MagazineSize_StaticText;
	FGameplayTag Item_Fragment_MagazineSize_Diff;
		
	FGameplayTag Item_Fragment_Ammo;
	FGameplayTag Item_Fragment_Ammo_Text;
	FGameplayTag Item_Fragment_Ammo_Value;
	FGameplayTag Item_Fragment_Ammo_MaxValue;
	FGameplayTag Item_Fragment_Ammo_Separator;
	FGameplayTag Item_Fragment_Ammo_StaticText;
	FGameplayTag Item_Fragment_Ammo_Diff;

	FGameplayTag Item_Fragment_Damage;
	FGameplayTag Item_Fragment_Damage_Text;
	FGameplayTag Item_Fragment_Damage_Value;
	FGameplayTag Item_Fragment_Damage_MaxValue;
	FGameplayTag Item_Fragment_Damage_Separator;
	FGameplayTag Item_Fragment_Damage_StaticText;
	FGameplayTag Item_Fragment_Damage_Diff;

	FGameplayTag Item_Fragment_Range;
	FGameplayTag Item_Fragment_Range_Text;
	FGameplayTag Item_Fragment_Range_Value;
	FGameplayTag Item_Fragment_Range_MaxValue;
	FGameplayTag Item_Fragment_Range_StaticText;
	FGameplayTag Item_Fragment_Range_Diff;

	FGameplayTag Item_Fragment_Reload;
	FGameplayTag Item_Fragment_Reload_Text;
	FGameplayTag Item_Fragment_Reload_Value;
	FGameplayTag Item_Fragment_Reload_MaxValue;
	FGameplayTag Item_Fragment_Reload_StaticText;
	FGameplayTag Item_Fragment_Reload_Diff;

	FGameplayTag Item_Fragment_Condition;
	FGameplayTag Item_Fragment_Condition_Text;
	FGameplayTag Item_Fragment_Condition_Value;
	FGameplayTag Item_Fragment_Condition_MaxValue;
	FGameplayTag Item_Fragment_Condition_StaticText;
	FGameplayTag Item_Fragment_Condition_Diff;

	FGameplayTag Item_Fragment_Accuracy;
	FGameplayTag Item_Fragment_Accuracy_Text;
	FGameplayTag Item_Fragment_Accuracy_Value;
	FGameplayTag Item_Fragment_Accuracy_MaxValue;
	FGameplayTag Item_Fragment_Accuracy_StaticText;
	FGameplayTag Item_Fragment_Accuracy_Diff;

	FGameplayTag Item_Fragment_Recoil;
	FGameplayTag Item_Fragment_Recoil_Text;
	FGameplayTag Item_Fragment_Recoil_Value;
	FGameplayTag Item_Fragment_Recoil_MaxValue;
	FGameplayTag Item_Fragment_Recoil_StaticText;
	FGameplayTag Item_Fragment_Recoil_Diff;

	FGameplayTag Item_Fragment_Weight;
	FGameplayTag Item_Fragment_Weight_Text;
	FGameplayTag Item_Fragment_Weight_Value;
	FGameplayTag Item_Fragment_Weight_MaxValue;
	FGameplayTag Item_Fragment_Weight_StaticText;
	FGameplayTag Item_Fragment_Weight_Diff;



private:
    
    static FAZ_GameplayTags GameplayTags;
};