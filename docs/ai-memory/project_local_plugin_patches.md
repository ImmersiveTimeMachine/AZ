---
name: project_local_plugin_patches
description: Local patches kept on top of the UnrealClaude plugin (Natfii/UnrealClaude master) and the AZ source. Re-apply after any plugin sync. Tracks 4 functional patches (#1, #1d, #1c, #1b), 6-site UE5.8 compatibility shim (#4) for FSharedString JSON keys + FIterator dereference, and 1 engine patch (#3) for USmoothWalkingMode MinimalAPI.
type: project
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# Local Patches — Reapply After Upstream Sync

This file tracks divergences from upstream (engine plugins or pristine GASP behavior) that we deliberately maintain. If the plugin is re-synced or memory is wiped, these are the deltas to restore.

## Sync log

| Date | From → To | Notes |
|---|---|---|
| 2026-05-01 | upstream master → local | Initial patches #1, #1d, #1c, #1b applied |
| 2026-05-10 | (UE 5.7 → 5.8 engine migration) | First wave of plugin source needed FSharedString shims — applied ad-hoc, not documented at the time |
| **2026-05-25** | **v1.4.4 → v1.5.0** | All 4 patches re-applied + EngineVersion bumped to 5.8.0 in `.uplugin` + 6-site UE 5.8 compatibility shim formalised as patch #4 below |

## 1. UnrealClaude plugin — bindings exposed in `get_node_pins`

**Why:** Pure pin connections don't show AnimGraph **property bindings** (the "promoted" pin → variable binding done in the editor's pin Bind dropdown). Without this patch, MCP can't see that e.g. BlendSpace `X/Y` are bound to `AO.X/AO.Y`. Critical for inspecting AnimGraph layers (AimOffset, BlendStack, BlendListByBool gating).

**File:** `C:\UnrealEngine\Engine\Plugins\Marketplace\UnrealClaude\UnrealClaude\Source\UnrealClaude\Private\MCP\Tools\MCPTool_BlueprintQuery.cpp`

**Includes added:**
```cpp
#include "AnimGraphNode_Base.h"  // FAnimGraphNodePropertyBinding (binding readout for AnimGraph nodes)
#include "UObject/UnrealType.h"
```

**Inside `ExecuteGetNodePins`, before the pin-loop:**
```cpp
// For AnimGraph nodes, build a name→binding map for pin lookup. Bindings live on
// UAnimGraphNode_Base::Binding (a UAnimGraphNodeBinding UObject) inside its
// "PropertyBindings" TMap<FName, FAnimGraphNodePropertyBinding>. The concrete
// class is in Private/, so we read the map via reflection.
TMap<FName, FAnimGraphNodePropertyBinding> AnimPinBindings;
if (UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(FoundNode))
{
    FObjectProperty* BindingObjProp = FindFProperty<FObjectProperty>(AnimNode->GetClass(), TEXT("Binding"));
    UObject* BindingObj = BindingObjProp ? BindingObjProp->GetObjectPropertyValue_InContainer(AnimNode) : nullptr;
    if (BindingObj)
    {
        FMapProperty* MapProp = FindFProperty<FMapProperty>(BindingObj->GetClass(), TEXT("PropertyBindings"));
        if (MapProp)
        {
            FScriptMapHelper MapHelper(MapProp, MapProp->ContainerPtrToValuePtr<void>(BindingObj));
            for (FScriptMapHelper::FIterator It(MapHelper); It; ++It)
            {
                const FName* Key = reinterpret_cast<const FName*>(MapHelper.GetKeyPtr(*It));
                const FAnimGraphNodePropertyBinding* Bind = reinterpret_cast<const FAnimGraphNodePropertyBinding*>(MapHelper.GetValuePtr(*It));
                if (Key && Bind)
                {
                    AnimPinBindings.Add(*Key, *Bind);
                }
            }
        }
    }
}
```

