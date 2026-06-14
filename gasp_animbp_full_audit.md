---
name: GASP AnimBP Full Deep Audit
description: Complete reverse-engineering of /Game/Blueprints/SandboxCharacter_Mover_ABP — every AnimGraph node + property + binding, BlendStack inner graph (Steering/OW/SW non-defaults), all driver functions one-line summaries, SM states/transitions/conduits, OnStateEntry events. Authoritative parity reference for AZ_ABP_Mover/UAZ_AnimInstance.
type: reference
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# GASP AnimBP Full Deep Audit

Source: `/Game/Blueprints/SandboxCharacter_Mover_ABP` (parent: `AnimInstance`, GeneratedClass: `SandboxCharacter_Mover_ABP_C`).

23 top-level AnimGraph nodes, 40 BlendStack inner graph nodes, 9 SM states + 23 transitions + 1 Conduit + 1 CustomTransition graph, 63 functions, 107 variables.

## 1) Top-Level AnimGraph (pose flow)

`MotionMatching → Inertialization → TwoWayBlend(B=BlendStack) → BlendListByInt(LocomotionSetup, idx0=BlendStack-side) → LAL Procedural_PreLayering → LAL AdditiveLeans → LAL AimOffset → Slot 'DefaultSlot' → OffsetRootBone → LAL Procedural → Pose History → Output Pose`

### OffsetRootBone (GUID `0C0C1D9B...`, Tag `OffsetRoot`)

**Live values:**
- `EvaluationMode = Graph`
- `TranslationMode = Interpolate` (default; bound)
- `RotationMode = Accumulate` (default; bound)
- `TranslationHalflife = 0.200` (bound)
- `RotationHalfLife = 0.100`
- `MaxTranslationError = 30.0` (bound)
- `MaxRotationError = -1.0` (no clamp)
- `bClampToTranslationVelocity = False` (bound)
- `TranslationSpeedRatio = 0.5`
- `RotationSpeedRatio = 0.5`
- `bOnGround = True`
- `GroundNormal = (0,0,1)`
- `CollisionTestShapeRadius = 30.0`
- `CollisionTestShapeOffset = (0,0,60.0)`

**Pin bindings:**
- `RotationMode ← Get_OffsetRootRotationMode (func)`
- `TranslationMode ← Get_OffsetRootTranslationMode (func)`
- `bClampToTranslationVelocity ← IsMoving (func)`
- `TranslationHalflife ← Get_OffsetRootTranslationHalfLife (func)`
- `MaxTranslationError ← Get_OffsetRootTranslationRadius (func)`

### MotionMatching (GUID `C38E7907...`)

- `BlendTime = 0.5` (bound to `Get_MMBlendTime`)
- `BlendProfile = SK_UEFN_Mannequin:FastFeet_InstantRoot`
- `BlendOption = HermiteCubic`
- `PoseReselectHistory = 0.3`
- `PlayRate = (Min=0.85, Max=1.15)` (outer reselect rate; not the BS play rate)
- `bResetOnBecomingRelevant = True`
- `bShouldSearch = True`
- `bValidateResultAgainstAvailabilities = True`
- `bWarpUsingRootBone = True`
- `WarpingTranslationRatio = 1.0`
- `WarpingRotationRatio = 1.0`
- `bShouldFilterNotifies = True`
- `StitchBlendTime = 0.1`
- `StitchBlendMaxCost = 100.0`
- `MaxActiveBlends = 4`
- `bStoreBlendedPose = True`
- `NotifyRecencyTimeOut = 0.2` (bound to `Get_MMNotifyRecencyTimeOut`)
- `MaxBlendInTimeToOverrideAnimation = 0.03`
- `PlayerDepthBlendInTimeMultiplier = 1.1`
- `GroupRole = ExclusiveAlwaysLeader`
- `bOverridePositionWhenJoiningSyncGroupAsLeader = True`
- `EventToSearch.bUsePlayRateRangeOverride = True; PlayRateRangeOverride = (1,1)`
- `AlwaysDynamicProperties = ["Database"]`
- AnimNode functions: `OnUpdate = Update_MotionMatching`, `OnMotionMatchingStateUpdated = Update_MotionMatching_PostSelection`

