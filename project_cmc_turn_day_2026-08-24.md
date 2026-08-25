---
name: project-cmc-turn-day-2026-08-24
description: "★★ TURN DAY settled facts (commit 919ff9f): Enable_Warping was off on ALL clips (see feedback file); Get_TrajectoryTurnAngle is a MIS-PORT (accel-vs-vel caps at ~33°, GASP uses future-vs-current trajectory velocity); TIP contract (input-zero latch) built UNTESTED; pivot/starts gates + cascade bias; lean turn-budget rewrite; rotation flat 165 / friction flat 8. Read before any turn/TIP/selection work."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-24T21:05:05.308Z
---

# 2026-08-24 — the turn day (commit `919ff9f`, spike/cmc-backport)

Everything below is BUILT INTO THE BINARY (editor-closed CLI build ran; no Live-Coding-only patches remain).

## Settled facts (measured, engine-verified)

1. **`Get_TrajectoryTurnAngle` is a mis-port** — ours is `angle(Acceleration, Velocity)`, which caps at
   ~25–33° in ANY steady orient-to-movement turn (measured; it is the friction lag, not the turn). GASP's
   same-named function is `angle(Trj_FutureVelocity, Velocity)`, and the predictor bends the future with
   the CONTROLLER YAW RATE (`PoseSearchTrajectoryLibrary.cpp:451,458`), so camera sweeps register there
   and never in ours. Every consumer (IsPivoting 45°, pivot gate 135°, `turn=` logs) inherits the cap.
   **Standing repair: add the GASP signal as a NEW function; do NOT silently re-base the existing one —
   the Starts/Stops/Pivot gates were tuned against the current quantity.**
2. **A healthy turn**: ~10% speed loss (375→339), capsule 170–430 °/s. The old "turn craters speed to
   67–89" happens only near-180 (reversal degenerates the friction lerp into pure decel through zero).
3. **Turn radius = v/ω** is the visible-slide driver: 400 °/s @375 = 54 cm circle under a straight loop.
   Params now: `GroundedRotationRateYaw = 165` FLAT, `bGaitScaledRotationRate = false` (the per-gait
   115/90 rates matched arc clips that no longer play), `GroundFriction 8/8` flat (taper removed — it
   made friction lowest at speed). `RunRotationRateYaw`/`SprintRotationRateYaw` now inert.
4. **Lean**: lateral term is CENTRIPETAL (v·ω) — it exceeded the linear accel budget 2–7× at every
   rotation rate, so the lean was pinned ±1 through every turn. Rewritten: longitudinal vs accel/decel
   budget, lateral vs `Speed2D × LeanTurnRateReference(180°/s)`, combined clamped to 1 (was √2).
5. **Enable_Warping**: see [[feedback-abp-internal-graph-blindspot]] — OrientationWarping was OFF on all
   content; authored =1.0 on 33 straight clips, absent on pivots/TIP/arcs. Clips are GITIGNORED — this
   state exists only on this machine (curve-regen script into the repo is still owed).

## Selection layer (all in `Get_DatabasesToSearch` unless noted)

- **Pivot gate**: ≥135° AND ≥200 cm/s AND **input held** (released stick degenerates the angle to the
  negated velocity heading and held the window open through every stop). `[CmcPivot]` logs open frames.
- **Starts cap 100→50**: turn speed-dips made Starts legal mid-turn; genuine starts enter at 5–23,
  leaks at 67–89, nothing between.
- **Turn-start cascade** (180→135→90 @~50 ms, `MMBlendTime_Ground` 0.75 s stacking blends): fixed with
  `ContinuingPoseCostBias −1.0 over 0–60%` on 19 start clips (rotation completes by 50–60% on every
  measured turn-start; clean exits happen at 495–568 ms, after the window). −0.5 was NOT enough (2 swaps
  survived at cost ≤0.26).
- **Run pivot BlockTransition was authored TAIL-only (86–100%)** — entry legal in the first 87%, which
  WAS the "mid-entry" mystery. Re-authored 10–100%. Walk pair was already head-only.
- **TIP DBs wired**: StandIdle/CrouchIdle gate rows + ShouldTurnInPlace-gated filter.

## TIP contract (built, compiled, **NOT PIE-VERIFIED**)

Plant-rotate-then-move from idle. Hero latches on input edge (Idle, ground, orient-to-movement,
|input−facing| ≥ `TurnInPlaceEnterAngle` 60°, 2-frame debounce) → **zeroes movement input** (NOT
MaxWalkSpeed: analog floor 150; predictor reads MaxSpeed as intent; orient-to-movement keeps rotating on
any nonzero accel). Anim side: contract `bTurnInPlaceActive`/`TurnInPlaceTargetYaw`; `Update_Trajectory`
bends FUTURE facings to the target (world-Z delta — mesh −90 convention never enters) so the MM query
depicts the turn AND the TIP Steering node steers to it (one edit, both consumers); latch edge
invalidates continuing pose (Idle would out-compete). Release: root-arrived (≤30°) / clip ≥70% /
no-selection grace 0.35 s / watchdog 2 s — on release capsule SNAPS to mesh root yaw (OffsetRootBone
Accumulate keeps the root world-stable, so the snap is invisible). `[CmcTip]` LATCH/RELATCH/RELEASE logs.
Knobs: `AZ|Movement|TurnInPlace` on BP_CMC_Hero; kill switch `bTurnInPlaceLock`.
GASP honesty: GASP has NO such lock — its TIP only fires with no input held; ingredients are GASP's
(0-travel TurnLt/Rt clips, TIP Steering, TurnInPlace tag), the lock is ours.

## Open items — QUEUE AGREED WITH USER 2026-08-24 ("yes, queue it that way")

**1st — PIE-verify TIP. 2nd — the START RAMP (user's own mechanism, approved):** a state-latched
transition that ramps `MaxAcceleration` during starts. Latch on the idle→moving edge (or TIP release);
ramp from the content-derived start accelerations (~300 walk / ~480 run, derive offline from the start
clips) back to normal over the start window; release on events + watchdog. Write point stays
`ApplyMovementFeelParams` (already the one owner of MaxAcceleration). DOCTRINE NOTE: this is legal
because it is STATE-latched with content-derived CONSTANTS — not a per-frame animation-derived write;
the predictor stays consistent automatically (it reads live Acceleration and GetMaxSpeed). Targets the
open P2 mismatch: WalkFwdStart ratio 1.55 mean / 4.90 max; starts end at 114 vs loop 172.6. The
reduce-max-speed idea is WRONG for TIP (needs exactly zero: analog floor 150, orient-to-movement keeps
rotating, predicted creep selects walk starts) — both true, they split by case.

- **PIE-verify TIP** (first thing next session): `[CmcTip]` latch on a 180 press from idle, TurnLt/Rt
  selection, release reason distribution. Failure modes pre-analysed: "no selection" grace firing = the
  bent-trajectory query still loses to Idle; watchdog firing = release conditions wrong.
- **Walk reversals flash an arc loop** (~25 ms, cost 1.4–2.3) because `PSD_AZ_Stand_Walk_Pivots` is
  EMPTY — `AnimPro_WalkFwdTurn180_L/R` exist on disk, measured, correct notifies, just not indexed.
- `Get_DynamicPlayRate` still unbound (every clip at 1.0) and StrideWarping still absent — the two
  mismatch-correction rungs; the ABP's warping chain lives INSIDE the MM node's blend-stack graph.
- `ShouldSpinTransition` dead (no callers). Sprint borrows run pivots at 641 vs authored 270–370.
- One start→start swap survived at run (`Start180_R→Start135_R @70ms`); watch, don't fix yet.
