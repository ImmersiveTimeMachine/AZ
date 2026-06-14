---
name: GASP Pawn BP Full Audit
description: Complete deep audit of /Game/Blueprints/SandboxCharacter_Mover — every component, all vars/funcs/events, input handlers, Mover input packing logic, OrientationIntent generation, trajectory predictor calls. Use as reference for AAZ_HeroPawn parity audits.
type: reference
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# GASP `SandboxCharacter_Mover` Pawn — Full BP Audit

Audited 2026-05-03 against the live BP imported into AZ at `/Game/Blueprints/SandboxCharacter_Mover` (UE 5.7.4). Source: GameAnimationSample by Epic. Parent class: `Pawn` (NOT `Character`, NOT `MoverComponent`-aware base).

Cross-reference companion: `gasp_pawn_cpp_port_plan.md`, `project_gasp_pawn_port_audit_2026-05-02.md`.

---

## 1. Parent Class & Interfaces

- **Parent class:** `/Script/Engine.Pawn` (a plain `APawn` — not `ACharacter`, not GASP-specific subclass).
- **Implemented interfaces (BPI):**
  - **`BPI_SandboxCharacter_Pawn`** (`/Game/Blueprints/BPI_SandboxCharacter_Pawn`) — 4 functions:
    - `Get_PropertiesForAnimation()` → `S_CharacterPropertiesForAnimation`
    - `Get_PropertiesForCamera()` → `S_CharacterPropertiesForCamera`
    - `Get_PropertiesForTraversal()` → `S_CharacterPropertiesForTraversal`
    - `Set_CharacterInputState(Desired Input State: S_PlayerInputState)`  ← event handler in EventGraph at (32, 4256). Sets `PlayerInputState` member directly.
  - **`BPI_SandboxCharacter_ABP`** — likely on the ABP, not on the pawn.
  - The Mover input producer interface (`IMoverInputProducerInterface`) is **not enumerable via BP query** but is implied — its `ProduceInput` function is the canonical place where Mover pulls input. Comment in EventGraph: "*To find where Mover takes inputs, open the 'Produce Input' function in the INTERFACES tab.*"  Implementation lives outside the function_graphs list; details below in §10.

---

## 2. Components (SimpleConstructionScript)

13 unique components. Hierarchy:

```
SandboxCharacter_Mover (Actor)
├─ Capsule [CapsuleComponent]                   ← root, default scene root
│  ├─ SkeletalMesh [SkeletalMeshComponent]      ← rel_loc(0, 0, -88), rel_rot(0, -90, 0)
│  │  ├─ GameplayCamera [GameplayCameraComponent]
│  │  └─ VisualOverride [ChildActorComponent]
│  └─ SpringArm [SpringArmComponent]
│     └─ Camera [CameraComponent]               ← FOV=90, no PawnControlRotation (handled by SpringArm)
├─ CharacterMover [CharacterMoverComponent]     ← non-scene, Mover plugin core
├─ AC_TraversalLogic_C [BP component]
├─ MotionWarping [MotionWarpingComponent]
├─ AC_FoleyEvents_C [BP component]
├─ AC_VisualOverrideManager_C [BP component]
├─ AC_SmartObjectAnimation_C [BP component]
└─ NavMover [NavMoverComponent]                 ← Mover plugin nav-driver helper
```

### Per-component editor properties (only non-default values worth noting):

**Capsule (root):**
- `capsule_radius=30`, `capsule_half_height=86`
- mobility=Movable

