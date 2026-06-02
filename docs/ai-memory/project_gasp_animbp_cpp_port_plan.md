---
name: project_gasp_animbp_cpp_port_plan
description: Next session (2026-04-23+) — port GASP AnimBP /Game/Blueprints/SandboxCharacter_Mover_ABP into a new AZ AnimBP with C++ backing, same phased workflow as the pawn port.
type: project
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# Next Session — GASP AnimBP → AZ C++ Port

**Scheduled:** 2026-04-23+ (day after pawn port completion).

**Target:** `/Game/Blueprints/SandboxCharacter_Mover_ABP.SandboxCharacter_Mover_ABP` (GASP — on AZ side, also at `C:\UnrealEngine\Games\GameAnimationSample\Content\Blueprints\`).

**Why:** Finish GASP parity on the animation side. Current `UAZ_AnimInstance` has pieces (trajectory, state tracking, ShouldTurnInPlace, OffsetRootBone fix, chooser-driven BlendStack) but is not a full GASP port — missing pieces that Phases 1–8 of the pawn port deliver input/state data for but aren't consumed yet.

## What to create

1. **New AnimBP asset** (e.g. `AZ_ABP_HeroPawn_RT` or `AZ_ABP_Hero_MM`) as a fresh copy of `/Game/Blueprints/SandboxCharacter_Mover_ABP` structure.
2. **New C++ AnimInstance class** (e.g. `UAZ_AnimInstance_RT` or extend current) — full GASP-parity logic.
3. Wire new ABP onto `AZ_BP_HeroPawn` mesh when ready.

Keep current `UAZ_AnimInstance` + `AZ_ABP_HeroPawn` working as fallback until parity is verified.

## Workflow (same as today's pawn port)

- **Strict serial phases** — no parallelism.
- **Dual-reviewer gate per phase** — Reviewer A checks live GASP BP via MCP, Reviewer B checks memory files. Both must APPROVE before advancing. Haiku model for cheap reviewers.
- **Live Coding build check every phase** — `mcp__unrealclaude__unreal_run_console_command` with `LiveCoding.Compile`, then read `LogLiveCoding` in output log for errors.
- **GAS reintegration last** — same as Phase 9 of the pawn port.

## Phase sketch (to refine on day 1)

1. **Inventory GASP ABP** — dump all variables, event graph nodes, anim graph nodes, used functions/macros. Compare to current `UAZ_AnimInstance`. Produce a complete list of missing pieces.
2. **State variables** — any additional tracked state beyond what pawn now provides via `GetMoverStateSafe` / `GetPropertiesForAnimation_Implementation` (Phase 8 interface).
3. **Event graph / NativeUpdateAnimation** — GASP ABP reads from pawn via BPI_SandboxCharacter_Pawn → we already have `IAZ_SandboxCharacterPawn` (Phase 8 interface). Mirror the read sequence.
4. **Anim graph — State Controller SM** — GASP has a 9-state controller (see `gasp_sm_tip_flow.md`). AZ SM is currently simpler.
5. **BlendStack nodes** — GASP uses paired BlendStack nodes driven by choosers. AZ has a partial version.
6. **Chooser evaluation** — `SetCellAssetOnSub` and chooser-result unpack already exist in `UAZ_ChooserUtils`.
7. **Layered post-processing** — Foot IK / Control Rig / Offset Root Bone. OffsetRootBone enum fix is already shipped.
8. **Traversal / jump / slide sub-states** — as needed per GASP.
9. **GAS reintegration** — wire aim/fire/reload/hold-breath upper-body overlay, using existing `bWantsAimPose` / fire montage system.

## Starting context for next session

Read these memory files first:
- `gasp_pawn_cpp_port_plan.md` — pawn port plan (reference for structure)
- `project_input_stack_rt_mirror.md` — input stack current state
- `gasp_animbp_architecture.md` — GASP ABP architecture overview
- `gasp_sm_tip_flow.md` — 9-state SM topology
- `gasp_update_logic_flow.md` — GASP NativeUpdate logic
- `gasp_posesearch_choosers.md` — chooser tables
- `reference_gasp_anim_notifies.md` — notify catalog
- `project_idle_tip_implementation.md` — AZ idle TIP baseline (still working)

Key things already wired from today's pawn port that the ABP can consume:
- `AAZ_HeroPawn` implements `IAZ_SandboxCharacterPawn` → 4 methods return `FAZ_CharacterPropertiesFor{Animation,Camera,Traversal}` + `SetCharacterInputState`.
- Pawn exposes `MoverDefaultInputs_PostSim` + `MoverCustomInputs_PostSim` (replicated, PostSim, safe for anim worker thread reads).
- Pawn `Get_Gait` / `Get_RotationMode` / `Get_OrientationIntent` / `Get_MovementDirectionAndOffset` — all callable from ABP for chooser inputs.
- `bIdleTurnInProgress` single-source-of-truth — ABP should continue to mirror via `ShouldTurnInPlace()`.
- `OnMovementModeChanged` delegate bound on pawn — ABP can react to mode transitions.

## What NOT to break

- Working idle TIP baseline (commit `b5c076e1`).
- Existing anim instance is still in use by `AZ_ABP_HeroPawn` currently wired on pawn. Don't delete until new ABP is swapped in and verified.
- Rifle weapon system (upper-body montage blend, aim pose, fire montage) — part of current ABP. New port must preserve or re-implement.

## Blockers / risks

- GASP MCP server isn't registered in AZ's `.mcp.json` right now (MEMORY.md claims port 3001 but it's not actually configured). To query GASP BPs for parity, either add the gassample server entry OR read GASP files directly via AZ's unrealclaude MCP (GASP project files are on the same machine, but in a different content root). This blocks live BP-parity review from Reviewer A; may need to rely more on memory (Reviewer B) + manual BP inspection.
- New UINTERFACE/UCLASS additions require UHT pass. Live Coding often handles these in 5.7, but if LC fails, a full UBT build (editor close) will be needed.
