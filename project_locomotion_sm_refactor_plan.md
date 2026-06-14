---
name: project_locomotion_sm_refactor_plan
description: "Plan to extract DeriveSMState into a dedicated UAZ_LocomotionStateMachine class (before traversal states), plus the ChooserContext bool-vs-tag audit result (split is clean)."
metadata: 
  node_type: memory
  type: project
  originSessionId: 787f844b-69e1-48c0-8b39-9a9264829d57
---

# Locomotion SM refactor — extract `UAZ_LocomotionStateMachine`

**Why:** `UAZ_MoverAnimInstance::DeriveSMState` is a single growing if-else chain (idle-break scheduling + transition timing + reversal/turn detection + RM jump + proxy mirror). At its complexity ceiling; extract into a dedicated class BEFORE adding traversal states (vault/mantle/slide/cover). Decided 2026-06-01. Baseline restore point: commit `bc20e53` (working SP+MP locomotion+jump).

## Hard constraints (must survive the refactor)
- **State stays deterministic C++**, NOT an AnimGraph SM node. (An AnimGraph SM runs per-machine on the anim worker thread → the exact network non-determinism that caused the proxy-jump bug. Keep it in code.)
- Preserve the **sim-proxy air mirror**: `GetLocalRole()==ROLE_SimulatedProxy` derives the air phase from replicated MovementMode `"RMAction"`, NOT the one-shot jump edge.
- Emit the SAME outputs the chooser uses: `EAZ_StateMachineState` + latches (StartDirection bucket, bMovingTransition, bJustLanded) + timing (TransitionEndTime, NextIdleBreakTime/IdleBreakEndTime).
- AI-reusable; incremental (enum output + chooser unchanged).

## Design (chosen)
- **Plain UObject**, owned by the AnimInstance. NOT a component, NOT a struct, NOT an AnimGraph SM.
- **Pure decision fn:** `Tick(FAZ_LocoSMInputs) -> FAZ_LocoSMOutputs`. Zero engine API inside — role/mode/jump-edge passed IN (so no GetLocalRole in the SM body; testable + AI-reusable). **Mover side-effects (QueueNextMode, RM-move queue/teardown) STAY on the AnimInstance** (keeps "proxies never touch their Mover" in one place).
- Pattern: one `Tick()` dispatcher + per-state handler methods (literal lift of today's branches). NOT a declarative table (transitions are guarded by chooser-set timers + runtime turn-angle — a table would just hide them in lambdas).
- Hierarchy: lazy — a `Ground/Air/Traversal` group enum OVER the flat `EAZ_StateMachineState` (which stays the chooser/wire contract). Nested state-objects deferred until traversal needs shared Enter/Exit.
- **Simplification vs the agent plan:** keep the tunables (IdleBreakMin/MaxTime, the AlmostComplete thresholds) ON THE ANIMINSTANCE (already CDO-editable) and PASS them into Tick / the Notify* calls — avoids CreateDefaultSubobject/CDO-serialization risk. SM created via `NewObject` in `NativeInitializeAnimation`, holds RUNTIME state only (PreviousState, the 3 timers, the 3 latches).
- Chooser→timer handoff: `SetBlendStackAnimFromChooser` stops writing `TransitionEndTime`/`IdleBreakEndTime` directly; calls `StateMachine->NotifyTransitionClipPushed(...)` / `NotifyIdleBreakClipPushed(...)` instead.

## Migration steps (each compiles, behavior-identical)
0. Baseline-capture PIE oracles (idle→break timing, idle→run 180 start, run→stop foot, moving reversal, autonomous jump, **sim-proxy jump**). `bc20e53` is the restore point.
1. **Create the class dormant** (faithful port, NOT wired). New UCLASS ⇒ **full editor-closed build**, not Live Coding. ← files created `AZ_LocomotionStateMachine.h/.cpp` this session.
2. Wire the AnimInstance to call `Tick`; repoint the two timer writes to `Notify*`; move latch lifetime-gating into the SM output. **CHECK FIRST: are `TransitionEndTime`/`NextIdleBreakTime`/`IdleBreakEndTime` (BlueprintReadOnly) referenced in the ABP EventGraph?** Removing them blind can break the ABP compile — verify via MCP/editor before deleting.
3. Delete old `DeriveSMState` + the file-static `BucketStartDirection` + the dead `IdleSpeedThreshold` field. PIE-validate all oracles, esp. the sim-proxy jump + the reversal chain.
4. Add the `Ground/Air/Traversal` group seam (structure only). Natural stop before traversal.
5. (optional, later) promote handlers to Enter/Tick/Exit state objects if traversal needs shared hooks.

## Known gaps / TODO surfaced by the plan
- **Dead field:** `IdleSpeedThreshold` declared, never read → delete.
- **Latent AI-jump gap:** air trigger today = "edge if not sim-proxy, mirror if sim-proxy." AI is NEITHER (server-authority, no input edge) → an AI pawn entering RMAction gets the capsule arc but **no jump anim**. Fix when AI gets jumps: mirror the mode whenever "not the edge-presser" (careful re: authority re-entry during the exit window). NOT urgent — AI doesn't jump yet.
- Thread-safety: `SetBlendStackAnimFromChooser` is `BlueprintThreadSafe` (may run on anim worker) and writes the timers DeriveSMState reads on the game thread. Today benign (plain float store). Keep `Notify*` as plain single-word float writes; don't read-modify-write across the boundary.

## ChooserContext bool-vs-tag audit (2026-06-01) — split is CLEAN, no promotions
Audited all 14 fields of `FAZ_v2_ChooserContext`. **No gameplay-domain state masquerading as a bare bool.** Every bare bool/enum is per-frame ANIM-derived (anim curve `contact_l`, trajectory, Mover sync state, or DeriveSMState latches); every cross-system gameplay state (aim/sprint/reload/weapon/cooldown/stun) is already a GAS tag in `OwnedTags`. `Gait`/`Stance` are correctly the intended "GAS tag → resolve in ProduceInput → ship in replicated InputCmd → read back" derived-field bridge, handled consistently. Conclusion: keep the hybrid; the scalability win is keeping the two domains clean + hierarchical tags on the gameplay side, NOT collapsing to tags-only.
- Optional housekeeping (non-blocking): remove vestigial `FAZ_MoverCustomInputs::bWantsToCrouch` (v2 ProduceInput never writes it; v1 leftover); prune unused `Animation.State.*` / `Ability.Animation.*` per-pose tag blocks in `AZ_GameplayTags.h` (dead GASP-parity remnant, not read by v2 chooser).

See [[project_v2_locomotion_progress]], [[project_v2_architecture]].
