---
name: unrealclaude-mcp-tools
description: How to drive the UnrealClaude MCP server (port 3000) — direct vs deferred vs hidden tool catalog, when to use blueprint_query vs unreal_ue domain router vs execute_script, how to load deferred tools (ToolSearch select:...), per-tool gotchas (search_nodes can't see SM transition rule sub-graphs, inspect omits parent-class virtual overrides, get_nodes truncates types), and the standing rule "GASP content is imported into AZ at /Game/Blueprints/ — never look in C:\UnrealEngine\Games\GameAnimationSample".
---

# UnrealClaude MCP — Tool Catalog & Decision Tree

UnrealClaude MCP runs on the editor when AZ is open. Server name `unrealclaude`, port 3000, tool prefix `mcp__unrealclaude__*`. Registered in `C:\UnrealEngine\Games\AZ\.mcp.json`.

## Standing rule — GASP lives at `/Game/Blueprints/`

GASP content was **imported into the AZ project** at `/Game/Blueprints/...` (filesystem `C:\UnrealEngine\Games\AZ\Content\Blueprints\`). Always query it through this `unrealclaude` MCP. Do NOT:

- look in `C:\UnrealEngine\Games\GameAnimationSample\` (external copy, not source of truth)
- ask the user to launch a separate GASP editor
- use the deprecated `gassample` MCP at port 3001

Examples: `unreal_blueprint_query operation=get_nodes blueprint_path=/Game/Blueprints/SandboxCharacter_Mover_ABP graph_name=AnimGraph`.

## Decision tree — which tool?

| Intent | Tool |
|---|---|
| Read-only BP info (vars, funcs, graphs, nodes, pins, refs) | `unreal_blueprint_query` (operation switch) |
| Modify BP / AnimBP / Character / Material / Asset / Input | `unreal_ue` (domain router — pick `blueprint`, `anim`, `character`, `enhanced_input`, `material`, `asset`) |
| Run a Python / cpp / console / editor_utility script | `unreal_execute_script` (deferred — see §loading) |
| Search assets by class/path/name | `unreal_asset_search` |
| Forward / reverse asset deps | `unreal_asset_dependencies` / `unreal_asset_referencers` |
| Read editor log (filterable) | `unreal_get_output_log` |
| Spawn / move / delete / set_property on level actor | `unreal_spawn_actor`, `unreal_move_actor`, `unreal_delete_actors`, `unreal_set_property` |
| Open / new / save_as level | `unreal_open_level` |
| Screenshot viewport | `unreal_capture_viewport` |
| UE 5.7 API docs by category | `unreal_get_ue_context` |

The decision table above IS the per-tool catalog organized by intent. For per-tool schema details, the editor's `unreal_status` confirms connection and `mcp__unrealclaude__unreal_get_ue_context` returns engine-side API docs by category (`animation`, `blueprint`, `slate`, `actor`, `assets`, `replication`, `enhanced_input`, `character`, `material`, `parallel_workflows`, `ue_core`).

## Loading the deferred tool

`unreal_execute_script` is deferred (its schema isn't pre-loaded). Load it before first use:

```
ToolSearch query="select:mcp__unrealclaude__unreal_execute_script"
```

Once the schema appears in a `<functions>` block, call it like any other tool. Required header field: `@Description: <one line>`. `script_type` ∈ `python` / `cpp` / `console` / `editor_utility`.

For build/Live Coding workflow (which uses `script_type=console` to run `LiveCoding.Compile`) see `az-workspace` § "Build & Live Coding workflow".

## Hidden tools (callable but not in `list_tools`)

`task_submit`, `task_status`, `task_result`, `task_list`, `task_cancel`, `cleanup_scripts`, `get_script_history`, `run_console_command`. Use only if you know the schema; otherwise prefer `execute_script`.

## Per-tool gotchas

- **`unreal_blueprint_query operation=inspect`** — function list is a **lower bound**, omits parent-class virtual overrides like `BlueprintThreadSafeUpdateAnimation`. To verify, use `get_nodes graph_name=...` per-graph or load the asset from Python.
- **`unreal_blueprint_query operation=get_nodes`** — type truncation: `TArray<FName>` shows as `name`, `TSoftObjectPtr<X>` collapses to `object`. Always follow up with `get_node_pins` for full type fidelity.
- **`unreal_blueprint_query operation=search_nodes`** — **blind to AnimGraph SM transition rule sub-graphs**. The transition rule BoundGraph (the little expression graph inside an arrow between two states) is not enumerated. To inspect, use `UAZ_AnimBlueprintUtils::ListTransitions` / `InspectTransitionRule` (BP-callable from `execute_script script_type=python`), or ASK the user for SM screenshots.
- **`unreal_blueprint_query operation=get_node_pins`** — returns pin metadata flags (`bIsReference`, `bIsConst`, etc.) and AnimGraph property bindings only if the local plugin patch is applied (see below).
- **Asset Registry lag** — after creating/modifying assets via `unreal_ue domain=asset`, re-query or `EditorAssetLibrary.load_asset(...)` from Python for ground truth before downstream calls.

## SM transition rule access (state names have spaces)

States in AZ's "State Controller" SM have **spaces** in names ("Idle Loop", not "IdleLoop"). The `IdentifyingStateName` parameter on `ListTransitions` / `InspectTransitionRule` etc. must be a **state name inside the SM** ("Idle Loop"), NOT the SM node's name ("State Controller"). Some state names also have trailing spaces (`"Re-Enter "`). Passing the SM-node name returns empty results silently. Full recipe: `feedback_sm_transition_access.md`.

## Local plugin patches (re-apply after upstream sync)

The user maintains three patches on `Natfii/UnrealClaude` master. If MCP behavior regresses, suspect a missing patch:

1. **`get_node_pins` binding readout** — if AnimGraph pin output lacks a `binding: { path, type, property_name, is_promotion }` field on bound pins, the patch in `MCPTool_BlueprintQuery.cpp` is missing. Without it you cannot see e.g. that a BlendSpace's `X/Y` pin is bound to `AO.X/AO.Y`.
2. **`execute_script` exposed in tool list** — Node bridge `tool-router.js` reclassified from HIDDEN to SIMPLE. If `ToolSearch select:...execute_script` returns nothing, the bridge edit was reverted; restart Claude Code via `/mcp` after re-applying.
3. **`SkipScriptPermissionDialog` CVar** — bypass the per-script modal. Symptom of regression: `execute_script` calls hang because the editor is showing an invisible modal. Project-side activation (`UnrealClaude.SkipScriptPermissionDialog=1` in `Config/DefaultEngine.ini` `[ConsoleVariables]`) does nothing without the C++ patch reading it.

Full diffs and re-apply instructions: `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\project_local_plugin_patches.md` §1, §1b, §1c.

## When NOT to use this MCP

For programmatic AnimGraph / BP-function authoring at scale, the MCP `add_node` / `connect_pins` ops are slow and have type-fidelity gaps. Prefer the AZ C++ utilities (`UAZ_BlueprintNodeUtils`, `UAZ_AnimGraphNodeUtils`, `UAZ_AnimBlueprintUtils`, `UAZ_ChooserUtils`, `UAZ_PoseSearchUtils`, `UAZ_SkeletonUtils`) called from `execute_script script_type=python`. See skill `az-cpp-utility-tools`.

## Comparison vs other UE-AI MCPs

`unrealclaude` is strongest for BP/AnimBP/Material editing at the node level. For landscape sculpting / foliage / batch actor placement / unrestricted Python see SpecialAgent; for runtime LLM calls in gameplay see UnrealGenAISupport. Full comparison table: `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\ue_ai_plugins_comparison.md`.
