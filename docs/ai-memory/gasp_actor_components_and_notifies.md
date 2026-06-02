---
name: GASP Actor Components + Interfaces + Notifies
description: Deep audit of AC_PreCMCTick, AC_TraversalLogic, AC_VisualOverrideManager, all BPI_SandboxCharacter_* interfaces, all AnimNotifies + AnimModifiers. Includes what each provides and whether AZ should port it.
type: reference
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# GASP Actor Components + Interfaces + Notifies

Read-only audit performed 2026-05-03 against the GASP content imported into AZ at `/Game/Blueprints/`. Inspected via `unreal_blueprint_query` (port 3000, MCP `unrealclaude`).

The directory `/Game/Blueprints/` contains exactly **4 actor components**, **3 BP interfaces**, **17 anim-notify assets** (10 BP_AnimNotify + 2 BP_NotifyState + 5 enums/Foley sub-banks), and **17 anim modifiers**.

---

## 1. Actor Components

### 1.1 `AC_PreCMCTick` — Pre-CMC tick prerequisite hook (CMC ONLY)

| Path | `/Game/Blueprints/AC_PreCMCTick.AC_PreCMCTick` |
|---|---|
| Parent | `UActorComponent` |
| Vars | `As CBP Sandbox Character` (`SandboxCharacter_CMC_C*`), `Tick` (multicast delegate, no params) |
| Funcs | none — pure event-driven |
| Events | `EventTick` → calls `Tick.Broadcast()` |

**Comment from the BP**: *"This is a simple component which allows us to execute certain functions on the CBP before the CMC, using the tick pre-requisite system."*

**How it wires up** (in `SandboxCharacter_CMC.EventGraph`):
1. Owner calls `Add Tick Prerequisite Component` with `AC_PreCMCTick` as the prerequisite of the `CharacterMovementComponent` — this forces the AC to tick BEFORE the CMC every frame.
2. Owner binds its `PreCMCTick` custom event to the `Tick` delegate.
3. `PreCMCTick` then calls `UpdateMovement_PreCMC` (Gait → MaxWalkSpeed) and `UpdateRotation_PreCMC` (strafe-vs-forward → RotationRate, sets `RotationRate=-1` for instant rotation while grounded so the actor acts as the "target rotation" while the AnimBP independently spins root_bone — enables stick flicks, pivots, TIP).

**Why it exists**: CMC runs `Velocity = MaxWalkSpeed * InputDirection` inside its own tick. If you wait until `Event Tick` (post-physics) to set `MaxWalkSpeed` for the new gait, you're one frame late. The pre-tick hook fixes this 1-frame lag.

**Should AZ port?** **NO.** This is a CMC-only workaround. Mover has a native pre-/post-sim input pattern (see `gasp_pawn_cpp_port_plan.md`) — AZ already implements `OnPreSimulationInput()` / `OnPostSimulationInput()` on `AAZ_HeroPawn`. The Mover variant of GASP itself does NOT use this component (verified: `SandboxCharacter_Mover` has no `Get AC_PreCMCTick` references).

---

### 1.2 `AC_TraversalLogic` — Vault / hurdle / mantle / climb (Mover + CMC dual support)

| Path | `/Game/Blueprints/AC_TraversalLogic.AC_TraversalLogic` |
|---|---|
| Parent | `UActorComponent` |

**Variables** (6):
- `CharacterProperties` : `S_CharacterPropertiesForTraversal` — cached owner data (Mesh, Capsule, MotionWarping, MovementMode, Speed)
- `TraversalResult` : `S_TraversalCheckResult` (replicated across server↔clients) — chosen montage, ledge transforms, action type
- `DoingTraversalAction` : `bool` — gate against re-entry while a traversal is in flight
- `Mesh` : `SkeletalMeshComponent*`
- `IsMoverCharacter` : `bool` — branch flag (Mover path vs CMC path)
- `Mover` : `CharacterMoverComponent*`

**Functions / Events**:

