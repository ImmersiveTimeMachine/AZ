---
name: project_combat_fist_build_plan
description: "LIVE working doc (started 2026-06-15) for the FIST-FIRST combat build + quick-slot/equip skeleton. Decisions locked this session: CommonUI is the canonical equip path (keystone already built there); FIST = weapon #0 (Weapon.Fist, a real weapon, NOT 'no weapon'); equip axis no_fist(Weapon.None) <-> fist(Weapon.Fist) <-> pistol <-> rifle via a slim quick-slot manager (UAZ_QuickBarComponent on PC); number keys 0,1,2 = debug SIM of the future radial/cross UI; for fists equip(draw=Idle2Fists)=ready stance & unequip(holster=Fists2Idle)=relaxed/explore; ONE weapon-agnostic GA_MeleeAttack; full-RM attacks; combat = tags+montages+chooser rows, NOT new SM phases. Assets verified on disk. Resume point + open questions inside. Companion to [[project_combat_system_plan]]."
metadata:
  node_type: memory
  type: project
  originSessionId: resume-2026-06-15
---

# CHALK — Fist-First Combat & Quick-Slot Build Plan
**Status:** design converged, NOT yet built. Editor was DOWN this session (no live MCP checks, no content authoring). Companion/refinement of the broader [[project_combat_system_plan]] (read that for full design rationale). **Read this first to resume the build.**

## GOAL
Build and *iterate* the **FIST** combat model first (punch logic + combat movement), on the cheapest possible equipment, then generalize to weapons by **data, not code** (parity by construction). Build only the minimum **quick-slot/equip skeleton** needed to switch the active profile (fist <-> none <-> weapons). "Concentrate on logic, not design (UI)."

User's words: *"the first equipment is our fist and we will add more and more logic on fist first and then will move to another weapons after we will have a good fist model."* And: *"we need just the basic to simulate all this equipments."*

