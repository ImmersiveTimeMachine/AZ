---
name: project-cmc-mm-content-verdict
description: "★★★ 2026-08-27 audit VERDICT: why CMC MM never reached Mover fidelity — the -5 ContinuingPoseCostBias is INERT (clip notify overrides it), zero ExcludeFromDatabase in AZ vs 682 in GASP, and the loops DB holds ONE clip. Content-layer problem, not code."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-27T18:26:47.795Z
---

# Why CMC motion matching never reached Mover fidelity — content, not code (2026-08-27)

## ⇢ WHERE THE CODE LIVES (end of 2026-08-27)

- `spike/cmc-backport` was RESET to **`69c1a6e`** (frozen MM baseline) at the user's request, and rebuilt
  clean. This is the working MM state.
- The whole BlendStack-spine day is committed as **`01e0ac3`** on branch
  **`spike/cmc-blendstack-spine`** — chooser spine, CHT_CMC (99 rows), velocity steering, Mover
  movement-mode tunables, the 27 PSD `ExcludeFromDatabaseParameters` fix, OW threshold/WarpingSpace,
  trajectory turn rate, and the `AZ_PoseSearchUtils` reselection bridge. `git checkout
  spike/cmc-blendstack-spine` to resume.
- NOT committed (untracked, unrelated): `Plugins/`, `Content/AZ/Blueprints/Character/AZ_MHC_Hero/`.
- ⚠ The spine's last C++ change (velocity steering) was **never compiled** — Live Coding stopped
  responding to the console command for the rest of the session (see
  [[feedback_build_paging_file_parallelism]] § never-kill-LiveCodingConsole). Build it before judging
  that branch.

Five days of code iteration on `spike/cmc-backport` chased a **content-layer** defect. Verified by a
notify/database audit of all 27 `PSD_AZ_*` databases (56 clips) against GASP 5.8's 160 databases (2425
entries, 1156 clips), both live in this project.

## ★★★ 1. The `-5` pick-once fix has been INERT since the day it landed

Engine, `PoseSearchDatabase.cpp:1769` then `:1820-1822`:
```cpp
ContinuingPoseCostAddend = ContinuingPoseCostBias;            // DB-level, e.g. -5.0
if (const UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias* Notify = Cast<...>(...))
    ContinuingPoseCostAddend = Notify->CostAddend;            // HARD ASSIGN — replaces, not adds
```
Commit `69c1a6e` set `ContinuingPoseCostBias = -5.0` on six one-shot DBs (2026-08-26 23:29). But every
member clip already carried an `OverrideContinuingPoseCostBias` notify with the engine default
`CostAddend = -1.0`, authored **2026-08-24** — two days earlier. The notify outranks the database.

| DB | DB bias | notify | notify covers | effective |
|---|---|---|---|---|
| Run_Starts / Walk_Starts | -5.0 | -1.0 | 0–60% | -1.0 for 60%, -5.0 only on the tail |
| **Run_Stops / Walk_Stops** | -5.0 | -1.0 | **0–100%** | **-1.0 always — -5.0 never applies** |
| **Run_Pivots** | -5.0 | -1.0 | **0–100%** | **-1.0 always — -5.0 never applies** |
| Walk_Pivots | -5.0 | -1.0 | 0–45% | -1.0 for 45% |

So "pick-once" ran at 1/5 strength on starts and **zero** on stops/pivots. The GREEN measurement that
"proved" it was taken on the 40% start tail — the only region where -5.0 survives. ⚠ Changing `CostAddend`
rebuilds the search index → expect the [[feedback_posesearch_branchin_db_sync]] RESTART GOTCHA.

## ★★★ 1b. THE CUT: `ExcludeFromDatabaseParameters` deleted the last 0.3 s of every one-shot from the index

**FIXED 2026-08-27 ~16:40** — set `(Min=0, Max=-0.3)` → `(0, 0)` on all 27 `PSD_AZ_*` and saved.

`PoseSearchDerivedData.cpp:420` — for a NON-LOOPING asset the indexed range is `[Min, PlayLength + Max]`
= `[0, len − 0.3]`. `PoseSearchIndex.h:188-193` — past the last indexed sample `GetPoseIndexFromTime`
returns `INDEX_NONE`, so **the continuing pose stops existing** and MM is forced into a full re-search with
no continuing-pose protection (the −5 bias cannot help — there is nothing left to bias).