**SkeletalMesh:**
- `skeletal_mesh_asset = /Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin`
- `anim_class = /Game/Blueprints/SandboxCharacter_Mover_ABP_C`
- **`relative_location = (0, 0, -88)`** — **CRITICAL** alignment offset
- **`relative_rotation = (Pitch=0, Yaw=-90, Roll=0)`** — **CRITICAL** mesh-faces-X axis correction
- receives_decals=True, cast_shadow=True, always_create_physics_state=False, component_use_fixed_skel_bounds=False
- Attached to **Capsule** (NOT to mesh's own root logic).

**SpringArm:**
- `target_arm_length=300`
- `socket_offset=(0,0,0)`, `target_offset=(0,0,0)`
- `do_collision_test=True`
- **`use_pawn_control_rotation=True`**, **`inherit_pitch=True`**, **`inherit_yaw=True`**, **`inherit_roll=True`**
- `enable_camera_lag=True`, `camera_lag_speed=10`
- `enable_camera_rotation_lag=False`, `camera_rotation_lag_speed=10`
- `probe_size=12`, `probe_channel=ECC_Camera`
- Attached to **Capsule**.

**Camera:**
- `field_of_view=90`
- **`use_pawn_control_rotation=False`** (rotation comes via SpringArm)
- aspect_ratio=1.7777, post_process_blend_weight=1
- Attached to **SpringArm**.

**GameplayCamera:**
- Optional alternate camera using `GameplayCameraComponent` (UE5 GameplayCamera plugin). Activated only when CVar `DDCVar.NewGameplayCameraSystem.Enable` is true. See `SetupCamera` graph.
- Attached to **SkeletalMesh**.

**VisualOverride (ChildActorComponent):**
- For swapping visual mesh at runtime via `AC_VisualOverrideManager`.
- Attached to SkeletalMesh.

**CharacterMover:**
- `starting_movement_mode = "Falling"`
- `movement_modes = {`
  - `"Walking" → BP_MovementMode_Walking_C` (`/Game/Blueprints/MovementModes/BP_MovementMode_Walking`)
  - `"Falling" → BP_MovementMode_Falling_C` (`/Game/Blueprints/MovementModes/BP_MovementMode_Falling`)
  - `"Flying" → /Script/Mover.FlyingMode` (engine default, no override)
  - `"Sliding" → BP_MovementMode_Slide_C` (`/Game/Blueprints/MovementModes/BP_MovementMode_Slide`)
- `}`
- `transitions = []` (empty)
- `shared_settings`:
  - `CommonLegacyMovementSettings_0`: `max_speed=300`, `jump_upwards_speed=500`, `ground_friction=0.0`, `braking_friction=8`, `braking_friction_factor=0`, `max_step_height=40`
  - `StanceSettings_0`: `crouch_half_height=60`

**NavMover:** vanilla, used by `Get_MoveInput` to pull AI nav input via `Consume Nav Movement Data`.

**MotionWarping:** vanilla, used by traversal logic for warping into climb/vault targets.

**AC_TraversalLogic_C:** has function `TryTraversalAction(Inputs: S_TraversalCheckInputs, DebugType)` returning `TraversalCheckFailed, MontageSelectionFailed`. Has `DoingTraversalAction` bool. Driven by Jump input from EventGraph — see `GetTraversalCheckInputs` and EventGraph IA_Jump branches.

**AC_FoleyEvents_C:** plays gameplay-tagged audio events: `Foley.Event.Slide.Loop`, `Foley.Event.Land`, `Foley.Event.Jump`. Triggered from `OnMovementModeChanged`.

**AC_VisualOverrideManager_C:** swaps `VisualOverride` ChildActor visual.

**AC_SmartObjectAnimation_C:** smart-object interaction.

### Pawn CDO defaults (non-default):
- `base_eye_height=64.0`
- `auto_possess_player=Disabled`
- `auto_possess_ai=PlacedInWorld`
- `ai_controller_class=AIController`
- `use_controller_rotation_pitch/yaw/roll=False` (the SpringArm handles rotation inheritance)
- `spawn_collision_handling_method=AdjustIfPossibleButDontSpawnIfColliding`
- `replicates=True`, `replicate_movement=False`

---

## 3. Variables (18 total)

All `instance_editable=True`, no `expose_on_spawn`.

| Var | Type | Category | Default |
|---|---|---|---|
| `MoverDefaultInputs_PreSim` | `FCharacterDefaultInputs` (engine struct) | Mover | empty |
| `MoverCustomInputs_PreSim` | `FS_MoverCustomInputs` (project struct: RotationMode, Gait, Stance, MovementDirection, WantsToCrouch, ...) | Mover | empty |
| `MoverDefaultInputs_PostSim` | `FCharacterDefaultInputs` | Mover | empty |
| `MoverCustomInputs_PostSim` | `FS_MoverCustomInputs` | Mover | empty |
| `PlayerInputState` | `FS_PlayerInputState` (toggles: WantsToSprint, WantsToWalk, WantsToStrafe, WantsToAim, WantsToCrouch) | Input | empty |
| `Jump_JustPressed` | bool | Input | false |
| `MovementModeMap` | TMap\<FName, E_MovementMode\> | Mover | `{Walking→0, Falling→1, Sliding→2, Flying→3}` |
| `SpeedHistory` | TArray\<float\> | Debug | [] |
| `TargetableActors` | TArray\<AActor*\> | Default | [] |
| `TargetedActor` | AActor* | Default | None |
| `DebugAngle` | double | Debug | 0 |
| `SlidingAudioComponent` | UAudioComponent* | Default | None |
| `TwinStickMode` | bool | Input | false |
| `TwinStickAimRotation` | FRotator | Input | (0,0,0) |
| `LastControlRotation` | FRotator | Input | (0,0,0) |
| `ControlRotationRate` | double | Input | 0 |
| `FloorNormal` | FVector | Mover | (0,0,0) |
| `FloorLocation` | FVector | Mover | (0,0,0) |

Plus implicit cached locals seen in graph reads:
- `PreviousMovementMode`, `NewMovementMode` (E_MovementMode) — set in `OnMovementModeChanged`
- `NewMovementDirection` (E_MovementDirection), `MovementDirectionAngle` (float), `DirectionOfMovement` (FVector) — set in `Get_MovementDirectionAndOffset`
- `PlayerController` (APlayerController*) — cached in `SetupCamera`

---

## 4. Functions (23 total)

| Function | Inputs | Outputs | Summary |
|---|---|---|---|
| `UserConstructionScript` | — | — | Empty (1 node) |
| `SetupInput` | — | — | Cast Controller→PlayerController, get EnhancedInputLocalPlayerSubsystem, **Add Mapping Context** (priority 0, `bIgnoreAllPressedKeysUntilRelease=True`). IMC ref is hidden in node param. |
| `SetupCamera` | — | — | Caches PlayerController. If CVar `DDCVar.NewGameplayCameraSystem.Enable=true` → activate `GameplayCamera` and `Activate Camera for Player Controller` (push). Else → `SetViewTarget` to self with `Camera` component active. |
| `Get_CurrentMovementMode` | — | `E_MovementMode` | `CharacterMover.GetMovementModeName()` → `MovementModeMap.Find(name)` → enum value. |
| `Get_MoveInput` | — | `FVector` | If controller is AIController → `NavMover.ConsumeNavMovementData()` → normalized OutMoveInputVelocity. Else → `IA_Move` (Vector2D) → `ToVector` → `RotateVector` by `GetControlRotation()` (yaw-only via Get Forward/Right) → `ClampVectorSize(0,1)` → `Normalize`. **Camera-space** input. |
| `Get_AimingRotation` | — | `FRotator` | If `TargetedActor` valid → `RotationFromXVector(target.location - actor.location)`. Elif `TwinStickMode` → `TwinStickAimRotation`. Else → `GetControlRotation()`. |
| `Get_RotationMode` | — | `E_RotationMode` (OrientToMovement, Strafe, Aim) | Priority: TargetedActor → forces Aim if `WantsToAim` else Strafe. TwinStickMode → if RightStick deflected: aim/strafe per WantsToAim, else OrientToMovement. Default: WantsToAim→Aim, WantsToStrafe→Strafe, else OrientToMovement. |
| `Get_OrientationIntent` | — | `FVector` | **See §5 — verbatim.** |
| `Get_Gait` | — | `E_Gait` (Walk, Run, Sprint) | If `!WantsToSprint && WantsToWalk` → Walk. Else: switch on RotationMode. **OrientToMovement**: branch on `DDCVar.AnalogInputStyle` — 0: WantsToSprint→Sprint else Run. 1: stick magnitude > 0.8 → Run else Walk. **Strafe**: WantsToSprint AND `dot(MoveInput, OrientationIntent) > threshold` (-0.1, +0.5, or -0.1 based on `DDCvar.StrafeStyle`) → Sprint, else Run. **Aim**: limit to Run (no sprint when aiming). |
| `Get_Speed` | — | double | `VectorLengthXY(CharacterMover.GetVelocity())` |
| `DebugDraws` | — | — | Large debug visualization, skipped (per port-checklist policy). |
| `Get_MovementDirectionThresholds` | — | `S_MovementDirectionThresholds` (FL, FR, BL, BR) | Switch on RotationMode. **OrientToMovement** → (-60,60,-120,120). **Strafe/Aim** → switch on `DDCvar.StrafeStyle` / `DDCvar.AimStyle` (0/1/2): **0**=(-40,40,-140,140) all-4-dir; **1**=(-140,140,-140,140) F+B-only; **2**=(-180,180,-180,180) F-only. |
| `Get_MovementDirectionFromThresholds` | Thresholds: S_MovementDirectionThresholds, Direction: double | E_MovementDirection | InRange tests against FL,FR,BL,BR to pick F/FL/FR/B/BL/BR/L/R enum value. |
| `Get_MovementDirectionAndOffset` | — | MovementDirection: E_MovementDirection, RotationOffset: double | If `RotationMode == OrientToMovement` → return F + 0 offset (and clear DebugAngle). Else: cache `DirectionOfMovement` (Falling/Sliding modes use `Velocity.Normalized.XY` selected via Select on MovementMode, else use `MoveInput`). Compute `MovementDirectionAngle = Delta(RotationFromX(DirectionOfMovement), RotationFromX(OrientationIntent)).Yaw` (clamped via composite `ClampAngleToPreventConstantFlippingAt180`). Lookup MovementDirection via `Get_MovementDirectionFromThresholds`. **Sprint fallback**: if `Gait == Sprint`, force MovementDirection=F. RotationOffset comes from `EvaluateChooser(CHT_RotationOffsetCurve)` with inputs `(MovementMode, MovementDirection)` → `CurveFloat`, then `GetFloatValue(MovementDirectionAngle)`. |
| `GetTraversalCheckInputs` | — | S_TraversalCheckInputs (TraceForwardDirection, TraceForwardDistance, TraceOriginOffset, TraceEndOffset, TraceRadius, TraceHalfHeight) | Switch on MovementMode. **Walking**: forward dir from Velocity.XY (normalized) if MoveInput non-zero else Velocity-fallback; distance `MapRangeClamped(speedXY, 0..375 → 75..300)`; trace radius=30, half-height=60. **Falling**: forward dir from velocity normalized (Z=0); distance=75; origin=0,0,0; end=0,0,50; radius=30; half-height=86. Other modes have specific defaults. |
| `Update_SlidingAudio` | — | — | If `SlidingAudioComponent` valid → set "Speed" float param to `VectorLength(CharacterMover.GetVelocity())` via `IAudioParameterControllerInterface::SetFloatParameter`. |
| `CacheInputsFromMover` | — | — | `CharacterMover.GetLastInputCmd()` → break MoverInputCmdContext → MoverDataCollection → `Get Data From Collection<CharacterDefaultInputs>` → `Set MoverDefaultInputs_PostSim`; same for `S_MoverCustomInputs` → `Set MoverCustomInputs_PostSim`. **Comment**: *"PostSim are values retrieved FROM mover, safe to use to control other systems. PreSim are values sent TO Mover in the Produce Input interface function — non-replicated, locally controlled pawn only."* |
| `Update_TwinStickMode` | — | — | If `DDCvar.ControlStyle == 1` → set `TwinStickMode=true`, set Controller's ControlRotation to (0,0,0). Read `IA_TwinStick_AimDirection` (Vector2D); if non-zero → `TwinStickAimRotation = CombineRotators(ControlRotation, MakeRot(0,0,Atan2(Y,X)*-1))`. Else → cache ActorRotation. |
| `Update_TargetedActor` | — | — | If `TargetableActors` not empty → `TargetedActor = FindNearestActor(self.location, TargetableActors)`; draw debug cone above target. Else → `TargetedActor = None`. |
| `OnMovementModeChanged` | PreviousMovementModeName, NewMovementModeName | — | Map both names → enum cache `PreviousMovementMode`, `NewMovementMode`. **Sequence**: (a) Switch on NewMovementMode: **Sliding**→`AC_FoleyEvents.PlayFoleyEvent("Foley.Event.Slide.Loop", {})` → `Set SlidingAudioComponent`. **Falling**→ later when entering Walking from Falling, `MapRangeClamped(velocity.Z, -500..-900 → 0.5..1.5)` → `Foley.Event.Land`. **Jump check**: if Previous==Walking AND CharacterDefaultInputs.bIsJumpJustPressed → `MapRangeClamped(speedXY, 0..375 → 0.5..1.0)` → `Foley.Event.Jump`. (b) If Previous==Sliding → `SlidingAudioComponent.FadeOut(0.5s)`. (c) If Previous==Walking AND `IsCrouching()` → set `WantsToCrouch=False`, `UnCrouch()` (auto-uncrouch when leaving ground). |
| `OnPreSimulateTick` | TimeStep: FMoverTimeStep, InputCmd: FMoverInputCmdContext | — | Break InputCmd → MoverDataCollection → `Get Data From Collection<S_MoverCustomInputs>` → if `WantsToCrouch` → `CharacterMover.Crouch()` else `CharacterMover.UnCrouch()`. **Bound at BeginPlay via `Bind Event to On Pre Simulation Tick`**. |
| `Update_ControlRotationRate` | — | — | `Delta(GetControlRotation(), LastControlRotation).Yaw / DeltaSeconds` → `Set ControlRotationRate`; `Set LastControlRotation = GetControlRotation()`. **Comment**: *"used by BP_MovementMode_Walking to prevent under-rotation when spinning the camera quickly in strafe and aim modes."* |
| `Update_FloorValues` | — | — | `CharacterMover.TryGetFloorCheckHitResult()` → if hit: `Set FloorLocation = ImpactPoint`, `Set FloorNormal = ImpactNormal`. Else: `Set FloorLocation = SkeletalMesh.GetWorldLocation()`, `Set FloorNormal = (0,0,1)`. |

---

## 5. `Get_OrientationIntent` — Verbatim Logic

This is the most important function for AAZ_HeroPawn parity. It returns the direction the pawn should face. Switches on **Movement Mode** outer, then **Rotation Mode** inner.

**Top comment:** *"This function returns the OrientationIntent, which is the direction we want the pawn to face, and is determined primarily by the Rotation Mode. Since we also apply a rotation offset in some movement modes, and since movement modes often will have smoothing applied to the rotation, this can be thought of as the general direction we want to pawn to orient toward."*

### Switch on `Get_CurrentMovementMode`:

#### **Case Walking (NewEnumerator4):** Switch on RotationMode:
- **OrientToMovement (NewEnumerator0):** *"When on the ground WITH movement input applied, the OrientationIntent will be the movement input direction…"* Actually checked: if `MoverDefaultInputs_PreSim.MoveInput != (0,0,0)` → return `MoveInput`. Else (no input) → return last frame's `MoverDefaultInputs_PreSim.OrientationIntent` (i.e. don't change it). 
- **Strafe (NewEnumerator1):** WITH input → return `GetForwardVector(MakeRot(0,0,GetAimingRotation().Yaw))`. WITHOUT input → return last frame's `OrientationIntent`. 
- **Aim (NewEnumerator2):** WITH input → return `GetForwardVector(MakeRot(0,0,Aiming.Yaw))`. WITHOUT input → if `|Delta(GetActorRotation(), GetAimingRotation()).Yaw| > 60°` → return `GetForwardVector(MakeRot(0,0,Aiming.Yaw))` (basic turn-in-place). Else → return last frame's `OrientationIntent`.