| Name | Purpose |
|---|---|
| `BeginPlay` | Calls `BPI_SandboxCharacter_Pawn::Get_PropertiesForTraversal` on owner → caches `CharacterProperties`. Detects if owner has a `CharacterMoverComponent` → sets `IsMoverCharacter` + `Mover` ref. |
| `TryTraversalAction(Inputs, DebugType) → (TraversalCheckFailed, MontageSelectionFailed)` | **241 nodes**. The full traversal pipeline. Documented stages from in-graph comments: **Step 1**: Cache ActorLocation, CapsuleHalfHeight, CapsuleRadius. **Step 2.1**: `Capsule Trace By Channel` forward → looking for an actor castable to `LevelBlock_Traversable`. **Step 2.2**: Call `LevelBlock_Traversable::Get Ledge Transforms` to fetch front + back ledge data. **Step 3.1**: Validate front ledge exists. **Step 3.2**: Trace from actor up to front ledge to confirm there's room to mantle up. **Step 3.4**: Trace across the top from front→back ledge for room. **Step 3.5**: Cache obstacle depth (front-to-back distance, or front-to-impact if blocked, invalidate back ledge). **Step 4.2**: Build `S_TraversalChooserInputs` (action type, ledge flags, height, depth, gait, speed, MovementMode, **PoseHistory** from the AnimBP), then evaluate either `Chooser_Asset: CHT_TraversalMontages_Mover` **or** `CHT_TraversalMontages_CMC` (chosen via `IsMoverCharacter` branch). The chooser has a **PoseMatch column** that picks the best montage and entry frame based on current pose + distance-to-ledge. **Step 5.2**: If everything succeeded, fires `PerformTraversalAction` event (which then plays the montage; replicated server→clients). |
| `SetWarpTargets` | **78 nodes**. Updates the MotionWarping component's targets `FrontLedge`, `BackLedge`, `BackFloor` per montage. Uses `MotionWarpingUtilities::Get Motion Warping Windows` + `AnimationWarpingLibrary::Get Curve Value from Animation` on the `Distance_From_Ledge` curve to compute exact warp endpoints from the chosen anim's curve baked by `AM_DistanceFromLedge`. Removes targets that don't apply to action type (e.g. removes `BackLedge` for non-vault, removes `BackFloor` for non-hurdle). |
| `SetMovementMode(EMovementMode)` | If `IsMoverCharacter`: `Mover->QueueNextMovementMode(name)` mapping `MOVE_Falling → "Falling"`, `MOVE_Flying → "Flying"`, default → `"Walking"`. If CMC: forwards to `CharacterMovementComponent->SetMovementMode`. Used to put the character into `Flying` (or `MOVE_None` in CMC) during the traversal so CMC/Mover collision/gravity doesn't fight the warped root motion. |
| `SetReplicationBehavior(ClientAuthoritative)` | CMC-only path (Mover branch is empty): toggles `bIgnoreClientMovementErrorChecksAndCorrection` and `bServerAcceptClientAuthoritativePosition` so clients can authoritatively warp without server snap-back. |
| `PerformTraversalAction` (Custom Event) | Plays montage with `Play Montage (Mover Actor)` (Mover path) **or** `Play Montage` (CMC path). On `OnCompleted`/`OnBlendOut`/`OnInterrupted` resets `DoingTraversalAction=false` and via 0.2s retriggerable delay switches movement mode back to `Walking` and re-enables movement error checks. Calls `IgnoreComponentWhenMoving(true)` on the hit primitive at start, `(false)` after delay. Calls `SetWarpTargets` once before the montage. |
| `PerformTraversalAction_Server` (RELIABLE replicated, runs on server) | Server forwards to `PerformTraversalAction_Clients`. |
| `PerformTraversalAction_Clients` (replicated to all) | Sets `TraversalResult`, then on local clients calls owner-cast `Pawn` → calls back into `PerformTraversalAction`. |

**Caller**: `SandboxCharacter_Mover.EventGraph` Jump/Traversal Input → calls `GetTraversalCheckInputs` → `AC_TraversalLogic::TryTraversalAction`. Has a debug-only first-call (`ForOneFrame`) and a real call (`ForDuration`) gated by `DoingTraversalAction`.

**Should AZ port?** **YES — high value, but not immediately blocking.** AZ has zero traversal/parkour. Without it: no vault, no hurdle, no mantle, no climb-up. This is independent of the locomotion gaps however, so it can wait until basic locomotion is solid. **Dependencies before porting**: `S_TraversalCheckInputs`, `S_TraversalCheckResult`, `S_TraversalChooserInputs`, `S_TraversalChooserOutputs`, `S_CharacterPropertiesForTraversal` structs; `LevelBlock_Traversable` actor + `Get Ledge Transforms` function; `CHT_TraversalMontages_Mover` chooser; `MotionWarpingComponent` on the pawn; the actual traversal montages with `Distance_From_Ledge` curves baked by `AM_DistanceFromLedge`.

