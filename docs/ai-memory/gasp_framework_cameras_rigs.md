---
name: GASP Framework + Cameras + Rigs
description: Deep audit of GM_Sandbox, PC_Sandbox, Cameras/, ControlRigs/, RetargetedCharacters/, SmartObject/, plus any uncovered top-level BPs. Every BP class, variable, function, and behavior.
type: reference
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# GASP Framework + Cameras + Rigs — Deep Audit

Source: live BP inspection of GameAnimationSample (imported into AZ at `/Game/Blueprints/`) on 2026-05-03 via unrealclaude MCP.

Scope: GM_Sandbox, PC_Sandbox, Cameras/, ControlRigs/, RetargetedCharacters/, SmartObject/, AI/, plus other top-level BPs not covered by the pawn / AnimBP / MovementMode / AC_* / BPI / AnimNotifies / AnimModifiers / Data audits.

---

## 1. GM_Sandbox (`/Game/Blueprints/GM_Sandbox`)

**Parent:** `GameModeBase`

### Variables
| Name | Type | Category | Notes |
|---|---|---|---|
| `PawnClasses` | `TArray<TSubclassOf<APawn>>` | Classes | Cycle list of selectable pawns (CMC + Mover variants) |
| `VisualOverrides` | `TArray<TSubclassOf<AActor>>` | Classes | Cycle list of retargeted character actors (Manny, Quinn, Echo, Twinblast, UE4 Mannequin) |

### Functions
- **`GetDefaultPawnClassForController(InController)`** → `TSubclassOf<APawn>` — Calls Parent first, then returns `BFL_HelpfulFunctions.GetPawnClassWithCVAR(PawnClasses, DefaultPawnClass)`. CVAR = `DDCvar.PawnClass` (int index into `PawnClasses`).
- **`CyclePawn()`** — Reads `DDCvar.PawnClass` int, increments `(prev+1) % len(PawnClasses)`, executes console command `DDCvar.PawnClass <newIndex>`. Wraps. The DataDrivenCVar then fires `OnDataDrivenCVarChanged` → `ResetAllPlayers`.
- **`CycleVisualOverride()`** — Same pattern with `DDCvar.VisualOverride`. Out-of-range → resets to `-1` (no override).
- **`ResetAllPlayers()`** — `for i in 0..GetNumPlayerControllers()-1`: get PC, get controlled pawn, cache its transform + base aim rotation, destroy pawn, `RestartPlayerAtTransform(PC, savedTransform)`, then `SetControlRotation(cachedRot)`.

### EventGraph
- **BeginPlay** → `DataDrivenCVars` engine subsystem → `BindEventToOnDataDrivenCVarDelegate(OnDataDrivenCVarChanged)`.
- **`OnDataDrivenCVarChanged(CVarName)`** custom event → `Switch on String`: case `DDCvar.PawnClass` → `ResetAllPlayers()`. (Other cases default-fall.)

**Architecture insight:** GM acts as a runtime-switchable pawn factory driven entirely by data-driven console variables. No GAS, no save data, just hot-swap during PIE.

---

## 2. PC_Sandbox (`/Game/Blueprints/PC_Sandbox`)

**Parent:** `PlayerController`

### Variables
| Name | Type | Notes |
|---|---|---|
| `Characters` | `TArray<SandboxCharacter_CMC_C*>` | Editor-set list (unused in runtime logic — vestigial?) |
| `Current Character Index` | `int32` | unused at runtime |
| `CachedControlRotation` | `Rotator` | scratch for `TeleportToTarget` and reset flow |
| `TeleportMaxDistance` | `float` | Teleport trace length |
| `TraceStart`, `TraceEnd`, `HitResult` | local-style instance vars used by Teleport function |

### Functions
- **`TeleportToTarget()`** — Sphere-trace from eye-point in forward direction × `TeleportMaxDistance` (radius 32, channel TraceTypeQuery1, ignores controlled pawn). On hit: `SetActorLocation(hit.Location + hit.ImpactNormal * 50)` with sweep=false, teleport=true; on miss: teleport to `TraceEnd`. Plain "ghost" debug cheat — no validation against blockers.
- `NextPawn` (Server RPC, RELIABLE) → `GameMode->CyclePawn()`.
- `NextVisualOverride` (Server RPC, RELIABLE) → `GameMode->CycleVisualOverride()`.

