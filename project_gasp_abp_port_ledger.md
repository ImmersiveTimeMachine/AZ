---
name: project_gasp_abp_port_ledger
description: GASP SandboxCharacter_Mover_ABP → UAZ_AnimInstance C++ port ledger. Every GASP variable and function mapped to AZ status (exists/rename/add/skip). Source of truth for reviewers per-phase.
type: project
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# GASP AnimBP → UAZ_AnimInstance Port Ledger

**Worker:** Opus 4.7 (main session)
**Reviewers:** Haiku 4.5 (Reviewer A = live BP parity via MCP; Reviewer B = memory + this ledger)

**Source:** `/Game/Blueprints/SandboxCharacter_Mover_ABP` (107 vars, 63 functions, 1092 nodes, parent = plain AnimInstance)
**Target:** `C:\UnrealEngine\Games\AZ\Source\AZ\Public\Animation\AZ_AnimInstance.h` + `.cpp`

## Decisions (locked 2026-04-24)

1. **Rename `CharacterTrajectory` → `Trajectory`** (GASP parity). All callsites updated in Phase 1.
2. **`MM Search Cost`**: C++ identifier `SearchCost`, BP DisplayName meta `"MM Search Cost"`. (We also have a per-SM-path `SearchCost`; disambiguate as `MMSearchCost` with display name.)
3. **Skip ALL debug logic.** No DebugTransitions, no history arrays (TransitionHistory, PawnSpeedHistory, MoveData_Speed_History, Phase_History, Contact_L_History, Contact_R_History, Enable_Warping_History), no `DebugDraws()`, no `FootPlacement_Debug`. Drop these entirely from the port.
4. **Enable `UseThreadSafeUpdateAnimation`**: route through `BlueprintThreadSafeUpdateAnimation(float DeltaTime)` — property reads must be thread-safe (PostSim data from pawn).

## Variable Ledger (GASP 107 → AZ)

### States — 6 enums × 5 vars (30 total)
All 30 exist in AZ_AnimInstance.h. **Status: ✅ COMPLETE.** No action Phase 1.

| GASP | AZ | Status |
|---|---|---|
| MovementMode, _LastFrame, _Recent, _Time, _LastStateTime | same | ✅ |
| RotationMode, _LastFrame, _Recent, _Time, _LastStateTime | same | ✅ |
| MovementState, _LastFrame, _Recent, _Time, _LastStateTime | same | ✅ |
| Gait, _LastFrame, _Recent, _Time, _LastStateTime | same | ✅ |
| Stance, _LastFrame, _Recent, _Time, _LastStateTime | same | ✅ |
| MovementDirection, _LastFrame, _Recent, _Time, _LastStateTime | same | ✅ |

### Essential Values (14)
| GASP | AZ | Status |
|---|---|---|
| CharacterProperties | CharacterProperties | ✅ |
| CharacterTransform | CharacterTransform | ✅ |
| CharacterTransform_LastFrame | CharacterTransform_LastFrame | ✅ |
| RootTransform | RootTransform | ✅ |
| HasAcceleration | bHasAcceleration | ✅ |
| Acceleration | Acceleration | ✅ |
| Acceleration_LastFrame | Acceleration_LastFrame | ✅ |
| AccelerationAmount | AccelerationAmount | ✅ |
| HasVelocity | bHasVelocity | ✅ |
| Velocity | Velocity | ✅ |
| Velocity_LastFrame | Velocity_LastFrame | ✅ |
| RelativeAcceleration | RelativeAcceleration | ✅ |
| VelocityAcceleration | VelocityAcceleration | ✅ |
| LastNonZeroVelocity | LastNonZeroVelocity | ✅ |
| Speed2D | Speed2D | ✅ |
| HeavyLandSpeedThreshold | HeavyLandSpeedThreshold | ✅ |
| SmoothedGroundNormal | SmoothedGroundNormal | ✅ |
| **InteractionTransform** | — | ⚠ ADD (Phase 1) |

