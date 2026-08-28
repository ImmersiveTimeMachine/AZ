---
name: project-cmc-chooser-spine-landed
description: "CMC BlendStack+CHT spine WORKING (2026-08-27) — the two-selector fight root cause, the 4 fixes that made it Mover-clean, what remains (stop-speed RED, sprint untested)"
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-27T16:14:34.910Z
---

# CMC chooser spine — the day it started working (2026-08-27)

Branch `spike/cmc-backport`. After the graph rewire (UseCache→Slot'FullBody'→ORB→PoseHistory→Root bypassing the broken lean/additive section), the 15:33 PIE log is Mover-clean: pick-once transitions, full clips (dt 450–1900ms), pivots go loop→RunFwdTurn180_*/WalkFwdStart180_* DIRECTLY with moveTrans=1 (no stop-flash), [CmcRatio] GREEN on starts/turns/loops.

**Root cause of "each iteration worse" (the 15:07 horror log): TWO SELECTORS.** The MM node (TwoWayBlend branch A) still ticked pre-ABP-compile and its `Update_MotionMatching_PostSelection` published into the same `CurrentSelectedAnim` bookkeeping the spine uses. Its pool keeps Stops/Starts (keep-guards) → stop↔loop overwrites every 22ms. Diagnostic key: in `[CmcSel]`, `db=PSD_*`+cost = MM-search pick, `db=None`+cost 0 = direct chooser row; any `db=*` line at a Transition state = the wrong selector.

