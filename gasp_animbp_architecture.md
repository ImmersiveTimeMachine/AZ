---
name: gasp_animbp_architecture
description: GASP AnimBP architecture — dual locomotion (pure MM vs SM+BlendStack), Chooser-driven databases, layered post-processing, Mover trajectory
type: reference
---

# GASP AnimBP Architecture (SandboxCharacter_Mover_ABP)

**Path:** `/Game/Blueprints/SandboxCharacter_Mover_ABP`
**Parent Class:** AnimInstance (plain, not custom C++)
**Total Nodes:** 1092 across 64 graphs

## Dual Locomotion System

Two parallel paths selectable via `BlendListByInt` (driven by `LocomotionSetup` int):

### Path 0 — Pure Motion Matching
```
MotionMatching --> [internal BlendStack+OrientationWarping+Steering]
```
- MM node: BlendTime=0.5, NotifyRecencyTimeOut=0.2
- Internal BlendStack handles Orientation Warping + Steering

### Path 1 — State Machine + Chooser + BlendStack (Experimental)
```
StateMachine "State Controller" (logical, no pose output)
  + BlendStack (pose output)
  --> TwoWayBlend (alpha=1 passes only BlendStack)
  --> Inertialization
```
- SM is purely logical — outputs no pose, only drives BlendStack via OnStateEntry functions

### Merged Signal Chain (both paths)
```
BlendListByInt[LocomotionSetup]
  --> Procedural_PreLayering (Anim Layer)
  --> AdditiveLeans (Anim Layer)
  --> AimOffset (Anim Layer)
  --> Slot 'DefaultSlot'
  --> OffsetRootBone
  --> Procedural (Anim Layer — foot IK)
  --> PoseHistory
  --> Output Pose
```

## Key AnimGraph Nodes

| Node | Purpose |
|------|---------|
| AnimGraphNode_MotionMatching | Primary MM. BlendTime=0.5, NotifyRecencyTimeOut=0.2 |
| AnimGraphNode_StateMachine "State Controller" | Logical SM driving BlendStack |
| AnimGraphNode_BlendStack | Receives anim/time/loop/blend from Chooser |
| AnimGraphNode_Inertialization | Smooth blending for SM path |
| AnimGraphNode_OffsetRootBone | Translation=Interpolate, Rotation=Accumulate, HalfLife=0.2, MaxError=30 |
| AnimGraphNode_PoseSearchHistoryCollector | PoseHistory — placed AFTER all processing, before Output |

## Motion Matching Integration

**Update_MotionMatching (OnUpdate callback on MM node):**
1. Evaluates Chooser `CHT_PoseSearchDatabases_Relaxed` passing `self` as context
2. Chooser returns array of `PoseSearchDatabase*` based on current states
3. Calls `SetDatabasesToSearch()` on the MM node
4. Passes `InterruptMode` from `Get_MMInterruptMode()`

**Get_MMInterruptMode logic:**
- Default: `DoNotInterrupt` (let MM find naturally better match)
- On core state changes (MovementState, MovementMode transitions): `ForceInterrupt`

**PostSelection callback:**
- Gets search result: CurrentSelectedAnim, CurrentSelectedDatabase, SearchCost
- Overrides blend: BlendTime=0.2, HermiteCubic, no inertial blend

## SM "State Controller" States

| State | Enum | ForceBlend | Description |
|-------|------|------------|-------------|
| IdleLoop | 0 | false | Standing idle |
| LocomotionLoop | 1 | false | Walking/running cycles |
| InAirLoop | 2 | false | Airborne |
| TransitionToIdle | 3 | true | Stops/pivots into idle |
| TransitionToLocomotion | 4 | true | Starts/pivots into movement |
| TransitionToInAir | 5 | true | Jump/fall transitions |
| IdleBreak | 6 | true | Idle fidget animations |
| SlideLoop | 9 | false | Sliding |

## SetBlendStackAnimFromChooser — Core Function

1. Set StateMachineState from input
2. Cache Previous_BlendStackInputs
3. Reset transition bools
4. Evaluate Chooser `CHT_MoverCharacterAnimations` → returns:
   - Array of valid AnimationAsset*
   - S_ChooserOutputs: UseMM, MMCostLimit, StartTime, BlendTime, BlendProfile, Tags
