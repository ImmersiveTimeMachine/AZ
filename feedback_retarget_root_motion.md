---
name: feedback_retarget_root_motion
description: Fix for root motion lost during IK Retargeting — duplicate the retargeted asset to preserve root bone motion
type: feedback
originSessionId: f6181671-d4a5-4b82-954f-4f2f5396f92f
---
When retargeting animations via UE5's IK Retargeter, root motion (translation on the `root` bone) is NOT copied to the target anim automatically, even with correct Root chain settings (One-to-One rotation, Absolute translation, Alpha=1.0).

**Fix:** After retargeting, **duplicate the retargeted asset** in the Content Browser. The duplicated asset will have correct root motion preserved.

**Why:** The retargeter's live preview/output asset has some internal reference that prevents root motion from being baked into the root bone. Duplicating forces UE to fully serialize the asset, which bakes the root motion data properly into the root bone.

**How to apply:**
- After running "Duplicate and Retarget Animations" from the IK Retargeter
- Right-click the output asset → Duplicate
- Use the DUPLICATE, not the original retargeted output
- Verify: open the duplicate in Persona → enable Process Root Motion → character should visibly translate/rotate
- This applies to ALL retargeted anims that need root motion (turn-in-place, locomotion starts/stops, etc.)