AZ's clips are 0.6–1.5 s, so the default deleted **20–50%** of every one-shot (median ~30%; GASP's 2.6–5 s
clips lose only ~8% from the same default). Measured match: `AnimPro_WalkFwdStart`/`RunFwdStart` are
**0.767 s → last indexed frame 0.467 s**, and the long-standing observation was "MM cut every start at
~450 ms". Same number, to within a frame. Precedent for the override: GASP's own
`PSD_SM_CMC_Transitions` (short segments) ships `ExcludeFromDatabaseParameters=(Max=0.0)`.

⚠ Requires an EDITOR RESTART to reindex — [[feedback_posesearch_branchin_db_sync]] restart gotcha.

## ★★★ 1c. `bDisableReselection` is FALSE on all 56 AZ entries; GASP sets it TRUE on all 1604 — STILL OPEN

`PoseSearchDatabase.cpp:1557-1583`: when the continuing pose belongs to this DB and the entry has
`bDisableReselection`, every pose of that source asset goes into `NonSelectableIdx` — MM may continue the
clip or leave it, but can NEVER re-enter it at a different frame. This is the engine's designated guard
against "MM re-cuts the clip it is already playing". The fallback guard at line 1584 needs
`PoseJumpThresholdTime != (0,0)`, and **both** AZ's and GASP's MM node ship `()` — so GASP is protected by
the flag alone and AZ has **neither guard**. Explains the same-family flips (`Stop_RU ↔ Stop_LU`,
`Turn180_L ↔ Turn180_R`) seen in the log.

**BLOCKED from Python**: the entry struct is unreachable — `db.get_animation_asset(i)` returns the
`AnimSequence`, not `FPoseSearchDatabaseAnimationAsset`, and there is no `animation_assets` property.
`UAZ_PoseSearchUtils` has no setter. Needs a NEW UFUNCTION → editor-closed CLI build (LC cannot add
UFUNCTIONs).

## ★★★ 0. WHICH BRANCH RENDERS: read the Alpha PIN, not the node struct

Two independent audits + the compiler source settle it. `AnimGraphNode_TwoWayBlend_0`
(`DFFADB24470D87A07EF197B00C9DDDDB`) exports `PinName="Alpha" DefaultValue="1.000000"`, no `LinkedTo`,
no binding. `AnimBlueprintCompiler.cpp:465-492`: when a pin is **exposed but neither connected nor bound**,
the compiler writes the **pin default** over the struct value into the generated class. So:

- Python `node.blend_node.alpha` → **0.0** = the stale pre-compile EDITOR struct. NOT what runs.
- Compiled/runtime Alpha → **1.0** ⇒ pin **B** ⇒ **the chooser spine renders; the MM node is dormant.**
- With `bAlwaysUpdateChildren=false` (GASP ships `true`), `FAnimNode_TwoWayBlend::Update_AnyThread`
  updates only child B — so the MM node's `UpdateMotionMatching` / `Get_MMBlendTime` never execute.
  That is why "the MM node doesn't play any anim": by design at Alpha 1.

⚠ **To A/B the MM baseline you must edit the Alpha PIN value in the graph**, not the details-panel struct.
`IsChooserSpineDriving()` reads the compiled value at runtime, so C++ always agrees with what renders.
Log signature confirming the spine is live: `db=None` = direct chooser pick; `db=PSD_*_Loops` = the spine's
OWN `UPoseSearchLibrary::MotionMatch` call for `bUseMM` loop rows — neither comes from the MM node.

## ★★ 0b. Spine-path defects found by the Mover-vs-CMC diff (ranked, Alpha=1 = live config)

1. **Continuing-pose time drift — SELF-INFLICTED 2026-08-27, REVERTED same day.** I passed
   `Continuing.PlayingAssetAccumulatedTime = CurrentSelectedTime`, which is dead-reckoned `+= DeltaSeconds`,
   while the BlendStack plays that clip through `Get_DynamicPlayRate` at **0.6–1.5×**. The continuing pose
   was evaluated at a frame the character was never on → wrong cost → wrong keep/abandon. **Mover passes a
   default-constructed `FPoseSearchContinuingProperties()`** (`AZ_MoverAnimInstance.cpp:867`). Reverted to
   Mover parity. Re-add ONLY with the BlendStack's real playhead.
2. **Pool width at LocomotionLoop**: Mover searches **exactly ONE** database (gait+stance gated);
   CMC searches the gate union minus 8 excluded tags. Near-ties in *different* DBs flap frame to frame.
3. **Trajectory turn OVER-prediction: ~573 °/s predicted vs 180 °/s actual.** `TrajectoryGenerationData_Moving`
   is pure engine default (`RotateTowardsMovementSpeed=10`), and `QInterpConstantTo` clamps to 1 rad per
   0.1 s step = 573 °/s, while the pawn's `GroundedRotationRateYaw` is 180 °/s. The query describes a much
   sharper turn than the pawn performs → turn/pivot poses win → reality diverges → re-pick.
   Suggested `RotateTowardsMovementSpeed ≈ 3.1` (180°/s × 0.1 s = 0.314 rad).
