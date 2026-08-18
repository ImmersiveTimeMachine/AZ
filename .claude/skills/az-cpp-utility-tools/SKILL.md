---
name: az-cpp-utility-tools
description: Reference for the AZ C++ scripting utilities — AZ_BlueprintNodeUtils, AZ_AnimGraphNodeUtils, AZ_AnimBlueprintUtils, AZ_ChooserUtils, AZ_PoseSearchUtils, AZ_SkeletonUtils. Use when programmatically modifying Blueprint/AnimGraph nodes from Python (via mcp execute_script) or C++. Lists all UFUNCTION signatures, pin-name cheat sheets, transition rule node helpers, BlendStack inner graph helpers, and the FixFunctionForAnimBinding rule (any function called from a thread-safe function must also be marked thread-safe).
---

# AZ C++ Utility Tools — Programmatic BP / AnimBP / Chooser / PoseSearch Authoring

Six `UBlueprintFunctionLibrary` classes under `C:\UnrealEngine\Games\AZ\Source\AZ\Public\Animation\`. All UFUNCTIONs are callable from Python via `unreal.AZ_<Name>.<snake_case_func>(...)`. Live Coding **cannot add new UFUNCTIONs** — adding a new method requires a full editor restart + CLI rebuild.

## Per-utility one-line table (full API tables → memory)

| Utility | Header | Purpose | Full API ref |
|---|---|---|---|
| `UAZ_BlueprintNodeUtils` | `AZ_BlueprintNodeUtils.h` | K2 nodes (CallFunc, VarGet/Set, Branch, Sequence, Cast, BreakStruct, SetFieldsInStruct, Reroute, AnimNodeRef, Return), local vars, pin connection, function-pin sync, **`ListFunctionNodes` (READ a graph — see below)**, `FixFunctionForAnimBinding`, `CompileBlueprint` | `reference_bp_node_tools.md` |

## ★ READING an existing BP/AnimBP graph — `ListFunctionNodes`

**Use this, not the rider clipboard export.** `unreal.AZ_BlueprintNodeUtils.list_function_nodes(BpPath, FunctionName)`
returns one line per node plus one per *meaningful* pin, with **defaults and wiring**:

```
[AB66F8B4] CallFunction | Pose Search Generate Trajectory (for Character)
    in  execute:exec <-A6515C1F.then
    in  InTrajectoryData:struct <-2720F511.ReturnValue
    in  InHistorySamplingInterval:real = -1.000000
    in  InTrajectoryHistoryCount:int = 30