### EventGraph
- `IA_NextPawn` → call `NextPawn` (replicated to server).
- `IA_NextVisualOverride` → call `NextVisualOverride`.
- `IA_TeleportToTarget` → `TeleportToTarget()`.
- **Tick** — Mobile-platform branch only: hide virtual joystick when gamepad connected (Android/iOS). Marked TODO in source.

**Architecture insight:** Vanilla PC. The interesting behavior (cycling, teleporting) is driven entirely by EnhancedInput actions on the local client, replicated to the GM via reliable RPCs.

---

## 3. Cameras (`/Game/Blueprints/Cameras/`)

GASP uses the **GameplayCameras** plugin (UE 5.7's new procedural camera framework). 20 assets in this folder:

### Top-level
| Asset | Class | Role |
|---|---|---|
| `CameraAsset_SandboxCharacter` | `CameraAsset` | Top-level camera "blueprint" — references the CameraDirector |
| `CameraDirector_SandboxCharacter` | `BlueprintCameraDirectorEvaluator` | The runtime evaluator — picks rigs each frame |
| `CHT_CameraRig` | `ChooserTable` | Maps `S_CharacterPropertiesForCamera` → `CameraRigAsset` |
| `E_CameraStyle` | `UserDefinedEnum` | Camera style enum (FreeCam / Strafe / Aim / etc., values driven by `DDCvar.CameraStyle`) |
| `E_CameraMode` | `UserDefinedEnum` | Distance mode (Close / Medium / Far / DebugView) |

### Rigs (`Rigs/`) — 12 CameraRigAsset
Persistent base layer:
- `CameraRig_BaseDefaults` — applied via `ActivatePersistentGlobalCameraRig`
- `CameraRigPrefab_BasicThirdPersonBehavior` — applied via `ActivatePersistentBaseCameraRig`
- `CameraRig_CrouchOffset`, `CameraRig_CollisionOffset`, `CameraRig_TwinStick` — offset overlays

Switchable mode/style rigs (chosen via CHT_CameraRig per frame):
- `CameraRig_Close_Aim`, `CameraRig_Close_Strafe`, `CameraRig_Close_Freecam`
- `CameraRig_Medium_Aim`, `CameraRig_Medium_Strafe`, `CameraRig_Medium_Freecam`
- `CameraRig_Far_Aim`, `CameraRig_Far_Strafe`, `CameraRig_Far_Freecam`
- `CameraRig_DebugView`

(CameraRigAsset properties are all on internal node graphs — not exposed via Python `dir()`. The asset is opened in the dedicated Camera Rig editor.)

### CameraDirector_SandboxCharacter (BPTYPE_Normal, parent `BlueprintCameraDirectorEvaluator`)

**Variable:** `CharacterPropertiesForCamera : S_CharacterPropertiesForCamera` (cached per tick — fields: `CameraStyle`, `CameraMode`, `Gait`, `Stance`).

**Event Graph:**
- **`Event ActivateCameraDirector`** (one-shot at start) →
  1. Read `DDCvar.CameraStyle` int → branch `!= 3` → `ActivatePersistentGlobalCameraRig(<base prefab>)` then `ActivatePersistentBaseCameraRig(...)`.
  2. (Else case = DebugView mode skips the persistent base layer.)
- **`Event RunCameraDirector(DeltaTime, EvaluationContextOwner, Params)`** (every frame):
  1. `FindEvaluationContextOwnerActor(Pawn class)` → cast to actor implementing `BPI_SandboxCharacter_Pawn`.
  2. Call interface `Get_PropertiesForCamera()` on pawn → store in `CharacterPropertiesForCamera`.
  3. Sequence:
     - **then_0:** `Switch on E_Stance` (from cached properties) → on `Crouching` (Enumerator1) → `ActivateCameraRig(CameraRig_CrouchOffset)`.
     - **then_1:** Read `DDCvar.CameraStyle` → `Make S_CharacterPropertiesForCamera(CameraStyle=cvar, CameraMode=cached, Stance=cached)` → `Evaluate Chooser: CHT_CameraRig` → `ActivateCameraRig(result)`.

**Architecture insight:** The director runs every frame, evaluates a Chooser table mapping (style, mode, gait, stance) → rig, and activates it. A persistent base layer is set once at activation. Crouch is handled as an additive overlay rig. All actual lag/FOV/boom values live inside the `CameraRigAsset` node graphs (not BP-readable).

### S_CharacterPropertiesForCamera struct fields
- `Gait : E_Gait` (byte)
- `Stance : E_Stance` (byte)
- `CameraStyle : E_CameraStyle` (byte)
- `CameraMode : E_CameraMode` (byte)

---

## 4. ControlRigs (`/Game/Blueprints/ControlRigs/`)

Only **one** rig: `CR_Biped_FootPlacement` (parent: `ControlRig`). Used as a Post-Process AnimBP rig for foot IK on all sandbox character meshes.

### Variables — 47 total, organised by category:

**Initialization:** `LeftLegLength`, `RightLegLength`, `FootToToeAxis_Left/Right`

**Graph Inputs:** `Ground Normal`, `Has Teleported`

**Default:** `Pelvis Height Offset`, `Pelvis Offset`, `WorldZDamper`, `WorldZDamperVelocity`, `WorldZPrev`, `LeftSmoothGroundWorldTransform`, `RightSmoothGroundWorldTransform`, `PendingReset`, `Adjust Feet Targets For Collision`

**Pinning:** `LeftFootPinned`, `LeftPinWeight`, `LeftFootPinTransformWorldSpace`, `LeftFootPinAnimTransformWorldSpace`, `RightFootPinned`, `RightPinWeight`, `RightFootPinTransformWorldSpace`, `RightFootPinAnimTransformWorldSpace`

**Config (tuning):** `Roll Limit Degrees`, `DebugDraw`, `DoRaycast`, `Enable Foot Pinning`, `Enable Slope Warping`, `MaxFootPinRadius`, `ClampFootMinHeightToReferencePose`, `FootContactLockThreshold`, `PinYawLimitDegrees`, `AllowHeelLift`, `Toe Length`, `Hyper Extension Limit Factor`, `Pelvis Smoothing Time`, `PinBlendOutSmoothingTime`, `HeelUpAngleDegrees_Start`, `HeelUpAngleDegrees_End`, `Floor Smoothing Time`, `UseIKBoneTargets`, `WorldZDamperEnabled`, `RootDamperSmoothingTime`, `ForceReset`, `Debug Draw Scene Query`

**Alphas:** `WorldZDamperAlpha`, `SlopeWarpingAlpha`

### Functions — 26 sub-graphs, 1076 nodes total
`Update Leg Controls`, `Init Leg`, `Solve Leg IK`, `Calculate Limb Length`, `Clamp Pelvis Offset`, `Update Floor Control`, `Do Scene Query`, `DebugDrawLegs`, `Wants To Lock`, `Update Pelvis Control`, `Update Foot Pinning`, `DebugDrawFootPins`, `ComputePinnedToeTransform`, `CalculateTargetAnimatedToeTransformGlobal`, `ClampTransformMinHeightFromReference`, `CollidePositionWithPlane`, `AdjustPositionToPlane`, `Update Foot Post Solve`, `CalculateSlopeAngle`, `Reset`, `UpdateRootDamper`, `CheckEarlyOut`, `DebugDrawExternalIKTargets`, `DebugDrawGroundCollision`, `CalculatePinnedToeTargetTransformGlobal_SubGraph`, `AlphaLinearInterp_SubGraph`

**Bones controlled:** L/R legs (foot, calf, thigh) via two-bone IK; pelvis offset for terrain conforming; toe roll for heel-lift on slopes.

**Inputs the rig takes:** `Ground Normal` (from anim — typically traced floor normal), `Has Teleported` (suppresses interp on warp).

**Curves consumed:** Foot contact curves (`contact_l`, `contact_r`) drive `Wants To Lock` for foot pinning; per-foot motion curves drive `Update Foot Pinning`. Pelvis is offset to keep root height stable while feet conform.

**Architecture insight:** This is a fully procedural foot-IK + pelvis-warp rig. AZ already has a **copy** of this checked in as `CR_AZ_Hero_FootPlacement.uasset` (see git status). It's wrapped in a Post-Process AnimBP that runs after the main ABP.

---

## 5. RetargetedCharacters (`/Game/Blueprints/RetargetedCharacters/`)

### 5 actor BPs (BP_Manny, BP_Quinn, BP_Echo, BP_Twinblast, BP_UE4_Mannequin)

All 5 have **identical structure** (parent `Actor`, no own variables, identical event graph topology — 12 nodes, 3 events: BeginPlay, ActorBeginOverlap, Tick):

**EventGraph (BeginPlay):**
- `Delay Until Next Tick` → loop pattern: `RootComponent.GetChildComponent(0)` → cast SceneComponent → `GetAttachParent()` chained twice (climbs the SCS hierarchy) → `IsValid` → `AddTickPrerequisiteComponent(self, parent)`.

This makes the visual override mesh tick **after** the parent character's mesh, so the retargeted skeleton sees the most recent parent pose. This is the **only** logic in these BPs — the actual character data (mesh + animBP) is set up via SCS components in the editor (which we can't introspect from BP query).

