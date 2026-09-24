---
name: project_rifle_mh_native_migration_2026-09-09
description: "★★★ 2026-09-09: the RIFLE animation set is MetaHuman-NATIVE (/Game/AZ/Assets/M16/Riffle_RTG_MH/AZ_RTG_MH_*, user's own batch retarget, default-pose quality: hand 15 deg) — CHT/6 PSD_P01/DA/AO/montage repointed; metadata (loop, rm, force_root_lock, additive base, reconstructed root, contact curves) transplanted from Riffle_P_*; sockets are per-skeleton (fixup preserves them). Traps: additive ref_pose_type, BranchIn double-add, struct-array copies, index staleness in PIE."
metadata: 
  node_type: memory
  type: project
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-23T03:26:12.659Z
---

# Rifle set = MetaHuman-native clips (2026-09-09)

## ★ 2026-09-23: the MH-native set LEANS BACK — constant spine error from the default-pose retarget
Artur: "feels like I'm bent backwards" (rifle walk + unarmed idle screenshots). Measured with
`AnimPoseExtensions.get_anim_pose_at_time` + `optional_skeletal_mesh` (hero body
`/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh` vs `/Game/SurvivalMan/Meshes/SKM_SurvivalMan_Mesh1`),
torso = pelvis→neck_01 tilt in mesh Y-Z (forward = +Y, negative = back):
- REST poses already read negative (SM −3.9°, MH −3.2°) — that is spine geometry, the baseline, not a lean.
- `AnimPro_Idle` (unarmed, SurvivalMan skeleton, compatible playback): SM −2.8° / MH −2.1° → **no pipeline error**;
  the idle is authored that way. Runtime (ABP) adds <1° (head vs pelvis from `[v2 CrouchEnd]`) → ABP is clean.
- Rifle walk: source `Riffle_P_W2_Walk_F_Loop_IPC` on SM −1.5°, same source via compatible playback on MH −0.7°,
  **`AZ_RTG_MH_W2_Walk_F_Loop_IPC` −4.7°** (3.2° further back). Per-segment diff MH-native − SM source is CONSTANT
  over the loop: pelvis>spine_01 +7.9, spine_01>02 +0.3, **spine_02>03 −12.9**, 03>04 −2.1, 04>05 −5.4, 05>neck −4.4,
  neck>head −0.6 (rest-geometry diffs only +4.0/+2.2/−1.7/−0.2/−2.9/+1.5/−1.0) → spine_02 world orientation ~11° back.
  Cause: the user's batch retarget used the DEFAULT retarget pose (the same reason as the 15° hand), not
  `RTG_SurvivalMan_to_MetaHuman_Aligned`. Because the error is constant, it is fixable like the hand fix
  (constant bone-space correction per bone), either in the clip data or at runtime.
