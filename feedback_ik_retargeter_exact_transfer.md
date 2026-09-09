---
name: feedback_ik_retargeter_exact_transfer
description: "★★★ IK Retargeter exact transfer (2026-09-08): the deviation was an UNALIGNED retarget pose (zero offsets) + Run IK Rig op, NOT the chains. Compute per-bone offsets numerically (delta = inv(localRef)·inv(parentAlignedWorld)·sourceWorld, hierarchy order; engine applies LocalRef*Delta) -> 0.0 deg on every 1:1 bone. Pin Bones fix ONE bone, children stay wrong. Compatible skeletons never rest-compensate (MetaHuman hand 21 deg). 5.8 op-controller API traps inside."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-08T23:56:12.570Z
---

# IK Retargeter: how to get an exact transfer, and the traps (2026-09-08)

**Diagnosis pattern that worked.** Measure the rest-pose orientation difference per bone between the two
meshes (`AnimPoseExtensions.get_reference_pose`, WORLD space, geodesic angle). If the animated error is
CONSTANT across clips and frames, it is a rest-pose offset the retarget pose is not compensating — not a chain
problem. The existing RTG had both retarget poses at "Default Pose" with **zero offsets on every bone** and
`Run IK Rig` enabled: hand 11°, lowerarm 22°, upperarm 27° carried into every frame.

**The fix = a numerically aligned retarget pose, FK-only.** Engine applies `LocalRotation = RefPoseLocal *
Delta` (`IKRetargetProcessor.cpp:77`, globals rebuilt parent-first). Walk the TARGET hierarchy (parent map from
`SkeletalMeshEditorSubsystem.get_bone_parent`); for each bone shared with the source:
`delta = inv(local_ref) · inv(parent_aligned_world) · source_world`; non-shared bones keep identity and
`aligned_world = parent_aligned_world · local_ref`. Self-check with your own FK before writing (0.0000°). Write
with `IKRetargeterController.create_retarget_pose / set_current_retarget_pose /
set_rotation_offset_for_retarget_pose_bone(bone, Quat, TARGET)`. Result: **0.0° on every 1:1 bone**; only
chains with different bone counts (UE4 spine 3 → UE5 spine 5) keep a residual, and it does not reach the head.

**Pin Bone op is NOT a rest-pose fix.** Pinning hand_r/hand_l: hand → 0.0 but fingers → 17.9° and
pelvis/arms unchanged. Use pins for a single weapon/IK bone only.

