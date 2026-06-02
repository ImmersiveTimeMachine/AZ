---
name: cpp-build-livecoding
description: AZ C++ build and Live Coding workflow. Use when editing files under Source/AZ or the local UnrealClaude plugin source, or when the user reports a C++ compile/link error or asks to rebuild. Covers CLI build vs Live Coding, where to read the real error log (UnrealBuildTool Log.txt — NOT the editor log), iteration loop, when plugin DLL changes need a full editor restart, and the "only ask user to PIE after Result: Succeeded" discipline.
---

# AZ C++ Build & Live Coding Workflow

Strict order whenever you edit C++ under `C:\UnrealEngine\Games\AZ\Source\AZ\` or the plugin at `C:\UnrealEngine\Engine\Plugins\Marketplace\UnrealClaude\`.

## Step 1 — Try CLI build first (editor may be closed)

```
"C:/UnrealEngine/Engine/Build/BatchFiles/Build.bat" AZEditor Win64 Development -Project="C:/UnrealEngine/Games/AZ/AZ.uproject" -WaitMutex -FromMsBuild
```

If output contains `Unable to build while Live Coding is active`, the editor has a Live Coding session — go to Step 2.

## Step 2 — Trigger Live Coding via MCP

Do NOT ask the user to press `Ctrl+Alt+F11`. Fire the console command yourself:

```
mcp__unrealclaude__unreal_execute_script
  script_type: console
  script_content: "LiveCoding.Compile"
```

Tool is deferred — load via `ToolSearch query="select:mcp__unrealclaude__unreal_execute_script"`. Header must include `@Description`. Compile runs async; wait ~5-10 seconds before checking results.

## Step 3 — Read the REAL compile errors from the right log

The editor's main log (`unreal_get_output_log`) only says `Live coding failed, please see Live console for more information` — **useless**. The actual errors live in:

```
C:/UnrealEngine/Engine/Programs/UnrealBuildTool/Log.txt
```

Use Bash `tail -100`. Look for `Result: Succeeded` (clean) or `Result: Failed (OtherCompilationError)` followed by `cl.exe` lines (`(line, col): error C####:`).

Secondary: `C:/UnrealEngine/Engine/Programs/LiveCodingConsole/Saved/Logs/LiveCodingConsole.log` — should end with `Patch creation for module ... successful`. The `.voltbl` warnings are non-fatal — ignore.

## Step 4 — Iterate without bothering the user

Fix → re-trigger `LiveCoding.Compile` → re-tail `Log.txt` → repeat. Don't loop in the user.

## Step 5 — Only after `Result: Succeeded`, ask the user to PIE

Stale binary == wasted user time. Never ask "can you test it?" until you've confirmed the green build line.

## Plugin DLL flakiness — when to skip Live Coding

For changes to plugin module init/shutdown lifecycle (registering CVars, UObjects, tool factories), prefer **close-editor + CLI build + reopen** over Live Coding. Live Coding can leave stale function pointers / late-destructor crashes on plugin reloads.

Signal: change touches `IModuleInterface::StartupModule/ShutdownModule`, `IConsoleManager::Get().Register*`, `FCoreDelegates::*`, or any `UObject` static/CDO mutation.

Past incident (FConsoleManager re-registration): `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\project_local_plugin_patches.md` § 1c.

## Live Coding cannot add new UFUNCTIONs

New `UFUNCTION` / `UCLASS` / `USTRUCT` field / `UPROPERTY` requires full editor restart with CLI build — Live Coding compiles fine but BP/Python won't see the new symbol. Existing function body edits are fine.

## The Python-from-MCP GC trap

Python scripts that modify Blueprint nodes must NEVER call `ReconstructNode()`, `MarkBlueprintAsStructurallyModified()`, `compile_blueprint()`, or `EditorAssetLibrary.save_loaded_asset()` on AnimBPs. These crash the editor in `PyUtil::CollectGarbage` → `FPythonScriptPlugin::OnPreGarbageCollect`.

In C++ utility helpers called from Python (`UAZ_AnimGraphNodeUtils`, `UAZ_BlueprintNodeUtils`), use `Modify()` instead of `ReconstructNode()`. Have the user compile manually (`Ctrl+F7`). C++ scripts (`script_type: cpp`) don't have this issue.

See `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\feedback_python_gc_crash.md` for the full rule and symptom.

## Local plugin patches — re-apply after upstream sync

If UnrealClaude is re-synced from `Natfii/UnrealClaude`, our local patches must be re-applied. Quick smoke test: call `mcp__unrealclaude__unreal_blueprint_query` with `operation=get_node_pins` on an AnimGraph node that has bindings. If response lacks a `bindings` field, the patch is missing.

Full patch records: `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\project_local_plugin_patches.md`.
