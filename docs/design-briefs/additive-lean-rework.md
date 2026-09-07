# Additive lean rework — port the CMC acceleration model, restore the Y axis, bind the layer

**Status: SPEC, queued. Not implemented.** Written 2026-09-07.
Companion to [moveraniminstance-gasp-refactor-plan.md](moveraniminstance-gasp-refactor-plan.md); the C++ half
lands inside that plan's **step 2a** (`Update_AdditiveLean` stage) and **step 3** (`Update_States`).

**Prerequisites — both real, neither optional:**
1. `AZ_MoverAnimInstance.*` must be free of the in-flight rifle/aim workstream (as of writing it carries
   ~200 uncommitted lines there).
2. Every item below adds a `UPROPERTY` or a new member, which **Live Coding cannot patch**. Closed-editor
   CLI build, then reopen.

---

## 1. Why — three implementations, measured against each other

| | GASP / AZ v1 | v2 Mover (today) | CMC (`UAZ_CmcAnimInstance`) |
|---|---|---|---|
| accel source | raw frame derivative | raw frame derivative | **smoothed**, `VInterpTo(..., LeanInterpSpeed=8)` |
| normalisation | one tuned divisor, `MapRange(Speed2D, 200..320 -> 500..800)` | same | **physical budgets** — longitudinal / `MaxAcceleration` or `MaxDeceleration`; lateral / `Speed2D * rad(LeanTurnRateReference)` |
| decomposition | velocity space, take `.Y` | same | **longitudinal + lateral split**, recombined, clamped to unit |
| output space | velocity space | velocity space | **actor space** (`.X` forward, `.Y` right) |
| axis packing | direction switch routes to X *or* Y | X only, **Y hardcoded 0** | `.Y` into X, **Y hardcoded 0** |
| speed scaling | none | none | `lerp(0.5, 1, invlerp(Speed2D, 165..375))` |

**Verdict: adopt CMC's acceleration model; fix the packing in both.**

The model is better for four reasons, in descending order of importance:

1. **The lateral budget is dimensionally correct.** Centripetal acceleration required to turn at rate `w`
   while travelling at `v` is exactly `v*w`. So `|LatVec| / (Speed2D * rad(LeanTurnRateReference))` reads as
   *"how hard am I cornering relative to a 180 deg/s turn"* and is **speed-invariant**. GASP's single divisor
   curve is a fudge that must be re-tuned whenever movement speeds change — and ours have.
2. **It is smoothed.** A raw frame-to-frame derivative of a simulated velocity is noisy; GASP and v2 feed
   that directly into the pose.
3. **Separate deceleration budget**, so braking reads correctly instead of being scaled by the acceleration
   budget.
4. **Actor-space output already IS the body-space lean vector**, so the movement-direction switch becomes
   unnecessary — it generalises to strafe and diagonals for free. The v1 switch was hand-coding four cases
   of what the vector supplies directly.

Supporting coincidence worth noting: `LeanSpeedRangeIn = (165, 375)` on the CMC instance is exactly
`WalkSpeed = 165` / `RunSpeed = 375` from `AZ_PawnMovementMode_Walking`. The tuning ports over unchanged.

---

## 2. Defects being fixed

| # | Defect | Evidence |
|---|---|---|
| D1 | `LeanAmount.Y` is **never written** — the only write is `FVector2D(Lat, 0.f)` | `AZ_MoverAnimInstance.cpp:638` |
| D2 | B / L / R lean was dropped when the forward-only gate went in; v1 had a 4-case direction switch | `AZ_AnimInstance.cpp` `Update_AdditiveLean` vs v2 `bForwardLean` |
| D3 | `ApplyMeshSpaceAdditive.Alpha` in `AZ_ABP_MoverHero_MHC` is fed by an `Absolute(Float)` whose input pin is **unbound (0.0)** — the body lean renders at alpha 0, i.e. not at all | live graph query, node `188A4AA8` |
| D4 | `LeanAlpha` exists in C++ for exactly this pin (see its header comment) and has **zero occurrences in either ABP** | `grep -a -c LeanAlpha` on both `.uasset` = 0 |
| D5 | The AO gate `BlendListByBool.bActiveValue` and both `BlendListByEnum.ActiveEnumValue` pins are unbound literals | live graph query, nodes `995AAAE1`, `E972706A`, `46CEC53C` |
| D6 | Both `BlendListByEnum` nodes are typed to **GASP's** `E_MovementState` / `E_MovementMode`; they cannot bind to `ChooserContext.MovementMode` (`EAZ_MovementMode`, different asset **and** different member order) | live graph query |