**The 4 fixes (all landed, LC'd green):**
1. `Update_MotionMatching`/`_PostSelection` hard-return when `LocomotionChooser` set — two-selector fight impossible by construction ([[project-cmc-velocity-master-verdict]] baseline untouched when chooser null).
2. Intent off-edge grace, `IntentGraceFrames=10` in `AZ_CmcCharacterBase.cpp FillAnimContract` — WASD/stick reversals cross zero intent for a few frames; raw edge fired a stop-flash + Randomize re-roll (different start clip each SM flap) on every held direction change. v2's pawn input ramp bridges this implicitly.
3. Spine MotionMatch now passes `FPoseSearchContinuingProperties(CurrentSelectedAnim, CurrentSelectedTime)` **only while `bCurrentAssetLooping`** — loops get continuity (no per-tick argmin flap); one-shots excluded so a finished stop's −5 DB bias can't resurrect at LocomotionLoop.
4. CHT_CMC sprint rows 85–98 had **NULL assets** (earlier authoring never persisted; null row matches → selects nothing → previous clip keeps playing = "sprint stuck"). Refilled via `AZ_ChooserUtils.set_cell_asset_on_sub(path, "", row, asset_obj)` (THE row-asset setter; preserves cells), verified 0 nulls, compile_and_save.

## ★★★ ROOT CAUSE of "big diff / very weird" (found 2026-08-27 ~16:00): TWO BlendStack nodes

`AZ_ABP_CmcAnimInstance`'s AnimGraph contains **two `AnimGraphNode_BlendStack` nodes, identical in every
setting** (same `Tag="BlendStack"`, same 5 `BlendStackInputs.*` bindings, same MDT/MaxActiveBlends=4/
HermiteCubic/StitchBlendTime) — they differ ONLY in their inner (BoundGraph) content:

| GUID | inner graph | was |
|---|---|---|
| `1AED997F` | **EMPTY** — Input→Output, nothing else | **WIRED to TwoWayBlend.B (live!)** |
| `535B7750` | FULL chain: `Input(PlayRate ← Get_DynamicPlayRate) → LocalToComponent → Steering ×2 (Enable_Steering, Get_DesiredFacing) → OrientationWarping (Alpha ← Enable_Warping curve, LocomotionDirection ← LastNonZeroVelocity) → ResetRoot → ComponentToLocal` | **ORPHANED — no output link** |

So every clip the spine picked played completely **raw**: no dynamic play rate, no orientation warping, no
steering. That is the entire "far from Mover / transitions weird / MM is degrading" complaint — selection
was fine, post-processing was simply not in the pose path. Proof in `[CmcRatio] raw` (= Speed2D / clip's
authored MoveData_Speed; 1.0 = feet match ground): **starts raw 1.23–1.67**, **stops raw 0.30–0.66**, loops
raw 1.00 (loops match only because gait speeds were tuned to the loop clips).

**Fix**: `connect_pose_link(ABP, 535B7750, DFFADB24 /*TwoWayBlend*/, "B")`; orphan retagged
`BlendStack_Unused` (two live nodes sharing Tag "BlendStack" is an AnimNodeReference-lookup hazard).
Tag check that made the swap safe: 535B7750's inner `BlendStackInput` carries tag
`State Machine Blend Stack Input`, matching its own AnimNodeReference nodes (the MM node's uses
`MMBlendStackInput`; the empty one had **no tag**, so even a play-rate wire could never have resolved there).

⚠ **Diagnostic rule earned here**: when an ABP "plays the right clip but feels wrong", dump AnimGraph
CONNECTIVITY (`AZ_BlueprintNodeUtils.list_function_nodes(abp,"AnimGraph")`) before theorising — a duplicate
node with the right settings and the wrong graph is invisible to any property/settings inspection.

⚠ **Reporting rule**: with no play-rate wire, the `eff` column is FICTION (it divides raw by the rate the
warp *would* apply). Quote **raw** for the spine, `eff` only for the MM baseline. Calling eff "GREEN" while
raw said 1.6/0.4 is what made the model report "clean" on a run the user saw as badly wrong.

## ✗ FAILED EXPERIMENT — do not retry: RM montages triggered from the chooser spine (2026-08-27 15:47)

Routing spine-picked one-shots to `PlaySlotAnimationAsDynamicMontage` (the `343d553` RM mechanism) was a
**total disaster** and is fully reverted (LC patch 15:52). Three independent defects, all measured:
1. **One-frame lag** — the spine was moved to the game thread (`NativeUpdateAnimation`) to reach the montage
   API, so it read LAST frame's SM state. RM then committed the capsule to a state already gone:
   `[CmcRmPlay] STOP WalkFwdStop_RU | spd=0` with `SM=IdleLoop` (stop played after the stop finished),
   `[CmcRmPlay] START WalkFwdStart | spd=173` (start clip at full walk speed).
2. **No speed/cooldown gates** — the retired character-side `TryPlayRootMotionStart` rejected on
   `Velocity > RootMotionStartMaxSpeed` and had a cooldown; the spine executor had neither, so 180-turns
   fired at spd=9/27/78 back-to-back.
3. **Double drive** — the move handler still called `AddMovementInput` while root motion moved the capsule.
Structural lesson: **a pose selector must not trigger capsule-committing montages.** RM commitment belongs
at the input edge (where gates and cooldowns live), one frame earlier than any anim-side selection.
The spine is back on the anim worker immediately after `Update_LocomotionStateMachine` — same thread, same
frame as the SM it selects from.

## ★★ A/B SWITCH: the graph's TwoWayBlend Alpha is the ONE OWNER (fixed 2026-08-27 16:14)

`LocomotionChooser != nullptr` was used as "the spine is driving" in FOUR places. It is true whenever the
asset is merely **assigned**, so it never agreed with what the graph was actually blending. Damage:
- `Update_MotionMatching` returned early → MM node got no databases → **MM branch played nothing at Alpha 0**.
- With that guard removed, BOTH selectors published → the spine's `PublishSelection` overwrote
  `CurrentSelectedAnim` / `CurrentDatabaseTags`, and **`Get_DatabasesToSearch()` keep-guards read
  `CurrentDatabaseTags`** → the spine was choosing the MM node's search pool for it ("MM chooses worse").
  Log signature: `db=None cost=+0.00` (spine direct pick) interleaved with `db=PSD_* cost>0` (MM search)
  in one run.
- `Get_OffsetRootRotationMode/TranslationMode` forced **Release on both branches**, flattening MM's turns
  (MM baseline wants Accumulate).

Fix: file-local `IsChooserSpineDriving(const UAnimInstance*)` in `AZ_CmcAnimInstance.cpp` reads the compiled
`FAnimNode_TwoWayBlend::Alpha` via `IAnimClassInterface::GetAnimNodeProperties()` (same pattern as
`FindOffsetRootBoneNode`). **Alpha ≥ 0.5 = spine (B), < 0.5 = MM baseline (A)**; all four sites gate on it.
The artist's Alpha literal in the AnimGraph is now the only switch — set it in the graph, C++ follows.

**Open items:** (a) STOPS measure RED — raw≈eff 0.43–0.66: capsule brakes ~2x faster than clip depicts (curve-driven-braking tuning on BP_CMC_Hero, see [[project-cmc-movement-feel-tuning]]); play-rate warp not applied to stops. (b) Sprint repaired but UNTESTED in PIE. (c) Leans disconnected (rebuild Mover-style additive layer after acceptance). (d) JumpIdleLand plays at PIE spawn (bJustLanded from spawn-drop, cosmetic). (e) Uncommitted: all of the above + ABP graph.
