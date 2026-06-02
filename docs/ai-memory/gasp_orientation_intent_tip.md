---
name: gasp_orientation_intent_tip
description: GASP Get_OrientationIntent per-mode behavior + the threshold-gated TIP pattern (60° trigger, cached intent, hysteresis-via-cache)
type: reference
originSessionId: a2ff7a1b-69a0-4cb1-ae34-89e39479ce96
---
# GASP `Get_OrientationIntent` Logic (verbatim from BP comments)

**Source:** `/Game/Blueprints/SandboxCharacter_Mover` → `Get_OrientationIntent` function, BP comment nodes.

## Per-mode behavior — IDLE on the ground (no movement input)

| RotationMode | Behavior |
|---|---|
| **OrientToMovement** | "we simply return last frame's values, meaning the OrientationIntent will not change" |
| **Strafe** | Same as OrientToMovement — keep last |
| **Aim** | "update OrientationIntent to AimingRotation **whenever the character is rotated 60 degrees or more away from AimingRotation**. This effectively creates a basic turn in place behavior." |

## Per-mode — IDLE in the air

| RotationMode | Behavior |
|---|---|
| OrientToMovement | "don't change the OrientationIntent" |
| Strafe / Aim | "use AimingRotation as the OrientationIntent" |

## Per-mode — IDLE while sliding

| RotationMode | Behavior |
|---|---|
| OrientToMovement | "use the current velocity as the OrientationIntent" |
| Strafe / Aim | "use AimingRotation as the OrientationIntent" |

## Traversing mode

> "set the OrientationIntent to be the actors forward vector, meaning it will not try to rotate."

## Walking + Movement Input (any mode, ground)

OrientToMovement → MoveInput direction
Strafe / Aim → AimingRotation

## TIP Pattern (threshold-gated rotation)

```
CurrentDelta = |AimingRotation.Yaw - ActorYaw|

if CurrentDelta >= 60°:
    write OrientationIntent = AimingRotation_AsForwardVector
    cache it
else:
    keep last cached value (do NOT touch OrientationIntent)
```

**Why caching matters:** once the threshold crosses and intent is set, the Mover starts rotating capsule. As capsule catches up, CurrentDelta drops below 60° — but the rule doesn't re-fire (intent stays cached). Capsule continues rotating to the cached target until aligned. This gives clean "commit-to-turn" semantics without oscillation.

**No hysteresis needed:** the cache itself acts as hysteresis. Once committed, ride to alignment. Next camera rotation that exceeds 60° from current actor position commits the next turn.

## TIP firing condition (separate from rotation)

GASP's `ShouldTurnInPlace` should mirror the same threshold check (current delta ≥ 60°), so TIP anim plays during the rotation window.

## Implementation in AZ (matching GASP exactly)

**`OnProduceInput` idle branch:**
```cpp
const float ControllerYaw = CharInputs.ControlRotation.Yaw;
const float ActorYaw = GetActorRotation().Yaw;
const float CurrentDelta = FMath::Abs(FRotator::NormalizeAxis(ControllerYaw - ActorYaw));
if (CurrentDelta >= 60.f)
{
    const FVector CameraFwd = FRotationMatrix(CharInputs.ControlRotation).GetUnitAxis(EAxis::X);
    LastIdleOrientationTarget = FVector(CameraFwd.X, CameraFwd.Y, 0.f).GetSafeNormal();
}
CharInputs.OrientationIntent = LastIdleOrientationTarget;  // cached, persists across frames
```

**`ShouldTurnInPlace`:**
```cpp
const float CurrentDelta = FMath::Abs(FRotator::NormalizeAxis(
    CharacterProperties.AimingRotation.Yaw - CharacterTransform.Rotator().Yaw));
return CurrentDelta >= 60.f && Speed2D < 50.f && MovementState == Idle;
```

## Common Pitfalls (we hit them all in this project)

1. **Always-set OrientationIntent without threshold gate** → Mover continuously rotates with mouse, no TIP threshold ever crossed for moderate rotations.
2. **Reading FutureFacingDelta in ShouldTurnInPlace** → chicken-egg: predictor needs OrientationIntent set to predict rotation, but OrientationIntent is gated on bIsTurning derived from FutureFacingDelta.
3. **Clearing the cache when delta drops** → re-triggers oscillation as capsule catches up.
4. **Comparing first trajectory sample to actor yaw** → injects mesh-component baked offset (-90° on SK_SurvivalMan) as permanent FutureFacingDelta bias.

The GASP-correct pattern avoids all four.

## What's different in our AZ HeroPawn

We don't have an explicit Aim/Strafe/OrientToMovement RotationMode switch yet. Treating idle as "Aim mode equivalent" (always allow TIP at 60°) gives a typical TPS body-follows-camera feel. If/when we add a real RotationMode, OrientToMovement would skip the TIP behavior entirely (keep last unconditionally).