Each variant uses:
- A different `SkeletalMeshComponent` (Manny mesh / Quinn mesh / Echo / Twinblast / UE4 Mannequin)
- The **same `ABP_GenericRetarget` AnimBP** with a different `IKRetargeter_Map` tag so it picks the correct IK Retargeter at runtime.

**ActorBeginOverlap & Tick** — both events present but with no body (vestigial).

### ABP_GenericRetarget

**Parent:** `AnimInstance`. Used as the AnimBP for ALL retargeted variant meshes (UE5 mannequin → Echo / Twinblast / UE4 mannequin). Pulls pose from the parent SK mesh and retargets it.

**Variables:**
- `IKRetargeter : UIKRetargeter*` — currently active retargeter
- `IKRetargeter_Map : TMap<FName, UIKRetargeter*>` — keyed by the first ComponentTag on the owning SK mesh (the tag is literally the IK Retargeter asset name)
- `RetargetProfile : FRetargetProfile`

**Event Graph:**
- **`Event Blueprint Initialize Animation`** + **`Event Blueprint Update Animation`** both lead to:
  1. `GetOwningComponent().ComponentTags[0]` → look up in `IKRetargeter_Map` → `Set IKRetargeter`
  2. `CopyRetargetProfileFromRetargetAsset(IKRetargeter)` → `Set RetargetProfile`
  3. Call `UpdateRetargetProfile(RetargetProfile)` (function below) and stash result.

