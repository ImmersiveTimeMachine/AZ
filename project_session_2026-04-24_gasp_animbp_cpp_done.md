---
name: project_session_2026-04-24_gasp_animbp_cpp_done
description: Session 2026-04-24 — GASP SandboxCharacter_Mover_ABP C++ port complete (Phases 0-8). UAZ_AnimInstance now has all pure-logic parity with GASP ABP. User creates ABP in Phase 9.
type: project
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# Session 2026-04-24 — GASP AnimBP → C++ Port Done

## Update 2026-04-25 — Phase 9 (full C++ port) COMPLETE

User reversed the C++/ABP split decision and ported the previously-deferred node-bound functions to C++ via 4 sub-phases (9a-9d), all dual-reviewer-APPROVED.

**Phase 9a — Build.cs + Chooser asset UPROPERTYs:**
- Added `BlendStack`, `AnimationWarpingRuntime`, `AnimGraphRuntime` to PublicDependencyModuleNames
- Added 3 UPROPERTYs: `CharacterAnimationChooser`, `LocomotionDatabaseChooser`, `CharacterMirrorDataTable`

**Phase 9b — Warping/Procedural helpers (9 funcs):**
- Get_DynamicPlayRate(BlendStackInput) — speed/curve play rate + circling bonus
- Get_DesiredFacing(Node) — Quat target rotation for steering
- EnableSteering(Node) — bool gate for steering enable
- Get_OrientationWarpingWarpingSpace() — RootBoneTransform vs ComponentTransform
- Get_StrafeWarpDirection() — lerp(LastNonZeroVelocity, Trj_NearFutureVelocity, alpha)
- Get_StrideWarpAlpha(Node) — Enable_Warping + Enable_StideWarping curves (note GASP typo)
- Get_StrafeWarpAlpha(Node) — Enable_Warping + Enable_StrafeWarping curves
- Get_ProceduralTargetTime(Node) — MapRange(SteeringTargetTime, [0..1], [0.1..0.3])
- Biped_FootPlacement_OnBecomeRelevant — sets bForceFootPlacementReset

