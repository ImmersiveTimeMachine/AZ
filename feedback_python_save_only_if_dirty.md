---
name: feedback_python_save_only_if_dirty
description: "★ Python save trap: EditorAssetLibrary.save_loaded_asset defaults to only_if_is_dirty=True and returns True even when it SKIPPED a clean package — BlueprintCallable mutators that don't Modify() (InputMappingContext.map_key) are silently lost on editor restart. Force the save and verify the file mtime."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-03T02:15:04.481Z
---

# Python asset writes: "saved True" is not proof the file was written

**What happened (2026-09-03):** `imc.map_key(ia, key)` + `EditorAssetLibrary.save_loaded_asset(imc)` printed
`True`; after the editor restart the R -> HeavyStrike mapping was gone. `map_key` is BlueprintCallable and
does NOT mark the package dirty; `save_loaded_asset(asset, only_if_is_dirty=True)` skips clean packages and
still reports success. `set_editor_property` writes DO dirty the package, so every other asset in the same
batch survived.

**Why:** the return value means "nothing failed", not "bytes hit the disk".

**How to apply:**
- After any BlueprintCallable mutator (map_key, add/remove helpers, C++ utils that forget MarkPackageDirty)
  save with `save_loaded_asset(asset, only_if_is_dirty=False)`; `asset.modify()` is exposed,
  `mark_package_dirty` is NOT.
- Verify persistence by the file's mtime (`ls -l --time-style=full-iso Content/...uasset`) or by re-reading
  after a restart — never by the in-memory read-back in the same session.
- Same class of trap as [[feedback_bp_cdo_write_needs_recompile]] (write ≠ landed) and
  [[feedback_verify_never_presume]].
- Related Python facts (5.8): `GameplayTag.tag_name` and `Key.key_name` are read-only — build them with
  `struct.import_text('Input.Action.X')` / `import_text('R')`; struct fields marked EditDefaultsOnly refuse
  `set_editor_property` on standalone struct instances ("cannot be edited on instances") — use
  `export_text` -> string edit -> `import_text` on a fresh struct, or make the UPROPERTY EditAnywhere;
  `unreal.Rotator(...)` positional order is (roll, pitch, yaw) — pass yaw= by keyword;
  `AssetEditorSubsystem.open_editor_for_assets([a])` (plural) opens an asset editor (forces a PoseSearch
  index build).