---

## 3. C++ change

### 3.1 New / changed members on `UAZ_MoverAnimInstance`

```cpp
// ---- Lean model (ported from UAZ_CmcAnimInstance) ----
/** Smoothed velocity derivative. The raw per-frame derivative is too noisy to drive a pose. */
UPROPERTY(Transient) FVector SmoothedVelocityAcceleration = FVector::ZeroVector;

UPROPERTY(EditDefaultsOnly, Category = "AZ|V2|Anim|Lean", meta = (ClampMin = "0.1"))
float LeanInterpSpeed = 8.f;

/** Reference turn rate for the centripetal lateral budget: lateral accel to hold a turn of this rate at
 *  the current speed is Speed2D * radians(this). Makes the cornering signal speed-invariant. */
UPROPERTY(EditDefaultsOnly, Category = "AZ|V2|Anim|Lean", meta = (ForceUnits = "deg/s"))
float LeanTurnRateReference = 180.f;

/** Speed band over which the lean scales in. Defaults match WalkSpeed/RunSpeed on the walking mode. */
UPROPERTY(EditDefaultsOnly, Category = "AZ|V2|Anim|Lean")
FVector2D LeanSpeedRangeIn  = FVector2D(165.f, 375.f);
UPROPERTY(EditDefaultsOnly, Category = "AZ|V2|Anim|Lean")
FVector2D LeanSpeedRangeOut = FVector2D(0.5f, 1.f);
```

`PrevVelocity` already exists (`.h:464`) and is retained — it feeds the raw derivative before smoothing.

### 3.2 The shared math

Implement as a **`static`** on `UAZ_MoverAnimInstance`, taking everything by parameter — the same shape as
the existing `ResolveGrabIKTarget`, which is already shared between the hero and infected instances. This
gives one owner for the math without editing `AZ_CmcAnimInstance.cpp` during the rifle workstream.

```cpp
/** Acceleration expressed in ACTOR space and normalised to [-1,1] per axis against physical budgets:
 *  the longitudinal component against accel/decel, the lateral against the centripetal budget for
 *  LeanTurnRateReference. Returns the body-space lean vector directly (X forward, Y right) — no
 *  movement-direction switch needed. */
static FVector ComputeRelativeAccelerationAmount(
    const FVector& SmoothedAccel, const FVector& Velocity, float Speed2D,
    const FQuat& ActorRotation, float MaxAcceleration, float MaxDeceleration, float TurnRateRefDeg);
```

Body is a direct port of `UAZ_CmcAnimInstance::CalculateRelativeAccelerationAmount()`
(`AZ_CmcAnimInstance.cpp:693-723`) with the `CharacterProperties` reads replaced by parameters.

**Follow-up, deliberately deferred:** repoint `UAZ_CmcAnimInstance` at this static and delete its copy.
Not done now only to avoid touching a second file mid-workstream. Until then the two copies can drift —
if you change one, change both.

### 3.3 The Mover-side budgets

CMC reads `CurrentMaxAcceleration` / `CurrentMaxDeceleration` from its `CharacterProperties` struct —
**the CMC instance already has the pawn→anim seam the Mover instance lacks** (plan §G1, again).

On Mover, source them from `UAZ_PawnMovementMode_Walking`:

