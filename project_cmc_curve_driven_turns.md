---
name: project_cmc_curve_driven_turns
description: "★★ PLAN v2 (2026-08-23, Fable-reviewed): make the capsule's ROTATION follow authored turn content. Contains the measured 3-class turn taxonomy, why deg/s curves are the WRONG encoding (turns have no attractor), the remaining-yaw + latched-target design, the verified orient-to-movement/CalcVelocity engine facts that broke plan v1, and an honest comparison against the dynamic-montage + motion-warping alternative. Read before any turn/pivot/TIP work on the CMC hero."
metadata:
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-24T00:07:23.794Z
---

# Curve-driven turns — v2, after review

Branch `spike/cmc-backport`. Follows [[project_cmc_movement_feel_tuning]] (stops) and
[[project_mm_state_selection_plan]] (selection). User proposal: *"on turn we almost don't move, so read
the turn from the animation."* The instinct is right; plan v1's ENCODING and OWNERSHIP were wrong.

## 1. The measurement — turns are THREE classes

Root motion sampled at 30 Hz (2026-08-23):

| class | clips | travel | peak speed | peak yaw |
|---|---|---|---|---|
| **In-place** | `TurnLt/Rt90_Loop`, `TurnLt/Rt180`, `Crouch_Turn90L/R_new` | **exactly 0 cm** | 0 | 104-155 °/s |
| **Moving pivot** | `Walk/RunFwdTurn180_*` (6) | 78-356 cm | 117-442 | **257-727 °/s** |
| **Arc loop** | `Walk/RunArchLoop_L/R` | 120-241 cm | 186-369 | walk **181**, run **97** |

★ Rotation spread 104 → 727 °/s, a factor of SEVEN — wider than the braking spread (2.6) that already
defeated a single constant. ★ The two walk 180s disagree: `_L` 180° in 0.9 s peaking 727; `_R` 1.4 s
peaking 404. Same nominal move, different authoring.

## 2. ★★ WHY v1's ENCODING WAS WRONG — stops have an attractor, turns do not

Every suffix of a stop clip still converges to speed 0, so mid-entry and early eviction are FORGIVEN.
That is the real reason curve-driven stops shipped at 75 engagements / 1 rejection.

A turn's suffix integrates to "whatever was left." Measured entry `clipTime` 0.10-0.33 plus survival
830-1018 ms against 1.267-1.533 s clips = the middle 55-70% plays = **a 180° pivot delivers ~100-140°**.
On the front-loaded `_L` clip (727 °/s early) entering after the peak skips most of the rotation. The
result is PERMANENT HEADING ERROR, re-rolled every turn. An absolute deg/s curve is phase-fragile and
open-loop; it cannot converge.

**The fix is the encoding, not the mechanism: REMAINING YAW.** Author normalised remaining rotation
(1 → 0) instead of deg/s. At any entry frame it states what fraction of the turn is left; drive the
capsule from its per-frame delta scaled to a LATCHED TARGET yaw. Mid-entry then costs turn DURATION only,
never heading accuracy — and it lets a 180 asset serve a 90° request, which raw root motion cannot.
Derivable from the same 30 Hz sampling by integrating backward from clip end.

⚠ `MoveData_TurnRate` (signed deg/s) was already authored on the 12 clips. Keep it for diagnostics; it is
NOT the drive signal. Re-author as `MoveData_RemainingYawAlpha`.

## 3. ★ ENGINE FACTS THAT BROKE v1 (both verified in 5.8 source)

1. **`bOrientRotationToMovement` orients toward `Acceleration`, NOT velocity**
   (`CharacterMovementComponent.cpp:6605-6621` — zero acceleration returns `CurrentRotation`).
   v1 said velocity. The in-place conclusion survives (no input → no rotation) but by a different path,
   and the consequence v1 MISSED: during a moving pivot the stick is HELD, so acceleration is non-zero and
   `PhysicsRotation` rotates toward it at `RotationRate` **while** the curve applies 257-727 °/s. Double
   rotation, plus a sign conflict when the authored pivot direction opposes the shortest path to the stick.
   The blocker is in Stage C too, not only Stage B.
2. **The braking branch needs zero acceleration** (`CalcVelocity`, `:3910`:
   `(bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax`). A pivot has held input, so CMC
   takes the FRICTION branch at `:3923` and `BrakingDecelerationWalking` is never applied.
   **"Reuse the stop path unchanged" for pivots is impossible** — the stop mechanism lives in a branch
   pivots never enter. Pivot speed tracking needs `GroundFriction`/`MaxAcceleration` ownership or an
   explicit velocity target, and braking is clamped >= 0 so it can only REMOVE speed while a pivot
   re-accelerates on exit.

