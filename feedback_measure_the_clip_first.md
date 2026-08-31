---
name: feedback_measure_the_clip_first
description: "★ CORRECTED same night: the per-bone reversal metric flags LIVELY idles, not just tremor — CrouchLoop_new was NOT the cause (user placed it standalone: clean). Real cause in feedback_mover_visual_component_two_writers. Keep: the metric, the write/de-noise recipe, and the rule that a standalone-actor-in-PIE test discriminates content from pipeline."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-31T03:00:16.326Z
---

# "The whole body shakes" — measure the clip before the pipeline (2026-08-31)

**★ CORRECTION (same night):** the clip was NOT the cause. The user placed `AnimPro_CrouchLoop_new` in
the level and it played clean; the reversal metric below flags a *lively* idle (hands/head moving at
28°/s) as well as tremor. The actual writer was game code fighting Mover for the mesh transform —
see [[feedback_mover_visual_component_two_writers]]. What stays valid here: the metric as a content
screen, the write/de-noise recipe, and the discriminator "same clip on a standalone actor in PIE".

**What happened:** user saw a small whole-body shake in crouch IDLE. I instrumented, in order: seam
re-pushes, LOD/URO, Face copy-pose lag, capsule/mesh Z, camera position/rotation, GAS tag vs Mover
crouch (user's own hunch), anti-aliasing OFF, shadows/AO OFF, SurvivalMan vs MetaHuman, camera
FOV/boom/socket. Every one came back static/clean (`[v2 CrouchTrace]`: 0.00 transform deltas over
1,237 frames, bones ≤0.24 cm/frame). The cause was found in five minutes once I sampled the CLIP:
`AnimPro_CrouchLoop_new` carries ~15–20 Hz mocap tremor (hand rotation direction reversals on
30–34 % of 60 fps frames, max 3°/frame; standing `AnimPro_Idle` 2–5 %, 0.3°). Both heroes play the
same clip — which is why nothing mesh/render/input-side could ever have fixed it.

**Rule:** when a visual artifact survives two independent pipeline tests and the game-thread
transforms/bone POSITIONS are static, the next test is the animation asset itself — and measure
ROTATIONS (positions of pelvis/head stay still while a jittery arm/neck rotates).

**The metric (Python, editor, read-only):** sample `AnimationLibrary.get_bone_pose_for_frame`
(native frames, LOCAL space) per bone; per frame `angular_distance` in degrees; count sign flips
of the per-axis euler delta where both consecutive deltas exceed 0.02° → reversal %. Clean idle
≈ 0–5 %, max <0.5°/frame; tremor = 15–35 %, max 1–3°.

**Fix path (write API, 5.8):** `seq.controller.open_bracket/…/set_bone_track_keys(bone, pos, rot,
scale)/close_bracket`; keys = frames+1 (key N == key 0 on loops, keep it); circular Gaussian on
rotations (weighted quaternion average with hemisphere alignment); endpoint-clamped on one-shots.
Dry-run numbers for CrouchLoop_new (60 fps): radius 9 / σ 4 → hands 30 % → 2 %, max 3.0° → 1.6°,
mean motion −35 % (breathing kept). `Content/Assets/RTG_AZ/**` is gitignored — back up the .uasset.

**Content map found on the way:** two crouch families, each seamless internally and 100.9 apart:
OLD (`Crouch_Idle` clean, `Crouch_WalkFwdStart`, 90/180 pivots, `Crouch_WalkFwdLoop`, `Idle2Crouch`,
`Crouch2Idle`) vs NEW `*_new` (idle+start+8-way loops+stops+turns, all noisy). No stops / 8-way / turns
exist in the OLD family, so the NEW family stays and gets de-noised.

Related: [[feedback_verify_never_presume]], [[feedback_stop_the_patch_loop]],
[[project_mover_metahuman_2026-08-31]], [[feedback_posesearch_mm_mechanism_rules]].
