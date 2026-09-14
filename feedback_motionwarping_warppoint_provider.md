---
name: feedback_motionwarping_warppoint_provider
description: "★★ Motion Warping WarpPointAnimProvider=Bone with a MISSING bone does NOT disable the window — it silently falls back to identity, which equals provider=None (root driven onto the target). Retargeted GASP traversal montages hit this: 'attach' does not exist on metahuman_base_skel. Plus the Mover warping-context constraint and the just-pressed-jump root-motion abort."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-14T03:48:48.291Z
---

Verified from UE 5.8 engine source 2026-09-13 (`Engine/Plugins/Animation/MotionWarping/`). Every claim below has a line cite; re-verify if the plugin is updated.

## 1. A missing warp-point bone is WORSE than a broken window — it is a wrong one

`UMotionWarpingComponent` resolves the anim-space warp point at `MotionWarpingComponent.cpp:211-232`.
When `GetPoseBoneIndexForBoneName` returns `INDEX_NONE` the whole block is skipped and the function
falls through to `return FTransform::Identity;` — **no log, no warning, no disabled window**.

`RootMotionModifier.cpp:415-454` then computes
`TargetTransform = CachedOffsetFromWarpPoint * WarpPointTransformGame`, where the offset is
`RootTransform.GetRelativeTransform(WarpPointTransform)` sampled at the window's `EndTime`
(`MotionWarpingComponent.cpp:227`, `:250`). With the warp point degraded to identity the offset
becomes the root transform itself, so the result is **identical to `WarpPointAnimProvider::None`**:
the target is taken as *"where the ROOT should end at the end of the window"* (engine's own comment,
`RootMotionModifier.cpp:421`).

**Why this bites:** an authored contact anchor (hands on a ledge) becomes a root anchor. A mantle
whose window ends with the root still on the ground ~93 cm back from the lip will instead drive the
root ONTO the lip inside that window — a fast yank up the wall, not a missing warp.

**Where it bites us:** the retargeted GASP traversal montages (`/Game/AZ/Assets/GASP/AZRTG_GASP_AM_M_*_Traversal_*`)
carry `WarpPointAnimProvider=BONE, bone="attach"`. `metahuman_base_skel` has 342 bones and **no `attach`**
(receipt: `docs/design-briefs/mantle-assets-audit.json`). So those windows are live and wrong, not inert.

## 2. The fix, and how to measure it

`WarpPointAnimTransform` (provider `Static`) is **in the same coordinate space as the root motion track**
— engine comment `RootMotionModifier_SkewWarp.cpp:318`, consumed by
`UMotionWarpingUtilities::CalculateRootTransformRelativeToWarpPointAtTime` (`MotionWarpingComponent.cpp:245-251`)
which compares it directly against `ExtractRootTrackTransform`. (The editor PREVIEW additionally applies
`* FirstFrameTransform.Inverse()` at `SkewWarp.cpp:321` for drawing only — the runtime does not. Do not
calibrate off the preview gizmo.)

To recover the authored anchor, measure it on the SOURCE (mannequin) clip and bake it as `Static` on our copy:
`UMotionWarpingUtilities::ExtractBoneTransformFromAnimationAtTime(...)` is **`UFUNCTION(BlueprintCallable)`**
(`MotionWarpingComponent.h:92-93`) so Python can call it. `ExtractRootMotionFromAnimation` (`:69-70`) and
`GetMotionWarpingWindows*FromAnimation` (`:76-81`) are BlueprintCallable too; `ExtractComponentSpacePose`
and `ExtractRootTransformFromAnimation` are C++-only.

## 3. Mover: warping only rides the attribute layered-move path

`UMoverComponent` auto-wires warping — `MoverComponent.cpp:286-290` finds any `UMotionWarpingComponent`
on the actor and creates a `UMotionWarpingMoverAdapter`, binding `ProcessLocalRootMotionDelegate`
(`MotionWarpingMoverAdapter.cpp:22`). That delegate fires **only** inside
`UMoverComponent::ConvertLocalRootMotionToWorld` (`MoverComponent.cpp:2084-2086`), reached from
`RootMotionAttributeLayeredMove.cpp:146` / `AnimRootMotionLayeredMove.cpp:102`.

So warping happens only when root motion flows through one of those layered moves — which is exactly what
`UAZ_PawnMoverComponent::DriveRootMotion` queues (`FLayeredMove_RootMotionAttribute`). The context is built
at `RootMotionAttributeLayeredMove.cpp:89-108` from
`Mesh->GetAnimInstance()->GetRootMotionMontageInstance()`: the montage must be the **root-motion montage
instance on the primary visual component**, and the mesh must tick in sim time (`:81-82` warns fixed-tick
modes break it).

★ **TRAP:** at `RootMotionAttributeLayeredMove.cpp:137-141`, with **no valid warping context**, a
*just-pressed jump* or the `Mover_SkipAnimRootMotion` tag **aborts root motion entirely**. A jump-triggered
traversal action that sets jump-pressed in the same frame can therefore play its montage fully in place
with the capsule never moving. Intercept before `IAZ_JumpRequester::SetJumpPressed(true)`
(`AZ_GA_PawnJump.cpp:58`), not after.

Related: [[project_traversal_system]], [[feedback_ik_retargeter_exact_transfer]], [[project_root_motion_mode]]