---

### 1.3 `AC_VisualOverrideManager` — Mesh-swap CVar driver

| Path | `/Game/Blueprints/AC_VisualOverrideManager.AC_VisualOverrideManager` |
|---|---|
| Parent | `UActorComponent` |
| Vars | `VisualOverride` : `Actor*` (presumably a class ref under the hood; the Set node treats it as a class) |

**Functions**:
- `BeginPlay` event: calls `FindAndApplyVisualOverride`, then subscribes to `DataDrivenCVarEngineSubsystem::OnDataDrivenCVarDelegate`. When a CVar named `DDCvar.VisualOverride` changes, re-runs `FindAndApplyVisualOverride`.
- `FindAndApplyVisualOverride` — gets `GM_Sandbox.VisualOverrides` (a class list curated by the GameMode), then calls `BFL_HelpfulFunctions::Get Visual Override with CVAR` to pick the class matching the current CVar value, then sets `VisualOverride` (RepNotify-style: triggers `OnRep_VisualOverride`).
- `OnRep_VisualOverride` (replicated) → calls `ApplyVisualOverride`.
- `ApplyVisualOverride` — gets all `ChildActorComponent` on owner with tag `VisualOverride`, for each: `SetChildActorClass(VisualOverride)`, then walks up to the parent SkeletalMeshComponent and `SetVisibility(false)` if `VisualOverride` is a valid class (otherwise leaves the original mesh visible).

**Architecture**: GASP characters carry one or more `ChildActorComponent`s tagged `VisualOverride` parented to their SkeletalMeshComponent. Setting a CVar (`DDCvar.VisualOverride <name>`) swaps the displayed mesh actor at runtime — used by the GASP demo's Quinn / Manny / Echo / Mannequin selector dropdown.

**Should AZ port?** **NO.** Pure demo-UX feature for switching characters at runtime. Has no effect on locomotion, animation correctness, or gameplay. AZ uses a single Hero mesh.

---

### 1.4 `AC_SmartObjectAnimation` — NPC StateTree smart-object integration

| Path | `/Game/Blueprints/SmartObjects/AC_SmartObjectAnimation.AC_SmartObjectAnimation` |
|---|---|
| Parent | `UActorComponent` |

**Variables** (10): `Owner Skeletal Mesh`, `Incoming Animation Payload` (`SmartObjectAnimationPayload`), `Number Of Loops`, `Warp Target Name`, `Owner Montage Finished` (mcdelegate), `Ignore Character Movement Server Corrections`, `Character Movement Component`, `Smart Object Selection Inputs/Outputs` (`SmartObjectSelectionInputs/Outputs`), `Mover Component`.

**Functions** (8): `Cache Necessary Data`, `Try Add Warp Target`, `Set Ignore Collision State(bShouldIgnore, OtherActor)`, `Set Ignore Character Movement Correction(bool)`, `NPC Approach Angle and Distance(Destination) → (Distance, Angle)`, `Evaluate Distance and Motion Match(Destination, ProxyTable) → (Result Montage, Cost, StartTime)`, `Is Mover() → bool`, `Setup Play Timer`, plus EventGraph (6 events, 46 nodes).

**Used by**: `ST_NPC_SandboxCharacter_SmartObject` (StateTree), `ST_NPC_SandboxCharacter_Patrol_Subtree`. AI pawns interacting with smart objects (sit-down, lean-on, etc).

**Should AZ port?** **NO** for now — AZ has no AI/StateTree NPCs using SmartObjects.

---

## 2. Blueprint Interfaces

### 2.1 `BPI_SandboxCharacter_Pawn` — Pawn → world contract (4 functions)

| Function | Args | Returns | Purpose |
|---|---|---|---|
| `Get_PropertiesForAnimation` | — | `S_CharacterPropertiesForAnimation` | Snapshot consumed by AnimBP each frame: input acceleration, movement mode, gait, stance, etc. |
| `Get_PropertiesForCamera` | — | `S_CharacterPropertiesForCamera` | Snapshot consumed by camera director: stance, aim state, etc. |
| `Get_PropertiesForTraversal` | — | `S_CharacterPropertiesForTraversal` | Snapshot consumed by `AC_TraversalLogic`: Mesh, Capsule, MotionWarping ref, MovementMode, Speed. |
| `Set_CharacterInputState(Desired Input State : S_PlayerInputState)` | — | — | Inbound: external systems push input state into the pawn. |

