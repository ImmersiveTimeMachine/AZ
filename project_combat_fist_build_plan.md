---
name: project_combat_fist_build_plan
description: "LIVE working doc (started 2026-06-15) for the FIST-FIRST combat build + quick-slot/equip skeleton. Decisions locked this session: CommonUI is the canonical equip path (keystone already built there); FIST = weapon #0 (Weapon.Fist, a real weapon, NOT 'no weapon'); equip axis no_fist(Weapon.None) <-> fist(Weapon.Fist) <-> pistol <-> rifle via a slim quick-slot manager (UAZ_QuickBarComponent on PC); number keys 0,1,2 = debug SIM of the future radial/cross UI; for fists equip(draw=Idle2Fists)=ready stance & unequip(holster=Fists2Idle)=relaxed/explore; ONE weapon-agnostic GA_MeleeAttack; full-RM attacks; combat = tags+montages+chooser rows, NOT new SM phases. Assets verified on disk. Resume point + open questions inside. Companion to [[project_combat_system_plan]]."
metadata:
  node_type: memory
  type: project
  originSessionId: resume-2026-06-15
---

# CHALK — Fist-First Combat & Quick-Slot Build Plan
**Status:** ✅ **FIRST WORKING PUNCH (2026-06-17)** — equip + LMB/RMB punch + full-RM moving punch all work in SP PIE. **C++ BUILD PENDING**: Stage-2 RM (`AZ_GA_MeleeAttack`) + the QuickBar `Server_Select` RPC are written but NOT compiled (new `UFUNCTION` ⇒ editor-closed CLI build). Committed as a checkpoint. Companion/refinement of [[project_combat_system_plan]]. **Read the SESSION LOG below first to resume.**

## SESSION LOG — 2026-06-17 (first working punch)
**Net: punch is on screen and advances with RM. Build pending; next = fist anim set + MP polish.**

### What now works (SP PIE)
- **Equip:** tap **`0`** (`AZ_IA_RT_Weapon_0`) → `QuickBar.Select(0)` → grant `BP_GA_Punch_L/R` (+ seed InputTag) → `OnWeaponEquipped(Weapon.Fist)`.
- **Punch:** **LMB**=`Input.Action.PrimaryAttack`→`BP_GA_Punch_L`, **RMB**=`Input.Action.SecondaryAttack`→`BP_GA_Punch_R` (ability `InputTag` ↔ ASC `HasTagExact`).
- Montage plays on slot **`FullBody`**; `bIsMoving` picks idle vs move punch; Stage 1+2 = no glide + forward RM advance.

### Verified project facts (corrected stale assumptions)
- **Active hero ABP = `AZ_ABP_MoverAnimInstance`** (parent `UAZ_MoverAnimInstance`). `AZ_ABP_Hero` / `AZ_ABP_HeroPawn` are INACTIVE — don't edit.
- Active pawn = `AAZ_PawnMoverHeroCharacter`, mesh via **`GetMesh()`**. `AAZ_HeroPawn` is a SIBLING (both `: APawn`), NOT a parent, and is the wrong/inactive class.
- Active IMC = `AZ_IMC_RT_PawnInputs` (UE5.8 stores mappings in **`DefaultKeyMappings`**, not the deprecated `mappings`). `0`→Weapon_0, `1`→Weapon_1, LMB/RMB→Primary/SecondaryAttack.
- InputConfig = `AZ_InputConfig` (`ability_input_actions`) — pre-today only Jump/Run/Crouch were wired; combat was unrouted (nothing to break).

### Input architecture — DECIDED (Model A)
One IMC + **profile-gated ability grants** (NOT per-weapon IMCs). Meaning lives in the ability: **InputTag = which button**, **ability logic = what it does**; the equipped profile's grants decide who answers. User took the clean path: **deleted** `AZ_IA_RT_FireWeapon`/`Aim`, **created** `AZ_IA_RT_PrimaryAttack`(LMB)/`SecondaryAttack`(RMB), filled the 2 InputConfig rows. Per-weapon IMC rejected (pose≠movement; client-only IMC vs server-authoritative grants split) — revisit only if a weapon needs different IA *trigger* config on a shared key.

