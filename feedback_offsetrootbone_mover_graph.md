---
name: feedback_offsetrootbone_mover_graph
description: "★★ OffsetRootBone does NOT belong in AZ_ABP_MoverHero_MHC as GASP configures it — the node was pasted 2026-09-07 (absent from checkpoint 229f9b9), Accumulate rotation cancels the capsule turn because the graph has no Steering, and every prior PIE-verified behaviour was validated without it. Also: verify a node exists in the COMMITTED asset before calling it load-bearing; read pin literals AFTER any compile that could strip bindings."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-07T04:39:37.391Z
---

**What happened (2026-09-07):** the user pasted GASP's OffsetRootBone node (with its five pin bindings) into
`AZ_ABP_MoverHero_MHC`. I treated it as a long-standing, load-bearing node, wrote a GASP-parity port plan, and built
"step 1" (five getters returning the recorded literals). Result: the character stopped turning. The bindings had been
stripped by the compiler while their targets didn't exist, and `git show 229f9b9:…MHC.uasset` proved the node never
existed in any committed version — it arrived that day.

**Mechanics that decide it (engine source, `AnimNode_OffsetRootBone.cpp`):**
- `Accumulate`/`Interpolate` do NOT follow the component (`ShouldCounterComponentDelta=false`); `Release`/`Lock*` do.
  `Accumulate` rotation therefore cancels capsule yaw — GASP relies on Steering to put rotation back. The MHC graph
  has no Steering / warping / AimOffset / foot IK, so the mesh simply stops turning.
- The node writes the ORIGINAL root-motion delta back to the attribute stream (pass-through, line ~520), so it does not
  double-drive a capsule fed by `FLayeredMove_RootMotionAttribute`. That worry was wrong.
- With in-place loops (ours), `Interpolate` translation builds a trailing offset every frame (bounded by
  `MaxTranslationError`); and `bClampToTranslationVelocity` limits catch-up to a ratio of the *animated* RM delta — ~0 for
  in-place loops — so GASP's `IsMoving` binding would freeze catch-up here.
- Struct defaults are `Interpolate/Interpolate/0.1/-1`; GASP's pasted literals were `Interpolate/Accumulate/0.2/30`.
- `Release` on both modes = identity steady state (no extraction, follows component, bleeds any offset) — the zero-risk
  "off" setting without deleting the node.

**Stale evidence trap:** `AZ_PawnMovementMode_Walking.h:76` ("letting OffsetRootBone-Accumulate hide the residual")
describes the v1 `UAZ_AnimInstance` ABP (which had OFR + Steering). It is not true of the Mover/MHC hero. `AZ_ABP_HeroPawn`
(v1) has resolving bindings but 9 compile errors — reference for wiring shape only.

**Why:** the Mover path keeps capsule and animation in agreement by construction (RM-driven transitions, velocity-driven
in-place loops, spring-to-`OrientationIntent` facing). OffsetRootBone exists to absorb disagreement; porting it re-creates
a problem the architecture deliberately avoided, and `Accumulate` needs a rotation source we don't have.

**How to apply:**
1. Before calling any graph node "load-bearing", `git show <checkpoint>:<asset> | grep -ac <NodeClass>` — an uncommitted
   paste is not architecture.
2. Read pin literals/bindings AFTER any compile that could have stripped them; a binding whose target doesn't exist is
   dropped on compile and the literal may be a struct default, not an authored value.
3. Never port GASP's OFR modes into a graph without Steering (or another rotation driver) and RM-authored loops. If OFR is
   wanted on the MHC hero: measure `GetOffsetRootTransform()` per SM state first, use `Release` during in-place loops,
   never bind the velocity clamp, keep drivers on the game thread ([[feedback_animbp_post_event_vs_thread_safe]]).
4. Corrected verdict + cleanup list: `docs/design-briefs/offsetrootbone-mover-port-plan.md` (§3-§5). Leftover step-1
   getters in `UAZ_MoverAnimInstance` should be removed (revert hunks, not files — those files carry other work).

Related: [[feedback_verify_never_presume]], [[feedback_stop_the_patch_loop]], [[feedback_mover_visual_component_two_writers]].