## LOCKED DECISIONS (this session, 2026-06-15)
1. **CommonUI is the canonical equip path.** (Project has 2 inventory UIs — old UMG `AZ_Inv_*` deprecated, new CommonUI `AZ_Inv_CommonUI_*` active.) Build on the CommonUI equip flow. Do NOT add new equip logic to `UAZ_EquipmentManagerComponent` — its *equip* role is superseded; its world pickup/overlap role is audited separately before any retirement (do NOT delete blindly).
2. **The keystone is ALREADY BUILT in the CommonUI path.** `UAZ_Inv_CommonUI_EquipmentComponent::OnItemEquipped` already (a) grants the item's abilities from `AbilityGrantFragment` *with each ability's `InputTag` seeded* (so they're instantly fireable by the input rig), and (b) publishes the profile tag via `ASC->OnWeaponEquipped(EquipmentType)`. We do NOT build the keystone — we just trigger this flow.
3. **FIST = weapon #0 = `Weapon.Fist`** — a real, first-class weapon profile (sibling of `Weapon.Pistol`/`Rifle`), NOT "no weapon." `Weapon.None` = genuinely empty hands ("no_fist"). `Weapon.Fist` tag must be ADDED (missing today).
4. **Fist is a real inventory item**: `EquipmentFragment(EquipmentType=Weapon.Fist, EquipActorClass=null)` (no mesh — hands already exist) + `AbilityGrantFragment(GA_MeleeAttack)`. Goes through the *identical* CommonUI equip path as every gun → real parity, punch is *equip-granted* (not special-cased default-granted).
5. **Equip axis (the quick-slot):** `no_fist(Weapon.None)  <->  fist(Weapon.Fist)  <->  pistol  <->  rifle ...`
6. **For FISTS, equip = ready stance** (cleaner, free given the clips): equip/draw fist = `Idle2Fists` -> fighting stance (Strafe, ready); unequip/holster = `Fists2Idle` -> relaxed/explore (= no_fist). The **quick-slot selection doubles as the fists-up toggle.** GUNS are different — equipped-but-lowered is a real state, so they keep the full **explore -> combat-ready -> aiming** tier axis. *(CONFIRMED 2026-06-15: equip=ready.)*
7. **Quick-slot manager skeleton IS needed** (user reaffirmed — the equip/unequip of fist & weapons flows through it). Build a slim **`UAZ_QuickBarComponent` on the PlayerController** (cross-pawn, co-op-safe): `Slots[] -> CommonUI item refs`, `ActiveSlotIndex`, `Select(N)` / `CycleNext/Prev`. On change -> holster old + draw new -> `Server_EquipSlotClicked(newItem, oldItem)`. **Logic only, no widget.**
8. **Number keys `0,1,2,...` = debug front-end that SIMULATES the future real UI** (radial wheel / directional cross — see ref image, STALKER/Metro-style). Keys and the future UI are interchangeable front-ends; both call `Select(N)`. Slot 0 = no_fist, slot 1 = fist, slot 2 = pistol, ...
9. **`GA_MeleeAttack` = ONE weapon-agnostic parameterized ability.** Params latched at activation: `hand` (LMB=L / RMB=R), `weight` (hold>threshold = Heavy/Light), `movement` (`bIsMoving` INTENT = Moving/Idle — NO trace/targeting), `profile` (the ASC weapon tag), `comboIndex`. Selector: `montage = CHT(profile,hand,weight,movement,combo)`. Unarmed = the `Weapon.Fist` profile; weapons inherit by swapping rows/data.
10. **Attacks are FULLY root-motion.** Moving punch = forward-advancing RM (queue an `FLayeredMove_RootMotionAttribute` for the montage window — same Mover RM bridge as loco/jump). Idle punch = in-place, cosmetic.
11. **Combat is NOT new `EAZ_StateMachineState` phases** — it's tags + montages + chooser rows over the existing SM. Profile = ASC tag (from equip); tier = a readiness tag + `RotationMode` switch; chooser keys on `(profile x tier x MovementDirection x gait)`.

## ARCHITECTURE DRAWING
```
FRONT-ENDS  (number keys = debug SIM of the future UI)
   keys 0,1,2...   |   radial wheel (later)   |   directional cross (later)
        +-----------------+----------------------+
                          | Select(N)
                          v
QUICK-SLOT MANAGER  (UAZ_QuickBarComponent, on PlayerController) -- the skeleton
   Slots: [0]=no_fist(Weapon.None)  [1]=fist(Weapon.Fist)  [2]=pistol  [3]=rifle ...
   Select(N): if slot changed -> holster old (montage) -> draw new (montage)
                          | Server_EquipSlotClicked(newItem, oldItem)
                          v
EQUIP EXECUTION  (CommonUI -- already built)
   item fragments:  EquipmentFragment(profile tag, draw/holster montage, null mesh for fist)
                    AbilityGrantFragment(abilities)
   -> grant/clear abilities  +  ASC->OnWeaponEquipped(profile tag)
              +-----------+---------------+
              v profile tag -> OwnedTags   v abilities granted (InputTag-seeded)
        [ CHT chooser ]                LMB/RMB -> GA_MeleeAttack
        profile x tier x dir x gait      -> punch montage (full RM)
        -> anim set / clip

EQUIP AXIS (quick-slot):  no_fist(None) <-> fist(Weapon.Fist) <-> pistol <-> rifle
   draw fist = Idle2Fists      holster fist = Fists2Idle
TIER AXIS (guns only):    explore --ready--> combat-ready --aim--> aiming
   (fists: equip already = ready, so no separate tier)
```

## PROFILE x TIER MATRIX
| profile | explore (down, OrientToMovement) | combat-ready (up, Strafe) | aiming (ADS, Strafe) |
|---|---|---|---|
| `Weapon.None` (no_fist, slot 0) | empty hands, can't fight | — | — |
| `Weapon.Fist` (slot 1) **<- BUILD FIRST** | (collapses into ready) | `Fists_Idle` + strafe loco + punches | — (no ADS; RMB = R punch) |
| `Weapon.Pistol` (slot 2) | std loco | combat loco | ADS zoom |
| `Weapon.Rifle` (slot 3) | std loco | combat loco | ADS zoom |
Cells = chooser rows (data), NOT new SM states. "Same for pistol and so on" = add rows + a data set, no new C++.

## VERIFIED WIRING (file:line — validated this session; do NOT re-investigate)
**Input -> ability (Lyra-style, complete):**
- `UAZ_InputConfig.AbilityInputActions` : `TArray<FAZ_InputAction{InputAction, InputTag}>` — `Public/Input/AZ_InputConfig.h:11-34`
- bind: `AAZ_PlayerController::SetupInputComponent` -> `UAZ_EnhancedInputComponent::BindAbilityActions` — `Public/Input/AZ_EnhancedInputComponent.h:19-44`
- dispatch: `AZ_PlayerController.cpp:210-235` -> `UAZ_AbilitySystemComponent::AbilityInputTag{Pressed,Held,Released}` — `Private/AbilitySystem/AZ_AbilitySystemComponent.cpp:46-100` (matches `AbilitySpec.DynamicSpecSourceTags.HasTagExact(InputTag)`)
- ability's tag: `UAZ_GameplayAbility::InputTag` — `Public/AbilitySystem/Abilities/AZ_GameplayAbility.h:82`; seeded by `GrantAbilitiesWithInputTag` `AZ_AbilitySystemComponent.cpp:26`

**Equip -> profile tag -> chooser:**
- `ASC->OnWeaponEquipped(tag)` — `Private/AbilitySystem/AZ_AbilitySystemComponent.cpp:137` -> `AddStateTag/RemoveStateTag` (replicated loose tags, Iris-safe) `:116-135`
- lands in `GetOwnedGameplayTags` -> `ChooserContext.OwnedTags` — `Private/Animation/AZ_MoverAnimInstance.cpp:283-284`
- chooser ctx struct `FAZ_v2_ChooserContext` — `Public/Animation/AZ_LocomotionTypes.h:587-662` (OwnedTags @654; Gait/Stance/MovementMode/MovementDirection/bLeftFootDown/SMState/bIsMoving/AimingRotation/RotationOffset)
- `EAZ_RotationMode {OrientToMovement=0, Strafe=1, Aiming=2}` — `AZ_LocomotionTypes.h:78-83`; set in `AZ_HeroPawn.cpp Get_RotationMode():348-374`; foot read `MoverAnimInstance.cpp:280` / `AnimInstance.cpp:549`

**CommonUI equip flow (the path we drive):**
- `UAZ_Inv_CommonUI_EquipmentComponent::OnItemEquipped` — `Private/Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.cpp:71-130` (EquipmentFragment->OnEquip :85; reattach+Weapon.Slot.Primary :88-92; State.Equipped.Weapon.Primary :99; WeaponState->ApplyToASC :109; AbilityFragment->OnEquip :118; OnWeaponEquipped :126). Unequip :133 / Drop :191 reverse. Bound to inventory at :304.
- grant fragment `FAZ_Inv_CommonUI_AbilityGrantFragment` — `Public/InventoryUI/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h:376-389` (AbilitiesToGrant @386); `OnEquip` `Private/.../AZ_Inv_CommonUI_ItemFragment.cpp:328` (per ability: `Spec.GetDynamicSpecSourceTags().AddTag(AbilityCDO->InputTag)` :349 + `ASC->GiveAbility` :351 + handle :352); `OnUnequip` :357 -> `ClearAbility` :368
- `FAZ_Inv_CommonUI_EquipmentFragment` — `...ItemFragment.h:338-373` (EquipmentType tag @372, EquipActorClass @367, EquipModifiers @364)
- **programmatic equip entry:** `UAZ_Inv_CommonUI_InventoryComponent::Server_EquipSlotClicked(ItemToEquip, ItemToUnequip)` — `Public/InventoryUI/AZ_Inv_CommonUI_InventoryComponent.h:55` / `cpp:192` (-> Multicast :198 -> `OnItemEquipped.Broadcast` :203). Find item: `FindFirstItemByTypeTag` `Public/InventoryUI/FastArray/AZ_Inv_CommonUI_FastArray.h:60`/`cpp:96`. Get component: `UAZ_Inv_InventoryStatics::Get_CommonUI_InventoryComponent(PC)` `Public/InventoryUI/Utils/AZ_Inv_InventoryStatics.h:37`/`cpp:22`. Live example (right-click equip): `AZ_Inv_CommonUI_InventoryGrid.cpp:1040`.

**v2 hero & ownership:**
- `AAZ_HeroPawn : public APawn, IAbilitySystemInterface, IAZ_CombatInterface, IMoverInputProducerInterface, IAZ_SandboxCharacterPawn` — `Public/Character/AZ_HeroPawn.h:82`; creates `UAZ_EquipmentManagerComponent` `Private/Character/AZ_HeroPawn.cpp:104` (NOT AAZ_CharacterBase).
- ASC lives on PlayerState (`AZ_PlayerState.h:112-113`), resolved cross-pawn. `OnWeaponEquipped` is on the ASC (reachable from anywhere with the ASC).
- v1 reference primitive: `AAZ_CharacterBase::SetWeaponAndTag(EWeaponType)` -> `ASC->OnWeaponEquipped(Weapon.X)` incl. None — `Private/Character/AZ_CharacterBase.cpp:217-260` (v1 only; the reusable bit is `OnWeaponEquipped`).
- CommonUI inv/equip components are added in **Blueprint on `BP_AZ_PlayerController`** (NOT C++); equipment auto-wires to the possessed pawn mesh via `OnPossessedPawnChange` `EquipmentComponent.cpp:318-325`. **UNVERIFIED that they're actually present/live on the Mover hero — see Open Q4.**

**GA template:** `UAZ_GA_Shoot` (`Public/AbilitySystem/Abilities/AZ_GA_Shoot.*`) — ONE ability, Single/Auto/Burst by data. Montage task `UAZ_AT_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(...)` `Public/AbilitySystem/AbilityTasks/AZ_AT_PlayMontageAndWaitForEvent.h:76-84` (delegates OnCompleted/OnBlendOut/OnInterrupted/OnCancelled/EventReceived).

**Tags:** EXIST — `Weapon.None`(GameplayTags.cpp:263), `Weapon.Pistol/Rifle/Shotgun/SMG/Melee`(264-269), `Weapon.Slot.Primary/Secondary/Sidearm/Melee`(272-275), `Input.Action.PrimaryAttack/SecondaryAttack/MeleeAttack`(197/198/201), `Input.Action.EquipWeaponSlot1/2/3`+`NextWeapon`+`PreviousWeapon`(194-225), `Ability.State.MeleeAttacking`(245), `Ability.Cooldown.Melee`(255), `Combat.Attacking/Aiming/...`(291-296), `Animation.State.Unarmed.*`(385+)/`Melee.*`(429+). **MISSING (add):** `Weapon.Fist` (needed now); `State.Combat.Ready`, `Weapon.Slot.Rifle/Pistol` (later/optional). Tag source: `Public/AZ_GameplayTags.h` + `Private/AZ_GameplayTags.cpp`.

## ASSETS (verified on disk this session)
**MovementAnimsetPro fist starter set** (`Content/MovementAnimsetPro/Animations/InPlace/`, same animset as the current CHT — on `SKEL_SurvivalMan`):
| role | clip |
|---|---|
| enter / exit combat stance (draw / holster) | `Idle2Fists` / `Fists2Idle` |
| combat-ready idle | `Fists_Idle` |
| idle punch L / R | `Fists_Punch_L` / `Fists_Punch_R` |
| moving punch L / R | `Fists_Punch_Move_L` / `Fists_Punch_Move_R` |
| heavy recovery | `Fists_Punch_Heavy2Idle` |
| kicks / hits (later) | `Fists_Kick_Front_L`, `Fists_Kick_Front_Move_R`, `Fists_Hit_Left/Right` |

**FightingAnimsetPro = deep kickboxing moveset for LATER** (`Content/FightingAnimsetPro/Animations/{RootMotion,InPlace}/`, `KB_` prefix; EVERY clip exists in both RM and InPlace folders): jabs/hooks/uppercuts/elbows/knees + combos (`KB_p_Jab_LR_combo`, `Jab_LL`, `Jab_LRL`, `DoubleJab`, `Hook_LR_combo`, `DoubleHooks`, `m_Jab_RLhookRMidKick_combo`), kicks (mid/low/round/back/jump), blocks (`Block_Loop`, `MidBlock_L_Single`), hit reactions + KOs (`Hit_*_Weak/Med`, `LowKO`, `UpperKO`, `TopKO`, `GetUpBack`, `GroundAttack`), footwork (`Sidestep_L/R`, `WalkLeft45`, `WalkRight135`, `SkipBwd`, `TurnL_90`, `Duck`), jump (`Jump_Start/Loop`, `JumpPunch`, `Land`), throwables (`Grenade`, `Mine`, `KnifeThrow`).
- **RISK / must verify in editor:** is FightingAnimsetPro on `SKEL_SurvivalMan`? It's a SEPARATE pack — if not, it needs retargeting before use (MovementAnimsetPro is already on our skeleton). Also confirm `bEnableRootMotion` per clip; author hit-window notifies.

## BUILD SCOPE
- **NOW (fist model):** `Weapon.Fist` tag + `GA_MeleeAttack` (light, L/R, idle vs moving from intent, full-RM) + `UAZ_QuickBarComponent` skeleton + number-key binds + fist item (frags) default-equipped + minimal chooser rows for the fist cells + draw/holster (Idle2Fists/Fists2Idle).
- **NEXT:** heavy (hold-threshold), combos (window + comboIndex), hit-detection notify -> GameplayEvent -> trace -> Damage GE, combat strafe locomotion (the `combat_no_weapon` movement / "lean").
- **LATER:** weapons (same GA, swap data + spawned actor + aiming tier) + FightingAnimsetPro deep moveset (blocks, kicks, KOs, hit reactions, footwork).

### Offline (C++, CLI build) vs Editor split
- **Offline now:** `Weapon.Fist` tag, `GA_MeleeAttack`, `UAZ_QuickBarComponent`. (The pure logic.)
- **Needs editor:** fist inventory item + fragments, default-equip on BeginPlay, chooser rows, FightingAnimsetPro skeleton check, PC-component verification (Q4), RM flags/notifies.

## OPEN QUESTIONS / TO VERIFY (resume here)
- **Q1 (design) — RESOLVED 2026-06-15: equip = ready stance.** Draw fist (`Idle2Fists`) -> fighting stance directly; NO relaxed-fist explore state; the quick-slot selection IS the fists-up toggle.
- **Q2 (design) — RESOLVED 2026-06-15: "lean movement" = combat STRAFE LOCOMOTION.** The `combat_no_weapon` movement = `RotationMode=Strafe` (face threat, never turn the back) + strafe directional loco (L/R/back) while fists equipped. NOT Q/E peek-lean, NOT weight-shift-into-punch. Clips: MovementAnimsetPro strafe set (`*Strafe*`, `RunLt/RunRt`, `WalkBwd/RunBwd`, `StrafeLeftStart/Stop_LU/RU`) now; FightingAnimsetPro footwork later. **Each strafe clip needs `contact_l`/`contact_r` foot curves** (same fix as the fwd loops — see [[feedback_posesearch_branchin_db_sync]] §foot curves). This is the biggest movement chunk; shared with ranged aiming + weapon parity (build once).
- **Q3 (editor):** FightingAnimsetPro skeleton == `SKEL_SurvivalMan`? (retarget if not). + `bEnableRootMotion` per clip.
- **Q4 (editor):** Does `BP_AZ_PlayerController` actually carry `UAZ_Inv_CommonUI_InventoryComponent` + `UAZ_Inv_CommonUI_EquipmentComponent`, live on the Mover hero on possess? (Load-bearing — fists go through this path.)
- **Q5 (editor):** Are weapon items actually in the hero's CommonUI inventory at play time (so the quick-slot has something to bind)?
- **Q6 (minor):** unequip publishes `OnWeaponEquipped(Empty)`, not `Weapon.None` — for slot 0 we likely want to explicitly publish `Weapon.None` (1-line in OnItemUnequipped) so the chooser's empty-hands rows have a tag to key on.

## NEXT ACTIONS (tomorrow)
1. ~~Get user answers on Q1/Q2~~ **DONE 2026-06-15: Q1 = equip=ready; Q2 = lean = combat strafe locomotion.**
2. **Editor up:** verify Q3 (FightingAnimsetPro skeleton), Q4 (PC has CommonUI components live on hero), Q5 (weapon items in inventory).
3. **Offline (can start regardless):** write `Weapon.Fist` tag + `GA_MeleeAttack` + `UAZ_QuickBarComponent`; CLI build.
4. Editor: fist item + fragments, default-equip, chooser rows; PIE-verify `Weapon.Fist` flips in OwnedTags + punch fires.

See also: [[project_combat_system_plan]] (full design), [[project_v2_architecture]], [[project_traversal_system]] (shares trace+warp primitive), [[feedback_posesearch_branchin_db_sync]] (foot curves on any new loco clips), [[project_jump_system_status]].
