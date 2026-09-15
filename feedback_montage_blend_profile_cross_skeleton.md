---
name: feedback_montage_blend_profile_cross_skeleton
description: "★★ CRASH: a duplicated/retargeted AnimMontage keeps its BlendProfileIn/Out pointing at a blend profile owned by the SOURCE skeleton. Blend profiles index by skeleton bone index, so evaluating one against another skeleton reads out of bounds — anim-worker crash in UBlendProfile::FillBoneScalesArray, and it can surface later as unrelated heap corruption. 44 such montages existed under /Game/AZ."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-14T20:19:32.495Z
---

# A retargeted montage keeps the SOURCE skeleton's blend profile — and it crashes

Hit 2026-09-14 while building the mantle. Crash on the anim worker thread:

```
FBoneContainer::GetCompactPoseIndexFromSkeletonIndex(int)   BoneContainer.h:616
UBlendProfile::FillBoneScalesArray(...)                     BlendProfile.cpp:315
FAnimInstanceProxy::SlotEvaluatePoseWithBlendProfiles(...)  AnimInstanceProxy.cpp:1662
FAnimNode_Slot::Evaluate_AnyThread(...)                     AnimNode_Slot.cpp:119
```

## The mechanism
`UAnimMontage` has `BlendProfileIn` / `BlendProfileOut` (`UBlendProfile*`). A blend profile stores per-bone
entries **by skeleton bone index** and belongs to ONE skeleton. Duplicating a montage — or retargeting one —
copies that pointer unchanged. The montage's own skeleton is then the new one while the profile's indices
still describe the old one, so `FillBoneScalesArray` walks indices from a 93-bone `SK_UEFN_Mannequin` against
a 342-bone `metahuman_base_skel` container. Out of bounds.

**It is intermittent**, because the profile is only touched on the blend it is attached to — clearing a
montage with an explicit blend-out (e.g. `ASC->CurrentMontageStop(BlendTime)`) is what started exercising it
every time. A second, totally unrelated crash stack appeared in the same session (render thread, inside
`FAppTime::Get()` during `UpdateReflectionSceneData`) — consistent with heap corruption from the OOB access
surfacing elsewhere, though that link was inferred, not proven.

## How to find it
Scan every montage and compare its skeleton against the blend profile's owner:
```python
bp = m.get_editor_property('blend_profile_out')      # and 'blend_profile_in'
owner = bp.get_outer().get_name()                    # the profile's skeleton
mismatch = owner != m.get_editor_property('skeleton').get_name()
```
On 2026-09-14 this found **44 mismatches** under `/Game/AZ`, all `FastFeet_InstantRoot` owned by
`SK_UEFN_Mannequin`: the whole retargeted GASP Traversal Catch/Climb set, the GASP walk-mantle montages, the
`RT_NWP_*` SurvivalMan retargets, plus bench-interaction and ragdoll-shove montages.

## The fix
Clear the property (`set_editor_property('blend_profile_out', None)`). A profile bound to a different
skeleton can never do anything useful on this one — it is inert at best and fatal at worst, so clearing is
strictly a repair. Authoring a real MetaHuman equivalent is the nicer long-term answer.

★ **Saving this needs the forceful API.** Both `EditorAssetLibrary.save_asset(path, False)` and
`save_loaded_asset(asset, False)` returned **False and did not write the file** (mtime unchanged five hours
later) — the trap in [[feedback_python_save_only_if_dirty]]. What worked:
```python
m.modify(True)
unreal.EditorLoadingAndSavingUtils.save_packages([m.get_outermost()], False)   # -> True
```
`Package` exposes neither `set_dirty_flag` nor `is_dirty` to Python; do not reach for them.
**Always verify by file mtime AND size, never by the return value.**

Related: [[feedback_python_save_only_if_dirty]], [[project_mantle_traversal_2026-09-14]],
[[feedback_ik_retargeter_exact_transfer]]
