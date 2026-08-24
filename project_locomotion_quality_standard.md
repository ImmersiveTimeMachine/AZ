---
name: project_locomotion_quality_standard
description: "★★★ THE STANDARD (2026-08-24, Fable-reviewed + engine-verified): how CHALK's CMC hero locomotion is built and judged. Ownership doctrine with a falsifiable rule, the mismatch-resolution ladder, content-derived constants, [CmcRatio] acceptance gate, and prioritised work items P0-P5 with acceptance criteria. Read FIRST before any locomotion/movement-feel work."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-24T01:27:58.398Z
---

# CHALK locomotion quality standard — CMC hero

Not "acceptable" — the best experience this content supports, as a repeatable standard rather than a
tuning session. Supersedes ad-hoc tuning. Detail lives in [[project_cmc_movement_feel_tuning]],
[[project_mm_state_selection_plan]], [[project_cmc_curve_driven_turns]].

## 0. Governing principle

```
Content defines the NUMBERS.  Gameplay owns INTENT.  Animation adapts within BOUNDS.
Discrete events are CONTRACTS, not per-frame ownership.
```

## 1. Ownership doctrine — with a FALSIFIABLE rule

> An animation-derived value may be written to a movement property **only if the PoseSearch trajectory
> predictor also reads that property.**

`BrakingDecelerationWalking` qualifies (predictor reads `GetMaxBrakingDeceleration()`,
`PoseSearchTrajectoryLibrary.cpp:74`) → prediction matches the clip.
`MaxWalkSpeed` does **not**: the predictor reads it as INTENT (`:73`, and `:189` clamps every predicted
step to it), so an animation-derived value inverts `intent → trajectory → clip` into
`clip → ceiling → trajectory → clip` and makes gait upshift unreachable through trajectory cost.

**Curve-driven stops are legitimate because BOTH halves hold:** a stop is a committed terminal event (no
higher-speed future intent left to destroy) AND it writes a property the predictor reads. Neither half
generalises alone. Depends on `BrakingFrictionFactor = 0` and near-linear stop content.

★ **The best seam for any speed modifier is a `GetMaxSpeed()` override** on a CMC subclass
(`CharacterMovementComponent.h:1324`, virtual). `CalcVelocity` reads it once (`:3873`) into a local, and
the predictor calls the same virtual — so the rule above is satisfied **by construction, not by
discipline**. `MaxWalkSpeed` stays pure gait intent, written only by `SetGait`.

## 2. The mismatch-resolution ladder

Apply in order; never skip down a rung to dodge a content problem.

1. derive the constant from content (offline, static) → 2. select better content (state gates decide what
is LEGAL, cost picks among legal) → 3. match a better entry phase → 4. play rate, bounded → 5. stride warp
→ 6. orientation warp → 7. foot lock → 8. explicit contract → 9. re-author content or change the design number.

Epic's own calibration, adopted as ours:
> "It is not uncommon to allow some foot sliding, since too much Distance Matching can result in very high
> play rate, and too much Stride Warping may stretch the pose of the character too much."

**The standard is not zero slide. It is: no visible distortion, no sustained mismatch, no layers fighting.**

## 3. Content-derived constants

| parameter | value | source | status |
|---|---|---|---|
| `WalkSpeed` | 172.6 | walk loop RM | applied |
| `RunSpeed` | 375.7 | run loop | already correct |
| `SprintSpeed` | 641.8 | sprint loop | applied |
| `GroundedRotationRateYaw` | 181 | walk arc yaw | applied ⚠ overrode a stated preference for −1 (instant) |
| `RunRotationRateYaw` | 97 | run arc `_L` | ⚠ 115 was inside the measured 93–116 band — REVERT CANDIDATE |
| `StopTimeSeconds` | 0.93 | stops reach 0 in 0.86–1.03 s | shipped |
| turn speed factor | ~0.70 walk / ~0.80 run | arc avg vs straight loop | P1, unbuilt |
| directional speed | back/lateral run 225–235 vs fwd 375.7 | directional loops | P1, unbuilt |
| `CrouchSpeed` | 90 vs depicted 172.5 | — | **design decision, NOT derivable** |

Starts deliberately excluded: heterogeneous (terminal 114–194, implied accel ~300 walk / ~478 run) — a
range, not a law. `LeanSpeedRangeIn` (`AZ_CmcAnimInstance.h:519`) is a literal copy of the OLD gait speeds
and is now stale.

## 4. Acceptance gate — `[CmcRatio]`

`ActualSpeed / DepictedSpeed` per selected clip, sampled from the **selected sequence at its own time**
(never `GetCurveValue` — blend-weighted, reads the outgoing clip, and treats a missing curve as 0).

`GREEN 0.85–1.15` play rate alone · `YELLOW 0.75–1.25` needs stride warp · `RED` = ladder rung 9.

**Standard: every sustained loop GREEN; nothing sustained RED; transients allowed in blends.**

Baseline measured in play: straight loops 0.87–0.97 (**exactly as the offline derivation predicted — this
validated the whole method**); **arc loops 1.19–1.81** ← largest live mismatch; `WalkFwdStop_LU` 1.23.

