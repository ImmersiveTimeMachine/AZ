---
name: anim-debug-pitfalls
description: Diagnostic checklist for AnimGraph/AnimBP issues — mesh visible spin, A-pose, SM stuck in source state, foot slide, OffsetRootBone misbehavior, anim always playing from frame 0. Covers the Post-event vs ThreadSafe driver-placement rule, the FAnimNodeReference must-be-wired rule (unwired = silent fail-closed = SM stuck), OffsetRootBone enum value mapping (Accumulate=0, Interpolate=1, Release=5 — NOT 0/1/2 with comment-mismatched names), the Get_OrientationIntent threshold-cache pattern (cache only on |delta| ≥ 60°), the IsMoving = Trj_FutureVelocity+Accel (NOT velocity) rule, and the nested-BlendStack-in-sample-graph rule (StartTime/MM SelectedTime silently ignored, everything plays from 0).
---

# AnimGraph / AnimBP Diagnostic Pitfalls

Six recurring failure modes in the AZ AnimBP port. Each section: **observed symptom → check this → fix recipe → memory file with full context**.

> Always show the user full absolute paths in any output (e.g. `C:\UnrealEngine\Games\AZ\Source\AZ\Private\Animation\AZ_AnimInstance.cpp`).

---

## 1. Mesh visibly spinning / phantom offset / mesh drifts from capsule

**Symptom shapes:**
- Pressing W → actor translates correctly, camera fixed, but **mesh body rotates clockwise continuously**.
- Mesh "leans" away from capsule and never recovers.
- Removing the OffsetRootBone node makes the spin stop (proves OffsetRootBone is the consumer).

**Root cause — wrong driver thread.**
- `BlueprintThreadSafeUpdateAnimation` runs on a **worker thread BEFORE AnimGraph evaluation** with stale/cached actor data.
- `BlueprintPostEvaluateAnimation` (Post-event graph) runs on the **game thread AFTER AnimGraph evaluation** with the actor transform Mover just produced.
- OffsetRootBone, root motion consumers, and any AnimNodeReference call that reads the *just-rotated* actor transform → MUST be in **Post-event**, not ThreadSafe. Otherwise it sees a phantom delta that never settles → mesh chases a moving target → continuous spin.

**Check:**
1. Open the AnimBP, find which event the OffsetRootBone consumer / `ResetOffset` / `SetTargetRotation` call is parented to.
2. Compare to the GASP equivalent at `/Game/Blueprints/SandboxCharacter_Mover_ABP` — query via `mcp__unrealclaude__unreal_blueprint_query` `get_graph` on the Post-event graph.
3. If GASP has it under `BlueprintPostEvaluateAnimation` and AZ has it under `BlueprintThreadSafeUpdateAnimation` → that's your bug.

**Fix:** Move the consumer from ThreadSafe to Post-event. Anything that touches:
- Mesh-vs-actor transform deltas (OffsetRootBone, foot IK against actor root)
- Consumed/extracted root motion (`ConsumeExtractedRootMotion`, `GetExtractedRootMotion`)
- AnimNodeReference library calls that mutate node state per-frame (`ResetOffset`, `SetTargetRotation`, `SetBlendStackAnimFromChooser` when its target depends on current actor rot)
- `Try Get Pawn Owner` reads of rotation/location for use in same frame's AnimGraph

**Full context:** `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\feedback_animbp_post_event_vs_thread_safe.md`

---

## 2. A-pose / SM stuck in source state / transition never fires

**Symptom shapes:**
- Character renders in T-pose or A-pose / ref pose only.
- SM gets stuck in `Transition to Locomotion` and never advances to `Locomotion Loop` (or any other Trans→Loop hop).
- A specific transition rule "looks correct" but never returns true.

**Root cause — invalid `FAnimNodeReference` from unwired pin.**
Engine helper libraries fail-closed when the reference is invalid:
- `UBlendStackAnimNodeLibrary::IsCurrentAssetLooping(invalid)` → returns `true` (default).
- `UBlendStackAnimNodeLibrary::GetCurrentAssetTimeRemaining(invalid)` → returns `0`.
- `UBlendStackAnimNodeLibrary::ConvertToBlendStackNode(invalid)` → reports failure silently.