5. If UseMM=true: single-frame MotionMatch() on ValidAnims → best match
6. If UseMM=false: use first anim with Chooser's timing params
7. ForceBlend logic for re-entry

## Trajectory (Mover-Specific)

Uses `MoverTrajectoryPredictor` (not CharacterTrajectoryComponent):
```
PoseSearchGenerateTrajectory(using Predictor):
  HistorySamplingInterval: 0.033
  TrajectoryHistoryCount: 15
  PredictionSamplingInterval: 0.1
  TrajectoryPredictionCount: 15
```
Then `HandleTrajectoryWorldCollisions` for gravity + traces.

Extracted values:
- Past velocity at [-0.3, -0.2]s
- Near future velocity at [0.1, 0.2]s
- Future velocity at [0.4, 0.5]s
- Future facing at 1.5s
- FutureFacingDelta sampled at [0, 0.25, 0.75, 1.5]s

## Key Variables

**States (all have _LastFrame, _Recent, _Time variants):**
- MovementMode, RotationMode, MovementState, Gait, Stance, MovementDirection

**Trajectory:** Predictor, Trajectory, TrajectoryCollision, Trj_PastVelocity, Trj_NearFutureVelocity, Trj_FutureVelocity, Trj_FutureFacing, Trj_TurnAngle, Trj_IsCircling

**MM:** MMDatabaseLOD, ValidDatabases, CurrentSelectedDatabase, CurrentSelectedAnim, MM Search Cost

**Layers:** LeanAmount (Vector2D), SmoothedAimTarget, AO (Vector2D), EnableAO

## Event Graph Flow

1. **Initialize**: Delay → InitializeMoverPredictor()
2. **Update**: GetPropertiesForAnimation() via BPI → Update_Logic()
3. **Update_Logic**: Update_Trajectory → Update_EssentialValues → Update_States → Update_AimOffset → Update_AdditiveLean

## Layered Post-Processing

### AdditiveLeans
- Body: BS_Relaxed_Walk_Leans + BS_Relaxed_Run_Leans (blended by speed)
- Head: BS_Relaxed_Lean_Head (suppressed when AO active)
- Filtered by MovementState (locomotion only) and MovementMode (grounded only)
- `Disable_AdditiveLeans` curve masks out during turns

### AimOffset
- BS_Neutral_AO_Stand_NoSmoothing (X=Yaw, Y=Pitch)
- Dead Blending for smooth reset
- 0.75s blend in, 1.5s blend out
- `Disable_AO` curve for per-bone masking

### Procedural (Foot IK)
- CR_Biped_FootPlacement (Control Rig) with pinning + slope warping
- OR basic FootPlacement+LegIK
- Selectable per movement mode

## AnimNotifies

### BP_NotifyState_EarlyTransition
- Placed on transition anims
- Checks E_EarlyTransition_Condition (gait change or unconditional)
- Routes to E_EarlyTransition_Destination (Re-Transition or ToLoop)
- Sets bools on ABP that SM reads for state transitions

### BP_NotifyState_MontageBlendOut
- Placed on traversal montages
- Checks E_TraversalBlendOutCondition (HasAcceleration, IsInAir, or unconditional)
- Calls MontageStopWithBlendSettings (HermiteCubic)

## AnimModifiers (Key Ones)
- AM_MoveData_Speed — bakes root motion speed curve
- AM_BakePhaseCurveFromFootstepNotifies — foot contact phase curve
- AM_OrientationWarpingAlpha — orientation warping enable curve
- AM_RateWarpingAlpha — rate warping alpha curve
- AM_FootSpeed_L/R — foot bone speed for plant detection

## CR_Biped_FootPlacement (Control Rig)
- Two-bone IK per leg
- Foot pinning (locks feet in world space)
- Slope warping
- Pelvis adjustment for reach
- Raycast ground detection
- Key settings: Roll Limit, MaxFootPinRadius, FootContactLockThreshold, PinYawLimitDegrees
