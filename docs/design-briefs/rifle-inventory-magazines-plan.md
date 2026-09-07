# Rifle, detachable magazines, inventory and HUD implementation plan

Status: proposal based on source and live read-only asset inspection, 2026-09-06 session. No gameplay code or assets were changed for this investigation; no build, PIE or editor tests were run.

## 1. Intended result and confirmed decisions

The first complete rifle slice is: find a rifle and magazines, pick them up into CommonUI inventory, assign/select the rifle, draw/holster and switch between rifle and fists, aim/fire, reload using real magazine items, and drop/recover the same equipment without changing its ammunition. Standing and crouching are supported throughout. HUD and inventory show the same committed gameplay state.

Confirmed by the user:

- Fists/punch are a weapon profile, and later weapons should extend that foundation through weapon-specific data and action implementations.
- Use the existing functional CommonUI inventory, which is not currently connected to the active gameplay input/pickup path.
- Ammunition uses **detachable magazines as separate inventory items**, each retaining its remaining rounds.
- Use HQUI_ProgressBars and ProHUDV2_Horror for presentation, with the old AZ UI as a reference.
- RifleAnimsetPro is the intended animation family. Its exact retargeted location still needs clarification; verified asset findings are below.
- Preserve the active Mover MetaHuman hero, crouch, and exploration-only sprint. Fight mode must not enable sprint.
- **Animation architecture decision:** retain the existing CHT + locomotion state machine + BlendStack, with Motion Matching where already used. Add rifle as a weapon profile through that architecture. The user considered classical BlendSpaces/root-motion montages and chose to retain the current approach.
- **Rifle modes:** rifle selected + exploration uses the lowered/exploration rifle set; aiming enters fight mode + rifle and uses the aimed rifle set. Both modes have standing and crouched variants. Selecting/owning the rifle alone does not force fight mode.
- This is planning, not authorization to start implementation. Ask before editor tests; otherwise the user runs PIE and the agent reads the logs. Do not add automated tests without an explicit request.

Recommended initial product defaults, still proposals: pickup adds ownership without forcibly switching the active weapon; reload selects the fullest compatible spare magazine with a stable tie-break; keep old partial and empty magazines; reject a reload if its final storage arrangement cannot fit rather than silently discard a magazine. Start with loaded/partially loaded magazine pickups. Loose-round packing, manual magazine selection, mixed ammunition and a separate chamber/+1 model can follow without changing magazine identity.

## 2. What already exists, and what must be joined

| Area | Verified reusable foundation | Work required for the active hero |
|---|---|---|
| Weapon selection | QuickBar, profile tags, input-tagged GAS grants, working fist/punch/heavy-strike profiles | QuickBar is still an inline grant path, independent of inventory. Make selection request the canonical equipment operation. CycleNext/Prev are empty. |
| Inventory | CommonUI component, FastArray items, manifests/fragments, grid, descriptions, popup/drop/split, equipment component | Capacity currently comes from the widget; add authoritative placement/capacity and committed change events. Preserve current controller ownership. |
| Controller wiring | Both CommonUI inventory/equipment components are on the live controller; the inventory screen is assigned on its component template | I is mapped but not dispatched to the menu; OpenInventoryAction is assigned but not bound. The active hero grants Jump/Run/Crouch/Sprint, not inventory/interaction abilities. E has an input-tag mapping but still needs a functioning interaction handler. |
| Pickup | CommonUI rifle pickup template, item actors/components, world-item GUID pattern | Pickup only accepts the old AAZ_HeroCharacter and acquires its target only when the old HUD exists. Mover is an APawn. Target acquisition must work independently of HUD visibility. |
| Equipment | Carry/equip/drop representation, fragment ability grants with actor source, attribute application | Body-mesh resolution casts ACharacter. The existing multicast equips the new item before unequipping the old, corrupting state/order. Fix ownership and ordering before adding guns. |
| Rifle | AZ_BP_Rifle, M16 skeletal mesh, carry/hand and left-grip sockets | Rifle has no Muzzle socket. Gameplay tags, grants, effects and several settings are incomplete. AZ_M16 is only an item actor, not the gameplay weapon. |
| Fire/aim | GA_Shoot hitscan, target data, single/auto/burst structure, montage/sound/VFX hooks; old aim/camera examples | Fire/aim presentation depends on old ACharacter/UAZ_AnimInstance. Shoot hardcodes RifleClipAmmo and the old Data.Damage tag; modern damage consumes SetByCaller.Damage. Fire cadence/spread have duplicate configuration sources. |
| Reload/magazines | Reload input action, source animations and ammo-description widgets | No reload ability or AZ magazine pickup implementation was found. Current per-rifle reserve counters are not a detachable-magazine model. |
| Animation | Broad rifle source content, a separate retargeted Rifle_01 set, existing rifle BlendSpaces/AO/fire montage | Active MHC graph has a fist overlay and FullBody slot only. Rifle pose/action layers, prepared rifle MM pools, correct slots, attachment/reload events and stance coverage are required. |
| HUD | Both requested kits installed; CommonUI HUD base and old AZ ammo-description visuals | Current controller creates the old inventory HUD. Add an owning-player presentation bridge and bind current combat health and magazine state. |
| Persistence | Fragment state survives some old equip/drop operations; world actors have a GUID pattern | CommonUI items have no stable instance ID/location model, and no inventory save/load implementation was found. Session state conservation is required now; disk persistence is a separate later adapter. |