```

GUIDs are truncated to 8 chars; `<-` / `->` give the linked node + pin, so exec order and dataflow are
both recoverable. Pins with no link AND no default are dropped (they are the majority and say nothing).
Filter `EdGraphNode_Comment` lines out when you want structure only — but READ them once, Epic documents
known issues in graph comments.

**Why not the alternatives** (all tried 2026-08-17, all dead ends):
- `mcp__rider__ue_export_blueprint_nodes` — real data but ~2KB of pin boilerplate per node; truncates at
  ~5 nodes, so a 43-node function is 10 round trips.
- Python `node.get_editor_property('function_reference' / 'node_pos_x')` — **protected, unreadable**.
  `UEdGraph.Nodes` is protected too. `UBlueprint.function_graphs` is not exposed at all.
  You CAN reach a graph with `unreal.load_object(None, '<bp>.<bp>:<FunctionName>')` and a node with
  `...:<FunctionName>.<K2NodeClass>_<n>` — but only to prove it exists.
- UnrealClaude MCP `unreal_blueprint_query get_nodes` — would work, but the MCP client frequently
  registers only `unreal_status` and never refreshes the rest (its description even reads
  "NOT CONNECTED" while the server reports connected). Do not count on it.

**Validate the reader against a screenshot before trusting a port.** Structure alone is not enough —
the literals are the port. Reading GASP's trajectory node without defaults hid `-1.0 / 30 / 0.1 / 15`.
| `UAZ_AnimGraphNodeUtils` | `AZ_AnimGraphNodeUtils.h` | AnimGraph nodes (BlendStack, TwoWayBlend, Inertialization, OffsetRootBone, PoseHistory), `SetAnimNodeTag`, `SetPinBinding`, PoseLink wiring, **BlendStack BoundGraph** (internal-graph node creation, function calls, AnimNodeRef, pin-by-name connection, property reflection inspect/set) | `reference_animgraph_node_tools.md` |
| `UAZ_AnimBlueprintUtils` | `AZ_AnimBlueprintUtils.h` | SM transition CRUD (`CreateTransition`, `SetEntryState`, `ListTransitions`, `InspectTransitionRule`, `InspectTransitionProperties`, `SetTransitionProperty`); transition rule graph nodes (FunctionCall, VariableGet, AnimGetter, BreakStruct, EnumEquality, ArrayContains, GenericNode, RecreateRuleGraph, ClearRule) | `reference_animgraph_node_tools.md` § "AZ_AnimBlueprintUtils" |
| `UAZ_ChooserUtils` | `AZ_ChooserUtils.h` | Chooser table config, columns (Enum, MultiEnum, FloatRange, Bool, Randomize, OutputStruct), nested sub-choosers, asset/enum/property remap, `AutoRemapChooserAssets` (Jaccard fuzzy), `DumpChooserFullTree`, `DecodeEnum` | header + `reference_cht_chooser_structure.md` |
| `UAZ_PoseSearchUtils` | `AZ_PoseSearchUtils.h` | Add/remove anims to PoseSearch DB (engine doesn't expose); five notify helpers (`AddBlockTransitionNotify`, `AddBranchInNotify`, `AddExcludeFromDatabaseNotify`, `AddModifyCostNotify`, `AddOverrideContinuingPoseCostBiasNotify`) + bulk `AddBlockTransitionToDatabase` + `RemoveAllPoseSearchNotifies` | `reference_ue5_python_anim_notifies.md`, `reference_ue5_python_posesearch.md`, `reference_gasp_anim_notifies.md` |
| `UAZ_SkeletonUtils` | `AZ_SkeletonUtils.h` | Blend profile create/get/set per-bone weight, copy single profile or copy-all between skeletons, list bones | header |

## Pin-name cheat sheet (most-used)

| Node | Pin | Direction |
|---|---|---|
| FunctionEntry | `then` | Out (exec) |
| FunctionEntry | `<ParamName>` | Out (data flows out of entry) |
| FunctionResult | `execute` | In (exec) |
| FunctionResult | `ReturnValue` / `<OutParam>` | In |
| K2Node_CallFunction (impure) | `execute` / `then` / `self` (hidden) / `<Param>` / `ReturnValue` | mixed |
| Branch | `execute`, `Condition`, `then`, `else` | — |
| Cast (impure) | `execute`, `then`, `CastFailed`, `Object`, `As<Type>` | — |
| Sequence | `execute`, `then_0`, `then_1`, … | — |
| AnimNodeReference | `Value` (FAnimNodeReference) | Out |
| AnimGraph BlendStackInput | `Pose` (out) / `PlayRate` (in) | — |
| LocalToComponent | `LocalPose` in / `ComponentPose` out | — |
| ComponentToLocal | `ComponentPose` in / `Pose` out | — |
| Steering / OrientationWarping | `Pose` out / `ComponentPose`, `Alpha`, … in | — |

## Key rules — read before authoring nodes

1. **Set type-specific config BEFORE `AllocateDefaultPins`** (TargetType for Cast, StructType for Set/BreakStruct, FunctionReference for CallFunction).
2. **Connect via `Schema->TryCreateConnection`** — handles type conversion / direction fallback. The utilities do this for you.
3. **FunctionEntry param pins are `EGPD_Output`; FunctionResult return pins are `EGPD_Input`.** Direction inversion vs intuition.
4. **`EnsureFunctionPins` after MCP `add_function`.** MCP creates the function graph but Entry/Result pins are bare; format `["Name:Type", ...]`.
5. **`AddLocalVariable` BEFORE `AddLocalVariableGet/SetNode`.** Get/Set looks up by name.
6. **`AnimNodeReference` requires matching `Tag`** on the target AnimGraph node — see `SetAnimNodeTag`. Unwired reference = silent fail-closed = SM stuck (skill `anim-debug-pitfalls`).
7. **`FixFunctionForAnimBinding` after creating any function used in SM `OnStateEntry/Update/Exit` dropdowns** — sets `bThreadSafe`, `bIsEditable`, FUNC flags. MCP `add_function` creates with `bIsUserCreated=false`, missing flags.
8. **Thread-safe contagion** — any function CALLED FROM a thread-safe function must ALSO be `bThreadSafe`. Apply `FixFunctionForAnimBinding` to the entire call chain (e.g. `SetBlendStackAnimFromChooser` and every helper it invokes).
9. **SM binding params MUST be pass-by-reference.** `AnimUpdateContext` and `AnimNodeReference` are `const&` in the prototype. Use `"TypeName&"` suffix in `EnsureFunctionPins`, or rely on the auto-ref for known anim types. If SM dropdown doesn't show your function, compare `bIsReference` with a working one (`feedback_deep_pin_inspection.md`).
10. **Live Coding cannot add new UFUNCTIONs.** Adding a method to any of these utilities requires CLI build + editor restart for Python bindings to refresh. (See skill `cpp-build-livecoding`.)
11. **Enum pin defaults use enum value names** (e.g. `"TransitionToIdle"`, not `2`). If programmatic set fails, set manually in editor.

## Transition rule node patterns (gotchas)

These are tested and work; deviating crashes:

- **`AnimGetter`** (e.g. "Current State Time") → `AddTransitionRuleAnimGetter`. Sets `SourceNode` (SM), `GetterClass`, `SourceAnimBlueprint`, `Contexts.Add("Transition")` before `AllocateDefaultPins`, and `CachedTitle` via `FText::Format`.
- **Array Contains** → `AddTransitionRuleArrayContains` (NOT a generic `CallFunction`). It creates `K2Node_CallArrayFunction`; after connecting `TargetArray`, `NotifyPinConnectionListChanged` propagates the FName array type. `ConnectTransitionRulePins` auto-fires it on both ends.
- **Comparisons `>` / `<`** → use `Greater_DoubleDouble` / `Less_DoubleDouble` (concrete functions). **Do NOT** use `K2Node_PromotableOperator` or `K2Node_CommutativeAssociativeBinaryOperator` — they crash or create wildcard pins (need editor-only init we don't provide).
- **Boolean AND / OR** → use `BooleanAND` / `BooleanOR` function calls. For 3+ inputs chain or use multiple OR nodes.
- **Shared transition rules** — when `bSharedRules=true`, `BoundGraph` may be null on non-owner transitions. `RecreateTransitionRuleGraph` calls `UnshareRules()` for a private editable copy; the duplicated graph may contain old broken nodes — clear and rebuild.

Full discussion: `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\feedback_transition_rule_tools.md`.

## BlendStack inner-graph chain (wire-style, GASP parity)

GASP's BlendStack BoundGraph chain (per-anim post-processing):

`Input → LocalToComponent → CopyBone(x2) → Steering → Steering(TIP) → StrideWarping → OrientationWarping → ComponentToLocal → Output`

AZ simplified chain (current):

`Input → LocalToComponent → Steering → StrideWarping → OrientationWarping → ComponentToLocal → Output`

GASP wires K2Node_CallFunction + K2Node_AnimNodeReference into pose-processor data pins (vs the dropdown "pin binding" alternative). For GASP-style wires use `AddBlendStackGraphFunctionCall` + `AddBlendStackGraphAnimNodeRef` + `ConnectBlendStackGraphPins` (the latter takes pin names so you can wire data, not just pose). For dropdown bindings use `SetBlendStackGraphPinBinding`.

## Python recipe template

```python
# @Description: Build Get_DynamicPlayRate function in AZ_ABP_Mover
import unreal