In `IsAnimationAlmostComplete(BlendStackNode)` if the input is invalid → `bLooping=true` → returns `false` forever → SM stuck.

**Check:**
1. Open the failing transition rule in the editor.
2. Confirm there is a `K2Node_AnimNodeReference` node in the rule sub-graph whose `Tag` field matches the AnimGraph node's title text exactly (e.g. `State Machine Blend Stack`, `Motion Matching`).
3. Confirm the AnimGraph node itself has its `Tag` Details-panel field populated with the same string (visible title is NOT the same as the Tag field).
4. Confirm the `Value` output of the K2Node_AnimNodeReference is wired into the helper function's `BlendStackNode` (or equivalent FAnimNodeReference) input pin.

**Critical MCP gotcha:** `mcp__unrealclaude__unreal_blueprint_query` `search_nodes` does **NOT** see transition rule sub-graphs. A clean search returning 0 hits does **not** mean the rule is correctly wired. **ASK the user to open the rule and confirm**, or have them paste a screenshot.

**Fix:** RMB in the rule graph → "Add Anim Node Reference" → set Tag to the target node's title → wire `Value` output into the helper's `BlendStackNode` (or equivalent) input.

**Applies to:** `IsAnimationAlmostComplete`, `SetBlendStackAnimFromChooser`, `ConvertToBlendStackNode`, `GetMotionMatchingSearchResult`, any custom helper in `UAZ_AnimInstance` that takes `FAnimNodeReference`, every BlendStack/MotionMatching library call.

**Full context:** `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\feedback_animgraph_node_reference_wiring.md`

---

## 3. OffsetRootBone misbehaving (drift, locked-on-idle, accumulating-on-walk)