### Pose History (GUID `BDEF837D...`, Tag `PoseHistory`)

- `PoseCount = 2`
- `CollectedBones = [foot_r, foot_l, thigh_r, thigh_l, spine_05, pelvis, hand_l, hand_r]`
- `CollectedCurves = ["Phase"]`
- `bResetOnBecomingRelevant = True`
- `RootBoneRecoveryTime = 0.3`
- `RootBoneTranslationRecoveryRatio = 1.0`
- `RootBoneRotationRecoveryRatio = 1.0`
- `TrajectorySpeedMultiplier = 1.0`
- `TrajectoryHistoryCount = 10`
- `TrajectoryPredictionCount = 8`
- `PredictionSamplingInterval = 0.4`
- `TrajectoryData.RotateTowardsMovementSpeed = 10.0`, `MaxControllerYawRate = 70.0`
- Pin bindings: `TransformTrajectory ← Trajectory (variable)`

### Slot 'DefaultSlot'
SlotName = "DefaultSlot" (no overrides)

### State Machine "State Controller" (GUID `24CFE1EB...`)
- `MaxTransitionsPerFrame = 3`
- `MaxTransitionsRequests = 32`
- `bSkipFirstUpdateTransition = True`
- `bReinitializeOnBecomingRelevant = True`
- `bCreateNotifyMetaData = True`
- `bAllowConduitEntryStates = True`

### Outer BlendStack (GUID `BE7289C0...`, Tag `State Machine Blend Stack`)

- `BlendTime = 0.2`
- `bLoop = True`
- `WantedPlayRate = 1.0`
- `MaxAnimationDeltaTime = -1.0`
- `BlendOption = QuadraticInOut`
- `MirrorDataTable = MDT_UEFN_Mannequin`
- `bResetOnBecomingRelevant = True`
- `StitchBlendTime = 0.1`, `StitchBlendMaxCost = 100.0`
- `MaxActiveBlends = 4`
- `bStoreBlendedPose = True`
- `NotifyRecencyTimeOut = 0.2`
- `PlayerDepthBlendInTimeMultiplier = 1.0`
- **All 5 inputs PIN-BOUND to `BlendStackInputs.*` struct fields**:
  - `AnimationAsset ← BlendStackInputs.Anim`
  - `AnimationTime ← BlendStackInputs.StartTime`
  - `bLoop ← BlendStackInputs.Loop`
  - `BlendTime ← BlendStackInputs.BlendTime`
  - `BlendProfile ← BlendStackInputs.BlendProfile`

### TwoWayBlend (GUID `3ED28074...`)

- `bAlphaBoolEnabled = True` (constant)
- **`bAlwaysUpdateChildren = True`** ← KEY: lets State Machine always tick (and write `BlendStackInputs`) even though only side B (BlendStack) reaches Output Pose
- `Alpha = 1.0`
- `AlphaScaleBiasClamp.InterpSpeedIncreasing/Decreasing = 10.0/10.0`

### Inertialization
- `bForwardRequestsThroughSkippedCachedPoseNodes = True`

### BlendListByInt
- 2 input poses
- `ActiveChildIndex ← LocomotionSetup (variable, int, CVar-driven)`

### Linked Anim Layers (4, all `self`-implemented)
- `Procedural_PreLayering` (input pin name `PreProcPose`)
- `AdditiveLeans` (input `PreLeanPose`)
- `AimOffset` (input `PreAimOffsetPose`)
- `Procedural` (input `InPose`) — POST-Layer, post-OffsetRoot

## 2) BlendStack INNER graph (`AnimationBlendStackGraph_0`, 40 nodes)

