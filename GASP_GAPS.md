# GASP → AZ — Gap Inventory

Comprehensive "what's missing from AZ" list, derived from the 6 GASP audit memory files plus live source inspection of `C:\UE57\Games\AZ\Source\AZ\Public\Character\AZ_HeroPawn.h`, `C:\UE57\Games\AZ\Source\AZ\Public\Animation\AZ_AnimInstance.h`, `C:\UE57\Games\AZ\Source\AZ\Public\Animation\AZ_LocomotionTypes.h`, `C:\UE57\Games\AZ\Config\DefaultEngine.ini`.

Severity: **critical** (blocks GASP-parity behaviour) / **important** (visible glitch / wrong gameplay) / **nice-to-have** (polish or dev-UX).

---

## Missing classes / components

| Item | Severity | Affected feature | Source | Recommendation |
|---|---|---|---|---|
| `BP_MovementMode_Walking` (subclass of `USmoothWalkingMode`) registered in `AAZ_HeroPawn.MovementModes` | **critical** | Smooth capsule yaw rotation; spring-damper facing; per-gait FacingTime/TurnStrength; JustLanded sticky brake; OverridenDesiredFacing 179° clamp | `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\gasp_movement_modes.md` §1, §6, §7 | Port directly: re-target `AZ_BP_HeroPawn.CharacterMoverComponent.MovementModes["Walking"]` to `/Game/Blueprints/MovementModes/BP_MovementMode_Walking_C` (GASP BP imported into AZ). Fallback: subclass `USmoothWalkingMode` in C++ as `AZ_MovementMode_Walking`. |
| `BP_MovementMode_Falling` (subclass of `UFallingMode`) registered | **important** | Air capsule rotation (`ComputeAngularVelocityDegrees(TurningRateLimit=300)`); without it, in-air yaw never tracks input | `gasp_movement_modes.md` §2, §6 (insight 7) | Port directly: re-target Falling slot to `BP_MovementMode_Falling_C`. |
| `BP_MovementMode_Slide` + `BP_MovementTransition_To/FromSlide` | nice-to-have | Slide locomotion + boost/decay/slope-modulated speed | `gasp_movement_modes.md` §3, §4, §5 | Skip until slide is needed; engine has no slide mode by default. |
| `MotionWarpingComponent` on `AAZ_HeroPawn` | important | Required for traversal (vault/mantle), motion-warping anim notifies, smart-object alignment | `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\gasp_pawn_bp_full.md` §2 (component list); `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\gasp_data_model_full.md` §2.7 ("FAZ_CharacterPropertiesForTraversal — missing MotionWarping") | Port directly: add `MotionWarpingComponent` as a default subobject in `AAZ_HeroPawn` ctor. |
| `GameplayCameraComponent` (UE5 plugin) | nice-to-have | Procedural camera director with per-state rigs (`CameraAsset_SandboxCharacter` + 12 `CameraRigAsset`s + `CHT_CameraRig` chooser) | `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\gasp_framework_cameras_rigs.md` §3 | Skip — AZ has its own `UAZ_PawnCameraMovementComponent`. Re-evaluate if AZ wants per-stance camera-rig switching. |
| `AC_TraversalLogic` actor component | important (when traversal needed) | Vault/hurdle/mantle/climb (full pipeline: capsule trace → ledge transforms → chooser → motion warp → montage) | `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\gasp_actor_components_and_notifies.md` §1.2 | Port directly when traversal is in scope. Depends on: `S_TraversalCheck*` structs (AZ has stubs; see `AZ_LocomotionTypes.h:399-528`), `LevelBlock_Traversable` actor, `CHT_TraversalMontages_Mover` chooser, traversal montages with `Distance_From_Ledge` curves. |
| `AC_FoleyEvents` component | nice-to-have | Footstep / land / jump / slide audio dispatched from anim notifies | `gasp_actor_components_and_notifies.md` §3.1; `gasp_pawn_bp_full.md` §2 | Skip until audio polish pass. AZ's `PlayFoleyEvent_Stub` (`C:\UE57\Games\AZ\Source\AZ\Public\Character\AZ_HeroPawn.h:397`) preserves the call shape. |
| `AC_VisualOverrideManager` component | skip | Demo character mesh-swap (Manny/Quinn/Echo dropdown) | `gasp_actor_components_and_notifies.md` §1.3 | Skip — pure demo UX. AZ uses one Hero mesh. |
| `AC_SmartObjectAnimation` component | skip | NPC StateTree + PoseSearch interaction with smart objects | `gasp_actor_components_and_notifies.md` §1.4; `gasp_framework_cameras_rigs.md` §6 | Skip until AI/StateTree NPC work begins. |
| `AC_PreCMCTick` | **skip** (do not port) | CMC-only workaround for tick-order — Mover replaces it natively with PreSim/PostSim pattern | `gasp_actor_components_and_notifies.md` §1.1 | Already correctly handled by AZ's `OnPreSimulationInput`/`OnPostSimulationInput`. Do not port. |
| Post-Process AnimBP wrapper for `CR_AZ_Hero_FootPlacement` Control Rig | important | Foot IK + pelvis warp on slopes (mesh foot placement). Asset already exists: `Content/AZ/Blueprints/Animation/CR_AZ_Hero_FootPlacement.uasset` (per git status) | `gasp_framework_cameras_rigs.md` §4; `gasp_animbp_full_audit.md` §5.39 (Procedural layer description) | Create a Post-Process AnimBP that runs `CR_AZ_Hero_FootPlacement` on the SK mesh; wire `bFootPlacementEnabled`, `AllowFootPinning`, `AllowSlopeWarping`, `JustTeleported` from `UAZ_AnimInstance` (header lines 884–893). |