### Trajectory (11)
| GASP | AZ | Status |
|---|---|---|
| Predictor (MoverTrajectoryPredictor*) | — (lazy local) | ⚠ PROMOTE to UPROPERTY (Phase 1) |
| Trajectory (FTransformTrajectory) | CharacterTrajectory | ⚠ RENAME to Trajectory (Phase 1) |
| TrajectoryCollision (FPoseSearchTrajectory_WorldCollisionResults) | — | ⚠ ADD (Phase 1) |
| PreviousDesiredControllerYaw | PreviousDesiredControllerYaw (private) | ⚠ PROMOTE to UPROPERTY |
| Trj_PastVelocity | Trj_PastVelocity | ✅ |
| Trj_NearFutureVelocity | Trj_NearFutureVelocity | ✅ |
| Trj_FutureVelocity | Trj_FutureVelocity | ✅ |
| Trj_PreviousFutureVelocity | Trj_PreviousFutureVelocity | ✅ |
| Trj_FutureFacing | Trj_FutureFacing | ✅ |
| Trj_TurnAngle | Trj_TurnAngle | ✅ |
| Trj_PastAngularVelocity (Vector in GASP) | Trj_PastAngularVelocity (float in AZ) | ⚠ FIX TYPE to FVector (Phase 1) |
| Trj_CurrentAngularVelocity (Vector in GASP) | Trj_CurrentAngularVelocity (float in AZ) | ⚠ FIX TYPE to FVector (Phase 1) |
| Trj_IsCircling | Trj_IsCircling | ✅ |
| Trj_CirclingTime | Trj_CirclingTime | ✅ |
| FutureFacingDelta | FutureFacingDelta | ✅ |
| FutureFacingDelta_LastFrame | FutureFacingDelta_LastFrame | ✅ |

### Motion Matching (6)
| GASP | AZ | Status |
|---|---|---|
| MMDatabaseLOD (int32) | — | ⚠ ADD (Phase 1) |
| ValidDatabases (TArray<UPoseSearchDatabase*>) | — | ⚠ ADD (Phase 1) |
| CurrentSelectedDatabase (UPoseSearchDatabase*) | CurrentLocomotionDatabase | ⚠ RENAME (Phase 1) |
| CurrentSelectedAnim (UObject*) | — | ⚠ ADD (Phase 1) |
| CurrentDatabaseTags (TArray<FName>) | CurrentDatabaseTags | ✅ |
| MM Search Cost (float) | — | ⚠ ADD as `MMSearchCost` with DisplayName="MM Search Cost" (Phase 1) |

### State Machine Experimental (7)
| GASP | AZ | Status |
|---|---|---|
| ValidAnims (TArray<AnimationAsset*>) | ValidAnims | ✅ |
| BlendStackInputs (S_BlendStackInputs) | BlendStackInputs (FAZ_BlendStackInputs) | ✅ |
| Previous_BlendStackInputs | Previous_BlendStackInputs | ✅ |
| StateMachineState (byte) | StateMachineState (EAZ_StateMachineState) | ✅ |
| NoValidAnim | bNoValidAnim | ✅ |
| NotifyTransition_Re-Transition | bNotifyTransition_ReTransition | ✅ |
| NotifyTransition_ToLoop | bNotifyTransition_ToLoop | ✅ |
| FutureFacingOnTransitionStart (Rotator) | FutureFacingOnTransitionStart | ✅ |
| Search Cost (float) | SearchCost | ✅ |

### Aim Offset (5)
| GASP | AZ | Status |
|---|---|---|
| SmoothedAimTarget | SmoothedAimTarget | ✅ |
| AO (Vector2D) | AO | ✅ |
| Previous_AO | Previous_AO | ✅ |
| EnableAO (bool) | EnableAO | ✅ |
| In Out Angular Velocity (Vector) | InOutAngularVelocity | ✅ |

### Root Offset (2)
| GASP | AZ | Status |
|---|---|---|
| OffsetRootBoneEnabled | bOffsetRootBoneEnabled | ✅ |
| OffsetRootTranslationRadius | OffsetRootTranslationRadius | ✅ |

