---
name: feedback_animbp_post_event_vs_thread_safe
description: AnimGraph nodes that depend on post-tick actor state (OffsetRootBone, consumers of extracted root motion, anything that reads/resets game-thread-only state) MUST be driven from the Post-event graph (BlueprintPostEvaluateAnimation), NOT from BlueprintThreadSafeUpdateAnimation. Wrong thread → stale data → mesh visible spin / phantom offsets.
type: feedback
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# Rule: AnimBP driver placement — Post-event vs Thread Safe Update

**Rule:** When porting GASP (or any GASP-style) AnimBP, every AnimGraph node whose driver/consumer logic depends on **post-tick actor world transform** (mesh-vs-capsule deltas, consumed root motion, anything that needs the rotation Mover *just* produced) MUST be driven from the **Post-event graph** (`BlueprintPostEvaluateAnimation` / "Post Event" entry point) — not from `BlueprintThreadSafeUpdateAnimation`.

**Why:**
- `BlueprintThreadSafeUpdateAnimation` runs on a **worker thread**, **before** AnimGraph evaluation, with **stale/cached** owning-actor data (whatever was read at the start of the tick).
- `BlueprintPostEvaluateAnimation` (Post-event graph) runs on the **game thread**, **after** AnimGraph evaluation, with the **actual current** actor transform Mover/CharacterMovement just produced this frame.
- For an AnimGraph node like **OffsetRootBone** that compares mesh-root vs capsule-root each frame and interpolates the offset toward zero: if its consumer/reset call (`UOffsetRootBoneLibrary::ResetOffset`, target-rotation feed, etc.) runs on the worker thread reading pre-tick actor rotation, it sees a phantom delta that never settles → OffsetRootBone chases a moving target → **mesh visibly spins continuously while the actor is stationary or moving in a straight line**.
- Symptom we hit (2026-05-03): pressing W → actor translates correctly forward, camera (uses controller rotation) stays fixed, but the **mesh body rotates clockwise continuously**. Removing OffsetRootBone stopped the spin. Root cause was that the GASP-equivalent driver logic had been placed in `BlueprintThreadSafeUpdateAnimation` instead of the Post-event graph.

**How to apply:**
- During any GASP AnimBP port, before classifying logic as "thread-safe", check whether it touches:
  1. **Mesh-vs-actor transform deltas** (OffsetRootBone, foot IK that compares to actor root)
  2. **Consumed/extracted root motion** (anything that calls `ConsumeExtractedRootMotion` or reads `GetExtractedRootMotion`)
  3. **AnimNodeReference library calls that mutate node state per frame** (`ResetOffset`, `SetTargetRotation`, `SetBlendStackAnimFromChooser` when its target depends on current actor rot, etc.)
  4. **Anything calling `Try Get Pawn Owner` and reading its rotation/location** for use in the same frame's AnimGraph
- If yes → Post-event graph. If no (pure cached-state reads, no AnimNodeReference mutation that depends on post-tick state) → Thread Safe is fine.
- The Post-event graph is added to an AnimBP via the Anim Class Defaults → "Post Event" / via `BlueprintPostEvaluateAnimation` event in the event graph.
- Verify in GASP first: open the GASP equivalent ABP and check **which event** the same logic is placed under (Event Graph BPI_*, BlueprintPostEvaluateAnimation, BlueprintThreadSafeUpdateAnimation). Mirror its placement exactly. Don't default-classify-as-thread-safe just because the function reads no obvious actor state — the dependency may be transitive through library calls.

**Related:**
- `feedback_animgraph_node_reference_wiring.md` — the OFFSET problem here was upstream of an FAnimNodeReference: the K2Node_AnimNodeReference *was* wired (no spin would happen at all otherwise), but the call site was on the wrong thread/timing. So this rule complements that one: wiring the ref is necessary but not sufficient — the call has to happen on the right tick phase too.
- AnimBP "Use Multi Threaded Animation Update" project setting controls whether `BlueprintThreadSafeUpdateAnimation` is even legal; if disabled, all updates run on game thread (heavier perf cost). Most GASP AnimBPs have it enabled — the discipline is per-function, not per-AnimBP.
