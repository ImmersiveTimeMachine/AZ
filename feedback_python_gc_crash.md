---
name: feedback_python_gc_crash
description: Never call ReconstructNode/MarkStructurallyModified/CompileBlueprint from Python scripts — triggers GC crash
type: feedback
---

Never call `ReconstructNode()`, `MarkBlueprintAsStructurallyModified()`, or `CompileBlueprint()` from Python scripts that modify blueprint nodes. These trigger UE garbage collection which conflicts with the Python plugin's GC (`PyUtil::CollectGarbage`), causing a crash.

**Why:** The Python plugin hooks into UE's pre-GC delegate. When blueprint compilation triggers GC, Python tries to collect its tracked UObjects simultaneously, causing a crash in `PyUtil::CollectGarbage` -> `FPythonScriptPlugin::OnPreGarbageCollect`.

**How to apply:**
- In C++ utility functions called from Python (AZ_AnimGraphNodeUtils, AZ_BlueprintNodeUtils), use `Modify()` instead of `ReconstructNode()`
- Never call `compile_blueprint()` from Python — user should compile manually (Ctrl+F7) or close/reopen the ABP first
- Never call `EditorAssetLibrary.save_loaded_asset()` on modified AnimBPs from Python
- Add `gc.collect()` at the end of Python scripts to clean up Python refs before user compiles
- C++ scripts (`script_type: "cpp"`) don't have this issue — they can call compile safely