### Foot Placement (5) + Procedural (1)
| GASP | AZ | Status |
|---|---|---|
| PlantSettings_Default (FFootPlacementPlantSettings) | — | ⚠ ADD (Phase 1) |
| PlantSettings_Stops | — | ⚠ ADD (Phase 1) |
| InterpolationSettings_Default (FFootPlacementInterpolationSettings) | — | ⚠ ADD (Phase 1) |
| InterpolationSettings_Stops | — | ⚠ ADD (Phase 1) |
| FootPlacement_Enable | bFootPlacementEnabled | ✅ (rename internal OK) |
| FootPlacement_Debug | — | ❌ SKIP (debug) |
| FootPlacementMode (int32) | — | ⚠ ADD (Phase 1) |
| TeleportThreshold (double) | — | ⚠ ADD (Phase 1) |
| ForceFootPlacementReset | bForceFootPlacementReset | ✅ |

### Additive Lean (1)
| GASP | AZ | Status |
|---|---|---|
| LeanAmount (Vector2D) | LeanAmount | ✅ |
| LateralAccelerationAmount | LateralAccelerationAmount | ✅ (extra, not in GASP but used) |

### Default (5)
| GASP | AZ | Status |
|---|---|---|
| HasOwningActor | — | ⚠ ADD (Phase 1) |
| HasMover | — | ⚠ ADD (Phase 1) |
| UseThreadSafeUpdateAnimation | — | ⚠ ADD (Phase 1) as bUseThreadSafeUpdateAnimation |
| LocomotionSetup (int32) | LocomotionSetup | ✅ |
| Mover (MoverComponent*) | — | ⚠ ADD (Phase 1) — cached ref |

### Debug (8) — ALL SKIPPED
DebugTransitions, TransitionHistory, PawnSpeedHistory, MoveData_Speed_History, Phase_History, Contact_L_History, Contact_R_History, Enable_Warping_History → ❌ SKIP per user decision.

## Function Ledger (GASP 63 → AZ)

Legend: ✅ exists • ⚠ refactor/rename • ⚠ ADD • ❌ skip (debug)

### Orchestration (5)
| GASP Function | AZ Status | Phase |
|---|---|---|
| EventGraph (init + update dispatch) | folded into NativeInitializeAnimation/NativeUpdateAnimation | ⚠ refactor Phase 2 |
| InitializeMoverPredictor | — | ⚠ ADD Phase 2 |
| Update_CVarDrivenVariables | — | ⚠ ADD Phase 2 |
| Update_PropertiesFromCharacter | inline in NativeUpdateAnimation | ⚠ extract Phase 2 |
| Update_Logic | inline in NativeUpdateAnimation | ⚠ extract Phase 2 |
| BlueprintThreadSafeUpdateAnimation(DeltaTime) | — | ⚠ ADD Phase 2 |

### Update pipeline (4)
| GASP | AZ | Phase |
|---|---|---|
| Update_Trajectory | inline | ⚠ extract Phase 2 |
| Update_EssentialValues | inline | ⚠ extract Phase 2 |
| Update_States | inline | ⚠ extract Phase 2 |
| Update_AimOffset | — (fields exist, no update logic) | ⚠ ADD Phase 3 |
| Update_AdditiveLean | — (field exists, no update logic) | ⚠ ADD Phase 4 |

### Motion Matching callbacks (6)
Decision 2026-04-24 (extends warping/procedural decision): functions binding to the
MotionMatching AnimGraph node stay in the ABP. Chooser eval + `SetDatabasesToSearch`
+ `GetDatabaseSearchResult` + `OverrideBlendOverride` all operate on the node ref.
Pure helpers (InterruptMode, BlendTime, NotifyRecencyTimeOut) stay in C++.

| GASP | AZ | Location |
|---|---|---|
| Update_MotionMatching(Context, Node) | — | **ABP Phase 9** |
| Update_MotionMatching_PostSelection(Context, Node) | — | **ABP Phase 9** |
| Get_MMInterruptMode | Get_MMInterruptMode | ✅ C++ |
| Get_MMBlendTime | Get_MMBlendTime | ✅ C++ |
| Get_MMNotifyRecencyTimeOut | Get_MMNotifyRecencyTimeOut | ✅ C++ (added Phase 5 — returns 0.2) |
| Get_PoseHistoryReference | — | **ABP Phase 9** (node-bound) |