**Inside the pin-loop, after `connected_to` array is set:**
```cpp
if (const FAnimGraphNodePropertyBinding* Bind = AnimPinBindings.Find(Pin->PinName))
{
    if (Bind->bIsBound)
    {
        TSharedPtr<FJsonObject> BindObj = MakeShared<FJsonObject>();
        BindObj->SetStringField(TEXT("path"), Bind->PathAsText.ToString());
        BindObj->SetStringField(TEXT("type"), Bind->Type == EAnimGraphNodePropertyBindingType::Function ? TEXT("Function") : TEXT("Property"));
        BindObj->SetStringField(TEXT("property_name"), Bind->PropertyName.ToString());
        BindObj->SetBoolField(TEXT("is_promotion"), Bind->bIsPromotion);
        PinObj->SetObjectField(TEXT("binding"), BindObj);
    }
}
```

**Build:** `AnimGraph` is already in `PrivateDependencyModuleNames` of `UnrealClaude.Build.cs`, no change needed.

**Hot-applied with Live Coding 2026-05-01.** Verified: `get_node_pins` on a BlendSpace Player now returns `binding: { path: "AO.X", type: "Property", ... }` per pin.

---

## 1d. UnrealClaude plugin — recursive sub-graph collection in `CollectGraphs`

**Why:** Upstream `CollectGraphs` only returned `UbergraphPages + FunctionGraphs + MacroGraphs`. That misses every nested graph: `AnimGraphNode_BlendStack::BoundGraph` (per-anim template — where Steering / Orientation Warping / Stride Warping live), `AnimGraphNode_StateMachine`'s contained state graphs, and `K2Node_Composite` sub-graphs. As a result `get_nodes` / `search_nodes` over an AnimBP only saw the top-level AnimGraph and reported "Steering not found" / "OrientationWarping not found" even when those nodes existed inside the BlendStack template.

This was the root cause of a several-hour false trail diagnosing OFR Accumulate-mode lag — the diagnostic claimed AZ was missing nodes that were actually present.

**File:** `C:\UnrealEngine\Engine\Plugins\Marketplace\UnrealClaude\UnrealClaude\Source\UnrealClaude\Private\MCP\Tools\MCPTool_BlueprintQuery.cpp`

**Change:** Replace the body of `FMCPTool_BlueprintQuery::CollectGraphs` with a version that recursively expands `UEdGraph::SubGraphs` AND each node's virtual `UEdGraphNode::GetSubGraphs()`. When `GraphName` is provided, fall back through the upstream `FindGraph` first (handles function/event naming) then match against any reachable graph by `GetName()` (case-insensitive). Helper `CollectGraphsRecursive` walks the tree with a TSet<UEdGraph*> Visited cycle guard.

No new module deps — `UEdGraph::SubGraphs` and `UEdGraphNode::GetSubGraphs()` are both in `Engine` (already linked).

**Hot-applied via Live Coding 2026-05-04.** Verify by running `search_nodes` for "Steering" against `/Game/AZ/Blueprints/Animation/AZ_ABP_Mover` — should return ≥ 1 hit (the `AnimGraphNode_Steering` inside BlendStack BoundGraph).

---

## 1c. UnrealClaude plugin — `SkipScriptPermissionDialog` CVar (bypass modal)

**Why:** Without this, every `execute_script` MCP call shows a modal "Execute X Script?" Slate dialog inside the editor — kills the workflow when running many fix scripts. The CVar is set to `1` in `Config/DefaultEngine.ini` under `[ConsoleVariables]`, but the plugin patch that **reads** the CVar gets reverted on plugin sync. **Always re-apply after any plugin update.**

**Symptom of regression:** "execute_script" MCP calls hang or get rejected because the editor is showing a modal that you can't see / forgot is there.

**File:** `C:\UnrealEngine\Engine\Plugins\Marketplace\UnrealClaude\UnrealClaude\Source\UnrealClaude\Private\ScriptPermissionDialog.cpp`

