---
name: project_session_2026-04-26_abp_first_motion
description: Session 2026-04-26 — AZ_ABP_Mover wired end-to-end, character moves for the first time. Records the exact wiring needed (3 chooser/MDT defaults + 9 OnStateEntry chooser-eval graphs) plus C++ struct + thread-safe meta fixes from the same session.
type: project
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# Session 2026-04-26 — First Motion in AZ_ABP_Mover

## Result
Character animates for the first time after the GASP ABP C++ port. All wiring verified end-to-end: pawn → ABP → State Machine → BlendStack.

## C++ changes this session

1. **Struct field type fix** — `FAZ_BlendStackInputs.Tags` and `FAZ_ChooserOutputs.Tags` corrected from `FName` → `TArray<FName>` (matches GASP `S_BlendStackInputs/S_ChooserOutputs`). 4 cpp call sites updated to TArray semantics (`Tags.Contains(...)`, `Tags.Num() == 0`, manual join for debug).
2. **Thread-safe meta added** to `Update_CVarDrivenVariables`, `Update_Logic`, `Update_Trajectory`, `Update_EssentialValues`, `Update_States`, `Update_AimOffset`, `Update_AdditiveLean` — so they can be called from `BlueprintThreadSafeUpdateAnimation`.
3. **Thread-safe meta added** to `SetBlendStackAnimFromChooser` and `OnUpdate_TransitionToLocomotion` (caught after Phase 9d had merged without them — would have blocked OnStateEntry thunks calling them from the worker thread).
4. **`Update_PropertiesFromCharacter` deliberately NOT thread-safe** — it touches CMC + Controller state. Doc comment added: "GAME-THREAD ONLY — wire to Event Blueprint Update Animation, NOT thread-safe variant."

## ABP wiring required for first motion (the user-side Phase 9 work)

`AZ_ABP_Mover` (parent class `UAZ_AnimInstance`) needed all of these to actually animate:

### Class defaults (3 properties — set in Class Defaults panel)
- `CharacterAnimationChooser` = `CHT_AZ_CharacterAnimations`
- `LocomotionDatabaseChooser` = `CHT_NoWeapon_Locomotion`
- `CharacterMirrorDataTable` = `MDT_CHR_M16`

### EventGraph (3 wires)
- `Event Blueprint Initialize Animation` → `InitializeMoverPredictor`
- `Event Blueprint Update Animation` (game thread) → `Update_PropertiesFromCharacter`
- **Override `Blueprint Thread Safe Update Animation`** (worker thread) → `Update_Logic(DeltaTime)` — this is an override of the parent virtual, NOT an Event node added in EventGraph

### AnimGraph (already structurally correct from prior work)
GASP-pattern SM+BlendStack:
- State Controller (logic-only SM) → Inertialization → Two Way Blend.A
- standalone BlendStack (reads BlendStackInputs.* via thread-safe bindings) → Two Way Blend.B
- Two Way Blend Alpha = 1.0, **Always Update Children = true** → outputs only B (BlendStack pose), but ticks A (SM) so OnStateEntry fires
- → Procedural_PreLayering → AdditiveLeans → AimOffset → OffsetRootBone → Procedural → Pose History → Output

### State Machine "State Controller" — 9 states
TransitionToIdle, IdleLoop, IdleBreak, TransitionToLocomotion, LocomotionLoop, TransitionToInAir, InAirLoop, TransitionToSlide, SlideLoop

### 9 `OnStateEntry_*` thunks (the part that took the longest to figure out)
Each thunk needs THIS exact graph (3 nodes + the C++ call):
1. `K2Node_AnimNodeReference` pointing at the standalone BlendStack node → drives `BlendStackNode` pin
2. `K2Node_EvaluateChooser2` with Chooser Table = `CharacterAnimationChooser` (or set per state) and Result Type = AnimationAsset → drives `ChosenAnim` pin AND struct output drives `ChooserOut` pin
3. `SetBlendStackAnimFromChooser(State=<this state's enum>, bForceBlend=<true for transitions, false for loops>, BlendStackNode, ChosenAnim, ChooserOut)`

**A-pose root cause** (in case it happens again): if any of `BlendStackNode`/`ChosenAnim`/`ChooserOut` is unconnected, the C++ function bails with `bNoValidAnim=true` and BlendStackInputs.Anim stays null → BlendStack outputs ref pose → A-pose.

## Pawn wiring (verified, no change needed)
`AZ_BP_HeroPawn` Mesh component:
- `AnimClass` = `AZ_ABP_Mover_C` ✓
- `SkeletalMesh` = `SKM_SurvivalMan_Mesh3` (skeleton: `SKEL_SurvivalMan`)
- `AnimationMode` = `AnimationBlueprint`

## Reviewer-checklist lessons added this session
See `feedback_bp_to_cpp_port_review_checklist.md`:
- **Miss #1** (Phase 9d): UFUNCTION metadata parity — verify `BlueprintThreadSafe`, `BlueprintPure` flags match GASP function flags, not just body/logic.
- **Miss #2** (this session): UDS field type — `get_nodes` operation truncates `TArray<FName>` to `name`. Always use `get_node_pins` for full type.
- **Miss #3** (this session): `unreal_blueprint_query operation=inspect` returns an INCOMPLETE function list — parent-class virtual overrides (e.g., `BlueprintThreadSafeUpdateAnimation`) and certain function types are silently omitted. Treat the inspect functions array as a lower bound, never as authoritative for "X is missing" claims. Verify with `get_nodes graph_name=...` per-graph.

## Branch / commit state
- Branch: `feature/rootmotion`
- Commit (this session): C++ + ABP + BP class only (per user request — animation .uasset edits left out)
