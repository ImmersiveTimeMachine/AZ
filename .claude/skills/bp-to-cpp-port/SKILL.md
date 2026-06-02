---
name: bp-to-cpp-port
description: Phased BP→C++ porting workflow with the dual-reviewer (Reviewer A live-source / Reviewer B memory-ledger) gate. Use when porting any GASP Blueprint asset (Pawn, AnimBP, MovementMode, ActorComponent, struct, function) into an AZ C++ class, when reviewing a port that's already underway, or when planning a phased migration. Covers the 7 mandatory acceptance gates (live-source verification, USTRUCT field type fidelity, UFUNCTION metadata, UPROPERTY parity, body/logic, engine API signatures, build/symbol export), the past-misses catalog, the get_nodes vs get_node_pins type-truncation pitfall, and the inspect-is-not-authoritative pitfall.
---

# BP → C++ Port Workflow

For porting any GASP Blueprint asset (Pawn, AnimBP, MovementMode, ActorComponent, struct, function) into an AZ C++ class.

## When to use

- Starting a new port (e.g. "port `BP_X` to C++").
- Reviewing a port that's already underway ("Phase N reviewer prompt").
- Planning a phased migration.
- Any time someone says "USTRUCT parity", "UFUNCTION metadata", "GASP port".

## Phase template (per-phase loop)

1. **Implement** the phase: write/edit C++ in `C:\UnrealEngine\Games\AZ\Source\AZ\` matching the live GASP source.
2. **Live Coding compile** via the `cpp-build-livecoding` skill — must hit `Result: Succeeded` before reviewers run.
3. **Reviewer A — live-source verification** (Haiku Task agent). Mission: query live GASP BP via `mcp__unrealclaude__unreal_blueprint_query` against `/Game/Blueprints/...` and verify each gate against ground truth. NO memory snapshots.
4. **Reviewer B — ledger / memory cross-check** (Haiku Task agent). Mission: cross-check against the relevant ledger / port plan / past-misses. Flag anything that disagrees with memory; surface UNVERIFIED items.
5. **Both APPROVE → next phase.** Any REJECT → fix → recompile → re-review (only the rejected gates).

Reviewers must call out each gate `APPROVE` / `REJECT` / `UNVERIFIED` with **evidence** — never a summary "looks good". UNVERIFIED items go to the user as final reviewer.

## The 7 mandatory gates (paste verbatim into reviewer prompts)

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
> **CRITICAL PITFALL — `get_nodes` truncates types.** The summary view from `operation=get_nodes` displays types in shortened form: `TArray<FName>` becomes just `name`, `TArray<UAnimationAsset*>` becomes just `AnimationAsset*`. **NEVER conclude a type from `get_nodes` output alone.** Always follow up with `get_node_pins` for the deep view.
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

### Gate 5 — Body/logic parity
- Branch structure matches the BP node graph
- Output writes match (which member vars are mutated, in what order)
- Side-effect calls match (which engine functions are invoked, with what args)
- Math/formula identity (especially curve/blend/interp constants)

> **Sub-rule:** Do not conclude "BP function X is missing" from the `inspect` operation alone. Verify each suspected-missing function via:
> 1. `unreal_blueprint_query operation=get_nodes graph_name="<func name>"` — if the graph exists, it returns nodes; if it truly doesn't exist, you get an error.
> 2. OR ask the user to screenshot My Blueprint panel.
>
> The `inspect` summary is good for "what definitely exists" but unreliable for "what definitely doesn't exist" — especially for parent-class virtual overrides and recently-added functions. **Treat inspect's function list as a lower bound, not a complete enumeration.**

### Gate 6 — Engine API call signatures
When the C++ body calls into engine plugin libraries (`UPoseSearchLibrary`, `UBlendStackAnimNodeLibrary`, `UAnimationWarpingLibrary`, `UChooserFunctionLibrary`, etc.):
- Verify the function still exists in the current engine version
- Verify parameter order/types haven't changed (UE5.7 vs the version GASP was built against)
- Verify out-params and return values are consumed correctly

### Gate 7 — Build + symbol export
- Project compiles via Live Coding (see `cpp-build-livecoding` skill)
- For Build.cs changes (added modules), confirm full UBT rebuild succeeds
- Watch for unresolved external symbols on plugin structs whose constructor/destructor isn't `*_API`-marked (use storage+memzero workaround — see Phase 9d linker fix in the port ledger)

## Past misses — what the gates protect against

Three real misses, all documented in detail in the source memory file. Read them when reviewing a similar port:

1. **UFUNCTION metadata miss** — both reviewers approved a function lacking `meta = (BlueprintThreadSafe)`; broke at ABP wire-up time, sessions later. Gate 3 protects.
2. **Struct field type wrong** — `Tags` declared as `FName`, actually `TArray<FName>`. `get_nodes` returned `name`, `get_node_pins` returned `TArray<FName>`. Gate 2 + the get_nodes pitfall protect.
3. **Falsely claimed BP items "missing"** — `inspect` summary omitted a parent-class virtual override and a recently-added function. Gate 5 sub-rule protects.

Full incident histories: `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\feedback_bp_to_cpp_port_review_checklist.md`.

## Pitfall: ASSET REGISTRY lag

Right after creating/modifying an asset, the asset registry may not reflect it for `unreal_asset_search` / dependency queries. If a search comes up empty for something you just created, re-query after a few seconds, or load the asset directly via `EditorAssetLibrary.load_asset('/Game/...')` for ground truth.

## References (live ledgers and port plans — do NOT duplicate inline)

- Pawn port plan + 9-phase execution: `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\gasp_pawn_cpp_port_plan.md`
- Pawn port re-audit (post-Phase-8): `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\project_gasp_pawn_port_audit_2026-05-02.md`
- AnimBP port ledger (107 vars / 63 fns): `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\project_gasp_abp_port_ledger.md`
- AnimBP port plan: `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\project_gasp_animbp_cpp_port_plan.md`
- "What stays in ABP vs C++" decision: same ledger, Phase 9 section.

When in doubt, query the live BP via `mcp__unrealclaude__unreal_blueprint_query` against `/Game/Blueprints/...` instead of trusting memory snapshots — see `agent-and-research-discipline` skill rule 1.