**Function `UpdateRetargetProfile(InputProfile)` → `FRetargetProfile`:**
- Sequence (3 branches):
  1. `Get Op Controller from Retarget Profile (InputProfile, "Retarget IK Goals")` → cast to `IKRetargetIKChainsController` → `GetSettings()` → for each `RetargetIKChainSettings` in `ChainsToRetarget`, look up its `TargetChainName` in `ChainCurveMap` (a member map FName→FName). If found, get the curve value (anim curve), and lerp `BlendToSource`/`BlendToSourceTranslation`/`BlendToSourceRotation`/`BlendToSourceWeights`/`StaticOffset`/`StaticLocalOffset`/`StaticRotationOffset`/`ScaleVertical`/`Extension` between defaults and target values based on curve alpha. Write back via `SetArrayElem`. Call `SetSettings(...)` to push back. (The map `ChainCurveMap` is referenced but NOT in the variable list above — must be a member I missed; could also be hidden / a class member from inheritance.)

**Architecture insight:** A single AnimBP that adapts to ANY skeleton — it picks the right IK Retargeter from the SK mesh's tag, then dynamically lerps IK chain blending using anim curves. Useful for: when the user enables "rifle aiming" curve on the retarget character, IK goals blend differently. **AZ doesn't currently do retargeted character demos** — would only matter if AZ ever supports player-character swapping or 3rd-party DLC characters.

---

## 6. SmartObjects (`/Game/Blueprints/SmartObjects/`)

### Core actor: BP_SmartObject_Base (parent `Actor`)
- No own variables, only `UserConstructionScript`. Acts as base class — interesting behavior is in components (likely a `USmartObjectComponent` set up in SCS). Subclasses (`BP_SmartBench`) extend with a `SmartAreaClaimCollisionSphere : USphereComponent*`.

### AC_SmartObjectAnimation (the brain) — parent `ActorComponent`

This is **the** key SmartObject component. Lives on the user character (NPC). 10 variables, 8 named functions, 6 events.

**Variables:**
| Name | Type |
|---|---|
| `Owner Skeletal Mesh` | `SkeletalMeshComponent*` |
| `Incoming Animation Payload` | `S_SmartObjectAnimationPayload` |
| `Number Of Loops` | `int32` |
| `Warp Target Name` | `FName` |
| `Owner Montage Finished` | multicast delegate |
| `Ignore Character Movement Server Corrections` | `bool` |
| `Character Movement Component` | `UCharacterMovementComponent*` |
| `Smart Object Selection Inputs` | `S_SmartObjectSelectionInputs` |
| `Smart Object Selection Outputs` | `S_SmartObjectSelectionOutputs` |
| `Mover Component` | `UMoverComponent*` |