---

## Missing data / structs / enums / curves

### Enum value/order divergences (silently break Chooser numerics + net replication)

| Enum | AZ value | GASP value | Severity | Source |
|---|---|---|---|---|
| `EAZ_MovementState` | `Idle=0, Moving=1` (`AZ_LocomotionTypes.h:42-46`) | `Moving=0, Idle=1` | **critical** | `gasp_data_model_full.md` §1.4, §7 (#1) |
| `EAZ_MovementDirection.LL` / `.LR` | `LL=2, LR=3` (`AZ_LocomotionTypes.h:60-65`) | `LR=2, LL=3` | **critical** | `gasp_data_model_full.md` §1.5, §7 (#2) |
| `EAZ_StateMachineState` (values 1..5) | Loop/Transition pairs interleaved (`AZ_LocomotionTypes.h:88-98`) | Loops first (0/1/2), transitions next (3/4/5) | **critical** | `gasp_data_model_full.md` §1.8, §7 (#3) |
| `EAZ_TraversalActionType.Hurdle` / `.Vault` | `Hurdle=1, Vault=2` (`AZ_LocomotionTypes.h:104-107`) | `Vault=1, Hurdle=2` | important (traversal-only) | `gasp_data_model_full.md` §1.9, §7 (#4) |

**Recommendation for all four:** Port directly — fix the AZ enum values to match GASP. Use `CoreRedirects` to map old values during migration if any saved data references them.

### Enum display label divergences (cosmetic + Chooser-inspector)

| Enum | AZ label | GASP label | Severity | Source |
|---|---|---|---|---|
| `EAZ_Stance` | `Standing/Crouching` | `Stand/Crouch` | nice-to-have | `gasp_data_model_full.md` §1.2, §7 (#5) |
| `EAZ_MovementMode.Slide` | `Slide` | `Sliding` | nice-to-have | `gasp_data_model_full.md` §1.3, §7 (#6) |
| `EAZ_RotationMode.Aiming` | `Aiming` | `Aim` | nice-to-have | `gasp_data_model_full.md` §1.7, §7 (#7) |
| `EAZ_MovementDirectionBias` | `LeftBias/RightBias` | `LeftFootForward/RightFootForward` | nice-to-have | `gasp_data_model_full.md` §1.6, §7 (#8) |

**Recommendation:** Re-implement (rename) — pure label change, no value impact.

### Struct boolean-prefix divergences (`b` prefix breaks FName-keyed Chooser bindings)

| Struct | AZ field | GASP field | Severity | Source |
|---|---|---|---|---|
| `FAZ_BlendStackInputs.bLoop` | `bLoop` (`AZ_LocomotionTypes.h:323`) | `Loop` | important | `gasp_data_model_full.md` §2.1, §7 (#9) |
| `FAZ_ChooserOutputs.bUseMM` | `bUseMM` (`AZ_LocomotionTypes.h:346`) | `UseMM` | important | `gasp_data_model_full.md` §2.2, §7 (#10) |
| `FAZ_PlayerInputState.bWantsTo*` (5 fields) | all `b`-prefixed (`AZ_LocomotionTypes.h:131-143`) | `WantsTo*` | important | `gasp_data_model_full.md` §2.3, §7 (#11) |
| `FAZ_MoverCustomInputs.bWantsToCrouch` | `bWantsToCrouch` (`AZ_LocomotionTypes.h:166`) | `WantsToCrouch` | important | `gasp_data_model_full.md` §2.4, §7 (#12) |
| `FAZ_TraversalCheckResult.bHas*` (3 fields) | all `b`-prefixed (`AZ_LocomotionTypes.h:438,447,456`) | `Has*` | important (traversal-only) | `gasp_data_model_full.md` §2.12, §7 (#13) |
| `FAZ_TraversalChooserInputs.bHas*` (3 fields) | all `b`-prefixed (`AZ_LocomotionTypes.h:487,490,493`) | `Has*` | important (traversal-only) | `gasp_data_model_full.md` §2.13, §7 (#14) |
| `FAZ_CharacterPropertiesForAnimation.bJustLanded` | `bJustLanded` (`AZ_LocomotionTypes.h:257`) | `JustLanded` | important | `gasp_data_model_full.md` §2.5, §7 (#15) |

**Recommendation:** Rename to drop `b` prefix. Use property redirects. Important because BP `EvaluateChooser` evaluates struct bindings by Display Name (which UE generates from the field name); a mismatched prefix means the chooser binding silently reads default values.

### Missing struct fields

| Struct | Missing field | Severity | Source |
|---|---|---|---|
| `FAZ_CharacterPropertiesForAnimation` (`AZ_LocomotionTypes.h:246-273`) | 9 of 18 GASP fields: `MovementMode, RotationMode, Stance, Gait, ActorTransform, Velocity, InputAcceleration, CurrentMaxAcceleration, CurrentMaxDeceleration, InputState` | **critical** if feeding GASP-derived helper functions | `gasp_data_model_full.md` §2.5, §7 (#16) |
| `FAZ_CharacterPropertiesForCamera` (`AZ_LocomotionTypes.h:277-289`) | `CameraStyle` field | nice-to-have | `gasp_data_model_full.md` §2.6, §7 (#17) |
| `FAZ_CharacterPropertiesForTraversal` (`AZ_LocomotionTypes.h:293-311`) | `MotionWarping` (TObjectPtr<UMotionWarpingComponent>) field | important (traversal) | `gasp_data_model_full.md` §2.7, §7 (#18) |
| `FAZ_TraversalChooserInputs` (`AZ_LocomotionTypes.h:479-515`) | `PoseHistory` (FPoseHistoryReference) — required for MM cost evaluation in traversal Chooser | important (traversal) | `gasp_data_model_full.md` §2.13, §7 (#19) |

**Recommendation:** Port directly when corresponding feature is in scope.

### Missing curve assets (and the chooser that picks them)

| Asset | GASP path | Severity | Source |
|---|---|---|---|
| `CHT_RotationOffsetCurve` (Chooser asset) | `/Game/Blueprints/Data/CHT_RotationOffsetCurve` | important | `gasp_data_model_full.md` §4, §7 (#20); `gasp_pawn_bp_full.md` §4 (Get_MovementDirectionAndOffset) |
| `Curve_RotationOffset_F` | `/Game/Blueprints/Data/Curve_RotationOffset_F` | important | §3.1 |
| `Curve_RotationOffset_B` | same dir | important | §3.2 |
| `Curve_RotationOffset_LL` | same dir | important | §3.3 |
| `Curve_RotationOffset_LR` | same dir | important | §3.4 |
| `Curve_RotationOffset_RL` | same dir | important | §3.5 |
| `Curve_RotationOffset_RR` | same dir | important | §3.6 |
| `Curve_RotationOffset_Slide_Knees` | same dir | nice-to-have | §3.7 |
| `Curve_StrafeSpeedMap` | same dir | important | §3.8 |
| `BPI_InteractionTransform` | `/Game/Blueprints/Data/` | skip (deprecated `_Old` suffix) | `gasp_data_model_full.md` §6, §7 (#23) |

**Recommendation:** Port directly — these are needed for AZ's `Get_MovementDirectionAndOffset` (`AZ_HeroPawn.h:499`) to produce the per-direction rotation offsets that feed the steering pipeline. Today the function exists but with no chooser/curves it returns 0 offset → strafe loops can't anchor to actual velocity yaw.

---

## Missing AnimBP nodes / bindings

Status note: `gasp_animbp_full_audit.md` exists at `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\gasp_animbp_full_audit.md` (1156 lines) — fully consumed for this gap list.

| Divergence ID | AZ_ABP_Mover location | Issue | Severity | Source |
|---|---|---|---|---|
| **A** | `AnimGraph.OffsetRootBone` | `RotationHalfLife` and `MaxRotationError` pins NOT bound to `Get_OffsetRootRotationHalfLife()` / `Get_OffsetRootMaxRotationError()` (functions exist in `AZ_AnimInstance.h:871, 875`) | **critical** for spin | `gasp_animbp_full_audit.md` §2.1, §10 (A) |
| **B** | `AnimGraph.PoseSearchHistoryCollector` | `RootBoneRecoveryTime` likely default-zero in AZ; GASP=0.3 | important | `gasp_animbp_full_audit.md` §2.2, §10 (B) |
| **C** | `AnimGraph.StateMachine` "State Controller" | `bAllowConduitEntryStates` likely False; GASP True (required for Grounded/InAir/Slide entry conduits) | important if conduits used | `gasp_animbp_full_audit.md` §2.3, §10 (C) |
| **D** | `AnimGraph.BlendStack` | `BlendOption=HermiteCubic` (AZ) vs `QuadraticInOut` (GASP) | nice-to-have | `gasp_animbp_full_audit.md` §2.4, §10 (D) |
| **E** | `AnimGraph.BlendStack` | `BlendTime` literal `0.2` (AZ) vs `0.5` (GASP) — overridden by binding so benign unless input zero | nice-to-have | `gasp_animbp_full_audit.md` §2.4, §10 (E) |
| **F** | `AnimGraph` | NO `AnimGraphNode_MotionMatching` node; NO `AnimGraphNode_BlendListByInt`. Pure-MM path absent — only SM+BlendStack. | **critical** if `LocomotionSetup=0` (PureMM) is needed; not blocking SM path | `gasp_animbp_full_audit.md` §1, §2.5, §2.8, §10 (F) |
| **G** | `AnimGraph` | No `LocomotionSetup`-driven mode switch in graph (UPROPERTY in C++ but no consumer) | **critical** with F | `gasp_animbp_full_audit.md` §10 (G) |
| **H** | `AnimGraph.BlendStack.InnerGraph` | 31 inner nodes vs GASP 40 — likely missing `Get Curve Value from Animation` chain feeding 2nd Steering, possibly other helper bindings absent | **critical** for spin | `gasp_animbp_full_audit.md` §3, §10 (H) |
| **I** | `AnimGraph.BlendStack.InnerGraph` | Verify all `K2Node_AnimNodeReference` instances (tag `State Machine Blend Stack Input`) are tag-wired to helpers (`Get_DesiredFacing` ×2, `Get_DynamicPlayRate`, `EnableSteering`, `Get_ProceduralTargetTime`, `Get_StrideWarpAlpha`, `Get_StrafeWarpAlpha`). Unwired = silent fail-closed (per `feedback_animgraph_node_reference_wiring.md`). | **critical** for spin | `gasp_animbp_full_audit.md` §3.7, §10 (I) |
| **J** | `UAZ_AnimInstance::IsMoving` (`AZ_AnimInstance.h:910-915`) | Tighter threshold than GASP (`SizeSq>100/1` vs near-zero) — may suppress micro-movements | important | `gasp_animbp_full_audit.md` §5.7, §10 (J) |
| **K** | `UAZ_AnimInstance::AnimationAlmostCompleteThreshold` (`AZ_AnimInstance.h:830`) | Default 0.25s; GASP hardcodes 0.75s | nice-to-have | `gasp_animbp_full_audit.md` §5.36, §10 (K) |
| **L** | `AZ_ABP_Mover` | `OnBecomeRelevant` exists but `Biped_FootPlacement_OnBecomeRelevant` (specific FP-node binding) absent — verify ForceFootPlacementReset wired | important (foot placement) | `gasp_animbp_full_audit.md` §8, §10 (L) |

**Recommendation:** Address A, H, I as part of the spin-fix path (`GASP_SOLUTION.md` Phases 1, 3). B, C, F, G, K, L can wait. D, E are cosmetic.

---

## Missing input handlers / interfaces

| Item | Severity | Status | Source | Recommendation |
|---|---|---|---|---|
| `IMoverInputProducerInterface::ProduceInput` on `AAZ_HeroPawn` | OK — implemented | `AZ_HeroPawn.h:124` | `gasp_pawn_bp_full.md` §10 | No action; verify the implementation packs both `FCharacterDefaultInputs` AND `FAZ_MoverCustomInputs` into the cmd's InputCollection (per `project_session_2026-04-22_gasp_pawn_done.md`). |
| `BPI_SandboxCharacter_Pawn` interface impl on `AAZ_HeroPawn` | OK — implemented as `IAZ_SandboxCharacterPawn` | `AZ_HeroPawn.h:82, 425-428` | `gasp_actor_components_and_notifies.md` §2.1 | No action. AZ uses C++ interface; functionally equivalent to the GASP BPI. |
| `BPI_SandboxCharacter_ABP` interface impl on `UAZ_AnimInstance` (Get_PoseHistory, Get_Gait, Get_InteractionTransform, Set_NotifyTransition_*) | important — NOT implemented | (no impl found in `AZ_AnimInstance.h`) | `gasp_actor_components_and_notifies.md` §2.2 | Re-implement (lightweight): add an `IAZ_SandboxCharacterABP` interface and forward to existing AnimInstance fields. Required for `BP_NotifyState_EarlyTransition` to work on retargeted GASP anims. |
| `BPI_InteractionTransform` (legacy) | skip | — | `gasp_actor_components_and_notifies.md` §2.3 | Skip — `_Old` suffix indicates GASP itself deprecated this. |
| `Update_TwinStickMode` / TwinStick input action | OK — implemented | `AZ_HeroPawn.h:225-231, 372` | `gasp_pawn_bp_full.md` §6 | No action. |
| `Update_ControlRotationRate` (yaw rate per second for camera-snap protection in BP_MovementMode_Walking) | OK — implemented | `AZ_HeroPawn.h:366, 547` | `gasp_pawn_bp_full.md` §4 (function 21), `gasp_movement_modes.md` §1 Phase-3 then_5 | No action. Drives `MoverCustomInputs.ControlRotationRate` which the GASP movement mode uses to short-circuit FacingSmoothingTime when the camera whips fast (§1 then_5 sub-clamp). Confirms Phase 2 of the SOLUTION will benefit from this already-existing wiring. |
| `Bind Event to On Pre Simulation Tick` (dynamic delegate) | OK — implemented | `AZ_HeroPawn.h:124` (`ProduceInput_Implementation`) | `gasp_pawn_bp_full.md` §6 | Verify in `AZ_HeroPawn.cpp` that `BeginPlay` calls `CharacterMoverComponent->OnPreSimulationTick.AddDynamic(this, &…)` per GASP pattern. |
| `Add Tick Prerequisite Component(SkeletalMesh, CharacterMover)` | important — verify | (likely in `AZ_HeroPawn.cpp BeginPlay`) | `gasp_pawn_bp_full.md` §6 | Verify mesh ticks AFTER mover sim. Without this, AnimInstance reads stale Mover state. |

---

## Missing animation curves

These are baked into individual locomotion anim assets via the GASP AnimModifiers. Without them, downstream BP functions (`Get_DynamicPlayRate`, `Get_StrideWarpAlpha`, `Get_StrafeWarpAlpha`, `EnableSteering`, etc.) return defaults and visible glitches occur.

| Curve | Used by | Severity | Source | AnimModifier to bake it |
|---|---|---|---|---|
| `Phase` (oscillates 0/1 between feet) | PoseSearch / MM pose-matching → wrong-foot starts if missing | **critical** for MM quality | `gasp_actor_components_and_notifies.md` §4 (AM_BakePhase…), §5 (#1) | `AM_BakePhaseCurveFromFootstepNotifies` (depends on `Foley_Walk_L`/`_R` notify placements first) |
| `movedata_speed` | `Get_DynamicPlayRate` (anim playback rate scaling vs capsule speed) | **critical** for foot-skating | `gasp_actor_components_and_notifies.md` §4 (AM_MoveData_Speed), §5 (#2); `gasp_animbp_full_audit.md` §5.17 | `AM_MoveData_Speed` |
| `Enable_Warping` | `Get_StrideWarpAlpha`, `Get_StrafeWarpAlpha` | important | `gasp_animbp_full_audit.md` §5.18, §5.19; `gasp_actor_components_and_notifies.md` §4 (AM_WarpingAlpha) | `AM_WarpingAlpha` |
| `Enable_StrideWarping` | `Get_StrideWarpAlpha` | important | `gasp_animbp_full_audit.md` §5.18 | (per-anim manual or modifier) |
| `Enable_StrafeWarping` | `Get_StrafeWarpAlpha` | important | `gasp_animbp_full_audit.md` §5.19 | (per-anim manual or modifier) |
| `Enable_PlayRateWarping` | `Get_DynamicPlayRate` | important | `gasp_animbp_full_audit.md` §5.17 | `AM_RateWarpingAlpha` (per `gasp_actor_components_and_notifies.md` §4) |
| `SteeringTargetTime` | `Get_DesiredFacing`, `Get_ProceduralTargetTime` | **critical** for steering | `gasp_animbp_full_audit.md` §5.15, §5.16 | (per-anim manual) |
| `Enable_TurnInPlaceSteering` | `ShouldReEnterTurnInPlace` (AZ_AnimInstance.h:719) | important | `gasp_sm_tip_flow.md` (TIP re-enter rule); `gasp_animbp_full_audit.md` §10 (TIP rule) | (per-anim manual on TIP anims) |
| `contact_l`, `contact_r` | Foot pinning (`Wants To Lock` on Control Rig) | important (foot IK) | `gasp_framework_cameras_rigs.md` §4 | `AM_FootSteps_Walk` / `AM_FootSteps_Run` per gait |
| `Distance_From_Ledge` | Traversal warp targets | important (traversal) | `gasp_actor_components_and_notifies.md` §4 (AM_DistanceFromLedge) | `AM_DistanceFromLedge` |
| `MovingTraversal` | `JustTraversed` (AZ_AnimInstance.h:735) | nice-to-have (traversal) | `gasp_animbp_full_audit.md` §5 area | (per-anim manual on traversal anims) |

**Recommendation:** Apply in batch via the GASP AnimModifier assets (port `AM_BakePhaseCurveFromFootstepNotifies`, `AM_MoveData_Speed`, `AM_WarpingAlpha`, `AM_RateWarpingAlpha` first; see `gasp_actor_components_and_notifies.md` §6 priority list). Run on every locomotion anim in `Content/AZ/Assets/RTG/NoWeapons/RootMotions/` (per `reference_noweapon_anim_catalog.md`).

---

## Cosmetic but harmless

These are GASP features AZ can legitimately skip without breaking parity-of-behaviour:

| Item | Why skip | Source |
|---|---|---|
| `BFL_HelpfulFunctions` (10 debug-draw helpers) | AZ has its own debug pipeline; GASP's are pure debug overlays | `gasp_data_model_full.md` §5; `gasp_framework_cameras_rigs.md` §7 |
| `S_DebugGraphLineProperties` struct | Debug-only; only used by `BFL_HelpfulFunctions::DebugDraw_MultiLineGraph` | `gasp_data_model_full.md` §2.8 |
| GM_Sandbox `CyclePawn`/`CycleVisualOverride` (DDCvar driven) | Pure dev/PIE QoL for swapping characters at runtime | `gasp_framework_cameras_rigs.md` §1 |
| PC_Sandbox `TeleportToTarget` cheat | Dev cheat | `gasp_framework_cameras_rigs.md` §2 |
| 5× `BP_Manny`/`BP_Quinn`/`BP_Echo`/`BP_Twinblast`/`BP_UE4_Mannequin` retargeted variants | Demo content | `gasp_framework_cameras_rigs.md` §5 |
| `ABP_GenericRetarget` | Demo retarget AnimBP for variant meshes | `gasp_framework_cameras_rigs.md` §5 |
| `AC_PreCMCTick` (CMC variant) | Mover replaces this natively | `gasp_actor_components_and_notifies.md` §1.1 |
| `AC_VisualOverrideManager` | Demo mesh-swap | `gasp_actor_components_and_notifies.md` §1.3 |
| `AC_SmartObjectAnimation` + `BP_SmartBench` + StateTree tasks | NPC interaction polish — can re-port if/when NPCs land | `gasp_framework_cameras_rigs.md` §6 |
| `FoleyEvent` AnimNotify variants (10 sub-classes) | Audio polish; depends on `AC_FoleyEvents` + `DABP_FoleyAudioBank` | `gasp_actor_components_and_notifies.md` §3.1, §3.2 |
| `BPI_InteractionTransform` (`_Old` suffix) | GASP itself deprecated | `gasp_actor_components_and_notifies.md` §2.3; `gasp_data_model_full.md` §6 |
| `SandboxCharacter_CMC` + `SandboxCharacter_CMC_ABP` | CMC parallel implementation; AZ committed to Mover | `gasp_framework_cameras_rigs.md` §7 |
| `SandboxCharacter_CMC.Ragdoll_Start/End` | Ragdoll API; AZ has none yet, can add when needed | `gasp_framework_cameras_rigs.md` §7 |

---

## Quick file-path reference

- AZ pawn header: `C:\UE57\Games\AZ\Source\AZ\Public\Character\AZ_HeroPawn.h`
- AZ pawn cpp: `C:\UE57\Games\AZ\Source\AZ\Private\Character\AZ_HeroPawn.cpp` (1277 lines)
- AZ AnimInstance header: `C:\UE57\Games\AZ\Source\AZ\Public\Animation\AZ_AnimInstance.h`
- AZ AnimInstance cpp: `C:\UE57\Games\AZ\Source\AZ\Private\Animation\AZ_AnimInstance.cpp` (1552 lines)
- AZ locomotion types: `C:\UE57\Games\AZ\Source\AZ\Public\Animation\AZ_LocomotionTypes.h`
- AZ pawn BP: `C:\UE57\Games\AZ\Content\AZ\Blueprints\Character\Hero\AZ_BP_HeroPawn.uasset`
- AZ AnimBP: `C:\UE57\Games\AZ\Content\AZ\Blueprints\Animation\AZ_ABP_Mover.uasset`
- AZ engine config: `C:\UE57\Games\AZ\Config\DefaultEngine.ini`
- GASP movement modes (imported into AZ): `/Game/Blueprints/MovementModes/BP_MovementMode_{Walking,Falling,Slide}`
- GASP pawn (imported into AZ): `/Game/Blueprints/SandboxCharacter_Mover`
- GASP AnimBP (imported into AZ): `/Game/Blueprints/SandboxCharacter_Mover_ABP`
- GASP curves (need import): `/Game/Blueprints/Data/Curve_RotationOffset_*`, `/Game/Blueprints/Data/CHT_RotationOffsetCurve`
- Memory audit files (all under `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\`):
  - `gasp_movement_modes.md` (387 lines) — Mover modes deep audit
  - `gasp_pawn_bp_full.md` (360 lines) — pawn BP audit
  - `gasp_actor_components_and_notifies.md` (264 lines) — AC components, BPI interfaces, notifies
  - `gasp_data_model_full.md` (461 lines) — structs, enums, curves
  - `gasp_framework_cameras_rigs.md` (355 lines) — GM/PC, cameras, rigs, smart-objects
  - `gasp_animbp_full_audit.md` (1156 lines) — AnimBP per-node deep audit
