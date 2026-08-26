---
name: project-cmc-rm-locomotion
description: "★ SUPERSEDED 2026-08-26 by [[project-cmc-velocity-master-verdict]] — RM montages retired for locomotion (toggle bRootMotionStarts/Stops), mechanism keeps for committed actions. Was: the RM architecture pivot (commit 343d553) — discrete loco events play as dynamic montages so authored root motion drives the capsule. Hand-back contract at 60%, entry-speed bands for stops, the foot-curve/suffix bugs and their measurements, MM gating under montages, CHT hooks. START HERE for locomotion work on spike/cmc-backport."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-25T05:08:55.023Z
---

# RM locomotion — starts and stops on root motion (commit `343d553`, 2026-08-25)

The pivot away from "MM selects a pose, an impulse approximates the motion". User's call, after three
days proved MM+impulse cannot reach RM fidelity: **MM plays poses; the root is discarded by design.**

## The enabling fact: nothing had to be built

`RootMotionMode = RootMotionFromMontagesOnly` + per-clip `bEnableRootMotion` + a `DefaultSlot` node +
`bAllowPhysicsRotationDuringAnimRootMotion = false` were ALL already true. So
`PlaySlotAnimationAsDynamicMontage` gives clip-exact capsule motion with **no montage assets, no new
components, no config**. Verified live: `rmDelta 0.10→2.22`, `vel 5→113`, `mtgHasRM=1 charHasRM=1`.

**Loops deliberately stay on CMC velocity.** RM loops make the capsule a slave to clip cadence, cost
gait speed as a gameplay dial, and leave network prediction nothing to predict (the Mover generation's
lesson). RM is for DISCRETE events only.

## ★ The hand-back contract — the lockout fix

First cut committed the player for the clip's whole 0.77-1.4s → *"as soon as we start RM animation
inputs are prohibited unless it ends"*. RM was working perfectly the whole time; the fault was the
**commitment**, not the mechanism.

Measured: **every start clip delivers 95-100% of its rotation by 50-60% of its length**; the rest is
run-out the player should own. So: release at `ReleaseFraction 0.60` carrying RM's built velocity, plus
three interrupts — `redirected` (>60° input change), `input released`, `stop cancelled`.
`[CmcRmEnd] <reason> at <pct>% vel=<n>` logs which fired.

⚠ **Stop-cancel needs post-launch input**, not "input held": the 0.1s input-staleness window still counts
frames from just BEFORE the release, so the naive test cancelled **19 of 19 stops at 0%**.

## ★★ Two foot bugs, both measured

1. **Curve name**: `bLeftFootDown` read `FootSpeed_L/R` — curves on **ZERO** clips. `GetCurveValue`
   returns 0 for missing, so `(0 < 1) && (0 <= 0)` = TRUE forever; `_RU` stops were unreachable and half
   of all stops landed wrong-footed. Clips carry `contact_l/contact_r` — **contact flags, opposite
   polarity to a speed** (high = planted). Fixed + only updates when a foot reports contact, so
   curve-less clips (all stops) HOLD the last foot. That matters: the stop foot is decided at the stop
   edge while the LOOP is still playing, and loops are the clips with the curves.
2. **Suffix meaning, bone-sampled**: `_LU` starts with the **RIGHT** foot planted (left **UP**), `_RU`
   with the LEFT. The table mapping was inverted → `bLeftFootDown` must map to the **opposite** suffix
   letter. Verify with `AnimPoseExtensions.get_anim_pose_at_time` + foot bone XY travel over 0-0.15s.
3. ⚠ Staging trap: `bChooserLeftFootDown` is only written inside `EvaluateLocomotionChooser`, which
   **returns early when no chooser is assigned** — so on the array path it kept its default. Read the
   foot from the anim instance directly.

## MM while a montage owns the pose

- Starts/Stops pools stripped for the montage's whole life. **Gate on the game-thread snapshot**
  (`GCmcMontageWasActive`, written in `NativeUpdateAnimation`), NOT `GetCurrentActiveMontage()` — that
  read is cross-thread from the anim worker and failed intermittently (`Walk_Starts` elected at +101ms
  into a playing montage).
- **Third invalidation edge**: montage-just-ended → `InterruptOnDatabaseChangeAndInvalidateContinuingPose`,
  alongside the stop and TIP edges. Without it the blend-out lands into a stale/garbage continuing pose
  (measured `WalkFwdLoop` at cost 5.2-6.99 with the body at 8 cm/s). Hand-back costs went 6.99 → 0.42.
- `[CmcSel:<owner>|mtg=<0/1>]` — owner tag exists because an editor-preview ABP instance polluted the
  trace for an evening; `mtg` is the acceptance test for the strip.

## Selection tables + CHT

`RootMotionStartClips` (gait × direction, 14 rows) and `RootMotionStopClips` (gait × planted foot ×
**entry-speed band** — walk 100-220 vs authored 147, run 250-450 vs 353-388). Outside the band the
curve-driven stop contract owns the stop, and it **stands down** while RM plays (two controllers solving
one deceleration fight). Buckets from `UAZ_LocomotionStateMachine::BucketStartDirection`, now **public**:
one owner for the 45/112.5/157.5 thresholds across movement AND animation.

**CHT is additive and not yet needed**: `RootMotionStartChooser`/`StopChooser` + reflected axis
properties. Null chooser → arrays. Switch when a third axis appears (stance × rotation mode × variants),
when randomised variants are wanted, or when adding rows starts needing rebuilds too often. Playback
code never changes — only where `Clip` comes from.

## Content decisions locked

Arc loops **removed from all three Loops databases** (not just disabled — a revert re-enabled them once).
They were the hand-back landing pad (cost 1.5-2.2) and the sprint-from-idle fallback (7.88). Pivot floor
**200 → 120** (live property) so cancelled-stop reversals at 50-150 cm/s have event content.

## Open next session

1. **Residual small jerk.** Next data: the `mtg=` flag settles whether the Starts strip leaks
   (`mtg=1` + a Starts election = strip bug; `mtg=0` mid-montage = snapshot bug). If clean, the remaining
   candidate is the **blend mismatch** — montage blends out over 0.15s against `MMBlendTime_Ground` 0.5s.
2. Walk pivots DB membership (`AnimPro_WalkFwdTurn180_L/R`) may have been lost in the 919ff9f revert.
3. Sprint has no start/stop content — deliberately falls through to MM.
4. Promote `AZ::RmMontage` file-scope constants to UPROPERTYs.
5. Foot locking still absent (`AllowFootPinning`/`ShouldSpinTransition` dead, no IK node in the graph) —
   the remaining structural gap for carve foot-slide. Skeleton HAS `ik_foot_l/r/root` and they ARE
   animated (16.6/14.0 cm travel), so it is buildable.

Related: [[project_cmc_turn_day_2026-08-24]], [[feedback_abp_internal_graph_blindspot]],
[[project_cmc_movement_feel_tuning]], [[project_mm_state_selection_plan]], [[feedback_retarget_root_motion]]
