---
name: reference_bp_node_tools
description: AZ_BlueprintNodeUtils capabilities — full BP function builder with all node types, pin management, local vars
type: reference
---

# AZ_BlueprintNodeUtils — BP Function Builder

**File:** `C:\UnrealEngine\Games\AZ\Source\AZ\Public\Animation\AZ_BlueprintNodeUtils.h`

## Capabilities

### Node Creation
| Function | Creates | Notes |
|---|---|---|
| `AddFunctionCallNode(bp, fn, TargetClass, UFunctionName, x, y)` | K2Node_CallFunction | Pure or impure auto-detected |
| `AddVariableGetNode(bp, fn, VarName, x, y)` | K2Node_VariableGet | Class member variable |
| `AddVariableSetNode(bp, fn, VarName, x, y)` | K2Node_VariableSet | Class member variable |
| `AddBranchNode(bp, fn, x, y)` | K2Node_IfThenElse | Pins: execute, Condition, then, else |
| `AddSequenceNode(bp, fn, NumOutputs, x, y)` | K2Node_ExecutionSequence | Pins: execute, then_0..then_N |
| `AddReturnNode(bp, fn, x, y)` | K2Node_FunctionResult | PostPlacedNewNode syncs ReturnValue pin |
| `AddCastNode(bp, fn, TargetClass, bPure, x, y)` | K2Node_DynamicCast | Pure: no exec. Pin: "As" prefix |
| `AddSetFieldsInStructNode(bp, fn, StructType, x, y)` | K2Node_SetFieldsInStruct | Pins: StructRef, StructOut + fields |
| `AddBreakStructNode(bp, fn, StructType, x, y)` | K2Node_BreakStruct | Pure. Input = struct, outputs = fields |
| `AddRerouteNode(bp, fn, x, y)` | K2Node_Knot | Wildcard type, resolves on connect |
| `AddAnimNodeReferenceNode(bp, fn, Tag, x, y)` | K2Node_AnimNodeReference | Tag must match anim graph node tag |

### Local Variables
| Function | Purpose |
|---|---|
| `AddLocalVariable(bp, fn, VarName, VarType)` | Create function-local var. Types: float, bool, int, AnimSequence, etc. |
| `AddLocalVariableGetNode(bp, fn, VarName, x, y)` | Get node for local var (SetLocalMember) |
| `AddLocalVariableSetNode(bp, fn, VarName, x, y)` | Set node for local var |

### Function Pin Management
| Function | Purpose |
|---|---|
| `EnsureFunctionPins(bp, fn, InputPins, OutputPins)` | Fix Entry/Result pins after MCP add_function. Format: ["PinName:Type"] |

### Connections & Config
| Function | Purpose |
|---|---|
| `ConnectNodes(bp, fn, SrcGUID, SrcPin, TgtGUID, TgtPin)` | Schema->TryCreateConnection with direction fallback |
| `SetPinDefaultValue(bp, fn, NodeGUID, PinName, Value)` | Set literal default on a pin |
| `CompileBlueprint(bp)` | MarkBlueprintAsStructurallyModified + Compile |
| `ListFunctionNodes(bp, fn)` | Debug: list all nodes with GUIDs and pins |

## Pin Name Cheat Sheet
| Node | Pin | Direction |
|---|---|---|
| FunctionEntry | "then" | Output (exec) |
| FunctionEntry | ParamName | Output (data flows out) |
| FunctionResult | "execute" | Input (exec) |
| FunctionResult | "ReturnValue" | Input (data flows in) |
| CallFunction (impure) | "execute" / "then" | In / Out |
| CallFunction | "self" | Input (hidden) |
| CallFunction | ParamName / "ReturnValue" | In / Out |
| Branch | "execute", "Condition", "then", "else" | — |
| Cast (impure) | "execute", "then", "CastFailed", "Object", "As..." | — |
| Sequence | "execute", "then_0", "then_1", ... | — |
| AnimNodeReference | "Value" | Output (FAnimNodeReference) |

## Key Rules
1. Set type-specific config BEFORE AllocateDefaultPins (TargetType, StructType, FunctionReference)
2. Use Schema->TryCreateConnection for editor-time connections (handles type conversion)
3. FunctionEntry parameter pins are EGPD_Output
4. FunctionResult return pins are EGPD_Input
5. Call EnsureFunctionPins after MCP add_function to create parameter/return pins
6. AddLocalVariable must be called BEFORE AddLocalVariableGet/SetNode
7. AnimNodeReference requires matching Tag on the target anim graph node
8. Live Coding can't add new UFUNCTIONs — need full rebuild for new Python bindings
9. **SM state binding requires FixFunctionForAnimBinding** — sets bThreadSafe + bIsEditable + FUNC flags
10. **Any function called FROM a thread-safe function must ALSO be thread-safe** — apply FixFunctionForAnimBinding to all functions in the call chain (e.g., SetBlendStackAnimFromChooser)
11. MCP add_function creates with bIsUserCreated=false — missing function flags. Always call FixFunctionForAnimBinding after
12. Enum pin defaults use enum value names (e.g., "TransitionToIdle"), not integers — set manually in editor if programmatic set fails
13. **SM binding params MUST be pass-by-reference** — AnimUpdateContext and AnimNodeReference are const& in the prototype. Use "TypeName&" suffix in EnsureFunctionPins or types like AnimUpdateContext/AnimNodeReference auto-detect as reference
14. ParsePinType supports "&" suffix for explicit reference (e.g., "Vector&") and auto-refs known anim types

## Proven Functions Built
- **SetBlendStackAnimFromChooser** — 53 nodes, 100% GASP match (built by user manually)
- **Get_DynamicPlayRate** — 42 nodes, 100% GASP match (built programmatically)
- **IsAnimationAlmostComplete** — 9 nodes, 100% GASP match (built programmatically)

## Other C++ Utility Tools
- **AZ_SkeletonUtils** — blend profile create/copy/query
- **AZ_AnimBlueprintUtils** — SM state/transition creation
- **AZ_ChooserUtils** — Chooser table config, nested sub-tables, database population
- **AZ_PoseSearchUtils** — PoseSearch database animation add/remove, notifies
