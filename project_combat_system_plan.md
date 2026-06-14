---
name: project_combat_system_plan
description: "v2 melee/combat design + phased plan (2026-06-14). ONE weapon-agnostic GA_MeleeAttack parameterized by {hand, weight, range, profile, comboIndex}; \"unarmed\" = the Weapon.None profile; weapon parity by construction (same 5 params, different selector rows). Locked decisions: explicit fists-up toggle stance, hold-threshold heavy, moving-vs-idle punch from movement INTENT (not distance), full-body root-motion attacks; cone-trace+warp deferred as a traversal-shared optional soft-aim. KEYSTONE prerequisite: equip must publish a profile tag to the ASC. NOT new SM phases — combat is tags + montages."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
---

Design settled 2026-06-14 (collaborative). NOT yet built — this is the plan. Builds on [[project_v2_architecture]] (GAS owns logic, Chooser owns anim selection, tags bridge; "parameterize, don't proliferate").

## Core shape — do NOT build "GA_Fight"
ONE weapon-agnostic **`GA_MeleeAttack`** (extends `UAZ_GameplayAbility`). Everything is a PARAMETER it computes; a Chooser selector maps the tuple → montage. "Unarmed" is just the `Weapon.None` profile — a knife/sword later is `Weapon.Slot.Melee` with the SAME params. Weapon parity falls out by construction; adding a weapon's melee = data + selector rows, not new code.