The existing CommonUI rifle pickup is a demonstration manifest: 3x2 footprint, stackable with maximum 3, static 23/76 ammo text, empty ability grants, a 30/90 clip/reserve snapshot, and Item.Type.Weapon.Rifle used where the ASC profile expects Weapon.Rifle. Its item component replication is off. Reuse its mesh/icon/layout as a template; author a correct non-stackable rifle item rather than enable those values unchanged.

## 3. One owner for each fact

Keep and extend CommonUI. Do not activate its equipment grants alongside QuickBar grants and the weapon actor's older grant/ASC/ammo path.

| Fact | Canonical owner | Consumers |
|---|---|---|
| Rifle/magazine ownership, identity, location and magazine rounds | Existing player's CommonUI inventory model, extended with instance/location state | Equipment, reload/fire, inventory UI, HUD, later save adapter |
| Static weapon/magazine settings | Editor-assigned definitions referenced by manifests/fragments | Abilities, equipment, animation and presentation |
| Selected/active rifle and equip transaction | CommonUI equipment component, extended as the single coordinator | QuickBar, inventory commands, ability source validation, animation |
| Ability lifetime, input eligibility, damage/effects | Player ASC and generic action abilities | Movement, animation, HUD |
| Weapon skeletal mesh/rig, mechanical animation instance, attachments, muzzle/grip transforms and VFX/audio | Equipped weapon actor | Synchronized weapon presentation; ammunition remains on the real magazine instance |
| Locomotion and crouch physics | Mover | Weapon-aware animation reads the same authoritative stance/gait |
| Health | AZ_VitalsAttributeSet.Health/MaxHealth on the current combat ASC | HQUI health bar and damage feedback |
| Numbers and indicators on screen | Derived presentation state | ProHUD/HQUI/CommonUI widgets; UI cannot change ammunition directly |

QuickBar and inventory equipment controls must call the same operation. QuickBar holds references to real inventory/loadout entries, not copies of rounds or another ability list. Fists remain an intrinsic, non-droppable entry in that same equipment/action contract, without a backpack footprint or spawned gun actor.

### Item and definition data

Proposed fields extend the existing manifest/fragment pattern; exact class names are implementation details to finalize in phase 1.

- **Weapon definition:** weapon profile, actor/presentation class, permitted actions, firearm/melee capability, magazine-family compatibility, ammo type, supported fire modes, cadence/damage/spread/recoil settings, sockets, character animation profile, weapon skeletal mesh/AnimBP/mechanical animation set and cues. Weapon.Fist has melee actions and no ammunition source; rifle has aim/fire/reload. A new gun should usually be definition/content work. A fundamentally different action, such as a shotgun's per-shell loading, can specialize the reload policy.
- **Rifle instance:** stable ItemInstanceId, definition reference, location, inserted MagazineInstanceId (optional), and genuinely per-instance settings such as selected fire mode. Do not use the word Rifle as its identity: two identical rifles can contain different magazines.
- **Magazine definition:** compatible magazine family, ammo type, capacity, grid footprint, icon and world/hand presentation. Weapon category alone does not establish magazine compatibility.
- **Magazine instance:** stable ItemInstanceId, definition reference, current rounds and location. Non-stackable even when two full magazines have equal round counts.
- **Location:** backpack grid, loadout/held weapon slot, a specific rifle's magazine slot, or world pickup payload. An inserted magazine is visible in rifle details/equipment UI and does not also occupy a backpack cell.