**Patch:** at the top of the file, after `#include "UnrealClaudeModule.h"` add:
```cpp
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<bool> CVarSkipScriptPermissionDialog(
    TEXT("UnrealClaude.SkipScriptPermissionDialog"),
    false,
    TEXT("If true, FScriptPermissionDialog::Show returns true immediately without showing the modal dialog."),
    ECVF_Default);
```

Then at the very top of `FScriptPermissionDialog::Show(...)`, before the IsInGameThread check:
```cpp
if (CVarSkipScriptPermissionDialog.GetValueOnAnyThread())
{
    UE_LOG(LogUnrealClaude, Log, TEXT("Script permission dialog skipped (UnrealClaude.SkipScriptPermissionDialog=1)"));
    return true;
}
```

**Project-side activation** (already in `Config/DefaultEngine.ini`, do NOT remove):
```ini
[ConsoleVariables]
UnrealClaude.SkipScriptPermissionDialog=1
```

**Rebuild:** AZEditor build (or Live Coding from inside the editor) recompiles the plugin via the editor target dependency.

**Verified:** zero dialogs after rebuild; log shows `Script permission dialog skipped`.

**Re-applied:** 2026-05-03.

---

## 1b. UnrealClaude MCP bridge — expose `execute_script` in tool list

**Why:** The Node.js MCP bridge classifies `execute_script` as `"hidden"`, so it never appears in the deferred-tool list available to Claude. Without this, every Python/cpp/console script must be pasted by the user into the editor's Python console — wasteful round-trip when MCP could invoke it directly.

**File:** `C:\UnrealEngine\Engine\Plugins\Marketplace\UnrealClaude\UnrealClaude\Resources\mcp-bridge\tool-router.js`

**Edits:**
1. Add `"execute_script"` to the `SIMPLE_TOOL_NAMES` Set (the set whose members appear in `list_tools`).
2. Remove (comment out) `"execute_script"` from `HIDDEN_TOOL_NAMES`.

**Restart required:** The bridge is a long-lived Node process spawned by Claude Code per `.mcp.json`. Use `/mcp` to reconnect after editing this file (or restart Claude Code).

**Note:** `execute_script` runs with `Annotations.bDestructiveHint = true` on the Unreal side — the user is prompted before each run. The Hidden classification was a sweep-style guard, but per-call permission prompts already provide that safety. This patch keeps the prompts intact.

**Applied:** 2026-05-03.

---

## 2. ~~AZ_HeroPawn — TIP snap-to-90° + reversal abort~~ — REVERTED 2026-05-02 (Phase 6)

**Status:** Removed in the Phase 6 GASP-parity revert. `Get_OrientationIntent` now uses GASP's raw threshold-cache (`|Delta(AimingRot, ActorRot).Yaw| > 60°` → AimingFwd, else LastOrient). `Update_IdleTIPAccumulator` is narrowed to Aiming-only and only signals `bIdleTurnInProgress` for AnimInstance consumption (no snap, no reversal-abort, no LastIdleOrientationTarget). This section retained for historical reference only — do not re-apply.

---

## 2-historical. AZ_HeroPawn — TIP snap-to-90° + reversal abort (Strafe/Aiming-gated) [no longer in code]

**Why:** AZ has only 4 stand turn anims (`TurnLt90/Rt90/Lt180/Rt180`) — no 045/135 variants. GASP commits TIP to raw camera direction; for us that produces foot-slide because the chooser still picks the 90° anim. We snap target to actorYaw ± 90° (or ±180° if delta ≥ 135°) so body rotation always matches the anim foot motion exactly.

**Plus:** while TIP is in progress (target locked), we monitor for direction reversal. If the camera moves ≥ 60° on the OPPOSITE side of the actor from the locked target, we abort and re-commit in the new direction. Without this, body keeps turning the "wrong" way relative to current intent.

