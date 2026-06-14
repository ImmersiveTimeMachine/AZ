---
name: reference_ue5_python_posesearch
description: Working Python script pattern for creating PoseSearch Schema + Database assets in UE5.7 editor
metadata: 
  node_type: memory
  type: reference
  originSessionId: 787f844b-69e1-48c0-8b39-9a9264829d57
---

## Creating PoseSearch Assets via Python in UE5.7

Script location: `Content/Python/create_posesearch_databases.py`

### What Works
- `asset_tools.create_asset(name, path, unreal.PoseSearchSchema, None)` — creates schema
- `asset_tools.create_asset(name, path, unreal.PoseSearchDatabase, None)` — creates database
- Pass `None` as factory (no factory class needed)
- `unreal.EditorAssetLibrary.make_directory(path)` for folder creation
- `unreal.EditorAssetLibrary.save_asset(full_path)` to save
- `unreal.EditorAssetLibrary.does_asset_exist(path)` to check before creating

### Adding Animations to Databases
Python can't access `DatabaseAnimationAssets` (private) or call `AddAnimationAsset` (not a UFUNCTION).
Solution: Custom C++ bridge `UAZ_PoseSearchUtils` in `Source/AZ/Public/Animation/AZ_PoseSearchUtils.h`:
- `AddSequenceToDatabase(Database, Sequence)` — sets `AnimAsset` on `FPoseSearchDatabaseAnimationAsset`, calls `AddAnimationAsset`
- `AddSequencesToDatabase(Database, Sequences)` — batch version
- Requires `PoseSearch` in Build.cs PublicDependencyModuleNames
- Python call: `unreal.AZ_PoseSearchUtils.add_sequences_to_database(db, [seq1, seq2, ...])`

### Schema Configuration via Python
- Skeleton: `PoseSearchRoledSkeleton` → `set_editor_property("skeleton", skel)` → set on schema via `set_editor_property("skeletons", [entry])`
- Pose Channel: `PoseSearchFeatureChannel_Pose` → `sampled_bones` array of `PoseSearchBone` with `reference` (BoneReference, set `bone_name`) and `flags` (3 = Position+Velocity)
- Trajectory Channel: `PoseSearchFeatureChannel_Trajectory` → `samples` array of `PoseSearchTrajectorySample` with `offset` (float seconds) and `flags` (3)
- Set channels: `schema.set_editor_property("channels", [pose_ch, traj_ch])`

### What Does NOT Work
- `unreal.PoseSearchSchemaFactory()` — class doesn't exist in Python bindings
- `unreal.PoseSearchDatabaseFactory()` — class doesn't exist in Python bindings
- `db.get_editor_property("database_animation_assets")` — private, not accessible
- `db.get_editor_property("DatabaseAnimationAssets")` — protected, blocked

### Animation Loading
- `unreal.load_asset("/Game/path/to/AnimSequence")` works for loading anim sequences
- Retargeted anims in project use `sv_` prefix with optional `1` suffix (sv_Idle2 vs sv_Idle21)

### Schema/Database Configuration
Schema bone channels and trajectory channels must be configured manually in editor after Python creation — no Python API to set channels.

### Run Command
```
py "C:/UnrealEngine/Games/AZ/Content/Python/create_posesearch_databases.py"
```

### Required Plugins
PoseSearch, Chooser, BlendStack, MotionTrajectory, PythonScriptPlugin — all in AZ.uproject

## UE5.8 update (2026-05-31) — working recipe for MM over OUR clips (SKEL_SurvivalMan)

Built a from-scratch locomotion-loops MM DB; see [[project_v2_locomotion_progress]] milestone for the full chain. Key facts that differ from / extend the above:

- **Schema: DUPLICATE + repoint skeleton beats authoring channels.** `EditorAssetLibrary.duplicate_asset(GASP "PSS_Default", dst)` then repoint the skeleton: `sks = list(schema.get_editor_property("skeletons")); for e in sks: e.set_editor_property("skeleton", skel); schema.set_editor_property("skeletons", sks); EditorAssetLibrary.save_asset(dst)`. In 5.8 PoseSearchSchema uses **`skeletons`** (plural, `PoseSearchRoledSkeleton` array) NOT `skeleton`, and has **NO `post_edit_change`** (don't call it — it throws, aborting the save). Reuses GASP's Pose+Trajectory channels; works because standard UE bone names (`foot_l/r`, `thigh_l/r`, `pelvis`, `spine_05`, `hand_l/r`) exist on both skeletons. `BuildIndex Succeeded` in the editor log = schema valid.
- **`unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, path, unreal.PoseSearchDatabase, None)`** then `db.set_editor_property("schema", schema)` + `UAZ_PoseSearchUtils.add_sequences_to_database(db, seqs)`. DB indexes on save/access (DDC). Opening the SCHEMA editor afterwards CANCELS the DB index (`PreCancelled because of PSS...`) — rebuilds on next access; close schema/DB tabs before PIE.
- **Don't bother with BranchIn notifies.** `MotionMatch` AssetsToSearch accepts a **`UPoseSearchDatabase` directly** — pass the DB, MM searches all its clips and picks by trajectory (cross-clip gear change). `UAZ_PoseSearchUtils.AddBranchInNotify` does NOT set the notify's `Database`, and Python can't reach `AnimSequence.Notifies` (protected-read AND write) to fix it — so the DB-direct path is the only clean one from script.
- **Cookability:** an asset referenced only by a C++ `LoadObject(path)` is ORPHANED (Reference Viewer shows nothing → won't cook). Reference it via a `UPROPERTY(EditDefaultsOnly) TObjectPtr<UPoseSearchDatabase>` assigned in the ABP CDO instead.
- **🔑 If the chooser output struct (`bUseMM`/`BlendTime`/...) arrives default at runtime, check the OUTPUT column's `ContextIndex`** — it must point at the `AZ_ChooserOutputs` context. `AZ_ChooserUtils` hardcodes `FOutputStructColumn` `Binding.ContextIndex = 1` (a 2-context assumption); the 3-context CHT_v2 (`[0]` AnimInstance, `[1]` AZ_v2_ChooserContext, `[2]` AZ_ChooserOutputs) needs **2**, else the matched row's struct goes to the wrong context and the EvaluateChooser node outputs defaults. Fix: `set_column_binding_chain(cht, outCol, [], 2)`. (Output-side twin of the `bLeftFootDown` wrong-context bug.) ⚠ multi-vs-first-match mode is a RED HERRING here — it does NOT cause this. Full story: [[project_v2_locomotion_progress]].