## 4. Ownership — corrected

- **Rotation owner: override `PhysicsRotation`** in a thin `UAZ_CmcMovementComponent`. NOT flag toggling.
  `PerformMovement` calls it only when `bAllowPhysicsRotationDuringAnimRootMotion || !HasAnimRootMotion()`,
  so montage root motion and motion warping (hit reacts, grab) suppress curve yaw automatically; ordering
  inside the movement update is guaranteed; `bOrientRotationToMovement` never flips, so the PoseSearch
  predictor's model stays stable. When a turn is committed apply the delta, else `Super::PhysicsRotation`.
- **Translation pin: ZERO THE INPUT, never `GetMaxSpeed()=0`.**
  `FPoseSearchTrajectoryData::UpdateData` reads `GetMaxSpeed()` LIVE to simulate the future trajectory, so
  pinning it collapses the prediction to "stationary forever" while the player holds a stick — MM loses the
  signal to exit, and the continuing pose is exempt from every filter, so the turn DB goes sticky. Zeroing
  input kills translation AND makes `ComputeOrientToMovementRotation` return `CurrentRotation` for free.
  (Contrast: the stop system works partly BECAUSE `BrakingDecelerationWalking` feeds the predictor via
  `GetMaxBrakingDeceleration()` — prediction matches the clip. Curve-driven yaw gets no such modelling.)
- **A pin is only load-bearing if selection already failed.** In-place clips should only be selectable from
  idle. Log/assert when one is selected with input held — that is a gate bug, not a movement bug.

## 5. Two more v1 misses

- **Play rate.** Pivots carry `MoveData_Speed`, so `Get_DynamicPlayRate` warps their clock. Any rate curve
  is only valid MULTIPLIED BY THE APPLIED PLAY RATE. (In-place clips escape via the
  `MoveDataSpeed <= KINDA_SMALL_NUMBER → 1.0` early-out.)
- **Blend weighting.** `EvaluateCurveData` on the incoming clip reads full value at blend frame 0 while the
  rendered pose is still mostly the OUTGOING clip — the capsule would lead the mesh at every transition,
  which is the artifact this plan exists to remove. `GetCurveValue` has the mirror fault (dilutes toward 0
  against the ~745 curve-less clips). Correct: per-clip `EvaluateCurveData` **weighted by blend-stack
  weights**. This refines the stop-side rule; it does not reverse it.

## 6. Stages (revised)

- **A — publish only.** Blend-weighted remaining-yaw alpha + turn class + applied play rate. No behaviour.
  Turn class must key off the PLAYING asset's owning database, not the current gate set — the continuing
  pose can belong to a database no longer in the set.
- **B — in-place turns.** Latch target yaw at onset, drive from remaining-yaw delta via `PhysicsRotation`,
  zero input for the duration.
- **C — moving pivots.** Needs a NEW speed mechanism (see §3.2), not the stop path. Do not start until B
  is measured.
- **D — arc loops: mostly leave alone.** Run 115 vs measured run arc 97 is fine. **But walk is on
  `GroundedRotationRateYaw = -1` (instant) against a walk arc of 181 °/s** — that one is a real mismatch
  and instant rotation is a prime suspect for the `Start180_L` churn.

## 7. ★★ THE ALTERNATIVE THIS PLAN MUST BEAT — dynamic montage + motion warping

Play discrete turns as MONTAGES entered at frame 0, warped to the latched target; MM keeps the loops.

**What it deletes outright**, rather than compensating for:
- mid-entry (a montage starts at 0 by construction) — the root cause of §2
- the rotation owner problem: `RootMotionMode` is already `RootMotionFromMontagesOnly`, so a montage's root
  motion drives the capsule natively — no `PhysicsRotation` override, no curves
- the translation pin: RM overrides velocity, so in-place clips hold position by themselves
- heading error: Motion Warping closes to the target EXACTLY
- `UMotionWarpingComponent` already exists on `AAZ_CmcCharacterBase` (P0), and **every clip already has
  `bEnableRootMotion = True`** (verified), so `PlaySlotAnimationAsDynamicMontage` needs no montage assets

**What it costs:**
- loses MM's phase-matched entry — but that is measured at a quarter-cycle off with 2 variants/gait, so it
  is trading a broken feature for a working one