| Mechanic | Param | Source |
|---|---|---|
| no weapon → punch | **profile** = `Weapon.None` (vs `Weapon.Slot.*`) | the KEYSTONE ASC tag; also gates which ability each button fires |
| LMB / RMB | **hand** = L / R | which Input.Action.{Primary,Secondary}Attack activated |
| hold LMB longer | **weight** = Light / Heavy | input hold past threshold inside the GA (hold-threshold, NOT charge-release — phase-2 upgrade) |
| intend to move while punching | **movement** = Moving / Idle | `bIsMoving` (intent) LATCHED at GA activation — reuses existing context, NO trace/targeting/enemy needed (2026-06-14 decision; simpler + player-driven + fits CHALK's deliberate melee). Distance-to-enemy cone-trace is an OPTIONAL later soft-aim assist, not core. |
| chained presses | **comboIndex** | combo window in the GA |

Selector: `montage = CHT(profile, hand, weight, movement, comboIndex)`. "Moving punch" = a full-body forward-ADVANCING root-motion punch (movement=Moving, from INTENT), carrying you where you steer (you always face camera-forward) — NOT upper-body-while-walking, and (base) NOT auto-homed to an enemy. So attacks are full-body root-motion; no spine_02 upper-body layer needed for melee (that layer is a later concern for gun-fire-while-moving).

**Attacks are FULLY root-motion (committed — the attack OWNS the capsule for its window, no mid-swing steering; matches the full-body "weighty" choice).** Moving (movement=Moving): a forward-advancing punch whose RM carries you where you steer; driven by an `FLayeredMove_RootMotionAttribute` queued for the montage window — the SAME Mover RM bridge as locomotion transitions / the jump rise, NOT the SM-transition auto-path (a DefaultSlot montage isn't an SM phase, so the GA/AnimInstance must queue the RM move itself). Idle (movement=Idle): authored in-place → plays cosmetically, no RM move needed. See [[project_root_motion_mode]].

**Shared "trace → warp-RM-to-target" primitive (combat ↔ traversal):** the cone-trace + AnimationWarping-to-target mechanism is NOT built for combat — it is the core of the TRAVERSAL system ([[project_traversal_system]]: trace ahead → chooser-by-context → warped RM action | physics-jump fallback) which we need anyway for vault/mantle/hurdle over obstacles. So when traversal builds that primitive, the OPTIONAL combat soft-aim (warp a Moving punch onto an enemy in a forward cone) just reuses it — same "warp RM onto a traced target" call (ledge for traversal, enemy for combat). Combat base stays intent-driven; the warp assist is a later, cheap add once traversal exists.

## KEYSTONE (prerequisite for ALL weapon-aware anim + ability gating)
Today `UAZ_EquipmentManagerComponent` only sets a private replicated `ActiveSlotTag` + grants the weapon's abilities — it does NOT push a tag to the ASC (verified: no `AddLooseGameplayTag`, cpp:31/252-276). The chooser reads `ChooserContext.OwnedTags` straight from the ASC (`Cached_Pawn->GetOwnedGameplayTags`, `AZ_MoverAnimInstance.cpp:283`). So the anim layer + ability gating are currently BLIND to what's equipped. FIX: in `HandleActiveSlotChanged_Server`, `AddReplicatedLooseGameplayTag(Weapon.Slot.X)` when armed / `Weapon.None` when empty. After this the chooser AND every ability's required/blocked-tag gating see the profile for free.

**Equip is itself a parameterized ability** — ONE `GA_EquipSlot` (slot via EventData/input), triggered by NUMBER KEYS now (1=unarmed/holstered→`Weapon.None`, 2=pistol, 3=rifle, …) and a CommonUI loadout widget LATER (both are front-ends to the same ability — the widget triggers the same GA/equip path). The equip flow (GA → EquipmentManager slot change) is WHERE the profile tag is published, so the keystone lives at the end of equip. "Unarmed" = selecting the empty/holstered slot (code-default profile), NOT a fists inventory item.

**Why ONE GA_EquipSlot, not GA_EquipPistol/Rifle/Knife (asked 2026-06-14):** three layers must not be conflated — (1) the equip ACTION (one ability), (2) what you HOLD = state owned by EquipmentManager + the profile tag (not an ability), (3) what the weapon LETS YOU DO = fire/aim/melee = separate abilities GRANTED ON EQUIP (THIS is where "per equipment" correctly lives). The equip action is identical LOGIC across weapons (holster→draw montage→attach mesh→switch slot→grant abilities→publish tag); only DATA differs (montage/mesh/ability set, already in each weapon's ItemDefinition). Decisive test: **parameterize when the difference is DATA, separate classes when it's LOGIC.** Clincher: NUMBER KEYS MAP TO SLOTS, NOT TYPES (key 2 = "equip what's in slot 2", any weapon) — so per-type equip abilities don't even fit the input; per-slot ones are N copies differing by one constant. Project precedent: `GA_Shoot` is already ONE ability (Single/Auto/Burst, params from the weapon), and the EquipmentManager already grants per-weapon abilities from `ItemDefinition`. New weapon = a data-asset entry, not a C++ class. Escape hatch: subclass `GA_EquipSlot` for a genuine LOGIC outlier (e.g. two-handed unsling).

**Lyra precedent (asked 2026-06-14) — validates this + refines "equip as ability":** Lyra has NO per-weapon equip abilities and equip is NOT an ability — it's a COMPONENT op. Stack: `ULyraInventoryItemDefinition` (data + fragments) → `ULyraQuickBarComponent` (slots + `ActiveSlotIndex`; number keys `IA_QuickSlot1..3` bound to `SetActiveSlotIndex(N)` — a method, not a GA) → `ULyraEquipmentManagerComponent` equips via `ULyraEquipmentDefinition` (pure data: `AbilitySetsToGrant` + `ActorsToSpawn` + instance class) which GRANTS the weapon's `ULyraAbilitySet` while equipped. Weapon ACTIONS = abilities granted on equip, parameterized by the weapon INSTANCE (ONE `ULyraGameplayAbility_RangedWeapon` for all guns, reads stats via `GetAssociatedEquipment`), not one-per-gun; categories differ, within-category is data. Weapon ANIMS = per-weapon LINKED ANIM LAYER (`LinkAnimClassLayers`) — the ONE place AZ v2 diverges (chooser-by-profile-tag instead). AZ already mirrors Lyra (Fragment/Manifest inv + EquipmentManager grants from ItemDefinition). **Refinement: `GA_EquipSlot` is OPTIONAL — justified ONLY if equip needs a holster/draw MONTAGE + cancel/cooldown (deliberate CHALK swap); if switching is instant, bind number keys straight to the EquipmentManager (Lyra-style) and skip the ability.** Keystone tag-publish lives in the EquipmentManager regardless.

## Contextual buttons (handled by the keystone tag, not new code)
Same physical buttons mean different things per profile, via each ability's activation gating on the profile tag:
- `Weapon.None` → punch abilities activate (LMB=L hand, RMB=R hand); shoot/aim blocked.
- `Weapon.Slot.Rifle` → LMB=fire / RMB=aim; punch blocked.
"I choose to fight, no weapon, I can punch" ≡ the `Weapon.None` tag gates punch on / guns off.

## Tags (mostly already exist — verify catalog at build)
Exist: `Input.Action.{Primary,Secondary,Melee}Attack`, `Ability.State.MeleeAttacking`, `Ability.Cooldown.Melee`, `Weapon.Slot.{Primary,Secondary,Sidearm,Melee}`, `Weapon.None`, `Animation.State.Unarmed.{Idle,Walking,Running,Attack.Light,Attack.Heavy}`, `Animation.State.Melee.Attack.{Light,Heavy}`, `Combat.MeleeSwing`. LIKELY NEW: a fists-up "combat-ready" tag (reuse a `Combat.*` or add `State.Combat.Ready`) + a fight-toggle Input.Action + IMC mapping. The Unarmed-vs-Melee anim-tag split already encodes "one action, varied flavor".

## Explore / Combat-ready / Aiming = THREE TIERS (refined 2026-06-14c — supersedes "aim = the fight axis")
User corrected: AIM means ADS/ZOOM (RMB), which is NOT the fight-trigger. So there are THREE tiers (== the v2 smoothing table already: Exploration 0.4s / Combat 0.15s / Aiming 0.05s):
| Tier | Pose | Rotation | Entered by |
|---|---|---|---|
| **Explore** (default) | lowered/hands-down | OrientToMovement | default; AUTO-returns when idle |
| **Combat-ready** | raised/fists-up | **Strafe** (face threat) | **ATTACKING** |
| **Aiming** | ADS/zoom | Strafe, tightest | **RMB** (ranged only) |
- **Fight-trigger = ATTACKING, not aim.** Any combat input raises you to ready; idle a few sec -> auto-lower to Explore. NO manual stance toggle (the old "fists-up toggle" is dropped). The FIRST combat input from Explore raises you into Combat-ready as part of doing it.
- **Per-profile inputs:** Unarmed: LMB=left punch, RMB=right punch (no zoom), hold=heavy. Pistol/Rifle: LMB=fire (hold=auto/burst), RMB=aim/ADS zoom (->Aiming tier). Knife: LMB=light, RMB=heavy/block(later), hold=heavy. (So RMB = aim for ranged, = right-hand attack for unarmed.)
- **TWO ORTHOGONAL AXES stay:** EQUIP (what's in hand; `GA_EquipSlot`+draw/holster montage; default lowered=Explore) is separate from the TIER (Explore/Combat-ready/Aiming, driven by attack/aim).
- **"aim/no-aim anim set" per profile:** each profile has an Explore set (orient, lowered) + a Combat/Strafe set (face-camera, directional, up); Aiming adds the ADS pose/zoom. Chooser picks `(profile × tier × MovementDirection)`.
- **REUSES EXISTING:** `EAZ_RotationMode {OrientToMovement(explore), Strafe(combat+aim)}`, `GA_Aim` = the RMB ADS/zoom (Aiming tier), the 3-tier smoothing table, rifle aim/zoom infra (AO_Rifle_Aim camera-trace, Relaxed/Aim socket interp, FOV/boom per stance). Not new machinery.
- **TAG MODEL:** **profile** (`Weapon.None`/`Weapon.Slot.X`) + **tier/readiness** (Explore / Combat-ready / Aiming). Chooser + ability gating read both. Abilities (attack) available in Combat-ready/Aiming.
- ATTACKS: GA in the ready tiers; moving-vs-idle punch from movement intent; hand from LMB/RMB (unarmed); weight from hold.
- OPEN (feel, PIE): first-input-from-Explore = raise-ONLY (2nd click attacks) vs raise+attack-in-one (recommended); aim = hold vs toggle; auto-lower idle timeout.
- DEBUG (now): number keys equip-from-inventory (exists) = EQUIP axis only (what's in hand, lowered/Explore); slot system + rebindable keys later.

## Combat is NOT new EAZ_StateMachineState phases
(The SM is delicate — see [[project_jump_system_status]] 2026-06-14.) Combat = two orthogonal things layered over the existing SM:
- **A. Fists-up stance + STRAFE locomotion** = a LOCOMOTION VARIANT + a ROTATION-MODE switch. In combat the character must NOT turn its back to the threat → switch `RotationMode = Strafe` (face camera/threat, move any direction) instead of exploration's orient-to-movement. Same idle/loco/start/stop SM phases; the chooser picks fists-up + STRAFE directional clips via a column on the combat-ready tag, selected by the existing `MovementDirection` (F/B/LL/LR/RL/RR) context. The strafe set (strafe L/R 45/90/135 loops, strafe starts/stops, backpedal) ALREADY EXISTS in MovementAnimsetPro (`AnimPro_*Strafe*`, `RunLt/RunRt`, `WalkBwd/RunBwd`, `StrafeLeftStart/Stop_LU/RU`); wiring = chooser rows + EACH clip needs `contact_l`/`contact_r` foot curves (same fix as the fwd loops, [[feedback_posesearch_branchin_db_sync]] §foot curves). `EAZ_RotationMode::Strafe` + MovementDirection already exist for exactly this; NO new SM phases. **Shared, not combat-only:** Strafe mode + this directional set also serve ranged AIMING and weapon parity — build once. This is the BIGGEST chunk of combat work.
- **B. Attack actions** = one-shot montages from `GA_MeleeAttack` on the full-body `DefaultSlot` (the only slot today; aim-offset auto-disables when it's hot — `AZ_AnimInstance.cpp:535`), over locomotion.

## Flow
`Input.Action.{Primary|Secondary}Attack` → `GA_MeleeAttack` (req `State.Combat.Ready`(+`Weapon.None`), blocked-by `Ability.Cooldown.Melee`, grants `Ability.State.MeleeAttacking`) → cone-trace target+distance → read hand+hold → selector montage(profile,hand,weight,range,combo) → play DefaultSlot (Lunge warps to target) → notify hit-window → SendGameplayEvent → GA trace/overlap → Damage GE → re-press in window → comboIndex++.

## Phased build order
0. **Equip foundation + keystone** — `GA_EquipSlot` (parameterized) bound to keys 1/2/3 (unarmed/pistol/rifle/…); drives the EquipmentManager slot change which PUBLISHES the profile tag to the ASC (`Weapon.None`/`Weapon.Slot.X`). Verifiable: press 1/2/3 → tag flips in `OwnedTags`. Unblocks anim + gating. The CommonUI loadout widget is a LATER front-end to the same ability.
1. **Combat stance + STRAFE locomotion** (the biggest chunk) — toggle input → combat-ready tag → (a) `RotationMode=Strafe` (face threat, never turn back), (b) fists-up + STRAFE directional rows (loops + starts/stops + backpedal) keyed on (combat tag + MovementDirection + gait + foot). Clips exist in MovementAnimsetPro; add `contact_l`/`contact_r` curves per clip. Reuses SM + RotationMode + MovementDirection; zero new SM phases. Shared with ranged aiming.
2. **`GA_MeleeAttack` MVP** — weapon-agnostic GA; bind Primary/Secondary→hand; hold-threshold→weight; hardcode unarmed light in-place punch to prove input→montage→cooldown→cleanup end-to-end. (`UAZ_AT_PlayMontageAndWaitForEvent` exists; GA_Shoot is the template, `AZ_GA_Shoot.cpp:114`.)
3. **Moving vs idle from intent** — at GA activation latch `bIsMoving` → Moving (forward-advancing RM punch; queue the RM-attribute move) vs Idle (in-place, cosmetic). NO trace/targeting. *(Deferred optional: warp-to-enemy soft-aim on the Moving punch — reuses the traversal trace+warp primitive once that exists; add only if playtests show whiffing.)*
4. **Selector Chooser** — `CHT` keyed on (profile,hand,weight,movement,combo) → montage; move choice out of the GA. Unarmed rows first.
5. **Hit detection/damage** — notify→event→melee trace→Damage GE. DEP: confirm Health attribute + damage exec exist (`AZ_HeroAttributeSet`/`AZ_AttributeSet` present).
6. **Combos** — window + comboIndex chaining.
7. **Weapon parity proof** — equip melee weapon → `Weapon.Slot.Melee` gates the same GA → selector returns weapon swings via that profile's rows. No new ability.
8. *(later)* upper-body `Layered Blend per Bone (spine_02)` linked layer — gun-fire/move-while-attacking; serves all profiles.

## Open deps to confirm at their phase
- Health/Damage AttributeSet + Damage GE/exec (phase 5).
- `UMotionWarpingComponent` on the pawn — ONLY for the deferred optional warp-to-enemy assist; base intent-driven combat needs none. (Comes with the traversal workstream anyway — [[project_traversal_system]].)
- New fight-toggle InputAction + IMC entry + combat-ready tag (phase 1); number-key (1/2/3) InputActions + equip event tag (phase 0).

See [[project_v2_architecture]], [[project_traversal_system]] (shares the trace+warp primitive), [[weapon_swap_architecture]], [[project_gas_gameplay]], [[project_jump_system_status]].