Existing world-item GUID code is a reference to reuse, with identity explicitly transferred into the CommonUI item and preserved through drop/re-pick. A dropped rifle must carry its nested magazine record, not just an unresolved magazine ID.

Any temporarily retained ASC clip-ammo field is a synchronized compatibility mirror of the inserted magazine. Shooting writes through the magazine owner, then publishes the committed state. Remove per-rifle reserve restoration from the new path. Spare availability is computed from actual compatible magazine items. Empty magazine and no magazine are distinct states.

### Conservation example

Pick up a rifle containing magazine A with 12 rounds and a spare magazine B with 30. Fire five shots: A has 7. Reload: B is inserted with 30 and A returns to inventory with 7. Fire two shots: B has 28. Switching to fists and back changes neither magazine. Dropping the rifle transfers the rifle and B/28 into its world payload; A/7 stays in inventory. Picking the rifle up again restores that exact relationship. Total ammunition is 35 after the seven shots; switching, reload and pickup create none.

## 4. Pickup, equip and reload transactions

### Pickup/drop

Use the existing interaction/item component route, with a Mover-compatible pawn/controller interface and a selected interactable independent of the HUD. Before acquisition, validate the actual world item, distance/availability, ownership and inventory capacity on the authoritative side. Commit the item data before publishing the pickup notification or destroying the world representation. Full inventory leaves the world item intact.

Drop performs the reverse transfer. Prepare a valid world payload and spawn location first; failed world spawn leaves ownership unchanged. Drop of a rifle includes its inserted magazine. Spare magazines drop independently. Only loadout-assigned weapons need carried meshes; owning several backpack rifles must not attach several actors to one back socket.

### Equip/switch

Use one equipment transaction/generation ID and exact item identities across callbacks:

1. Validate the requested owned item/loadout slot and readiness policy before disturbing the current item.
2. Respect the outgoing action's commitment. Queue the requested switch while a fist/paired strike is committed; at its allowed cancel boundary, end that outgoing profile's actions and release root-motion/paired-interaction ownership. Cancel the old firearm's fire, aim and reload through their lifecycle; stop loops/target tasks, resolve reserved magazines and clear pending weapon input. Do not clear unrelated locomotion. Death/grab take priority and must not automatically resume a queued draw while locked out.
3. Holster/release the old active representation and grants in the correct order. Ownership and inserted magazine remain on that rifle.
4. Draw/attach the new representation at an authored attachment event.
5. Publish the usable active identity/profile and corresponding grants at the agreed ready event. Abilities validate this active identity, not merely a non-null source actor.
6. Publish committed equipment changes to QuickBar, inventory, animation and HUD.

Repeated requests must not duplicate grants or tags. Re-selecting the current slot follows the explicit holster/toggle policy. Rapid changes retain at most the latest desired item rather than starting overlapping draws. Stale montage callbacks must not attach, fire or reload a later weapon. If an interrupt happens after the old weapon is holstered, settle in a defined holstered state or resume the latest validated selection; do not pretend the old item is still usable.

### Detachable-magazine reload

Reload is a generic firearm ability using the active rifle's policy and stance-specific montage:

1. Select the fullest compatible eligible spare, with stable tie-breaking. For automatic selection, eligible means owned, accessible, unreserved, compatible and containing rounds; empty magazines remain real items but are not chosen automatically. An already full inserted magazine makes ordinary reload a no-op. Reject when no eligible compatible spare exists, the action is already busy, or the resulting inventory arrangement is invalid. A rifle with no inserted magazine can load a spare normally. A user-selected magazine can be supported later through the same command.
2. Reserve the selected magazine and, if an old magazine is inserted, its return destination. Account for the space freed by removing the incoming magazine; different footprints still require a real capacity check. An initially empty magazine slot needs no old-magazine return reservation.
3. Play the chosen standing/crouched reload. Mag-out, stow and hand-held props are presentation events; they never create extra inventory items.
4. At the authoritative magazine-in/commit event, atomically swap magazine identities/locations. Round counts are unchanged. Duplicate event delivery is a no-op after the transaction commits.
5. At the ready event, release the action lock. A bolt/chamber animation beat may be required visually even when the first gameplay model has no separate chamber round.

Before commit, cancellation retains original inventory/inserted-magazine state and restores presentation. After commit, cancellation retains the completed swap. Switching, opening inventory, crouch/stand changes, death/grab and drop must use this same cancellation rule. No reserve or magazine can be stranded in an invisible temporary container.

