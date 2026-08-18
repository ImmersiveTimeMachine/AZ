---
name: project-gasp-cmc-abp-spec
description: "Full teardown of GASP 5.8 SandboxCharacter_CMC_ABP (MM-node path) as the build spec for AZ's new CMC hero ABP — contract struct, graph spine, function surface, required curves, and the database-density blocker."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-18T01:46:24.534Z
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

## 2b. ★★ THE UPDATE CHAIN — exact, read node-by-node 2026-08-17 (screenshot-validated)

Only ONE Blueprint update event is implemented: **`BlueprintThreadSafeUpdateAnimation`** (worker thread).
It runs `Update_Logic`, whose entry is `bThreadSafe=True`. `Update_Logic` is 8 nodes:

```
Update_Logic → Update_Trajectory → Update_EssentialValues → Update_States
             → Branch(UseExperimentalStateMachine)
                  true  → Update_MovementDirection → Update_TargetRotation
                  false → END
```

★ **`UseExperimentalStateMachine = False` on the CDO**, so on the shipped MM-node path
`Update_MovementDirection` and `Update_TargetRotation` **NEVER RUN**. The MM node needs no direction
bucket and no target rotation — motion matching selects by trajectory. A 6-way direction enum is an
artifact of the state-machine path, where a chooser row must be addressed by a discrete direction.
(AZ still needs it for CHT rows — but that is OUR requirement, not inherited.)

### Update_Trajectory (27 nodes) — every literal
```
PoseSearchGenerateTransformTrajectory(
    InAnimInstance   <- PropertyAccess GetOwningComponent.GetAnimInstance  (Pre-Event Graph)
    InTrajectoryData <- Select(Index = Speed2D > 0.0, 0=TrajectoryGenerationData_Idle,
                                                     1=TrajectoryGenerationData_Moving)
    InDeltaTime      <- PropertyAccess GetDeltaSeconds (Thread Safe)
    InOutTrajectory  <- Trajectory
    InOutDesiredControllerYawLastUpdate <- PreviousDesiredControllerYaw
    InHistorySamplingInterval = -1.0     ← NEGATIVE = collect EVERY update
    InTrajectoryHistoryCount  = 30
    InPredictionSamplingInterval = 0.1
    InTrajectoryPredictionCount  = 15 )
→ HandleTrajectoryWorldCollisions(bApplyGravity=true, FloorCollisionsOffset=0.01,
    TraceChannel=TraceTypeQuery1, bTraceComplex=false, bIgnoreSelf=true,
    MaxObstacleHeight=150.0, DrawDebugType=None, AnimInstance<-Self)
→ TrajectoryCollision = CollisionResult      (carries TimeToLand + LandSpeed — free landing predictor)
→ Trajectory          = OutTrajectory        (the COLLIDED one, not the raw)
→ Trj_PastVelocity    = GetTrajectoryVelocity(Time1=-0.3, Time2=-0.2)
→ Trj_CurrentVelocity = GetTrajectoryVelocity(Time1= 0.0, Time2= 0.2)
→ Trj_FutureVelocity  = GetTrajectoryVelocity(Time1= 0.4, Time2= 0.5)
```
THREE velocity samples, not one. `Trj_FutureVelocity` is **0.4→0.5s ahead** — much further than intuition
suggests, and it is the one the IsMoving rule keys off.