**AZ status**: AZ does NOT implement this interface on AAZ_HeroPawn. The **C++ ABP port** for AZ instead reads pawn state directly via cached pawn pointer and `SetThreadSafeProperty` + accessor helpers. **Recommendation**: Implement at least `Get_PropertiesForTraversal` if/when porting `AC_TraversalLogic`. Otherwise OK to skip — AZ's C++ AnimInstance doesn't need it because it has direct access.

---

### 2.2 `BPI_SandboxCharacter_ABP` — AnimBP → world contract (6 functions)

| Function | Args | Returns | Purpose |
|---|---|---|---|
| `Get_PoseHistory` | — | `PoseHistoryReference` | AnimBP exposes its PoseHistory node ref so traversal/anim-notify code can do pose matching against current pose. |
| `Get_InteractionTransform` | — | `Transform` | Read-only access to the in-progress interaction transform (door, switch, etc). |
| `Set_InteractionTransform(Transform)` | — | — | Inbound: world systems set the transform the AnimBP should use for IK/aligned interactions. |
| `Set_NotifyTransition_ReTransition(bool)` | — | — | Anim notify state pings AnimBP to force a re-transition out of current SM state. Used by `BP_NotifyState_EarlyTransition`. |
| `Set_NotifyTransition_ToLoop(bool)` | — | — | Anim notify state pings AnimBP to force transition into the loop state. Used by `BP_NotifyState_EarlyTransition`. |
| `Get_Gait` | — | `E_Gait` | AnimBP exposes current gait so anim notifies can branch on it (e.g. `EarlyTransition.GaitNotEqual`). |

**AZ status**: AZ's C++ `UAZ_AnimInstance` has the equivalent fields but does NOT implement this interface. **Recommendation**: Implement on `UAZ_AnimInstance` to enable EarlyTransition / MontageBlendOut notify states without rewriting them. Cost is low — five of the six functions are just exposing existing fields.

---

### 2.3 `BPI_InteractionTransform` — Legacy interface, "old" suffix on functions

3 functions: `SetInteractionTransform_Old(Transform)`, `GetInteractionTransform_Old() → Transform`, `Get_PoseHistory_Old() → PoseHistoryReference`.

**Should AZ port?** **NO.** Functions are explicitly named `_Old` — appears to be a deprecated predecessor of `BPI_SandboxCharacter_ABP`. Skip.

---

## 3. Anim Notifies (10 + 2 NotifyStates + 5 enums)

### 3.1 `BP_AnimNotify_FoleyEvent` (base class)

| | |
|---|---|
| Parent | `UAnimNotify` |
| Vars | `Event` (`FGameplayTag`), `Side` (`E_FoleyEventSide`), `VolumeMultiplier` (double), `PitchMultiplier` (double), `DefaultBank` (`DABP_FoleyAudioBank_C*`), `VisLogDebugColor`, `VisLogDebugText` |

**Behavior** (`Received_Notify`): Skips if blending out. Looks up `AC_FoleyEvents` component on owner. If found → calls `AC_FoleyEvents::Play Foley Event(Event, S_FoleyEventParams{Side, Volume, Pitch})`. If NOT found → falls back to `DefaultBank::Get Sound from Foley Event(Event)` → `Play Sound 2D` (UI sound, volume/pitch multiplied).

### 3.2 Sub-notify variants (preset Side + Event tag for convenience)

All 9 inherit from `BP_AnimNotify_FoleyEvent` with no new vars — they just preset defaults: `BP_AnimNotify_FoleyEvent_Walk_L`, `_Walk_R`, `_Run_L`, `_Run_R`, `_Scuff_L`, `_Scuff_R`, `_Handplant_L`, `_Handplant_R`, `_Land`, `_Jump`.

### 3.3 `BP_NotifyState_EarlyTransition`

| | |
|---|---|
| Parent | `UAnimNotifyState` |
| Vars | `TransitionDestination` (`E_EarlyTransition_Destination` — Loop / ReTransition), `TransitionCondition` (`E_EarlyTransition_Condition` — Always / GaitChanged), `GaitNotEqual` (`E_Gait`) |

