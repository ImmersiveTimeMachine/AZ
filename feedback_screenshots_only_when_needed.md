---
name: feedback-screenshots-only-when-needed
description: "Artur's rule: no screenshots as proof or for the report; ask him to check. Take one only when the agent itself needs it for a fix or a test."
metadata:
  node_type: memory
  type: feedback
  originSessionId: 41cf67a5-49dc-4758-8dbb-dd05c0674cb0
  modified: 2026-10-17T18:01:35.818Z
---

**Rule (Artur, 2026-10-17, after the JAW crosshair task):** do not take screenshots unless he asks. Do not spend time on
them. Ask him to check instead: he looks himself. Take a screenshot only when the agent personally needs it to make a fix
or to test something.

**Why:** screenshot runs cost time (launches, waiting, reading the images), and Artur checks the picture himself
anyway. In the JAW task the scripted screenshot runs also overlapped with his own play in the launcher window.

**How to apply:**
- Default check = log lines and HUD state (`az_hud_set`, `[AZNoesis]` lines). Then write "проверь, пожалуйста: ..."
  with the exact steps for him.
- No `r_GetScreenShot` in scenarios unless the fix or the test depends on the image (layout, a render bug, a value only
  visible on screen). Do not send screenshots to him as proof.
- Applies to CryEngine and Unreal work alike.

Related: [[feedback_cryengine_rider_debug_runs]], [[feedback_deliver_result_not_analysis]].