- **Acceleration is per gait**: `WalkAcceleration = 500`, `RunAcceleration = 800`, `SprintAcceleration = 300`.
  Select by `ChooserContext.Gait`.
- **Deceleration**: `StoppingDeceleration = 6000`, `GaitChangeDeceleration = 300`,
  `JustLandedDeceleration = 20000`. **Read `AZ_PawnMovementMode_Walking.cpp` at implementation time and
  mirror its own selection rule** — do not guess which applies when. If mirroring turns out to be awkward,
  the honest fallback is `StoppingDeceleration` whenever move intent is zero, else the gait's acceleration.

Until the §G1 seam exists, read these through the cached mover component. When the seam lands, they move
into the struct and this code stops reaching.

### 3.4 The stage body

Replacing `AZ_MoverAnimInstance.cpp:614-649`:

```cpp
void UAZ_MoverAnimInstance::Update_AdditiveLean(float DeltaSeconds)
{
    const FVector Velocity  = /* frame context */;
    const FVector RawAccel  = (Velocity - PrevVelocity) / FMath::Max(0.0001f, DeltaSeconds);
    PrevVelocity            = Velocity;
    SmoothedVelocityAcceleration =
        FMath::VInterpTo(SmoothedVelocityAcceleration, RawAccel, DeltaSeconds, LeanInterpSpeed);

    const FVector Rel = ComputeRelativeAccelerationAmount(
        SmoothedVelocityAcceleration, Velocity, ChooserContext.Speed2D,
        Cached_Pawn->GetActorQuat(), GaitAcceleration, ActiveDeceleration, LeanTurnRateReference);

    const float T = FMath::Clamp((ChooserContext.Speed2D - LeanSpeedRangeIn.X)
                    / FMath::Max(LeanSpeedRangeIn.Y - LeanSpeedRangeIn.X, KINDA_SMALL_NUMBER), 0.f, 1.f);
    const float SpeedScale = FMath::Lerp(LeanSpeedRangeOut.X, LeanSpeedRangeOut.Y, T);

    // AXIS ORDER AND SIGNS ARE UNVERIFIED - see section 5.
    const FVector2D Target = FVector2D(Rel.Y, Rel.X) * SpeedScale;

    // Gate on the PHASE only. The MovementDirection == F requirement is REMOVED: it is what erased
    // B / L / R lean (D2), and the actor-space vector already encodes direction. The LocomotionLoop
    // requirement stays - a transition plays an RM clip that already owns the body, and an additive
    // lean stacked on root motion tilts over the planted turn-start.
    const bool bLean = ChooserContext.bIsMoving
        && ChooserContext.SMState == EAZ_StateMachineState::LocomotionLoop;

    LeanAmount = FMath::Vector2DInterpTo(LeanAmount, bLean ? Target : FVector2D::ZeroVector, DeltaSeconds, 10.f);
    LeanAlpha  = FMath::FInterpTo(LeanAlpha, bLean ? 1.f : 0.f, DeltaSeconds, 10.f);

    if (const UAZ_WeaponAnimationProfile* P = ActiveWeaponAnimationProfile.Get(); P && !P->bUseUnarmedLeans)
    {
        LeanAmount = FVector2D::ZeroVector;
        LeanAlpha  = 0.f;
    }
}
```

### 3.5 State variables for the layer (plan step 3)

```cpp
/** GASP's MovementState. The enum already exists in AZ_LocomotionTypes.h and is populated only by the
 *  v1 instance; v2 replaced it with the bIsMoving bool. Restoring it lets the AdditiveLeans layer gate
 *  the GASP way, and is the first consumer of the Update_States trackers. */
UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|V2|Anim|States")
EAZ_MovementState MovementState = EAZ_MovementState::Idle;

/** GASP's EnableAO — true while the aim offset owns the head, so the head lean is suppressed. */
UPROPERTY(BlueprintReadOnly, Transient, Category = "AZ|V2|Anim|States")
bool bEnableAO = false;
```

