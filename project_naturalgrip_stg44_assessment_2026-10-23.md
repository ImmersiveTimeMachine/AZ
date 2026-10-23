---
name: project-naturalgrip-stg44-assessment-2026-10-23
description: "Independent review of Codex STG44 MetaHuman grip (NaturalGrip) — root causes, open questions, proposed stage R-1"
metadata:
  node_type: memory
  type: project
  originSessionId: 9f1db4ba-c6e2-4a2d-b7dd-43a2b7b73076
  modified: 2026-10-23T13:38:06.920Z
---

Review written 2026-10-23 at `Art/MetaHumanPlayer/v20261020_01/claude_naturalgrip_assessment_v01/CLAUDE_ASSESSMENT.md` (channel `dialogue.md` there; Codex = `codex-mhc-rifle-grip`, me = `claude-naturalgrip-review`; read-only role, writes only those two files).

Findings that are easy to lose:
- Right hand root cause = PLACEMENT, not finger angles: knuckle line ~0-2° (inherited from the GameSDK clip hold) vs STG44 grip rake 33-36°; ring/pinky MCPs 1-2 cm ahead of the grip front face. Finger/thumb tweaks at that placement give open fingers, tips inside the grip, or the thumb on the palm side.
- `GripContact` (NGContact.h) checks only prohibitions + index pad; no positive "hold" condition, so an open hand gets `passed`. v02 support "palm" patch was the carpal heel (hand_l verts x −1.5..2.9 cm).
- MetaHuman L/R hand frames are exact negatives: same local quat = mirrored motion, but local vectors flip (palmar +Y right / −Y left). `NGHand::PadPoint/PadNormal` hardcode +Y (latent bug for left trigger roles).
- MetaHuman corrective joints (_half/_mcp/_side/...) are not driven in the Blender grip scenes.
- CE: installed FP/world CDFs are headless; all MHC attachments HideInShadow(true); shadow comes from the GameSDK 3P body; a head exists in `mhc_hero_outfit_posture_v03.cdf`.

**Why:** Artur stopped Codex after repeated visual rejections; the next step depends on his answers.
**How to apply:** Artur's answers (2026-10-23): thumb wraps the LEFT side pointing forward; grip wrap + right-finger placement outrank exact trigger point (index in guard, ~1 cm); elbow decision delegated to me — right pole ≤25°, minimal (elbow off-screen in FP). R-1 (right-hand re-placement by the grip with positive hold criteria) APPROVED 09:37 for Codex: core build, solver, MCP apply with before_r1.blend, actual-skin check; not Unreal/CE install or left hand. Check `dialogue.md` for Codex's R-1 result before further grip work. Related: [[project_natural_grip_plugin_2026-10-02]], [[feedback_stop_the_patch_loop]].
