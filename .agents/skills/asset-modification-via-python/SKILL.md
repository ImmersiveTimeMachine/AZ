---
name: asset-modification-via-python
description: Recipes for modifying AZ assets (BPs, AnimBPs, AnimSequences, PoseSearch databases, Choosers, IK Retargeter outputs) via Python through mcp execute_script. Covers what works directly via unreal Python API, what requires the UAZ_PoseSearchUtils / UAZ_AnimGraphNodeUtils / UAZ_BlueprintNodeUtils C++ bridges, what crashes the editor (ReconstructNode/CompileBlueprint/save_loaded_asset on AnimBPs from Python), the Anim Notifies are protected pitfall, and the IK Retargeter "duplicate the asset to bake root motion" workaround.
---

# Asset Modification via Python (mcp `execute_script`)

How to drive AZ asset edits from Python through `mcp__unrealclaude__unreal_execute_script`. Schema is deferred — load with `ToolSearch query="select:mcp__unrealclaude__unreal_execute_script"`.

## Run-command template

```python
# @Description: <one-line summary, REQUIRED header field>
import unreal
# ... body ...
import gc; gc.collect()  # always at end — clean Python refs before user compiles
```

`script_type=python` runs in the editor's `PythonScriptPlugin`. Output goes to `LogPython` (filterable via `unreal_get_output_log`). For C++-domain edits use `script_type=cpp` (Live Coding compile per `cpp-build-livecoding` skill).

## Hard rule — these CRASH the editor (Python GC vs UE GC)

NEVER call from `script_type=python`:

- `unreal.KismetEditorUtilities.compile_blueprint(bp)` / any compile-blueprint helper
- `<Node>.reconstruct_node()`
- `<Blueprint>.mark_blueprint_as_structurally_modified()`
- `unreal.EditorAssetLibrary.save_loaded_asset(<AnimBP>)` (specifically AnimBPs — regular assets are usually OK)

**Why:** The Python plugin hooks UE's pre-GC delegate. Compile triggers GC; Python tries to collect its tracked UObjects simultaneously → crash in `PyUtil::CollectGarbage` → `FPythonScriptPlugin::OnPreGarbageCollect`.

**Workaround:** in C++ utilities use `Modify()` instead of `ReconstructNode()`; let the user compile manually (Ctrl+F7) or close/reopen the ABP after the script runs. End every script with `gc.collect()`. `script_type=cpp` is exempt — it runs in the C++ thread context. Full reasoning: `feedback_python_gc_crash.md`.

## PoseSearch database creation (factory=None)

```python
import unreal
tools = unreal.AssetToolsHelpers.get_asset_tools()
schema = tools.create_asset("PSS_NoWeapon", "/Game/AZ/.../Schemas", unreal.PoseSearchSchema, None)
db     = tools.create_asset("PSD_NoWeapon_Idles", "/Game/AZ/.../Databases", unreal.PoseSearchDatabase, None)
unreal.EditorAssetLibrary.save_asset(schema.get_path_name())
```

Pass `None` as the factory — `unreal.PoseSearchSchemaFactory()` / `unreal.PoseSearchDatabaseFactory()` don't exist in Python bindings. Schema channels (Pose, Trajectory) configurable via `set_editor_property` on `PoseSearchFeatureChannel_*` instances; full recipe in `reference_ue5_python_posesearch.md`.

## Adding anims to a PoseSearch DB (C++ bridge required)

`DatabaseAnimationAssets` is private and `AddAnimationAsset` isn't a UFUNCTION — Python can't reach them. Use the bridge:

```python
db = unreal.load_asset("/Game/AZ/.../Databases/PSD_NoWeapon_WalkLoops")
seqs = [unreal.load_asset(p) for p in [...]]
unreal.AZ_PoseSearchUtils.add_sequences_to_database(db, seqs)
```

Other DB ops: `add_sequence_to_database`, `remove_animation_at_index`, `clear_database`. See header `C:\UnrealEngine\Games\AZ\Source\AZ\Public\Animation\AZ_PoseSearchUtils.h`.

## Adding anim notifies (C++ bridge required)