⚠ `WalkFwdStart` mean 1.55 / max 4.90 is partly a **metric artifact** — near-zero denominator on early
start frames. Switch one-shot measurement to speed DIFFERENCE (or floor the denominator) before acting.

★ **MISSING METRIC: curvature.** Everything measured is scalar speed. Capsule radius = `V / appliedYawRate`;
content radii ≈ 38 cm (walk arc) and ≈ 172 cm (run arc). P1 can drive the speed ratio to 1.0 while the
RADIUS mismatch — which is what actually reads as lateral foot slide — stays invisible. Add radius to
`[CmcFeel]` before declaring P1 done.

## 5. Work items

### P0 — correctness (they corrupt every measurement above)
- **Empty InAir gate union — the worst bug in this document.** No airborne row; pool empties, node holds a
  one-database pool, search returns a NULL anim: 300–400 ms with no animation, 12× per session. Also
  switches `GetMaxBrakingDeceleration` to the falling value, breaking stop prediction while it lasts.
  *Accept: zero `-> None` selections in a session.*
- **Stale stop-band latch.** `bStopActive` clears on halt (`AZ_CmcCharacterBase.cpp:262`) but
  `bStopBandLatched` only on re-input (`:270`), pinning gates to a stale band at rest (`cmd=Run sel=Walk`).
  *Accept: `sel` never below `cmd` at rest.*

### P1 — TURN SPEED REDUCTION (the original goal; largest live mismatch)
Arcs run 1.19–1.81: the capsule corners at full straight-line speed while arc content travels 25–30% less
ground. Intent-derived, therefore predictor-safe.

- **Seam:** `GetMaxSpeed()` override on a CMC subclass, factor updated per tick from the existing feel pass
  (which already owns per-tick `MaxAcceleration`/`GroundFriction`/`RotationRate`/braking writes).
  ✗ NOT `SetGait` — that is EDGE-triggered and this is a continuous quantity; per-tick calls would spam the
  intent edge and quantised steps are the one thing that genuinely feels abrupt.
- **Driver:** 2D angle between `Acceleration` and `Velocity`. This is exactly what `CalcVelocity:3926-3928`
  resolves, and it means the same thing for player input and AI requested moves.
  ✗ not facing (a rate-limited cosmetic follower), ✗ not predicted curvature (**builds a real algebraic
  loop** — predictor output → MaxSpeed → predictor input), ✗ not applied yaw rate (saturates at
  `RotationRate*dt`, so 40° and 180° read identically).
- **Shaping:** deadband ≡1.0 below ~20°; plateau at 0.70/0.80 by ~45–60°; **saturate by ~90–100°**.
  Asymmetric rate limit (attack ~0.1–0.15 s, release ~0.3 s) — the slow release is what prevents a
  limit cycle (lower speed → faster angle closure → factor recovers → speed rises → angle reopens).
  Gate on `Speed2D > ~50` and non-zero acceleration, which makes it self-vacate when the stop takes over.
- ★★ **`MinAnalogWalkSpeed = 150` SWALLOWS THE WALK CASE.** `CalcVelocity:3902` and predictor `:73` both
  compute `Max(MaxSpeed * AnalogInputModifier, GetMinAnalogSpeed())`. 0.70 × 172.6 = **121 < 150**, so the
  effective walk factor is **0.87, not 0.70**, and walk arcs will barely move. **Scale
  `MinAnalogWalkSpeed` by the same factor IN THE SAME CHANGE**, or P1 will look like it failed at walk.
- **UNEXAMINED COST — steering degrades on entry.** The braking branch (`:3910`) and the friction-bend
  branch (`:3923`) are mutually exclusive. While `bVelocityOverMax`, the strong `GroundFriction` velocity
  bend is OFF; direction change survives only through accel-add-then-clamp (~130 °/s at run). The
  over-max tolerance is ~0.5%, so any descent faster than ~90 cm/s² pins you there for the whole ramp.
  **For 0.15–0.3 s at turn entry — exactly when the player wants the sharpest response — steering
  authority drops.** Log time-in-`bVelocityOverMax` and the velocity-direction rate during it; that number
  picks the ramp duration, and it is in TENSION with the jitter filter (which wants slower).
- **The snap-back is a friend, not a hazard:** `:3918-3921` sets velocity exactly to `MaxSpeed` when
  braking overshoots, so a rate-limited descending cap tracks exactly. Abruptness comes from a STEPPED cap.
- **Prediction improves on the SPEED axis only.** The predictor rotates future acceleration solely by
  `ControllerYawRate` (`:451`), so a stick-steered future is an **elbow**, not a sustained arc, while arc
  clips are indexed with sustained arcs. Expect ratios to compress a lot and arc SELECTION to improve less.
  Orientation warping is the closer for the shape axis.
- **Sprint: do NOT apply a continuous factor.** Sprint has no arc content (the feel pass says so); 0.8 gives
  513 cm/s curving futures matching nothing, and MM would grab run arcs at 315 — ratio 1.63, worse than
  today. Either leave sprint uncapped with its wide carve, or demote hard sprint turns to Run via a
  DISCRETE `SetGait` call (a legitimate edge-triggered intent change).
