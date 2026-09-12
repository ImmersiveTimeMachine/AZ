---
name: feedback_mover_mode_state_not_rollback_safe
description: "★★★ The walking mode's GenerateWalkMove runs ~60 times per FRAME (the Motion-Matching trajectory predictor steps the same mode object 1 s ahead every tick; PIE was Standalone, no network rollback). Any decision state kept on the mode object (latch, pacing time, 'which spring') is overwritten by those predictor steps -> the live tick inherits garbage. Sim decisions MUST ride the InputCmd (FAZ_MoverCustomInputs) or tunables; the mode may only read."
metadata:
  type: feedback
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
---

# Mode-object state is not safe: the trajectory predictor runs the mode ~60x per frame (2026-09-11)

**Symptom:** aim turn-in-place paced correctly on paper but the user saw the body "sway from side to side at
the end" of every turn. `[v2 Facing]` (logged inside `GenerateWalkMove`) showed, EVERY game frame, a block of
~62 hero steps at one timestamp (dt 0.0167 each) whose delta ran from the live value to ~1 s in the future.

**First (wrong) reading:** NetworkPrediction rollback replay. It was not: PIE was `PIE_Standalone`, one
client, no emulation. **Correct reading:** `UMoverTrajectoryPredictor` (Motion Matching's trajectory) steps
the SAME movement-mode object forward ~60 ticks every frame with the current InputCmd. Every mode member
written in `GenerateWalkMove` (`CachedRotationOffsetDegrees`, `FacingSmoothingTime`, the old
`bAimTurningInPlace` latch) is therefore rewritten 60 times before the next live tick.

**Rule:** the walking mode reads its tunables (UPROPERTYs), the InputCmd, and the sync state — nothing else.
Any per-tick decision (latch, hold, pacing, rate limit, "which spring") is produced in `ProduceInput` on the
game thread and shipped in `FAZ_MoverCustomInputs` (+ ShouldReconcile / Interpolate / NetSerialize), exactly
like `bGrabbed`, `RotationMode` and the aim/strafe OrientationIntent latches. Stateless per-tick derivations
from the InputCmd are fine because every predictor step gets the same cmd.

**How to spot it:** `[v2 Facing]` — many hero steps per frame sharing one timestamp with a monotonically
evolving delta = predictor steps. The FIRST line of each block is the live state.

**Also learned the same day:** a spring paced via T = angle/rate ramps up slowly at large angles (T ~1 s ->
0.3 s to reach speed); a fast spring + yaw-rate CLAMP after `Super::GenerateWalkMove` gives full speed on
tick one and a short natural tail. And an aim turn must FINISH after the aim button is released: stopping
mid-turn hands the explore idle hold a body with 170 deg/s of spring velocity -> overshoot + swing back.

Related: [[feedback_mover_visual_component_two_writers]], [[project_v2_architecture]],
[[project_rifle_aim_upper_body_lock_2026-09-10]].