### Transition condition helpers (8)
| GASP | AZ | Phase |
|---|---|---|
| IsMoving | IsMoving (inline lambda) | ⚠ reimplement Phase 2 (GASP body: FutureVel!=0 AND Accel!=0) |
| IsStarting | IsStarting | ✅ (audit vs GASP Phase 6) |
| IsPivoting | IsPivoting | ✅ (audit) |
| ShouldTurnInPlace | ShouldTurnInPlace | ✅ (audit) |
| ShouldSpinTransition | ShouldSpinTransition | ✅ (audit) |
| JustLanded_Light | JustLanded_Light | ✅ (audit) |
| JustLanded_Heavy | JustLanded_Heavy | ✅ (audit) |
| JustTraversed | JustTraversed | ✅ C++ Phase 6 (reads bJustTraversed member) |
| Get_LandVelocity | Get_LandVelocity | ✅ C++ Phase 6 (returns CharacterProperties.LandVelocity.Z) |

### SM State Controller functions (13)
Decision 2026-04-24 (consistent with Phase 5/7 C++/ABP split):
- **C++**: pure-logic helpers that don't need chooser asset, AnimNodeReference,
  or MotionMatch library calls.
- **ABP (Phase 9)**: SetBlendStackAnimFromChooser (needs CHT asset + MotionMatch),
  all 9 OnStateEntry_* thunks (take FAnimNodeReference), OnUpdate_TransitionToLocomotion
  (FAnimNodeReference), IsAnimationAlmostComplete (needs GetRelevantAnimTimeRemaining
  from BlendStack node), Get_DynamicPlayRate (takes BlendStackInput AnimNodeReference).

| GASP | AZ | Location |
|---|---|---|
| SetBlendStackAnimFromChooser(SMState, ForceBlend) | — | **ABP Phase 9** |
| IsAnimationAlmostComplete | — | **ABP Phase 9** (node-bound) |
| OnStateEntry_IdleLoop | — | **ABP Phase 9** |
| OnStateEntry_IdleBreak | — | **ABP Phase 9** |
| OnStateEntry_TransitionToIdle | — | **ABP Phase 9** |
| OnStateEntry_LocomotionLoop | — | **ABP Phase 9** |
| OnStateEntry_TransitionToLocomotion | — | **ABP Phase 9** |
| OnUpdate_TransitionToLocomotion | — | **ABP Phase 9** |
| OnStateEntry_InAirLoop | — | **ABP Phase 9** |
| OnStateEntry_TransitionToInAir | — | **ABP Phase 9** |
| OnStateEntry_SlideLoop | — | **ABP Phase 9** |
| OnStateEntry_TransitionToSlide | — | **ABP Phase 9** |
| Get_DynamicPlayRate(BlendStackInput) | — | **ABP Phase 9** |

### Warping / Procedural (10)
Decision 2026-04-24: functions taking `AnimNodeReference` stay in the ABP (Phase 9),
not C++. Reason: trivial bodies (2–4 nodes each), tightly coupled to node internals,
native BP APIs (UAnimationBlueprintLibrary, UBlendStackAnimNodeLibrary) already cover
them. C++ wrappers would just indirect into BP. Phase 7 is collapsed into Phase 8.

| GASP | AZ | Location |
|---|---|---|
| Get_DesiredFacing(Node) | — | **ABP Phase 9** |
| EnableSteering(Node) | — | **ABP Phase 9** |
| Get_OrientationWarpingWarpingSpace | — | **ABP Phase 9** (simple getter, co-located with WarpingNode) |
| Get_StrafeWarpDirection | — | **ABP Phase 9** |
| Get_StrideWarpAlpha(Node) | — | **ABP Phase 9** |
| Get_StrafeWarpAlpha(Node) | — | **ABP Phase 9** |
| Get_ProceduralTargetTime(Node) | — | **ABP Phase 9** |
| Biped_FootPlacement_OnBecomeRelevant | — | **ABP Phase 9** |
| AllowFootPinning | AllowFootPinning | ✅ C++ |
| AllowSlopeWarping | AllowSlopeWarping | ✅ C++ |
| JustTeleported | JustTeleported | ✅ C++ |