Per-anim graph applied to each animation in the Blend Stack.

Connection chain (reconstructed):
`BlendStackInput → Steering(TIP) → CompToLocal → CopyBone(foot_l→ik_foot_l) → CopyBone(foot_r→ik_foot_r) → LocalToComp → ResetRoot → OrientationWarping → CompToLocal → StrideWarping → LocalToComp → Steering(Locomotion) → BlendStackResult`

### Steering — TIP variant (GUID `236690B8...`):
- `alpha_input_type = Bool`
- `alpha_bool_enabled = True`
- `alpha_bool_blend.blend_in_time = 0.100`, `blend_out_time = 0.100`, `blend_option = Linear`
- `procedural_target_time = 0.200`
- `animated_target_time = 2.000`
- `mirrored = False`
- Pin bindings: `bAlphaBoolEnabled ← EnableSteering()`, `TargetOrientation ← Get_DesiredFacing()`, `ProceduralTargetTime ← Get_ProceduralTargetTime()`, `CurrentAnimAsset ← Get Current Blend Stack Anim Asset (BS lib)`, `CurrentAnimAssetTime ← Get Current Blend Stack Anim Asset Time`

### Steering — Locomotion variant (GUID `6110CD58...`):
- Same as TIP variant EXCEPT:
- `alpha_input_type = Float`
- `alpha_bool_blend.blend_in_time = 0`, `blend_out_time = 0`
- Pin bindings: `Alpha ← (function)` (continuous fade for steering during locomotion), `TargetOrientation ← Get_DesiredFacing`, `CurrentAnimAsset/Time ← BS-lib helpers`

> Comment on the second Steering: "A second steering node is needed for turn in places, since certain properties on this node are not yet pinnable. This will be removed in future releases."

### OrientationWarping (GUID `24C2DBE8...`):
- `alpha_input_type = Float`
- `target_time = 0.800`
- `min_root_motion_speed_threshold = 10.0`
- `locomotion_angle_delta_threshold = 135.0`
- `warping_space = RootBoneTransform`
- Pin bindings: `Alpha ← Get_StrafeWarpAlpha`. `WarpingSpace` follows the `Get_OrientationWarpingWarpingSpace` function on the linked-property side.

### StrideWarping (GUID `3CB2C154...`):
- `alpha_input_type = Float`
- `alpha_bool_blend.blend_option = HermiteCubic`
- `stride_direction = (1,0,0)`
- `min_root_motion_speed_threshold = 10.0`
- `floor_normal_direction.value = (0,0,1)` (WorldSpaceVector mode)
- `gravity_direction.value = (0,0,-1)` (WorldSpaceVector mode)
- Pin bindings: `Alpha ← Get_StrideWarpAlpha`, `StrideDirection ← Get_StrafeWarpDirection`

### ResetRoot (GUID `E372A42F...`):
- `alpha_input_type = Float`, `alpha = 1.0` (constant)

### CopyBone foot_l→ik_foot_l, CopyBone foot_r→ik_foot_r:
- `alpha = 1.0`, `alpha_bool_blend.blend_option = HermiteCubic`
- `copy_translation/rotation/scale = False` in struct; runtime values come from pin bindings

### LocalToComp/CompToLocal (×2 each), BSResult, BSInput: no overrides.

## 3) State Machine (9 states, 23 transitions, 1 Conduit, 1 CustomTransition)