**Functions:**
- `Cache Necessary Data` — On BeginPlay, find `Owner Skeletal Mesh`, `Character Movement Component`, `Mover Component` from owner.
- `Try Add Warp Target` — If `Incoming Animation Payload.UseWarpTarget`, get owner's `MotionWarpingComponent`, call `AddOrUpdateWarpTargetFromTransform(WarpTargetName, payload.WarpTargetTransform)`.
- `Set Ignore Collision State(bShouldIgnore, OtherActor)` — toggles ignore-collision pair between actors during anim.
- `Set Ignore Character Movement Correction(IgnoreCorrections)` — sets bool on CMC to suppress server reconciliation during scripted anim.
- `NPC Approach Angle and Distance(Destination)` → distance + angle from owner to destination (used for scoring).
- `Evaluate Distance and Motion Match(Destination, ProxyTable)` → `(AnimMontage, Cost, StartTime)` — calls `BPI_SandboxCharacter_ABP::Get_PoseHistory`, builds `S_SmartObjectSelectionInputs (TargetDistance, TargetAngle, PoseHistoryNode)`, calls `EvaluateProxy: CHPA_SmartObject` (proxy asset chooser) → returns best-matching montage + start-time + cost (Pose Search!).
- `Is Mover()` → bool — true when CMC is null (so owner is Mover-based).
- `Setup Play Timer` — internal scheduling.

**EventGraph (Networked Events — API):**
- **`PlaySmartObjectMontage(payload, ignoreCorrections)`** — entry point called by State Tree. Sets vars, branches `Is Mover`, calls `Try Add Warp Target`, then either `PlayMontage` (CMC) or `PlayMontageOnMoverActor` (Mover). Wires `OnNotifyBegin` / `OnBlendOut` to `EvaluationMontageLoopAndExit` macro for loop handling. On loop end → `On Montage Finished Requested` → broadcasts `Owner Montage Finished` delegate (which State Tree has bound).
- **`PlayMontage_Multi`** — RELIABLE multicast (server→all). Replicates the play.
- **`SetIgnoreActorState`** + **`SetIgnoreState_Multi`** — replicated collision-ignore pair state.
- **`On Montage Finished Requested`** — calls delegate.

**Architecture insight:** This is the **animation orchestrator** for SmartObjects. Pose-search picks the best matching anim from a proxy table based on approach distance + angle + pose history → motion-warp aligns the character → montage plays with looping support → server-replicated → CMC corrections suppressed during scripted anim. Decoupled from the SmartObject definition itself.

### BP_SmartBench (subclass of `BP_SmartObject_Base`)
- Adds `SmartAreaClaimCollisionSphere : USphereComponent*` (claim trigger volume).
- All other behaviour comes from the SmartObject Subsystem + `SO_BenchDefinition` (the SmartObjectDefinition asset) + `ST_SmartObject_Bench` (StateTree) + `CHT_SmartObject_BenchAnim` (Chooser per slot).

### Bench data assets
| Asset | Class | Role |
|---|---|---|
| `SO_BenchDefinition` | `SmartObjectDefinition` | Defines slots, tags, behaviors |
| `ST_SmartObject_Bench` | `StateTree` | Bench-specific state-tree (Sit / Idle / Stand) |
| `CHT_SmartObject_BenchAnim` | `ChooserTable` | Picks bench animations (sit / idle variations) |
| `CHPT_SmartObject_Bench` | `ProxyTable` | Bench-specific proxy table for Evaluate Proxy |

### Top-level SmartObject data
| Asset | Class |
|---|---|
| `CHPA_SmartObject` | `ProxyAsset` (PoseSearch proxy asset, references `PSS_SmartObject` schema) |
| `PSS_SmartObject` | `PoseSearchSchema` |
| `DistanceToSmartObject` | `PoseSearchFeatureChannel_Distance` (custom BP-channel: `BP_GetDistance(ChooserEvaluationContext) → float` — reads target distance from selection inputs struct) |
| `SmartObjectAnimationPayload` | `UserDefinedStruct` — fields: `MontageToPlay`, `PlayTime`, `RandomPlaytimeVariance`, `StartTime`, `Playrate`, `NumLoops`, `WarpTargetTransform`, `UseWarpTarget` |
| `SmartObjectSelectionInputs` | `UserDefinedStruct` — `TargetDistance`, `TargetAngle`, `PoseHistoryNode` |
| `SmartObjectSelectionOutputs` | `UserDefinedStruct` — `Cost`, `StartTime` |
| `SmartObjectSearchType` | `UserDefinedEnum` (Closest, Random, FurthestUp-to-N, etc — 4 enumerators) |

