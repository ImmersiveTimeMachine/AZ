---
name: project_obstacle_reaction_system
description: "DESIGN (decisions locked 2026-06-22) for CHALK's contextual obstacle-reaction system: the pawn forward-trace sensor classifies what it ran into (height band x closing speed x surface) into ONE EAZ_ObstacleReaction enum that drives BOTH animation (chooser) and gameplay (damage + stagger + AI-noise via GAS). Superset of the traversal system. Read before implementing obstacle/wall/trip/traversal reactions."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
---

Contextual **obstacle-reaction system** — evolved from the wall-impact work. One sensor measures the obstacle once; a classifier turns context into a single **reaction**; that reaction drives BOTH the animation and the gameplay consequences. Superset of [[project_traversal_system]] (vault/mantle/step are just more reactions). Builds on the forward-trace sensor already shipped (see "Current state"). Survival-horror flavour ([[project_lore_chalk]]): reckless movement *costs* you.

## Model
```
UAZ_ObstacleSensorComponent (measure: height, depth, distance, closing speed, normal, hit-actor/surface)
   -> CLASSIFIER (height band x closing speed x surface x intent -> a Reaction)
        -> ANIMATION  (chooser picks the clip for Reaction x arm x mode)
        -> GAMEPLAY   (OnObstacleImpact event -> damage GE + stagger tag + AI noise, via GAS)
```
One detection feeds both. Sensor BROADCASTS what happened; anim + gameplay each decide. Decoupled, authority-applied + GAS-replicated => co-op-safe ([[project_sp_first_coop_extensible]]).

