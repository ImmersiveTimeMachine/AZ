---
name: reference_animgraph_node_tools
description: AZ_AnimGraphNodeUtils capabilities — AnimGraph + BlendStack internal graph node creation, connections, bindings
type: reference
---

# AZ_AnimGraphNodeUtils — AnimGraph & BlendStack Tools

**File:** `C:\UnrealEngine\Games\AZ\Source\AZ\Public\Animation\AZ_AnimGraphNodeUtils.h`

## AnimGraph Node Creation
| Function | Creates | Notes |
|---|---|---|
| `AddBlendStackNode(bp, x, y)` | AnimGraphNode_BlendStack | Returns GUID |
| `AddTwoWayBlendNode(bp, alpha, x, y)` | AnimGraphNode_TwoWayBlend | Alpha + AlphaInputType |
| `AddInertializationNode(bp, x, y)` | AnimGraphNode_Inertialization | |
| `AddOffsetRootBoneNode(bp, x, y)` | AnimGraphNode_OffsetRootBone | EvaluationMode=Graph, Halflife=0.2 |
| `AddPoseHistoryNode(bp, x, y)` | AnimGraphNode_PoseSearchHistoryCollector | |

## AnimGraph Configuration
| Function | Purpose |
|---|---|
| `SetAnimNodeTag(bp, nodeGUID, tag)` | Set Tag on any AnimGraph node |
| `SetPinBinding(bp, nodeGUID, pin, path, bIsFunc)` | Property binding via reflection on UAnimGraphNodeBinding |
| `ConnectPoseLink(bp, srcGUID, tgtGUID, tgtPin)` | Wire PoseLink between AnimGraph nodes |
| `ListAnimGraphNodes(bp)` | List all nodes with GUIDs |

## BlendStack Internal Graph (BoundGraph)
| Function | Purpose |
|---|---|
| `ListBlendStackGraphNodes(bp, bsGUID)` | List all nodes inside BlendStack's BoundGraph |
| `AddBlendStackGraphNode(bp, bsGUID, className, x, y)` | Add AnimGraph node (Steering, StrideWarping, etc.) |
| `ConnectBlendStackGraphNodes(bp, bsGUID, srcGUID, tgtGUID, tgtPin)` | Connect PoseLink (auto-finds Pose/ComponentPose output) |
| `SetBlendStackGraphPinBinding(bp, bsGUID, nodeGUID, pin, path, bIsFunc)` | Pin binding (dropdown style) |
| `AddBlendStackGraphFunctionCall(bp, bsGUID, class, funcName, x, y)` | K2Node_CallFunction inside BS graph |
| `AddBlendStackGraphAnimNodeRef(bp, bsGUID, x, y)` | K2Node_AnimNodeReference inside BS graph |
| `ConnectBlendStackGraphPins(bp, bsGUID, srcGUID, srcPin, tgtGUID, tgtPin)` | Connect ANY pins by name (data or pose) |

## BoundGraph Access
- BlendStack stores internal graph in `BoundGraph` UPROPERTY (TObjectPtr<UEdGraph>)
- Located on `UAnimGraphNode_BlendStack_Base` class
- Accessed via reflection (FObjectProperty) or SubGraphs fallback
- Type: `UAnimationBlendStackGraph` (extends UAnimationGraph)

## Key Pin Names
| Node | Output Pin | Input Pin |
|---|---|---|
| LocalToComponent | ComponentPose | LocalPose |
| ComponentToLocal | Pose | ComponentPose |
| Steering | Pose | ComponentPose, Alpha, TargetOrientation, ProceduralTargetTime, CurrentAnimAsset, CurrentAnimAssetTime |
| StrideWarping | Pose | ComponentPose, Alpha, StrideDirection, StrideScale |
| OrientationWarping | Pose | ComponentPose, Alpha, TargetTime, OrientationAngle, CurrentAnimAsset, CurrentAnimAssetTime |
| BlendStackInput | Pose | PlayRate |
| AnimNodeReference | Value | — |

## AZ_AnimBlueprintUtils — SM Transition Tools

### Transition Query
| Function | Purpose |
|---|---|
| `GetStatesInStateMachine(bp, stateName)` | List all states |
| `ListTransitions(bp, stateName)` | List all transitions with rule info |
| `InspectTransitionRule(bp, stateName, from, to)` | Dump rule graph nodes |
| `ListTransitionRuleNodes(bp, stateName, from, to, idx)` | List nodes in rule graph |
| `InspectTransitionProperties(bp, stateName, from, to, idx)` | Dump transition settings |

### Transition Modification
| Function | Purpose |
|---|---|
| `CreateTransition(bp, stateName, from, to)` | Create new transition |
| `SetTransitionProperty(bp, stateName, from, to, prop, val, idx)` | Set Priority, Duration, etc. |
| `RecreateTransitionRuleGraph(bp, stateName, from, to, idx)` | Unshare + recreate BoundGraph |
| `ClearTransitionRule(bp, stateName, from, to, idx)` | Delete all nodes except Result |

### Transition Rule Graph Nodes
| Function | Purpose |
|---|---|
| `AddTransitionRuleFunctionCall(bp, sid, from, to, class, func, x, y, idx)` | K2Node_CallFunction |
| `AddTransitionRuleVariableGet(bp, sid, from, to, var, x, y, idx)` | K2Node_VariableGet |
| `AddTransitionRuleAnimGetter(bp, sid, from, to, getter, x, y, idx)` | K2Node_AnimGetter (with SM ref) |
| `AddTransitionRuleBreakStruct(bp, sid, from, to, struct, x, y, idx)` | K2Node_BreakStruct |
| `AddTransitionRuleEnumEquality(bp, sid, from, to, x, y, idx)` | K2Node_EnumEquality |
| `AddTransitionRuleGenericNode(bp, sid, from, to, className, x, y, idx)` | Any K2Node by class name |
| `DeleteTransitionRuleNode(bp, sid, from, to, guid, idx)` | Delete by GUID |
| `ConnectTransitionRulePins(bp, sid, from, to, srcGUID, srcPin, tgtGUID, tgtPin, idx)` | Wire pins |
| `SetTransitionRulePinDefault(bp, sid, from, to, guid, pin, val, idx)` | Set pin default |

### Shared Transition Rules
- Shared rules use `bSharedRules=true` + `SharedRulesGuid` on UAnimStateTransitionNode
- When shared, `BoundGraph` may be null on non-owner transitions
- `GetTransitionRuleGraph` follows shared references to find owner's graph
- `RecreateTransitionRuleGraph` calls `UnshareRules()` to create private copy (includes broken nodes from shared source)
- After unsharing, can delete/fix broken nodes in the private graph

## GASP BlendStack Internal Chain
Input → LocalToComponent → CopyBone(x2) → Steering → Steering(TIP) → StrideWarping → OrientationWarping → ComponentToLocal → Output

## AZ Simplified Chain  
Input → LocalToComponent → Steering → StrideWarping → OrientationWarping → ComponentToLocal → Output

## Wire vs Binding Approach
- **GASP uses wires**: K2Node_CallFunction + K2Node_AnimNodeReference wired to pins
- **Pin bindings**: Dropdown selectors (functional but visually different)
- Use `AddBlendStackGraphFunctionCall` + `ConnectBlendStackGraphPins` for GASP-style wires