- **STATE 2026-09-23 ~02:40 UTC: all 923 clips restored from backup (MD5-identical). Orientation-copy method
  REJECTED** (overshoots +2°: bone axes differ between UE4 / SurvivalMan / MetaHuman; even the "exact 0.0°"
  `RTG_RifleP01_UE4_to_MetaHuman` leans −4.9° vs source −1.3°; `RTG_RifleP01_UE4_to_SurvivalMan` currently acts as
  default-pose). **Chosen method = GEOMETRIC** (`Tools/rifle_mh_spine_fix.py`): per frame swing each MH
  spine/neck segment onto the UE4 mocap spine curve by arc-length fraction, then swing the whole spine so
  pelvis→neck_01 equals the source; pelvis untouched, head+clavicles held, `target.modify()` so Save All sees it.
  Final pilot: torso lean = source to 0.1° on all 4 (idle/start/loop/stop). MODE='referenced' ran 02:56 UTC:
  fixed=192, skipped=17 (the additive Aim_Point_*). Artur saved + tested: head bobbed forward/back "like a
  goose" on turns — cause: neck re-aimed at the UE4 neck while head world was held. Restored backup, re-ran
  (03:03 UTC) SPINE ONLY (spine_01..05 swung; neck_01/neck_02/head keep local rotations and ride the chest;
  clavicles held): same exact lean, 192 fixed. Artur: turns still "twist back then return". MEASURED cause:
  MH clips carry the turn in the ROOT (extracted), UE4 sources turn the PELVIS → hip heading drifts 90/180°
  over a pivot, so world-space source directions flipped forward↔back mid-turn. Fix: per frame yaw the source
  onto the clip's hip heading (thigh_l→thigh_r axis, convention-free). Restored backup again, pilot 6 clips incl.
  L90/R180 turn starts: body-relative lean = source on EVERY frame (worst 0.0°). 03:25 UTC referenced run:
  192 fixed + dirty → awaiting Save All + PIE turn check. Note: body-relative metric sign = +back.
  Pistol set still to be checked (`/Game/AZ/Assets/Pistol/Runtime/AS_Pistol_*`). Direct to MetaHuman, NO proxy (Artur's decision). Lesson: an in-memory batch
  got half-saved by a UI Save All + an editor crash — restore from the file backup, not from memory.
- (history) **FIX IN PROGRESS 2026-09-23 (Artur: "да, исправляй набор винтовки, idle не трогай"; pistol must be checked too).**
  Per-bone K was constant only for pelvis 9.5°/spine_05 6.6°/head 1.3°; spine_01..04 + neck_02 vary per frame
  (interpolated spine chain) → per-FRAME fix, not a constant: spine_01..head world = source world, pelvis
  untouched (legs/contacts), clavicles held at current world (rifle grip/aim as tuned); additive Aim_Point_*
  skipped (bone-frame constant cancels in mesh-space deltas). Pilot Walk_F: 0.00° vs source, clavicle/pelvis/
  foot/hand 0.00° moved, torso −4.7° → +0.4°. Tools: `Tools/rifle_mh_spine_fix.py` (118 clips with a
  `Riffle_P_` SurvivalMan twin; 101 fixed in memory) + `Tools/rifle_mh_spine_fix_ue4.py` (the ~140
  game-referenced clips whose only source is UE4: retarget source → SurvivalMan with the SAME
  `RTG_RifleP01_UE4_to_SurvivalMan` / `RTG_RifleAnimsetPro_SurvivalMan` into temp `/Game/AZ/Temp/SpineFixSrc`
  (SFX_*), fix, cleanup). ~660 unreferenced UE4-sourced clips left as-is. Game refs: 209 clips (69 loops with
  SM twin + 140 UE4-only). **Edits are IN MEMORY: the user must Save All from the UI** (Python save of
  PSD-indexed clips deadlocks). Backup of the original 923 files: `Saved/Backups/Riffle_RTG_MH_before_spine_fix_2026-09-23`.

**State.** `CHT_v2` (50 idle/transition refs + 126 root-motion refs: the 122 `rm_W2_*` SurvivalMan clips
from `/Game/AZ/NoWeapons/RootMotions/` → `AZ_RTG_MH_W2_<x>` — the pack's non-IPC clips, the `rm_` prefix
was an import artefact; twins had the root motion but not the flags), `PSD_P01_{Crouch,Jog,Walk}{Aim,Explore}` (8 members each),
`DA_WeaponAnim_P01.play_rate_loop_assets` (48), `AO_Rifle_Aim` (17 samples), `AZ_AM_Rifle_Fire` (segment
→ `AZ_RTG_MH_W2_Stand_Fire_Single`, 0–0.379 s window) all point at `AZ_RTG_MH_*` (`metahuman_base_skel`,
919 clips, gitignored under Content/AZ/Assets). Still SurvivalMan-sourced: `PSD_P01_Land` + 10
`AS_P01_Jump_*` composites + 2 `Fall_v2`. The exact direct retargeter `RTG_RifleP01_UE4_to_MetaHuman`
(0.0° every bone, pose `MH_AlignedToUE4Manny`, IK-arm op saved disabled) exists if the user wants the set
regenerated into the same names. Full record: `docs/design-briefs/rifle-p01-retarget-rebuild-plan.md` §9.

**Why it plays through an SKEL_SurvivalMan AnimBP:** `USkeleton::IsCompatibleForEditor` accepts the reverse
direction via the asset-registry `CompatibleSkeletons` tag (Skeleton.cpp:145+), so MH clips are accepted by
SM-schema PSDs / SM blendspaces / SM montages; at runtime the mesh IS metahuman_base_skel so they play native.
Verified by sampling the live PIE skeleton vs the clip on the same mesh: ≤0.1° every bone, grip vector equal.

**Metadata a raw batch retarget lacks (all had to be transplanted from the old set):** `loop` (idles were
False), `enable_root_motion` + `force_root_lock` (loops), the R17 reconstructed root on IPC loops (copied per
frame via `controller.set_bone_track_keys` — walk F 145.7 cm/1.17 s, jog 223.5, crouch 108.4, fwd +Y right
−X), `contact_l/contact_r` (copied by `AnimPoseExtensions.get_curve_weight` sampling + `add_float_curve_keys`),
and the additive setup on the 20 `Aim_Point_*` clips.

**Traps (each cost a PIE or a crash):**
- **Additive base type.** Set `ref_pose_type = ABPT_ANIM_FRAME` BEFORE `ref_pose_seq`; with the default
  `ABPT_REF_POSE` the engine drops `ref_pose_seq` and bakes the aim offset against the A-pose → upper body
  flung around in-game while the raw clip previews fine.
- **PSD membership is BranchIn-synced** (`PreSaveRoot`). `remove_all_pose_search_notifies(old)` +
  `add_branch_in_notify(new, db, 0, 0)` + save; calling `add_sequences_to_database` as well double-adds
  (16 members). `Tools/rifle_p01_setup.py` says so; ignored once.
- **Struct arrays are copies in Python** (`BlendSample`, `AnimSegment`): build new instances into a fresh
  `unreal.Array` and assign; mutating elements of `get_editor_property()` writes nothing.
- **`save_asset` returns False after PreSaveRoot saved** — verify by mtime.
- **Indexes don't rebuild inside a running PIE.** Costs of 3,000–22,000 on the loops = stale index (built
  before the root rebuild). Stop/start PIE (or open the DB) after editing member clips.
