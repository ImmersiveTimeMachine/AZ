---
name: project_chalkie_territory_combat_plan
description: "★ UNIFIED COMBAT PLAN v2 (2026-07-20, awaiting user review): one melee rail hero↔NPC↔weapons — shared damage spine (notify→event→trace→GE→attributes) is THE missing keystone; hero-rail gap audit; 150-clip census; phases P0-P3 in §F. TERRITORY: designed but POSTPONED by user (§C kept for later). Read before NPC combat / damage work."
metadata:
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-07-21T02:29:33.521Z
---

# Chalkie territory + combat/anim plan (DRAFT for user review, 2026-07-20)

# ⚠ STATUS UPDATE 2026-07-20 (late) — center of gravity moved
- **Territory volumes: POSTPONED by user** ("not so important now") — §C design stays for later; do NOT build unprompted.
- **New direction (user):** hero already has 2 movement types (explore + fists-up combat-ready) with a WORKING punch ([[project_combat_fist_build_plan]]). Perfect the melee model ONCE on that rail — "punch is also a weapon" — then replicate to all weapons. §F below supersedes §D's C1-C3.

## F. UNIFIED MELEE RAIL PLAN (v2)

### Gap audit (code-verified 2026-07-20 vs the 28-day-old docs)
**EXISTS (hero rail):** UAZ_GA_MeleeAttack fully built + compiled (hand param, idle/move latch, montage select, Stage-2 RM FLayeredMove_RootMotionAttribute w/ proxy guard + replace-don't-stack, EffectsOnActivate, HitWindowEventTag plumbed into PlayMontageAndWaitForEvent, OnMontageEvent handler stub); QuickBar Server_Select; CommonUI equip keystone; combat-ready C++ interface (bCombatReady/CombatReadyAlpha); AZ_HeroAttributeSet HAS Health (ReplicatedUsing); UAZ_AT_PlayMontageAndWaitForEvent; AZ_TargetType (EventData→HitResult).
**MISSING — damage spine greenfield END-TO-END (THE keystone):** no anim-notify sender class anywhere, no Event.Montage.Melee.Hit tag, OnMontageEvent goes nowhere (no trace), no Damage GE / ExecCalc, nothing modifies Health, no death hook.
**MISSING — zombie side:** infected ASC has NO AttributeSet and NO granted abilities (grep-verified); Chalkie ABP has NO montage slot; GA_MeleeAttack hero-hardcoded ×2 (GetHeroPawnFromActorInfo @cpp:62 bIsMoving latch; Cast<AAZ_PawnMoverHeroCharacter> @cpp:99 RM bridge) → can't run on the infected avatar as-is.
**VERIFY in editor:** GE_CombatReady + fists-up AnimGraph layer done by user?; BP_GA_Punch_L/R montage assignments; FightingAnimsetPro skeleton (old Q3); weapon items in inventory (old Q5). Known debt: GetHeroPawnFromActorInfo wrong project-wide; AddReplicatedLooseGameplayTag switch pending.

## H. COMBAT LOOP COMPLETE (2026-07-21, commits e65be0c→6e5605b, PIE-verified)
Bidirectional melee LIVE: chase→claw (10 dmg/2.2s bite, notify@1.1s)→break-off past 360→re-pursue; punches single-target nearest, 10 dmg = 5-punch kill (punch BP CDOs set via python — loaded CDOs don't pick up LC'd native ctor defaults); variant flinches (HitReactMontage DA field, 1.1s window of the 3-7s KnockBack clips, interrupts a mid-swing claw = player defense); pack alert on death (GA_Death→HordeSubsystem→urgent investigation converging on the killer); GAS death (directional montage via the replicated task) → ANIM-ONLY corpse (user: NO ragdoll, NO collision change, NO despawn — permanent bodies, anims paused after 4s; death latch = deactivated Mover).
**HARD LESSONS:** (1) NEVER DestroyComponent a live Mover — the NP backend ticks the freed sim → editor crash in WalkingMode::SimulationTick; Deactivate instead. SetPrimaryVisualComponent(nullptr) mid-game can bake a stale smoothing offset into the mesh (skeleton-under-floor); anim-only death needs neither. (2) Project ini DELETES the engine "Ragdoll" profile → SetCollisionProfileName("Ragdoll") silently = no collision (fell through floor). (3) BT latent task + GAS: CancelAbilityHandle fires OnAbilityEnded SYNCHRONOUSLY → unbind BEFORE cancelling or FinishLatentTask(Succeeded) lands mid-abort and WEDGES the tree ("stuck in attack"); also detect sync-end after TryActivate (else 5s hang). (4) "Zombies go blind after chase" = that wedge, NOT perception — proven via TEMP [ChalkieDiag] telemetry (per-instance throttle — a shared static throttle HID the broken instance). (5) Sight flickers seen=0 for seconds at 76-321cm (1.1s facing spring swings the 140° cone off an orbiting player) — proximity retention (engaged target stays fresh within InstantDetectRange) bridges it; unengaged notice-speed lever = peripheral angle/instant zone per-variant. (6) Zombie RM override must be BiteSeconds, not the 8-10s clip. (7) Damage homes: per-attack dmg on the ABILITY, HP on the pawn ctor (→config DA), GE carries no numbers, mitigation = ExecCalc. Remaining → task #12; diag telemetry still in, remove after a clean session.

### Phases
- **P0/S1 — ✅ DONE 2026-07-20 (commit `7a3cc0c`, build green, pushed). Infected 50 HP / hero 100 HP / punch 25 (now 10).**
- **S2+S3 — ⚙ BUILT 2026-07-21 (building at handoff): PIE-VERIFIED: punch damage lands (hit notifies on 4 punch montages @45%, applied via cpp harness — python can't construct FGameplayTag, read-only). Death evolved v1→v2 same session: v1 direct Montage_Play (user challenged hardcoded paths → moved montages into anim-set DA DeathMontage_F/B/L/R fields, all 4 DAs filled; then challenged non-GAS play) → v2 GAS-PROPER: `UAZ_GA_Death` (Event.Death-triggered — AbilityTriggers CANNOT be ctor-set, native tags register in AssetManager AFTER CDO construction → `ConfigureTriggerOnCDO()` patched at grant site; cancels other abilities; plays via UAZ_AT_PlayMontageAndWaitForEvent bStopWhenAbilityEnds=false → REPLICATED montage; pawn BeginCorpse(delay)). Wall-clip fix: montage→RAGDOLL at 60% of clip (impact beat) — SK_ZombieAC_A_PhysicsAsset exists; Ragdoll profile, PhysicsOnly. `UAZ_GA_ZombieMelee` = subclass of the SAME GA_MeleeAttack (dmg 10, range 170, hand randomized, SelectMontage→DA AttackMontage_L/R, ServerOnly): pack attack clips are ALL long clawing cycles 6-19s → ability plays a 2.2s BITE (timer self-EndAbility, blend-out smooths; BT loop = sustained clawing). `UAZ_BTTask_ZombieAttack` (instanced, OnAbilityEnded-latent, timeout 5s, cancel on abort). Native grants at possess (bStartupAbilitiesGranted guard): GA_Death + GA_ZombieMelee. AM_Zombie_Death_F/B/L/R (auto-blend-out OFF = corpse holds frame; from Death_Hit_*_1_IPC) + AM_Zombie_Atk_L/R montages. DA gained AttackMontage_L/R. ARCH DECISION (user asked): NO separate NPC/hero ASC class, NO per-faction attribute sets, NO parallel GA trees — differentiate by COMPOSITION: shared Vitals (damage spine hits one attribute), faction sets stack as additions (HeroAttributeSet/Weapon vs future InfectedAttributeSet w/ Rage), GAs split by interaction SHAPE with subclass+grant-list+NPC-only-GA as the differentiation. PENDING after reopen: re-apply attack-montage hit notifies @1.1s (harness got raced), user hand-adds "Zombie Melee Attack" BT node replacing Chase Wait, PIE bidirectional.** Original S1 spec: `Event.Montage.Melee.Hit` native tag; `UAZ_AnimNotify_SendGameplayEvent` (tag+payload); melee sweep in GA OnMontageEvent (hand-socket sphere → TargetData); `UAZ_VitalsAttributeSet` (Health/MaxHealth + IncomingDamage meta) for BOTH ASCs; `GE_Damage` + ExecCalc (SetByCaller); death hook Health<=0 → `Event.Death` to the ASC; de-hero-ize GA_MeleeAttack (interface/duck-typed getters). Rides the task-#9 native-batch restart.
- **P1 — hero punch completes:** hit windows on the 4 AM_Fists_Punch_*; infected vitals + damage intake; zombie hit-react = variant-matched KnockBack montage (anim-set DA gains fields); directional death (15 clips) + collision off + despawn; horde NotifyAggro on pack-mate death. PIE: punch a Chalkie to death, pack reacts.
- **P2 — zombie attacks back (SAME GA class):** DefaultSlot in Chalkie ABP; BP child GA_ZombieMelee (strike montages Atk_Arm_L/R + Hyper); granted at possess; BTTask_ZombieAttack replaces Chase Wait; hero damage intake + hit-react + death stub. PIE: bidirectional melee.
- **P3 — feel + parity proof:** combos/heavy; MotionWarping lunge (shared traversal primitive); Atk_Loop grapple state; FightingAnimsetPro after skeleton check; then WEAPON PARITY: knife/bat = rows + numbers on the same GA (zero new ability code); GA_Shoot re-pointed at the same GE spine.

## G. STRATEGIC LAYER (added 2026-07-20 latest, per user: fossil-aware, UI-next-not-now, CommonUI inventory = the backbone)

### G1. Fossil boundary (v1 vs v2 — code-verified census)
**ACTIVE v2 (build on these ONLY):** AAZ_PawnMoverHeroCharacter / AAZ_PawnMoverInfectedCharacter, UAZ_MoverAnimInstance / UAZ_InfectedAnimInstance, AZ_ABP_MoverAnimInstance / AZ_ABP_Chalkie, CommonUI inventory+equipment path, UAZ_QuickBarComponent, AZ_HeroAttributeSet (data-only, ASC on PlayerState — v2-safe), UAZ_GameplayAbility (GetHeroPawnFromActorInfo NOW returns AAZ_PawnMoverHeroCharacter — repointed, hero-side fine; still null on infected → P0 de-hero-ize stands).
**QUARANTINED v1 (do NOT build on; delete sprint later):** AZ_HeroPawn, AZ_HeroCharacter, AZ_CharacterBase, AZ_EnemyCharacter, AZ_EquipmentManagerComponent (equip role superseded; pickup role audit before delete), **AZ_TargetType (typed on AAZ_CharacterBase — damage spine must NOT route targeting through it; melee sweep emits TargetData directly)**, InventoryOld/* tree, AZ_ABP_Hero/AZ_ABP_HeroPawn, AZ_BP_Hero/AZ_BP_HeroPawn. GA_Shoot: active-pattern template but audit its TargetType/container path when joining the GE spine (S4).

### G2. CommonUI inventory review (2026-07-20; the 71-day-old inventory-system.md "missing" list is STALE)
C++ suite is essentially COMPLETE: 29 headers incl. everything once listed missing — ItemDescription, CharacterDisplay, EquippedGridSlot/SlottedItem, InventoryHudWidget, InfoMessage, ItemPopUp, InventorySwitcherPanel, **CharacterVitalsPanel + CharacterSkillsPanel (RPG UI groundwork already present)** — plus EquipmentComponent w/ the verified OnItemEquipped keystone (ability grants + profile tag), Equipment/AbilityGrant fragments, FastArray replication. **This is the item/equip backbone for ALL weapons AND the S5 UI phase.** Open items: (a) verify at runtime which ITEM DEFINITIONS exist (fist item w/ EquipActorClass=null per locked decision; weapon items) and whether they're IN the inventory at play (old Q5); (b) widget BP/implementation completeness unknown (some were stubs); (c) InventoryOld coexists — quarantined.

### G3. Strategic sequence (supersedes bare P0-P3 ordering; NO UI until S5)
- **S0 fossil-proofing rule** (standing): every new combat file touches only G1-active surfaces. Free rider on every phase.
- **S1 = P0** damage spine (fossil-proofed: no AZ_TargetType, no CharacterBase types).
- **S2 = P1** hero punch kills Chalkie (debug keys as front-end, no UI).
- **S3 = P2** Chalkie punches back (same GA).
- **S4 = weapon parity THROUGH INVENTORY:** fist item + first real weapon item (knife/bat) as CommonUI ItemDefinitions w/ Equipment+AbilityGrant fragments; QuickBar slots bound to CommonUI items (the designed integration); melee rows + damage numbers = data. Proves "new weapon = data entry". GA_Shoot joins the GE spine here (ranged parity).
- **S5 = UI PHASE (user: the NEXT step after combat):** all front-ends over existing logic, built on the CommonUI suite — quickbar/radial widget replacing number keys (both call Select(N) — designed for this swap), HUD vitals via CharacterVitalsPanel (reads the SAME UAZ_VitalsAttributeSet from P0 — design P0 attributes knowing the panel consumes them), damage/pickup feedback via InfoMessage, item pickup→inventory→equip loop polish. ZERO new gameplay code by construction.
- **Parallel rail:** NPC behavior/anim polish (turn-controller v2, per-set turns, shamble sets) continues independently — never blocks combat.

Follows [[project_zombie_ai_plan]] (all of 2026-07-20's shipped work) and applies the rail doctrine from [[project_combat_fist_build_plan]]: **sim-led/condition-driven → SM state; anim-led one-shot with gameplay windows → GAS montage.**

## A. Anim census — /Game/Zombie_01/Animations (150 InPlace + 150 Root mirrors + 2 Blends)
Root = same clips WITH root motion (source for RM attack lunges via RMAction; the IPC turn/loco pipeline stays IPC).
| Category | Clips | Count | Notes |
|---|---|---|---|
| Loco loops (in SM ✅) | Walk_F_1..6_Full_Loop, Chase_1..5_Full_Loop, HyperChase_1..3 | 14 | all have fwd-vel curves; 4 variant sets built |
| Loco loops (unused) | Shamble_1/2, Shamble_Long_1/2, Crawl_1/2_Loop | 6 | Shamble = 5th gait candidate or set variety; Crawl = crippled mode |
| Idles | Idle_1-v2(_Full), Idle_2/3/4 | 5 | in sets ✅ |
| Idle breaks/transitions | MOB_Stand↔Idle pair, Stand_Walk_F_1..6(+Full), Stand_To_Chase_1..5(+Full), Stand_To_HyperChase_1..3 | ~20 | start-clips per variant — future loco-start polish, NOT this plan |
| Turns | L/R 90 (×5/4), L/R 180 (×4/4) | 17 | _1 set wired; per-set turns queued |
| **Attack entries** | Stand_To_Atk_1..5 | 5 | into sustained attack |
| **Attack loops** | Atk_Loop_1..5 | 5 | sustained clawing — SM-shaped (condition-driven) |
| **Attack exits** | Atk_End_1..5 | 5 | out of sustained attack |
| **Strike one-shots** | Atk_Arm_1_L, Atk_Arm_2_R, Atk_Arms_3/4, HyperAttack_1..4 | 8 | montage-shaped (damage windows) |
| **Grabs** | Reach_1/2, Reach_Full_1/2 | 4 | TLOU grab — LATER (needs paired player anim) |
| **Hit reacts (moving)** | Walk_F_1..6_KnockBack_Walk, Chase_1/2/3/5_KnockBack_Chase, Atk_KnockBack_1..5 | 15 | ★ VARIANT-MATCHED to loco clips — slots into anim-set DA |
| **Deaths** | Death_Fwd_1/2, Back_Mid_1..3, Left_1..3, Right_1..3, Death_Hit_B/F/L/R_1 | 15 | full directional coverage |
| Crippled | Stand(_Hit)_to_Crawl_1/2, Crawl_1/2(_Loop), Crawl_to_Stand_1/2 | 10 | complete crawl lifecycle |
| On fire | OnFire_1, 2A/2B, 3A/3B/3C, 4A/4B | 8 | fire damage flavor — LATER |

## B. Design decisions (rail doctrine applied)
1. **Strikes/deaths/hit-reacts = GAS montages** (anim-led, damage/ragdoll windows via notifies, replicate as montages): new `DefaultSlot` node in ABP main graph (after Turning blend, before layered attack blend). GA_ZombieMelee (BTTask-activated, replaces Chase's Wait breather when in AttackRange), GA_ZombieDeath (directional pick from hit direction — 8+ deaths available), hit-reacts fired by the damage pipeline.
2. **Sustained attack = SM later, montage first.** Pack shape Stand_To_Atk → Atk_Loop → Atk_End is condition-driven (stay while target in range) = legitimately SM — but v1 ships parameterized montage strikes (Atk_Arm_L/R + Hyper for sprint-chase hits) for speed; the loop-grapple state is v2. The pack ABP's existing **Attacking SM (9 states, 26 transitions) stays dormant** — audit its drivers before ever wiring (likely AttackAlphaBlend + index var).
3. **Crawl = real SM/gait mode, not montage** (sustained crippled locomotion; health-threshold entry) — phase 4.
4. **Anim-set DA grows** per-variant combat clips: AttackStrikes[] , KnockBackWalk, KnockBackChase, DeathOverrides[] (KnockBack clips are literally variant-matched: Walk_F_4 set → Walk_F_4_KnockBack_Walk).
5. **Attack lunge**: Root-folder strike clips via RMAction + MotionWarping cone-snap (shared with hero traversal primitive, per Phase-4 plan) — v1 can ship without warp (in-range strikes only).

## C. Territory volumes
- `AAZ_ZombieTerritory` (C++ actor): BoxComponent (scale/rotate; covers courtyards/streets; v2 may add multi-box), `FColor TerritoryColor`, editor-visible/hidden-in-game.
- **Debug draw**: CVar `az.Zombie.DebugTerritory` (registered in FAZModule per CVar-lifetime rule) → per-frame DrawDebugBox in territory color + line from each assigned zombie to its territory + HomeLocation marker. Editor-time: box lines always visible when selected (free).
- Pawn: `EditInstanceOnly` UPROPERTY `Territory` (AAZ_ZombieTerritory*) — assign per placed zombie like AnimSet. Server-only concern, no replication.
- Controller OnPossess: if Territory → HomeLocation = nav-projected point nearest territory center (zombie placed outside walks in).
- `AZ_BTTask_FindPointNear` new mode: territory assigned → random point INSIDE box → ProjectPointToNavigation (rejection-sample ~8 tries, fall back to radius mode); else current radius behavior.
- **Soft leash** (flag `bSoftLeash=true` on pawn): chase NEVER leashed (horror rule); investigation outside territory decays urgency faster (EscalationWindow × 0.5 outside) → drifts home naturally.
- NavModifierVolume no-go zones: separate feature, only when a real safe-room exists.

## D. Build phases (implement AFTER user review)
- **Phase T (editor-closed CLI, rides task #9 native batch):** territory actor + pawn ref + FindPointNear territory mode + CVar debug draw; PLUS the queued native classes (UAZ_ChalkieAnimSet UCLASS w/ combat-clip fields, DA_ChalkieConfig, turn-controller v2). One build, one restart.
- **Phase C1 (attack v1):** DefaultSlot in ABP (scripted); GA_ZombieMelee + BTTask_ZombieAttack (montage, damage GE on notify window, AttackRange gate); health AttributeSet on infected ASC; GA_ZombieDeath + directional montage + collision-off + despawn timer. PIE: chase→strike→player HP down; shoot zombie→directional death.
- **Phase C2 (feel):** hit-react montages from damage events (variant-matched KnockBack clips); MotionWarping lunge; Atk_Loop sustained-attack SM state; per-set turns.
- **Phase C3 (flavor, unscheduled):** crawl crippled mode, Reach grabs, OnFire, Shamble gait/sets.

## E. Open questions for review
1. Attack v1 = montage strikes (recommended) — or straight to the sustained Atk_Loop SM shape?
2. Damage numbers/HP: placeholder GE values fine for v1? (Real tuning belongs to DA_ChalkieConfig / RPG pass.)
3. Death: keep montage-then-ragdoll, or physics ragdoll immediately on kill?
4. Territory shape: box-only v1 OK? (sphere/multi-box later)
5. Does hero damage pipeline exist to hurt zombies yet (GA_Shoot → GE), or does Phase C1 include wiring zombie-side damage intake from the existing weapon GAS?
