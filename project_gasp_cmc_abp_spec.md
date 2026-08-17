---
name: project-gasp-cmc-abp-spec
description: "Full teardown of GASP 5.8 SandboxCharacter_CMC_ABP (MM-node path) as the build spec for AZ's new CMC hero ABP — contract struct, graph spine, function surface, required curves, and the database-density blocker."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-17T02:52:20.264Z
---

Analysis of `/Game/GameAnimationSample/Blueprints/SandboxCharacter_CMC_ABP` (2026-08-16), done AFTER the
decision to target **path 0 (the Motion Matching node)** rather than path 1 (which Epic itself labels
"highly experimental… the current workflow is far from ideal… to inform future tool development").

Asset facts: target skeleton `SK_UEFN_Mannequin`, native parent **plain `UAnimInstance`** (all logic in BP),
`RootMotionMode = ROOT_MOTION_FROM_MONTAGES_ONLY`. Supersedes nothing in
[[project-gasp58-update-audit]] — that file's ABP section stays valid, this adds build-level detail.

## 1. The pawn→anim contract (ONE interface call per frame)

`Get_PropertiesForAnimation` returns `S_CharacterPropertiesForAnimation` — 19 fields, the ENTIRE seam:

```
ActorTransform          Gait (E_Gait)              MovementMode (E_MovementMode)
Velocity                Stance (E_Stance)          MovementDirection (E_MovementDirection)
InputAcceleration       RotationMode (E_RotationMode)   OrientationIntent
AimingRotation          InputState (S_PlayerInputState) SteeringTime
CurrentMaxAcceleration  GroundLocation             JustLanded
CurrentMaxDeceleration  GroundNormal               LandVelocity
BasedMovementDelta
```

This is the model for AZ's C++ seam: `UAZ_CmcAnimInstance` fills one struct; everything else is BP.
AZ already has: Gait, Stance, MovementMode, Velocity, AimingRotation (`GetAimRotation`), JustLanded +
LandVelocity (added 2026-08-16). Missing: CurrentMaxAccel/Decel (the feel pass computes both — just
publish them), GroundLocation/Normal (CMC `CurrentFloor`), BasedMovementDelta, OrientationIntent,
RotationMode, SteeringTime, MovementDirection, InputState, ActorTransform, InputAcceleration.

## 2. AnimGraph spine (verified by node export, not inferred)

```
MotionMatching node ──► DefaultSlot (ONLY slot) ──► OffsetRootBone ──► RemapCurves
   └ internal BlendStack Graph:                          ──► FootPlacement ──► LegIK
     OrientationWarping + Steering                       ──► PoseSearchHistoryCollector ──► Root
```

- **PoseSearchHistoryCollector** (last node before Root): CollectedBones = `foot_r, foot_l, thigh_r,
  thigh_l, spine_05, pelvis`; CollectedCurves = `Phase`; `SamplingInterval=0` (collect every update);
  `RootBoneRecoveryTime=0.3`. Its `TransformTrajectory` pin is **property-bound** to the `Trajectory`
  variable via thread-safe property access (not a wire).
- **RemapCurves** expression, literally: `contact_l=(1-contact_l)*100` / `contact_r=(1-contact_r)*100`.
- Warping lives INSIDE the MM node's BlendStack graph, applied per selected animation — not in the spine.

## 3. Function surface (~45 BP functions) — the real content of the ABP

- **Data in**: `Get_PropertiesForAnimation`, `Update_PropertiesFromCharacter`, `Update_EssentialValues`
- **Trajectory**: `Update_Trajectory`, `Get_Trajectory`, `HandleTransformTrajectoryWorldCollisions`,
  `Get_TrajectoryTurnAngle`
- **MM control**: `Update_MotionMatching` (evaluates a Chooser → **array of databases**),
  `Update_MotionMatching_PostSelection` (caches selected DB so its tags can be read — tags are grabbed in
  the event graph due to a thread-safety issue → `CurrentDatabaseTags`), `Get_MMBlendTime`,
  `Get_MMInterruptMode`, `Get_MMNotifyRecencyTimeOut`, `OnMotionMatchingStateUpdated(Function)`