Initial recommendation: latch the reload stance at activation; standing and crouched reload both work. If stance changes during a reload and the authored layer cannot safely accommodate it, cancel through the transaction instead of restarting the clip and replaying magazine events. Allow walking reload only after the chosen upper-body reload preserves the lower-body pose correctly; this is an explicit acceptance gate, not an assumed property of the source clip.

## 5. Weapon-specific behavior without breaking fists

Share the weapon definition, equip/grant lifecycle, input intents, damage contract and cancellation rules. Do not put shooting into GA_MeleeAttack. Aim, fire, reload and melee are different action lifetimes and should remain distinct abilities, parameterized by the equipped definition.

| Input | Fists | Rifle |
|---|---|---|
| LMB / PrimaryAttack | Existing left punch/strike flow | Fire according to the rifle definition |
| RMB / SecondaryAttack | Existing right punch | Aim; hold is proposed, with toggle an input preference |
| R | Existing heavy strike | Reload; only the equipped profile's handler may respond |
| E | Shared interact/pickup | Shared interact/pickup |
| I | CommonUI inventory | CommonUI inventory |
| Existing 0/1 actions | Fist selection / available loadout entry | Item-backed rifle selection; bindings stay data-driven |
| X, if the rifle supports multiple modes | No action | Existing change-fire-mode input can be connected |
| Q | Existing exploration-only sprint rule | Sprint only while in exploration; not while aiming/fighting/reloading |

Keep the shared IMC and profile-gated grants. InputConfig currently has Interact and HeavyStrike but no Inventory, Aim or Reload routing. R currently has reload and heavy action assets in the map, so the integration must make their semantic ownership explicit. Avoid adding a second live RMB aim handler while right-punch remains granted.

The current ASC retries inactive abilities on every held frame regardless of ActivationPolicy. Define firearm edges deliberately: semi-auto consumes one press, auto repeats through its owned cadence while held, burst owns its bounded sequence. Switching or menu input capture must not turn a held mouse button into an unintended new weapon's shot. Preserve the existing held-input fist behavior.

### Fire/damage

Adapt the existing hitscan/target-data infrastructure. Resolve muzzle/grip data from the active weapon representation, and resolve ammunition from its inserted magazine. Use one weapon-definition cadence/spread source instead of the old ability/attribute/actor duplicates. Debit exactly one round per accepted shot, with a shot/action identity for duplicate callback protection.

Use the current SetByCaller.Damage and hit-result effect-context contract so Chalkie damage, directional reactions and death follow the same combat spine as fists. The old Shoot uses Data.Damage and cannot simply be assigned the modern damage effect. Connect real fire sound/muzzle effects, confirmed hit feedback and shot hearing noise. A receiving actor's damage/reaction belongs to the accepted hit, not the cosmetic fire animation.

Camera trace establishes the intended aim point; a muzzle-to-aim trace determines what the barrel can actually reach. The current camera-origin-only trace can shoot past cover that blocks the barrel, especially when crouched. Check the muzzle while close to walls and low cover; do not apply the punch wall-preflight helper to bullets as if it were the same action.

## 6. Rifle stance, animation and crouch

The user confirmed the existing CHT architecture and two rifle modes. Keep ownership, selected weapon, draw state and physical stance distinct. A rifle can be owned/carried without being active, and selected while lowered without aiming. Fists retain their existing equip-to-fight behavior; selecting the rifle leaves exploration available.

| Selected profile | Mode | Animation selection |
|---|---|---|
| Weapon.None | Exploration | Existing empty-hand set |
| Weapon.Fist | Fight | Existing fist profile and its current overlay/strafe behavior |
| Weapon.Rifle | Exploration | Rifle lowered/exploration idle, locomotion and state-driven transitions |
| Weapon.Rifle | Aiming / fight | Rifle aimed idle and directional fight locomotion, with aim offset and rifle action animations |

Every relevant rifle row/profile includes Standing/Crouching. CHT selection combines the weapon profile in OwnedTags with mode/aim state, stance, gait, locomotion phase, direction and foot information. Reuse existing context fields/tags where available. Rifle candidate pools must be selected by that profile instead of the hardcoded unarmed overrides.

The rifle aim ability requests fight behavior while active and blocks/cancels sprint consistently. Ending aim releases that ability's fight request and returns to rifle exploration when no other action requires fight mode. It must not clear a tag still owned by another valid action. Holstering/switching cancels aim through the same ownership rules. Do not add a separate sticky fight-ready tier or timeout as an implicit requirement of rifle selection. Whether firing outside aim is permitted is a remaining action-policy choice, not a reason to introduce another animation architecture.

