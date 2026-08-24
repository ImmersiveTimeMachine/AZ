# Design question — driving capsule speed from per-frame animation curves (UE 5.8, CharacterMovementComponent + Motion Matching)

## The proposal I want reviewed

Every locomotion animation carries a per-frame ground-speed curve (`MoveData_Speed`), baked from that
clip's own root motion. Drive the character as:

```
capsuleVelocity = inputDirection * clipSpeedAtCurrentFrame
```

**The animation supplies the speed MAGNITUDE. The player supplies the DIRECTION.**

The claim: this gets most of root motion's benefit (content-authored motion, no foot slide, no tuning
constants) while avoiding root motion's main liability with a sparse animation library — the clip's
authored *direction* being wrong for what the player actually asked for.

Intended scope is continuous locomotion only: loops, starts, stops. Discrete turns and pivots are handled
separately (dynamic montages + Motion Warping), because there the authored direction IS the content.

---

## Architecture

- **UE 5.8.** `ACharacter` + `UCharacterMovementComponent` (CMC). CMC is authoritative for position and velocity.
- `RootMotionMode = RootMotionFromMontagesOnly` — locomotion graph clips do **not** move the capsule today.
- **Animation selection is Motion Matching** (PoseSearch plugin). C++ returns a set of PoseSearch databases
  each frame; the MM node picks both the clip AND the entry frame by pose+trajectory cost, feeding a BlendStack.
- **The content library is SPARSE:** ~125 locomotion clips, versus Epic's Game Animation Sample at 500+
  (they ship 136 run pivots; we have 4). Cost ties of 0.05–0.26 between candidates are routine, so selection
  between near-identical candidates is close to arbitrary.
- Single-player first, but the design shouldn't make network prediction impossible later.

---

## Measured data (root motion sampled at 30 Hz — not assumed)

**Loops — steady-state ground speed:**

| clip | speed | configured gait speed |
|---|---|---|
| `WalkFwdLoop` | 172.6 | WalkSpeed **165** |
| `RunFwdLoop` | 375.7 | RunSpeed **375** |
| `SprintFwdLoop1` | 641.8 | SprintSpeed **585** |
| `Crouch_WalkFwdLoop` | **172.5** | CrouchSpeed **90** |
| `RunBwdLoop` / `RunLtLoop` / `RunRtLoop` | 229.6 / 224.5 / 235.0 | — |
| `RunStrafeLeft45Loop` / `Left135Loop` | 375.7 / 229.6 | — |
| all standing strafe loops | 172.6 | — |
| `WalkArchLoop_L` (curving) | peak 186, avg 120 | — |

Note the crouch row: the crouch loop depicts **172.5 cm/s** while the game moves the character at **90**.
That is a 92% mismatch, and it is the kind of thing this proposal is meant to make structurally impossible.

**Starts** — heterogeneous. `WalkFwdStart` peaks 128 and *ends at 114*, while the walk loop runs at 172.6 —
a genuine discontinuity at the start→loop handoff. Turning starts end anywhere from 117 to 194.

**Stops** — peak 147–388 cm/s, all reaching zero in 0.86–1.03 s. Note the invariant is stop *time*, not
deceleration.

**Turn/pivot clips** split three ways: in-place turns translate **exactly 0 cm** (104–155 °/s yaw); moving
pivots travel 78–356 cm at 257–727 °/s; arc loops are continuous.

---

## What is already built and working

Curve-driven **stops** ship and work:

```cpp
BrakingDecelerationWalking = (Speed2D - ClipSpeed) / ConvergenceTime;
```

75 engagements versus 1 rejection in a play session. Stop duration now lands on target at any release speed,
where the previous three-band constant was exact at only three speeds.

The proposal above is the generalisation of this to all locomotion.

---

## Verified engine facts that constrain the answer

- `ComputeOrientToMovementRotation` orients toward **`Acceleration`**, not velocity
  (`CharacterMovementComponent.cpp:6605-6621`); zero acceleration returns `CurrentRotation`.
- `CalcVelocity`'s braking branch requires `(bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax`
  (`:3910`). With input held, CMC takes the **friction** branch at `:3923` and `BrakingDeceleration` is never
  applied. So anything that works during a stop does not automatically work while a stick is held.
- The PoseSearch trajectory predictor reads `GetMaxSpeed()` and `GetMaxBrakingDeceleration()` **live** to
  simulate the future path (`PoseSearchTrajectoryLibrary.cpp` ~`:73-74`). Anything written to those flows
  straight into the Motion Matching query.
- `UAnimInstance::GetCurveValue` returns the **blend-weighted** value across the BlendStack.
  `UAnimSequenceBase::EvaluateCurveData` reads one clip at one time. These give materially different answers
  during blends — for stops, the blend-weighted read was wrong (it returned the outgoing loop's 172 instead of
  the stop clip's 147).
- Motion Matching enters clips **mid-way**: measured entry at 10–33% into the clip, and one-shots survive
  830–1018 ms of 1.267–1.533 s clips, so roughly the middle 55–70% of any one-shot actually plays.

---

## Questions

1. Is `velocity = inputDir × clipSpeed` sound in CMC, and what is the correct implementation point?
   Candidates: write `MaxWalkSpeed = ClipSpeed` per frame and let normal acceleration track it; override
   `CalcVelocity` in a thin subclass; or set `Velocity` directly after the movement update. What breaks with
   each — collision resolution, slopes and ramps, step-up, crouch, falling and landing, based movement,
   analog input scaling, `MinAnalogWalkSpeed`?
2. **Blend handling.** For stops we concluded per-clip `EvaluateCurveData` is correct and blend-weighted
   `GetCurveValue` is wrong. For this proposal I believe the *opposite* — a blend between the walk loop (172)
   and run loop (375) *should* yield an intermediate ground speed. Is that right, or is there a trap? What
   about blending between a clip that has the curve and one that doesn't?
3. **Feedback loop.** Capsule speed comes from the selected clip; the trajectory predictor simulates the
   future from live CMC values; Motion Matching picks the next clip from that trajectory. Is that stable, or
   does it lock in / oscillate? Does clip A's speed bias selection toward clip A? With ties at 0.05–0.26 this
   matters.
4. **Mid-entry.** Entering a start clip 30% in reads a speed the body doesn't have. Convergence, like the stop
   system uses, or something better?
5. Is this meaningfully different from `RootMotionFromEverything`, or a worse-engineered version of it? Where
   specifically does it win or lose against real root motion — especially during blends, where root motion
   extracts from the actual blended pose while a curve reads a single clip?
6. Does this expose, hide, or worsen the start→loop discontinuity (start ends 114, loop runs 172.6)?
7. **Two speed owners.** Gameplay tags currently drive `MaxWalkSpeed` via a gait system. If the clip drives
   speed, which yields, and what else depends on `MaxWalkSpeed` that would break?
8. What does the proposal miss?
