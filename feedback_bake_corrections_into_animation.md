---
name: feedback-bake-corrections-into-animation
description: "User rule - pose/trajectory corrections are solved offline and baked into a corrected clip per situation, not solved per frame at runtime"
metadata:
  node_type: memory
  type: feedback
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-10-04T04:22:56.690Z
---

When an animation needs correcting (arm through a leg, hand off a grip, etc.), the user wants the correction **solved
offline and baked into the animation**: one corrected clip per concrete situation (stand/crouch x action), so at runtime
it simply plays. Not a per-frame runtime solver/IK push.

**Why:** 2026-10-03 the runtime approach (predictive reach path + per-frame left-arm leg push) made the hand jerk and jump
in PIE - the push alternated between hand and elbow solutions every frame and never converged. User: "if the correction
is baked into the animation, there is one animation that works correctly in this situation - then it always works."
It mirrors the Natural Grip workflow that succeeded for the palm (simulate -> solve offline -> review -> apply).

**How to apply:**
- Runtime keeps only cheap, deterministic playback plus a no-op safety IK (e.g. the clip's own grip curve).
- Simulate against the REAL live pose (record from PIE - my hand-composed offline pose missed foot placement/layering
  and under-estimated penetration 1.4 vs 9.6 cm).
- Solve the whole trajectory at once (smoothness + minimal change), show before/after for approval, write NEW clip
  copies (protected originals untouched), repoint the profile with the user's go.
- Related: [[project-natural-grip-plugin-2026-10-02]], [[feedback-stop-the-patch-loop]], [[project-protected-weapon-sets]].
