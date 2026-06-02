---
name: feedback_bp_to_cpp_port_review_checklist
description: Strong dual-reviewer checklist for BP → C++ ports. Verify (1) USTRUCT field type fidelity from LIVE source — never from memory snapshots, (2) UFUNCTION metadata parity (ThreadSafe/Pure/Category/DisplayName), (3) UPROPERTY meta + replication, (4) function-flag verification via direct MCP query.
type: feedback
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# BP → C++ Port: Strong Reviewer Checklist

When porting BP-side assets (functions, structs, properties) to C++ with the dual-Haiku reviewer pattern, behavioral/body parity is necessary but **not sufficient**. Below is the full acceptance checklist that future reviewers MUST run.

## Why this checklist exists — past misses

**Miss #1 — UFUNCTION metadata (Phase 9d, 2026-04-25)**
Both reviewers APPROVED `SetBlendStackAnimFromChooser` and `OnUpdate_TransitionToLocomotion`, but both lacked `meta = (BlueprintThreadSafe)`. Without it, BP `OnStateEntry_*` thunks on the worker thread can't call them. The compiler won't flag this until the user wires the ABP — sessions later.

**Miss #2 — Struct field type wrong (Phase 1, 2026-04-25, REAL MISS, fixed)**
`FAZ_BlendStackInputs.Tags` and `FAZ_ChooserOutputs.Tags` were declared as `FName` based on a memory snapshot of GASP. User pointed at the UDS Default Value panel showing "0 Array element" UI. I initially reverified via `unreal_blueprint_query operation=get_nodes` and saw pin `type=name`, concluded "user is wrong, FName is correct." THAT WAS WRONG TOO. The summary `get_nodes` view truncates `TArray<FName>` down to just `name`. Only `get_node_pins` (per-node deep inspection) shows the full `TArray<FName>` type. After running `get_node_pins`, both `Tags` fields confirmed as `TArray<FName>`. Code fixed: structs changed to `TArray<FName> Tags`, and 3 dependent cpp sites (`Tags == FName(...)`, `Tags.IsNone()`, `Tags.ToString()`) rewritten to TArray semantics (`Tags.Contains(...)`, `Tags.Num() == 0`, manual join).

**Miss #3 — Falsely claimed BP items "missing" from inspect summary (Phase 9 user-side audit, 2026-04-25)**
While auditing `AZ_ABP_Mover` to diagnose A-pose, I called `unreal_blueprint_query operation=inspect include_functions=true include_graphs=true` and reported to the user that **`Blueprint Thread Safe Update Animation` override and `OnStateEntry_LocomotionLoop` were missing**. User screenshotted the editor showing BOTH present in My Blueprint panel.

Root cause: The `inspect` operation's `functions` array does NOT reliably include all function-style graphs. Specifically: parent-class virtual overrides (e.g., `BlueprintThreadSafeUpdateAnimation`) and certain late-added functions can be omitted from the summary. The inspect operation returned 15 functions but missed 2 that existed in the BP.

**How to apply (added to Gate 5 — Body/logic parity):**
> Do not conclude "BP function X is missing" from the `inspect` operation alone. Verify each suspected-missing function via:
> 1. `unreal_blueprint_query operation=get_nodes graph_name="<func name>"` — if the graph exists, it returns nodes; if it truly doesn't exist, you get an error.
> 2. OR ask the user to screenshot My Blueprint panel.
>
> The `inspect` summary is good for "what definitely exists" but unreliable for "what definitely doesn't exist" — especially for overrides of parent-class virtuals and recently-added functions. **Treat inspect's function list as a lower bound, not a complete enumeration.**

**Common root cause:** reviewers checked logic/body parity but not the *static metadata surface* (function flags, property types, USTRUCT fields, UPROPERTY metas). They also accepted memory snapshots as ground truth instead of re-querying the live source via the BP node-pin method.

## Mandatory checks for every BP → C++ port phase

The reviewer prompt MUST contain ALL of these gates. Each gate must be APPROVE/REJECT with evidence.

### Gate 1 — Live-source verification (no memory snapshots)
- For every claim about GASP types/flags/values, query the **live GASP BP via MCP** (or read the source `.uasset`/header in this conversation). Memory files are point-in-time observations and may be stale.
- If a memory file is the *only* available source, flag it as `UNVERIFIED` and request the user confirm before approving.