4. `ForceBlendNextUpdate` is dead — `Update_ChooserSpine` passes an empty `FAnimNodeReference()`
   (classic unwired-AnimNodeReference silent fail-closed). Mover's EventGraph passes a real node ref.
5. History is frame-rate dependent: trajectory `SamplingInterval -1 × 30`, PoseHistory `SamplingInterval 0`,
   `PoseCount 2` (foot velocities differenced over ONE frame). Mover uses fixed **0.04 s** for both.
6. `HandleTransformTrajectoryWorldCollisions` runs every frame on CMC; Mover never runs it.
7. `Get_DatabasesToSearch` uses `Result.Add` not `AddUnique` (a DB in two matching gates lands twice);
   `LoopExcludedTags` lists `"IdleBreaks"`, a tag no database carries (`PSD_AZ_Crouch_IdleBreaks` is `Idles`).

Ruled out: BlendStack node settings (byte-identical to Mover), PoseHistory bones/curves vs schema, schema
mismatch (shared), database tags, and — importantly — **`ContinuingPoseCostBias -5` is inert on the spine
path** anyway (continuing pose supplied only for loops, and the LocomotionLoop pool excludes those tags).

## ★★ 0c. LANDED 2026-08-27 17:42 (LC patch verified: "Manual recompile triggered" + "Patch creation ... successful")

1. **Trajectory turn rate — affects BOTH branches (the query is shared).** `TrajectoryGenerationData_Moving/
   _Idle.RotateTowardsMovementSpeed` was the engine default **10.0**. That value is **radians/sec** for
   `QInterpConstantTo`, i.e. **573 °/s**, while `BP_CMC_Hero`'s `CharacterMovement.RotationRate.Yaw` is
   **360 °/s** (measured, NOT the 180 an earlier report assumed). The predicted trajectory therefore
   described turns ~59% sharper than the pawn ever makes → turn/pivot poses win the search → reality
   diverges → re-pick. Set to **6.2832 rad/s** on both structs (= 360 °/s). Formula:
   `RotateTowardsMovementSpeed = RotationRate.Yaw * PI / 180`. ⚠ It is an ABP **CDO** value — set in memory
   via Python, persists only when the USER saves the ABP.