#### **Case Falling (NewEnumerator5):** Switch on RotationMode:
- **OrientToMovement:** return last frame's `OrientationIntent` (no change in air without strafe).
- **Strafe / Aim:** return `GetForwardVector(MakeRot(0,0,Aiming.Yaw))` (always face aim while in air with these modes).

#### **Case Sliding (NewEnumerator6):** Switch on RotationMode:
- **OrientToMovement:** return `Normalize(CharacterMover.GetVelocity())` (face slide direction).
- **Strafe / Aim:** return `GetForwardVector(MakeRot(0,0,Aiming.Yaw))`.

#### **Case Flying / Traversing (NewEnumerator7 etc.):** *"When in the Traversing movement mode (active during traversal montages) set the OrientationIntent to be the actor's forward vector, meaning it will not try to rotate."* → return `GetForwardVector(GetActorRotation())`.

### Key takeaways for AAZ port:
1. **Default = LAST FRAME's value** (read from `MoverDefaultInputs_PreSim.OrientationIntent`) — this is the "no-change" branch. AZ likely needs the same hysteresis pattern: if no input AND not in turn-in-place trigger, **don't write** OrientationIntent.
2. **Aim mode TIP threshold = 60°** (hardcoded). Single-source-of-truth for the `bIdleTurnInProgress` accumulator.
3. **Aiming.Yaw only** (Pitch/Roll stripped) — uses `MakeRot(0,0,Yaw)` then `GetForwardVector`. Equivalent to `FRotator(0, ControlRotation.Yaw, 0).Vector()`.
4. **Sliding-OrientToMovement uses VELOCITY** (not MoveInput) — different from Walking.

