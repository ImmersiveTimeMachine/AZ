---
name: project_rifle_mh_native_migration_2026-09-09
description: "★★★ 2026-09-09: the RIFLE animation set is MetaHuman-NATIVE (/Game/AZ/Assets/M16/Riffle_RTG_MH/AZ_RTG_MH_*, user's own batch retarget, default-pose quality: hand 15 deg) — CHT/6 PSD_P01/DA/AO/montage repointed; metadata (loop, rm, force_root_lock, additive base, reconstructed root, contact curves) transplanted from Riffle_P_*; sockets are per-skeleton (fixup preserves them). Traps: additive ref_pose_type, BranchIn double-add, struct-array copies, index staleness in PIE."
metadata: 
  node_type: memory
  type: project
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-09T05:27:59.050Z
---

# Rifle set = MetaHuman-native clips (2026-09-09)

**State.** `CHT_v2` (50 refs), `PSD_P01_{Crouch,Jog,Walk}{Aim,Explore}` (8 members each),
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
- **Sockets are per skeleton.** Tuned `RightHandRifleSocketAim/Relaxed` on `SKM_MHC_Hero_BodyMesh` are
  the user's; `Tools/metahuman_fixup.py` now PRESERVES them. `…Relaxed` was derived from the user's `…Aim`
  (rifle-in-hand re-expressed in the hand_r frame): loc (−6.63, 4.61, −1.86) rot (P −3.73, Y 102.63, R 18.36).
  The preview-vs-game "difference" was this socket, not the pose.
- The user's batch left duplicates with a `1` suffix (`…_D91`, `…_Center1`) — name collisions, harmless.

**Two-strike facts kept:** IK-arm goals in the 5.8 retargeter can't reach the source grip (no blend-to-source;
3.0–3.5 cm at 3–4° arm cost) — the 4.7 cm left-hand offset is inherent to MH arm length under FK.
`AimYaw` reads −90 when looking straight ahead (pre-existing; check the AO axis convention if aiming looks off).

See [[feedback_ik_retargeter_exact_transfer]], [[feedback_posesearch_mm_mechanism_rules]] (R17),
[[feedback_python_save_only_if_dirty]], [[feedback_metahuman_modular_hero]].