2. **LocomotionLoop pool → EXACTLY ONE database** (Mover parity). Was the whole gate union minus 8 tags;
   Mover hands `MotionMatch` a single gait+stance-gated loops DB. Union search let near-ties in *different*
   DBs win on alternate frames. Now `break`s after the first untagged DB (gate order puts the current
   gait's loops DB first).
3. **`ForceBlendNextUpdate` is NOT a gap** — verified: the block is guarded by `bForceBlend`, and both AZ
   and Mover pass a literal `false`. Dead in both = already parity. Do not "fix" it.

**Alpha pin left at 0.000000** (MM baseline) at the user's request — set via
`AZ_BlueprintNodeUtils.set_pin_default_value(abp, "AnimGraph", "DFFADB24…", "Alpha", "0.000000")`.
Set it to `1.000000` to test the spine.

## ★★★ 0d. THE UNPORTED LAYER: Mover's MOVEMENT MODES (found 2026-08-27, user's question)

The whole spike ported the ANIMATION side and never ported the CAPSULE side. Mover's feel comes as much
from `BP_MovementMode_Walking` ([[gasp_movement_modes]] §8) as from its ABP. Measured gap on
`BP_CMC_Hero` before the fix:

| | Mover | CMC (was) | set to |
|---|---|---|---|
| Accel walk / run / sprint | 500 / 800 / **1100** (sprint additive) | base 400 → **300 at speed** (INVERTED) | base 500, top 1100, taper **165→585** ⇒ run 375 lands on exactly 800 |
| Decel, input RELEASED | **1000** (StoppingDeceleration) | 190 walk / 500 run / 725 sprint | 1000 all three |
| Decel, input HELD over gait speed | 300 (GaitChangeDeceleration) | 500 | 300 |
| Facing times walk-run / sprint / idle | 0.4 / 0.8 / 0.2 | 0.4 / 0.8 / 0.2 | already parity ✓ |

Applied to the `BP_CMC_Hero` **CDO and saved — pure data, needs NO compile, just PIE.**

⚠ **Speeds deliberately NOT copied.** Mover's 165/375/585 are derived from GASP's clips; ours
(172.6 / 375 / 641.8) are derived from the `AnimPro_*` clips and are already correct — the loops measure
`[CmcRatio] raw 1.00`. Copying Mover's speeds would BREAK that. Crouch 90 vs Mover 200 is likewise
content-driven (verify against the crouch loop before changing).

⚠ **The invented stop contract is now OFF** (`stop_time_braking=False`, `stop_curve_braking=False`).
Mover has no such thing: it stops at a flat 1000 cm/s² and lets motion matching cope. The AZ time-based
stop (`StopTimeSeconds`, and the 2026-08-27 gait-split to 1.35 s for run) made stops FLOATIER, i.e. the
opposite of Mover — 375→0 takes 0.375 s under Mover's 1000 vs 1.35 s under the time contract.
`AZ::CmcStop::AuthoredStopTime_Run` and the clip-velocity override remain in source but are bypassed.

## 2. BlockTransition was NOT the problem (hypothesis falsified)

39/56 AZ clips carry `BlockTransition`, uniformly 10–15% → 100% (machine-applied via
`AddBlockTransitionToDatabase`). Entry into one-shot middles is already blocked. Semantics confirmed:
BlockTransition blocks **entry only**; playback advances through it fine.

## 3. Zero `ExcludeFromDatabase` in AZ vs 682 clips in GASP

GASP's real pattern is **low DB bias + aggressive pose exclusion**, not a big continuing bias — its
strongest `ContinuingPoseCostBias` anywhere is **-0.3** (AZ uses -5.0, ~17× stronger, as a substitute).
Of GASP's 1441 Exclude bands, **801 (56%) are head-anchored at t=0**, trimming the full-speed lead-in off
stops and pivots so those "still running" poses cannot be matched while the player is still running.
AZ leaves the first 10–15% of every stop/pivot both indexed AND enterable (BlockTransition starts only
after it) — so the cheapest match for a running character is often the head of a stop or a 180° pivot.

## 4. ★ The density ceiling: the loops DB holds ONE clip

Enterable poses (indexed − Exclude − BlockTransition, 30 Hz), running pool:

| | AZ | GASP Dense |
|---|---|---|
| Run_Loops | 1 clip / **23 poses** | 20 / 1173 |
| Run_Starts | 7 / 29 | 28 / 1315 |
| Run_Stops | 2 / 8 | 20 / 308 |
| Run_Pivots | 4 / 15 | 136 / 6675 |
| **total** | **14 / 75** | **204 / 9471** |

**69% of enterable poses in the running pool are one-shots.** No strafe/backward/arc loop exists in the CMC
loops DBs at all, so any non-dead-ahead trajectory has no loop pose to match and its nearest neighbour
*must* be a one-shot. Walk pool is the same shape (12 clips / 83 poses vs 204 / 17561).

**Six DBs are EMPTY**: `Stand_Sprint_Starts`, `Stand_Sprint_Stops`, `Stand_Sprint_Turns`,
`Stand_GaitTransitions`, `Crouch_Walk_Pivots`, `Crouch_Walk_Turns` — sprint has no one-shot vocabulary.

## 5. Other findings

- **Stale cross-link**: `AnimPro_RunFwdLoop` / `AnimPro_WalkFwdLoop` carry `BranchIn` notifies pointing at
  the legacy **`PSD_v2_Loco_Loops`** (pre-CMC leftover; BranchIn injects the clip into that DB).
- AZ has **0** `ModifyCost` and **0** `SamplingRange` notifies (GASP: 22 / 6).
- No null-class "corpse" notifies in the `AnimPro_*` set — unlike `RT_NWP_*` (see
  [[reference_gasp_anim_notifies]]). These are real, live notify objects.
- **Missing clip curves** (verified separately): `Enable_Warping` absent on `AnimPro_WalkFwdStop_RU` and
  `AnimPro_RunFwdTurn180_R_RU` → orientation warping silently OFF there (missing curve reads 0, see
  [[feedback_abp_internal_graph_blindspot]]). `contact_l`/`contact_r` exist ONLY on WalkFwdLoop and
  RunFwdLoop; `AnimPro_Idle` has zero curves.

## Method note (reusable)

`seq.get_editor_property('notifies')` **is protected from Python**. Use
`unreal.AnimationLibrary.get_animation_notify_events(seq)` + `get_anim_notify_event_trigger_time/_duration`,
and cross-check with `unreal.ObjectIterator()` over `AnimNotifyState_PoseSearch*` filtered by outer package.
Do NOT read a protected-property exception as "this clip has no notifies".

Related: [[project-cmc-chooser-spine-landed]], [[project_cmc_velocity_master_verdict]],
[[project_gasp_cmc_abp_spec]] (already recorded the density blocker), [[feedback_posesearch_branchin_db_sync]]
