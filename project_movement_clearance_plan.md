---
name: project_movement_clearance_plan
description: "BUILD PLAN (authored 2026-06-23, for 2026-06-24) for the 'where can I move' clearance system that fixes blocking/run-in-place/idle-slide the INTENT-PURE way — clamp the move INTENT to free directions (NOT read resolved velocity, which would regress v2 to old-school reactive loco). Mover already resolves MOVEMENT (slide/stop); we reuse its collision math but stay intent/MM-driven. Holds the keep/remove list, incremental build order, Mover-utils investigation, future mechanics (cover/AI/ledge), risks + PIE matrix. Read FIRST when resuming obstacle/blocking/movement work."
metadata:
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
---

# Movement / Blocking — Build Plan (for 2026-06-24). The "where can I move" clearance system.

Resumes [[project_obstacle_reaction_system]]. Session arc: 3-probe sensor + reactions → Blocked=intent-cancel (Approach B) → Blocked=strafe (idle-slide patch) → impact=stop → "why doesn't Mover do this?" → "won't velocity regress us to old-school?" → the two settled points below.

## ★ TWO SETTLED POINTS (the architecture)
1. **Mover ALREADY does movement-clearance.** Its walking mode collision-resolves every move — slides velocity along walls, stops you at them (that's why you don't pass through). So MOVEMENT is handled; we do NOT rebuild it.
2. **STAY INTENT-DRIVEN — do NOT read velocity for the anim (user's call, 2026-06-23).** v2 is intent/trajectory/MM-driven (predictive) on purpose. Reading Mover's RESOLVED VELOCITY to drive the anim = reactive = regression to old-school velocity loco, and makes the MM match actual motion instead of predicting. **REJECTED as the primary fix.** Kept only as a FALLBACK if the clamp proves impractical.

## THE APPROACH — clamp the INTENT (intent-pure), = the "where can I move" system
The reusable clearance query IS the blocking fix:
- Each tick in **ProduceInput**, query clearance in the desired direction (reuse MOVER collision math — below — not a reinvented sweep).
- **Clamp the intent** to the free direction: `Clamped = Intent - (Intent·N)*N`, zero if mostly into-wall.
- Ship the CLAMPED intent → BOTH Mover and the anim consume it.
- Result, all INTENT-driven (no velocity read): straight-in → intent→0 → idle; angled → intent→tangent → loco-along-tangent (slide); **MM/trajectory still PREDICTS, just along the reality-aware free intent.** NO strafe.

**Why clamp-intent not read-velocity:** it's a fix at the INPUT (predictive, keeps v2) vs the OUTPUT (reactive, old-school). Same visible result, opposite architecture. Clamp wins.

**Not redundant with Mover's slide:** Mover resolves VELOCITY (movement, reactive backstop); the clamp resolves INTENT (so the intent-driven anim is correct, predictive). Complementary layers — if the clamp leaves a tiny into-wall component, Mover's collision backstops it; no fight (both produce tangent motion).

## USE MOVER / NETWORKPREDICTION (don't reinvent)
- Clamp lives in **`ProduceInput`** = deterministic input producer → NetworkPrediction replays identically → co-op-safe by construction.
- **INVESTIGATE 2026-06-24:** which Mover collision helper gives the free/slide direction the SAME way the real move resolves it — `UMovementUtils` (slide/penetration/floor helpers), the walking mode's move, or the blocking-hit normal off the last move (`FMovementRecord`/proposed move). Use that; fall back to a sweep that reads the blocking-hit normal ONLY if none is cleanly callable.
- The clearance probe must read the **RAW cached intent** (pre-clamp), NOT the clamped cmd — else it loses the wall the instant you slide off (flicker). Same RAW source must feed the reaction sensor's gate (today it reads `Mover->GetLastInputCmd`, which becomes the CLAMPED value → would blind the sensor on a straight-in hit; switch the sensor to the pawn's raw cached intent).

## KEEP / REMOVE / ADD
- **KEEP:** 3-probe sensor (impact flinches + wall-ahead/normal feed), impact reactions Brace/Stumble/HeadHit (as stops → idle), enum `HeadHit`.
- **REMOVE:** strafe-during-blocked (pawn `bStrafe|=Blocked` + AnimInstance `bStrafe|=Blocked`), AnimInstance HARD intent-cancel, `bObstacleReacting`, the sustained **Blocked** reaction (the clamp makes pinning = zero intent = idle).
- **ADD:** the clearance query + intent clamp = the "where can I move" foundation (PRIMARY tomorrow work).

## BUILD ORDER (each PIE-testable)
1. **Clearance query** (component/util): `IsDirectionClear(dir)` / `GetFreeDirection(intent)` / blocked-normal — reuse Mover collision math where callable. Reusable.
2. **Clamp intent** in ProduceInput via the query → ship clamped (both Mover + anim use it). Sensor reads RAW intent (fix the source).
3. **Verify:** straight-in→idle, angled→slide-with-matching-loco, **intent-driven, no strafe, no velocity-read**; MM predicts along free dir.
4. **Rip out patches:** strafe-during-blocked, intent-cancel, bObstacleReacting, Blocked reaction.
5. **Sensor → impact-only:** drop Blocked latch; keep rising-edge Brace/Stumble/HeadHit one-shots + expose wall-ahead/normal.
6. **Chooser impact=stop:** the 3 impact clips on the **TransitionToIdle (stop)** rows (Brace→`Run2Wall`, Stumble→`KB_Hit_m_LowRight_Med`, HeadHit→`KB_Hit_m_HighFront_Med`); remove dead LocomotionLoop reaction rows + old Stop/Blocked rows (needs `RemoveRowAt` util — only `RemoveColumnAt` exists; add it or neutralize to Any).
7. **Tune** with `bDrawDebug`: probe heights 50/100/165, ImpactMinSpeed, hold times (≈ clip lengths; Run2Wall ~1.7s; KB TBD).

## FUTURE MECHANICS (same query — the payoff)
cover / AI steering / ledge-edge / contextual prompts all read the same clearance query (+ the RAW intent + blocked-normal so a "hold into the wall" context like climb/cover stays readable even though movement is clamped). Ties into [[project_traversal_system]] (trace→chooser→warped-RM | physics-fallback).

## RISKS / MITIGATIONS
- **Corner (two walls):** projecting on one normal may still push into the other → after the first projection, re-query; if still blocked, zero. Or use a Mover "test move" that resolves both.
- **Impact preserved:** clamp uses an IMMEDIATE range (don't decelerate early) so momentum still carries you into a fast hit → the flinch fires; the reaction sensor keeps its own look-ahead. Two ranges, same direction.
- **Flicker:** probe RAW intent (above).
- **Mover-util feasibility:** if no clean predictive collision helper, fall back to a sweep (still reads Mover's normal). Decide in step 1.
- **Feel:** can't shove into walls (input slides/stops) — intended; most action games do this.

## VALIDATION MATRIX (PIE)
straight-in slow → idle (no flinch) · straight-in fast → flinch-stop→idle · angled → slide+matching loco (no strafe) · along-wall → slide loco · corner → no jitter · turn-off → resume · start facing wall → idle (no start flash) · low fast → Stumble · overhead fast → HeadHit · real strafe (combat) into wall → directional strafe slide · MP listen-server → clamp identical (ProduceInput deterministic).