### State Tree Tasks (`SmartObjects/TasksAndConditions/`)

All parent `StateTreeTaskBlueprintBase` (or `ConditionBlueprintBase` / `EvaluatorBlueprintBase`).

- **STT_FindSmartObject** — Vars: `Actor` (context), `SearchType`, `Search Box Extents`, `Smart Object` (output), `CandidateSlot` (output), `SearchResults`, `Index`, `Search Result`. EnterState: clear results, build `MakeSmartObjectRequestFilter(UserTags = SmartObject.ObjectType.NPC)`, `MakeSmartObjectRequest(QueryBox = box around actor, Filter)`, call `SmartObjectSubsystem.FindSmartObjects()` → switch on `SearchType` (Closest / Random / FurthestUp / etc.), pick a slot, `GetSmartObjectComponentByRequestResult` → `GetOwner` → set output `Smart Object`. Function `Find Slot Using Distance(In Results, Closest)` → SmartObjectRequestResult.
- **STT_ClaimSlot** — Vars: `Actor`, `ClaimedHandle` (output), `SlotToBeClaimed`, `SmartObject`. Function `IsSlotFreeOrFindNewSlot()` → SmartObjectSlotHandle.
- **STT_UseSmartObject** — Vars: `AIController`, `Claimed Handle`. EnterState: `IsValid(claimHandle)` branch → latent `Use Smart Object with Gameplay Interaction(AIController, ClaimHandle, bLockAILogic=true)` → on succeeded/failed/moveto-failed → FinishTask(true/false).
- **STT_PlayAnimMontage** — Vars: `Actor`, `Montage to Play`, `Start Time`, `Play Time`, `Play Time Variance`, `Play Rate`, `Number Of Loops`, `Ignore Collision`, `Mover Component`, `Slot Handle`, `Smart Object Anim Component`, `Smart Object Actor`. EnterState: get `AC_SmartObjectAnimation` from owner, bind `OnAnimationFinished` to `OwnerMontageFinished`, get slot transform from subsystem, call `PlayMontage_Multi(payload built from vars)`. On finished → FinishTask.
- **STT_PlayAnimFromBestCost** — Vars: `Actor`, `Animation Proxy Table : UProxyTable*`, `CostThreshold`, `Claimed Handle`, `Destination`, `MaximumDistanceThreshold`, `Smart Object Animation Component`, `Minimum Velocity Check`, `Needs Evaluation`, `PossibleOwnerMoverComponent`. Functions: `NPC Approach Angle and Pathed Distance` → distance, `Setup Task`, `Get Actor Velocity`. Calls `Evaluate Distance and Motion Match` on the AC component to get best (anim, cost, startTime) and gates by `CostThreshold`.
- **STT_AddCooldown** — Vars: `CooldownName : FString`, `Cooldown Tag : FGameplayTag`, `Cooldown Time : double`, `AIController`. Stores cooldowns somewhere on the AIC.
- **STC_CheckCooldown (StateTreeConditionBlueprintBase)** — Vars: `AIController`, `CooldownName`. `ReceiveTestCondition() → bool`, `ReceiveGetDescription(Formatting) → FText`.
- **STE_GetAIData (StateTreeEvaluatorBlueprintBase)** — Vars: `Actor` (context), `AIController` (output). Just resolves AIController from Actor.

### AI/StateTree shared tasks (`AI/StateTree/TasksAndConditions/`)

- **STT_CharacterIgnoreCollisionsWithOtherActor** — task wrapping ignore-collision toggle.
- **STT_ClearFocus** — clears AIC focus.
- **STT_FindRandomLocation** — Vars: `AIReference`, `SearchRadius`, `Random Location` (output). Wraps NavSystem random reachable point.
- **STT_FocusToTarget** — sets AIC focus.
- **STT_SetCharacterInputState** — Vars: `Character : APawn*`, `WantsToWalk : bool`. Calls `BPI_SandboxCharacter_Pawn::Set_CharacterInputState(InputState)` to drive the NPC's gait/walk via the same input pipeline as the player.