**Mode gate (GASP parity):** the whole accumulator early-returns unless `Get_RotationMode()` is `Strafe` or `Aiming`. In `OrientToMovement` (current default until GAS Aim wires) GASP applies no body rotation in idle — only AO clamped by BlendSpace ±90°. So this code is effectively dormant today; it activates only when GAS Aim flips RotationMode.

**File:** `Source/AZ/Private/Character/AZ_HeroPawn.cpp`, `Update_IdleTIPAccumulator()`.

**Snap on commit (replaces direct camera-direction target):**
```cpp
const float ActorYaw = GetActorRotation().Yaw;
const float SignedDelta = FRotator::NormalizeAxis(ControllerYaw - ActorYaw);
const float SnapMagnitude = (FMath::Abs(SignedDelta) >= 135.f) ? 180.f : 90.f;
const float TargetYaw = ActorYaw + FMath::Sign(SignedDelta) * SnapMagnitude;
LastIdleOrientationTarget = FRotator(0.f, TargetYaw, 0.f).Vector();
```

**Reversal abort during in-progress (replaces simple alignment release):**
```cpp
const float CamRelToActor    = FRotator::NormalizeAxis(ControllerYaw - ActorYawNow);
const float TargetRelToActor = FRotator::NormalizeAxis(LastIdleOrientationTarget.Rotation().Yaw - ActorYawNow);
const bool bDirectionFlipped = FMath::Sign(CamRelToActor) != 0.f
                            && FMath::Sign(TargetRelToActor) != 0.f
                            && FMath::Sign(CamRelToActor) != FMath::Sign(TargetRelToActor);
const bool bFarEnoughInNewDir = FMath::Abs(CamRelToActor) >= 60.f;
if (bDirectionFlipped && bFarEnoughInNewDir) { /* re-commit in new direction */ }
else { /* normal alignment release on dot ≥ 0.998 */ }
```

**Revert path:** if AZ later retargets GASP's 045/135 turn anims, drop the snap (back to raw camera-direction target) so the chooser can pick the right magnitude per FacingDelta.

## 4. UnrealClaude plugin — UE 5.8 compatibility shims (6 sites)

**Why:** Upstream UnrealClaude targets UE 5.7. Two 5.8 API breakages cascade through every place that iterates a `FJsonObject` or a `FScriptMapHelper`:

1. **`FJsonObject::Values` keys are now `UE::FSharedString`** (was `FString`). The header at `Engine/Source/Runtime/Json/Public/Dom/JsonObject.h` documents the migration. Existing code that does `Pair.Key` in `for (const auto& Pair : JsonObj->Values)` and then passes it to anything expecting `const FString&` no longer compiles — implicit conversion is gone. Fix: `FString(Pair.Key)` at every call site.

2. **`FScriptMapHelper::FIterator` no longer supports `operator*`.** Engine code in 5.8 (`InstancedReferenceSubobjectHelper.cpp:166`, `LinkerPlaceholderBase.cpp:157`, etc) now passes the iterator itself: `MapHelper.GetKeyPtr(It)` not `MapHelper.GetKeyPtr(*It)`. Same with `GetValuePtr`.

If the plugin is re-synced from upstream, every one of these sites compiles-broken again. This is also listed in [project_ue58_migration_2026-05-10.md](project_ue58_migration_2026-05-10.md) as a 5.8 surprise.

**Files + line anchors (post-v1.5.0):**