---

## 6. EventGraph — Initialization & Tick Chain

5 events, 142 nodes. Major regions:

### **Event BeginPlay** (pos 32, -560)
1. Get CharacterMover.
2. **`Bind Event to On Movement Mode Changed`** (`K2Node_AssignDelegate`) → fires `OnMovementModeChanged`.
3. **`Bind Event to On Pre Simulation Tick`** (`K2Node_AddDelegate`) → fires `OnPreSimulateTick`.
4. `Add Tick Prerequisite Component` — sets `CharacterMover` as prerequisite of `SkeletalMesh` (mesh ticks AFTER mover sim).

### **Event Possessed** (pos 32, -144) and **Event Possessed_ClientReplicated From Server** (CustomEvent, pos 528, -176)
- Both call `EventPossessedClient` (function — replicated to owning client) → which calls `SetupCamera` → `SetupInput`.

### **Event Tick** (pos 32, 112) — chain from left to right:
`Update_FloorValues` → `Update_ControlRotationRate` → `Update_SlidingAudio` → `Update_TargetedActor` → `Update_TwinStickMode` → `CacheInputsFromMover` → `DebugDraws`.

NOTE: `Get_OrientationIntent` and the various Get_* helpers are **not** called in Tick directly — they are called from inside the `ProduceInput` interface implementation (which the BP query API does not enumerate as a discrete graph; see §10).