### Slide helpers (2)
| GASP | AZ | Status |
|---|---|---|
| Get_SlideSlopeRotation | Get_SlideSlopeRotation | ✅ |
| Get_SlideSlopeOffset | Get_SlideSlopeOffset | ✅ |

### Offset root bone getters (4)
| GASP | AZ | Status |
|---|---|---|
| Get_OffsetRootTranslationMode | same | ✅ |
| Get_OffsetRootRotationMode | same | ✅ |
| Get_OffsetRootTranslationHalfLife | same | ✅ |
| Get_OffsetRootTranslationRadius | same | ✅ |

### Utility getters (2)
| GASP | AZ | Phase |
|---|---|---|
| Get_TrajectoryTurnAngle | — | ⚠ ADD Phase 8 (trivial: return Trj_TurnAngle) |
| Get_TotalFacingDelta(Times) | exists in Update_Trajectory (uses) | ⚠ extract as public helper Phase 8 |

### Debug / skipped (2)
| GASP | AZ | Status |
|---|---|---|
| DebugDraws | — | ❌ SKIP |
| AnimGraph (graph, not function) | — | ❌ N/A (ABP concern) |

### AnimGraph-referenced functions (callbacks used inside graph)
Keep as class members for the ABP child to wire node pins to:
- Procedural, Procedural_PreLayering, AdditiveLeans, AimOffset — these are **empty "Anim Layer" placeholder functions** in GASP. They exist so the ABP graph has labeled anim layers. Not needed in C++; will be re-introduced in the ABP child (Phase 9 user-side).

## Phase Execution Order

| # | Scope | Primary deliverable |
|---|---|---|
| 0 | This ledger | project_gasp_abp_port_ledger.md |
| 1 | Variables | ~18 new/renamed UPROPERTYs, type fixes |
| 2 | Update pipeline refactor | 8 helper functions, thread-safe dispatch |
| 3 | Update_AimOffset | spring damp + AO compute + EnableAO |
| 4 | Update_AdditiveLean | lean Vector2D from Accel |
| 5 | MM callbacks | Update_MotionMatching + PostSelection + PoseHistory |
| 6 | SM State Controller | SetBlendStackAnimFromChooser + 9 OnStateEntry_* + helpers |
| 7 | (collapsed into 8) | — |
| 8 | Finalize | Get_TrajectoryTurnAngle + Get_TotalFacingDelta public + final LC build |
| 9 | USER: ABP creation | child ABP inheriting UAZ_AnimInstance + 7 warping/procedural helpers as BP |
| 10 | GAS reintegration | aim/shoot/reload overlay |

## Per-phase review gate

1. Worker commits code
2. LC compile: `LiveCoding.Compile` → scrape `LogLiveCoding` for errors
3. Reviewer A (Haiku) queries GASP BP via MCP, produces APPROVE/REJECT with cite
4. Reviewer B (Haiku) reads this ledger + phase spec, produces APPROVE/REJECT with cite
5. Both APPROVE → next phase. One REJECT → fix → re-review.

## Renames to execute in Phase 1 (callsite sweep)

| Old | New | Callsites |
|---|---|---|
| CharacterTrajectory | Trajectory | NativeUpdateAnimation, Update_Trajectory (when extracted), any BP refs to break |
| CurrentLocomotionDatabase | CurrentSelectedDatabase | NativeUpdateAnimation, MM callbacks |
| Trj_PastAngularVelocity (float → FVector) | same name, type change | Update_Trajectory |
| Trj_CurrentAngularVelocity (float → FVector) | same name, type change | Update_Trajectory |

BP references are concerning — the only consumer currently is `AZ_ABP_HeroPawn`. The user will create a new ABP in Phase 9 so we can clean-rename here freely. Existing ABP is the fallback; if it breaks on rename, it's acceptable because new ABP supersedes it.