### States
| State node | State graph | OnStateEntry / OnUpdate functions |
|---|---|---|
| AnimStateNode_0 | Idle Loop | `OnStateEntry_IdleLoop` → `SetBlendStackAnimFromChooser(IdleLoop, false)` |
| AnimStateNode_1 | Transition to Idle | `OnStateEntry_TransitionToIdle` → SetBSAFromChooser(TransitionToIdle, true) |
| AnimStateNode_2 | Transition to In Air | `OnStateEntry_TransitionToInAir` → SetBSAFromChooser(TransitionToInAir, true) |
| AnimStateNode_3 | Transition to Locomotion | `OnStateEntry_TransitionToLocomotion` (+ caches `FutureFacingOnTransitionStart = Trj_FutureFacing`) and `OnUpdate_TransitionToLocomotion` (RInterps stored facing toward live `Trj_FutureFacing`) |
| AnimStateNode_4 | Locomotion Loop | `OnStateEntry_LocomotionLoop` → SetBSAFromChooser(LocomotionLoop, false) |
| AnimStateNode_5 | Idle Break | `OnStateEntry_IdleBreak` → SetBSAFromChooser(IdleBreak, true) |
| AnimStateNode_6 | Transition to Slide | `OnStateEntry_TransitionToSlide` |
| AnimStateNode_7 | Slide Loop | `OnStateEntry_SlideLoop` |
| AnimStateNode_8 | In Air Loop | `OnStateEntry_InAirLoop` |
| AnimStateConduitNode_1 | Conduit (transition graph "Conduit") | (rule decides routing into multiple states) |

### Transitions (all Hermite-Cubic, CrossfadeDuration=0.20s)
Pulled live; the Start-Notify name doubles as the human label.