There is also a dead node at (2080, -560): `Set members in Character Default Inputs` writing `OrientationIntent` — apparently orphaned debug code, no exec connection in or out. Not used.

### **Event Set_CharacterInputState** (pos 32, 4256) — interface from `BPI_SandboxCharacter_Pawn`
- Receives `Desired Input State: S_PlayerInputState` parameter and **directly writes** it to the `PlayerInputState` member. This is how external systems (e.g., AI, scripted sequences) can drive the pawn's input toggles without using EnhancedInput.

### **Input handlers** (all from EnhancedInput):

| InputAction | Behavior |
|---|---|
| `IA_Move` | Used inside `Get_MoveInput` (read-only); not handled in EventGraph. |
| `IA_Look` | `Triggered` → `AddControllerYawInput(X)` + `AddControllerPitchInput(Y)`. |
| `IA_Look_Gamepad` | `Triggered` → `(ActionValue * GetWorldDeltaSeconds()).XY` → `AddControllerYawInput` + `AddControllerPitchInput` (delta-time scaled). |
| `IA_Walk` | `Triggered` (toggle) → flip `PlayerInputState.WantsToWalk`; if turning ON → also clear `WantsToSprint`. |
| `IA_Sprint` | `Triggered` → `PlayerInputState.WantsToSprint = ActionValue` (held bool); also clears `WantsToWalk` when sprint goes true. **Note: this is hold, not toggle.** |
| `IA_Jump` | `Started` → `Set Jump_JustPressed = true`; `Triggered` → if `!AC_TraversalLogic.DoingTraversalAction` → call `AC_TraversalLogic.TryTraversalAction(GetTraversalCheckInputs(), ForOneFrame)`; if traversal failed AND `!DoingTraversalAction` → second `TryTraversalAction(..., ForDuration)` on each tick. After processing → `Set Jump_JustPressed = false`. (The actual jump impulse is applied via Mover when `bIsJumpPressed/bIsJumpJustPressed` is in the input cmd; see §10.) |
| `IA_Crouch` | `Triggered` (toggle) → branch: if currently crouching (`CharacterMover.IsCrouching()`) → set `WantsToCrouch=false` (auto-uncrouch via OnPreSimulateTick); else → set `WantsToCrouch=true`. |
| `IA_Strafe` | `Triggered` (toggle) → flip `PlayerInputState.WantsToStrafe`. |
| `IA_Aim` | `Triggered` → `PlayerInputState.WantsToAim = ActionValue` (held). |
| `IA_TwinStick_AimDirection` | Read inside `Update_TwinStickMode` and `Get_RotationMode`; not handled. |