**Phase 9c — Motion Matching callbacks (2 of 3):**
- Update_MotionMatching — eval LocomotionDatabaseChooser → ValidDatabases, SetDatabasesToSearch with InterruptMode
- Update_MotionMatching_PostSelection — cache CurrentSelectedAnim/Database/MMSearchCost/CurrentDatabaseTags, override blend HermiteCubic 0.2s no-inertial
- (Get_PoseHistoryReference STAYS in ABP — K2Node_AnimNodeReference can't be ported)

**Phase 9d — SM State Controller (Option A hybrid):**
- SetBlendStackAnimFromChooser(State, ForceBlend, BlendStackNode, ChosenAnim, ChooserOut) — full chooser+MM+populate logic in C++
- IsAnimationAlmostComplete(BlendStackNode) — !looping && remaining<=0.75
- OnUpdate_TransitionToLocomotion — RInterpTo(FutureFacingOnTransitionStart, Trj_FutureFacing, dt, 4.0)
- (9 OnStateEntry_* STAY in ABP — K2Node_AnimNodeReference can't be ported)

**ABP Phase 9 user-side work shrinks to:**
- AnimGraph node network (BlendStack node, MotionMatching node, OffsetRootBone, Inertialization, PoseHistory, BlendListByInt LocomotionSetup)
- State Machine "State Controller" (9 states + 29 transitions per gasp_sm_tip_flow.md)
- 9 OnStateEntry_* tiny thunks (each ~3 BP nodes: get BlendStack node by name + chooser eval + call C++ SetBlendStackAnimFromChooser)
- Get_PoseHistoryReference (5-node BP function)
- 4 anim layer placeholders (AdditiveLeans, AimOffset, Procedural, Procedural_PreLayering)
- EventGraph wiring (Init → InitializeMoverPredictor; Update → BlueprintThreadSafeUpdateAnimation/Update_Logic)

Late reviewer-driven corrections accepted (trivial body fixes for exact GASP parity):
- Phase 5 Get_MMNotifyRecencyTimeOut: gait-aware (0.16 Sprint, 0.2 else)
- Phase 6 JustTraversed: curve-based (MovingTraversal>1 AND DefaultSlot inactive)

Total: ~50 C++ functions, all dual-reviewer-APPROVED, LC compile green throughout.

---


## Summary

Completed all 8 C++ phases of the GASP `SandboxCharacter_Mover_ABP` → `UAZ_AnimInstance` port. Every phase was dual-reviewer-gated (Haiku model, Reviewer A = live GASP BP via MCP, Reviewer B = ledger/memory). All phases APPROVED. Live Coding build green throughout.

## Scope decisions locked this session

1. **Rename** `CharacterTrajectory` → `Trajectory`, `CurrentLocomotionDatabase` → `CurrentSelectedDatabase`, `PreviousLocomotionDatabase` → `PreviousSelectedDatabase` (match GASP exactly).
2. **DisplayName meta** `MMSearchCost` identifier in C++ with DisplayName="MM Search Cost" (spaces in GASP BP name preserved for display).
3. **Skip ALL debug** — no DebugTransitions, no history arrays, no DebugDraws(), no bFootPlacementDebug.
4. **Thread-safe update** enabled (bUseThreadSafeUpdateAnimation = true default).
5. **C++/ABP split** (introduced mid-session): functions taking `FAnimNodeReference` or requiring Chooser asset / MotionMatching library / node introspection stay in the ABP (Phase 9). Only pure-logic helpers go in C++.

## Phase-by-phase deliverables

| # | Scope | Deliverable |
|---|---|---|
| 0 | Ledger | `project_gasp_abp_port_ledger.md` — 107 GASP vars + 63 fns mapped |
| 1 | Variables + renames | 18 new UPROPERTYs, 3 renames, 2 float→FVector type fixes (Trj_PastAngularVelocity, Trj_CurrentAngularVelocity) |
| 2 | Pipeline refactor | 7 helper functions extracted: InitializeMoverPredictor, Update_CVarDrivenVariables (stub), Update_PropertiesFromCharacter, Update_Logic, Update_Trajectory, Update_EssentialValues, Update_States. IsMoving body rewritten to GASP formula (FutureVel AND Accel, not Speed) |
| 3 | Update_AimOffset | Extracted as named method. Spring-damp + AO Vector2D + 4-gate EnableAO logic |
| 4 | Update_AdditiveLean | Extracted as named method. Velocity-space accel → LeanAmount Vector2D per MovementDirection |
| 5 | MM callbacks | `Get_MMNotifyRecencyTimeOut` (Sprint=0.16, else=0.2 per GASP). Rest → ABP Phase 9 |
| 6 | SM State Controller | `JustTraversed` (MovingTraversal curve + slot check), `Get_LandVelocity` (CharacterProperties.LandVelocity.Z). Rest → ABP Phase 9 |
| 7 | (collapsed into 8) | — |
| 8 | Finalize | `Get_TotalFacingDelta(const TArray<float>& Times)` extracted as public helper (used by Update_Trajectory). `Get_TrajectoryTurnAngle` inline getter |

## Files touched

- `C:\UnrealEngine\Games\AZ\Source\AZ\Public\Animation\AZ_AnimInstance.h` — full header expansion: 7 Update_* declarations, 18 new UPROPERTYs, renames, type fixes, new helpers (Get_MMNotifyRecencyTimeOut, JustTraversed, Get_LandVelocity, Get_TotalFacingDelta, Get_TrajectoryTurnAngle)
- `C:\UnrealEngine\Games\AZ\Source\AZ\Private\Animation\AZ_AnimInstance.cpp` — pipeline extraction, all 7 update helpers + 2 new transition helpers + Get_TotalFacingDelta impl. NativeUpdateAnimation trimmed to an orchestrator.

## ABP Phase 9 (next session — user creates)

Must be done in an AnimBP (derived from `UAZ_AnimInstance`):

**AnimGraph itself** (the pose-producing graph):
- State Machine "State Controller" (9 states, 29 transitions — see `gasp_sm_tip_flow.md`)
- BlendStack node receiving BlendStackInputs
- OffsetRootBone node (Translation=Interpolate, Rotation=Accumulate, HalfLife/Radius from Get_OffsetRoot* C++ getters)
- Inertialization node
- PoseHistory collector
- BlendListByInt dispatcher on LocomotionSetup (0=PureMM, 1=SM+BlendStack)

**Anim Layer functions** (empty placeholder hosts for subgraphs):
- `AdditiveLeans` — BS_*_Leans blend graph, filtered by `Disable_AdditiveLeans` curve
- `AimOffset` — BS_Neutral_AO_Stand_NoSmoothing with Dead Blending
- `Procedural` — CR_Biped_FootPlacement or FootPlacement+LegIK
- `Procedural_PreLayering` — anim layer slot

**Node-bound functions to implement as BP thunks:**
- `Update_MotionMatching(Context, Node)` — cast Node to MotionMatchingNode, eval Chooser (CHT_PoseSearchDatabases_Relaxed), call `SetDatabasesToSearch(Node, ValidDatabases, Get_MMInterruptMode())`
- `Update_MotionMatching_PostSelection(Context, Node)` — `GetDatabaseSearchResult` → store CurrentSelectedAnim/Database/MMSearchCost; `OverrideBlendOverride(Node, 0.2, HermiteCubic, false)`
- `Get_PoseHistoryReference`
- `SetBlendStackAnimFromChooser(StateMachineState, ForceBlend)` — eval CHT_MoverCharacterAnimations, if UseMM: single-frame MotionMatch; else: first anim
- 9 `OnStateEntry_*` — thunk to SetBlendStackAnimFromChooser
- `OnUpdate_TransitionToLocomotion`
- `IsAnimationAlmostComplete` — `GetRelevantAnimTimeRemaining(BlendStackNode, DefaultSlot) < 0.1`
- `Get_DynamicPlayRate(BlendStackInput)`
- 7 warping/procedural helpers (Get_DesiredFacing, EnableSteering, Get_OrientationWarpingWarpingSpace, Get_StrafeWarpDirection, Get_StrideWarpAlpha, Get_StrafeWarpAlpha, Get_ProceduralTargetTime, Biped_FootPlacement_OnBecomeRelevant)

**EventGraph wiring:**
- OnAnimationInitialize → `InitializeMoverPredictor()`
- BlueprintThreadSafeUpdateAnimation(DeltaTime) → `Update_CVarDrivenVariables()` → `Update_PropertiesFromCharacter()` → `Update_Logic(DeltaTime)`

**Properties the ABP needs:**
- `CharacterAnimationChooser` (UChooserTable ref) → CHT_MoverCharacterAnimations equivalent
- `LocomotionDatabaseChooser` (UChooserTable ref) → CHT_PoseSearchDatabases_Relaxed equivalent

## Review workflow used

1. Worker implements phase
2. Live Coding compile via MCP `LiveCoding.Compile` → scrape UBT log for errors
3. Reviewer A (Haiku) queries live GASP BP via MCP, compares contract to C++ impl
4. Reviewer B (Haiku) reads `project_gasp_abp_port_ledger.md` + phase spec, checks compliance
5. Both APPROVE → next phase

Two late corrections were made in response to Reviewer A findings (both accepted without re-review since they were trivial body fixes, not scope changes):
- Phase 5: `Get_MMNotifyRecencyTimeOut` was returning constant 0.2 — updated to match GASP's gait-dependent 0.16 (Sprint) / 0.2 (other).
- Phase 6: `JustTraversed` was returning a stored `bJustTraversed` bool — updated to GASP's actual formula `(GetCurveValue("MovingTraversal") > 1) AND (GetSlotMontageLocalWeight("DefaultSlot") <= 0)`.

## Compile status

Live Coding green after every phase. UBT final build: Result: Succeeded.

Legacy ABP warnings (`AZ_ABP_Hero`, `AZ_ABP_HeroPawn` can't find old-name properties `Character Trajectory`, `Current Locomotion Database`, type mismatches on Trj_*AngularVelocity) are **expected and acceptable** — those ABPs will be replaced in Phase 9 when user creates the new ABP.

## Commit state

Working tree has all Phase 0-8 changes. Not committed. Branch `feature/rootmotion` (same branch as yesterday's pawn port).
