---
name: gasp_pawn_cpp_port_plan
description: Full inventory of GASP SandboxCharacter_Mover — variables, functions, events, graphs — with phased C++ port plan for AZ_HeroPawn
type: project
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# GASP SandboxCharacter_Mover → AZ_HeroPawn C++ Port Plan

**Source path:** `/Game/Blueprints/SandboxCharacter_Mover`
**Parent:** `APawn` (not ACharacter)
**Scale:** 18 variables, 23 functions, 5 events, 142 event-graph nodes, 722 total nodes

## Variables (18)

| Name | Type | Category |
|---|---|---|
| MoverDefaultInputs_PreSim | FCharacterDefaultInputs | Mover |
| MoverDefaultInputs_PostSim | FCharacterDefaultInputs | Mover |
| MoverCustomInputs_PreSim | S_MoverCustomInputs | Mover |
| MoverCustomInputs_PostSim | S_MoverCustomInputs | Mover |
| MovementModeMap | FName | Mover |
| FloorNormal | FVector | Mover |
| FloorLocation | FVector | Mover |
| PlayerInputState | S_PlayerInputState | Input |
| Jump_JustPressed | bool | Input |
| TwinStickMode | bool | Input |
| TwinStickAimRotation | FRotator | Input |
| LastControlRotation | FRotator | Input |
| ControlRotationRate | double | Input |
| TargetableActors | TArray<Actor*> | Default |
| TargetedActor | Actor* | Default |
| SlidingAudioComponent | UAudioComponent* | Default |
| SpeedHistory | TArray<float> | Debug |
| DebugAngle | double | Debug |

## Functions (23)

### Setup / Lifecycle
- `UserConstructionScript` — component init
- `SetupInput` — enhanced input context setup
- `SetupCamera` — camera setup

### Getters (derived state)
- `Get_CurrentMovementMode()` → E_MovementMode (byte)
- `Get_MoveInput()` → FVector
- `Get_AimingRotation()` → FRotator
- `Get_RotationMode()` → E_RotationMode (byte)
- `Get_OrientationIntent()` → FVector
- `Get_Gait()` → E_Gait (byte)
- `Get_Speed()` → double

### Movement Direction
- `Get_MovementDirectionThresholds()` → S_MovementDirectionThresholds (FL/FR/BL/BR doubles)
- `Get_MovementDirectionFromThresholds(Thresholds, Direction)` → E_MovementDirection
- `Get_MovementDirectionAndOffset()` → (E_MovementDirection, RotationOffset)

### Updates (per-tick or event-driven)
- `OnPreSimulateTick(TimeStep, InputCmd)` — **main entrypoint** — builds CharacterDefaultInputs + S_MoverCustomInputs and feeds Mover
- `OnMovementModeChanged(PreviousModeName, NewModeName)` — bound to Mover delegate
- `CacheInputsFromMover` — pulls last-sim inputs into `_PostSim` vars for anim/camera
- `Update_ControlRotationRate` — tracks controller yaw delta/sec
- `Update_FloorValues` — tracks floor normal/location from Mover
- `Update_TwinStickMode` — detects right-stick input, toggles `TwinStickMode`
- `Update_TargetedActor` — cycles through `TargetableActors`
- `Update_SlidingAudio` — plays slide loop sound

### Traversal
- `GetTraversalCheckInputs()` → S_TraversalCheckInputs (trace params for traversal)

### Debug
- `DebugDraws` — visual overlays

## Events (5)

- `BeginPlay` — binds OnMovementModeChanged delegate, adds tick prerequisite (mover ticks before pawn)
- `Possessed` — calls SetupCamera, SetupInput
- `Possessed_ClientReplicated From Server` — custom event for client-side setup
- `Tick` — calls CacheInputsFromMover, Update_* methods, DebugDraws
- Input events: IA_Jump, IA_Crouch, IA_Sprint, IA_Walk, IA_Strafe, IA_Aim, IA_Move, IA_Look, IA_Look_Gamepad + Mouse Wheel (camera style cycling), D-pad (camera / pawn cycling)