Combat.Ready currently drives the fists overlay independently of Movement.Strafe, so the weapon-aware overlay must not raise fists over an equipped rifle. Existing fist timing and stance behavior remain unchanged.

### Content actually located

| Family | Verified coverage | Planning implication |
|---|---|---|
| Retargeted Rifle_01, under Content/AZ/Assets/RTG/Riffle_P01 | 119 SurvivalMan clips; standing relaxed/aim idles; eight-way walk/jog and aimed variants; crouch idle/aim; 16 crouch movement loops; fire and jump material | Useful old implementation reference, but not RifleAnimsetPro. None of these clips have the contact_l/contact_r curves required by the current foot-aware pipeline; loops have no BranchIn. |
| RifleAnimsetPro source, under Content/RifleAnimsetPro/Animations | 277 sequences: 127 RootMotion, 127 InPlace, 23 AimOffsets; standing/crouch locomotion, aim, fire, reload and equip/holster content | Located assets still use the pack's UE4 skeleton. Verify the user's intended retarget folder or prepare the selected clips before wiring them. |
| Mocup_Online/Rifle folder | Sample imports and exhaustive matching identify many MovementAnimsetPro/unarmed clips despite rifle-like names | Folder names do not establish rifle-pose provenance. Do not use this as proof of ready rifle content. |
| Existing AZ_AM_Rifle_Fire | 0.379-second mesh-space additive P01 montage in UpperBody slot | Active MHC has no UpperBody slot. Preserve additive semantics when integrating; a normal slot alone is not proof of correct additive evaluation. |
| Existing AO_Rifle_Aim | 17 SurvivalMan standing samples | Reference for standing aim. RifleAnimsetPro has separate crouched aim samples; crouched grip needs its own verification. |

No recognizable RifleAnimsetPro retarget was located after checking import provenance across the 2,799 SurvivalMan sequences and source-name matches across other skeletons. This is a bounded search result, not proof that an arbitrarily renamed asset with cleared provenance cannot exist. The plan can proceed using the supplied RifleAnimsetPro source content; no new animation purchase is assumed.

Concrete source action assets include EquipRifle/HolsterRifle (1.8 seconds), Rifle_Reload_2 (2.167 seconds), Rifle_Crouch_Reload (2.067 seconds), Rifle_Crouch_Reload2 (2.4 seconds), and Rifle_Crouch_Burst. The inspected equip/holster/reload sequences have no gameplay notifies. Do not assign tactical/empty meanings to numbered variants until their actual hand/bolt motion is checked.

### Integration approach

