---
name: project_hurdle_spec
description: "★★★ USER WORK ORDER for HURDLE (2026-09-14, 7 decisions): geometry-first hurdle-vs-mantle, verify V2 contact needs from the animation not the filename, 20cm L_001 walls are the first targets, reject unmeasured far-side drops, standing hurdle only where the clip fits, same commitment policy, and ONE shared data-driven traversal execution path with UAZ_GA_Mantle kept as a thin wrapper."
metadata: 
  node_type: memory
  type: project
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-14T21:52:06.627Z
---

# HURDLE — user work order (2026-09-14)

Implement hurdle **preserving the working mantle behaviour**. Builds on
[[project_mantle_traversal_2026-09-14]] and [[project_mantle_handoff_spec]] (still in force).

## Measured asset structure (verified 2026-09-14, do not re-derive)
26 hurdle montages under `/Game/AZ/Assets/GASP/`. Hurdle needs **up to THREE warp targets**, unlike
mantle's one:

| Target | Provider | Note |
|---|---|---|
| `FrontLedge` | **BONE / `attach`** | same missing-bone problem as mantle → needs measured **Static** points |
| `BackLedge` | **None** | only on V2 clips |
| `BackFloor` | **None** | the landing beyond the obstacle |

Provider `None` means the target IS the desired ROOT transform at window end
([[feedback_motionwarping_warppoint_provider]]) — so BackLedge/BackFloor need **no** anchor measurement.
Only FrontLedge does. Some walk clips list `bone=interaction` on a provider-None window; inert, ignore.

Sets: `Hurdle_1_0_{run,walk}_F_{L,R}foot`, `Hurdle_1_0_{run,walk,stand}_F_V2_{L,R}foot` (★ **stand exists
ONLY as V2**), plus `Catch_Hurdle_{low,med,high}_stand` (airborne catches — OUT OF SCOPE, like Catch_Mantle).
Non-V2 = FrontLedge → BackFloor. V2 = adds a BackLedge window between them. Lengths: run 2.67–3.13,
walk 3.93–4.67, stand_V2 1.50–1.77.

## The 7 decisions
1. **Hurdle vs mantle = GEOMETRY FIRST, then movement context.** Depth, clearance and a valid far-side
   landing decide whether hurdling is legal; top support decides whether mantling is legal. Where BOTH are
   genuinely legal: **prefer mantle when standing/walking, hurdle when running**, using current intent
   together with actual approach motion — NOT the speed left after the wall clamp has slowed the character.
   ★ **Do NOT hardcode "40 cm divides everything."** Derive supported depth ranges from the ANIMATIONS. A
   50 cm-deep block is not automatically a valid mantle destination — check the real support footprint and
   clearance.
2. **V2 selection = actual contact requirements.** Include both sets, but **verify what the animations do
   first; do not infer "V2 = step onto obstacle" from the filename.** If V2 requires a foot plant on top,
   enable it only where there is sufficient usable top support; use a clearing variant for thinner
   obstacles when its motion and landing fit. Selection considers depth, contact/support requirements,
   approach pose and distance. ★ **Keep these requirements in ANIMATION DATA, not special cases scattered
   through the ability.**
3. **The 20 cm L_001 walls are the first intended targets** (100 cm high, 20 cm thick) with a clear,
   supported far-side landing. ★ Keep the **broad platforms as regression cases**: adding hurdle must not
   make ordinary mantle targets start selecting hurdle.
4. **No valid far-side floor → REJECT hurdle.** Before commitment require a supported landing within
   calibrated height/drop, slope, distance and capsule-clearance limits; otherwise fall back to the normal
   jump if its own gates permit. **Never commit to an unmeasured drop.** If the destination becomes invalid
   AFTER commitment: controlled interruption/recovery including Falling. Never leave the character
   suspended, never teleport it to the old destination.
5. **Standing hurdle only where its animation fits.** Support standing V2 on geometry satisfying its
   verified contact and landing requirements. This does NOT mean every thin wall is hurdle-able from
   standing — if standing V2 needs more top support than 20 cm, reject that combination and keep the normal
   jump fallback. **Never substitute a moving hurdle at time zero for a stationary character.**
6. **Same commitment/interruption policy as mantle.** Committed barrier/head-hit reaction finishes first; a
   timely accepted traversal prevents a NEW cosmetic reaction to its target; distinguish **Started /
   NoMatchingTraversal / BodyBusy(Consumed)** with only no-match permitting jump fallback; releasing Jump
   or movement does not cancel an underway hurdle; death/grab/destroyed target/genuine obstruction keep
   controlled interruption; **no delayed traversal fires after a reaction finishes**.
7. **ONE shared execution path, via a narrow compatibility-preserving refactor.** Generalise execution into
   a data-driven traversal ability/base/executor. ★ **Keep `UAZ_GA_Mantle` as a thin compatible wrapper**
   so existing Blueprint parents, grants and references do not break. Per-action DATA defines: montage
   candidates, legal entry windows, required warp targets, contact rules, exit policy. SHARED execution
   owns: Traversing mode, finite root-motion driving, scoped collision exemptions, callbacks, cleanup.
   Action-specific geometry validation stays explicit — hurdle requires a far-side destination, mantle
   requires support on top.

## Preconditions before ANY playback (user-mandated)
- Measure contact anchors and legal entry windows for the hurdle clips (as was done for mantle).
- **Verify skeleton compatibility of montages, sequences AND blend profiles** — see
  [[feedback_montage_blend_profile_cross_skeleton]]; 44 montages already had a foreign blend profile.
- Preserve **1× playback**.

## Carry-forward requirements (still in force from the mantle work)
- Select an entry using **distance, speed AND outgoing pose** — never speed alone.
- Preserve momentum when input stays held; use a suitable stop when released.
- **Avoid the Idle → Start → Loop restart after a moving traversal** (still OUTSTANDING: ~0.62 s cold start).
- ★ **A hurdle still AIRBORNE at its release point hands movement to Falling**, not to a forced grounded
  loop. Grounded recovery must follow actual support.
- Complete build and asset read-back checks, then provide manual validation steps.
  **No automated tests; do not start PIE or editor tests without asking.**