| Notify name | Priority | Likely route (per GASP topology) |
|---|---|---|
| Transition: Idle to Locomotion | 1 | Idle Loop → Transition to Locomotion |
| Transition: Locomotion To Idle | 1 | Locomotion Loop → Transition to Idle |
| Transition: Animation Finished | 1 | Transition state → its Loop (anim end) |
| Transition: No Valid Anim | 1 | (any) → Transition to Idle (NoValidAnim flag) |
| Transition: State Changed | 1 | (any) → Transition to Locomotion (re-select on state change) |
| Pivot! | 1 | Locomotion Loop → Transition to Locomotion (pivot re-enter) |
| Transition: State Changed (#19) | 1 | likely Locomotion re-entry |
| Transition: Pivot | 1 | dedicated pivot transition |
| (unnamed #15) | 1 | Conduit-related |
| Transition: Turn In Place | **2** | Idle Loop → Transition to Locomotion (TIP gate) |
| Transition: "Broke" rotation | **2** | breaks rotation start/pivot using `OnUpdate_TransitionToLocomotion` interp delta |
| Transition: Idle Break | 1 | Idle Loop → Idle Break |
| (unnamed #23) | 1 | Idle Break → Idle Loop |
| (unnamed #10, has CustomTransition) | 1 | special cross-fade transition |
| Transition: to Locomotion | 1 | Conduit → Transition to Locomotion |
| Transition: to Idle | 1 | Conduit → Transition to Idle |
| Transition: to Slide | 1 | (any) → Transition to Slide |
| Transition: to In Air | 1 | (any) → Transition to In Air |
| Transition: Started Circling | 1 | Locomotion Loop → re-select (Trj_IsCircling true) |
| Transition: WrongWay! | **3** | reversal-direction transition (highest prio) |
| (unnamed #29) | 1 | Slide → Transition to Idle (likely) |

> The five `_LastFrame`/`_Recent`/`_Time`/`_LastStateTime` quintuplets per state enum are essential to drive the "transition on state change" rules — these are EXACTLY the variables AZ was missing per gasp_update_logic_flow.md.

## 4) Driver functions — one-line behavior summaries

### EventGraph (3 events)
- **Event Blueprint Initialize Animation** → `Delay Until Next Tick` → `InitializeMoverPredictor`
- **Event Blueprint Update Animation** → `Branch (HasOwningActor && HasMover)` → `Update_CVarDrivenVariables` → `Update_PropertiesFromCharacter` → `Branch (NOT UseThreadSafeUpdateAnimation)` → `Update_Logic`
- **Event Blueprint Post Evaluate Animation** → `Branch (HasOwningActor)` → `Set ForceFootPlacementReset=false` → `DebugDraws`. Contains a Composite "SM Transition Debug Events".

### Update_Logic sequence (executed from BPE_Update_Animation when NOT thread-safe)
1. `Update_EssentialValues` — caches Acceleration/Velocity/CharTransform; computes AccelerationAmount, Speed2D, HasAcceleration, HasVelocity; updates LastNonZeroVelocity (only when HasVelocity); VelocityAcceleration=(V-V_lastFrame)/DT; computes RootTransform from OffsetRootBone node when `OffsetRootBoneEnabled` else from CharacterTransform; computes RelativeAcceleration = UnrotateVector(SafeDivide(Accel, MaxAccel), RootTransform.Rotation); SmoothedGroundNormal = VInterpTo to CharProps.GroundNormal.
2. `Update_Trajectory` — `PoseSearchGenerateTrajectory(usingPredictor)`; `HandleTrajectoryWorldCollisions` for fall; `GetTrajectoryVelocity` × 3 for Past/NearFuture/Future velocities; `Trj_FutureFacing = Rotator(GetTrajectorySampleAtTime(Trajectory, ~0.5).Facing)`; `Trj_TurnAngle = Get_TrajectoryTurnAngle()`; angular velocities Past/Current via `GetTrajectoryAngularVelocity`; `Trj_IsCircling` boolean from sign-coherence; `Trj_CirclingTime += DT` if circling else 0; `FutureFacingDelta = Get_TotalFacingDelta(times)`; **forces MovementDirection→B (Backward) one frame** when large facing delta + non-X RotationMode (used to trigger 360-spin reselection).
3. `Update_States` — Sequence of 7 paths, each via macro `Update State Values` updating `<Enum>`/`<Enum>_LastFrame`/`<Enum>_Recent`/`<Enum>_Time`/`<Enum>_LastStateTime` for: MovementMode (RecentTimeLimit=0.2), RotationMode (0.1), MovementState (0.1), Gait (0.1), Stance (0.1), MovementDirection (0.1), and one extra path.
4. `Update_AimOffset` — `AO = MakeVec2D(Delta(SmoothedAimTarget, RootTransform.Rotation))`; `EnableAO = (RotationMode==Strafe || ==Aim) AND |AO| <= ε AND GetSlotLocalWeight < ε`; `SmoothedAimTarget = CriticalSpringDampRotator(SmoothedAimTarget, AimTarget, In Out Angular Velocity, DT)`. Comment: "Only enable the Aim Offset if the pawn is in the Strafe or Aim modes."
5. `Update_AdditiveLean` — `LateralAccelerationAmount = UnrotateVector(VelocityAcceleration, RotationFromX(Velocity)).Y / DT`; sign-flipped per MovementDirection; squared; `Map Range Clamped` to a Vec2D and stored in `LeanAmount`.

### Helpers used as transition rules / pin bindings

- **IsMoving** — `Trj_FutureVelocity != 0 AND Acceleration != 0`. (NOT velocity. The cause of AZ idle-walk-idle bug.)
- **IsStarting** — `IsMoving AND (V.Length2D + Trj_FutureVelocity.Length2D) >= ε AND Speed2D > 0 AND NOT(CurrentDatabaseTags contains pivot)`. Pivot tag check prevents interrupting pivots.
- **IsPivoting** — `MovementState==Moving` AND one of two collapsed condition graphs (chosen by `LocomotionSetup` int — different rules for MM-only vs SM+MM mode).
- **ShouldTurnInPlace** — `|FutureFacingDelta| >= 50° AND Speed2D < ε AND MovementState != Moving`.
- **ShouldSpinTransition** — `|FutureFacingDelta| >= ?° AND Speed2D >= ?` AND `NOT(CurrentDatabaseTags contains spin)`.
- **JustTraversed** — `MovingTraversal curve > 1 AND DefaultSlot NOT active AND |Trj_TurnAngle| <= ?`.
- **JustLanded_Light** — `MovementMode==Walking AND MovementMode_LastFrame==Falling AND LandVelocity > HeavyLandSpeedThreshold` (light landing).
- **JustLanded_Heavy** — same gate but `<=` HeavyLandSpeedThreshold.
- **JustTeleported** — `DistanceSquared(CharTransform.Loc, CharTransform_LastFrame.Loc) > Square(TeleportThreshold)`.

### Offset-root parameter providers (binding sources for OffsetRootBone)

- **Get_OffsetRootRotationMode** — `Release` if DefaultSlot active (montage playing), else `Accumulate`.
- **Get_OffsetRootTranslationMode** — `Release` if (DefaultSlot active OR MovementMode==Falling OR (Grounded AND NOT IsMoving)), else `Interpolate`.
- **Get_OffsetRootTranslationHalfLife** — Switch on MovementState: small value (snap) when Idle, larger value when Moving.
- **Get_OffsetRootTranslationRadius** — Returns CVar-cached `OffsetRootTranslationRadius`.

### Motion Matching parameter providers

- **Update_MotionMatching** (called from MM node OnUpdate, AnimNodeReference parameter) — `EvaluateChooser(CHT_PoseSearchDatabases_Relaxed)` → `SetDatabasesToSearch(MMNode, dbs, Get_MMInterruptMode())`.
- **Update_MotionMatching_PostSelection** — Caches `CurrentSelectedDatabase`, `CurrentDatabaseTags`, `CurrentSelectedAnim`, `MM Search Cost`. Calls `Override Motion Matching Blend Settings` with a MakeStruct.
- **Get_MMBlendTime** — Branches: if just landed (Falling→Walking) short blend; if just jumped (Walking→Falling AND VelZ > ?) very short blend; else default.
- **Get_MMInterruptMode** — `InterruptOnDatabaseChange` if any of (MovementState/MovementMode/Gait/Stance/MovementDirection) changed and gated on certain MovementState; else `DoNotInterrupt`.
- **Get_MMNotifyRecencyTimeOut** — Select on Gait → one of three literal float CVars; "must exceed footstep interval".
- **Get_PoseHistoryReference** — Convert AnimNodeReference (tag `PoseHistory`) to `PoseHistoryReference`.

### Per-anim BS inner graph providers (called via AnimNodeReference)

- **Get_DesiredFacing(Node)** — Sample current BS anim's "SteeringTargetTime" curve at current anim time, MapRangeClamped → use as time index into `Trajectory.GetTrajectorySampleAtTime` → returns `Quat` of the future facing. Used for Steering's `TargetOrientation`.
- **Get_ProceduralTargetTime(Node)** — Same shape: curve sample → MapRangeClamped → real. Used as `ProceduralTargetTime` of TIP Steering node.
- **Get_DynamicPlayRate(BlendStackInput)** — `Speed2D / SpeedCurve(t)`, clamped by Min/MaxDynamicPlayRate curves (defaults 0.75/1.25), modulated by `Enable_PlayRateWarping` curve as alpha; further modulated by `Trj_CirclingTime`.
- **Get_StrideWarpAlpha(Node)** — `Clamp(Enable_Warping_curve(t) + StrideStuff_curve(t), 0, 1)`.
- **Get_StrafeWarpAlpha(Node)** — Same shape: sums two curves on the playing anim, clamped 0–1.
- **Get_StrafeWarpDirection** — `Lerp(LastNonZeroVelocity, Trj_NearFutureVelocity, MapRangeClamped(|...|, ...))`.
- **EnableSteering(Node)** — `(MovementMode==Walking OR Falling) AND (BlendStackInputs.Loop OR IsMoving())`. Comment: prevents idle anims from being steered (causes sliding).

### State Machine helpers

- **SetBlendStackAnimFromChooser(StateMachineState, ForceBlend)** — Heart of the SM:
  1. Set `StateMachineState` var.
  2. Cache `Previous_BlendStackInputs = BlendStackInputs`.
  3. `EvaluateChooser2(CHT_MoverCharacterAnimations)` → array `(Asset, ChooserOutputs)`.
  4. If empty → `NoValidAnim=true`, exit.
  5. If first ChooserOutputs.UseMM → `Motion Match(self, ValidAnims)` to pick best anim+entry; if cost > MMCostLimit (when >0) → NoValidAnim=true, exit.
  6. Set fields in `BlendStackInputs` (Anim, StartTime, BlendTime, BlendProfile, Loop=IsAnimationAssetLooping).
  7. If `ForceBlend` → `Force Blend On Next Update` on the BS AnimNodeRef tagged `State Machine Blend Stack`.
  8. Reset `NotifyTransition_Re-Transition`, `NotifyTransition_ToLoop`. Set `Search Cost`.
- **IsAnimationAlmostComplete** — `NOT IsCurrentAssetLooping AND GetCurrentAssetTimeRemaining <= 0.75` (hard-coded 0.75s).

### Procedural Linked Anim Layers

- **Procedural** (post-layering) — Branches on `FootPlacementMode` int: Mode0=Control Rig (Foot IK), Mode1=Leg IK; then `BlendListByEnum (E_MovementMode)` + `JustTeleported / ForceFootPlacementReset` gates. Caches `PreProceduralPose`, `BasicIK`, `Control Rig IK`.
- **Procedural_PreLayering** — `BlendListByEnum (E_MovementMode)` → `Modify Bone (root)` for slope alignment when sliding; LocalToComp/CompToLocal pair.
- **AdditiveLeans** — `BlendListByEnum (E_MovementMode)` → `BlendListByEnum (E_MovementState)` → `ApplyMeshSpaceAdditive` of `BS_Relaxed_Lean_Head` / `BS_Relaxed_Run_Leans` / `BS_Relaxed_Walk_Leans`. Has `BlendListByBool` to mask leans during turn anims (curve-driven).
- **AimOffset** — `ApplyMeshSpaceAdditive` of `BS_Neutral_AO_Stand_NoSmoothing` blendspace. `BlendListByBool(EnableAO)` against `Additive Identity Pose`. Includes `Dead Blending` node.
- **Biped_FootPlacement_OnBecomeRelevant(Context, Node)** — Sets `ForceFootPlacementReset=true` (cleared post-tick in EventGraph).
- **AllowFootPinning** — `Select on E_MovementMode (true only Walking) AND IsMoving`.
- **AllowSlopeWarping** — `Select on E_MovementMode` (typically true except Falling).

### Slide helpers
- **Get_SlideSlopeRotation** — `Get Slope Degree Angles` × ground normal × actor right/up vectors → `MakeRotator(roll, pitch, 0)`.
- **Get_SlideSlopeOffset** — `ProjectPointOnToPlane(GroundLocation, SmoothedGroundNormal) - CharProps.location`.

### Init / properties
- **InitializeMoverPredictor** — `Try Get Pawn Owner` → `Get Component By Class (MoverComponent)` → cache `Mover`; `Construct MoverTrajectoryPredictor` → `Setup(Mover)` → cache `Predictor`; set `HasMover` and `HasOwningActor`.
- **Update_CVarDrivenVariables** — Reads CVars: `OffsetRootBoneEnabled`, `MMDatabaseLOD`, `OffsetRootTranslationRadius`, `UseThreadSafeUpdateAnimation`, `LocomotionSetup`, `FootPlacementMode`, `FootPlacement_Enable`, `FootPlacement_Debug`. Two paths in middle override `LocomotionSetup` based on `Component Has Tag` checks.
- **Update_PropertiesFromCharacter** — `Get Owning Actor` → BPI message `BPI_SandboxCharacterPawn::Get Properties for Animation` → set `CharacterProperties`.

## 5) Variables (107) — categories

- **States** (E_*): `MovementMode`, `RotationMode`, `MovementState`, `Gait`, `Stance`, `MovementDirection` — each with `_LastFrame`, `_Recent`, `_Time`, `_LastStateTime`.
- **Essential Values**: `CharacterProperties` (S_CharacterPropertiesForAnimation), `CharacterTransform`(+_LastFrame), `RootTransform`, `HasAcceleration`, `Acceleration`(+_LastFrame), `AccelerationAmount`, `HasVelocity`, `Velocity`(+_LastFrame), `RelativeAcceleration`, `VelocityAcceleration`, `LastNonZeroVelocity`, `Speed2D`, `HeavyLandSpeedThreshold`, `InteractionTransform`, `SmoothedGroundNormal`.
- **Trajectory**: `Predictor` (MoverTrajectoryPredictor*), `Trajectory` (TransformTrajectory), `TrajectoryCollision`, `PreviousDesiredControllerYaw`, `Trj_PastVelocity`, `Trj_NearFutureVelocity`, `Trj_FutureVelocity`, `Trj_PreviousFutureVelocity`, `Trj_FutureFacing` (Rotator), `Trj_TurnAngle`, `Trj_PastAngularVelocity`, `Trj_CurrentAngularVelocity`, `Trj_IsCircling`, `Trj_CirclingTime`, `FutureFacingDelta`(+_LastFrame).
- **Aim Offset**: `SmoothedAimTarget` (Rotator), `AO`/`Previous_AO` (Vec2D), `EnableAO`, `In Out Angular Velocity` (Vec).
- **Motion Matching**: `MMDatabaseLOD`, `ValidDatabases` (TArray<UPoseSearchDatabase*>), `CurrentSelectedDatabase`, `CurrentSelectedAnim` (UObject*), `CurrentDatabaseTags` (TArray<FName>), `MM Search Cost`.
- **Root Offset**: `OffsetRootBoneEnabled`, `OffsetRootTranslationRadius`.
- **State Machine (Experimental)**: `ValidAnims` (TArray<UAnimationAsset*>), `BlendStackInputs` (S_BlendStackInputs), `Previous_BlendStackInputs`, `StateMachineState` (E_ExperimentalStateMachineState), `NoValidAnim`, `NotifyTransition_Re-Transition`, `NotifyTransition_ToLoop`, `Search Cost`, `FutureFacingOnTransitionStart` (Rotator).
- **Foot Placement**: `PlantSettings_Default`, `PlantSettings_Stops`, `InterpolationSettings_Default`, `InterpolationSettings_Stops`, `FootPlacement_Enable`, `FootPlacement_Debug`, `FootPlacementMode` (int), `TeleportThreshold`.
- **Default**: `HasOwningActor`, `HasMover`, `UseThreadSafeUpdateAnimation`, `LocomotionSetup` (int), `Mover` (UMoverComponent*).
- **Procedural**: `ForceFootPlacementReset`.
- **Additive Lean**: `LeanAmount` (Vec2D).
- **Debug**: `TransitionHistory` (TArray<FString>), `PawnSpeedHistory`, `MoveData_Speed_History`, `Phase_History`, `Contact_L_History`, `Contact_R_History`, `Enable_Warping_History`, `DebugTransitions` (bool).

## 6) Choosers used (two distinct)

- AnimGraph MM `Update_MotionMatching` evaluates `EvaluateChooser2(CHT_PoseSearchDatabases_Relaxed)` — ONLY filters DBs.
- `SetBlendStackAnimFromChooser` evaluates `EvaluateChooser2(CHT_MoverCharacterAnimations)` — picks the actual anim.

## 7) Anim node tags
| Tag | Node |
|---|---|
| `OffsetRoot` | OffsetRootBone (top-level) |
| `PoseHistory` | Pose History (top-level) |
| `State Machine Blend Stack` | Outer BlendStack |
| `State Machine Blend Stack Input` | each `K2Node_AnimNodeReference` inside BS inner graph that re-fetches the BS to query playing anim |