- Coordinate **two separate rigs**: the hero's Mover/AnimBP animates body, arms and hands, while the rifle actor's skeletal mesh and weapon animation instance animate its moving parts. The rifle is not a static mesh-only prop. Both consume the same active action/phase; they do not each run an independent gameplay reload or shot counter.
- Fresh M16 inspection confirms a skeletal WeaponMesh3P with trigger, selector, charginghandle, catch, ejector and magazine bones. It currently has no assigned weapon AnimBP; no AnimSequences referencing that M16 skeleton were found in the registry. RifleAnimsetPro's character animations do not automatically animate this different weapon rig. Add weapon-rig animation preparation to content phase 0: reusable authored clips where compatible, otherwise authored mechanical clips or a small data-driven bone-animation layer.
- For firing, an accepted shot coordinates character recoil, the weapon's firing mechanism, muzzle/casing/audio effects and any empty/lock-back presentation supported by the rifle definition. Whole-weapon motion inherited from the character's hand, weapon-mechanism motion and camera recoil need distinct roles so recoil is not accidentally applied twice.
- For reload, character mag-out/hand/stow/mag-in/bolt/ready phases coordinate the rifle mechanism and magazine representation using the action identity and agreed timeline. Interruption, pause/play-rate changes and switching must update/cancel both animation instances coherently. A cosmetic weapon notify must not independently debit a round or commit another magazine swap.
- A magazine's **inventory instance** and its **visual representation** are different objects. A rigid magazine usually needs a detachable static-mesh prop, attached to the magwell, hand or world as appropriate; it only needs its own skeleton if its visuals require moving parts. Hide/replace the rifle's built-in magazine geometry while using the detachable prop so two magazines are not rendered. Every representation resolves the same magazine instance ID.
- Keep the active AZ_ABP_MoverHero_MHC and UAZ_MoverAnimInstance, including the existing CHT/state-machine/BlendStack/MM selection model. The old AZ_ABP_Hero Rifle state machine and BlendSpaces remain content references; a classical animation-path conversion is outside this plan by the user's decision.
- Introduce an editor-assigned weapon animation profile for chooser/pool references, exploration/aim poses, stand/crouch action montages, aim offsets and attachment configuration. Add rifle exploration and rifle aimed/fight cases through CHT/profile data. Make native walk/run/strafe/crouch database overrides weapon-aware. Adding rifle chooser rows alone would otherwise be overwritten by current unarmed pools.
- Preserve the Mover state machine and existing jump/crouch/transition ownership. Prepare compatible rifle loops with measured speed/foot contacts, indexed ranges, loop-continuity flags and shared gait normalization where appropriate. Do not casually mix different crouch animation families.
- Add rifle stance/aim and action evaluation before the existing FullBody override; grabs, hit reactions and committed full-body actions retain precedence. Route slot names and additive/local/mesh-space types explicitly. Keep firearm cadence/gameplay and cosmetic recoil responsibilities clear.
- Author draw/holster attach/ready and reload mag-out/mag-in/ready events at actual clip times, separately for standing and crouched variants. Existing montage utilities and gameplay-event tasks should author and consume those beats.
- Existing MetaHuman body sockets are BackRifleSocket (spine_04), RightHandRifleSocketRelaxed (hand_r), RightHandRifleSocketAim (middle_01_r), and Hand_LeftSocket. The different relaxed/aim attachment bones require validation. Prefer one stable attachment owner with profile offsets instead of multiple writers fighting the weapon transform.
- AZ_BP_Rifle has LeftHandGrip/LeftHandGripAim and magazine-related bones, but no Muzzle socket. Establish the muzzle and magazine hand/world representation before firing/reload presentation.
- Procedural IK is currently deliberately disabled. Start with authored retarget/grip alignment; do not silently restore old IK. If later agreed as necessary, a weapon-specific current-frame hand constraint must release during magazine handling.

Crouch acceptance includes pickup, equip/holster, relaxed and aimed idle, directional movement, aim enter/exit, fire, reload, blocked muzzle at low cover, and clean return to standing. Camera height and mesh base remain owned by Mover/camera code; weapon layers must not write the hero mesh transform.

## 7. HUD and inventory presentation

Use an AZ-owned, per-player HUD/presenter that subscribes to committed inventory/equipment/ammunition and GAS-vitals events. Seed a snapshot on creation, then update on changes and remove bindings on teardown. Compose or subclass the installed kit widgets under AZ content; keep vendor demo characters, simulated numbers and global lookups out of the gameplay data path.

ProHUDV2_Horror provides the HUD container, crosshair, recoil/hit feedback, pickup/option notifications and damage effects. Its BPi_HUDManagerV2_H exposes these operations. HQUI_ProgressBars exposes PB_SetPercent/PB_SetTargetPercent and appearance controls for linear/circular bars. The former is an AHUD Blueprint; the current AZ controller also creates its own old inventory HUD, so select one AZ bootstrap instead of displaying two roots. Pass the owning player explicitly when creating widgets.

Initial HUD proposal:

- Active weapon icon/name and fire mode.
- Inserted magazine rounds/capacity, for example 18/30, plus compatible spare-magazine count. Distinguish NO MAG from an inserted EMPTY magazine.
- Optional compact spare-magazine entries showing their actual counts, for example 30 and 11. Do not present all rounds as a freely interchangeable reserve pool.
- Contextual pickup prompt and committed pickup/no-room/incompatible-magazine messages through ProHUD.
- Crosshair and confirmed hit feedback driven by the actual shot/aim/spread state.
- HQUI health bar bound to AZ_VitalsAttributeSet.Health/MaxHealth. The legacy HeroAttributeSet.Health is not combat health. A reload-progress indicator may follow the action's montage progress; it must not decide reload completion.

The CommonUI inventory should show each magazine separately, inserted-magazine information in rifle details, quickslot assignment and busy/reserved states during transactions. Existing AZ ammo-description widgets can be reused visually, but static demo text must be replaced by instance data. Existing health/stamina-style UI art is not evidence that a stamina gameplay resource exists; adding a stamina system is outside this rifle slice.

