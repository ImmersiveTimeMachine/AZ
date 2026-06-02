---
name: feedback_ik_setup
description: Two Bone IK must use Bone Space relative to hand_r, Layered Blend per Bone uses spine_02 not hand_l
type: feedback
---

Two Bone IK for left hand weapon grip must use **Bone Space** relative to `hand_r`, not world/component space. This eliminates 1-frame lag because the IK target is computed from the current frame's pose data, not the external weapon component transform.

Layered Blend per Bone (fire montage) branch filter must be `spine_02`, not `hand_l`. Using `hand_l` causes wobble because the montage and IK compete to control the same bone.

**Why:** World/component space IK reads weapon socket from previous frame's pose (NativeUpdateAnimation runs before AnimGraph). Bone-relative eliminates this dependency. `hand_l` branch filter on Layered Blend fights with Two Bone IK.

**How to apply:** When setting up weapon hand IK, always use Bone Space relative to the attachment bone (hand_r). For upper body montage blending, use spine_02 as the branch filter.