### **Camera-style swap** (Mouse Wheel / D-pad):
`Mouse Wheel Down` / `Gamepad D-pad Down` → decrement `DDCVar.CameraStyle`; `Mouse Wheel Up` / `D-pad Up` → increment. Clamped 0..3, displayed via Print String "Camera Style: Close/Medium/Far/Debug" + Play Sound 2D, executed via `ExecuteConsoleCommand("DDCVar.CameraStyle N")`.

---

## 7. Camera & Spring Arm Setup

- SpringArm: 300 length, no socket offset, full inheritance (pitch/yaw/roll), camera lag enabled at 10, rotation lag DISABLED (instant rotation tracking), probe channel `ECC_Camera`, probe size 12.
- Camera: FOV 90, no PawnControlRotation (SpringArm carries the rotation).
- **No per-stance camera offset/FOV interp on the pawn.** The GASP pawn delegates camera tuning to the **GameplayCameraComponent** plugin (when CVar enabled), or to an external system (`SetViewTarget`-driven). This is a major architectural divergence from AZ's current per-stance camera tuning baked into `AAZ_HeroPawn`.

---

## 8. Trajectory generation (`PoseSearchGenerateTrajectoryUsingPredictor`)

**ZERO calls** in the pawn BP. The trajectory generation lives in the **AnimBP** (`SandboxCharacter_Mover_ABP`), not in the pawn. This is consistent with the AZ port plan: trajectory is an animation-side concern fed by Mover state. The pawn's role is ONLY to: (a) accept input, (b) write to MoverInputCmd, (c) cache PostSim inputs, (d) expose interface getters that the ABP polls.