Opening inventory must transfer input focus through CommonUI and resolve active fire/reload safely. Closing it must restore movement/aim/cursor without replaying held shots or leaving movement cached. No minimap, mission UI, crafting system or complete HUD redesign is required for the rifle milestone.

## 8. Implementation order and completion gates

| Phase | Bounded deliverable | User-visible acceptance |
|---|---|---|
| 0. Lock content and contracts | Verify intended character retargets and weapon-rig animations, rifle/mag definitions, sockets, synchronized animation beats and mode/input policy | Concrete assets and state/transaction contract are reviewable before runtime wiring. |
| 1. Inventory model and access | Stable item IDs/locations, rifle/mag fragments, authoritative capacity/change events, I/E integration and Mover pickup adapter | Pick up separate magazines and a rifle; inspect real round counts; a full bag refuses pickup without losing the item. Crouched pickup also works. |
| 2. One equipment owner | QuickBar/UI call CommonUI equipment, correct old/new ordering, source identity and grant lifetime; migrate intrinsic fists through the same contract | Existing punches/heavy/wall behavior remain intact; rifle/fist switching grants only the selected actions and preserves magazine identity. |
| 3. Rifle presentation and stance | Body-mesh/socket bridge, draw/holster/ready, weapon-aware chooser/pools, stand/crouch layers, rifle mechanical AnimBP/animations and detachable magazine props | Hero and rifle rigs stay synchronized; standing and crouched movement/aim work; fight remains sprint-free. No unarmed loop overrides the rifle profile. |
| 4. Shooting and minimal ammo readout | Adapt generic fire/aim, magazine debit, cadence/input semantics, modern damage context, muzzle obstruction, sound/VFX and HUD snapshot | Each accepted shot consumes one round from the correct mag and damages through the current combat system. No shooting through muzzle-blocking cover, including crouch. |
| 5. Detachable reload | Spare selection/reservation, mag presentation, insertion commit, ready and pre/post-commit cancellation | Partial/empty mags survive reload; no duplication/loss on cancel, repeated input, switch or drop. Standing and crouched reload are included. |
| 6. HUD/inventory finish and interruption pass | ProHUD/HQUI styling and event adapters, magazine details, focus lifecycle, remaining movement/stance/action combinations | All displays agree with inventory; aim/reload/menu/grab/death/drop/switch transitions clean up. Existing fists, crouch and exploration sprint still work. |

Build a thin, honest ammo readout during phases 1/4; polish kit presentation in phase 6. Do not postpone all visibility until after the mechanics. Crouch is a gate at each relevant phase, not a final animation add-on.

For later pistols/other magazine-fed guns, reuse the item/location model, equipment transaction, generic firearm abilities, HUD events and compatibility policy; supply different definitions and animation profiles. Weapon-specific melee can reuse the existing melee action family. Defer disk saves, loose-round magazine packing, manual magazine selection, chamber/mixed-round simulation, attachments and multiplayer-specific polish unless separately chosen.

## 9. Validation and risk controls

Validation is user-run PIE after successful builds/asset checks. Suggested implementation diagnostics should identify item/magazine IDs and transaction/shot IDs, not only clip names: pickup, equipment commit, magazine swap, shot accepted/debited, cancellation and presentation state.

Required scenario groups:

- Rifle with/without an inserted magazine; full, partial, empty and incompatible spares; two identical rifles with different magazines.
- Reload before/after insertion commit; repeated notifies/requests; selected spare removed/locked; full inventory including differently sized magazines.
- Pickup/drop/re-pick with loaded rifle and partial magazines; failed world spawn; no round or item creation through switching.
- Hold fire across switch, switch during auto/burst/reload, aim while switching, inventory open/close while buttons remain held, death/grab cancellation.
- Standing/crouched aim/fire/reload; stand/crouch changes mid-action; movement, low cover, wall-obstructed muzzle and the current exploration-only sprint rule.
- Fist selection, both punches, heavy/paired strikes, wall rejection, crouch/locomotion, exploration sprint and HUD/input-focus regression checks.

Keep per-player authority and event ownership compatible with future co-op, using the existing ASC/RPC/Mover architecture. Do not migrate the inventory off the controller merely to follow an older memory note, and do not build lobbies, prediction rewrites or multiplayer test infrastructure for this single-player-first slice.

Known boundaries: exact intended RifleAnimsetPro retarget location remains open; source reload variants need visual semantic labeling; grip/camera/magazine prop alignment needs user testing; save-game persistence is not implemented; no claim of runtime rifle readiness is made from the read-only audit.