**Compatible skeletons copy local rotation verbatim — never rest-compensated.** `metahuman_base_skel` lists
`SKEL_SurvivalMan` as compatible, yet the MetaHuman hand rest is 21.2° off (arms 2–3°, pelvis 5.7°, feet/head 0).
A plain-FK retarget with a default pose reproduces those exact numbers. `RTG_SurvivalMan_to_MetaHuman_Aligned`
(same topology, ONE_TO_ONE, aligned pose) = 0.0 everywhere. Consumption is an architecture choice: runtime
Retarget Pose From Mesh (Epic's MetaHuman pattern) / native clips / Modify Bone.

**Judge the VISIBLE hand, not just the bone (2026-09-08).** Bone-orientation error can be 0.0 while the
palm is still wrong: measure pure geometry — finger direction (`hand→middle_03`), across-palm
(`index_01−pinky_01`), palm normal (cross). MetaHuman vs SurvivalMan at rest: finger dir 5°, but across-palm
and palm normal **24°** — a real visual roll, not an axis convention. Animated, vs the SurvivalMan source:
compatible playback (today's game) palm roll **24°** (= the twisted rifle); pin hands: bone 0.0 but palm
**25.9°** (finger bases carry their own rest delta); **aligned pose: palm 3.9° / normal 8.3°**, socket 0.0°.
The MetaHuman body's `RightHandRifleSocketAim/Relaxed` were hand-tuned to compensate that 24° roll
(`Relaxed` pitch −21.8 vs SurvivalMan −2.0) — with the aligned path they must be reverted to SurvivalMan's
values, and never reverted without it. Evaluate a clip on another mesh with
`AnimPoseEvaluationOptions.optional_skeletal_mesh` to measure true compatible-skeleton playback.

**Feet:** 5–9 % shorter legs float 1.7–3 cm with FK-only. Leg-only `Run IK Rig` and the pelvis ground knobs
(`affect_ik_vertical`, `use_ground_falloff`, `floor_constraint_weight`) changed nothing — do not iterate there;
use a constant pelvis Z offset or runtime foot IK.

**5.8 controller API traps (all verified):**
- `set_retarget_chain_settings` → `ChainSettings_DEPRECATED`, ignored by the op processor. Use
  `rc.get_op_controller(i).get_settings()/set_settings()`; FK op field `chains_to_retarget`
  (`RetargetFKChainSettings`: `rotation_mode` = `FKChainRotationMode.ONE_TO_ONE/INTERPOLATED/COPY_LOCAL…`,
  `translation_mode` = `FKChainTranslationMode.GLOBALLY_SCALED/NONE/…`); IK op field `chains` (`enable_ik`).
- `add_retarget_op('/Script/IKRig.IKRetargetRunIKRigOp')` — full struct path (`FindObject<UScriptStruct>`).
- `add_default_ops()` = Pelvis / FK / Run IK Rig / Root Motion / Remap Curves — and Root Motion defaults its
  root bone to `pelvis`: `set_source_root_bone('root')` / `set_target_root_bone('root')`.
- `EAL.duplicate_asset(EpicRig, new)` + `IKRigController.set_skeletal_mesh(mesh)` rebinds a rig to another
  skeleton; `add_retarget_chain(name, start, end, 'None')`. Chain names must be IDENTICAL on both rigs for
  `auto_map_chains(EXACT, True)`; assert every source chain maps (`get_source_chain`).
- Epic's official rigs (`IK_UE4_Mannequin_Retarget`, `IK_UE5_Mannequin_Retarget`, `IK_Metahuman_Retarget`)
  share 30 chain names/spans but contain NO `ik_*` or twist chains — add them.
- `IKRetargetBatchOperation.duplicate_and_retarget(assets, src_mesh, tgt_mesh, rtg, search, replace, prefix,
  suffix, target_path, use_source_path, include_referenced_assets, overwrite)` — output copies the SOURCE's
  loop flag and drops project notifies; prefer an in-place bone-track transfer into the existing assets.
- Deprecated-property reads raise in this Python bridge: `warnings.simplefilter('ignore')`.
- `UE5 Manny ≡ SKEL_SurvivalMan` (0.00° on every bone); UE4 Manny is the only odd topology (spine 3, neck 1).

**★ The MetaHuman hand fix that SHIPPED (2026-09-08) = two Modify Bone nodes, not a retarget.** Under
compatible-skeleton playback every local rotation is copied verbatim, so the world error at a bone is a
CONSTANT in that bone's own frame — pose-independent. Measured over 12 samples (aim/relaxed/crouch idles,
jump air): one bone-space additive rotation on `hand_r`/`hand_l` — `(P −0.2086, Y −3.1971, R 20.9491)`,
identical for both hands — puts the hand on SurvivalMan's orientation to **0.04°** and the visible palm to
3° (was 24°), equal to the full aligned retarget. Correction = `inv(handLocal_today) · inv(lowerarmWorld_MH)
· handWorld_SM` = right-multiply in the hand frame = `BCS_BoneSpace` + `BMM_Additive`
(`InOutBoneSpaceTM *= BoneTM`). Authored in `AZ_ABP_MoverHero_MHC` with `AZ_AnimGraphNodeUtils`
(`add_anim_graph_node` `/Script/AnimGraph.AnimGraphNode_ModifyBone`, `set_anim_node_property('Node.…')`,
`connect_pose_link` — the schema inserts Local↔Component conversions itself); user compiles + saves.
The body's tuned `RightHandRifleSocketAim/Relaxed` were reverted to SurvivalMan's values in the same step
(`Tools/metahuman_fixup.py`). **Path A (SurvivalMan driver + retargeted MetaHuman child, GASP pattern) was
fully designed and coded, then PARKED as too heavy for "a small hand adjustment"** — design in the plan doc
§7, patch in the session scratchpad. Ask before choosing a structural fix when a constant corrects the bug.

**Trap:** `AnimPoseExtensions.get_reference_pose(metahuman_base_skel)` returns the FEMALE-MEDIUM archetype
(head 143 cm) — the skeleton asset is shared by every MetaHuman body; the hero mesh rests at 162 cm like
SurvivalMan. Read rest numbers off the MESH (spawn a `SkeletalMeshActor`, `get_socket_transform`).

See [[feedback_retarget_root_motion]], [[feedback_python_save_only_if_dirty]], [[feedback_metahuman_modular_hero]],
and `docs/design-briefs/rifle-p01-retarget-rebuild-plan.md` §5 (UE4→SurvivalMan), §7 (path A, parked), §8 (path C).
