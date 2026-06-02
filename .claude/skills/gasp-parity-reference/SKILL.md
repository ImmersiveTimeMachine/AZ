---
name: gasp-parity-reference
description: Index for GASP parity work in AZ. Use when comparing AZ to GASP, debugging divergence from GASP, or porting a missing function/property. Restates the standing rule "GASP content is imported into AZ at /Game/Blueprints/ — query via the unrealclaude MCP, do NOT use C:\UnrealEngine\Games\GameAnimationSample or the gassample MCP". Lists every gasp_*.md memory file with a one-line topic so the right reference can be loaded on demand.
---

# GASP Parity Reference Index

Pure pointer file — load the matching memory file on demand. Don't restate its contents here.

---

## Standing rule (READ BEFORE INSPECTING GASP)

**The entire GASP project content has been imported into AZ at `/Game/Blueprints/`** (filesystem: `C:\UnrealEngine\Games\AZ\Content\Blueprints\`).

- Query via `mcp__unrealclaude__unreal_blueprint_query` against `/Game/Blueprints/SandboxCharacter_Mover_ABP` etc. on port **3000** (the AZ editor's MCP).
- Do **NOT** ask the user to launch the standalone GASP editor.
- Do **NOT** look in `C:\UnrealEngine\Games\GameAnimationSample\` (external copy may exist but is not the source of truth).
- The `gassample` MCP at port **3001** is **deprecated** — ignore it.

When in doubt about anything in the memory dumps below, re-query the live BP via `unreal_blueprint_query` instead of trusting a snapshot. Memory files have point-in-time stale-warnings emitted on read.

All memory paths below are under `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\`.

---

## GASP reference index by topic

### Pawn (SandboxCharacter_Mover)

| File | Topic |
|---|---|
| `gasp_pawn_bp_full.md` | Authoritative pawn BP audit — every var/func/event/input handler/Mover input packing/Get_OrientationIntent verbatim |
| `gasp_pawn_cpp_port_plan.md` | Original port inventory (18 vars, 23 funcs, 5 events) + 9-phase plan (largely executed) |
| `project_gasp_pawn_port_audit_2026-05-02.md` | Live-vs-AZ parity table — drives next round of "exact GASP" pawn fixes |
| `project_session_2026-04-22_gasp_pawn_done.md` | Pawn port session log (Phases 1–8) |
| `project_input_stack_rt_mirror.md` | RT IMC + 21 IA mirror; PC/Pawn rewired; `Get_Gait` inverted (Walk=default, Shift=Sprint) |

### AnimBP (SandboxCharacter_Mover_ABP)

| File | Topic |
|---|---|
| `gasp_animbp_architecture.md` | High-level ABP structure: dual locomotion (pure MM vs SM+BlendStack), Chooser-driven DB selection, Mover trajectory |
| `gasp_animbp_full_audit.md` | **Authoritative deep audit** — every node + binding + driver fn body, BlendStack inner graph full structs, SM states/transitions, OnStateEntry events (342 lines) |
| `project_gasp_abp_port_ledger.md` | 107-var, 63-fn ledger comparing GASP↔AZ status row-by-row; C++/ABP split decisions locked |
| `project_gasp_animbp_cpp_port_plan.md` | Day-1 ABP port plan (executed) |
| `project_session_2026-04-24_gasp_animbp_cpp_done.md` | ABP port Phases 0–8 session log (all dual-reviewer-APPROVED) |
| `project_session_2026-04-26_abp_first_motion.md` | Phase 9 ABP wiring: 3 chooser/MDT defaults + EventGraph + 9 OnStateEntry chooser-eval graphs |

### Movement & character

| File | Topic |
|---|---|
| `gasp_character_movement.md` | Mover-based APawn architecture, movement modes overview, input flow, interfaces, traversal |
| `gasp_movement_modes.md` | BP_MovementMode_Walking/Falling/Slide deep audit — parent SmoothWalkingMode, FacingTime/TurnStrength defaults, GenerateWalkMove rotation logic |

### Data model

| File | Topic |
|---|---|
| `gasp_data_model.md` | Summary: enums (Gait/Stance/MovementMode/Direction/RotationMode), structs (S_BlendStackInputs, S_ChooserOutputs, S_PlayerInputState, S_MoverCustomInputs), curves |
| `gasp_data_model_full.md` | **Authoritative field-by-field dump** of every S_*/E_*/Curve_*/BFL_HelpfulFunctions with AZ-parity check (460 lines) |

### State machine & TIP

| File | Topic |
|---|---|
| `gasp_sm_tip_flow.md` | Full SM topology — 9 states + Conduits/Re-Enter pseudo-states; 29 transitions catalogued; TIP entry via Conduit; Re-Enter rule = bare `Enable_TurnInPlaceSteering < 0.1` |
| `gasp_orientation_intent_tip.md` | Per-mode `Get_OrientationIntent` rules verbatim from GASP comments; threshold-gated TIP pattern (cache only on |delta| ≥ 60°); 4-pitfall list |
| `gasp_update_logic_flow.md` | **Critical for idle-walk-idle cycle.** `IsMoving = Trj_FutureVelocity + Accel` (NOT velocity); 6-state-enum 5-field tracking pattern; full update flow + AZ missing-fields list |

### PoseSearch / Choosers / Notifies

| File | Topic |
|---|---|
| `gasp_posesearch_choosers.md` | 29 schemas, 168 databases (4 density tiers), 14 chooser tables, normalization sets |
| `reference_cht_chooser_structure.md` | CHT_AZ_CharacterAnimations row structure + IdleBreak rows; S_ChooserOutputs schema |
| `reference_gasp_anim_notifies.md` | Notify catalog per anim category (BlockTransition, BranchIn, ExcludeFromDatabase, ModifyCost, OverrideContinuingPoseCostBias) and curves (contact_l/r, movedata_speed, enable_warping, Phase) + 10 application rules |

### Project settings, framework, components

| File | Topic |
|---|---|
| `gasp_project_settings.md` | DDCVars, physics, network prediction 60Hz, plugins, input actions, blend spaces, gameplay tags |
| `gasp_framework_cameras_rigs.md` | GM_Sandbox / PC_Sandbox / Cameras/ / ControlRigs/ / RetargetedCharacters/ / SmartObject/ deep audit |
| `gasp_actor_components_and_notifies.md` | AC_PreCMCTick / AC_TraversalLogic / AC_VisualOverrideManager / BPI_* / AnimNotifies — what each does, when it ticks, what AZ should port |
| `gasp_cpp_architecture.md` | "GASP has zero custom C++" + plugin/data flow overview |

---

## Quick lookup: "what does GASP do for X?" → file

| You're asking about... | Open this file first |
|---|---|
| `S_BlendStackInputs` / `S_ChooserOutputs` / any GASP struct | `gasp_data_model_full.md` |
| Mover input packing (Pre/Post sim) | `gasp_pawn_bp_full.md` |
| `Get_OrientationIntent` per RotationMode | `gasp_orientation_intent_tip.md` |
| `IsMoving` formula | `gasp_update_logic_flow.md` |
| State machine transition rule N | `gasp_sm_tip_flow.md` then live query |
| OnStateEntry graph for state X | `project_session_2026-04-26_abp_first_motion.md` (AZ port wiring) + `gasp_animbp_full_audit.md` |
| Chooser table column meaning | `reference_cht_chooser_structure.md` |
| Notify pattern for stop/loop/start anim | `reference_gasp_anim_notifies.md` |
| Walking vs Falling vs Slide mode rules | `gasp_movement_modes.md` |
| Cameras / Control Rigs / Game Mode | `gasp_framework_cameras_rigs.md` |

---

## Cross-references

- **Skill `bp-to-cpp-port`** — when porting a missing GASP function/property, run the 7-gate dual-reviewer checklist.
- **Skill `unrealclaude-mcp-tools`** — for `blueprint_query` operations against `/Game/Blueprints/...`.
- **Skill `anim-debug-pitfalls`** — for the 5 recurring failure modes (post-event vs threadsafe, FAnimNodeReference wiring, OffsetRootBone enum, TIP threshold, IsMoving formula).
