---
name: feedback_posesearch_branchin_db_sync
description: "For the v2 single-clip BranchIn databases (PSD_v2_*), DB membership is driven by the PoseSearchBranchIn notify on the SEQUENCE via an engine sync — AddBranchInNotify alone creates the synced DB entry; adding AddSequenceToDatabase too DOUBLE-adds. A clip whose BranchIn still points at a DB gets re-injected on every reindex. To swap clips: strip old clips' notifies, ClearDatabase, then AddBranchInNotify per new clip. entry.BranchInId must == notify.GetBranchInId() or reindex re-adds dupes."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
---

Learned doing the MovementAnimsetPro animset swap (2026-06-14), updating `PSD_v2_Loco_Loops` + `PSD_v2_Jump` from `RTG_RM_*` to `AnimPro_*`.

**The model:** these are single-clip BranchIn DBs (each `bUseMM=true` CHT row outputs one raw clip; MM searches it THROUGH its `UAnimNotifyState_PoseSearchBranchIn` notify, whose `Database` must index the clip). DB membership (`UPoseSearchDatabase::DatabaseAnimationAssets`) is **driven by the BranchIn notify on the SEQUENCE** via an engine sync (`NotifySynchronizeWithExternalDependencies`, runs on `PostEditChange`/PostLoad/reindex). The notify is the source of truth; entries are synced FROM it.

**Consequences (all observed):**
- `UAZ_PoseSearchUtils::AddBranchInNotify(seq, db, 0, 0)` **alone** creates ONE synced DB entry. Also calling `AddSequenceToDatabase` → **2 entries per clip** (explicit + synced). For these DBs use AddBranchInNotify ONLY. (The original Loco DB had 2×Walk+2×Run duplicates — exactly this double-authoring; the lands had 1 each.)
- An in-place pointer swap of an entry's `AnimAsset` (reflection) does NOT stick cleanly: the OLD clip still has its `BranchIn`→DB notify, so the next reindex **re-injects the old clip** as a fresh entry. Swapping while keeping the old `BranchInId` also confuses the sync (it adds a new entry for the now-"missing" old clip). Net: old + new both present.
- Each entry has `BranchInId` (uint32); the notify has `GetBranchInId()`. The sync links them by ID. **entry.BranchInId MUST == notify.GetBranchInId()**, else reindex/reload thinks the clip is unindexed and **adds a duplicate**. When the sync creates the entry (notify-only path), the IDs match automatically — verify before declaring done.

**Migration recipe (clip swap in a BranchIn DB) — all via one `script_type=cpp` snippet (see [[feedback_cpp_executescript_harness]]):**
1. `RemoveAllPoseSearchNotifies(oldClip)` for every clip being replaced → stops old clips re-syncing into ANY DB.
2. `ClearDatabase(db)` for each affected DB (resets entries).
3. Per new clip: `RemoveAllPoseSearchNotifies(newClip)` then `AddBranchInNotify(newClip, db, 0, 0)` (full-clip window matches the loops/lands convention — every old clip here had exactly one `[0,len]` BranchIn). Do NOT call AddSequenceToDatabase.
4. `db->PostEditChange(); db->MarkPackageDirty();`
5. Save via Python `save_loaded_asset` (DBs + new clips + the stripped old clips).
6. VERIFY: re-dump entries (count == #new clips, assets all new) AND entry.BranchInId == notify.GetBranchInId() for each → reload-stable.

**Reindex is automatic:** `FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex` runs on PostLoad and at every MM search; the index is DDC-keyed by content hash, so changing entries/notifies rebuilds it on next PIE. No manual reindex call needed.

Skeleton parity is a prerequisite: confirm old & new clips share the schema's skeleton (here `SKEL_SurvivalMan`) via AssetRegistry `Skeleton` tag before swapping. See [[project_jump_system_status]], [[reference_cht_chooser_structure]], skill `az-cpp-utility-tools`.

## SIBLING GOTCHA — locomotion LOOP clips also need their foot-contact curves copied (2026-06-14)
Swapping a locomotion loop to a new animset clip drops more than DB membership: the foot system reads the **`contact_l` / `contact_r`** float curves off the *currently-playing loop clip* (`AZ_MoverAnimInstance.cpp:280` `bLeftFootDown = GetCurveValue("contact_l") > 0.5`; `bRightFootDown` from `contact_r`). The chooser's `bLeftFootDown` BoolColumn uses this to pick the correct `_LU`/`_RU` stop/start/pivot variant. The MovementAnimsetPro `AnimPro_WalkFwdLoop`/`AnimPro_RunFwdLoop` shipped with **NO curves** (`get_animation_curve_names`→`[]`) while the old `RTG_RM_*` loops carried dense per-frame 0/1 step curves — so post-swap `bLeftFootDown` was stuck (always `_LU`).
**Fix = copy the curves (clips are same length → exact transfer), all in Python `unreal.AnimationLibrary` (AnimSequence curve edits + `save_loaded_asset` are GC-safe, no C++/Live Coding):**
```
times, values = AL.get_float_keys(src, "contact_l")          # tuple(Array times, Array values)
if AL.does_curve_exist(dst, c, RCT_FLOAT): AL.remove_curve(dst, c)
AL.add_curve(dst, c, unreal.RawCurveTrackTypes.RCT_FLOAT)
AL.add_float_curve_keys(dst, c, [float(t) for t in times], [float(v) for v in values])
EditorAssetLibrary.save_loaded_asset(dst, only_if_is_dirty=False)
```
Done for WalkFwdLoop+RunFwdLoop 2026-06-14 (verified element-wise exact). Other directional loops (strafe/back/crouch) would need the same IF used. (Notify reads are protected from Python, but CURVE reads/writes are fully Python-exposed via AnimationLibrary — unlike the PoseSearch notifies which need the C++ bridge.)