### AIC_NPC_SmartObject (parent `AIController`)
- Var: `Cooldowns : FString` (semicolon-joined list — naive cooldown registry).
- EventGraph: BeginPlay → `if !IsDedicatedServer` → `Delay(8s)` then `StateTreeAI.StartLogic()`. Else: `Delay(2s)` then `StateTreeAI.StartLogic()`. Boots NPC AI after a small grace period.

**Architecture insight:** Complete SmartObject + StateTree + PoseSearch pipeline. Player or NPC uses an object via:
1. `STT_FindSmartObject` discovers slots within bounding box, filtered by gameplay tag.
2. `STT_ClaimSlot` reserves it.
3. (NPC) Move to it via Use Smart Object task.
4. `STT_PlayAnimFromBestCost` evaluates Pose Search → picks best entry/transition anim that matches current pose history & approach distance.
5. `STT_PlayAnimMontage` plays it on `AC_SmartObjectAnimation` which handles motion warping + replication + loop control.
6. On exit: `STT_AddCooldown` to prevent re-entry.

---

## 7. Other top-level / uncovered BPs

### Interfaces
- **`BPI_SandboxCharacter_Pawn`** — 4 funcs: `Get_PropertiesForAnimation() → S_CharacterPropertiesForAnimation`, `Get_PropertiesForCamera() → S_CharacterPropertiesForCamera`, `Get_PropertiesForTraversal() → S_CharacterPropertiesForTraversal`, `Set_CharacterInputState(S_PlayerInputState)`. **The pawn-to-camera contract.**
- **`BPI_SandboxCharacter_ABP`** — 6 funcs: `Get_PoseHistory() → FPoseHistoryReference`, `Get_InteractionTransform() → FTransform`, `Set_InteractionTransform(FTransform)`, `Set_NotifyTransition_ReTransition(bool)`, `Set_NotifyTransition_ToLoop(bool)`, `Get_Gait() → E_Gait`. **The ABP-to-everything-else contract.**
- **`BPI_InteractionTransform`** (in `Data/`) — 3 OLD funcs: `SetInteractionTransform_Old(Transform)`, `GetInteractionTransform_Old() → Transform`, `Get_PoseHistory_Old() → PoseHistoryReference`. Vestigial — replaced by `BPI_SandboxCharacter_ABP`.

### `BFL_HelpfulFunctions` (`Data/`, parent `BlueprintFunctionLibrary`)
10 functions, mostly debug:
- `DrawDebugArrowWithCircle(...)` — combined arrow + circle + axis + label
- `DrawDebugAngleThresholds(...)` — visualize yaw threshold cones
- `DebugDraw_MultiLineGraph(...)` — runtime graph visualization (multi-series)
- `DebugDraw_BoolStates(...)` — list of named bool indicators
- `DebugDraw_StringArray(...)` — labeled string list with one highlighted entry
- `DebugDraw_ObjectNameArray(...)` — list of object names
- `AddToStringHistoryArray(...)` — push-and-trim to FIFO
- `GetObjectNames(Objects[]) → FString[]`
- **`GetPawnClassWithCVAR(PawnClasses, DefaultPawnClass) → TSubclassOf<APawn>`** — reads `DDCvar.PawnClass` int, returns array element or default. **Used by GM_Sandbox.**
- **`GetVisualOverrideWithCVAR(VisualOverrides) → AActor*`** — reads `DDCvar.VisualOverride` int.

### `SandboxCharacter_CMC` (parent `Character`) — NOT in MovementMode-agent scope but related
The CMC-based variant of the sandbox character (parallel to `SandboxCharacter_Mover`). 20 vars, 19 funcs, 13 events, 166 nodes. Same conceptual API as Mover variant: `MovementStickMode`, `Gait`, `WalkSpeeds/RunSpeeds/SprintSpeeds (Vector)`, `JustLanded`, `LandVelocity`, `CharacterInputState (S_PlayerInputState)`, `IsRagdolling`, `OnRequestInteract` delegate. Functions: `GetMovementInputScaleValue`, `SetupInput`, `UpdateMovement_PreCMC`, `UpdateRotation_PreCMC`, `SetupCamera`, `GetDesiredGait`, `CalculateMaxAcceleration/BrakingDeceleration/BrakingFriction/GroundFriction/MaxSpeed/MaxCrouchSpeed`, `GetTraversalCheckInputs → S_TraversalCheckInputs`, `HasMovementInputVector`, `CanSprint`, `UpdatedMovementSimulated(OldVelocity)`, `Ragdoll_Start`, `Ragdoll_End`. Includes `StrafeSpeedMapCurve : UCurveFloat*` for analogue stick → speed mapping. **Has an explicit `Ragdoll_Start/End` flow** that AZ's HeroPawn does NOT.