- **Pivots excluded.** A speed cap cannot produce decel-through-zero-and-reaccelerate. Engine already helps:
  the snap-back requires `Dot(Accel, OldVelocity) > 0` (`:3918`), so reversals dig below the cap naturally.
  Pivots belong to P2/P3.
- **In-place turns excluded by construction** (speed gate makes it a no-op).
- **AI: hero-only or flag-gated, default OFF for infected.** They share `AAZ_CmcCharacterBase`; path
  following clamps `RequestedSpeed = Min(MaxSpeed, ...)` (`:3979`), so waypoint direction steps would spike
  the angle and throttle NPCs at every corner, changing the fight rulebook's chase pacing.
- Second-order: `GroundFriction` is speed-tapered, so lowering speed RAISES friction and tightens the bend.
  Radius changes through two channels, not one. Measure and document.
- *Accept: arc loops GREEN, no periodic oscillation in a held circle, radius within tolerance.*

### P1b — directional speed (fwd/strafe/back per gait)
The shipped GASP pattern, not a novel design (GASP: walk 200/180/150, run 500/350/300). Intent-derived.
Only bites in Strafe/Aiming rotation modes. *Accept: strafe/backward loops GREEN in those modes.*

### P2 — the entry-frame problem (root cause of remaining one-shot error)
Speed-validated start entry (reject candidates whose depicted speed is far from actual); distance-matched
stop phase. Do P1 first — P2 calibrates against the live speed field and P1 changes it.

### P3 — discrete turns: montage + Motion Warping at frame 0
Verified real: `AddRootMotionModifierSkewWarp(..., InStartTime, InEndTime, ...)`
(`RootMotionModifier_SkewWarp.h:30`) and `AddModifierFromTemplate` (`MotionWarpingComponent.h:149`) take
explicit windows — **no notify authoring**. Gives rotation-only warp, `WarpMaxRotationRate` (the 727 °/s
clamp), and `RotationMethod::Scale` (a 180 asset serves a 90 request). Plan:
[[project_cmc_curve_driven_turns]]. Re-derive the Mover-era warp gotchas rather than carrying them over.

### P3b — ORIENTATION WARPING (was missing a work item entirely)
For arc residue with a sparse library this is the standard, highest-leverage closer, and it is the only
thing that addresses the elbow-vs-arc prediction shape limit. Higher value for turns than stride warping.

### P4 — content decisions (no code fixes these)
- **Crouch: 90 or 172.5?** Play rate CANNOT bridge it — required 0.52 is below the 0.8 loop floor, so feet
  depict ~138 against a capsule at 90. Also `CrouchSpeed 90 < StopAnimEnterSpeed 120` means crouch stops
  are never animated today, and keeping 90 reproduces the trajectory pathology by config inside the crouch
  pool (predictor clamps futures to 90 against clips depicting 172).
- **Sprint stop content** — borrows run stops (peak 353–388); at 640 the curve handover stays rejected.

### P5 — infrastructure
- **Stride warping node is ABSENT from the graph** — the whole mismatch budget currently falls on play rate.
- **Curve regeneration script into the repo** — 76 authored clips live under gitignored `Content/Assets`;
  the generator existed only as tool calls. The script is the backup; the assets cannot be.
- **Predictor facing-rate sync:** `RotateTowardsMovementSpeed` is ONE ABP constant while `RotationRate` is
  now per-gait — at least two gaits' facing predictions are wrong today. Same family:
  `MaxControllerYawRate`, `BendVelocityTowardsAcceleration`.

## 6. Rejected — do not re-propose

| approach | why |
|---|---|
| `velocity = inputDir × clipSpeed` | severs the intent channel; upshift unreachable via trajectory cost |
| animation-derived `MaxWalkSpeed` | same; also a silent no-op while crouched |
| blend-weighted `GetCurveValue` as movement authority | missing curve reads as **0**, not absent → phantom half-speeds |
| `RootMotionFromEverything` for locomotion | loops carry full RM; authored direction wrong with 4 pivots |
| per-frame `braking = Speed2D / T` | exponential decay, never reaches zero |
| turn-scaled `MaxAcceleration` | wrong shape — in-place turns and 356 cm pivots need opposite treatment |
| runtime clamp on 727 °/s pivot yaw | guarantees yaw foot-slide; retime the asset |
| turn factor inside `SetGait` | edge-triggered function, continuous quantity |
| predicted curvature as the turn driver | genuine algebraic loop through the predictor |

## 7. Open risks
1. Network prediction: animation-derived movement inputs are absent from `FSavedMove`. SP-first; flagged.
2. One-frame latency: movement consumes the previous frame's anim snapshot. Already shipped for stops.
3. `p.AsyncCharacterMovement` duplicates `CalcVelocity` non-virtually and bypasses overrides. Off, but a landmine.
4. v2/Mover pawns still hold 165/375/585 and crouch 200. Two live generations will drift.