**Behavior** (`Received_NotifyTick`): Casts owner to `BPI_SandboxCharacter_ABP`. Skips if blending out. Switch on `TransitionCondition`: if `GaitChanged` → calls `BPI::Get_Gait` and sets a local `Transition` bool when `Gait != GaitNotEqual`; if `Always` → `Transition=true`. If transitioning → switch on `TransitionDestination`: ReTransition → `BPI::Set_NotifyTransition_ReTransition(true)`; Loop → `BPI::Set_NotifyTransition_ToLoop(true)`.

**Used on**: locomotion start anims (e.g. `RunStart`, `JogStart`) so they can early-transition out into the loop / re-evaluate the SM if the gait changes mid-anim.

### 3.4 `BP_NotifyState_MontageBlendOut`

| | |
|---|---|
| Parent | `UAnimNotifyState` |
| Vars | `BlendOutCondition` (`E_TraversalBlendOutCondition` — 3 values), `BlendOutTime` (double), `BlendProfile` (`FName`) |

**Behavior** (`Received_NotifyTick`): Caches AnimMontage + `S_CharacterPropertiesForAnimation` from owner via `BPI_SandboxCharacter_Pawn`. Switch on `BlendOutCondition`:
1. `NewEnumerator0` (always) → blend out always
2. `NewEnumerator1` (Acceleration != 0) → blend out if `InputAcceleration != FVector::Zero()`
3. `NewEnumerator2` (MovementMode == Falling) → blend out if `MovementMode == Falling` (enum index 5)

When triggered: `AnimInstance::Montage Stop with Blend Settings(MontageBlendSettings{BlendProfile, AlphaBlendArgs{BlendTime, HermiteCubic}}, AnimMontage)`.

**Used on**: traversal montages — to early-blend-out vault/mantle when the player gives input mid-action or the character starts falling off.

### 3.5 Enums under AnimNotifies/

- `E_EarlyTransition_Condition` — Always / GaitChanged
- `E_EarlyTransition_Destination` — Loop / ReTransition
- `E_FoleyEventSide` — L / R / Center / Both / Other
- `E_TraversalBlendOutCondition` — Always / OnInput / OnFall

**Should AZ port?**
- **FoleyEvent system**: NO — AZ has no `AC_FoleyEvents` and no `DABP_FoleyAudioBank`. Audio is out of scope for locomotion fixes. Skip until audio pass.
- **EarlyTransition + MontageBlendOut**: **YES, eventually** — they are required for high-quality start/loop transitions and clean traversal blendout. Both depend on `BPI_SandboxCharacter_ABP` (gait, transition flags) and `BPI_SandboxCharacter_Pawn` (props for animation). **Needed only when porting traversal**, not for current locomotion gaps.

---

## 4. Anim Modifiers (17)

All inherit from `UAnimationModifier` (or specialized library subclasses):

| Name | Parent | What it bakes |
|---|---|---|
| `AM_BakePhaseCurveFromFootstepNotifies` | `UAnimationModifier` | Walks `Foley_Walk_L/R` notifies on the anim → bakes a `Phase` float curve (oscillating 0/1 between feet). Critical for locomotion DBs / motion matching gait alignment. |
| `AM_FootSteps_Walk` | `FootstepAnimEventsModifier` (engine) | Engine's foot-step generator preset for walk speed thresholds. |
| `AM_FootSteps_Run` | `FootstepAnimEventsModifier` | Run preset. |
| `AM_FootSteps_Modulation` | `UAnimationModifier` | Adds `FootSteps_Modulation` curve modulating volume per stride. |
| `AM_FootSpeed_L`, `AM_FootSpeed_R` | `UAnimationModifier` | Bakes per-foot speed curves (probably world-space velocity). |
| `AM_Copy_IKFootRoot` | `UAnimationModifier` | Copies IK foot root tracks. |
| `AM_DistanceFromLedge` | `MotionExtractorModifier` (engine) | Bakes the `Distance_From_Ledge` curve consumed by `AC_TraversalLogic::SetWarpTargets` for warp computations. |
| `AM_MoveData_Speed` | `UAnimationModifier` | Bakes `movedata_speed` curve. |
| `AM_WarpingAlpha` | `UAnimationModifier` | Bakes `enable_warping` 0/1 curve gating runtime warps. |
| `AM_OrientationWarpingAlpha` | `UAnimationModifier` | Same for orientation warping. |
| `AM_RateWarpingAlpha` | `UAnimationModifier` | Same for play-rate warping. |
| `AM_TriggerWeightThreshold` | `UAnimationModifier` | Sets notify trigger weight thresholds. |
| `AM_Reset_Attach` | `UAnimationModifier` | Resets attach tracks. |
| `AM_ReorderCurves`, `AM_RenameCurve`, `AM_RemoveCurves` | `UAnimationModifier` | Curve management utilities. |