- loses cost-based blending between turn variants; state logic must choose the clip
- ~~warp windows need notifies~~ **RESOLVED 2026-08-23 — this cost is ~ZERO.**
  `URootMotionModifier_SkewWarp::AddRootMotionModifierSkewWarp(...)` (`RootMotionModifier_SkewWarp.h:30`)
  is a BlueprintCallable STATIC taking `(MotionWarpingComp, Animation, StartTime, EndTime, WarpTargetName,
  WarpPointAnimProvider, ..., bWarpTranslation, bIgnoreZAxis, bWarpRotation, RotationType, RotationMethod,
  WarpRotationTimeMultiplier, WarpMaxRotationRate)`. **The time window is an explicit argument — no
  `UAnimNotifyState_MotionWarping` required.** The notify is only an authoring convenience that calls the
  same thing (`AnimNotifyState_MotionWarping.h:34`). One call per turn at montage start; zero content edits
  on the 12 clips.
  Three properties matter for turns specifically:
  - `bWarpTranslation=false, bWarpRotation=true` → ROTATION-ONLY warp, exactly what a turn needs
  - `WarpMaxRotationRate` → the 727 °/s clamp, applied at the warp layer instead of desyncing the capsule
  - `EMotionWarpRotationMethod::Scale` → scales the authored rotation, so a **180 asset can serve a 90°
    request**. v1 claimed only renormalised curves could do this. Warping does it natively.
- montage RM replicates; graph RM does not — better for later coop, not worse
- it is the rail doctrine from [[project_combat_fist_build_plan]] and the push path from
  [[project_mm_state_selection_plan]], so it converges with the CHT direction rather than fighting it

**★ VERDICT (2026-08-23, after pricing the warp cost): BUILD OPTION 2, NOT THIS PLAN'S STAGES B/C.**

The warp cost was the only thing holding the comparison open, and it is ~zero. With it priced, option 2
delivers natively every hard part Stages B/C were inventing machinery for:

| what Stage B/C needed | option 2 |
|---|---|
| remaining-yaw curves + latched target | Motion Warping closes to the target exactly |
| `PhysicsRotation` override as rotation owner | montage RM already rotates the capsule |
| input-zeroing to pin translation | RM overrides velocity; in-place clips hold position |
| play-rate scaling of the curve | not applicable |
| blend-weighted curve sampling | not applicable |
| a new pivot speed mechanism (§3.2) | montage RM |
| tolerate mid-entry | montage starts at frame 0 |
| clamp 727 °/s without desync | `WarpMaxRotationRate` |
| serve a 90° request from a 180 asset | `RotationMethod::Scale` |

Keep from this plan: §1 measurements, §2's attractor insight (it is WHY partial turn playback is
unacceptable and therefore why frame-0 entry matters), §8's exit contract and feedback-loop risks — those
apply to option 2 unchanged — and §9's verification metrics.

Retire: Stages B and C as written. Stage D's walk finding stands independently.

Re-read [[project_motion_warping]] first — it records SkewWarp's clip-dependent branches and notify traps
from the Mover generation. Native CMC warping is a different application point, so re-verify rather than
assume those still apply.

## 8. Risks / open

1. **No exit contract.** Stick flick 300 ms into a turn — abandon remaining rotation (heading error) or
   complete it (snap)? Who releases rotation ownership, on what signal? Stops had a natural terminal state;
   turns do not. Must be specified before B.
2. **New feedback loop:** clip → curve → capsule yaw → trajectory → next search. Does not exist today.
   With ties at 0.05-0.26, L/R pivot flip-flop mid-turn is the specific instability.
3. **Network prediction debt** is identical for every option here (the driver is anim state, absent from
   `FSavedMove`) and identical to the debt the shipped stop system already carries. SP-first: accept once,
   do not re-litigate per option.
4. 727 °/s may be unplayable regardless of authoring. A runtime clamp guarantees yaw foot-slide — retime
   the asset or normalise the L/R pair instead.
5. 30 Hz authoring aliases the 727 peak (~24°/sample). Integrals are fine; peak comparisons in logs mislead.
6. Confirm the camera rig reads control rotation, not actor yaw, before letting the capsule spin at 727.
7. `Update_Trajectory` switches Idle/Moving generation data on `Speed2D > 0.f` exactly — epsilon residual
   during a pin flip-flops the config. Needs a tolerance.

## 9. Verification

- capsule yaw vs **mesh root** yaw error across the turn window (the direct analogue of foot slide)
- delivered heading vs latched target — the number §2 exists to fix
- translation during in-place turns must be **0**, not "small"
- turn clip survival (today: pivots 11-43% of clip length) before/after
- MM eviction timing before/after, to catch the §8.2 feedback loop

Related: [[project_cmc_movement_feel_tuning]], [[project_mm_state_selection_plan]],
[[project_root_motion_mode]], [[project_motion_warping]], [[project_combat_fist_build_plan]].