bp = "/Game/AZ/Blueprints/Animation/AZ_ABP_Mover"
fn = "Get_DynamicPlayRate"

# 1) Add function via MCP unreal_ue domain=blueprint op=add_function (or by hand)
# 2) Sync pins
unreal.AZ_BlueprintNodeUtils.ensure_function_pins(
    bp, fn,
    ["BlendStackInput:AnimNodeReference"],   # auto-refs
    ["ReturnValue:float"]
)

# 3) Build node graph
ref_guid = unreal.AZ_BlueprintNodeUtils.add_anim_node_reference_node(bp, fn, "BlendStackTag", 0, 0)
# ... add nodes, ConnectNodes, SetPinDefaultValue ...

# 4) Make thread-safe (REQUIRED if used in SM bindings or called from thread-safe fn)
unreal.AZ_BlueprintNodeUtils.fix_function_for_anim_binding(bp, fn)

# 5) DO NOT call compile_blueprint() from Python — GC crash. User compiles in editor.
import gc; gc.collect()
```

**Hard rule:** never call `compile_blueprint()`, `ReconstructNode()`, or `EditorAssetLibrary.save_loaded_asset()` on AnimBPs from Python — Python's plugin GC collides with UE GC and crashes. Use `Modify()` in C++; let the user compile (Ctrl+F7). Full reasoning: `feedback_python_gc_crash.md`. (For `script_type=cpp` via `execute_script`, compile is safe.)

## Proven outputs (sanity check known-good)

`SetBlendStackAnimFromChooser` (53 nodes, 100% GASP match), `Get_DynamicPlayRate` (42), `IsAnimationAlmostComplete` (9). If your built function differs in node count by ±2 from these, you have a wiring miss.