**Should AZ port?**
- **AM_BakePhaseCurveFromFootstepNotifies + AM_FootSteps_Walk + AM_FootSteps_Run**: **YES** — these are how GASP gets the `Phase` curve onto every locomotion anim. AZ may be missing `Phase` curves on retargeted RTG anims — this gap directly impacts MotionMatching pose-matching quality and could explain locomotion stuttering / wrong-foot starts.
- **AM_MoveData_Speed**: **YES** — adds `movedata_speed` curve required by GASP's IsMoving + trajectory comparator. AZ already noted as missing in `gasp_update_logic_flow.md`.
- **AM_WarpingAlpha / AM_OrientationWarpingAlpha / AM_RateWarpingAlpha**: **YES if using runtime warping** in AZ AnimGraph. Otherwise skip.
- **AM_DistanceFromLedge**: only needed for traversal anims (deferred with traversal).
- Others: utility-only, port as needed.

---

## 5. AZ Locomotion-Gap Implications (highest priority)

Of everything above, the items most likely to explain **current AZ locomotion gaps** are:

1. **Missing `Phase` curve on locomotion anims** — needs `AM_BakePhaseCurveFromFootstepNotifies` (depends on the `Foley_Walk_L`/`_R` notifies being placed on the anims first; foot-strike notifies are baked by `AM_FootSteps_Walk`/`_Run`). Without a Phase curve, MotionMatching cannot align gait phase between candidate poses → wrong-foot starts, foot skating, jitter.

2. **Missing `movedata_speed` curve** — needs `AM_MoveData_Speed` applied to every locomotion anim. Already flagged in `gasp_update_logic_flow.md`. Affects DB cost calculations.

3. **`BPI_SandboxCharacter_ABP::Get_Gait` not implemented on AZ AnimInstance** — blocks `EarlyTransition` notify states from working in any retargeted GASP anim that has them. Less critical — affects only start/stop transition quality, not core walk/run cycle.

4. **No `MotionWarpingComponent` on AAZ_HeroPawn** — required before any traversal port. Not blocking current locomotion.

The actor components themselves (`AC_PreCMCTick`, `AC_TraversalLogic`, `AC_VisualOverrideManager`) do **not** explain locomotion gaps:
- `AC_PreCMCTick` is replaced by Mover's pre-/post-sim pattern (already implemented in AZ).
- `AC_TraversalLogic` is for vault/mantle, not walk-cycle.
- `AC_VisualOverrideManager` is cosmetic mesh-swap.

---

## 6. Port Priority Summary

| Item | Priority | Reason |
|---|---|---|
| `AM_BakePhaseCurveFromFootstepNotifies` (+ Foley notifies on anims as data prep) | **HIGH** | Likely root cause of MM pose-mismatch issues |
| `AM_MoveData_Speed` applied to all locomotion anims | **HIGH** | Trajectory cost evaluation needs this curve |
| `BPI_SandboxCharacter_ABP` impl on UAZ_AnimInstance (at minimum `Get_Gait`, `Set_NotifyTransition_*`) | **MEDIUM** | Unblocks EarlyTransition notify states |
| `BP_NotifyState_EarlyTransition` (port BP as-is, retarget to AZ interface) | **MEDIUM** | Cleaner start→loop transitions |
| `AC_TraversalLogic` + supporting structs/chooser/`MotionWarpingComponent` | **MEDIUM** | Adds vault/hurdle/mantle/climb |
| `BP_NotifyState_MontageBlendOut` | LOW (depends on traversal) | Only useful with traversal montages |
| `AC_PreCMCTick` | **SKIP** | Mover handles this natively in AZ |
| `AC_VisualOverrideManager` | **SKIP** | Cosmetic/demo-only |
| FoleyEvent system | LOW | Audio polish, not blocking |
| `AC_SmartObjectAnimation` | **SKIP** | No NPC/StateTree usage in AZ |
| `BPI_InteractionTransform` | **SKIP** | Deprecated `_Old` suffix |