| File | Issue | Fix |
|---|---|---|
| `Source/UnrealClaude/Private/MCP/MCPToolBase.h:170-178` | `Known.Contains(Pair.Key)` + `Unknown.Add(Pair.Key)` in `GetUnknownParams` | Cache `const FString KeyStr(Pair.Key);` once at top of loop body, use `KeyStr` for both calls |
| `Source/UnrealClaude/Private/MCP/Tools/MCPTool_BlueprintModify.cpp:401` | `SetPinDefaultValue(Graph, NodeId, Pair.Key, ...)` (Create-Node path) | `FString(Pair.Key)` |
| `Source/UnrealClaude/Private/MCP/Tools/MCPTool_BlueprintModify.cpp:548` | same call (Add-Nodes path) | `FString(Pair.Key)` |
| `Source/UnrealClaude/Private/MCP/Tools/MCPTool_Material.cpp:544` | `SetScalarParameter(MatInst, Pair.Key, ...)` | `FString(Pair.Key)` |
| `Source/UnrealClaude/Private/MCP/Tools/MCPTool_Material.cpp:569` | `SetVectorParameter(MatInst, Pair.Key, ...)` | `FString(Pair.Key)` |
| `Source/UnrealClaude/Private/MCP/Tools/MCPTool_Material.cpp:587` | `SetTextureParameter(MatInst, Pair.Key, ...)` | `FString(Pair.Key)` |
| `Source/UnrealClaude/Private/MCP/Tools/MCPTool_AnimBlueprintModify.cpp:778` | `Bindings.Add(Pair.Key, Pair.Value->AsString())` | `Bindings.Add(FString(Pair.Key), ...)` |
| `Source/UnrealClaude/Private/AnimationBlueprintUtils.cpp:930` | same pattern | `Bindings.Add(FString(Pair.Key), ...)` |
| `Source/UnrealClaude/Private/MCP/Tools/MCPTool_SetProperty.cpp:379` | `Pair.Key.ToUpper()` — `UE::FSharedString` has no `ToUpper` | `FString(Pair.Key).ToUpper()` |
| `Source/UnrealClaude/Private/MCP/Tools/MCPTool_BlueprintQuery.cpp:301-302` | **inside patch #1** — `MapHelper.GetKeyPtr(*It)` / `GetValuePtr(*It)` | drop `*`: `MapHelper.GetKeyPtr(It)` / `GetValuePtr(It)` |

Each site is tagged with `// AZ local patch:` so a grep across the plugin for `AZ local patch` lists every divergence in one shot. Symptom on regression: `error C2664: cannot convert argument from 'const T' to 'const FString &'` (FSharedString) or `error C2100: you cannot dereference an operand of type 'FScriptMapHelper::FIterator'`.

**Forward path:** when upstream officially supports 5.8 (currently target is 5.7.0 per `UnrealClaude.uplugin`), this patch can be dropped — but verify upstream uses the same `FString(Pair.Key)` / `GetKeyPtr(It)` shape; if they choose `*Pair.Key` or similar, our patches will conflict.

**Applied: 2026-05-10 (ad-hoc); formalised 2026-05-25 during v1.4.4 → v1.5.0 sync.**

---

## 3. Engine — `USmoothWalkingMode` missing `MinimalAPI` (UE 5.8 only)

**Why:** UE 5.8 ships `SmoothWalkingMode.h` with `UCLASS(BlueprintType, Experimental)` — missing the `MinimalAPI` specifier that sibling `UWalkingMode` and `UFallingMode` have. Without it, the auto-generated `USmoothWalkingMode(const FObjectInitializer&)`, `USmoothWalkingMode(FVTableHelper&)`, and `~USmoothWalkingMode()` symbols aren't exported from `MOVER_API`, so any external module that derives from it (e.g. `UAZ_SmoothWalkingMode`) gets `LNK2019` unresolved externals. Looks like an Epic oversight — sibling classes have it, declaration is otherwise identical.

**File:** `C:\UnrealEngine\Engine\Plugins\Experimental\Mover\Source\Mover\Public\DefaultMovementSet\Modes\SmoothWalkingMode.h:15`

**Patch:**
```cpp
// Before
UCLASS(BlueprintType, Experimental)
class USmoothWalkingMode : public USimpleWalkingMode

// After
UCLASS(MinimalAPI, BlueprintType, Experimental)
class USmoothWalkingMode : public USimpleWalkingMode
```

**How to apply:** Re-apply if engine is reinstalled or upgraded. Symptom on regression: build fails at link with three `LNK2019` lines for `USmoothWalkingMode` constructor/destructor symbols when linking `UnrealEditor-AZ.dll`.