### Gate 2 — USTRUCT field-by-field parity
For every ported struct, list every source field as `Source.Name : Source.Type` → `C++.Name : C++.Type` and approve/reject each:
- **Type fidelity:** `FName` ≠ `FGameplayTag` ≠ `FGameplayTagContainer` ≠ `TArray<FGameplayTag>`. Single vs container vs array are NOT interchangeable.
- **Default value:** does the C++ default match BP default?
- **Order:** struct field order matters for serialization parity (less critical but flag).
- **UPROPERTY metas:** EditAnywhere/BlueprintReadWrite/Category — match the BP variable instance editability.

> **HOW to verify a UDS field type (most reliable → least):**
> 1. **Best:** Query a BP that uses the struct, find a `K2Node_SetFieldsInStruct` or `K2Node_BreakStruct` for it, then call `mcp__unrealclaude__unreal_blueprint_query operation=get_node_pins` with that node's GUID. The pin `type` field shows the FULL type including `TArray<>`/`TMap<>` wrappers.
> 2. Read engine plugin source for the consumer of the value (e.g. BlendStack node's `InertialBlendNodeTag = FName`).
> 3. Check memory snapshots — flag as UNVERIFIED until cross-checked.
>
> **CRITICAL PITFALL — `get_nodes` truncates types.** The summary view from `operation=get_nodes` displays types in shortened form: `TArray<FName>` becomes just `name`, `TArray<UAnimationAsset*>` becomes just `AnimationAsset*`. **NEVER conclude a type from `get_nodes` output alone.** Always follow up with `get_node_pins` for the deep view. (Confirmed 2026-04-25 on `S_BlendStackInputs.Tags`: get_nodes said "name", get_node_pins said "TArray<FName>" — the latter was correct.)
>
> **The UDS editor "Default Value" panel UI was actually correct** — when it shows "0 Array element + 🗑", the field IS a TArray. Trust the user when they report this UI shape.

### Gate 3 — UFUNCTION metadata parity
For every ported function:
- BP `bThreadSafe = true` → C++ `meta = (BlueprintThreadSafe)`
- BP `bPure = true` → C++ `BlueprintPure`
- BP `bCallable = true` → C++ `BlueprintCallable`
- BP function Category string → C++ `Category = "..."`
- Any DisplayName override → C++ `meta = (DisplayName="...")`
- BlueprintInternalUseOnly, AdvancedDisplay, BlueprintProtected, WorldContext, AutoCreateRefTerm — preserve all.

Query the BP function's `FunctionFlags` and `MetaData` map via MCP — do NOT eyeball the node graph.

### Gate 4 — UPROPERTY parity for ported variables
For every UPROPERTY ported from a BP variable:
- Type fidelity (same rules as Gate 2)
- Default value
- Replication: `Replicated` / `ReplicatedUsing` / `RepNotify` if BP variable is replicated
- Editability metas: EditAnywhere / EditDefaultsOnly / VisibleAnywhere / BlueprintReadWrite / BlueprintReadOnly
- Category, DisplayName, ToolTip

### Gate 5 — Body/logic parity (already strong — keep doing this)
- Branch structure matches the BP node graph
- Output writes match (which member vars are mutated, in what order)
- Side-effect calls match (which engine functions are invoked, with what args)
- Math/formula identity (especially curve/blend/interp constants)

### Gate 6 — Engine API call signatures
When the C++ body calls into engine plugin libraries (UPoseSearchLibrary, UBlendStackAnimNodeLibrary, UAnimationWarpingLibrary, UChooserFunctionLibrary, etc.):
- Verify the function still exists in the current engine version
- Verify parameter order/types haven't changed (UE5.7 vs the version GASP was built against)
- Verify out-params and return values are consumed correctly

### Gate 7 — Build + symbol export
- Project compiles via Live Coding
- For Build.cs changes (added modules), confirm full UBT rebuild succeeds
- Watch for unresolved external symbols on plugin structs whose constructor/destructor isn't `*_API`-marked (use storage+memzero workaround, see Phase 9d linker fix)

## How to apply
At the start of each phase, paste this checklist into BOTH reviewer prompts. Reviewers must explicitly call out each gate's verdict (APPROVE/REJECT/UNVERIFIED) with evidence — not a summary "looks good".

If a reviewer cannot verify a gate (e.g. MCP not connected to GASP), mark `UNVERIFIED` and surface it to the user before approving the phase. The user is the final reviewer for unverified items.

The defensive default `IsAnimationAlmostComplete` got right (BlueprintPure + BlueprintThreadSafe) is the standard, not a happy accident — make it the explicit rule.