Populated in `Update_States`:
```cpp
MovementState = ChooserContext.bIsMoving ? EAZ_MovementState::Moving : EAZ_MovementState::Idle;
bEnableAO     = AimAlpha > KINDA_SMALL_NUMBER;   // coordinate with the rifle/aim workstream
```

`bEnableAO` reads `AimAlpha`, which is **owned by the in-flight aim work** — agree the source with that
workstream before implementing, rather than duplicating the condition.

---

## 4. Graph change (editor, no build) — `AZ_ABP_MoverHero_MHC`, `AdditiveLeans`

The layer's topology is already GASP-shaped (114 nodes, both enum gates, the AO bool, the
`Disable_AdditiveLeans` curve mask, and `LeanAmount_X`/`_Y` wired into `BS_AZ_Relaxed_Walk_Leans` and
`BS_AZ_Relaxed_Run_Leans`). What is missing is bindings.

1. **`ApplyMeshSpaceAdditive.Alpha` ← `LeanAlpha`**, and delete the `Absolute(Float)` node. `Abs(X)` was
   the X-only workaround; with Y driven it would give alpha 0 for pure sideways lean. **This is the change
   that makes the layer visible at all (D3/D4).**
2. **AO gate** `BlendListByBool.bActiveValue` ← `bEnableAO` (interim: `ChooserContext.bIsAiming`).
3. **`E_MovementState` node** → either retype to `EAZ_MovementState` once §3.5 lands, or replace with a
   `BlendListByBool` on `ChooserContext.bIsMoving` today.
4. **`E_MovementMode` node** → retype to `EAZ_MovementMode`, bind ← `ChooserContext.MovementMode`, and
   **re-check which pin is the grounded one** — our order is `OnGround=0, InAir=1, Slide=2, Traversing=3`
   and does not match GASP's (D6).

`AZ_ABP_MoverAnimInstance` still has the old X-only layer (36 nodes, `LeanAmount_Y` unconnected). Decide
whether it follows or is retired — it is the non-MetaHuman ABP.

---

## 5. Unverified — confirm before or during implementation

1. **Blendspace axis semantics.** `BS_AZ_Relaxed_Walk_Leans` / `_Run_Leans` declare axes named literally
   `X` and `Y`, range −1..1, grid 4 — no semantic names in the metadata. **Which animation sits at
   `(+1,0)` versus `(0,+1)` decides the packing order and the signs in §3.4.** Read it off the blendspace
   editor; do not infer it.
2. **`Disable_AdditiveLeans` curve.** Bound as `AlphaCurveName` on the layer's final `TwoWayBlend`. If our
   turn clips do not carry that curve, the mask is inert and leans will fight the turn animations — the
   exact problem the GASP comment on that node describes. Check before tuning lean strength.
3. **Deceleration selection rule** — §3.3.
4. **Double-lean.** Cornering lean also arrives through MM: `WalkLocoDatabase` / `RunLocoDatabase` carry
   `LeanL/R` clips selected on a curving trajectory. With the additive layer finally rendering, the two
   may compound. Look at a hard corner before tuning either.

---

## 6. Falsifiable check

State before running: with the layer bound and Y driven, a **hard corner at run speed** should show lean
on `LeanAmount.X`, and **hard acceleration / braking in a straight line** should show a non-zero
`LeanAmount.Y` — which is impossible today, since Y is a literal `0.f`.

The debug HUD already prints `lean %+.2f / %+.2f alpha %.2f` (now in `UpdateDebug()`), so this is readable
without new instrumentation. Pass condition, in order:

1. `LeanAmount.Y != 0` during straight-line accel/brake — proves D1/D2 fixed.
2. `alpha > 0` while moving — proves D3/D4 fixed.
3. Lean falls to 0 through every start / stop / turn transition — proves the `LocomotionLoop` gate survived
   the removal of the `MovementDirection == F` requirement.

Item 3 is the regression risk of this change and is the one to watch.
