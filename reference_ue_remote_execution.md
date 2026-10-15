---
name: reference-ue-remote-execution
description: Run Python in the open AZ Unreal Editor without Rider/MCP/agent — Tools/ue_remote/ue_run.py over UE Python Remote Execution (enabled 2026-10-15)
metadata:
  node_type: memory
  type: reference
  originSessionId: 2fe7c274-bddc-45e1-ab33-fd1c3fc14bd2
  modified: 2026-10-15T19:32:38.470Z
---

`python Tools/ue_remote/ue_run.py --ping` · `... ue_run.py script.py` · `... ue_run.py -c "import unreal; ..."` — ExecuteFile mode, prints Unreal output, exit 1 on failure. Uses the engine's `Engine/Plugins/Experimental/PythonScriptPlugin/Content/Python/remote_execution.py`, picks the node whose project_root is C:/UnrealEngine/Games/AZ.

Enabled by `bRemoteExecution=True` under `[/Script/PythonScriptPlugin.PythonScriptPluginSettings]` in `Config/DefaultEngine.ini` (Artur asked to enable it "везде, где нужно", 2026-10-15). In a running editor without restart: Rider `ue_execute_python` → `unreal.get_default_object(unreal.load_class(None,'/Script/PythonScriptPlugin.PythonScriptPluginSettings')).set_editor_property('bRemoteExecution', True, unreal.PropertyAccessChangeNotifyMode.ALWAYS)` (Python name is the C++ name `bRemoteExecution`; `remote_execution` fails). Localhost only (multicast 239.0.0.1:6766 bound 127.0.0.1, TCP 6776).

Used by the Blender addon's Send to Unreal ([[project-blender-ai-utilities-audit]]); skills blockout/roundtrip/high-to-low point to it; Rider stays the fallback.
