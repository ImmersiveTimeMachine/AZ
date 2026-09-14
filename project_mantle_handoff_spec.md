---
name: project_mantle_handoff_spec
description: "★★★ THE WORK ORDER for the walk/run mantle → locomotion handoff (user-authored 2026-09-14, 13 points). Follow this next session before touching traversal. Includes the corrections to the assistant's proposed answers, the coordination checklist, the barrier-system rules, the low-obstacle audit scope, and the user-run validation list."
metadata: 
  node_type: memory
  type: project
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-14T05:04:16.296Z
---

# Walk/run mantle → locomotion handoff — USER WORK ORDER (2026-09-14)

Authored by the user in response to the assistant's 8 questions. **Follow this; do not re-litigate it.**
Current state of the system it applies to: [[project_mantle_traversal_2026-09-14]].
Goal: fix the moving-mantle exit. **Preserve the working standing mantle and existing traversal setup.**

## 1. Handoff pattern
Use the **animation-driven event/callback** pattern to transfer ownership from the ability back to
locomotion, **combined with a pose/foot-phase-matched transition** into the selected loop. These solve
different halves: the *event* controls WHEN traversal releases the body; the *matching* controls HOW
smoothly locomotion takes over.
- Do **not** blindly copy `entry=0.86` or the existing transition-end phase formula — they depend on
  specific animation content.
- Match against the **actual outgoing mantle pose**, NOT the locomotion pose hidden beneath the FullBody
  montage.
- Use a **traversal-specific handoff signal**, not the jump's landing-complete event.

## 2. Where to cut
**Author a handoff marker/time per montage, including style and foot variants.** Choose the earliest point
where ALL hold:
- the capsule has advanced far enough onto supported, walkable space;
- required warping and hand-contact phases are complete;
- the outgoing pose can transition naturally into locomotion;
- restoring normal obstacle collision will not leave the capsule penetrating the ledge.

★ **"Root reaches ledge height" alone is INSUFFICIENT** — the character may still need forward travel to
get onto the platform. Compare the **second warp-window end** and **GASP's `TraversalBlendOut` timing** as
candidate references, then **verify against the actual motion**. Events drive normal handoff; timers stay
watchdogs only.

## 3. Momentum
With movement input held, **preserve the actual horizontal Mover velocity** through the handoff and let
normal acceleration/deceleration respond to current direction and gait. Avoid: resetting velocity to zero,
playing an unnecessary new locomotion-start animation, or continuing an excessive authored tail that
carries the character too far. **Everything stays at 1× playback.**

## 4. Input released during the mantle
Complete the committed climb, then respect input **at handoff**:
- input held → appropriate moving transition/loop;
- no input → appropriate stop/settle, then idle;
- changed direction → appropriate locomotion transition once safely supported.

Do not force a loop with no input, and do not instantly erase remaining velocity.

## 5. Commitment and interruption
Once the mantle begins, **releasing Jump or movement input does not cancel it.** Death, grabs, destroyed
targets and genuine path obstruction still require controlled interruption + cleanup — **treated separately
from successful completion.**

## 6. Camera
**Retest after** the character handoff is corrected. "It merely follows the overshoot" is still only a
hypothesis. Compare **capsule, mesh and camera** motion. If it still misbehaves once character motion is
right, investigate separately. **Preserve existing camera smoothing** unless evidence points there.

## 7. Standing mantle
**Unchanged.** This change targets moving mantle exits only.

## 8. Run versus walk
Evaluate **both** against the same safe-handoff criteria, each with its **own measured timings**. Run's
shorter duration does NOT establish that its exit is already correct. Preserve useful forward motion and
recovery; shorten only the portion causing unwanted overshoot or delaying locomotion.
Cut mechanism (section arrangement / explicit montage blend-out / equivalent) and initial blend duration
are the implementer's choice, seeded from existing transition settings — but **verify the incoming pose and
remaining outgoing motion actually support that blend.**

## 9. Coordinate the actual movement handoff
The handoff must coordinate, together:
- incoming locomotion pose/phase;
- restoring the target component's collision exemption;
- releasing this action's root-motion drive;
- changing movement mode and preserving appropriate velocity;
- clearing traversal state and stale reaction/transition work.

★ Account for **animation/Mover update ordering** so a queued mantle delta cannot move the capsule after
handoff. Prevent locomotion callbacks from cancelling a **newer** action's root-motion drive.
★ The ability today routes completion, blend-out, interruption and cancellation through **one** callback —
**separate successful handoff from cancellation**, with idempotent, owner-aware cleanup. Ending the ability
must not accidentally replace the intended blend with another abrupt montage stop.

## 10. Preserve the agreed gameplay behavior
- Jump attempts traversal at a valid ledge; otherwise existing jump rules apply.
- Placed `LevelBlock_Traversable` actors first; ordinary geometry later.
- **Armed traversal: holster → mantle → draw the same weapon in relaxed posture.** Preserve inventory
  selection and magazine identity; do **not** auto-resume aiming/firing.
- Keep an **explicit Relaxed/Neutral comparison setting** until a gameplay stance mapping is chosen.
- An already-playing barrier/head-hit reaction **finishes first**; require a fresh press afterward.
- A timely accepted mantle **prevents a new cosmetic reaction to its target ledge**.
- Genuine overhead/path obstruction still blocks or aborts traversal.

★ **Report any unfinished parts separately rather than assuming they are already complete.**

## 11. Reuse the existing barrier systems
AZ already has low/chest/head obstacle probes, walking-intent clamping, and animation reaction holds.
- Being **blocked from walking** into a ledge must **not** itself reject a valid mantle.
- Conversely, **disabling the obstacle sensor alone is insufficient** — the AnimInstance reaction latch and
  the state-machine reaction timer survive independently.
- Keep normal stairs, curbs, wall sliding and unrelated collision reactions working.
- **Never disable capsule collision globally.**

## 12. Low obstacles / step-over — AUDIT ONLY, execution is a follow-up
Include an asset audit for low-obstacle coverage. Retargeted GASP `Hurdle_1_0` walk/run and V2 montages
exist; a dedicated **StepOver montage is NOT confirmed**. Classification to use:

| Case | Route |
|---|---|
| Ordinary curb/stair within step-up capability | existing Mover step-up + procedural feet |
| Low barrier beyond automatic step-up | suitable inspected step-over / low-hurdle animation |
| Narrow obstacle with a safe destination beyond | vault / hurdle |
| Raised platform with sufficient standing support | mantle |

Height, depth, support and clearance **all** matter. **Do not scale a one-metre mantle down and assume it
becomes a natural step-over.** Audit and identify clips now; keep step-over/vault/hurdle **execution** as a
clearly identified follow-up to the handoff fix.

## 13. Level and validation
Level has 13 traversable blocks. Broad one-metre candidates:
- `LevelBlock_Traversable4` — 200×400×100 cm;
- `LevelBlock_Traversable12` — 200×700×100 cm (**check the higher geometry on its far portion**).

Thin examples are 20 cm thick × 100 cm high and **must not pass as a supported standing mantle
destination**. The authored blocks are **Movable / WorldDynamic without physics simulation — do not reject
them on mobility or object type.**

User-run validation pass:
- walk/run handoff with input **held** and **released**;
- both styles and enabled foot variants;
- **standing mantle regression**;
- momentum, foot continuity, overshoot, camera;
- thin-wall rejection and blocked headroom;
- barrier-reaction timing and cancellation recovery;
- armed holster/restoration where implemented.

★ **No automated tests. Do not start PIE or editor tests without asking.** Complete the appropriate build
and asset readback checks, then provide concise manual steps and inspect the user's logs afterward.