## ★ PIVOT 2026-06-23 — Blocked = intent-cancel + reuse idle-breaks (Approach B), Live-Coding-patched (PIE-pending)
"Blocked plays like idle break" is done by CANCELLING the anim-intent, not a dedicated BlockedBreak reaction. Sensor still detects from RAW input (no flicker, still catches turn-to-escape); only the SM-facing `bIsMoving` is zeroed while Reaction==Blocked → the SM drops into IdleLoop→IdleBreak (reuses the whole idle system, incl. its foot-aware stop). New reaction model:
- **fast tall → Brace** (Run2Wall, held one-shot via `bObstacleReacting`) → then Blocked → idle+breaks.
- **slow tall → NO entry → Blocked**: SM cancels intent → plays its OWN natural foot-aware stop (WalkFwdStop/RunFwdStop) → idle+breaks. (The dedicated **Stop reaction is REMOVED** — the SM's stop is better/foot-aware.)
- **low+fast → Stumble** (held one-shot) → Blocked → idle+breaks.  **sustained → Blocked** → idle+breaks (held until you turn off the wall).
`bObstacleReacting` now = (Brace || Stumble) only. C++ edits: AZ_LocomotionTypes (BlockedBreak enum value removed/unused), sensor entry decision (Stop removed), AnimInstance (bIsMoving cancel on Blocked + bObstacleReacting narrowed). **CLEANUP DEBT:** the Stop chooser rows (i=81/82) + Blocked Idle6 row (i=80) are now DEAD (never match — Stop never produced; Blocked → SM idle, not LocomotionLoop) but harmless; remove via a future `RemoveRowAt` util (only RemoveColumnAt exists). **Known wrinkle:** fast Brace→Blocked fires one stop transition after Run2Wall (Run2Wall→stop→idle); accept or route brace→IdleLoop directly later. Live-Coding only → re-bake on next full build.

## ★ ADD 2026-06-23 — Blocked = STRAFE (fixes idle-slide), Live-Coding-patched (PIE-pending)
While Reaction==Blocked, force STRAFE locomotion (decided by the user) so movement against the wall plays the strafe directional anims instead of "idle but sliding". Local (no GAS tag): the PAWN `ProduceInput` ORs `bStrafe |= (sensor->CurrentReaction==Blocked)` (→ body faces camera + RotationMode=Strafe), and the ANIMINSTANCE ORs `ChooserContext.bStrafe |= (Reaction==Blocked)` (→ chooser routes the strafe loco/idle set). Intent-cancel is now CONDITIONAL: cancel SM `bIsMoving` only when `Blocked && Speed2D<=40` (pinned → strafe idle+breaks); when Blocked but SLIDING (>40) intent flows → strafe directional loco. On unblock both revert to the tag-driven value → prior mode (explore→explore; strafe stays strafe). Reuses the existing strafe loco rows (53-58) + idle/break rows (bStrafe=Any) — NO new chooser rows. All body-only (pawn cpp + AnimInstance cpp), Live-Coding-patched. MP note: the pawn reads the client-side sensor in ProduceInput (sim input) — fine SP, needs sim-determinism for co-op. Pinned threshold (40) hardcoded in AnimInstance ~ sensor BlockedMaxSpeed(30); tune later. **⚠️ 3 stacked Live-Coding patches now (bObstacleReacting widen + Approach B + blocked-strafe) — re-bake on next full build before any restart.**

## ★ REFACTOR 2026-06-23 — 3-PROBE body-band sensor + HeadHit (BUILT, full build `Result: Succeeded`; chooser+PIE pending)
Replaced the single capsule-sweep + top-edge height probe with THREE forward sphere-sweeps at LOW/MID/HIGH body heights (`LowProbeHeight` 50 / `MidProbeHeight` 100 / `HighProbeHeight` 165 cm above feet, `TraceRadius` 22). Reaction = WHICH band is blocked (vertical+opposing normal), no top-edge guessing — **MID dominant > LOW > HIGH**:
- **MID (chest) blocked → Brace** (wall, fast `Run2Wall`) / **Blocked** (slow). MID wins even if low/high also hit (full wall blocks all 3).
- **mid clear, LOW (knee) → Stumble** (low-band hit → `KB_Hit_m_LowRight_Med`).
- **mid+low clear, HIGH (head) → HeadHit** (overhead beam → `KB_Hit_m_HighFront_Med`). NEW enum value `HeadHit=8`.
Entry one-shot on rising edge: FAST (>ImpactMinSpeed for wall / >StumbleMinSpeed for low+head) → the band's clip held its hold time; SLOW → no entry → Blocked. `bObstacleReacting` now = (Brace||Stumble||HeadHit). REMOVED params: WallMinHeight, MaxHeightProbe, StumbleMinHeight, StumbleMaxHeight, StopSettleDelay + the ObstacleHeight output. This build also BAKED the 3 prior Live-Coding patches (Approach B + blocked-strafe). **PENDING: chooser wiring (Stumble row → KB_LowRight, add HeadHit row → KB_HighFront, Brace stays Run2Wall) + PIE + tune probe heights with bDrawDebug.** KB clips are FightingAnimsetPro (need retarget later, like Run2Wall).

## ★ DRIVE-MODE (RM vs physics) decision 2026-06-23 — DEFERRED, reactions stay COSMETIC
User asked whether to add a per-CHT-row "RM-driven vs physics-driven" flag. Finding: drive-mode is today decided by HEURISTIC — the RM bridge (RootMotionFromEverything + queued FLayeredMove_RootMotionAttribute) only queues an RM move for SM TRANSITION clips; loops (and obstacle-reaction rows, which play in the held LocomotionLoop) get RM DISCARDED → play IN-PLACE/cosmetic, *even the RootMotion KB clips*. So reactions are already cosmetic, capsule pinned by wall collision (= the "physics-driven" part) — no flag needed for current behavior. An explicit `DriveMode` (Cosmetic/RootMotion/Physics) field on the CHT output struct would only be needed to make a row's RM actually MOVE the capsule (e.g. a knockback that BOUNCES you back off the wall). DECIDED: keep reactions cosmetic this build; add the DriveMode flag + small RM-bridge gate later IF we want physical recoils. Matches the rail doctrine ([[project_combat_fist_build_plan]]).

## Decisions LOCKED (2026-06-22)
1. **Consequences = damage + stagger + AI-noise** (all three).
2. **Trip = a LIGHT STUMBLE that recovers** (not a full fall/get-up).
3. **Bands HARDCODED first** (tunable params on the component; a data-driven DataAsset table is deferred).
4. **Refactor the obstacle BOOLS -> one `EAZ_ObstacleReaction` enum** on the chooser context (bools don't scale to many reactions; enum column like SMState/StartDirection).

## Current state (already built, to be refactored)
`UAZ_ObstacleSensorComponent` (Source/AZ/.../Character/): gated forward sphere-sweep along move-intent, measures distance + height (top-edge down-probe) + closing speed + normal; outputs `bWallImpact` (hard wall reached within `ImpactTriggerDistance` + closing > `ImpactMinSpeed`, one-shot `ImpactHoldTime`) and `bBlocked` (LATCHED — enter when pinned within trigger at low speed; HOLD while the trace still hits; release only when you TURN enough to lose the hit). AnimInstance copies the flags into the chooser; chooser has the `Run2Wall` row (bWallImpact) + the Idle6 row (bBlocked), gated `c0=LocomotionLoop`. Params: TraceDistance, ImpactTriggerDistance, TraceRadius, WallMinHeight, MaxHeightProbe, MaxWallNormalZ, TraceChannel, ImpactMinSpeed, BlockedMaxSpeed, ImpactHoldTime, bDrawDebug. The soft-approach -> Stop -> Blocked refinement was discussed (closing < ImpactMinSpeed => play walk/run stop, then Blocked after an exposed delay) and folds into the enum below.

## EAZ_ObstacleReaction (the enum)
`None, Brace, Stop, Blocked, Stumble` + reserved `StepOver, Vault, Mantle` (keep reserved values for ABI like InAirLoop). The current `bWallImpact`=Brace, `bBlocked`=Blocked; Stop + Stumble are new.

## Height bands (hardcoded, tunable params)
- `StumbleMinHeight` (~15) .. `StumbleMaxHeight` (~60) = LOW barrier band -> Stumble.
- `>= WallMinHeight` (~110) = TALL WALL band -> Brace/Stop/Blocked.
- mid-gap (60..110) -> treat as wall for now (becomes Vault band when traversal lands).

## Classifier matrix (band x closing speed)
| Obstacle | Fast (>= ImpactMinSpeed) | Slow |
|---|---|---|
| Tall wall | **Brace** (Run2Wall, one-shot) -> **Blocked** | **Stop** (walk/run stop) -> (exposed delay) -> **Blocked** |
| Low barrier | **Stumble** (light, recovers -> None) | StepOver / None (bump) |
Blocked LATCHES until you turn enough to lose the hit (the escape rule). Brace/Stop settle to Blocked on an exposed timer. Stumble is one-shot then back to locomotion.

## Gameplay consequences (the OnObstacleImpact consumer)
- **Damage**: speed-scaled above a SafeSpeed threshold (gentle bumps free), x surface/band mult. Via GAS — `GE_ImpactDamage` with magnitude as a SetByCaller from closing speed (reuse the EffectsOn* / MakeOutgoingSpec patterns).
- **Stagger**: brief `State.Staggered` duration GE that gates locomotion/abilities + plays the stumble/recovery (couples with the Stumble/Brace clip); leaves you vulnerable.
- **AI-noise**: MakeNoise / hearing stimulus at the impact point, loudness ~ closing speed -> infected hear you blunder into things (CHALK hook).
- Wiring: sensor fires `OnObstacleImpact(FAZ_ObstacleImpact{reaction, band, closingSpeed, location, normal, actor, surface})`; a passive GA (or a pawn handler) applies the consequences. Authority-side; GAS-replicated.

## Architecture notes
- Sensor stays the single pawn-side detector (works on any geometry; surface semantics read OPTIONALLY from the hit actor's physical material — geometry-first, metadata-optional).
- Anim: chooser keys on `Reaction` (+ arm via OwnedTags, + mode via bStrafe). New reaction = enum value + row, no new column.
- This IS the traversal classifier: vault/mantle/step are reactions for traversable bands WITH traverse intent (the RM-action branch of [[project_traversal_system]]).

## Build phases
1. **Refactor bools -> `EAZ_ObstacleReaction` enum** — ✅ DONE 2026-06-22 (PIE-verify pending). `EAZ_ObstacleReaction{None,Brace,Stop,Blocked,Stumble,+reserved StepOver/Vault/Mantle}` in AZ_LocomotionTypes.h; `FAZ_v2_ChooserContext.Reaction` replaced bWallImpact/bBlocked; component outputs `CurrentReaction` (Brace/Blocked/None, blocked-latch baked in); AnimInstance copies it. Chooser: added a Reaction EnumColumn (now **c14**) bound to ChooserContext.Reaction @ context 0 — Run2Wall=Brace, Idle6=Blocked, 9 loop rows=None. **CLEANUP DEBT:** the 2 old bool columns (c12 bBlocked / c13 bWallImpact) could NOT be removed (AZ_ChooserUtils has no RemoveColumn, and `execute_script cpp` can't compile a brand-NEW generated file mid-session — Live Coding only patches files from the last full build) → neutralized to `Any` everywhere (inert no-ops, bindings now dangling but harmless; Compile tolerates them). Remove them later via a new `RemoveColumnAt` UFUNCTION (needs a full build). ColumnsStructs is public on UChooserTable (RemoveAt works in real C++). **— RESOLVED 2026-06-23:** added `UAZ_ChooserUtils::RemoveColumnAt(path, idx)`; dropped c12/c13 → **13 cols, Reaction now at c12** (cells verified intact: loop=None, Run2Wall=Brace, Idle6=Blocked). ALSO added `FAZ_LocoSMInputs.bObstacleReacting` → the SM **HOLDS LocomotionLoop while Brace/Blocked is active** (fixes "start/stop/turn anims fire while blocked" when you turn / flick the stick into the wall; clears the instant the reaction does → normal dispatch resumes). ⚠ Mid-way a leftover `execute_script cpp` generated file (`PromptForCheckoutAndSave` in its static init) crash-looped editor STARTUP — deleted + rebuilt; see [[feedback_cpp_executescript_harness]] trap #5.
2. **Height bands + Stumble + soft Stop** — ✅ DONE 2026-06-23 (PIE-verify pending; Stumble on a Run2Wall PLACEHOLDER — swap when a trip clip exists). Sensor: forward sweep is now a **CAPSULE** (feet→head) so it catches low barriers; bands `<StumbleMinHeight(40, above auto-step)` ignore / `[40,StumbleMaxHeight=60)` LOW / `≥60` TALL. **Entry-reaction model** (decided once on the rising edge of reaching the obstacle, by closing speed): tall+fast=Brace, tall+slow=Stop, low+fast=Stumble, low+slow=bump — one-shot (ImpactHoldTime / StopSettleDelay / StumbleHoldTime) then **Blocked** while pinned (latched until you turn off it). New params: StopSettleDelay, StumbleMinHeight, StumbleMaxHeight, StumbleMinSpeed, StumbleHoldTime. Chooser: +Stop rows (gait-keyed → AnimPro_{Walk,Run}FwdStop_RU) +Stumble row (Run2Wall placeholder), all gated on the Reaction enum (c12) — 84 rows. AnimInstance `bObstacleReacting` widened to `Reaction != None` so ALL reactions hold the SM (was Brace||Blocked) — **Live-Coding patch, NOT yet in a full build → re-bake on next editor-closed build** (chooser rows ARE saved). WallMinHeight now reserved for the future vault band.
3. **Gameplay layer**: `OnObstacleImpact` -> damage GE (speed-scaled) + `State.Staggered` + AI-noise. BP-authored GEs like GE_CombatReady.
4. **Traversal actions** (vault/mantle/step) + data-driven reaction table — later.