### Update_EssentialValues (43 nodes) — a Sequence of 4
```
then_0: CharacterTransform_LastFrame = CharacterTransform;  CharacterTransform = <PropertyAccess>
then_1: Branch → RootTransform = MakeTransform(GetOffsetRootTransform(OffsetRoot node).Location,
                                               Roll, Pitch, Yaw + 90, Scale 1)
                 else RootTransform = CharacterTransform
then_2: Acceleration_LastFrame = Acceleration;  Acceleration = <PropertyAccess>
        AccelerationAmount = SafeDivide(|Acceleration|, MaxAcceleration)   ← NORMALISED 0..1
        HasAcceleration    = AccelerationAmount > 0
then_3: Velocity_LastFrame = Velocity;  Velocity = <PropertyAccess>
        Speed2D            = VectorLengthXY(Velocity)
        HasVelocity        = Speed2D > 5.0
        VelocityAcceleration = (Velocity - Velocity_LastFrame) / Max(DeltaTime, 0.001)
        RelativeAcceleration = UnrotateVector(VelocityAcceleration, RootTransform.Rotation)
        if HasVelocity: LastNonZeroVelocity = Velocity
```
★ **`RootTransform` is the OFFSET ROOT, not the actor** — read back out of the OffsetRootBone node via
`AnimationWarpingLibrary::GetOffsetRootTransform` with **Yaw + 90** (the mesh's -90 convention). Everything
actor-relative (RelativeAcceleration, and direction downstream) is computed against THAT, not the capsule.
This is the mechanism by which the anim layer stays consistent while the capsule turns instantly.
(The `OffsetRootBoneEnabled` variable exists but its Get is UNWIRED and the Branch condition is a bare
`true` — a leftover; the offset path is effectively hard-on.)

### Update_States (20 nodes) — a Sequence of 5, all the same shape
`X_LastFrame = X; X = <PropertyAccess>` for **MovementMode, RotationMode, Gait, Stance**, plus
`MovementState_LastFrame = MovementState; MovementState = IsMoving() ? Moving : Idle`.
That is the whole function: a one-frame history for every discrete state, so transitions are detectable.

## 2c. ★★ UAZ_AnimInstance vs GASP 5.8 — MEASURED DIVERGENCES (2026-08-17)

`UAZ_AnimInstance` is a C++ port of ~45 of these functions, but from a **GASP 5.5-era** sample. Diffed
node-by-node against 5.8. **In every case ours is the stale one.** Ported the 5.8 behaviour into
`UAZ_CmcAnimInstance`; `UAZ_AnimInstance` itself is UNCHANGED (still live on feature/NPC) — treat these as
known-stale if that path ever misbehaves.

| Function | GASP 5.8 | UAZ_AnimInstance (stale) |
|---|---|---|
| `Get_DesiredFacing` | `GetTrajectorySampleAtTime(Trajectory, 0.5).Facing` — 3 lines | BlendStack anim + `SteeringTargetTime` curve + MapRange |
| `ShouldSpinTransition` | \|yaw(**CharacterTransform** vs **RootTransform**)\| ≥ 130 | \|`FutureFacingDelta`\| ≥ 130 — a DIFFERENT quantity |
| `IsStarting` | `IsMoving()` AND future ≥ current+100 AND !Pivots | `bHasVelocity` AND … AND **`Speed2D < 100`** (extra) |
| `Get_MMBlendTime` | ground 0.5, **just-landed 0.2**, rising 0.15, falling 0.5 | ground 0.2, landed 0.5 — **inverted** |
| `AllowFootPinning` | OnGround AND **`IsMoving()`** | OnGround AND a config bool |
| `Get_MMInterruptMode` | modeChanged OR ((state\|gait\|stance changed) AND OnGround) | state change NOT gated on ground; adds direction term |
| `Get_OffsetRootTranslationHalfLife` | Idle 0.1 / Moving 0.3 | adds a third sprint-specific value |
| `EnableSteering` | (State==Moving OR Mode==InAir) AND blendstack anim active | adds a `bLoop` fallback and Sliding |

★ **The `Get_DesiredFacing` change removes a blocker**: 5.8 no longer reads `SteeringTargetTime`, of which
AZ has **zero** clips. The old implementation would have returned a constant.

**Other 5.8 facts captured:**
- `Get_AO_Yaw` = `Select(RotationMode){ OrientToMovement:0, Strafe:Get_AOValue().X, Aiming:0 }` — aim
  offset yaw is non-zero ONLY while strafing.
- `Get_MovementDirectionThresholds` is **DYNAMIC, not a constant struct**: F/B → 60/120; sideways+pivoting
  → 60/120; sideways+looping+!aiming → 60/140; else → 40/140. (GASP stores them signed; AZ keeps
  positive magnitudes and negates at the comparison.)
- `Get_MMNotifyRecencyTimeOut` = Walk 0.2 / Run 0.2 / Sprint 0.16.
- `Get_OffsetRootRotationMode` = `IsSlotActive("DefaultSlot") ? Release : Accumulate`; TranslationMode adds
  `OnGround ? (IsMoving ? Interpolate : Release) : Release`.
- `Get_OrientationWarpingWarpingSpace` = `OffsetRootBoneEnabled ? RootBoneTransform : ComponentTransform`.
- `Get_StrafeYawRotationOffset` reads curves `StrafeOffset_{F,B,LL,LR,RL,RR}` out of a curve-container
  animation, indexed by `MapRangeClamped(CalculateDirection(...), -180..180 → 0..8) / 30`. AZ HAS all six
  curves (1 clip each). Deferred with the strafe slice.
- ENUM DECODE: `E_MovementState` NewEnumerator0=**Moving**, NewEnumerator4=**Idle**.
  `E_MovementMode` display order = OnGround, InAir, Sliding, Traversing, Flying, Ragdoll against internal
  keys 4,5,6,7,9,10 (pairing INFERRED — the map keys are not in the string table).

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

## 4. Curves — MEASURED both libraries 2026-08-17 (not inferred)

Method: `unreal.AnimationLibrary.get_animation_curve_names(seq, RCT_FLOAT)` over every AnimSequence
(`AnimationBlueprintLibrary` is NOT exposed to Python — the binding is `unreal.AnimationLibrary`).
AZ: 1980 clips, 66% carry >=1 curve, **33 distinct names**. GASP: 2011 clips, 71%, 2458 names (mostly face).

| GASP graph reads | AZ has | AZ count | GASP count |
|---|---|---|---|
| `movedata_speed` | `MoveData_Speed` | 745 | 1131 |
| `phase` | `Phase` | 339 | 611+97 |
| `enable_warping` | `Enable_OrientationWarping` **+** `Enable_PlayRateWarping` | 673 / 352 | 1024 |
| `contact_l` / `contact_r` | `FootSpeed_L` / `FootSpeed_R` | 824 / 824 | 1343 |
| `maxdynamicplayrate` | `MaxDynamicPlayRate` | 17 | 17 |
| `mindynamicplayrate` | — | 0 | **0 — GASP has none either; it is a node default, NOT an authored curve** |
| `steeringtargettime` | — | **0** | 228 |
| `enable_turninplacesteering` | `Enable_TurnInPlaceSteering` | 12 | 42 |
| `disable_ao` | `Disable_AO` | 6 | 8 |
| `disable{left,right}footik`, `disablepelvisadjustment` | same names | 86 each | 86 each |

Case is a NON-issue: GASP 5.8 authored lowercase, AZ CamelCase, and `FName` compares case-insensitively.

**Three design consequences:**
1. **Skip GASP's RemapCurves node.** `contact_l=(1-contact_l)*100` exists only to synthesize a foot
   SPEED from a contact flag. `FootSpeed_L/R` already IS one (measured on
   `RT_NWP_M_Neutral_Stand_Idle_Loop`: 300 keys, 0.02–4.75). Bind FootPlacement to it directly.
2. **Foot phase = `FootSpeed_L < threshold`**, not `contact_l > 0.5`.
   ★ LIVE BUG THIS EXPOSES: `AZ_MoverAnimInstance.cpp` sets `bLeftFootDown = GetCurveValue("contact_l")
   > 0.5`, but only **2 of the 104 clips in our databases carry `contact_l`** — so v2's foot phase has
   been permanently false and every foot-aware chooser row has only ever picked its default variant.
3. Warping enable is SPLIT in AZ (orientation vs play-rate) — finer-grained than GASP's single flag;
   bind the two separately rather than collapsing them.
`steeringtargettime` is the one genuine authoring gap (TIP steering) — constant or author it.

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

**BUT this is an ingestion gap, not a content gap** — and 2026-08-17 measurement shows the gap is even
more lopsided than thought. AZ holds TWO disjoint locomotion libraries:

| | `/Game/AZ/NoWeapons/RT` (`RT_NWP_M_*`) | `/Game/AZ/NoWeapons/RootMotions` (`LM_RM_*`) |
|---|---|---|
| clips | 929, of which **319 fully curved** | 536, **zero float curves** |
| in our PoseSearch DBs | **none** | ~all of the 104 |
| skeleton | SKEL_SurvivalMan | SKEL_SurvivalMan |

i.e. **92 of the 104 clips we ingested have no float curves at all**, while the curve-complete,
MM-shaped set sits entirely outside every database. ★★ CORRECTED 2026-08-17 (an earlier census in this file was WRONG — it parsed name tokens by position
over only the curved subset and mis-binned them). The real taxonomy, counted off disk:

`RT_NWP_M_Neutral_<Gait>_<Motion>_<Direction>_<Lfoot|Rfoot>` — foot phase is in the FILENAME.

| Motion | Walk | Run | Crouch | Sprint |
|---|---|---|---|---|
| Loop | 14 | 16 | 12 | 5 |
| **Start** | **20** | **20** | **20** | 6 |
| **Stop** | **20** | **20** | **20** | 6 |
| **Pivot** | **20** | **20** | **20** | — |
| Turn (in place) | 16 | 16 | 16 | 8 |
| Idle | (Stand_Idle_*) | — | 14 | — |
| Box / Diamond / Hourglass | 32/32/32 | 32/32/32 | 32/32/31 | — |

**STOPS EXIST** — 20 per gait, 10 directions (F/FL/FR/B/BL/BR/LL/LR/RL/RR) × 2 foot phases. An earlier
version of this file claimed there were none and that decelerations lived inside `Box`. **Both wrong.**
`Box` / `Diamond` / `Hourglass` are STRAFE DIRECTION-CHANGE sets (`Box_F_LL_Lfoot` = forward → strafe-left,
left foot leading) — they belong to the combat/strafe slice, not the explore slice. Also present:
`Arc_F_{Small,Tight,Wide}_{L,R}` (cornering) and `Circle_Strafe_{L,R}`.

`UAZ_PoseSearchUtils` exists precisely to add anims to databases (the engine doesn't expose it), so
ingestion is automatable. Order of work: **ingest the RT set → density → then the MM node earns its
keep.** Not the reverse.

### ✅ STEP 1 INGESTED 2026-08-17 — 12 databases / 222 clips, verified from disk

Structure copied from **GASP's `PSD_Sparse_*` tier** — Epic's own minimum viable MM-node set (Loops /
Starts / Stops / Pivots per gait, stand + crouch). GASP ships the same shape at four densities:
`Dense` 36 · `Relaxed` 77 (adds LL/LR/RL/RR directional sets) · `Sparse` 16 · `Extreme_Sparse` 16, plus
`PSD_SM_Mover_{Loops,Stops,Spins,Transitions,TraversalTransitions}` and `PSD_SM_CMC_{Idles,Loops,
Transitions}` for the experimental SM path. Tier is selected by a chooser (`CHT_PoseSearchDatabases*`).

Created at `/Game/AZ/Blueprints/Animation/MotionMatching/CMC/`:
`PSD_AZ_{Stand_Walk, Stand_Run, Crouch_Walk}_{Loops, Starts, Stops, Pivots}`
counts 14/20/20/20 · 16/20/20/20 · 12/20/20/20. Existing 17 v2 DBs UNTOUCHED (feature/NPC still uses them).

**Gates checked before writing anything:** all 222 clips have `enable_root_motion=True` and are on
`SKEL_SurvivalMan`. (Root motion is the gate that matters — MM derives trajectory features from it; RM-off
clips index as stationary and never match a moving query.)

**Schema = `PSS_v2_SurvivalMan_Loco`, reused.** `UPoseSearchSchema` exposes NOTHING to Python or to
`get_asset_properties` (its skeleton moved into a `PoseSearchRoledSkeleton` array in 5.8), so its sample
times could not be introspected. Justification for reuse: it already backs `PSD_v2_Strafe{Run,Walk,Crouch}`,
which are 8-clip MULTI-clip DBs on SurvivalMan clips working in the live v2 stack. **If MM search quality
is poor, the schema is the first suspect, not the clip set.**

**Two script gotchas (both cost a round trip):**
1. `unreal.PoseSearchDatabaseFactory` + `create_asset` returns **None** — `ConfigureProperties()` is a
   modal picker and fails headless. Use `AssetTools.duplicate_asset(name, dest, ExistingDB)` instead; it
   also inherits proven sampling/cost settings. Duplicates get NO inbound BranchIn notifies, so they stay
   clean after `ClearDatabase`.
2. `db.post_edit_change()` / `mark_package_dirty()` are NOT exposed on PoseSearchDatabase from Python.
   Not needed: reindex is DDC-keyed and automatic on PostLoad / every MM search. Just
   `EditorAssetLibrary.save_loaded_asset(db, False)`.

Direct entries via `AddSequencesToDatabase` — **no BranchIn notifies** (that is the single-clip v2 model;
mixing double-adds, see [[feedback-posesearch-branchin-db-sync]]).

### ✅ STEP 1b INGESTED 2026-08-17 — +9 databases / +71 clips (21 DBs, 293 clips total)

`PSD_AZ_Stand_Idles` 1 · `Stand_IdleBreaks` 6 · `Crouch_Idles` 1 · `Crouch_IdleBreaks` 5 ·
`Stand_TurnInPlace` 2 · `Crouch_TurnInPlace` 8 · `Stand_Walk_Turns` 16 · `Stand_Run_Turns` 16 ·
`Crouch_Walk_Turns` 16. All verified from disk, zero dupes, all root-motion ON.

**TWO turn families, do not merge them:**
- `<Gait>_Turn_<L|R>_<angle>_<foot>` = turning WHILE MOVING (foot-phase tagged ⇒ mid-stride) → `_Turns`
- `Crouch_Idle_Turn_<angle>_<L|R>` and `Idle_turn_{left,right}` = TURN IN PLACE from rest (no foot tag)
  → `_TurnInPlace`

**Idle breaks kept OUT of `_Idles`** on purpose: an idle break is a one-shot fidget triggered on a timer
(v2 used IdleBreakMinTime/MaxTime). Putting them in the resting pool lets MM drift into or re-enter one
arbitrarily. Separate DB keeps the timer-triggered option open.

★ **KNOWN GAP: standing turn-in-place has only 2 clips** (`Idle_turn_left/right`) vs crouch's 8
(4 angles × 2 sides). Candidates exist OUTSIDE the RT set — `LM_RM_TurnLt90_Loop` / `TurnRt90_Loop` /
`TurnLt180` / `TurnRt180` / `Turn180Surprised`, and a separate `rm_W2_Stand_{Aim,Rlx}_Turn_In_Place_*`
family — but the LM_RM ones carry NO curves, so mixing them in means MM can select a frame where
FootSpeed/MoveData_Speed read 0. Decide deliberately before mixing clip families.

### ✅ STEP 2 INGESTED 2026-08-17 — +6 databases / +39 clips. **TOTAL 27 DBs / 332 clips, all verified.**

`Stand_Sprint_{Loops 5, Starts 6, Stops 6, Turns 8}` · `Stand_GaitTransitions 12` · `StanceTransitions 2`.

Sprint is FORWARD-ONLY: loops are `F / FL / FR / F_L_20 / F_R_20`, starts+stops only `F/FL/FR × foot`, and
there are **no Sprint pivots** — the 180° entries in `Sprint_Turn` cover reversal. Do not go looking for
`Sprint_Pivot`; it does not exist.

★ **Gait vs STANCE transitions must not share a pool.** `_Transition_` naively matches 14 clips, but two
of them (`Transition_Crouch_to_Stand`, `Transition_Stand_to_Crouch`) are stance changes — pooled together,
MM can answer a walk→run query with a crouch→stand clip. Split: `GaitTransitions` = Walk↔Run↔Sprint
(12, foot-tagged), `StanceTransitions` = the 2 (no foot tag — they start from a settled state).

Still excluded from every pool (they matched sprint greps but belong elsewhere): `Sprint_Reface_*` and
`Sprint_Diamond_*` → strafe slice; `Sprint_Pose_Lean_*` (7) → ADDITIVE lean poses, not locomotion clips,
they belong to the lean/aim-offset system and must never enter an MM database;
`Jump_F_{Start,Land}_Sprint_*` → jump slice.

**Deferred:** step 3, the combat/strafe slice — `Box` / `Diamond` / `Hourglass` (≈32 each per gait),
`Reface`, `Shuffle`, `Spin`, `Arc`, `Circle`. Also unstarted: jumps/lands, traversal.

## 6. What AZ keeps regardless

Chooser tables survive the switch — in path 0 the chooser outputs an **array of databases** (pool filter)
instead of a single clip. Keep `UAZ_LocomotionStateMachine` in C++ as a **gameplay** state provider that no
longer drives pose, so combat keeps its deterministic `EAZ_StateMachineState` while MM owns selection.

Related: [[project-gasp58-update-audit]], [[project-cmc-backport-spike]], [[feedback-metahuman-modular-hero]]
