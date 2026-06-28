---
name: reference_ue_native_mcp_server
description: "UE 5.8 ships a NATIVE experimental MCP server plugin (ModelContextProtocol, 'Unreal MCP') — how to enable/start/configure it, its toolset surface, and the critical gaps vs unrealclaude. Read before any 'switch MCP backend' work."
metadata: 
  node_type: memory
  type: reference
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
---

UE 5.8 ships a **native experimental MCP _server_** plugin: `ModelContextProtocol` (FriendlyName **"Unreal MCP"**, *"Anthropic MCP server implementation for Unreal Engine"*) at `C:\UnrealEngine\Engine\Plugins\Experimental\ModelContextProtocol\`. Built on the **ToolsetRegistry** + **AIAssistant** (Epic's in-editor "EDA" assistant) plugin ecosystem (see the `Engine\Plugins\Experimental\Toolsets\*` plugins: GASToolsets, AnimationAssistant, EditorToolset, etc.). Discovered 2026-06-22 when the user enabled it; relevant to [[ue_ai_plugins_comparison]].

**Direction trap:** there are TWO MCP plugins. `ModelContextProtocol` = editor **AS server** (external clients like Claude Code connect IN — what we want). `MCPClientToolset` = editor **AS client** (Epic's EDA reaches OUT to MCP servers; settings = "list of MCP servers to connect to"). They are opposite directions — only `ModelContextProtocol` is usable as a Claude Code backend. (My `*MCP*` glob first missed `ModelContextProtocol` because the name is spelled out, not abbreviated.)

## Connection
- Transport: **Streamable HTTP** (POST + GET/SSE + DELETE routes, session IDs, `initialize`/`tools/list`/`tools/call`/`resources/*`). Verified working: POST initialize → HTTP 200 + `Mcp-Session-Id` + valid JSON-RPC.
- Defaults (`UModelContextProtocolSettings`, `ModelContextProtocol.h`): `ServerPortNumber=8000`, `ServerUrlPath=/mcp`, `DefaultServerName="unreal-mcp"`, `bAutoStartServer=false`, `bEnableToolSearch=true`.
- `.mcp.json` entry (project root — `C:\UnrealEngine\Games\AZ\.mcp.json`): `"unreal-mcp": { "type": "http", "url": "http://127.0.0.1:8001/mcp" }`. We use **8001**, not 8000 — on this machine **:8000 is squatted by a process "Manager"** (curl → exit 52 empty-reply; it's NOT the MCP server). Claude Code needs a **restart + approval** to load a newly-added project MCP server.

## Starting / persisting the server
- Console cmds: `ModelContextProtocol.StartServer [port]`, `ModelContextProtocol.StopServer`, `ModelContextProtocol.GenerateClientConfig <ClaudeCode|Cursor|VSCode|Gemini|Codex|All>`, `ModelContextProtocol.RefreshTools`.
- **unrealclaude `execute_script script_type=console` does NOT run `FAutoConsoleCommand`s** (no log, no effect). Start it via Python instead: `unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), "ModelContextProtocol.StartServer 8001")`.
- **Autostart can't be set from Python:** `UModelContextProtocolSettings` is `MinimalAPI` → `unreal.ModelContextProtocolSettings` = AttributeError. Persist via **Editor Preferences → Plugins → Model Context Protocol** (Port=8001 + Auto Start Server=✓), or the EditorPerProjectUserSettings.ini, or `-ModelContextProtocolStartServer` cmdline. With default port 8000 autostart would FAIL (Manager squats it) — must set 8001.
- **GenerateClientConfig gotcha:** in a *source* build (ours) it writes to `FPaths::RootDir()` = `C:\UnrealEngine\.mcp.json` (workspace root containing Engine/), NOT the project `.mcp.json` Claude Code reads. Wire the project file by hand.

## Toolset surface (with ALL Toolset plugins enabled)
Broad & Epic-maintained, via 3 meta-tools (`list_toolsets`/`describe_toolset`/`call_tool`, tool-search mode): EditorAppToolset (CVars/PIE/viewport/selection/content-browser), ObjectTools (read/modify ANY UObject property + class discovery), AssetTools, BlueprintTools, ActorTools, SceneTools, SkeletalMeshTools, DataTable/CurveTable/DataAsset/Material/MaterialInstance/Texture/StringTable editors, full `animation_toolset` (ControlRig + Sequencer + keyframing + FBX/AnimSequence export), GASToolsets (cue/attribute/**ASC runtime inspection**), GameplayTagsToolset, BehaviorTree + StateTree inspect, SemanticSearch (vector+BM25), SlateInspector (Playwright-style UI automation), LogsToolset, Plugin/GameFeatures/Config/AutomationTest/Physics/Niagara/PCG/UMG/Dataflow/Conversation/WorldConditions/DataRegistry.

## CRITICAL GAPS vs unrealclaude → why we DON'T fully switch
1. **No general `execute_script` (python/cpp/console).** `ProgrammaticToolset` is explicitly *"sandboxed… tool orchestration, not general Python execution."* → our `AZ_*Utils` C++ bridges are unreachable.
2. **No Chooser editing** (CHT_v2 columns/rows) — v2 locomotion core.
3. **No PoseSearch DB editing.**
4. **No AnimGraph/AnimBP node wiring.** Verified 2026-06-27: `BlueprintTools` IS a full K2 **graph DSL** (`read_graph_dsl`/`write_graph_dsl`/`get_graph_dsl_docs`, `create_node`/`connect_pins`/`set_pin_value`, variables, functions, components, `compile_blueprint`, `create`+`set_parent`) — so EVENT/FUNCTION-graph authoring on a regular BP (e.g. `BP_AZ_Chalkie` pawn child + mesh/anim-class props via ObjectTools) IS scriptable. But it has ZERO anim surface (grep of the 54 tools: 0 hits for anim/state-machine/montage/blend/transition/slot). So an **AnimBP state machine / blendspace / montage slot must be hand-built in-editor** (or duplicate a working ABP via `AssetTools.duplicate` — e.g. Zombie_01 `ABP_DemoPlayable` → `AZ_ABP_Chalkie`), OR use the `AZ_*Utils` bridges IF `unrealclaude execute_script` is restored.
5. Our `AZ_BlueprintNodeUtils/AZ_AnimGraphNodeUtils/AZ_ChooserUtils/AZ_PoseSearchUtils/...` aren't registered `UToolsetDefinition`s → invisible to the native server. (Exposing them = writing toolset wrappers = a build project.)

## DECISION (2026-06-22): HYBRID — Epic native = primary, unrealclaude retained
Prefer the native Epic toolsets for general editor/asset/actor/object-property/sequencer/GAS-inspection work (richer, maintained). Keep **unrealclaude** (`:3000` node stdio bridge) for `execute_script` + chooser/PoseSearch/animgraph/`AZ_*Utils` automation. Both live in the project `.mcp.json`. Do NOT remove unrealclaude — that would break all v2 chooser/anim automation (e.g. today's bBlocked column work).
