---
name: reference-mirror-anim-bake
description: "Recipe + traps for baking mirrored AnimSequences in UE 5.8 from Python (Tools/mirror_throw_anims.py): MirrorDataTable creation, the MirrorPose port, and six API gotchas that fail silently."
metadata:
  node_type: memory
  type: reference
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-09-22T03:30:06.567Z
---

Mirroring an animation for CHALK is done **inside Unreal with a Mirror Data Table**, never in Blender.
A Blender mirror needs a negative object scale, which flips face winding and imports inside-out, and it
leaves `_l`/`_r` bone names unswapped. Working script: `Tools/mirror_throw_anims.py` (2026-09-21 — baked
the six `AZ_RTG_MH_Throw*` clips to `*_Mirrored`, verified to <1e-4 cm).

**Bake, don't use the AnimGraph Mirror node,** when the clips are root-motion montages: root motion is
extracted from the montage, bypassing the graph, so a graph-side mirror flips the pose and leaves the root
delta pointing the old way.

**The axis is X** for UE/MetaHuman skeletons. Confirm on the **reference pose**, never the animated one:
`hand_l` X=+40.08 / `hand_r` X=−40.08 on `metahuman_base_skel`. An animated frame can put the hands
anywhere and reading it suggests the wrong axis. Epic's `MDT_UEFN_Mannequin` and `AZ_Hero_MDT` both use X.

**How to apply / traps (each of these cost a round trip):**
1. `UMirrorDataTableFactory::Skeleton` is **protected** — unreachable from Python, and calling the factory
   without it opens a modal skeleton picker that hangs the MCP call. Duplicate an existing MDT
   (`AZ_Hero_MDT` carries the standard find/replace list) and set `skeleton` on the copy.
2. Rows go in with `fill_from_csv_string` (returns a **bool**, not a problem list). Header:
   `---,Name,MirroredName,MirrorEntryType,bEnabled`.
3. **Centre bones need self-rows** (`Name == MirroredName`). A bone with no row gets `INDEX_NONE` from
   `FillMirrorBoneIndexes` and `MirrorPose` leaves it **completely untouched** — the spine keeps leaning
   the old way while the arms swap. Despite the comment in the engine source there is no self fallback.
4. `get_animation_track_names` and `find_bone_path_to_root` **disagree on casing** for MetaHuman correctives
   (`upperarm_correctiveroot_r` vs `upperarm_correctiveRoot_r`). FName does not care, Python does. Resolve
   every lookup through a `lower()` index.
5. The enum is `unreal.AnimPoseSpaces` (plural); the controller is `seq.controller`, **not**
   `seq.get_controller()`.
6. `unreal.Rotator(a, b, c)` positional order is **(roll, pitch, yaw)**. Assemble by field.
7. `get_editor_property` on a struct property returns a **live view**: read "old", call `set_editor_property`,
   and the old handle now prints the new value. Capture the numbers before writing or the log lies.

The math is a 1:1 port of `FAnimationRuntime::MirrorPose` / `MirrorQuat` / `MirrorVector`
(`Engine/Source/Runtime/Engine/Private/Animation/AnimationRuntime.cpp`). The corrective term
`Q *= MirrorQuat(SourceBoneRef).Inverse() * TargetBoneRef` is what keeps wrists and clavicles from rolling;
do not drop it. Write with the recipe in [[feedback_python_gc_crash]]:
`controller.open_bracket` → `set_bone_track_keys` → `close_bracket`, key count == `get_num_keys`.

Anything calibrated against the old hand (grip socket, release anchors) must be **mirrored, not
re-measured** — mirroring preserves whatever calibration was baked in, and the mirrored clip reproduces
`MirrorX(source socket pose)` at every time.

Related: [[feedback_python_save_only_if_dirty]], [[feedback_verify_never_presume]],
[[project_throwable_resume_2026-09-17]].
