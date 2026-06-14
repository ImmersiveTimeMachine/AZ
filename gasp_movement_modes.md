---
name: GASP Movement Modes
description: Full deep audit of GASP's BP_MovementMode_Walking/Falling/Slide and slide transitions — parent classes, every variable + default, every function signature, rotation logic in GenerateWalkMove. Use when porting Mover modes to AZ for GASP-parity capsule rotation.
type: reference
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# GASP Movement Modes — Deep Audit (2026-05-03)

All 5 BPs live at `/Game/Blueprints/MovementModes/` (imported into AZ, originally from Epic's GameAnimationSample).
Source verified live via `unreal_blueprint_query` and Python CDO introspection on running editor (port 3000).

Engine source paths referenced:
- `C:\UnrealEngine\Engine\Plugins\Experimental\Mover\Source\Mover\Public\DefaultMovementSet\Modes\SmoothWalkingMode.h`
- `C:\UnrealEngine\Engine\Plugins\Experimental\Mover\Source\Mover\Private\DefaultMovementSet\Modes\SmoothWalkingMode.cpp`
- `C:\UnrealEngine\Engine\Plugins\Experimental\Mover\Source\Mover\Public\DefaultMovementSet\Modes\SimpleWalkingMode.h`
- `C:\UnrealEngine\Engine\Plugins\Experimental\Mover\Source\Mover\Public\DefaultMovementSet\Modes\WalkingMode.h`
- `C:\UnrealEngine\Engine\Plugins\Experimental\Mover\Source\Mover\Public\DefaultMovementSet\Modes\FallingMode.h`

---

## 1. BP_MovementMode_Walking

**Parent:** `/Script/Mover.SmoothWalkingMode`
Inheritance chain: `USmoothWalkingMode` → `USimpleWalkingMode` → `UWalkingMode` → `UBaseMovementMode`.

### What the parent gives you (USmoothWalkingMode C++)
- `SimulationTick_Implementation` — copies `FSmoothWalkingState` into the output sync state (the BP cannot/does-not override this; everything else flows through `GenerateWalkMove_Implementation`).
- `GenerateWalkMove_Implementation` — the spring-damper math. Critical lines from the .cpp:
  - Adds an `FSmoothWalkingState` to the SyncStateCollection (carries `SpringVelocity, SpringAcceleration, IntermediateVelocity, IntermediateFacing, IntermediateAngularVelocity` between ticks).
  - Computes `VelocityMatch` (dot of internal SpringVelocity vs actual InOutVelocity, 0-1) and uses it to scale the `OutsideInfluenceSmoothingTime` so collisions don't poison the internal velocity.
  - When `TurningStrength > 0` and `DesiredVelocity` is non-zero, rotates IntermediateVelocity towards DesiredVelocity using `FMath::ExponentialSmoothingApprox(...)` with `SpringMath::StrengthToSmoothingTime(TurningStrength)`. **This is the lateral velocity-vector turn**, NOT capsule rotation.
  - Splits acceleration into `Lateral` ((1-DirectionalAccelerationFactor)*Acceleration) and `Directional` (DirectionalAccelerationFactor*Acceleration when accelerating only). Default DirectionalAccelerationFactor=1.0, so it behaves like classic CharacterMovement.
  - Uses `SpringMath::CriticalSpringDamper(SpringVelocity, SpringAcceleration, TrackVelocity, AccelerationSmoothingTime|DecelerationSmoothingTime, DeltaSeconds)`.
  - **Capsule facing**: when `bSmoothFacingWithDoubleSpring=true`, two cascaded `SpringMath::CriticalSpringDamperQuat` calls (each with `FacingSmoothingTime/2`). When false, single spring with full `FacingSmoothingTime`. **GASP Walking has `bSmoothFacingWithDoubleSpring = False` and `FacingSmoothingTime = 0.5s`** (single spring).
  - Snaps to deadzones (`FacingDeadzoneThreshold=0.1°`, `AngularVelocityDeadzoneThreshold=0.01°/s`).

### CDO defaults (live readout 2026-05-03)
| Variable | Type | Default | Category | Notes |
|---|---|---|---|---|
| `WalkSpeed` | float | **165.0** cm/s | Blueprint Overrides | Gait=Walk |
| `WalkAcceleration` | float | **500.0** | Blueprint Overrides | wired into `Acceleration` per-tick |
| `RunSpeed` | float | **375.0** | Blueprint Overrides | Gait=Run (default) |
| `RunAcceleration` | float | **800.0** | Blueprint Overrides | |
| `SprintSpeed` | float | **585.0** | Blueprint Overrides | |
| `SprintAcceleration` | float | **300.0** | Blueprint Overrides | added on TOP of Run only when speed > RunSpeed |
| `CrouchSpeed` | float | **200.0** | Blueprint Overrides | overrides any Gait when `IsCrouching` |
| `Walk/RunTurnStrength` | double | **8.0** | Blueprint Overrides | drives parent `TurningStrength` (lateral velocity steer) |
| `SprintTurnStrength` | double | **4.0** | Blueprint Overrides | smaller = wider sprint turn radius |
| `GaitChangeDeceleration` | float | **300.0** | Blueprint Overrides | when MoveInput≠0 and current speed > target gait speed |
| `StoppingDeceleration` | float | **1000.0** | Blueprint Overrides | when MoveInput == zero (and `JustLanded=false`) |
| `Walk/RunFacingTime` | double | **0.4** s | Blueprint Overrides | `FacingSmoothingTime` while moving at walk/run gait |
| `SprintFacingTime` | double | **0.8** s | Blueprint Overrides | larger smoothing time = wider sprint turn |
| `IdleFacingTime` | double | **0.2** s | Blueprint Overrides | `FacingSmoothingTime` while MoveInput == zero |
| `JustLanded` | bool | False | Default | latched true 0.2s after Falling→Walking via Retriggerable Delay |

Inherited (SmoothWalkingMode) actual CDO defaults (some differ from the C++ header defaults):
- `Acceleration = 1000.0` (header default 1500.0 — overridden by BP at construction: it's effectively a per-tick variable, set every frame in BP)
- `Deceleration = 1500.0` (header default 1500.0)
- `DirectionalAccelerationFactor = 1.0`
- `TurningStrength = 8.0` (BP overrides every frame from Walk/Run/SprintTurnStrength)
- `AccelerationSmoothingTime = 0.1`
- `DecelerationSmoothingTime = 0.1`
- `AccelerationSmoothingCompensation = 0.0`
- `DecelerationSmoothingCompensation = 0.0`
- `VelocityDeadzoneThreshold = 0.01`
- `AccelerationDeadzoneThreshold = 0.001`
- `OutsideInfluenceSmoothingTime = 0.05`
- `FacingSmoothingTime = 0.5` (header default 0.25 — **GASP override 0.5**, BP overrides per-tick from Walk/Run/Sprint/IdleFacingTime)
- `bSmoothFacingWithDoubleSpring = **False**` (header default true — **GASP turns this OFF**)
- `FacingDeadzoneThreshold = 0.1` deg
- `AngularVelocityDeadzoneThreshold = 0.01` deg/s
- `MaxSpeedOverride = -1.0` (BP sets this every tick to the active gait's max speed)
- `FloorCheckPolicy = OnDynamicBaseOnly`
- `TurnGenerator = None`

### Local variables (function-scoped inside `GenerateWalkMove`)
- `CurrentOffset` — float, the previous-frame angular delta (in degrees) between `CurrentFacing` and `DesiredFacing`. Used to clamp the new `RotationOffset` so the pawn always rotates *toward* DesiredFacing.
- `OverridenDesiredFacing` — Quat, the DesiredFacing rotated by the (clamped) `RotationOffset`. **This is what is actually fed to the parent spring-damper.**

### Functions
**`GenerateWalkMove`** — *override* of `USimpleWalkingMode::GenerateWalkMove` (BlueprintNativeEvent).

Signature (unchanged from parent):
```
void GenerateWalkMove(
    FMoverTickStartData& StartState (ref),
    float DeltaSeconds,
    FVector DesiredVelocity,
    FQuat DesiredFacing,
    FQuat CurrentFacing,
    FVector& InOutAngularVelocityDegrees (ref),
    FVector& InOutVelocity (ref))
```

Graph layout: 1 entry → Sequence(2) → [InputCache branch | InnerSequence(7) → Parent call]
Total 125 K2 nodes including 9 EdGraphNode_Comment annotations.

**Phase 1 — Input cache (Sequence then_0)**
1. Reads `StartState.InputCmd.InputCollection`.
2. `Get Data From Collection` → `CharacterDefaultInputs` → store in BP local `MoverDefaultInputs`.
3. `Get Data From Collection` → `S_MoverCustomInputs` → store in BP local `MoverCustomInputs`.
   - Comment: *"Cache the current input structs from the data collection for easy retrieval throughout this function."*

**Phase 2 — DesiredFacing override (Sequence then_1, sub-Sequence then_0)**
Comment: *"Apply the Rotation Offset input to the desired facing rotation, and save as the OverridenDesiredFacing. The offset is clamped to not be greater than the current offset + 179, or less than the current offset - 179. This means that the pawn will always rotate TOWARD the Desired Facing direction, even if the shortest path would've been away from it."*

Logic:
- `RotationOffsetIn = MoverCustomInputs.RotationOffset` (degrees, set by HeroPawn from camera/aim input)
- `Clamped = FMath::Clamp(RotationOffsetIn, CurrentOffset - 179.0, CurrentOffset + 179.0)`
- `OverridenDesiredFacing = (DesiredFacing.ToRotator() combined with Yaw=Clamped).ToQuaternion()`
- Then writes the *new* offset back: `CurrentOffset = AbsDelta(CurrentFacing, OverridenDesiredFacing).Yaw` (Comment: *"Cache the current offset between the current pawn orientation and desired facing orientation."*)

**Phase 3 — Per-tick parent property writes (sub-Sequence then_1..then_5)** — set parent's exposed knobs based on inputs:

| then_N | Sets | Source value |
|---|---|---|
| then_1 | `MaxSpeedOverride` (SimpleWalkingMode) | `IsCrouching ? CrouchSpeed : Select(MoverCustomInputs.Gait, [WalkSpeed, RunSpeed, SprintSpeed])` |
| then_2 | `Acceleration` (SmoothWalkingMode) | `Select(MoverCustomInputs.Gait, [WalkAcceleration, RunAcceleration, RunAcceleration]) + (IsCurrentSpeed > RunSpeed ? SprintAcceleration : 0)` — sprint accel layered on top |
| then_3 | `Deceleration` (SmoothWalkingMode) | `MoveInput == 0 ? StoppingDeceleration : GaitChangeDeceleration` ; AND `JustLanded ? 20000.0 : computedValue` (sticky landings) |
| then_4 | `TurningStrength` (SmoothWalkingMode) | `MapRangeClamped(VectorLengthXY(InOutVelocity), RunSpeed→SprintSpeed, Walk/RunTurnStrength→SprintTurnStrength)` |
| then_5 | `FacingSmoothingTime` (SmoothWalkingMode) | Multi-stage: <br>• If `MoveInput.IsZero()` → `IdleFacingTime` <br>• Else → `MapRangeClamped(VectorLengthXY(InOutVelocity), RunSpeed→SprintSpeed, Walk/RunFacingTime→SprintFacingTime)` <br>• ADDITIONAL: comment near coords (-352, 512) — *"This bit of logic prevents the character from under-rotating when the camera is turning quickly and the Current Offset is beyond 135 degrees from the Desired Facing orientation. It essentially clamps the pawns angular velocity to at least be the control rotations rotation rate, which will be high if the camera is turning quickly."* — uses `MapRangeClamped(Abs(Delta(DesiredFacing, CurrentFacing).Yaw), 90→135, 0→0.2)` to add **at most** an extra 0.2s subtraction/clamp on FacingSmoothingTime when the offset exceeds 90°. |

**Phase 4 — Parent call (sub-Sequence then_6)**
- `Parent: Generate Simple Walk Move` invoked with `DesiredFacing = OverridenDesiredFacing` (NOT the input `DesiredFacing`).
- All other args passed through.

### EventGraph
Single event `OnActivated` (8 nodes):
- `Get Mover Component → Get Movement Mode Name == "Falling"` ⇒ Branch:
  - True: `Set JustLanded = true` → `Retriggerable Delay (0.2s)` → `Set JustLanded = false`.
  - False: no-op (other entries don't trigger sticky landing).
- This is the ONLY event handler. The `JustLanded` flag is read in `GenerateWalkMove` Phase-3 deceleration calc to apply 20000 cm/s² braking for the first 0.2s after landing.

---

## 2. BP_MovementMode_Falling

**Parent:** `/Script/Mover.FallingMode` (`UFallingMode` → `UBaseMovementMode`).
Note: an earlier `get_super_class` introspection reported `BaseMovementMode` because Python's `__bases__` walked too far; the BP's authoritative `parent_class` field is `FallingMode`.

### What the parent gives you (UFallingMode C++)
- `GenerateMove_Implementation` — applies gravity, air control, vertical/lateral terminal speed clamping, friction.
- `SimulationTick_Implementation` — performs the falling integration AND the floor-check / `ProcessLanded` switch to walking when ground hit.
- Editable knobs (CDO defaults read live):

| Property | Type | GASP Default |
|---|---|---|
| `AirControlPercentage` | float | **0.4** (40% air control) |
| `FallingDeceleration` | float | **200.0** cm/s² (lateral, when below terminal) |
| `FallingLateralFriction` | float | **0.0** |
| `OverTerminalSpeedFallingDeceleration` | float | **800.0** cm/s² |
| `TerminalMovementPlaneSpeed` | float | **1500.0** cm/s |
| `bShouldClampTerminalVerticalSpeed` | bool | **True** |
| `VerticalFallingDeceleration` | float | **4000.0** cm/s² (only when bShouldClampTerminalVerticalSpeed=false) |
| `TerminalVerticalSpeed` | float | **2000.0** cm/s |
| `bCancelVerticalSpeedOnLanding` | bool | **True** (zeros vertical on landing, not redirected) |
| `OnLanded` | delegate | (FName NextMovementModeName, FHitResult Hit) |

### BP-added variables
**None.** The Falling BP adds zero variables.

### Functions
**`GenerateMove`** — full override of `UFallingMode::GenerateMove_Implementation` (33 K2 nodes).

Signature (unchanged):
```
void GenerateMove(
    FMoverTickStartData StartState,
    FMoverTimeStep TimeStep,
    FProposedMove& OutProposedMove)
```

Graph layout: Entry → Sequence(3)
1. **then_0** — calls `Parent: Generate Move` (passes everything through). Parent computes the actual gravity/lateral physics and writes `OutProposedMove`.
2. **then_1** — input cache identical to Walking: pulls `CharacterDefaultInputs` and `S_MoverCustomInputs` out of `StartState.InputCmd.InputCollection` and stores in BP locals (`MoverDefaultInputs`, `MoverCustomInputs`, `InputCollection`).
3. **then_2** — **Sets `OutProposedMove.AngularVelocityDegrees`** by:
   - Reads `SyncState.Orientation` (current rotation) from the `MoverDefaultSyncState` in `StartState.SyncState.SyncStateCollection`.
   - Reads `MoverDefaultInputs.OrientationIntent` (the input vector) and converts via `Rotation From X Vector` → yaw.
   - Reads `MoverCustomInputs.RotationOffset` (degrees).
   - Computes `TargetYaw = OrientationIntent.Yaw + RotationOffset`.
   - Calls `MovementUtils::ComputeAngularVelocityDegrees(From=CurrentRotator, To=MakeRotator(0, 0, TargetYaw), DeltaSeconds=GetWorldDeltaSeconds, TurningRateLimit=300.0)` → writes to `OutProposedMove.AngularVelocityDegrees`.

Net effect: while in air, the parent supplies linear physics; the BP layers an explicit angular velocity computation so the capsule yaw still steers toward the camera/orientation intent at up to 300°/s, even though the spring-damper machinery of SmoothWalkingMode is not active here.

### EventGraph
**Empty.** No events bound on this BP.

---

## 3. BP_MovementMode_Slide

**Parent:** `/Script/Mover.SmoothWalkingMode` (same as Walking — the slide reuses the spring-damper facing infrastructure).

### CDO defaults (BP-added vars; live)
| Variable | Type | Default |
|---|---|---|
| `InitialBoost` | bool | False (transient — set true on activation, cleared after `InitialBoostTime`) |
| `InitialBoostTime` | double | **0.2** s |
| `InitialBoostSpeed` | double | **800.0** cm/s |
| `InitialBoostAcceleration` | double | **2000.0** |
| `AfterBoostAcceleration` | double | **300.0** |
| `SteepSlopeAngle` | double | **40°** |
| `ShallowSlopeAngle` | double | **10°** |
| `SteepSlopeSpeed` | double | **800.0** |
| `ShallowSlopeSpeed` | double | **500.0** |
| `FlatGroundSpeed` | double | **100.0** |
| `SteepSlopeDeceleration` | double | **2000.0** |
| `FlatGroundDeceleration` | double | **500.0** |

Inherited SmoothWalkingMode actual defaults (different from Walking):
- `Acceleration = 2000.0`
- `Deceleration = 200.0`  *(very low — slide should glide, not stop)*
- `TurningStrength = **1.0**`  *(very low — no lateral velocity steering)*
- `AccelerationSmoothingTime = 0.0`  *(no smoothing — slide responds instantly)*
- `DecelerationSmoothingTime = 0.0`
- `FacingSmoothingTime = **0.15**` s  *(faster facing convergence than walking's 0.5)*
- `bSmoothFacingWithDoubleSpring = False` (same as Walking)
- All other deadzones same as Walking.

### Local variables (function-scoped)
- `CurrentOffset` (double) — same role as Walking
- `OverridenDesiredFacing` (Quat) — same role
- `SlopeAngle` (double) — angle between movement direction and ground normal, used to lerp Acceleration/Deceleration/MaxSpeed

### Functions
**`GenerateWalkMove`** — override of `USimpleWalkingMode::GenerateWalkMove` (78 K2 nodes).

Top comment: *"This Movement Mode is similar to BP_MovementMode_Walking, with the main difference being that the slope angle (relative to the direction of movement) affects the max speed deceleration."*

Layout: Entry → Sequence(2) → [InputCache | InnerSequence(7) → Parent call]

**Phase 1** — Same input cache (`MoverDefaultInputs`, `MoverCustomInputs`).

**Phase 2** — Computes `SlopeAngle`:
- Reads floor hit normal via `Mover.TryGetFloorCheckHitResult`.
- `SlopeAngle = ACos(Dot(GroundNormal, Normalize(DesiredVelocity))) - 90°`
  - Negative SlopeAngle = downhill (movement points downward)
  - Positive SlopeAngle = uphill

**Phase 3** — Per-tick parent writes:
- `MaxSpeedOverride`:
  - If `InitialBoost` = `InitialBoostSpeed` (800)
  - Else `MapRangeClamped(SlopeAngle, ShallowSlopeAngle*-1 → SteepSlopeAngle*-1, FlatGroundSpeed → ShallowSlopeSpeed → SteepSlopeSpeed)` — actually a 3-stage select based on whether `SlopeAngle > ShallowSlopeAngle*-1`.
  - Net: steeper downhill → higher max speed (FlatGround=100 → Shallow=500 → Steep=800)
- `Acceleration`: `InitialBoost ? InitialBoostAcceleration (2000) : AfterBoostAcceleration (300)`
- `Deceleration`: `MapRangeClamped(SlopeAngle, ShallowSlopeAngle*-1 → SteepSlopeAngle*-1, FlatGroundDeceleration → SteepSlopeDeceleration)`
  - Steeper downhill → higher Deceleration (counter-intuitive — counters speed gain)
  - Flat/uphill → low Deceleration so character glides longer
- **DesiredFacing override** — same Phase-2 logic as Walking (RotationOffset clamp to ±179° around CurrentOffset), writes `OverridenDesiredFacing`.

**Phase 4** — `Parent: Generate Simple Walk Move` with `DesiredFacing = OverridenDesiredFacing`.

Note: Slide does NOT override `Walk/Run/SprintFacingTime` etc. The parent `FacingSmoothingTime = 0.15` is a fixed CDO default while sliding.

### EventGraph
Single event `OnActivated` (5 nodes):
- `Set InitialBoost = true` → `Delay (Duration = InitialBoostTime = 0.2s)` → `Set InitialBoost = false`.
- This is what gives slides their kick-off acceleration burst.

---

## 4. BP_MovementTransition_FromSlide

**Parent:** `/Script/Mover.BaseMovementModeTransition`.

### Engine base provides
- `bool FirstSubStepOnly` (CDO: False)
- `bool bAllowModeReentry` (CDO: False)
- `bool bSupportsAsync` (CDO: False)
- `Evaluate(SimulationTickParams) → FTransitionEvalResult` — return `NextMode = NAME_None` to NOT transition, or a movement-mode name to switch.

### BP variables
**None.** Pure stateless transition.

### Functions
**`Evaluate`** — overrides parent. 14 K2 nodes.

Signature:
```
FTransitionEvalResult Evaluate(FSimulationTickParams Params)
```

Logic (single Branch):
1. Break `Params.ProposedMove → LinearVelocity`. Compute `Speed = VectorLengthXY(LinearVelocity)`.
2. Read `Params.MovingComps.MoverComponent`, cast to `CharacterMoverComponent`, call `IsCrouching`.
3. Branch condition = `(Speed <= 200) OR (NOT IsCrouching)`.
4. If true → `MakeTransitionEvalResult(NextMode = "Walking")`.
5. If false → return default (NextMode = NAME_None, no transition).

In other words: **slide ends when speed drops below 200 cm/s OR the character stops crouching.** Always lands in `Walking` mode.

Note: `StartState` is broken out but unused (the function only consults the in-flight `ProposedMove` and the live `MoverComponent`).

### EventGraph
**Empty.**

---

## 5. BP_MovementTransition_ToSlide

**Parent:** `/Script/Mover.BaseMovementModeTransition`.

### BP variables
**None.**

### Functions
**`Evaluate`** — 11 K2 nodes.

Logic (single Branch):
1. Break `Params.ProposedMove → LinearVelocity`. Compute `Speed = VectorLengthXY(LinearVelocity)`.
2. Read `Params.MovingComps.MoverComponent`, call `HasGameplayTag(TagToFind = "Mover.Stance.IsCrouching", bExactMatch = false)`.
3. Branch condition = `(Speed > 380) AND HasGameplayTag(IsCrouching)`.
4. If true → `MakeTransitionEvalResult(NextMode = "Sliding")`.
5. If false → no transition.

In other words: **slide starts when moving > 380 cm/s AND in a crouching stance.**

Note that this BP queries via `HasGameplayTag`, while FromSlide queries `IsCrouching` via direct `CharacterMoverComponent` API — slightly different paths to the same info.

### EventGraph
**Empty.**

---

## 6. Critical mesh-alignment insights (the "why" for AZ port)

1. **GASP's mesh stays on the capsule because the capsule yaw is itself smooth.** Engine `WalkingMode` (what AZ uses today) snaps capsule yaw instantly to `DesiredFacing`. GASP's `SmoothWalkingMode` runs every facing change through `SpringMath::CriticalSpringDamperQuat` with `FacingSmoothingTime` (0.5s walk, 0.4s run, 0.8s sprint, 0.2s idle, 0.15s slide). The Steering anim node is tuned around this curve — feeding it instant snaps causes the visible mesh-vs-capsule lag we have now.

2. **`bSmoothFacingWithDoubleSpring = false`** in GASP — the C++ header default is `true`. With double spring you get an S-curve with a longer lead-out (great for cinematic feel, bad for responsive locomotion). GASP overrode this off. Single-spring critical damper gives a faster lead-out and matches the Steering node's tuning.

3. **`OverridenDesiredFacing` clamping trick** (lives in BOTH Walking and Slide GenerateWalkMove): `RotationOffset` from `MoverCustomInputs` is added to `DesiredFacing` but clamped to `[CurrentOffset - 179°, CurrentOffset + 179°]`. This guarantees the spring-damper always picks the *short* arc going *toward* the input intent — the pawn can never decide that "the long way around" is shorter when the input gets close to ±180°. Without this clamp, a 179° → -179° swing in input would visibly snap the capsule the wrong way.

4. **Camera-snap protection** (Walking only, not Slide): there's an extra `MapRangeClamped(|Delta(DesiredFacing, CurrentFacing).Yaw|, 90→135, 0→0.2)` term subtracted from `FacingSmoothingTime`. When the user whips the camera 100°+ in one frame, this temporarily drops the smoothing time so the capsule starts catching up immediately (not lagging by 0.5s). Comment in BP: *"clamps the pawns angular velocity to at least be the control rotations rotation rate."*

5. **`JustLanded` 20000 cm/s² brake** — for 200ms after Falling→Walking transition, Deceleration is forced to 20000 (vs the normal 1000). This is what makes landings feel "sticky." Without this, the Mover would coast post-landing and the locomotion anims would think you're still mid-stride.

6. **Acceleration is NOT a single value** — Walking layers SprintAcceleration on top of RunAcceleration only when current speed already exceeds RunSpeed. So getting from 0 → 375 (Run) uses 800 cm/s²; getting from 375 → 585 (Sprint) uses 800+300=1100. AZ would need to replicate this layering or motion-matching transitions will feel wrong at the run→sprint boundary.

7. **Falling mode rotates the capsule too** — the BP explicitly writes `OutProposedMove.AngularVelocityDegrees` via `MovementUtils::ComputeAngularVelocityDegrees(... TurningRateLimit=300)`. AZ falling currently has no such override, so air rotation is whatever the engine does by default (likely none).

8. **Slide reuses Walking's facing infrastructure** — same SmoothWalkingMode parent, same OverridenDesiredFacing trick. Only Acceleration/Deceleration/MaxSpeed are slope-modulated. Don't try to write a custom rotation-only slide mode.

---

## 7. Properties on the Mover component itself (GASP wires these up)

GASP's `BP_MovementMode_*` BPs are *referenced* by `BP_SandboxCharacter_Mover`'s `CharacterMoverComponent` in its `MovementModes` map:
- `Walking → BP_MovementMode_Walking` (NOT engine `WalkingMode`)
- `Falling → BP_MovementMode_Falling` (NOT engine `FallingMode`)
- `Sliding → BP_MovementMode_Slide`
- The `Transitions` array on each mode (visible in CDO as `transitions = <Array>`) holds instances of `BP_MovementTransition_FromSlide` (on Slide) and `BP_MovementTransition_ToSlide` (on Walking).

AZ today uses engine defaults — `Walking → /Script/Mover.WalkingMode` and `Falling → /Script/Mover.FallingMode` (verified live 2026-05-04 via Python introspection of `AZ_BP_HeroPawn.CharacterMoverComponent.movement_modes`). To get GASP-parity capsule rotation, AZ must:
1. Either re-target its `MovementModes` map to point at GASP's BP_MovementMode_* (cross-folder reference), or
2. Subclass these BPs into AZ-owned versions and tune values, or
3. Port the BPs to C++ subclasses of USmoothWalkingMode / UFallingMode. Recommended path: create AZ_MovementMode_Walking.cpp inheriting USmoothWalkingMode, mirror the BP's GenerateWalkMove logic in C++ — this also gives access to `MoverCustomInputs.Gait/RotationOffset` without BP overhead.

---

## 8. Defaults quick-reference card (for porting)

```
Walking (USmoothWalkingMode subclass):
  WalkSpeed=165, RunSpeed=375, SprintSpeed=585, CrouchSpeed=200
  WalkAccel=500, RunAccel=800, SprintAccel=300 (additive)
  Walk/RunFacingTime=0.4s, SprintFacingTime=0.8s, IdleFacingTime=0.2s
  Walk/RunTurnStrength=8, SprintTurnStrength=4
  StoppingDecel=1000, GaitChangeDecel=300, JustLandedDecel=20000 (sticky)
  bSmoothFacingWithDoubleSpring=false, FacingDeadzone=0.1°

Falling (UFallingMode subclass — empty BP, parent + 1 angular vel write):
  AirControl=0.4, FallingDecel=200, OverTerminalDecel=800
  TerminalLateral=1500, TerminalVertical=2000 (clamped)
  bCancelVerticalSpeedOnLanding=true
  BP override: ComputeAngularVelocityDegrees(TurningRateLimit=300)

Slide (USmoothWalkingMode subclass):
  InitialBoost: 0.2s burst, Speed=800, Accel=2000
  AfterBoost: Accel=300
  SlopeAngles: Shallow=10°, Steep=40°
  SlopeSpeeds: Flat=100, Shallow=500, Steep=800
  SlopeDecels: Flat=500, Steep=2000
  TurningStrength=1, FacingSmoothingTime=0.15

Transition_ToSlide:   speed > 380 AND IsCrouching → "Sliding"
Transition_FromSlide: speed <= 200 OR NOT IsCrouching → "Walking"
```