---

## 9. Movement Mode classes (CharacterMover.movement_modes)

| Slot | Class path | Role |
|---|---|---|
| Walking | `/Game/Blueprints/MovementModes/BP_MovementMode_Walking.BP_MovementMode_Walking_C` | Custom BP — handles walk/run/sprint speeds, strafe rotation, control rotation rate compensation |
| Falling | `/Game/Blueprints/MovementModes/BP_MovementMode_Falling.BP_MovementMode_Falling_C` | Custom BP — air control, jump impulse handling |
| Flying | `/Script/Mover.FlyingMode` (engine default) | Vanilla |
| Sliding | `/Game/Blueprints/MovementModes/BP_MovementMode_Slide.BP_MovementMode_Slide_C` | Custom BP — slide velocity decay, rotation behavior |

**No "Traversing" registered mode** — traversal montages are root-motion driven, queued via `AC_TraversalLogic`, not a Mover mode. The `Traversing` enum value (NewEnumerator7 in some switches) maps to the `Flying` slot's name OR is a soft state.

---

## 10. Mover Input Packing — How inputs reach Mover

Based on the EventGraph comment ("To find where Mover takes inputs, open the 'Produce Input' function in the INTERFACES tab"), the pawn implements **`IMoverInputProducerInterface::ProduceInput`** (engine C++ interface). The BP query API does NOT enumerate this graph in `function_graphs` (it's an interface event override), but the comment confirms its existence.

**Inferred packing flow (from `MoverDefaultInputs_PreSim` reads in `Get_OrientationIntent`/`Get_Gait`/etc.):**
1. Mover ticks at fixed rate (60Hz per project config).
2. Mover calls `ProduceInput` on the pawn each sim tick.
3. The pawn's `ProduceInput` implementation:
   - Reads `PlayerInputState` (set by EnhancedInput handlers in EventGraph).
   - Reads `Get_MoveInput()`, `Get_OrientationIntent()`, `Get_AimingRotation()`, `Get_Gait()`, `Get_RotationMode()`, `Get_MovementDirectionAndOffset()`.
   - **Writes `MoverDefaultInputs_PreSim`** with: `MoveInput`, `OrientationIntent`, `ControlRotation`, `bIsJumpJustPressed = Jump_JustPressed`, `bIsJumpPressed`, `MoveInputType`.
   - **Writes `MoverCustomInputs_PreSim`** with: `RotationMode`, `Gait`, `Stance`, `MovementDirection`, `WantsToCrouch`.
   - Adds both structs to the outgoing `FMoverInputCmdContext`'s `InputCollection` (a `FMoverDataCollection`).
4. Mover then calls `OnPreSimulateTick(TimeStep, InputCmd)` on the pawn (bound via delegate), which reads InputCmd and triggers `Crouch()`/`UnCrouch()` on the CMC.
5. Mover sims, applies movement, then exposes the consumed input via `GetLastInputCmd()`.
6. Pawn Tick calls `CacheInputsFromMover` → reads `GetLastInputCmd()` → writes `MoverDefaultInputs_PostSim` and `MoverCustomInputs_PostSim` for downstream systems (camera, ABP) to consume.

**Critical for AZ port:** AAZ_HeroPawn must implement `IMoverInputProducerInterface::ProduceInput` (or override the equivalent) that does this exact packing — NOT just write to the structs from Tick. The current AZ implementation likely does this differently (probably writes from Tick → which is the wrong tick-rate).

---

## 11. PostSim handling

`CacheInputsFromMover` runs every Tick, reads `CharacterMover.GetLastInputCmd()` (the last sim input that Mover consumed), and writes:
- `MoverDefaultInputs_PostSim` — for ABP / camera / other systems to read what Mover actually used (replicated, safe to use anywhere).
- `MoverCustomInputs_PostSim` — same for custom inputs.

`Update_FloorValues` is the only other read-back (queries `TryGetFloorCheckHitResult` for cached floor info).

---

## 12. Camera offset / FOV interp per stance

**None on the pawn.** GASP delegates this entirely to:
1. The `GameplayCameraComponent` (when `DDCVar.NewGameplayCameraSystem.Enable=1`) — uses gameplay-camera-rig assets that adjust per-state.
2. The `Camera Style` system (DDCVar.CameraStyle 0..3 — Close/Medium/Far/Debug) — likely just changes spring-arm length via console var/setting.

Per-stance (aim/strafe) camera tuning is **not in the pawn** — this is a divergence from AZ which currently bakes per-stance interp into its hero pawn.

---

## 13. Major divergence opportunities for AAZ_HeroPawn

1. **`Get_OrientationIntent` cache-last-frame behavior** — AZ likely overwrites OrientationIntent every frame. Match the GASP "WITHOUT input → last frame's value" pattern explicitly to prevent jitter and snap.
2. **TIP threshold = 60° hardcoded** — verify AZ's idle TIP accumulator commits at 60° (current docs say 60°, ✓).
3. **Aiming.Yaw-only forward** — confirm AZ uses `FRotator(0, Yaw, 0).Vector()` not the full control-rotation vector.
4. **Mover inputs flow via `IMoverInputProducerInterface::ProduceInput`** — AAZ_HeroPawn must override this interface, packing both `FCharacterDefaultInputs` AND `FS_MoverCustomInputs` (or AZ's equivalent) into the `FMoverInputCmdContext.InputCollection`. Likely the biggest gap.
5. **PreSim/PostSim split** — GASP carefully separates: PreSim = locally controlled & non-replicated (driven by player input each Tick before Mover sim), PostSim = replicated & safe (read from `GetLastInputCmd` after sim). AZ may be conflating these.
6. **OnPreSimulateTick is dynamically bound at BeginPlay** — not auto-called. The AZ pawn must explicitly bind this delegate (via `BindUFunction` / `AddDynamic` in C++).
7. **Tick prerequisite: `SkeletalMesh` ticks AFTER `CharacterMover`** — added at BeginPlay. If AZ doesn't do this, anim updates may use stale Mover state.
8. **Mesh `(0, 0, -88)` location and `(0, -90, 0)` rotation** — standard UE Character offset to align mesh with capsule. Verify AZ matches.
9. **Stance handling via `WantsToCrouch` → OnPreSimulateTick → `CharacterMover.Crouch/UnCrouch`** — NOT direct stance change. AZ should follow this pattern (the Mover plugin owns stance state, not the pawn).
10. **Auto-uncrouch when leaving Walking** — `OnMovementModeChanged` clears `WantsToCrouch` when previous=Walking AND IsCrouching, then calls UnCrouch. Prevents stuck-crouched state when falling.
11. **Foley events tied to MovementModeChanged** — slide-loop start, jump impulse, land impact. Use volume mapped from horizontal speed. Easy to mirror.
12. **Rotation offset comes from `CHT_RotationOffsetCurve` chooser** (inputs: MovementMode + MovementDirection) → `CurveFloat.GetFloatValue(MovementDirectionAngle)`. AZ ABP should drive Mover OrientationIntent rotation offset via this curve evaluation (per-direction, per-mode).
13. **Sprint gait depends on `dot(MoveInput, OrientationIntent) > threshold`** in Strafe mode — different thresholds per `DDCvar.StrafeStyle` (0/1/2). AZ should preserve this gating to prevent sideways/backward sprint anims.
14. **Walk/Sprint are MUTUALLY EXCLUSIVE** — IA_Walk toggle clears Sprint, IA_Sprint hold clears Walk. AZ must mirror the cross-clearing logic.
15. **Camera NOT per-stance on pawn** — AZ's per-stance camera interp logic may belong elsewhere (camera component, gameplay camera) for parity.
16. **No PoseSearch trajectory generation in pawn** — that's purely ABP-side. The pawn only exposes Mover state via interface getters.
17. **GameplayCameraComponent integration via CVar** — feature-flagged camera system; consider whether AZ wants to integrate the same plugin or stick with hard-coded SpringArm/Camera.

---

## 14. Files referenced

- BP path: `/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover`
- Skel mesh: `/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin`
- AnimBP: `/Game/Blueprints/SandboxCharacter_Mover_ABP`
- Movement modes: `/Game/Blueprints/MovementModes/BP_MovementMode_{Walking, Falling, Slide}`
- Interfaces: `/Game/Blueprints/BPI_SandboxCharacter_Pawn`, `/Game/Blueprints/BPI_SandboxCharacter_ABP`
- Sub-components: `AC_TraversalLogic`, `AC_FoleyEvents`, `AC_VisualOverrideManager`, `AC_SmartObjectAnimation`
- Chooser used in `Get_MovementDirectionAndOffset`: `CHT_RotationOffsetCurve` (path TBD; lives in same project tree)
- Audit raw data: `C:\Users\Artur\.claude\projects\C--UE57-Games-AZ\memory\_gasp_audit\` (components.json, cmc_and_vars.json, interfaces_and_settings.json) — kept for re-audit / verification.