- **Sockets are per skeleton, and the user's tuned socket is the reference — do not derive.** The pawn's
  mesh is `/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh` (has the 9 sockets); the
  retarget-source twin `/Game/MetaHumans/MHC_Hero/Body/…` has none. The user tuned `RightHandRifleSocketAim`
  on `middle_01_r` and previews every clip attached there; `…Relaxed` is now IDENTICAL to it (bone +
  transform), so aim/relaxed attach the same. Two numeric derivations were rejected by eye: re-expressing
  the aim placement in the `hand_r` frame (40° off — the Aim socket rides a finger whose curl differs 27°
  between poses) and a swing about hand_r putting the M16's `LeftHandGrip` on the `hand_l` bone (2.3 cm at
  the wrist, still visibly off the palm — the wrist-bone metric is not what the eye judges). Live-PIE
  sampling proved the pose itself equals the clip (≤0.1° every bone) — every "game vs preview" difference
  was the attach socket. `Tools/metahuman_fixup.py` preserves both rifle sockets.
- The user's batch left duplicates with a `1` suffix (`…_D91`, `…_Center1`) — name collisions, harmless.

**Two-strike facts kept:** IK-arm goals in the 5.8 retargeter can't reach the source grip (no blend-to-source;
3.0–3.5 cm at 3–4° arm cost) — the 4.7 cm left-hand offset is inherent to MH arm length under FK.
`AimYaw` reads −90 when looking straight ahead (pre-existing; check the AO axis convention if aiming looks off).

See [[feedback_ik_retargeter_exact_transfer]], [[feedback_posesearch_mm_mechanism_rules]] (R17),
[[feedback_python_save_only_if_dirty]], [[feedback_metahuman_modular_hero]].