## Pre/Post Sim Input Pattern (critical)

GASP maintains TWO copies of Mover inputs:
- `_PreSim` — set locally each frame before Mover simulates. **Not replicated.**
- `_PostSim` — retrieved from Mover AFTER simulation via `CacheInputsFromMover`. **Replicated by Mover.**

Animation/camera/anim-trajectory must use `_PostSim` for network correctness.

## Interfaces

- **BPI_SandboxCharacter_Pawn** (4 funcs): Get_PropertiesForAnimation, Get_PropertiesForCamera, Get_PropertiesForTraversal, Set_CharacterInputState
- **IPoseSearchTrajectoryPredictorInterface** (on Mover predictor) — already wired
- **IMoverInputProducerInterface::ProduceInput_Implementation** — GASP's entry for feeding Mover

## Current AZ_HeroPawn state (what we have vs. what's missing)

| GASP feature | AZ status |
|---|---|
| APawn parent | ✓ |
| CharacterMover component | ✓ |
| MoverTrajectoryPredictor | ✓ (lazy-init pattern) |
| PlayerInputState struct | ✓ (just added) |
| Sprint/Walk/Strafe input bindings | ✓ (just added) |
| AlignControllerWithActor | ✓ |
| Idle TIP accumulator | ✓ (AZ-specific, speed-independent) |
| MoverDefaultInputs_PreSim/PostSim variables | ✗ |
| MoverCustomInputs_PreSim/PostSim variables | ✗ |
| FloorNormal / FloorLocation | ◑ (via MoverStateProxy only, no FRotator-fed Update_FloorValues) |
| Get_Gait | ✗ (bools exist but no derivation) |
| Get_RotationMode | ✗ |
| Get_OrientationIntent | ◑ (simplified version inline in OnProduceInput) |
| Get_MoveInput | ◑ (inline CachedMoveInputIntent) |
| Get_AimingRotation | ◑ (inline `CharInputs.ControlRotation`) |
| Get_Speed | ✗ |
| Get_MovementDirectionAndOffset | ✗ |
| OnPreSimulateTick | ◑ (partial — OnProduceInput does some of this) |
| CacheInputsFromMover | ✗ |
| Update_ControlRotationRate | ✗ |
| Update_FloorValues | ✗ |
| Update_TwinStickMode | ✗ |
| Update_TargetedActor | ✗ |
| Update_SlidingAudio | ✗ |
| OnMovementModeChanged binding | ✗ |
| BPI_SandboxCharacter_Pawn interface | ✗ |
| GetTraversalCheckInputs | ✗ |
| DebugDraws | ◑ (partial — only anim debug, no pawn debug) |

## Phased Port Plan

### Phase 1 — Input state plumbing (foundation)
- Add Pre/Post sim copies: `MoverDefaultInputs_PreSim/PostSim`, `MoverCustomInputs_PreSim/PostSim`, `Jump_JustPressed`.
- Define or reuse `FAZ_MoverCustomInputs` struct (already exists in LocomotionTypes).
- `CacheInputsFromMover()` — pull `GetLastInputCmd` from Mover after sim; store into `_PostSim`.

### Phase 2 — Derivation functions
- `Get_MoveInput()` → world-space vector (rotated by camera yaw from `CachedMoveInputIntent`).
- `Get_AimingRotation()` → PC's control rotation.
- `Get_Speed()` → `Mover->GetVelocity().Size2D()`.
- `Get_CurrentMovementMode()` → map Mover's movement mode FName to E_MovementMode via `MovementModeMap`.
- `Get_RotationMode()` → from `PlayerInputState.bWantsToAim/bWantsToStrafe` + `TwinStickMode`.
- `Get_Gait()` → from `bWantsToSprint/bWantsToWalk` + move input magnitude + rotation mode.
- `Get_OrientationIntent()` → replace the current inline idle-TIP logic with the GASP per-mode matrix (OrientToMovement/Strafe/Aim × Walking/Falling/Sliding).

