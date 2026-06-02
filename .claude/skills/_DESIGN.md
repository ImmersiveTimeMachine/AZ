# AZ Skills Architecture — Design Document

> Drafted 2026-05-03. Source of truth for the next pass that actually creates skills.
> No skills implemented yet; this file is read-only spec.

## Background

- **Skills location:** `C:\UnrealEngine\Games\AZ\.claude\skills\<skill-name>\SKILL.md` (YAML frontmatter `name`, `description`).
- **Skills are LAZY-LOADED** — only enter context when invoked or when `description` matches user intent.
- **Memory location:** `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\` — `MEMORY.md` is **always** in context (~132 lines, ~3.5k tokens), other files load on demand.
- **Goal:** move procedural ("how to do X") content out of `MEMORY.md` (and one-line index entries) into skills, so the always-loaded budget shrinks. Pure reference data (struct dumps, audit catalogs, anim pools) stays in memory because it has no autoloadable trigger surface.

---

## Section 1 — Existing skills inventory

| Skill folder | Description (verbatim) | What it covers |
|---|---|---|
| `az-workspace/` | Workspace map for the AZ project — every folder I should know about when working on AZ code, content, configs, GASP reference, engine plugins, MCP servers, and persistent memory. Use this skill at the start of any non-trivial AZ task to confirm where things live before searching or guessing. | Project root layout, `Source/AZ/Public` & `Private` subdirs, content folders, the GASP-imported-into-AZ standing rule (`/Game/Blueprints/...`), engine path / plugin folders, full unrealclaude MCP tool catalog (direct + deferred + hidden), git facts, **the entire build & Live Coding workflow** (CLI build → LiveCoding.Compile via MCP → tail UBT log → smoke gotchas), common gotchas (`inspect` incompleteness, `get_nodes` type truncation, transition-rule sub-graph invisibility, stale `+PropertyRedirects`). |

Only one skill exists today. It already overlaps significantly with several proposed skills below — particularly the build/Live Coding workflow and the gotchas list. The proposal below extracts those into focused skills and trims `az-workspace` back to a pure folder map.

---

## Section 2 — Memory inventory

`X-day age` shown when stale-warning was emitted. All paths are under `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\`.

### feedback_*.md — workflow / behavior rules (mostly WORKFLOW)

| File | Lines | Class | Reasoning |
|---|---|---|---|
| `feedback_check_memory_first.md` | 18 | **WORKFLOW** | "Before research, scan MEMORY.md and read matching files." Procedural rule. Currently lives as line 4 of MEMORY.md, always loaded. |
| `feedback_bp_to_cpp_port_review_checklist.md` | 98 | **WORKFLOW** | The 7-gate dual-reviewer checklist for any BP→C++ port. Pasted verbatim into reviewer prompts. Procedural. |
| `feedback_animgraph_node_reference_wiring.md` | 31 | **WORKFLOW** | "Any helper/transition rule taking FAnimNodeReference must be wired to a K2Node_AnimNodeReference; MCP search_nodes can't see transition rule sub-graphs — ASK user." Hybrid: rule + diagnostic. |
| `feedback_animbp_post_event_vs_thread_safe.md` | 29 | **WORKFLOW** | "Driver placement rule: Post-event vs ThreadSafe — wrong placement → mesh spin." Procedural classification rule. |
| `feedback_file_paths.md` | 11 | **WORKFLOW** | "Always show full absolute paths (Rider IDE clickability)." Trivial behavior rule. |
| `feedback_no_read_approval.md` | 11 | **WORKFLOW** | "Never ask for read/glob/grep approval." Behavior rule. |
| `feedback_deep_pin_inspection.md` | 16 | **WORKFLOW** | "Compare pin metadata flags (bIsReference, bIsConst), not just names/types." Diagnostic procedure for "function should work but doesn't". |
| `feedback_python_gc_crash.md` | 16 | **WORKFLOW** | "Never call ReconstructNode/CompileBlueprint/save_loaded_asset from Python — GC crash." Hard rule for Python-via-MCP scripts. |
| `feedback_validate_agent_findings.md` | 16 | **WORKFLOW** | "Cross-validate agent claims against editor state before destructive changes." Procedural. |
| `feedback_sm_transition_access.md` | 17 | **WORKFLOW** + REFERENCE | Specific SM access pattern: state names have spaces, use state-inside-SM not SM-node-name. Half rule, half lookup. |
| `feedback_use_agent_teams.md` | 17 | **WORKFLOW** | "Always launch teams of 3-8 agents in parallel for research." Behavior rule. |
| `feedback_transition_rule_tools.md` | 34 | **WORKFLOW** + REFERENCE | API/recipes for AnimGetter, ArrayContains, comparisons, BooleanAND/OR, shared-rule unsharing — i.e. how to use `AZ_AnimBlueprintUtils` for transition rules. |
| `feedback_retarget_root_motion.md` | 18 | **WORKFLOW** | "Duplicate retargeted asset to preserve root motion (IK Retargeter bug workaround)." Recipe. |
| `feedback_ik_setup.md` | 13 | **WORKFLOW** + REFERENCE | "Two Bone IK in Bone Space relative to hand_r; Layered Blend on spine_02 not hand_l." Recipe + values. |

### project_*.md — project state / decisions / sessions (mostly STATE)

| File | Lines | Class | Reasoning |
|---|---|---|---|
| `project_gas_gameplay.md` | 11 | STATE (stub) | Almost empty — a 51-day-old "starting work session" note. Could be deleted. |
| `project_input_stack_rt_mirror.md` | 61 | STATE | Decision log: Option 3 of 3 picked for input stack RT mirror, what was built, what's orphaned, Phase 9 plan. Project state. |
| `project_session_2026-04-22_gasp_pawn_done.md` | 83 | STATE | Session achievement log. Pure history. |
| `project_session_2026-04-24_gasp_animbp_cpp_done.md` | 144 | STATE | Session achievement log including Phase 9 reversal. Pure history. |
| `project_session_2026-04-26_abp_first_motion.md` | 65 | STATE + WORKFLOW | "First motion" session log + the exact wiring recipe needed (3 chooser/MDT defaults, 9 OnStateEntry thunk shape). Hybrid: extract the 9-thunk recipe into a skill. |
| `project_idle_tip_implementation.md` | 66 | STATE + WORKFLOW | Working baseline + tuning knobs + architecture rule "don't violate". Hybrid. |
| `project_gasp_animbp_cpp_port_plan.md` | 70 | STATE (planning doc) | Day-1 plan for next session (already executed). Historical. |
| `project_gasp_abp_port_ledger.md` | 306 | REFERENCE | 107-var, 63-fn ledger comparing GASP↔AZ status row-by-row. Pure data. |
| `project_gasp_pawn_port_audit_2026-05-02.md` | 138 | REFERENCE | Per-item parity table for the pawn port. Pure audit data. |
| `project_local_plugin_patches.md` | 173 | REFERENCE + WORKFLOW | Patch records (must re-apply after upstream sync). Mix of code blocks (REFERENCE) and the "re-apply" rule (WORKFLOW). The patches themselves should stay in memory; the "always check this list before/after a plugin update" hint belongs in a skill. |
| `project_mm_implementation_plan.md` | 129 | STATE (planning doc) | 4-phase MM plan. Mostly executed; partial future work. |
| `project_motion_matching_plan.md` | 138 | STATE (planning doc) | Older big-picture migration plan. Largely superseded by the SM+BlendStack path actually taken. |
| `project_motion_matching_progress.md` | 144 | STATE | Progress log + key learnings. Mix; mostly historical. |

### gasp_*.md — GASP reference dumps (REFERENCE)

| File | Lines | Class | Reasoning |
|---|---|---|---|
| `gasp_animbp_architecture.md` | 179 | REFERENCE | Architecture summary of GASP ABP. |
| `gasp_animbp_full_audit.md` | 342 | REFERENCE | Authoritative deep audit (every node + binding + driver fn body). |
| `gasp_pawn_bp_full.md` | 359 | REFERENCE | Authoritative pawn BP audit (vars/funcs/events/input handlers). |
| `gasp_pawn_cpp_port_plan.md` | 191 | REFERENCE (inventory + plan) | Original port plan; superseded by `_ledger.md`. Variable/function inventory still useful as quick lookup. |
| `gasp_character_movement.md` | 137 | REFERENCE | Pawn architecture + movement modes overview. |
| `gasp_cpp_architecture.md` | 100 | REFERENCE | "GASP has zero custom C++" + plugin/data flow. |
| `gasp_data_model.md` | 279 | REFERENCE | All enums/structs/curves summary. |
| `gasp_data_model_full.md` | 460 | REFERENCE | Authoritative field-by-field dump with AZ-parity check. |
| `gasp_framework_cameras_rigs.md` | 354 | REFERENCE | GM/PC/Cameras/ControlRigs/RetargetedCharacters/SmartObject deep audit. |
| `gasp_movement_modes.md` | 387 | REFERENCE | BP_MovementMode_Walking/Falling/Slide audit. |
| `gasp_actor_components_and_notifies.md` | 263 | REFERENCE | AC_PreCMCTick / AC_TraversalLogic / AC_VisualOverrideManager / BPI_* / AnimNotifies. |
| `gasp_orientation_intent_tip.md` | 95 | REFERENCE | Per-mode `Get_OrientationIntent` rules + threshold-gated TIP pattern + 4 pitfalls list. |
| `gasp_posesearch_choosers.md` | 121 | REFERENCE | 29 schemas, 168 databases, 14 chooser tables. |
| `gasp_project_settings.md` | 88 | REFERENCE | DDCVars, plugins, blend profiles, gameplay tags. |
| `gasp_sm_tip_flow.md` | 121 | REFERENCE | 9 states + Conduits/Re-Enter SM topology, transition rules. |
| `gasp_update_logic_flow.md` | 328 | REFERENCE + WORKFLOW | "IsMoving = Trj_FutureVelocity+Accel (NOT velocity)" — critical rule but also a deep flow doc. Body is REFERENCE; the "AZ missing" lesson belongs in a skill. |

### reference_*.md — pure catalogs

| File | Lines | Class | Reasoning |
|---|---|---|---|
| `reference_noweapon_anim_catalog.md` | 32 | REFERENCE | 191 anim names categorized. Pure catalog. |
| `reference_cht_chooser_structure.md` | 66 | REFERENCE | CHT row structure + IdleBreak rows. |
| `reference_gasp_anim_notifies.md` | 412 | REFERENCE | Notify catalog per anim category + 10 application rules. |
| `reference_bp_node_tools.md` | 88 | REFERENCE + WORKFLOW | `AZ_BlueprintNodeUtils` API reference + "Key Rules" section is procedural. Hybrid. |
| `reference_animgraph_node_tools.md` | 104 | REFERENCE + WORKFLOW | `AZ_AnimGraphNodeUtils` API reference + GASP BlendStack chain doc. Hybrid. |
| `reference_ue5_python_anim_notifies.md` | 48 | REFERENCE + WORKFLOW | Python recipe + C++ bridge function list. |
| `reference_ue5_python_posesearch.md` | 52 | REFERENCE + WORKFLOW | What works / what doesn't for PoseSearch via Python + recipes. |
| `reference_zaggoth_lh_ik_tutorial.md` | 34 | REFERENCE | External tutorial summary. |
| `inventory-system.md` | 56 | REFERENCE | CommonUI inventory migration status. Mostly state, but enumerative (lookup-shaped). |
| `weapon_swap_architecture.md` | 97 | REFERENCE | Weapon system architecture freeze. |
| `ue_ai_plugins_comparison.md` | 76 | REFERENCE | Comparison table for the 3 UE-AI plugins. |

### Raw dumps (NOT memory, never loaded)

`_az_abp_dump.txt`, `_gasp_animgraph_dump.txt`, `_gasp_audit/` (dir), `_gasp_blendstack_dump.txt`, `_gasp_funcs_dump.txt`, `_gasp_sm_dump.txt`, `_gasp_sm_full.txt`, `_gasp_sm_states.txt`, `_gasp_sm_transitions.txt`. Underscore-prefixed; ignore.

---

## Section 3 — Proposed new skills

Eight skills proposed. Order matches Section 7 implementation priority.

### 3.1 `cpp-build-livecoding`

```yaml
name: cpp-build-livecoding
description: AZ C++ build and Live Coding workflow. Use when editing files under Source/AZ or the local UnrealClaude plugin source, or when the user reports a C++ compile/link error or asks to rebuild. Covers CLI build vs Live Coding, where to read the real error log (UnrealBuildTool Log.txt — NOT the editor log), iteration loop, when plugin DLL changes need a full editor restart, and the "only ask user to PIE after Result: Succeeded" discipline.
```

- **Trigger conditions:** any edit to `C:\UnrealEngine\Games\AZ\Source\AZ\**` or `C:\UnrealEngine\Engine\Plugins\Marketplace\UnrealClaude\**`; user says "build", "compile", "live coding", "live code", "rebuild", "compile error", "linker error".
- **Content outline:**
  - Step 1: try CLI `Build.bat AZEditor Win64 Development` (full command).
  - Step 2: if CLI says "Live Coding active", trigger via MCP `unreal_execute_script script_type=console` running `LiveCoding.Compile`.
  - Step 3: read errors from `C:/UnrealEngine/Engine/Programs/UnrealBuildTool/Log.txt` (Bash `tail -100`), look for `Result: Succeeded` / `Result: Failed (OtherCompilationError)`. Secondary: `LiveCodingConsole.log`.
  - Step 4: iterate fix → recompile → recheck — without bothering user.
  - Step 5: only after green, ask user to PIE.
  - Plugin DLL flakiness: when changes touch module init/shutdown/CVar/UObject lifecycle, prefer close-editor + CLI build.
  - The Python-from-MCP GC trap: never call `compile_blueprint()`/`save_loaded_asset()` on AnimBPs from Python (link to `feedback_python_gc_crash.md`).
- **Memory it links to:** `feedback_python_gc_crash.md`, `project_local_plugin_patches.md` (FConsoleManager incident).
- **Tools referenced:** Bash, `mcp__unrealclaude__unreal_execute_script`, `mcp__unrealclaude__unreal_get_output_log`.
- **Estimated size:** ~70 lines.

### 3.2 `bp-to-cpp-port`

```yaml
name: bp-to-cpp-port
description: Phased BP→C++ porting workflow with the dual-reviewer (Reviewer A live-source / Reviewer B memory-ledger) gate. Use when porting any GASP Blueprint asset (Pawn, AnimBP, MovementMode, ActorComponent, struct, function) into an AZ C++ class, when reviewing a port that's already underway, or when planning a phased migration. Covers the 7 mandatory acceptance gates (live-source verification, USTRUCT field type fidelity, UFUNCTION metadata, UPROPERTY parity, body/logic, engine API signatures, build/symbol export), the past-misses catalog, the get_nodes vs get_node_pins type-truncation pitfall, and the inspect-is-not-authoritative pitfall.
```

- **Trigger conditions:** "port BP to C++", "GASP port", "phase N of port", "review port", "reviewer prompt", "USTRUCT parity", "UFUNCTION metadata".
- **Content outline:**
  - When to use: the dual-reviewer pattern with Haiku models.
  - Phase template: implement → LC compile → Reviewer A (live BP via MCP) → Reviewer B (ledger/memory) → both APPROVE → next phase.
  - Full 7-gate checklist verbatim (so reviewers can be prompted from inside the skill text).
  - Past misses (3 documented misses with detection recipe).
  - Pitfall: `get_nodes` truncates types (`TArray<FName>`→`name`). Always follow up with `get_node_pins`.
  - Pitfall: `inspect` function list is a lower bound, not authoritative.
  - Pitfall: ASSET REGISTRY lag — re-query or load via `EditorAssetLibrary.load_asset` for ground truth.
  - References to ongoing port work (link to ledgers but don't duplicate).
- **Memory it links to:** `feedback_bp_to_cpp_port_review_checklist.md` (full content), `project_gasp_pawn_port_audit_2026-05-02.md`, `project_gasp_abp_port_ledger.md`, `gasp_pawn_cpp_port_plan.md`.
- **Tools referenced:** `mcp__unrealclaude__unreal_blueprint_query` (get_node_pins, get_nodes, inspect, get_graph), Task (for spawning reviewers).
- **Estimated size:** ~140 lines (the checklist alone is ~60 lines; without it the skill loses its main value).

### 3.3 `unrealclaude-mcp-tools`

```yaml
name: unrealclaude-mcp-tools
description: How to drive the UnrealClaude MCP server (port 3000) — direct vs deferred vs hidden tool catalog, when to use blueprint_query vs unreal_ue domain router vs execute_script, how to load deferred tools (ToolSearch select:...), per-tool gotchas (search_nodes can't see SM transition rule sub-graphs, inspect omits parent-class virtual overrides, get_nodes truncates types), and the standing rule "GASP content is imported into AZ at /Game/Blueprints/ — never look in C:\UnrealEngine\Games\GameAnimationSample".
```

- **Trigger conditions:** any MCP tool call decision; user says "MCP", "blueprint_query", "execute_script", "unreal_ue", "GASP MCP", "gassample".
- **Content outline:**
  - Tool catalog (direct, deferred, hidden) — restate from az-workspace but more focused on *when to pick which*.
  - Decision tree: read-only → blueprint_query; modify → unreal_ue; script → execute_script; data dump → unreal_get_ue_context.
  - **Standing rule on GASP location** (single source: `/Game/Blueprints/` in AZ — not the external GASP folder; gassample MCP at port 3001 is deprecated).
  - Gotchas (search_nodes blind to transition rule sub-graphs, inspect lower-bound, get_nodes truncates).
  - SM transition rule access: state names have spaces, use state-inside-SM identifying name not the SM node's name (link to `feedback_sm_transition_access.md`).
  - Deferred-tool loading: `ToolSearch query="select:mcp__unrealclaude__unreal_execute_script"`.
  - Plugin patches the user maintains (link to `project_local_plugin_patches.md` § 1, 1b, 1c) — "if get_node_pins doesn't show bindings, the patch is missing".
- **Memory it links to:** `project_local_plugin_patches.md`, `feedback_sm_transition_access.md`, `ue_ai_plugins_comparison.md`.
- **Tools referenced:** the entire MCP tool catalog.
- **Estimated size:** ~110 lines.

### 3.4 `az-cpp-utility-tools`

```yaml
name: az-cpp-utility-tools
description: Reference for the AZ C++ scripting utilities — AZ_BlueprintNodeUtils, AZ_AnimGraphNodeUtils, AZ_AnimBlueprintUtils, AZ_ChooserUtils, AZ_PoseSearchUtils, AZ_SkeletonUtils. Use when programmatically modifying Blueprint/AnimGraph nodes from Python (via mcp execute_script) or C++. Lists all UFUNCTION signatures, pin-name cheat sheets, transition rule node helpers, BlendStack inner graph helpers, and the FixFunctionForAnimBinding rule (any function called from a thread-safe function must also be marked thread-safe).
```

- **Trigger conditions:** "AnimGraph", "BlendStack inner", "SetBlendStackAnim", "transition rule", "K2Node", "AnimGetter", "ArrayContains", "ChooserUtils", "PoseSearchUtils", "AZ_BlueprintNodeUtils", "AZ_AnimGraphNodeUtils".
- **Content outline:**
  - Per-utility function catalog (links to memory for full signatures).
  - Pin name cheat sheet (what's input vs output on each common K2Node).
  - "Key rules" — set type before AllocateDefaultPins; use Schema->TryCreateConnection; FixFunctionForAnimBinding must be called after add_function for thread-safe binding; SM bindings need pass-by-reference; Live Coding can't add new UFUNCTIONs (full rebuild for new Python bindings).
  - Transition rule node patterns (AnimGetter, ArrayContains, comparisons, BooleanAND/OR, shared rule unsharing).
  - BlendStack chain: GASP vs AZ simplified.
  - Wire vs binding approach (GASP uses K2Node_AnimNodeReference wires).
  - Python recipe template for calling these via `unreal_execute_script script_type=python`.
- **Memory it links to:** `reference_bp_node_tools.md` (full), `reference_animgraph_node_tools.md` (full), `feedback_transition_rule_tools.md`, `feedback_python_gc_crash.md`, `feedback_deep_pin_inspection.md`.
- **Tools referenced:** `mcp__unrealclaude__unreal_execute_script` (Python and cpp script types).
- **Estimated size:** ~150 lines.

### 3.5 `anim-debug-pitfalls`

```yaml
name: anim-debug-pitfalls
description: Diagnostic checklist for AnimGraph/AnimBP issues — mesh visible spin, A-pose, SM stuck in source state, foot slide, OffsetRootBone misbehavior. Covers the Post-event vs ThreadSafe driver-placement rule, the FAnimNodeReference must-be-wired rule (unwired = silent fail-closed = SM stuck), OffsetRootBone enum value mapping (Accumulate=0, Interpolate=1, Release=5 — NOT 0/1/2 with comment-mismatched names), the Get_OrientationIntent threshold-cache pattern (cache only on |delta| ≥ 60°), and the IsMoving = Trj_FutureVelocity+Accel (NOT velocity) rule.
```

- **Trigger conditions:** "mesh spinning", "mesh rotates", "A-pose", "ref pose", "stuck in state", "transition not firing", "OffsetRootBone", "TIP not triggering", "foot slide", "drift", user reports a visible animation glitch.
- **Content outline:**
  - **Mesh visible spin / phantom offset** → Post-event vs ThreadSafe rule (full from `feedback_animbp_post_event_vs_thread_safe.md`).
  - **A-pose / SM stuck in source state** → check K2Node_AnimNodeReference wiring + Tag match; library calls fail-closed when ref invalid (full from `feedback_animgraph_node_reference_wiring.md`).
  - **OffsetRootBone misbehaving (mesh drift/spin)** → enum value mapping + Translation/Rotation per state — link to `project_idle_tip_implementation.md` "OffsetRootBone enum mapping".
  - **TIP triggers wrong / not at all** → speed-independent accumulator design + per-mode rules + the 4-pitfall list — link to `gasp_orientation_intent_tip.md`.
  - **Wrong-time stops / no stops** → IsMoving formula = `Trj_FutureVelocity != 0 (tol 10) AND Accel != 0` (NOT Speed); 5-var per-state-enum tracking pattern (`X`, `_LastFrame`, `_Recent`, `_Time`, `_LastStateTime`) — link to `gasp_update_logic_flow.md`.
  - Where to look in logs: `unreal_get_output_log` filter `LogAnimation`, `LogPoseSearch`, `LogMover`.
- **Memory it links to:** `feedback_animbp_post_event_vs_thread_safe.md`, `feedback_animgraph_node_reference_wiring.md`, `project_idle_tip_implementation.md`, `gasp_orientation_intent_tip.md`, `gasp_update_logic_flow.md`, `gasp_sm_tip_flow.md`.
- **Tools referenced:** `mcp__unrealclaude__unreal_blueprint_query` (get_node_pins for binding readout), `unreal_get_output_log`.
- **Estimated size:** ~120 lines.

### 3.6 `gasp-parity-reference`

```yaml
name: gasp-parity-reference
description: Index for GASP parity work in AZ. Use when comparing AZ to GASP, debugging divergence from GASP, or porting a missing function/property. Restates the standing rule "GASP content is imported into AZ at /Game/Blueprints/ — query via the unrealclaude MCP, do NOT use C:\UnrealEngine\Games\GameAnimationSample or the gassample MCP". Lists every gasp_*.md memory file with a one-line topic so the right reference can be loaded on demand.
```

- **Trigger conditions:** "GASP", "GameAnimationSample", "parity audit", "vs GASP", "GASP equivalent", "S_BlendStackInputs", "S_ChooserOutputs", "Mover SM topology", "what does GASP do for X".
- **Content outline:**
  - **Standing rule** (GASP imported into AZ /Game/Blueprints/, NOT external project).
  - Pointers (just the index, no content):
    - Pawn audit: `gasp_pawn_bp_full.md`, `gasp_pawn_cpp_port_plan.md`, `project_gasp_pawn_port_audit_2026-05-02.md`
    - AnimBP: `gasp_animbp_architecture.md`, `gasp_animbp_full_audit.md`, `project_gasp_abp_port_ledger.md`
    - Movement modes: `gasp_movement_modes.md`, `gasp_character_movement.md`
    - Data model: `gasp_data_model.md`, `gasp_data_model_full.md`
    - State machine: `gasp_sm_tip_flow.md`, `gasp_update_logic_flow.md`
    - PoseSearch / Choosers: `gasp_posesearch_choosers.md`, `reference_cht_chooser_structure.md`
    - OrientationIntent + TIP: `gasp_orientation_intent_tip.md`
    - Project settings: `gasp_project_settings.md`
    - Actor components / Interfaces / Notifies: `gasp_actor_components_and_notifies.md`, `reference_gasp_anim_notifies.md`
    - Framework/Cameras/Rigs: `gasp_framework_cameras_rigs.md`
    - C++ overview: `gasp_cpp_architecture.md`
  - "When in doubt, query the live BP via `unreal_blueprint_query` against `/Game/Blueprints/...` instead of trusting memory snapshots."
- **Memory it links to:** all 16 gasp_*.md and the GASP-related project_*.md files.
- **Tools referenced:** `mcp__unrealclaude__unreal_blueprint_query`.
- **Estimated size:** ~70 lines.

### 3.7 `agent-and-research-discipline`

```yaml
name: agent-and-research-discipline
description: Workflow rules for research and Task-agent usage in the AZ project — check memory before spawning agents, launch teams of 3-8 in parallel for non-trivial research, validate agent findings against editor state before destructive changes, never ask for read approval, always use full absolute paths in output.
```

- **Trigger conditions:** before any Task / Agent call, before "Explore" / "research" tasks, when user asks about a system that has memory entries.
- **Content outline:**
  - 1. Check memory FIRST (full from `feedback_check_memory_first.md`).
  - 2. Use teams of 3-8 agents in parallel for research (full from `feedback_use_agent_teams.md`).
  - 3. Validate findings: when agents disagree, trust the one that queried actual node data; when an agent says "X doesn't exist", verify visually before removing code (full from `feedback_validate_agent_findings.md`).
  - 4. No read approval (full from `feedback_no_read_approval.md`).
  - 5. Always use full absolute paths in user-visible output (full from `feedback_file_paths.md`).
  - When to save NEW memory: a workflow rule we wish past Claude had known; a non-obvious fix; an architectural decision with rationale. NOT: code we just looked at; recoverable info.
- **Memory it links to:** `feedback_check_memory_first.md`, `feedback_use_agent_teams.md`, `feedback_validate_agent_findings.md`, `feedback_no_read_approval.md`, `feedback_file_paths.md`.
- **Tools referenced:** Task, Skill (for self-invocation reminder).
- **Estimated size:** ~60 lines.

### 3.8 `asset-modification-via-python`

```yaml
name: asset-modification-via-python
description: Recipes for modifying AZ assets (BPs, AnimBPs, AnimSequences, PoseSearch databases, Choosers, IK Retargeter outputs) via Python through mcp execute_script. Covers what works directly via unreal Python API, what requires the UAZ_PoseSearchUtils / UAZ_AnimGraphNodeUtils / UAZ_BlueprintNodeUtils C++ bridges, what crashes the editor (ReconstructNode/CompileBlueprint/save_loaded_asset on AnimBPs from Python), the Anim Notifies are protected pitfall, and the IK Retargeter "duplicate the asset to bake root motion" workaround.
```

- **Trigger conditions:** "Python script", "execute_script python", "modify asset", "add notify", "PoseSearch database", "BP defaults", "set CDO", "Chooser row", "retarget animation".
- **Content outline:**
  - Run command template: `unreal_execute_script script_type=python` with `@Description` header.
  - The GC crash rule (what NOT to call from Python).
  - PoseSearch creation pattern (factory=None).
  - Adding anims to PoseSearch databases via `UAZ_PoseSearchUtils` C++ bridge.
  - Adding anim notifies via `UAZ_PoseSearchUtils.AddBlockTransitionToDatabase`.
  - InputAction / IMC creation: 5.7 deprecation note (use `default_key_mappings.mappings`).
  - IK Retargeter root motion: duplicate the output asset to bake root motion (full from `feedback_retarget_root_motion.md`).
  - Setting CDO defaults via Python (e.g., AnimClass on Mesh component, IMC on PlayerController, Chooser refs on AnimBP).
- **Memory it links to:** `reference_ue5_python_posesearch.md`, `reference_ue5_python_anim_notifies.md`, `feedback_python_gc_crash.md`, `feedback_retarget_root_motion.md`.
- **Tools referenced:** `mcp__unrealclaude__unreal_execute_script` (script_type=python and cpp).
- **Estimated size:** ~90 lines.

---

## Section 4 — Memory files to keep as-is (REFERENCE / STATE — never become skills)

| File | Why it stays as memory |
|---|---|
| `gasp_animbp_full_audit.md` (342 lines) | Authoritative deep audit. Pure data lookup. Any skill wanting it would just say "read this file". |
| `gasp_data_model_full.md` (460) | Field-by-field struct/enum dump with parity check. Pure lookup. |
| `gasp_pawn_bp_full.md` (359) | Authoritative pawn audit. Lookup. |
| `gasp_movement_modes.md` (387) | Per-mode deep audit. Lookup. |
| `gasp_framework_cameras_rigs.md` (354) | GM/PC/Cameras/Rigs audit. Lookup. |
| `gasp_update_logic_flow.md` (328) | Body is reference (full GASP flow); the lesson "IsMoving=FutureVel+Accel" goes into `anim-debug-pitfalls` skill, but the full doc stays. |
| `project_gasp_abp_port_ledger.md` (306) | 107-var/63-fn ledger. Lookup. |
| `gasp_data_model.md` (279) | Enum/struct summary. Lookup. |
| `gasp_actor_components_and_notifies.md` (263) | Component/notify audit. Lookup. |
| `gasp_pawn_cpp_port_plan.md` (191) | Original port plan inventory. Lookup. |
| `gasp_animbp_architecture.md` (179) | ABP architecture summary. Lookup. |
| `project_local_plugin_patches.md` (173) | Patch records (code blocks). Lookup; the "re-apply after sync" hint surfaces in `cpp-build-livecoding` and `unrealclaude-mcp-tools`. |
| `project_motion_matching_progress.md` (144) | Historical progress + key learnings. STATE. |
| `project_session_2026-04-24_gasp_animbp_cpp_done.md` (144) | Session log. STATE. |
| `project_motion_matching_plan.md` (138) | Older big-picture plan. STATE / superseded. |
| `project_gasp_pawn_port_audit_2026-05-02.md` (138) | Per-item parity table. Lookup. |
| `gasp_character_movement.md` (137) | Pawn architecture summary. Lookup. |
| `project_mm_implementation_plan.md` (129) | Phased plan. STATE. |
| `gasp_sm_tip_flow.md` (121) | SM topology. Lookup. |
| `gasp_posesearch_choosers.md` (121) | Schema/database/chooser counts + structure. Lookup. |
| `reference_animgraph_node_tools.md` (104) | API ref (the procedural part is restated in `az-cpp-utility-tools` skill; full table stays here). |
| `gasp_cpp_architecture.md` (100) | Plugin overview. Lookup. |
| `gasp_orientation_intent_tip.md` (95) | Per-mode rules + 4-pitfall list. Lookup; the rules are summarised in `anim-debug-pitfalls`. |
| `weapon_swap_architecture.md` (97) | Weapon system architecture freeze. Lookup. |
| `gasp_project_settings.md` (88) | DDCVars / plugins / blend profiles / tags. Lookup. |
| `reference_bp_node_tools.md` (88) | API ref. Same disposition as animgraph counterpart. |
| `project_session_2026-04-22_gasp_pawn_done.md` (83) | Session log. STATE. |
| `ue_ai_plugins_comparison.md` (76) | Comparison table. Lookup. |
| `project_gasp_animbp_cpp_port_plan.md` (70) | Day-1 plan (executed). STATE. |
| `reference_cht_chooser_structure.md` (66) | CHT row structure + IdleBreak rows. Lookup. |
| `project_session_2026-04-26_abp_first_motion.md` (65) | Session log; the 9-thunk recipe gets summarised in `bp-to-cpp-port` or as a sub-section of `anim-debug-pitfalls`. |
| `project_idle_tip_implementation.md` (66) | Working baseline + tuning knobs. Mostly STATE; the key rules are summarised in `anim-debug-pitfalls`. |
| `project_input_stack_rt_mirror.md` (61) | Decision log. STATE. |
| `inventory-system.md` (56) | CommonUI inventory migration status. Lookup. |
| `reference_ue5_python_posesearch.md` (52) | Recipe; condensed into `asset-modification-via-python` skill but full doc retained for the "what does NOT work" details. |
| `reference_ue5_python_anim_notifies.md` (48) | Same as above. |
| `feedback_transition_rule_tools.md` (34) | API ref; full retained in memory; gist surfaces in `az-cpp-utility-tools`. |
| `reference_zaggoth_lh_ik_tutorial.md` (34) | External tutorial summary. Lookup. |
| `reference_noweapon_anim_catalog.md` (32) | 191-anim catalog. Lookup. |
| `feedback_sm_transition_access.md` (17) | Specific recipe; gist in `unrealclaude-mcp-tools` skill, full doc retained for the trailing-space gotcha. |
| `feedback_ik_setup.md` (13) | Recipe; could be folded into `weapon_swap_architecture.md`. Keep for now. |
| `project_gas_gameplay.md` (11) | Almost-empty stub. Candidate for deletion. |

---

## Section 5 — Estimated token savings

### Always-loaded budget today

`MEMORY.md` is 132 lines. At ~30 tokens / line average for prose with bullets and links → **~4,000 tokens always loaded** every conversation. Most of those tokens are one-line index entries pointing at memory files; they earn their keep when a topic is touched but cost on every cold-start.

### Where the wins are

| Move | Lines removed from MEMORY.md | Token saving (always) | Trigger probability per conversation |
|---|---|---|---|
| Workflow rules block (lines 3-7) → split between `bp-to-cpp-port`, `anim-debug-pitfalls`, `agent-and-research-discipline` | 5 | ~150 | most conversations don't touch porting; saving accrues |
| GASP reference index (lines 69-85) → moved to `gasp-parity-reference` skill | 17 | ~510 | only loads when user mentions GASP |
| Motion Matching index (lines 64-67) → folded into `gasp-parity-reference` and `asset-modification-via-python` | 4 | ~120 | rarely active in a given session |
| C++ Utility Tools block (lines 114-121) → `az-cpp-utility-tools` skill | 8 | ~240 | only when modifying BPs programmatically |
| References block (lines 122-127) → `asset-modification-via-python` and `weapon_swap_architecture` (kept as memory ref) | 6 | ~180 | rarely |
| GASP Pawn C++ Port block (lines 103-112) → `bp-to-cpp-port` skill links the ledgers; index removed | 10 | ~300 | only during port sessions |
| Animation System block (lines 53-59) | 7 | ~210 | most conversations don't touch anim debug from scratch |

**Conservative total saving in always-loaded budget: ~1,700 tokens per conversation cold-start.** That's ~40% of `MEMORY.md`'s current size.

### Skill file sizes (only loaded when triggered)

| Skill | Lines | Tokens (~30/line) |
|---|---|---|
| `cpp-build-livecoding` | 70 | 2,100 |
| `bp-to-cpp-port` | 140 | 4,200 |
| `unrealclaude-mcp-tools` | 110 | 3,300 |
| `az-cpp-utility-tools` | 150 | 4,500 |
| `anim-debug-pitfalls` | 120 | 3,600 |
| `gasp-parity-reference` | 70 | 2,100 |
| `agent-and-research-discipline` | 60 | 1,800 |
| `asset-modification-via-python` | 90 | 2,700 |
| **Total skill content** | **810 lines** | **~24,300 tokens** (only loaded on demand) |

### Net effect

- **MEMORY.md shrinks** from 132 → ~80 lines (workflow rules trimmed; pure indices retained for STATE/REFERENCE files since they still need pointers).
- **Per-conversation always-loaded** drops by ~1,700 tokens (~40% of MEMORY.md savings).
- Cost accrued only when a skill triggers — and even then, only the matching skill loads (not all of them).
- Net win is largest for short tasks that don't touch any of these skills' triggers.

---

## Section 6 — Implementation order

Ordered by impact × clarity × dependency.

1. **`agent-and-research-discipline`** (1st — highest leverage). Five short feedback files become one tight skill. Always-loaded MEMORY.md trimming starts here. Low risk. Probably auto-triggers most conversations.

2. **`cpp-build-livecoding`** (2nd — extracts the longest section currently in `az-workspace`). Removes ~30 lines of procedural content from `az-workspace` and gives it a more discoverable trigger surface. Lets `az-workspace` shrink to a pure folder map.

3. **`bp-to-cpp-port`** (3rd — biggest single token win on triggered load). The 7-gate checklist is currently re-pasted into reviewer prompts manually; making it a skill means the reviewer prompt template lives in one place. Triggers less often but high-value when it does.

4. **`unrealclaude-mcp-tools`** (4th). Consolidates the MCP catalog + standing rules + gotchas. Some content moves out of `az-workspace`; rest sourced from `feedback_sm_transition_access.md` and `project_local_plugin_patches.md`.

5. **`anim-debug-pitfalls`** (5th — saves the most time when a real anim bug hits). Five overlapping memory rules (post-event vs thread-safe, FAnimNodeReference wiring, OffsetRootBone enum, OrientationIntent threshold, IsMoving formula) become one diagnostic checklist.

6. **`az-cpp-utility-tools`** (6th). Reference-heavy; needs careful curation so it doesn't just duplicate `reference_bp_node_tools.md` and `reference_animgraph_node_tools.md`. Skill should be the index + key rules; full tables stay as memory references.

7. **`asset-modification-via-python`** (7th). Pulls together the Python recipes; smaller benefit but cleanly scoped.

8. **`gasp-parity-reference`** (8th — last because it's mostly an index that doesn't reduce always-loaded much). The biggest win here is conditional loading of the GASP block (currently always-loaded as one-line pointers).

After all 8 are written:
- Trim `az-workspace` to just the folder map + git facts.
- Trim `MEMORY.md` to: project overview, file index by topic (one line per file), and pointers to skills for "if you need to do X, the skill `<name>` covers it".

---

## Open questions / decisions for next pass

1. **Should `weapon_swap_architecture` and `inventory-system` get skills?** Both are large reference docs. They're STATE/REFERENCE not WORKFLOW — current proposal leaves them as memory. Could add a `commonui-inventory-migration` skill if the user resumes that work.
2. **`feedback_ik_setup.md` (13 lines)** — too small for its own skill. Either fold into `weapon_swap_architecture.md` or into `anim-debug-pitfalls`.
3. **`project_gas_gameplay.md` (11 lines, 51 days old)** — recommend deletion.
4. **The 8 skills proposed all have non-overlapping triggers but content overlaps slightly** (e.g. the `inspect`-truncation pitfall appears in both `bp-to-cpp-port` and `unrealclaude-mcp-tools`). Acceptable — each skill is self-contained from its trigger angle.
5. **Should `cpp-build-livecoding` be merged into `unrealclaude-mcp-tools`?** They overlap on `unreal_execute_script script_type=console`. Argument for merging: shared trigger surface. Argument against: build is a strict workflow with steps, MCP tools is a catalog. Recommend keeping them separate.