### `SandboxCharacter_CMC_ABP` (parent `AnimInstance`)
The CMC-variant ABP. (Inspecting omitted for brevity — parallel to `SandboxCharacter_Mover_ABP` audited elsewhere.)

---

## 8. Gaps / Things AZ Could Port

### High value (recommend porting):
1. **CR_Biped_FootPlacement (Control Rig)** — Foot IK + pelvis offset. AZ already has a copy — needs a Post-Process AnimBP wrapper to actually run it.
2. **GameplayCameras Camera framework** — `CameraAsset` + `CameraDirector` + `CameraRigAsset` + `Chooser` is Epic's recommended UE 5.7 camera pattern. Decouples camera from pawn cleanly. AZ uses inline `SetActorLocation`/`InterpTo` per-stance — this is the right replacement for AZ's bespoke camera-offset interp.
3. **AC_SmartObjectAnimation + StateTree task suite** — Adds NPC interaction (sit on bench, lean, etc.) and PoseSearch-driven entry animations. Critical for any survival game with shelters / craft stations / interactables that aren't tiny pickups. Hooks naturally into Mover (uses `PlayMontageOnMoverActor`).

### Medium value:
4. **ABP_GenericRetarget pattern** — `IKRetargeter_Map` keyed by ComponentTag is a clean way to support any 3rd-party character. Useful if AZ ever supports cosmetic character swaps.
5. **DataDrivenCVar driven pawn cycling (GM)** — Useful for dev/test workflow only. Nice-to-have during PIE iteration.
6. **BFL_HelpfulFunctions debug draw library** — Particularly `DebugDraw_BoolStates`, `DebugDraw_StringArray with highlighted`, `DrawDebugArrowWithCircle`. Ports cleanly as a UBlueprintFunctionLibrary.
7. **STT_SetCharacterInputState** — Lets State Tree drive NPC movement through the same `S_PlayerInputState` pipeline as the player. Simplifies NPC anim parity.

### Low value (skip):
- BP_Manny / BP_Quinn / BP_Echo / BP_Twinblast / BP_UE4_Mannequin — pure demo content.
- PC_Sandbox `TeleportToTarget` — dev cheat.
- AIC_NPC_SmartObject — too thin to port standalone; would design new for AZ AI.

### Key insight for camera porting:
Camera lag, FOV interp curves, boom offsets, pre-render delegates — all of these live INSIDE the `CameraRigAsset` node graph (opaque to BP query). Porting requires opening each rig in the Camera Rig editor and recreating the node graph in AZ's parallel asset. The `CameraDirector` BP graph is the simple bit — just chooser eval + activate. The complexity is in 12 rig assets, each authored visually.

---

## Quick lookup — file paths
- GM_Sandbox: `/Game/Blueprints/GM_Sandbox`
- PC_Sandbox: `/Game/Blueprints/PC_Sandbox`
- Camera director: `/Game/Blueprints/Cameras/CameraDirector_SandboxCharacter`
- Camera asset: `/Game/Blueprints/Cameras/CameraAsset_SandboxCharacter`
- Camera chooser: `/Game/Blueprints/Cameras/CHT_CameraRig`
- Camera rigs: `/Game/Blueprints/Cameras/Rigs/CameraRig_*`
- Foot IK rig: `/Game/Blueprints/ControlRigs/CR_Biped_FootPlacement`
- Generic retarget ABP: `/Game/Blueprints/RetargetedCharacters/ABP_GenericRetarget`
- Smart object anim component: `/Game/Blueprints/SmartObjects/AC_SmartObjectAnimation`
- Smart object base: `/Game/Blueprints/SmartObjects/BP_SmartObject_Base`
- Bench example: `/Game/Blueprints/SmartObjects/Bench/BP_SmartBench`
- Pawn interface: `/Game/Blueprints/BPI_SandboxCharacter_Pawn`
- ABP interface: `/Game/Blueprints/BPI_SandboxCharacter_ABP`
- Debug BFL: `/Game/Blueprints/Data/BFL_HelpfulFunctions`