### Phase 3 — OnPreSimulateTick rewrite
- Replace `OnProduceInput_Implementation` with a GASP-parity version that:
  1. Calls `Get_MoveInput`, `Get_AimingRotation`, `Get_OrientationIntent`, `Get_Gait`, `Get_RotationMode`.
  2. Packs `MoverDefaultInputs_PreSim` + `MoverCustomInputs_PreSim`.
  3. Copies them into the `InputCmdResult`.

### Phase 4 — MovementDirection helpers
- Port `Get_MovementDirectionThresholds` (returns FL=70, FR=70, BL=110, BR=110 degrees typically).
- Port `Get_MovementDirectionFromThresholds(thresholds, direction_degrees)` → E_MovementDirection (F/FL/L/BL/B/BR/R/FR).
- Port `Get_MovementDirectionAndOffset()` — returns movement direction + rotation offset (used by Aim mode for strafing).

### Phase 5 — Periodic updates (called from Tick)
- `Update_ControlRotationRate()` — tracks yaw rate for "is player rotating camera fast?" checks.
- `Update_FloorValues()` — reads floor normal/location from Mover blackboard or SyncState.
- `Update_TwinStickMode()` — polls IA_TwinStick_AimDirection, toggles TwinStickMode bool when stick is deflected.
- `Update_TargetedActor()` — D-pad cycles through TargetableActors.

### Phase 6 — Movement mode change binding
- Bind `UMoverComponent::OnMovementModeChanged` delegate in BeginPlay.
- Handler: stores previous/new, updates local state (e.g., clear slide audio on exit Slide mode).

### Phase 7 — Side-systems
- `Update_SlidingAudio()` — attach/detach audio component based on current movement mode.
- `GetTraversalCheckInputs()` — trace params for vault/climb detection (later when traversal lands).
- `DebugDraws()` — on-screen overlays.

### Phase 8 — Interfaces
- `IAZ_SandboxCharacter_Pawn` interface (port BPI_SandboxCharacter_Pawn).
  - `GetPropertiesForAnimation(out FAZ_CharacterPropertiesForAnimation)` — already have the struct.
  - `GetPropertiesForCamera(out FAZ_CharacterPropertiesForCamera)` — already have the struct.
  - `GetPropertiesForTraversal(out FAZ_CharacterPropertiesForTraversal)` — later.
  - `SetCharacterInputState(FAZ_PlayerInputState)` — allows external systems to override input state.

### Phase 9 — GAS reintegration
- Once pure-GASP flow works end-to-end, re-introduce GAS ability triggers for Jump/Crouch/Aim etc.
- GAS abilities set their respective bits in `PlayerInputState` (e.g., GA_Aim sets `bWantsToAim=true`).
- This bridges GASP's input-state-bool pattern with GAS.

## Execution order recommendation

Do Phases 1–3 together — that's the minimum to have a "working pawn" by GASP's definition (inputs flow into Mover correctly via Pre/Post sim). Phases 4–8 are additive (nothing breaks if you skip them, the character just can't turn in place / aim properly / traverse). Phase 9 happens last.

## Already-implemented AZ divergences to reconcile

- AZ's idle TIP uses **accumulated yaw** (speed-independent, our own design) instead of GASP's threshold check on current delta. Keep AZ version — it's strictly better than GASP's for our feel.
- AZ's `ShouldTurnInPlace` mirrors `HeroPawn->IsIdleTurnInProgress()` (single source of truth). Keep.
- AZ uses basic `UWalkingMode` (not `USmoothWalkingMode` like GASP). May want to switch later for parity with GASP's `FacingSmoothingTime`/`TurningStrength`.
- AZ's OffsetRootBone enum fix (Interpolate=1, Release=5) — already correct.
