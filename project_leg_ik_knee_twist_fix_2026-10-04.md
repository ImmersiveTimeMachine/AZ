---
name: project_leg_ik_knee_twist_fix_2026-10-04
description: "★★★ Leg crossing / idle knee twist: the cause is the foot rig's leg solve, not pinning. Twist forced by SecondaryAxisWeight=1, and the inherited fixed-hinge pole moves the knee up to 9 cm at ZERO correction. Fix applied 2026-10-04: weight 0 + blended knee-plane pole. User PIE 2026-10-04: 'much better'; handoff docs/design-briefs/leg-ik-knee-twist-fix-handoff-2026-10-04.md."
metadata:
  node_type: memory
  type: project
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-10-05T02:24:16.266Z
---

**Cause (offline identity test, 2026-10-04):** the engine solver re-implemented on hero poses, with effector = animated foot (zero correction).

1. **Twist.** `TwoBoneIKSimplePerItem.SecondaryAxisWeight=1` re-rolls the thigh and calf toward the knee-bend direction every frame (`ControlRigMathLibrary.cpp:42`).
   - Roll even at zero correction: crouch up to 25 deg, walk 5 deg, idle break 3 deg.
   - The bend vector of a straight leg is millimetres, so the roll is noise.
   - Positions do not change, which is why all the position metrics looked fine. MetaHuman twist bones turn it into twisted knees.
2. **Knee position.** The inherited GASP pole uses calfRot * cross(Secondary, Primary), which assumes a fixed hinge. The MH knee is not one: hinge spread up to 18 deg, and no fitted axis fits all clips.
   - At zero correction it moved the knee 9 cm on Winchester 45L, 6.4 cm on pistol walk, 5 cm on crouch rifle walk.
   - This is plausibly the rifle crossing that remained with pins off.
3. **Withdrawn animated-knee pole.** It used the raw knee as the pole. In `AnimPro_Idle6` the knee is 2 mm hyperextended, so the pole flipped and gave the idle twist.

**Fix applied (rig in memory; the user compiles + saves):**
- Rollback of the animated-knee pole (exact pre-snapshot match).
- `Tools/diagnostics/apply_knee_plane_pole.py`:
  - SecondaryAxisWeight 0.
  - Pole = the animated knee bend direction when the forward bend is >= 3 cm, the inherited pole when <= 1 cm, linear between.
  - Has `rollback(receipt)`.
- Receipts are in `Saved/Diagnostics/FootCircle/`.
- Doc: `docs/design-briefs/leg-crossing-and-idle-knee-twist-review-handoff.md` s.14.

**Offline prediction:**
- Zero-correction knee move is 0 on bent-leg clips and under 1 cm on straight-leg clips.
- Twist change is 0.
- No jerk increase under terrain or pin perturbation.

**How to apply:**
- Acceptance = user PIE: idle, turns, circles, all weapons, crouch, terrain.
- If a regression appears, roll back with the receipt. Do not stack another patch (see [[feedback_stop_the_patch_loop]]).
- Next suspects, if crossing remains while moving: pinning with in-place loops (40 cm anchor corrections, no Steering / OffsetRoot in AZ), and the RifleMega 45L authored cross-step.
- Rule learned: any IK must pass the identity test (zero correction = no change in positions AND twist) on idle, idle breaks, walk, crouch and every weapon set, before PIE.

Offline identity test: `Tools/diagnostics/leg_ik_identity_test.py` (exec in the editor; run(path), fit_hinge(), test_pole(path, perturb)). It re-implements the solver on AnimPose (hero mesh, WORLD space).

**Confirmed 2026-10-04 22:29 local:** user PIE 'вроде намного лучше'. Clean log, no RigVM zero-vector warnings, Idle6 played twice. Rig saved (SHA 8bd89514...). Rig Auto Compile is still OFF (user to re-enable). Not yet checked: crouch, circles, terrain, aim TIP. Handoff for other agents: `docs/design-briefs/leg-ik-knee-twist-fix-handoff-2026-10-04.md`.