### Architecture principle added
**One GA per interaction SHAPE — parameterize *within*, separate *across*.** Melee swing = one `GA_MeleeAttack` (punch/kick/weapon/combos via context-key→chooser). Aim/shoot/block/traversal = own GA (different lifecycle). Same discriminator as the RAIL DOCTRINE, one level up.

### Bugs found & fixed today
1. **Invisible punch** — Slot node was `UpperBody`, montages are `FullBody`; a montage only renders through a NAME-MATCHING slot. Renamed node → `FullBody`; chain `BlendStack→PoseHistory→Slot 'FullBody'→Output`.
2. **`bIsMovingLatched` always false** (→ always idle punch). `GetHeroPawnFromActorInfo()` casts avatar to `AAZ_HeroPawn` (sibling ⇒ null on `AAZ_PawnMoverHeroCharacter`). Fixed in GA: cast to `AAZ_PawnMoverHeroCharacter` + `GetMesh()`. ⚠️ `GetHeroPawnFromActorInfo()` is still wrong PROJECT-WIDE (null on the active pawn) — repoint or stop using.
3. **Equip toggled every frame while held** — `OnQuickSlotInput` bound to `ETriggerEvent::Triggered` (fires per-frame for a held key) + `Select` toggles. Fixed with a **Tap trigger on the equip IA** (data fix; bind stays `Triggered`). `ETriggerEvent::Started` is the code alternative.
4. **Glide / foot-slide on the moving punch** — pose overridden, but the Mover kept driving locomotion velocity (pose≠movement). **Stage 1** (done): `ProduceInput_Implementation` zeroes `WorldMove` while ASC has `Ability.State.MeleeAttacking`. **Stage 2** (done, build-pending): `GA_MeleeAttack::ActivateAbility` queues `FLayeredMove_RootMotionAttribute` (Duration=montage len, skip sim proxy, replace-don't-stack via `Mover_AnimRootMotion`), cancel in `EndAbility`. FullBody slot override ⇒ `RootMotionFromEverything` extracts the MONTAGE delta. **Move-punch clips need Enable Root Motion.** (Template: `AZ_MoverAnimInstance.cpp:112-116`.)
5. **Abilities don't trigger on a remote CLIENT** — `QuickBar::EquipSlot` is `HasAuthority`-gated but equip input runs client-only ⇒ no grant on client. Fixed: `Server_Select` RPC + `SelectInternal` split + `SetIsReplicatedByDefault(true)`. Granted spec + seeded InputTag replicate; client LocalPredicted punch then fires.

### GAS tag config on BP_GA_Punch_L/R
`ActivationOwnedTags = Ability.State.MeleeAttacking` (GAS auto add/remove; the signal Stage 1 reads; replaces the manual loose-tag TODO). Optional `ActivationBlockedTags = Ability.State.MeleeAttacking` for one-at-a-time. L/R differ ONLY in the custom `InputTag` + `Hand` fields, NOT these engine containers.

### OPEN / NEXT (tomorrow)
- **BUILD** (editor-closed CLI): Stage-2 RM + the `Server_Select` UFUNCTION are uncompiled (committed as checkpoint). Then 2-player PIE verify.
- **"Position correction" jitter** seen occasionally during the punch — suspect MP proxy / RM correction; investigate after build.
- **NEXT FEATURE: fist anim SET** in the equipped state (fists-up locomotion/stance set via the chooser reacting to `Weapon.Fist`). Needs `Weapon.Fist` visible to the AnimInstance/chooser.
- **Loose-tag replication gap**: `OnWeaponEquipped` uses `AddLooseGameplayTag` ⇒ `Weapon.Fist` NOT replicated ⇒ client anim/camera blind. Switch to `AddReplicatedLooseGameplayTag` for the anim-set pass.
- **HitWindowEventTag + damage**: greenfield (no `Event.*` tag, no sender). Plan: register `Event.Montage.Melee.Hit` + a `UAZ_AnimNotify_SendGameplayEvent`; wire `OnMontageEvent` → trace + Damage GE. Currently a guarded stub (optional).

## GOAL
Build and *iterate* the **FIST** combat model first (punch logic + combat movement), on the cheapest possible equipment, then generalize to weapons by **data, not code** (parity by construction). Build only the minimum **quick-slot/equip skeleton** needed to switch the active profile (fist <-> none <-> weapons). "Concentrate on logic, not design (UI)."

User's words: *"the first equipment is our fist and we will add more and more logic on fist first and then will move to another weapons after we will have a good fist model."* And: *"we need just the basic to simulate all this equipments."*

## LOCKED DECISIONS (this session, 2026-06-15)
1. **CommonUI is the canonical equip path.** (Project has 2 inventory UIs — old UMG `AZ_Inv_*` deprecated, new CommonUI `AZ_Inv_CommonUI_*` active.) Build on the CommonUI equip flow. Do NOT add new equip logic to `UAZ_EquipmentManagerComponent` — its *equip* role is superseded; its world pickup/overlap role is audited separately before any retirement (do NOT delete blindly).
2. **The keystone is ALREADY BUILT in the CommonUI path.** `UAZ_Inv_CommonUI_EquipmentComponent::OnItemEquipped` already (a) grants the item's abilities from `AbilityGrantFragment` *with each ability's `InputTag` seeded* (so they're instantly fireable by the input rig), and (b) publishes the profile tag via `ASC->OnWeaponEquipped(EquipmentType)`. We do NOT build the keystone — we just trigger this flow.
3. **FIST = weapon #0 = `Weapon.Fist`** — a real, first-class weapon profile (sibling of `Weapon.Pistol`/`Rifle`), NOT "no weapon." `Weapon.None` = genuinely empty hands ("no_fist"). `Weapon.Fist` tag must be ADDED (missing today).
4. **Fist is a real inventory item**: `EquipmentFragment(EquipmentType=Weapon.Fist, EquipActorClass=null)` (no mesh — hands already exist) + `AbilityGrantFragment(GA_MeleeAttack)`. Goes through the *identical* CommonUI equip path as every gun → real parity, punch is *equip-granted* (not special-cased default-granted).
5. **Equip axis (the quick-slot):** `no_fist(Weapon.None)  <->  fist(Weapon.Fist)  <->  pistol  <->  rifle ...`
6. **For FISTS, equip = ready stance** (cleaner, free given the clips): equip/draw fist = `Idle2Fists` -> fighting stance (Strafe, ready); unequip/holster = `Fists2Idle` -> relaxed/explore (= no_fist). The **quick-slot selection doubles as the fists-up toggle.** GUNS are different — equipped-but-lowered is a real state, so they keep the full **explore -> combat-ready -> aiming** tier axis. *(CONFIRMED 2026-06-15: equip=ready.)* **REFINED 2026-06-15: fist draw/holster (`Idle2Fists`/`Fists2Idle`) = an SM TRANSITION (like crouch's `TransitionStance`), NOT a montage — fists have no anim-led gameplay event (no weapon-attach frame, no holster-while-walking overlay) so nothing earns a montage. GUN draw/holster STAYS a montage (the "weapon now usable" attach frame + walk-while-holster overlay are anim-led events). See RAIL DOCTRINE below.**
7. **Quick-slot manager skeleton IS needed** (user reaffirmed — the equip/unequip of fist & weapons flows through it). Build a slim **`UAZ_QuickBarComponent` on the PlayerController** (cross-pawn, co-op-safe): `Slots[] -> CommonUI item refs`, `ActiveSlotIndex`, `Select(N)` / `CycleNext/Prev`. On change -> holster old + draw new -> `Server_EquipSlotClicked(newItem, oldItem)`. **Logic only, no widget.**
8. **Number keys `0,1,2,...` = debug front-end that SIMULATES the future real UI** (radial wheel / directional cross — see ref image, STALKER/Metro-style). Keys and the future UI are interchangeable front-ends; both call `Select(N)`. Slot 0 = no_fist, slot 1 = fist, slot 2 = pistol, ...
9. **`GA_MeleeAttack` = ONE weapon-agnostic parameterized ability.** Params latched at activation: `hand` (LMB=L / RMB=R), `weight` (hold>threshold = Heavy/Light), `movement` (`bIsMoving` INTENT = Moving/Idle — NO trace/targeting), `profile` (the ASC weapon tag), `comboIndex`. Selector: `montage = CHT(profile,hand,weight,movement,combo)`. Unarmed = the `Weapon.Fist` profile; weapons inherit by swapping rows/data.
10. **Attacks are FULLY root-motion.** Moving punch = forward-advancing RM (queue an `FLayeredMove_RootMotionAttribute` for the montage window — same Mover RM bridge as loco/jump). Idle punch = in-place, cosmetic.
11. **Combat is NOT new `EAZ_StateMachineState` phases** — it's tags + montages + chooser rows over the existing SM. Profile = ASC tag (from equip); tier = a readiness tag + `RotationMode` switch; chooser keys on `(profile x tier x MovementDirection x gait)`.

## RAIL DOCTRINE — montage vs CHT/SM (converged 2026-06-15)
The discriminator for "montage or chooser?" is **NOT** one-shot-vs-continuous — jump & crouch are one-shot and use NO montage. It is: **who owns gameplay timing — the simulation, or the animation?**
- **Sim-led -> CHT/SM rail (no montage).** Timing comes from sim/state: jump (apex = RM Z-delta; "falling" = MovementMode->Falling; land pose MM-picked), crouch (`TransitionStance` SM state). The action **IS / transitions the whole-body locomotion state**; anim follows physics. This is also the traversal rail (vault/slide/dodge belong here).
- **Anim-led -> montage via GA.** Timing is **authored into the clip**: fire point, **hit window**, **combo input window**, "weapon now usable" frame. Needs anim-frame -> `GameplayEvent` -> ability (the `EventReceived` path in `GA_Shoot`), deterministic start->end playback, GAS replication. Punch/shoot live here; gameplay follows anim.

**"Why not just restrict the CHT so fight can't escape to locomotion?"** That restriction = **containment**, and containment is ONE of four needs. The chooser/SM can do containment but NOT: (a) frame-accurate gameplay events, (b) **sequencing/memory** (combo windows, lockout — the chooser is a *stateless per-frame decision table*, no timeline), (c) replication/lifecycle. Forcing temporal logic into a spatial row-table = combinatorial explosion (`profile x tier x dir x gait x combatState x comboIndex x window x hand ...`) + undebuggable stateless-acting-stateful (the frame-0 fallback hell, see [[feedback_posesearch_branchin_db_sync]]). Building "lock + windows + state + events + lockout" into the chooser = **a worse montage**.

**They compose — three layers, not either/or:**
| layer | fight job | who |
|---|---|---|
| SM state (existing) | which state you're in / containment | locomotion SM |
| CHT | pick the combat pose set (fists idle, strafe combat loco) within the state | chooser |
| montage (GA) | the attack itself (**full-body montage + RM** for fists): hit/combo window, contact event, lockout, replication | `GA_MeleeAttack` |

**Containment for the fist build is FREE — no new SM phase needed.** An attack montage plays on its slot and owns the body for its duration. **Coverage is a per-attack choice:** *upper-body masked* slot (Layered Blend Per Bone from `spine_02`) = legs keep the CHT locomotion (this is shoot-while-moving / later niche jabs); *full-body* slot at full weight = the montage overrides the WHOLE body, CHT base blended to ~0, root motion drives the capsule. **Our fist punch is FULL-BODY** (decision #10 = full-RM): legs included, RM moves you, the moving punch steps into the advance — NOT an upper-body overlay (that would slide a floaty torso over still-strafing legs). The CHT only shows through at the blend-in/out edges. MM never gets to "escape" the punch because **the punch isn't on the MM rail at all.** Between attacks, combat-ready = strafe locomotion (tier tag + `RotationMode=Strafe` + chooser rows) — which IS locomotion, nothing to contain. So **decision #11 holds: no new `EAZ_StateMachineState` phases for the fist build.** (A dedicated SM "Attacking" hold is available LATER only if a full-body attack must suppress locomotion transitions globally — not required now.)

**One-liner:** the CHT/SM decides **what state you're in**; the montage decides **what happens, and when, inside an action**. Restricting the chooser fixes the first and leaves the second untouched — and the second ("when does the hit land, can I chain?") is the whole "good fist model."

## SELECTION MECHANISM — how each action picks its anim (converged 2026-06-15)
**Scope note:** `GA_MeleeAttack` only knows about MELEE (punch). Swap, fire, aim are OTHER abilities (quickbar/equip, `GA_Shoot`, `GA_Aim`). They do **not** all pick a montage — *what an action selects, and how, depends on its rail* (see RAIL DOCTRINE).

### `GA_MeleeAttack` — builds a context key, hands it to a selector
At activation it assembles a small key from gameplay state, then one selector returns one montage:
| key field | value | source |
|---|---|---|
| **profile** | `Weapon.Fist` / Pistol / Rifle | ASC `OwnedTags` (set by equip via `OnWeaponEquipped`) |
| **hand** | L / R | the **InputTag** that fired it (`Input.Action.PrimaryAttack` vs `SecondaryAttack`) — ability bound to both |
| **movement** | Idle / Moving | `ChooserContext.bIsMoving` (INTENT: Trj future-vel + accel, NOT distance) |
| **weight** | Light / Heavy (later) | hold time vs threshold, measured by the ability |
| **comboIndex** | 0,1,2… (later) | the ability's own state (bumped when a chain fires in the combo window) |

`key {profile, hand, weight, movement, combo} -> SelectMontage(key) -> one montage -> PlayMontageAndWaitForEvent`

**Selector has two forms (decision #9):**
- **NOW (offline, no editor):** hardcoded C++ `switch`/map -> the 4 fist clips (`!Moving`+L->`Fists_Punch_L`, `!Moving`+R->`Fists_Punch_R`, `Moving`+L->`Fists_Punch_Move_L`, `Moving`+R->`Fists_Punch_Move_R`).
- **LATER:** a **Chooser Table used as an ASSET PICKER** — `montage = CHT_Melee(profile, hand, weight, movement, combo)`, evaluated **ONCE** at activation (via `AZ_ChooserUtils`/EvaluateChooser), returning the montage asset (NOT a per-frame pose-stream). Same chooser tech as locomotion, different *use*. Adding pistol melee = new rows, no code.

### Generalization — each action builds a similar key, but the RAIL decides the output
| action | ability | rail | selects | based on |
|---|---|---|---|---|
| fist equip/holster | quickbar->equip | **SM transition** | transition clip (`Idle2Fists`/`Fists2Idle`) | target slot/profile |
| gun draw/holster | quickbar->equip | **montage** | draw/holster montage (on EquipmentFragment) | profile + draw vs holster |
| **punch** | `GA_MeleeAttack` | **montage** | a punch montage | profile, hand, weight, movement, combo |
| fire | `GA_Shoot` | **montage** | fire montage (`switch(FireMode)` -> CHT later) | profile, fire mode |
| aim | `GA_Aim` | **CHT pose-set (tier)** | *NO montage* — flips `RotationMode=Aiming` + tier tag; CHT serves the aim pose-set; AO from camera trace | profile + aiming tier |
| locomotion | (no GA — the SM) | **CHT pose-stream + MM** | anim set -> MM pose | SMState, profile, tier, dir, gait, tags |

**Key insight: aim does NOT "play an anim."** It changes a *context field* (`RotationMode` + tier tag) and the CHT picks a different **pose-set** while MM keeps generating the pose (+ camera aim-offset). Same for "combat-ready" — a tier tag flip, not a clip.

### THE PRINCIPLE (one rule for all combat actions)
Every action assembles the *same kind* of context key (profile + intent + tags + action-specific bits). The **output rail** differs:
> **anim-led -> the key picks a MONTAGE** (punch, fire, gun-draw)
> **tier/state -> the key flips a CHOOSER FIELD and the CHT serves a POSE-SET** (aim, combat-ready)
> **transition -> an SM TRANSITION state plays a transition clip** (fist equip, crouch)

Adding the pistol later: *fire* reuses this key-building pattern (own selector), *aim* uses the pose-set rail, *draw* uses the montage rail — none touch `GA_MeleeAttack`, none touch the SM.

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
**BUILT 2026-06-15 — 4 punch montages** in `/Game/AZ/Blueprints/Animation/Montage` (created via Python from the source clips; all on `SKEL_SurvivalMan`, slot `DefaultSlot`, RM-enabled source):
| montage | source clip | len |
|---|---|---|
| `AM_Fists_Punch_L` | `AnimPro_Fists_Punch_L` | 0.800 |
| `AM_Fists_Punch_R` | `AnimPro_Fists_Punch_R` | 0.800 |
| `AM_Fists_Punch_Move_L` | `AnimPro_Fists_Punch_Move_L` | 0.833 |
| `AM_Fists_Punch_Move_R` | `AnimPro_Fists_Punch_Move_R` | 0.833 |
These map 1:1 to `GA_MeleeAttack`'s `PunchIdle_L/R` + `PunchMove_L/R` UPROPERTYs (assign in the `BP_GA_Punch_L/R` children).
- ⚠️ **GATING: the AnimBP must have a montage Slot node named `DefaultSlot`** on a FULL-BODY pose path (so the RM punch overrides locomotion). Without a matching slot node the montage plays nothing on screen. The v2 graph is BlendStack+chooser — confirm/add a `DefaultSlot` slot before PIE. (Slot name is editable on the montage if the graph uses a different name.)
- Python montage-creation gotchas learned: `skeleton` + `AnimSegment.start_pos` are READ-ONLY (skeleton set via `AnimMontageFactory.target_skeleton`); `composite_sections`/`loop_count`/`link_value` not exposed (a "Default" section auto-creates); montage length does NOT recompute on `set_editor_property` (only in PostLoad) → set it via `montage.controller.set_play_length(len)`.
- Source clip names carry an **`AnimPro_`** prefix; actual folder is `/Game/Assets/RTG_AZ/MovementAnimsetPro` (NOT `/Content/MovementAnimsetPro/...InPlace`).

**MovementAnimsetPro fist starter set** (`/Game/Assets/RTG_AZ/MovementAnimsetPro`, `AnimPro_` prefix, same animset as the current CHT — on `SKEL_SurvivalMan`):
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
- **Q4 — RESOLVED 2026-06-15 (CONFIRMED ✓).** `BP_AZ_GameMode` → PC=`BP_AZ_PlayerController` (active), DefaultPawn=`AZ_BP_PawnMoverHeroCharacter` (active Mover pawn, has `AZ_PawnMoverComponent`). **`BP_AZ_PlayerController` carries BOTH CommonUI components**: `BP_AZ_Inventory_CommonUIComponent` (inventory) + `BP_AZ_Inv_CommonUI_EquipmentComponent` (equipment) — BP subclasses, so the C++ `FindComponentByClass<UAZ_Inv_CommonUI_InventoryComponent>()` (AZ_PlayerController.cpp:29) resolves. So **Phase 2 (real Server_EquipSlotClicked equip path) is VIABLE.** The active pawn has NO equipment component (correct — they're on the PC, attach to possessed pawn via `OnPossessedPawnChange`). ⚠️ `AZ_BP_HeroPawn` + `AZ_BP_Hero` are OLDER/INACTIVE hero BPs (still carry the superseded `AZ_EquipmentManagerComponent`; `AZ_BP_Hero`=v1 CMC). GameMode uses neither — don't edit them.
- **Q5 (editor):** Are weapon items actually in the hero's CommonUI inventory at play time (so the quick-slot has something to bind)?
- **Q6 (minor):** unequip publishes `OnWeaponEquipped(Empty)`, not `Weapon.None` — for slot 0 we likely want to explicitly publish `Weapon.None` (1-line in OnItemUnequipped) so the chooser's empty-hands rows have a tag to key on.

## NEXT ACTIONS (tomorrow)
1. ~~Get user answers on Q1/Q2~~ **DONE 2026-06-15: Q1 = equip=ready; Q2 = lean = combat strafe locomotion.**
2. **Editor up:** verify Q3 (FightingAnimsetPro skeleton), Q4 (PC has CommonUI components live on hero), Q5 (weapon items in inventory).
3. **Offline (can start regardless):** write `Weapon.Fist` tag + `GA_MeleeAttack` + `UAZ_QuickBarComponent`; CLI build.
4. Editor: fist item + fragments, default-equip, chooser rows; PIE-verify `Weapon.Fist` flips in OwnedTags + punch fires.

See also: [[project_combat_system_plan]] (full design), [[project_v2_architecture]], [[project_traversal_system]] (shares trace+warp primitive), [[feedback_posesearch_branchin_db_sync]] (foot curves on any new loco clips), [[project_jump_system_status]].