- **Pose history**: `Get_PoseHistory`, `Get_PoseHistoryReference`
- **Post-chain tuning**: `Get_OffsetRootRotationMode`, `Get_OffsetRootTranslationMode`,
  `Get_OffsetRootTranslationHalfLife`, `Get_OffsetRootTranslationRadius`,
  `Get_OrientationWarpingWarpingSpace`, `Get_DesiredFacing`, `Get_StrafeYawRotationOffset`,
  `Get_FootPlacementInterpolationSettings`, `Get_FootPlacementPlantSettings`, `Get_DynamicPlayRate`
- **Derived state**: `Update_States`, `Update_Logic`, `Update_MovementDirection`, `Update_TargetRotation`,
  `IsMoving`, `IsStarting`, `IsPivoting`, `OnGround`, `ShouldTurnInPlace`, `ShouldSpinTransition`,
  `CalculateDirection`, `CalculateRelativeAccelerationAmount`, `Get_Gait`, `Get_LeanAmount`, `Get_AO_Yaw`,
  `Get_AOValue`, `Get_MovementDirectionThresholds`
- **Misc / PSI**: `Update_CVarDrivenVariables`, `IsSlotActive`, `IsAnimationAlmostComplete`,
  `IsCurrentAssetLooping`, `Set_NotifyTransition_ToLoop/_ReTransition`, `Update_MMIConstraints`,
  `Get_InteractionTransform`, `Set_InteractionTransform`

Chooser outputs go through `S_ChooserOutputs`; BlendStack inputs through `S_BlendStackInputs`.

## 4. Curves the CLIPS must carry (else the graph is inert)

`Enable_Warping`, `Enable_Warping_History`, `Enable_TurnInPlaceSteering`, `Enable_AO` / `Disable_AO`,
`MoveData_Speed`, `MinDynamicPlayRate`, `MaxDynamicPlayRate`, `Phase`, `contact_l`, `contact_r`.
Dynamic play rate = `lerp(1, clamp(Speed2D / MoveData_Speed, Min, Max), Enable_Warping)` — so
`MoveData_Speed` must be authored per clip or play rate is meaningless.

## 5. ★ THE BLOCKER — database density (measured 2026-08-16)

| | AZ | GASP |
|---|---|---|
| PoseSearch databases | 17 (incl. dupes/`Old`) | **160** |
| clips inside databases | **~125 total** | 253 in just the first 14 |
| `PSD_Dense_Crouch_Walk_Pivots` | — | **131 clips** |
| Starts / Stops / Pivots / TurnInPlace DBs | **NONE** | dense, per gait + foot phase |

The MM node has no state logic — it trusts that some clip matches any trajectory. Ours is almost all
loops (Run 12 / Walk 14 / Crouch 9 / three strafe sets of 8), so a straight swap to the MM node would
resolve every query to the nearest loop: no plant on stops, no pivots, no TIP. **It would be a downgrade.**

**BUT this is an ingestion gap, not a content gap.** The clips exist on `SKEL_SurvivalMan`, outside any
database: `LM_RM_RunFwdStart90_L/135_L/180_R`, `RunFwdStop_LU/RU`, `RunFwdTurn180_L_LU`, Walk + Crouch
equivalents, `StrafeLeftStart/Stop`, `TurnLt90/180`, `TurnRt90/180`. `UAZ_PoseSearchUtils` exists precisely
to add anims to databases (the engine doesn't expose it), so ingestion is automatable.

Order of work is therefore: **ingest → density → then the MM node earns its keep.** Not the reverse.

## 6. What AZ keeps regardless

Chooser tables survive the switch — in path 0 the chooser outputs an **array of databases** (pool filter)
instead of a single clip. Keep `UAZ_LocomotionStateMachine` in C++ as a **gameplay** state provider that no
longer drives pose, so combat keeps its deterministic `EAZ_StateMachineState` while MM owns selection.

Related: [[project-gasp58-update-audit]], [[project-cmc-backport-spike]], [[feedback-metahuman-modular-hero]]
