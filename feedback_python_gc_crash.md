---
name: feedback_python_gc_crash
description: "Python-from-MCP hazards that CRASH or HANG the editor: never ReconstructNode/MarkStructurallyModified/CompileBlueprint (GC crash); never save_loaded_asset on a modified AnimBP; and never save_loaded_asset on an AnimSequence that a PoseSearch database indexes (hard deadlock, cost one editor 2026-09-07)."
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 3dd30bd7-e1c4-47aa-8af7-41a2784f5a5a
  modified: 2026-09-07T23:43:14.345Z
---

Never call `ReconstructNode()`, `MarkBlueprintAsStructurallyModified()`, or `CompileBlueprint()` from Python scripts that modify blueprint nodes. These trigger UE garbage collection which conflicts with the Python plugin's GC (`PyUtil::CollectGarbage`), causing a crash.

**Why:** The Python plugin hooks into UE's pre-GC delegate. When blueprint compilation triggers GC, Python tries to collect its tracked UObjects simultaneously, causing a crash in `PyUtil::CollectGarbage` -> `FPythonScriptPlugin::OnPreGarbageCollect`.

**How to apply:**
- In C++ utility functions called from Python (AZ_AnimGraphNodeUtils, AZ_BlueprintNodeUtils), use `Modify()` instead of `ReconstructNode()`
- Never call `compile_blueprint()` from Python — user should compile manually (Ctrl+F7) or close/reopen the ABP first
- Never call `EditorAssetLibrary.save_loaded_asset()` on modified AnimBPs from Python
- Add `gc.collect()` at the end of Python scripts to clean up Python refs before user compiles
- C++ scripts (`script_type: "cpp"`) don't have this issue — they can call compile safely

## ★ 2026-09-07 — saving a POSESEARCH-INDEXED AnimSequence from Python HANGS the editor

`EditorAssetLibrary.save_loaded_asset(seq)` on an `UAnimSequence` **that is a member of one or more
PoseSearch databases** deadlocked the editor hard. Cost: a full editor restart mid-session.

**Repro (exact):** wrote a reconstructed root-motion track to `Riffle_P_W2_Walk_F_Loop_IPC` via
`seq.controller.set_bone_track_keys(...)` (that part is safe and worked — displacement 0 -> 152.277 cm),
then called `save_loaded_asset(seq, only_if_is_dirty=False)`. The clip belongs to `PSD_P01_WalkExplore`
and `PSD_P01_WalkAim`; saving it kicks a synchronous PoseSearch index rebuild from inside the Python
call. The MCP call timed out and the editor never came back.

**How to tell a DEADLOCK from a slow-but-working rebuild — check before you panic or kill anything:**
- `Get-Process UnrealEditor | Select Responding, WorkingSet64` — a real DDC/index build CHURNS the
  working set and keeps writing to `Saved/Logs/AZ.log`.
- Here WS was pinned at **exactly** 14.58 GB across 90 s of polling and the log's last line was the
  instant the save was issued. Static WS + no log growth = deadlock, not work.
- The asset's file mtime stays unchanged, so **nothing is damaged on disk** — the in-memory edit is
  simply lost on restart.

**How to apply:**
1. Make all the track/curve edits from Python (safe), but **do NOT save from Python**. Let the USER save
   from the editor UI (Save All), which runs the index rebuild on the editor's own terms.
2. If a UI save also hangs, the index build is the culprit rather than the Python boundary: detach the
   clips from their PSDs, save, re-add, and let the DB reindex once.
3. Batch-editing clips that are PSD members is inherently risky — do ONE as a pilot and verify before
   touching the rest. (That is what limited this incident to a single clip.)
4. Related and already known: [[feedback_parallel_editor_edits]] item 4 — every `Modify()` on a sequence
   PreCancels an in-flight PoseSearch index build. Same subsystem, same fragility.

**Authoring root motion from Python DOES work** (this is worth keeping):
`seq.controller.open_bracket(desc)` -> `set_bone_track_keys(FName, positions[], rotations[], scales[], True)`
-> `close_bracket()`. Key count must equal `number_of_sampled_keys` (e.g. 37 for a 36-frame clip). Read the
existing per-key root transform via `AnimPoseExtensions.get_anim_pose_at_time` + `get_bone_pose(pose,'root')`
so authored height/rotation is preserved and only travel is added.

See [[feedback_python_save_only_if_dirty]] (the complementary "save silently skipped" trap),
[[reference_ue5_python_posesearch]], [[asset-modification-via-python]].