`AnimSequence.Notifies` is a protected property — Python cannot set it directly. Use the bridge:

```python
seq = unreal.load_asset("/Game/AZ/.../Anim_NoWeapon_Walk_F")
unreal.AZ_PoseSearchUtils.add_block_transition_notify(seq, 0.1, 0.8)
# Bulk: 80% coverage, 10% margin at start/end across all anims in a DB
unreal.AZ_PoseSearchUtils.add_block_transition_to_database(db)
```

Available helpers (all in `UAZ_PoseSearchUtils`): `AddBlockTransitionNotify`, `AddBranchInNotify`, `AddExcludeFromDatabaseNotify`, `AddModifyCostNotify`, `AddOverrideContinuingPoseCostBiasNotify`, `RemoveAllPoseSearchNotifies`. Per-category application rules: `reference_gasp_anim_notifies.md`. Full recipe + saving: `reference_ue5_python_anim_notifies.md`.

## Modifying BP / AnimBP nodes from Python

Use the AZ C++ utilities (`UAZ_BlueprintNodeUtils`, `UAZ_AnimGraphNodeUtils`, `UAZ_AnimBlueprintUtils`) — see skill `az-cpp-utility-tools`. Direct `unreal.EdGraphSchema_K2.try_create_connection` calls are slow and lack the type-coercion the utilities provide.

Notify-state and notify nodes get added via the bridge listed above; do NOT call `ReconstructNode` after.

## Chooser table edits

`UAZ_ChooserUtils` exposes everything: `ConfigureAnimChooser`, `AddAnimRow` / `AddDatabaseRows` / `AddDatabaseRowsToNested`, nested sub-chooser builders, enum/property/asset rebinding, `AutoRemapChooserAssets` (Jaccard fuzzy matcher with dry-run support), `DumpChooserFullTree`, `DecodeEnum`. After edits call `compile_and_save(chooser_path)` — Choosers are not AnimBPs and won't trigger the GC crash.

## InputAction / IMC creation (5.7 deprecation)

In UE 5.7 the old `add_mapping(action, key)` API on `UInputMappingContext` is deprecated. Use `default_key_mappings.mappings` (the underlying TArray) — `set_editor_property` on the IMC, populate `FEnhancedActionKeyMapping` entries. Wrap in `unreal.EditorAssetLibrary.save_loaded_asset(imc)` (regular asset, no GC issue).

## IK Retargeter root motion — duplicate the asset

After "Duplicate and Retarget Animations" in the IK Retargeter, root translation/rotation is **not baked into the `root` bone** even with correct chain settings (One-to-One rotation, Absolute translation, α=1.0). **Fix:** right-click the retargeted asset → Duplicate. Use the duplicate, not the original. Forces UE to fully serialize, baking root motion correctly. Verify in Persona with "Process Root Motion" enabled — character should visibly translate. Apply to ALL retargeted anims that need root motion (TIP, locomotion starts/stops, traversals). Full reasoning: `feedback_retarget_root_motion.md`.

## Setting CDO defaults from Python

```python
bp = unreal.load_asset("/Game/AZ/Blueprints/Animation/AZ_ABP_Mover")
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property("CharacterAnimChooser", unreal.load_asset("/Game/AZ/.../CHT_AZ_CharacterAnimations"))
# For component defaults (e.g. AnimClass on Mesh), modify the SCS node's template, not the CDO directly.
```

For `Mesh.AnimClass` / `PlayerController.IMC` / similar component-template defaults: traverse `bp.simple_construction_script.get_all_nodes()`, find the SCS node by `variable_name`, modify `component_template.set_editor_property(...)`. Save with `EditorAssetLibrary.save_loaded_asset(bp)` (BPs OK; only AnimBP compile triggers GC crash).

## Saving safely

| Asset type | Safe to `save_loaded_asset` from Python? |
|---|---|
| AnimSequence | Yes |
| PoseSearchDatabase | Yes |
| ChooserTable | Yes (use `UAZ_ChooserUtils.compile_and_save`) |
| Material / MI | Yes |
| Regular Blueprint | Usually yes |
| **AnimBlueprint** | **No — let user compile** |
