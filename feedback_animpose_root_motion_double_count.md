---
name: feedback_animpose_root_motion_double_count
description: "★★★ MEASUREMENT TRAP: AnimPoseExtensions WORLD-space bone transforms ALREADY include root motion unless extract_root_motion=True. Adding the root track on top double-counts and silently doubles every height, which is how a 'clears by 56cm' conclusion was reported to the user when the clip actually grazes."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-15T01:18:35.053Z
---

# AnimPose WORLD space already contains the root motion

`unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, t, opts)` +
`get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)` returns bone transforms that **already include the
root bone's animated transform** when `opts.extract_root_motion` is left at its default `False`.

Set `extract_root_motion = True` and the root is pinned at the origin instead, so WORLD becomes
root-relative. The flag means "pull the root motion OUT of the pose", not "give me the root motion".

**Verify it in one call before trusting any sweep** — the root bone itself is the probe:

```python
rt  = AnimationLibrary.get_bone_pose_for_time(seq, "root", t, False).translation   # the root track
rA  = E.get_bone_pose(E.get_anim_pose_at_time(seq,t,optFalse), "root", WORLD).translation
rB  = E.get_bone_pose(E.get_anim_pose_at_time(seq,t,optTrue ), "root", WORLD).translation
# rA == rt  (root motion included)      rB == (0,0,0)  (root motion extracted)
```

## Why it matters

Adding `root.z` to a WORLD-space bone Z **doubles the height**, and the error is invisible: every number
stays plausible, ordering is preserved, and relative comparisons still look sane. At t=0 the root is at the
origin, so a spot-check at the start of the clip agrees with both readings.

Concretely (hurdle clearance work, 2026-09-14): the double-counted sweep said the lowest body bone crossing
the barrier sat at **z≈156** against a 100 cm wall — "clears by half a metre". Measured correctly it is
**z≈96–111 on the clearing clips**, i.e. 0–11 cm of margin, which is exactly the graze the user was
reporting on screen. The wrong number was used to argue a lift was unnecessary and was reported to the user
as fact before the error surfaced. See [[project_traversal_2026-09-15]].

## Rules

- Decide which space you want, then **prove it with the root bone**, not from the flag's name.
- Never add a root-track value to a WORLD-space bone value without that proof.
- A measurement that contradicts what the user is seeing on screen is the measurement to re-check FIRST.
  "The clip clears by 56 cm" versus "it grazes the top" was a 1:1 contradiction and should have stopped the
  conclusion rather than overriding the report.
- Related: [[feedback_measure_the_clip_first]], [[feedback_verify_never_presume]].
