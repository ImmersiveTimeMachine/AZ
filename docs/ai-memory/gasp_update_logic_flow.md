---
name: gasp_update_logic_flow
description: GASP idle-walk-idle cycle — exact logic flow for IsMoving, Update_States, Update_Trajectory. What AZ is missing and the fix.
type: reference
originSessionId: f6181671-d4a5-4b82-954f-4f2f5396f92f
---
# GASP Idle-Walk-Idle Cycle — Exact Logic

Source: `/Game/Blueprints/SandboxCharacter_Mover_ABP` in AZ project (GASP's AnimBP imported).
Analyzed via `mcp__unrealclaude__unreal_blueprint_query`.

## The Key Insight

**`MovementState` in GASP is NOT "am I moving (velocity > 10)"** — it's **"do I INTEND to be moving"** based on trajectory prediction.

```
MovementState = Moving  ⇔  Trj_FutureVelocity != 0 (tolerance 10)  AND  Acceleration != 0
MovementState = Idle    otherwise
```

When player **releases W at speed 195**:
- Trajectory predictor looks 1s ahead → predicts velocity will be 0 → `Trj_FutureVelocity = (0,0)`
- IsMoving = false → **MovementState = Idle INSTANTLY** (while Speed2D is still 195)
- SM transitions `LocomotionLoop → TransitionToIdle` while speed is still high
- Chooser evaluates Stand Stopped row 3 (speed 10-200 + SM=TransIdle) → Walk Stops → stop anim fires ✅

## Full State Tracking Pattern

For EVERY state enum (MovementState, MovementMode, Gait, Stance, MovementDirection, RotationMode), GASP tracks **5 variables**:

| Variable | Purpose |
|----------|---------|
| `X` | Current value |
| `X_LastFrame` | Previous frame value (for change detection) |
| `X_Recent` | Delayed-previous value — holds old value for `RecentTimeLimit` seconds after change (used for "recently was in state" rules) |
| `X_Time` | Time accumulated in CURRENT state (resets on change) |
| `X_LastStateTime` | **Duration the PREVIOUS state lasted** (captured at transition) |

### Update Pattern (from GASP's `Update State Values` macro)

On state change:
```
X_LastStateTime = X_Time         // save duration of OLD state
X_Time = 0                       // reset counter
X_Recent = X_LastFrame           // keep old value as "recent"
// X_Recent reverts to X_LastFrame after RecentTimeLimit seconds (0.1-0.2s)
```

On same state:
```
X_Time += DeltaSeconds
```

### How Chooser Uses These

Walk Stops column c1 binds to `MovementState_LastStateTime`:
- When transitioning from Moving → Idle, `MovementState_LastStateTime` = duration character was in Moving state
- Row `c1 ≥ 0.5` means "was moving for at least 0.5s before this stop triggered"
- Prevents spurious stops from micro-movements (tap W briefly)

## Trajectory Generation — `Update_Trajectory` Function

GASP uses Engine function: `UPoseSearchTrajectoryLibrary::PoseSearchGenerateTrajectoryUsingPredictor`

Parameters:
- `InPredictor` — UMoverTrajectoryPredictor* (implements IMoverTrajectoryPredictor interface)
- `InDeltaTime` — frame delta
- `InOutTrajectory` — existing trajectory (updated in-place)
- `InOutDesiredControllerYawLastUpdate` — float (stored between frames)
- `InHistorySamplingInterval` = 0.033s (default)
- `InTrajectoryHistoryCount` = 15
- `InPredictionSamplingInterval` = 0.1s (default)
- `InTrajectoryPredictionCount` = 15

Output: 30 total samples (15 history + 15 future) with **proper deceleration prediction**.

After generation, GASP also calls:
- `PoseSearchHandleTrajectoryWorldCollisions` — applies gravity + collision traces

Then computes derived values from trajectory:
- `Trj_FutureVelocity` = `GetTrajectoryVelocity(trajectory, time1=0.4, time2=0.5)` — velocity 0.4-0.5s ahead
- `Trj_NearFutureVelocity` = `GetTrajectoryVelocity(trajectory, time1=0.1, time2=0.2)` — velocity 0.1-0.2s ahead
- `Trj_PastVelocity` = `GetTrajectoryVelocity(trajectory, time1=-0.3, time2=-0.2)` — velocity 0.2-0.3s ago
- `Trj_FutureFacing` = `GetTrajectorySampleAtTime(trajectory, 1.5).Facing.ToRotator()` — predicted facing 1.5s ahead
- `FutureFacingDelta` = `Get_TotalFacingDelta(times=[0, 0.25, 0.75, 1.5])`
- `Trj_TurnAngle` = `GetTrajectoryTurnAngle()`
- `Trj_CurrentAngularVelocity` = `GetTrajectoryAngularVelocity(trajectory, 0, 0.1)`
- `Trj_IsCircling` = `|PastAngularVel.Z| > 200 AND |CurrentAngularVel.Z| > 200`

## `Update_EssentialValues` Function

Populates (from Mover proxy / Character):
- `Velocity`, `Velocity_LastFrame`
- `Speed2D`
- `HasVelocity` (> some threshold)
- `LastNonZeroVelocity`
- `Acceleration`, `Acceleration_LastFrame`
- `AccelerationAmount`, `HasAcceleration`
- `RelativeAcceleration`
- `CharacterTransform`, `CharacterTransform_LastFrame`
- `VelocityAcceleration` (delta / DeltaTime)
- `CharacterProperties` struct

## `Update_States` Function — Order

GASP's Update_States runs **6 blocks** in sequence (Sequence node with 6 outputs), one per state enum:

1. **MovementMode** (RecentTimeLimit = 0.2s)
2. **RotationMode** (RecentTimeLimit = 0.1s)
3. **MovementState** (RecentTimeLimit = 0.1s)
   - NewState = `Select(IsMoving, Option0=Idle, Option1=Moving)` ← uses IsMoving function
4. **Gait** (RecentTimeLimit = 0.1s)
5. **Stance** (RecentTimeLimit = 0.1s)
6. **MovementDirection** (RecentTimeLimit = 0.1s)
   - Special: Uses `Trj_IsCircling AND MovementState==Moving` to mask direction when circling

## `IsMoving` Function Body

```
Return = (Trj_FutureVelocity != Vector(0,0,0) with tolerance 10.0)
         AND
         (Acceleration != Vector(0,0,0))
```

Both must be true. Comment:
> Look at the future velocity (determined by trajectory generation) to determine if the character is trying to move (future velocity > 0), or trying to stop (future velocity = 0).

## Full Idle-Walk-Idle Cycle

### Idle → Walk (press W)

```
Frame N:   Input=0,  Trj_FutureVelocity=0,  Accel=0  →  MovementState=Idle, SM=IdleLoop
           Chooser: Stand Idle Loops → RTG_RM_Idle (loop, BranchIn)

Frame N+1: Input=(1,0,0),  Mover accelerates
           Trj_FutureVelocity=(300,0,0) (predicts motion)
           Accel=(400,0,0) (accelerating)
           IsMoving=true → MovementState=Moving (CHANGED from Idle)
           MovementState_LastStateTime = MovementState_Time (idle duration, e.g. 5.2s)
           MovementState_Time = 0
           SM transition IdleLoop → TransitionToLocomotion fires
           Chooser: Stand Walks F → RTG_RM_WalkFwdStart (no PoseSearch notifies)

Frame N+2..M: Walk start plays, speed builds to ~195
              SM transition TransitionToLocomotion → LocomotionLoop fires (when anim complete)
              Chooser: Stand Walks F (locomotion) → RTG_RM_WalkFwdLoop (BranchIn)
```

### Walk → Stop (release W)

```
Frame M:     Input=0 (just released),  Speed=195,  Mover decelerates
             Trj_FutureVelocity=(0,0,0) (predictor sees stop in near future)
             Accel=(-400,0,0) (decelerating)
             IsMoving: FutureVel==0 → FALSE → MovementState=Idle (CHANGED from Moving)
             MovementState_LastStateTime = 3.5s (how long we walked)
             MovementState_Time = 0
             SM transition LocomotionLoop → TransitionToIdle fires (MovementState==Idle)
             Chooser: Top-level "SM=TransitionToIdle, Stance=Standing, Gait=Walk" → Stand Stopped
                      Stand Stopped row 3 (SM=TransIdle, speed=[10..200], turning=false) → Walk Stops
                      Walk Stops row 0 (delta=[-60..60], c1=any, dir=F) → RTG_RM_WalkFwdStop_LU
                      Tags="Stop", bLoop=false, BlockTransition notify

Frame M+1..K: Stop anim plays, speed decelerates from 195 → 0
              BlockTransition notify active → BlendStack WON'T switch to another anim
              Chooser re-evaluates every tick but BlendStack ignores (blocked)
              MovementState stays Idle, MovementState_Time accumulates

Frame K:     Stop anim ~90% complete (PoseSearchExcludeFromDatabase notify window ends)
             IsAnimationAlmostComplete returns true
             SM transition TransitionToIdle → IdleLoop fires (Rule 1: IsAnimAlmostComplete AND StateTime > 0)
             Chooser: Stand Idle Loops → RTG_RM_Idle
```

## What AZ Currently Has (WRONG)

1. **Wrong IsMoving**: `MovementState = bHasVelocity ? Moving : Idle` where `bHasVelocity = Speed2D > 10`. Fires too late on release (waits for speed to drop below 10).

2. **Missing state tracking fields**: We have `_LastFrame` and `_Time`/`_LastStateTime` only for MovementState. Missing: `_Recent`, and all fields for Stance/Gait/MovementMode/RotationMode/MovementDirection.

3. **Fake trajectory**: `NativeUpdateAnimation` builds trajectory as `MeshLocation + Velocity * Time` — just linear extrapolation, no deceleration prediction. Sample.Facing is set to current mesh facing for all samples (no predicted rotation).

4. **Missing trajectory values**: `Trj_NearFutureVelocity`, `Trj_PastVelocity`, `Trj_TurnAngle`, `Trj_CurrentAngularVelocity`, `Trj_IsCircling` are either unset or computed wrong.

## The Fix — What Needs To Change

To get GASP-exact idle-walk-idle:

### 1. Fix Trajectory Generation (CORE FIX)
Replace manual trajectory loop with call to:
```cpp
UPoseSearchTrajectoryLibrary::PoseSearchGenerateTrajectoryUsingPredictor(
    MoverTrajectoryPredictor,  // interface on our HeroPawn
    DeltaSeconds,
    CharacterTrajectory,
    PreviousDesiredControllerYaw,
    OutTrajectory,
    0.033f, 15, 0.1f, 15
);
```
This gives proper 15+15 samples with deceleration prediction.

### 2. Fix MovementState Logic
Replace:
```cpp
MovementState = bHasVelocity ? Moving : Idle;
```
With:
```cpp
const FVector FutureVel2D(Trj_FutureVelocity.X, Trj_FutureVelocity.Y, 0.f);
const FVector Accel2D(Acceleration.X, Acceleration.Y, 0.f);
const bool bFutureMoving = FutureVel2D.SizeSquared() > 100.f;  // tolerance 10
const bool bAccelerating = Accel2D.SizeSquared() > 1.f;
MovementState = (bFutureMoving && bAccelerating) ? Moving : Idle;
```

### 3. Add Full State Tracking (`_Recent`, `_Time`, `_LastStateTime`) for all 6 enums

Each state needs the 5-variable pattern. Simplest: write a templated helper or macro that does:
```
if (X != X_LastFrame) {
    X_LastStateTime = X_Time;
    X_Time = 0;
    X_Recent = X_LastFrame;
    X_RecentTimer = RecentTimeLimit;
} else {
    X_Time += DeltaSeconds;
    if (X_RecentTimer > 0) {
        X_RecentTimer -= DeltaSeconds;
        if (X_RecentTimer <= 0) X_Recent = X;
    }
}
```

### 4. Compute Trj_FutureVelocity from trajectory
After generating trajectory, call:
```cpp
Trj_FutureVelocity = UPoseSearchTrajectoryLibrary::GetTrajectoryVelocity(
    CharacterTrajectory, 0.4f, 0.5f, false
);
```

## Property Naming (MUST match GASP exactly for chooser bindings)

| GASP Property | Must Be In AZ_AnimInstance |
|---------------|----------------------------|
| `MovementState` | ✓ (we have it) |
| `MovementState_LastFrame` | ✓ (we have it) |
| `MovementState_Recent` | ✗ (missing) |
| `MovementState_Time` | ✓ (just added) |
| `MovementState_LastStateTime` | ✓ (just added, with underscore) |
| `Trj_FutureVelocity` | ✓ (we have it) |
| `Trj_NearFutureVelocity` | ✓ (we have it) |
| `Trj_PastVelocity` | ✗ (missing) |
| `Trj_FutureFacing` | ✓ (we have it) |
| `Trj_TurnAngle` | ✗ (missing) |
| `Trj_CurrentAngularVelocity` | ✓ (single float, should be Vector) |
| `Trj_IsCircling` | ✓ (we have it) |
| `FutureFacingDelta` | ✓ (we have it) |

Same pattern needed for: `MovementMode`, `Gait`, `Stance`, `MovementDirection`, `RotationMode` — each needs `_Recent`, `_Time`, `_LastStateTime`.

## Anim Notifies Applied (from previous work)

188 RTG_RM_ anims in `/Game/Assets/RM_Movement/` already have correct notifies:
- All stops: BlockTransition + ExcludeFromDatabase + cardinal BranchIn
- All starts: no PoseSearch (GASP pattern)
- All loops: cardinal BranchIn
- Turn-in-place: BlockTransition + OverrideContinuingPoseCostBias
- Jump starts & lands: BlockTransition + BranchIn
- Transitions (stand↔crouch): BlockTransition + BranchIn
- Idle loops: BranchIn only
- See `reference_gasp_anim_notifies.md` for full rules

## Chooser State

`CHT_AZ_CharacterAnimations` at `/Game/AZ/Blueprints/Animation/MotionMatching/` has:
- Top-level routing by SM state + Stance + Gait ✅
- Walk Stops sub-chooser columns:
  - c0: FutureFacingDelta [-60..60] for cardinal forward
  - c1: **MovementState_LastStateTime** [0..0.5] or [0.5..+inf] ← needs property
  - c2: MovementDirection
  - c3: OutputStructColumn with Tags="Stop"
- Anim refs remapped to RTG_RM_* (188 refs)

## Current C++ Patches That Need to Go

When implementing GASP-exact logic, REMOVE these patches:
1. `bHasMovementInput` property — not used by GASP, don't need
2. Velocity-based FutureFacingDelta workaround in trajectory loop — proper trajectory will give correct values via `GetTrajectorySampleAtTime`
3. Debug overlay extras — can keep for debugging but clean before shipping

## Recommended Implementation Order

1. **Replace trajectory generation** with `PoseSearchGenerateTrajectoryUsingPredictor` — this is the foundation
2. **Compute Trj_FutureVelocity** from trajectory using `GetTrajectoryVelocity`
3. **Fix MovementState logic** to use IsMoving (Trj_FutureVelocity + Accel)
4. **Add `_Recent`/`_Time`/`_LastStateTime` tracking** for all 6 state enums
5. **Remove patches** (bHasMovementInput, velocity-based FutureFacingDelta)
6. Test idle→walk→idle cycle end-to-end

## Trap: Mesh-space facing offset in trajectory samples

**Symptom:** `FutureFacingDelta` stuck at exactly ±90° regardless of input. Character continuously turns one direction in TIP loop. `Turn=1`, `Delta=-90.0` permanently.

**Cause:** `MoverPoseSearchTrajectoryPredictor.cpp:130` reads facing from `VisualComp->GetComponentRotation()`. The SK_SurvivalMan skeleton has a baked-in **-90° yaw offset** between mesh component and actor. So trajectory `Sample.Facing` is in mesh space, NOT actor space.

When `Get_TotalFacingDelta` initializes `PrevYaw = ActorFacing.Yaw` and the first trajectory sample is in mesh space, the first iteration's delta becomes `(mesh_yaw - actor_yaw) ≈ -90°` — a permanent bias injected into the sum.

**Fix (in `Update_Trajectory`):** initialize `PrevYaw` from the **first trajectory sample**, not from actor yaw. Sum-of-consecutive then telescopes to (last_sample - first_sample), which is correctly mesh-space-internal:

```cpp
FTransformTrajectorySample FirstSample;
UPoseSearchTrajectoryLibrary::GetTransformTrajectorySampleAtTime(CharacterTrajectory, 0.f, FirstSample, false);
float PrevYaw = FirstSample.Facing.Rotator().Yaw;  // NOT ActorFacing.Yaw
for (const float T : SampleTimes) { ... }
```

GASP's BP equivalent likely sidesteps this by using consecutive-sample-only deltas internally; our C++ port had the bug.

## Decision Point (Before Implementation)

User said "GASP exactly". Two approaches:
- **A) C++ in AZ_AnimInstance::NativeUpdateAnimation** — keep logic in C++, where our current code is
- **B) BP function `Update_Trajectory` in AZ_ABP_HeroPawn** — mirror GASP's BP structure exactly

GASP does it in BP. Option B is most faithful but requires BP node graph edits. Option A is cleaner code-wise but differs from GASP in implementation location (not logic).
