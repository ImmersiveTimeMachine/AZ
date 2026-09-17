# Claude — approved Throwable layer placement

Use option1's **intended priority**, with the corrected splice point below: **after the final ordinary weapon aim blend and before FullBody**. This is the implementation decision; do not ask Artur again to choose between the two earlier locations.

Read-only live review on September17 confirmed the two new nodes exist and are disconnected. No graph, skeleton or montage changes were made by this review. Source evidence: `C:/UnrealEngine/Games/AZ/Saved/ThrowSlotReview/`.

## Actual live graph

`RifleFireBase` is a cache **before** the RifleFire slot. Its two consumers are the RifleFire mask's BasePose and the RifleFire Slot.Source. They do not directly feed an aim TwoWayBlend and Control Rig as described in the question.

Current active path:

```text
Locomotion + standing/crouch weapon pose
  → AdditiveLeans
  → RifleFireBase cache
  → masked RifleFire
  → AdiativePoses cache
  → final masked AO_Rifle_Aim result [A82E98F4]
  → FullBody [5BF55C28]
  → DeadBlending → OffsetRootBone → procedural feet → paired hands
  → PoseHistory → Output
```

Putting Throwable before RifleFireBase or between that cache and its consumers leaves weapon-fire and/or aim layers downstream that can overwrite the throw. Insert at the final normal-pose boundary instead.

## Exact splice

Re-resolve complete GUIDs from the live graph; eight-character prefixes here identify the inspected nodes, not permission to guess a different node.

1. Feed final aim blend **A82E98F4.Pose** into one new SaveCachedPose, e.g. `PreThrowable`.
2. Use that cache as the existing new Throwable Slot **A30C1E6B.Source**.
3. Use the **same cache** as existing new LayeredBoneBlend **62E42C8E.BasePose**.
4. Feed **A30C1E6B.Pose → 62E42C8E.BlendPoses_0**.
5. Feed **62E42C8E.Pose → FullBody 5BF55C28.Source**.

Only the existing **A82E98F4→5BF55C28** edge is replaced. Preserve every other pre-existing edge, node, binding, state-machine path and rig parameter. Reuse the two already-created nodes; do not duplicate the Throwable slot for separate downstream consumers. One SaveCachedPose plus two UseCachedPose nodes supplies the shared pre-throw source without recursion or duplicate slot evaluation.

This puts Throwable above ordinary weapon aim/fire poses, while higher-priority FullBody actions remain outside it. Preserve the FullBody→DeadBlending→OffsetRootBone→Feet→PairedHands→PoseHistory tail. Do not move Throwable after those nodes merely to force visibility.

## Configuration corrections

- Keep `bAlwaysUpdateSourcePose=True`, `CurveBlendOption=UseBasePose`, mesh-space rotation blend and `bBlendRootMotionBasedOnRootBone=True`. Both BasePose and Slot.Source need the same valid pre-throw cache. Blend weight1 is appropriate: the slot already blends to/from its source when its montage weight changes and passes source through when inactive.
- The new mask is **spine_01/depth4**, but the live RifleFire mask is **spine_01/depth1**. They are not identical. Depth4 is a gradual .25/.5/.75/1 ramp down that bone hierarchy, not a four-bone cutoff. Start with the existing proven **depth1** if the goal is exact RifleFire-lane parity; treat depth4 as an explicit later seam adjustment after looking at these clips. Root/pelvis/legs must remain from locomotion in either case.
- A mask is not a root-motion safety switch. Keep carry/Start/aim Loop in-place with root motion disabled and playback1×. Preserve locomotion contact curves and the existing grounded feet gate.
- MetaHuman skeleton lookup confirms `Throwable→Throwable`, `RifleFire→WeaponFire`, `FullBody→DefaultGroup`. The ABP itself still targets SKEL_SurvivalMan, whose Throwable lookup is DefaultGroup, explaining the current node caption. Verify effective montage/skeleton group resolution and synchronize only necessary slot metadata; **do not retarget/reparent the working AnimBP or change its target skeleton as part of this splice**. Editor caption alone is not proof of runtime group ownership.

## Route the actual montages and preserve action ownership

Current loaded montage tracks are Carry/Start/Loop=`RifleFire`; Close/Far/Cancel=`FullBody`. Connecting an empty Throwable lane changes none of those routes. `Profile.MontageSlot` is record-only, not a runtime router.

Move the verified in-place **Carry, Start and held Loop** montage tracks onto `Throwable`, so equipped idle and movable preparation use the new mask. Treat Cancel according to its verified upper-body pose compatibility. Keep the short pelvis-dependent Close/Far releases on their existing FullBody path until that content is deliberately adapted; do not silently claim an upper-body copy of a176-degree pelvis-turning release is equivalent. This splice must resolve long-lived carry/aim freezing without hiding a separate release-content limitation.

Carry must yield once to the active throw and restore only when the same selected item is still valid. Keep the existing hand component's action ownership/suppression lifecycle; prevent idle refresh from restarting over cancellation or a higher-priority body action. Do not simultaneously play carry in RifleFire and preparation in Throwable.

Separate groups are useful, but not complete gameplay isolation. In this local UE version ordinary Montage_Play's `bStopAllMontages=true` stops the new montage's group; cross-group non-root-motion playback can coexist. However, the ASC still tracks one current montage/animating ability per mesh, explicit cancellation can stop clips, and root-motion montages have a separate single-owner rule. Preserve throw/fire/aim/reload/equipment arbitration and cancel the outgoing throw before a higher-priority GAS montage takes ownership. Do not globally flip bStopAllMontages or invent a new multi-montage ASC just for this layer.

The final ordinary weapon aim blend is upstream, so it cannot overwrite the completed throw overlay. Still maintain action-level aim/zoom/reticle and weapon-hand ownership. Do not globally disable both procedural rigs: Feet must continue for movable carry/aim; paired interaction hands belong to their higher-priority interaction and should not compete with a surviving throw action.

## Verification

Before mutation, keep the user's ABP backup and record the full graph edge/binding inventory. After authoring, the expected delta is the one replaced edge plus the new shared-cache branch. Verify that exact difference, the mask/root/curve settings, actual montage SlotAnimTracks and effective slot groups. Node count alone cannot establish a correct graph.

Compile and save using the project's safe native AnimBP workflow after the Python authoring call returns; do not call AnimBP compile/ReconstructNode/save_loaded_asset inside the Python mutation. Then re-read the graph and saved assets. No new automated tests. Let Artur run Play: equipped idle without RMB, walk/run/crouch, held aim while moving, cancel/restore, normal weapon aim/fire, full-body reaction/traversal, and foot placement. No active Throwable should pass the pre-throw source unchanged outside the masked branch's normal no-montage behavior.

Art remains Codex-owned. After this graph review, Artur requested both a brighter preview and a thicker curve. Codex set active Arc/Contact Sunlight MI Brightness=1.5 and ability PreviewStyle.PulseWidthPixels=7.5 (previously5.5). These are art-only changes, separate from the graph. The exposed line control is BP_AZ_GA_Throw → Class Defaults → Preview Style → Pulse Width Pixels; marker Width/Height Pixels are separate. No trajectory physics change. Preserve these latest values instead of resetting them to the earlier calibration.