**Symptom shapes:**
- Mesh drifts away from capsule over time and never returns.
- Mesh is stuck locked to an old position when idle (won't recover).
- Mesh accumulates an ever-growing offset while moving.

**Root cause — `EOffsetRootBoneMode` enum values are `Accumulate=0, Interpolate=1, …, Release=5`** (not 0/1/2 as historical comments may suggest). If `GetOffsetRootBoneTranslation()` / `GetOffsetRootBoneRotation()` returns the wrong int value, the wrong mode is applied.

**Check:** open `C:\UnrealEngine\Games\AZ\Source\AZ\Private\Animation\AZ_AnimInstance.cpp`, find the OffsetRootBone mode getters, verify the returned ints match the engine enum:
- `EOffsetRootBoneMode::Accumulate` = `0`
- `EOffsetRootBoneMode::Interpolate` = `1`
- `EOffsetRootBoneMode::Release` = `5`
- (Other values exist: `BlendOut`, `LockOffsetAndConsumeAnimation` — verify against `C:\UnrealEngine\Engine\Plugins\Animation\AnimationWarping\Source\Runtime\Public\BoneControllers\BoneControllerTypes.h` `EOffsetRootBoneMode` before assuming).

**Fix (per `project_idle_tip_implementation.md` baseline):**
- Translation: Moving → `1` (Interpolate), Idle/InAir → `5` (Release).
- Rotation: → `0` (Accumulate, GASP default).

**Full context:** `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\project_idle_tip_implementation.md` (§ "OffsetRootBone enum mapping").

---

## 4. TIP triggers wrong / never triggers / oscillates

**Symptom shapes:**
- Slow camera drag → no TIP. Fast flick → TIP fires (mouse-speed-coupled — wrong).
- TIP triggers in straight-line walking when it shouldn't.
- Camera and capsule oscillate around the target during a turn.
- After a turn, a small camera nudge re-triggers another full turn (no hysteresis).

**Root cause — speed-independent commit pattern violated.**

GASP rule (idle, Aim or equivalent):
```
CurrentDelta = |AimingRotation.Yaw - ActorYaw|

if CurrentDelta >= 60°:
    write OrientationIntent = AimingRotation_AsForwardVector
    cache it
else:
    keep last cached value (do NOT touch OrientationIntent)
```

The cache itself is the hysteresis. Once committed, ride to alignment. Only the next 60°+ delta re-commits.

AZ baseline (working) at `C:\UnrealEngine\Games\AZ\Source\AZ\Public\Character\AZ_HeroPawn.h` uses **accumulated mouse yaw** (not instantaneous delta) with 60° commit threshold and `dot ≥ 0.998` release. Single source of truth: `AAZ_HeroPawn::IsIdleTurnInProgress()`. `UAZ_AnimInstance::ShouldTurnInPlace()` is a pure mirror.

**Check the 4 known pitfalls (we hit all 4):**
1. **Always-set OrientationIntent without threshold gate** → Mover continuously rotates with mouse, threshold never crossed for moderate rotations.
2. **Reading FutureFacingDelta in ShouldTurnInPlace** → chicken-egg loop (predictor needs OrientationIntent set to predict rotation, but OrientationIntent is gated on bIsTurning derived from FutureFacingDelta).
3. **Clearing the cache when delta drops** → re-triggers oscillation as capsule catches up.
4. **Comparing first trajectory sample to actor yaw** → injects mesh-component baked offset (-90° on SK_SurvivalMan) as permanent FutureFacingDelta bias.

**Fix:** mirror the GASP-correct pattern. Don't re-derive trigger from `FutureFacingDelta` — use the accumulator on HeroPawn. Initialize `PrevYaw` from first trajectory sample, not actor yaw.

**Full context:**
- `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\gasp_orientation_intent_tip.md` (per-mode rules verbatim, 4-pitfall list)
- `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\project_idle_tip_implementation.md` (AZ working baseline + tuning knobs)
- `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\gasp_sm_tip_flow.md` (Conduit→TransIdle entry, Re-Enter rule = bare `Enable_TurnInPlaceSteering < 0.1`).

---

## 5. Wrong-time stops, no stops, "MovementState flips Idle while pressing W"

**Symptom shapes:**
- Releasing W → no stop anim (or stop fires too late, character keeps sliding).
- Pressing W → MovementState briefly flips to Idle (single-frame) at terminal speed.
- Walk-stop chooser row never matches because `MovementState_LastStateTime` is missing/zero.

**Root cause — wrong `IsMoving` formula. GASP uses `Trj_FutureVelocity + Acceleration`, NOT velocity.**

```
MovementState = Moving  ⇔  Trj_FutureVelocity != 0 (tolerance 10) AND Acceleration != 0
MovementState = Idle    otherwise
```

Why: the trajectory predictor looks 1s ahead. On release at speed 195, predictor sees stop coming → `Trj_FutureVelocity = 0` → MovementState **instantly Idle while Speed2D is still 195**. SM transitions `LocomotionLoop → TransitionToIdle` while speed is still high. Chooser picks the right Walk Stops row.

The `Speed2D > 10` formula fires too late (waits for speed to drop). The `d(velocity)/dt` formula is zero at terminal speed even while holding input → spurious Idle flips.

**Check:**
1. `C:\UnrealEngine\Games\AZ\Source\AZ\Private\Animation\AZ_AnimInstance.cpp` → find the `IsMoving` body. Should use `Trj_FutureVelocity` + `Acceleration`, not `bHasVelocity` / `Speed2D`.
2. Confirm `Acceleration` is sourced from input intent (Mover's `CommonLegacyMovementSettings.Acceleration` × rotated `CachedMoveInputIntent`), NOT from `d(Velocity)/dt`.
3. Confirm trajectory generation calls `UPoseSearchTrajectoryLibrary::PoseSearchGenerateTrajectoryUsingPredictor` with the lazy-initialized `MoverTrajectoryPredictor` — not a manual linear-extrapolation loop.

**Check the 6-state-enum tracking pattern (each enum needs 5 fields):**

| Field | Purpose |
|---|---|
| `X` | Current value |
| `X_LastFrame` | Previous frame value (change detection) |
| `X_Recent` | Delayed-previous value (held for `RecentTimeLimit` ≈ 0.1-0.2s after change) |
| `X_Time` | Time accumulated in current state (resets on change) |
| `X_LastStateTime` | Duration the **previous** state lasted (captured at transition — chooser uses this) |

GASP tracks this for: `MovementMode`, `RotationMode`, `MovementState`, `Gait`, `Stance`, `MovementDirection`. Walk Stops chooser column `c1` binds to `MovementState_LastStateTime` ≥ 0.5 to filter out spurious stops from micro-tap movements.

**Fix:** add the missing `_Recent`, `_Time`, `_LastStateTime` fields for all 6 enums. Use a templated update macro.

**Full context:** `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\gasp_update_logic_flow.md` (full idle-walk-idle cycle, exact `IsMoving` body, missing-fields list, mesh-space facing offset trap).

---

## 6. Anim always plays from frame 0 / StartTime & MM SelectedTime silently ignored

**Symptom shapes:**
- The BlendStack node's input pins show correct values (Anim, StartTime) in the debugger, but the clip visibly starts at frame 0 on every push.
- MM returns a sensible `SelectedTime` that never appears on screen.
- A clip "restarts" or "holds its last frame" in a phase where nothing new should have been pushed.
- Changing the chooser/C++ StartTime to ANY value makes no visible difference.

**Root cause — a FULL `Blend Stack` node nested inside the outer Blend Stack's per-sample inner graph, where a `Blend Stack Input` node belongs.** The rendered pose comes from the inner graph: each sample spawns a fresh inner stack that plays the bound Anim from its own literal AnimationTime (typically 0). The outer node — the one your C++/chooser drives — renders to nobody; its player appears frozen at its push time.

**Check (10 seconds):** double-click the Blend Stack node in `AZ_ABP_MoverAnimInstance` → the inner sample graph must be `Blend Stack Input` → (warping chain) → output. Ctrl+F the ABP for "Blend Stack" — more than one FULL node is the bug.

**Detection recipe (definitive):** temporary `UE_LOG` in engine `FAnimNode_BlendStack::ConditionalBlendTo` (`C:\UnrealEngine\Engine\Plugins\Animation\BlendStack\Source\Runtime\Private\AnimNode_BlendStack.cpp`) printing requested asset/time, playing asset/time, exec flag, player count. Multiple interleaved streams per frame = multiple node-state instances; a stream born `playing=none players=0` at every transition edge that dies one blend-duration later = a per-sample inner-graph instance.

**Engine rules confirmed while debugging this (valid knowledge, keep):** the BlendStack consumes AnimationTime/StartTime ONLY when a push fires (asset change / `ForceBlendNextUpdate()` / mirror change / `MaxAnimationDeltaTime >= 0` drift) — a same-asset request never re-seeks; `players=N` is stack depth during cross-fades, not instance count; raw-sequence MM needs a `BranchIn` notify with a non-null Database.

**Full context:** `C:\Users\Artur\.claude\projects\C--UnrealEngine-Games-AZ\memory\feedback_blendstack_input_node_ref.md` (§ Second incident) + `project_jump_system_status.md` (§ TRUE ROOT CAUSE 2026-06-06).

---

## Where to look in the editor logs

Use `mcp__unrealclaude__unreal_get_output_log` with these filters:

| Filter | Tells you |
|---|---|
| `LogAnimation` | Anim graph evaluation errors, missing notifies |
| `LogPoseSearch` | Database eval failures, schema mismatches, search result issues |
| `LogMover` | Movement mode transitions, sim step errors, predictor calls |
| `Error` / `Warning` | Catch-all (broad — combine with category filter when possible) |

For deep node-pin inspection (verify a binding actually wires what you think it wires), use `mcp__unrealclaude__unreal_blueprint_query` `get_node_pins` — it shows the bound delegate / property reads with full type fidelity, where `get_nodes` truncates types (`TArray<FName>` → `name`).

For binding readouts to work end-to-end, the local UnrealClaude plugin patch in `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\project_local_plugin_patches.md` § 1 must be applied to the editor's plugin source.

---

## Cross-references

- **Skill `bp-to-cpp-port`** — when the bug is in code that was ported from a GASP BP, run the 7-gate checklist before declaring a fix.
- **Skill `gasp-parity-reference`** — to query the live GASP BP at `/Game/Blueprints/SandboxCharacter_Mover_ABP` for ground truth.
- **Skill `unrealclaude-mcp-tools`** — for the `get_nodes` vs `get_node_pins` decision and the transition-rule sub-graph blindness.
- **Skill `az-cpp-utility-tools`** — for the `AZ_AnimBlueprintUtils::ListTransitions` / `InspectTransitionRule` recipe (BP-callable from Python console for transition rule introspection that MCP can't do).