## 10. Source and asset reference map

All paths below were read directly or verified through the live editor. Older memory describes useful design intent but is not treated as current implementation proof.

| Reference | Purpose |
|---|---|
| [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Inventory/AZ_QuickBarComponent.cpp:46](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Inventory/AZ_QuickBarComponent.cpp:46) | Current direct grant/select/unequip path |
| [C:/UnrealEngine/Games/AZ/Source/AZ/Private/InventoryUI/AZ_Inv_CommonUI_InventoryComponent.cpp:62](C:/UnrealEngine/Games/AZ/Source/AZ/Private/InventoryUI/AZ_Inv_CommonUI_InventoryComponent.cpp:62) | UI capacity, pickup/drop, equip multicast ordering |
| [C:/UnrealEngine/Games/AZ/Source/AZ/Public/InventoryUI/AZ_Inv_CommonUI_InventoryItem.h:27](C:/UnrealEngine/Games/AZ/Source/AZ/Public/InventoryUI/AZ_Inv_CommonUI_InventoryItem.h:27) | Current manifest/count-only item state |
| [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.cpp:71](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.cpp:71) | Equipment primitives, state/grant lifetime and mesh coupling |
| [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Items/AZ_PickupItem.cpp:22](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Items/AZ_PickupItem.cpp:22) | Old character/HUD-dependent pickup targeting |
| [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Player/AZ_PlayerController.cpp:269](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Player/AZ_PlayerController.cpp:269) | Current old HUD bootstrap and input bridge |
| [C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_Shoot.cpp:213](C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_Shoot.cpp:213) | Fire implementation and old ammo/damage assumptions |
| [C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_Aim.cpp:13](C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/Abilities/AZ_GA_Aim.cpp:13) | Old aim presentation coupling |
| [C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/AZ_AbilitySystemComponent.cpp:87](C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/AZ_AbilitySystemComponent.cpp:87) | Held input activation behavior |
| [C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/AZ_DamageExecCalc.cpp:38](C:/UnrealEngine/Games/AZ/Source/AZ/Private/AbilitySystem/AZ_DamageExecCalc.cpp:38) | Current combat damage contract |
| [C:/UnrealEngine/Games/AZ/Source/AZ/Public/AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h:10](C:/UnrealEngine/Games/AZ/Source/AZ/Public/AbilitySystem/AttributeSets/AZ_VitalsAttributeSet.h:10) | Canonical combat health versus legacy UI health |
| [C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_MoverAnimInstance.cpp:1217](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_MoverAnimInstance.cpp:1217) | Context-driven unarmed DB overrides to make weapon-aware |
| [C:/UnrealEngine/Games/AZ/Content/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC.uasset](C:/UnrealEngine/Games/AZ/Content/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC.uasset) | Active animation graph and fist layer |
| [C:/UnrealEngine/Games/AZ/Content/AZ/Blueprints/Items/Equippables/Weapons/BPAZ_CommonUI_PickupItem.uasset](C:/UnrealEngine/Games/AZ/Content/AZ/Blueprints/Items/Equippables/Weapons/BPAZ_CommonUI_PickupItem.uasset) | Existing rifle pickup visual/manifest template |
| [C:/UnrealEngine/Games/AZ/Content/AZ/Assets/RTG/Riffle_P01](C:/UnrealEngine/Games/AZ/Content/AZ/Assets/RTG/Riffle_P01) | Verified retargeted Rifle_01 family |
| [C:/UnrealEngine/Games/AZ/Content/RifleAnimsetPro/Animations](C:/UnrealEngine/Games/AZ/Content/RifleAnimsetPro/Animations) | Intended rifle source family including crouch actions |
| [C:/UnrealEngine/Games/AZ/Content/ProHUDV2_Horror/Blueprints/Libraries/BPi_HUDManagerV2_H.uasset](C:/UnrealEngine/Games/AZ/Content/ProHUDV2_Horror/Blueprints/Libraries/BPi_HUDManagerV2_H.uasset) | Installed HUD presentation interface |
| [C:/UnrealEngine/Games/AZ/Content/HQUI_ProgressBars/Blueprints/Interfaces/BPi_ProgressBars.uasset](C:/UnrealEngine/Games/AZ/Content/HQUI_ProgressBars/Blueprints/Interfaces/BPi_ProgressBars.uasset) | Installed progress-bar interface |
