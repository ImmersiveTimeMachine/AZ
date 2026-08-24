# Problem statement — locomotion stops in UE 5.8: capsule deceleration vs. stop-animation selection

## The question

In a UE 5.8 third-person game using `UCharacterMovementComponent` (CMC) for movement and Motion Matching
(PoseSearch plugin) for animation selection, **what is the stable architecture for stops?**

Concretely: when the player releases the movement stick at an arbitrary speed, the character must
(a) decelerate over a distance/duration that matches an authored stop animation, and
(b) reliably *play* that stop animation.

Today neither is guaranteed, and every fix so far has been a tuning constant that is only correct at one
speed. I want an architecture that is correct across the whole continuous speed range, not a better set
of numbers.

---

## Architecture (relevant parts only)

- **Engine:** UE 5.8 (source build). Third-person, single-player-first.
- **Movement:** `ACharacter` + `UCharacterMovementComponent`. CMC is **authoritative** for position and
  velocity.
- **Animation:** a C++ `UAnimInstance` subclass drives a Motion Matching node
  (`FAnimNode_MotionMatching`) feeding a BlendStack. Every frame, C++ returns the set of PoseSearch
  databases the node is allowed to search:
  `TArray<UPoseSearchDatabase*> Get_DatabasesToSearch() const`.
  The node then picks both the clip AND the entry frame by trajectory+pose cost.
  Interrupt mode: `InterruptOnDatabaseChange`.
- **Root motion mode: montages only.** Locomotion animation does NOT move the capsule. The doctrine is
  *CMC drives, animation follows.* Foot slide is therefore a pure mismatch between the capsule's motion
  and the motion the clip depicts.
- **Pool gating:** a small typed table of rows matched on
  `(MovementMode, Stance, MovementState, Gait)`; the union of matching rows' databases is the search
  pool. Databases carry tags: `Starts`, `Stops`, `Pivots`, `Loops`, `Idles`, `StanceTrans`.
- **Gait speeds:** Walk 165, Run 375, Sprint 585 cm/s. `MaxAcceleration` 500, tapering to 300 across
  speed 300→700.

### Content inventory — this is the real constraint, the library is sparse

Six stop clips total: walk L/R, run L/R, crouch-walk L/R. Sprint has **no** stop content and borrows the
run stops. For comparison, Epic's Game Animation Sample ships ~160 databases with e.g. 136 run pivots;
this project has ~125 clips in total. Motion Matching's cost function is reliable in dense pools and
unreliable in sparse ones — that is the background cause of a whole class of bugs here.

**Root-motion speed of each stop clip, measured at 16 samples/clip:**

| clip | length | peak speed | reaches 0 at | implied decel |
|---|---|---|---|---|
| WalkFwdStop_LU | 1.333 s | 147 cm/s | 0.92 s | 160 cm/s² |
| WalkFwdStop_RU | 1.533 s | 162 cm/s | 0.86 s | 188 cm/s² |
| RunFwdStop_LU  | 1.267 s | 353 cm/s | 0.95 s | 372 cm/s² |
| RunFwdStop_RU  | 1.500 s | 388 cm/s | 1.03 s | 377 cm/s² |

**Key observation:** peak speed varies 147→388 cm/s (2.6×) but time-to-zero is 0.86–1.03 s in all four.
The content's invariant is **stop TIME**, not deceleration.

---

## Current implementation

### Movement side

CMC has `BrakingFrictionFactor = 0` and `bUseSeparateBrakingFriction = false`, so in
`ApplyVelocityBraking` the friction term is zero and **`BrakingDecelerationWalking` is the only
decelerating term**. `GroundFriction` affects only direction change while input is held.

`BrakingDecelerationWalking` is selected from a three-band lookup, latched once at the instant input is
released. Latching was necessary: recomputing per frame as the character slowed made deceleration decay,
and one stop from sprint stepped 615→375→190 and travelled 493 cm over 2.15 s against a clip depicting
167 cm / 0.95 s.

```
band = Walk   if speed <= 270   -> braking 190     (= 165 / 0.87)
band = Run    if speed <= 480   -> braking 375     (= 375 / 1.00)
band = Sprint otherwise         -> braking 615     (= 585 / 0.95)
```

### Animation side

- Stop databases are removed from the search pool while input is held (a held stick mid-turn brakes
  exactly like a stop and the trajectory cannot tell them apart; input can).
- Start databases are removed above 100 cm/s (in sparse pools, start clips were winning mid-turn).
- A currently-playing one-shot's database is force-kept in the pool until 70% of clip length (90% for
  stops), because when speed reaches 0 the gate row flips to `StandIdle` and the Stops database would
  leave the union entirely — evicting the clip by *set membership*, not by cost. Before this fix, stop
  clips survived a median of 82 ms against 0.93–1.53 s of content.
- Idle databases are suppressed while a stop plays AND speed > 2 cm/s, never allowed to empty the pool.
- Play rate is warped by `groundSpeed / clipSpeed` using an authored `MoveData_Speed` curve on each stop
  clip, floor 0.2 while a stop is selected, so the stride matches the ground at any braking value.

---

## The symptom

Tap and release the forward key. The character slides to a halt with **no stop animation playing at
all** — it keeps a start or loop clip and blends to idle.

### Traced numbers for that case (walk gait, braking 190)

| release speed | latched band | capsule stop time | capsule distance | nearest stop clip |
|---|---|---|---|---|
| 100 | Walk (190) | 0.53 s | 26 cm | 0.92 s / 68 cm |
| 165 | Walk (190) | 0.87 s | 72 cm | matches |
| **269** | **Walk (190)** | **1.42 s** | **190 cm** | 0.92 s / 68 cm |
| 270 | Run (375) | 0.72 s | 97 cm | 0.95 s / 167 cm |

Three independent contributors were identified:

1. **Braking is exact at three speeds only.** The band lookup is correct at 165 / 375 / 585 and degrades
   toward each band edge. Worst case is one cm/s below the run threshold: 1.42 s of capsule travel
   against a 0.92 s clip.
2. **The stop is chosen by cost, not by event.** Releasing the stick is a discrete, unambiguous event,
   but it is expressed as "a stop database becomes eligible and may or may not out-bid whatever is
   currently playing."
3. **There is no low-speed stop content.** The slowest stop peaks at 147 cm/s. Below ~120 cm/s release
   speed, no stop clip is a plausible match and Motion Matching correctly declines to pick one.
   Additionally, on a short tap the character is still inside a Start one-shot, which the keep-alive rule
   holds for 0.7 × ~1.2 s = 0.84 s — longer than the entire tap — with no input-release escape.

---

## Already tried — please do not re-propose these

| approach | outcome |
|---|---|
| Raise/lower `GroundFriction` | **Inert.** `BrakingFrictionFactor = 0` removes friction from the braking path entirely. |
| One global `BrakingDecelerationNoInput` | Cannot serve three gaits. At 500: walk slid 0.59 s, sprint overshot by 0.22 s. |
| Per-frame `braking = speed / StopTime` | **Wrong.** Deceleration proportional to speed = exponential decay; never reaches zero, travels v₀·T. |
| Per-frame gait-band recompute | Deceleration decayed as speed fell; also re-classified the pool mid-stop, so one stop played two different clips. Fixed by latching the band at the release edge. |
| `OverrideContinuingPoseCostBias = -1.0` on stop clips | Inert alone — a cost bias cannot help a candidate that is not in the search pool. Load-bearing only after the membership fix. |
| `EPoseSearchInterruptMode` tuning | Governs only whether the *continuing pose* is considered. Cannot prevent a fresh re-pick. |
| Suppressing Idle unconditionally during a stop | **Regression:** emptied the pool at speed 0 (idle's database is the only one in the standing-idle row) → ~480 ms with no animation at all. |
| Releasing the keep-alive on a fraction of the *current* clip | **Regression:** a stop handing off to its opposite-foot variant resets playback time, so the release point was never reached and two stop clips traded forever with idle locked out. |
| More braking to kill the slide | Past ~500 the play-rate warp floors at 0.2 and the stride visibly stalls. Beyond that the honest answer is shorter stop content, not more braking. |

---

## Hard constraints

- Motion Matching + PoseSearch + BlendStack stay. They are production-grade in 5.8 and work well for
  *loops*, including blending straight↔arc by predicted curvature.
- Content is fixed for now — assume no new stop clips can be authored.
- CMC currently owns movement; animation follows. Inverting this (making stops animation-authoritative
  via root motion or curve-driven braking) is *permitted* but is a doctrine change and must be argued.
- Must not depend on per-frame cost tuning; the failure mode being fixed is precisely that cost
  competition is unreliable in sparse pools.
- Single-player first, but the design should not make later network prediction impossible. Note that the
  parameter push currently happens in the actor's `Tick`, which would not survive prediction as-is.

---

## One candidate solution — please challenge it

**Part 1 — make stop *time* the invariant instead of deceleration.**
At the existing release-edge latch, also capture entry speed, and compute once:
`braking = EntrySpeed / StopTimeSeconds` (≈0.93 s), held constant for the whole stop.
Because it is latched rather than recomputed, this is constant deceleration reaching zero at exactly T,
travelling `v₀·T/2` — matched to the content's own invariant at *every* entry speed, replacing three
tuned constants with one. Note this is the same formula as the rejected row above; the difference is
latch-once versus per-frame, which changes it from exponential decay to linear.

**Part 2 — make the stop an event, not a bid.**
Narrow the search pool to the Stops database exclusively for the duration of a stop, so a released stick
above the content floor deterministically plays a stop. Below the ~120 cm/s content floor, blend
deliberately to idle instead of pretending a stop exists. Add an input-release escape to the one-shot
keep-alive so a Start clip cannot outlive the stick.

---

## Specific things I want a second opinion on

1. Is latched `v₀/T` the right movement model, or is there a better standard approach for matching a
   capsule's deceleration to fixed-duration authored stop content?
2. Should stop selection be removed from the cost competition entirely (state-driven pool narrowing), or
   is there a way to make Motion Matching reliable here that does not require dense content?
3. Is inverting ownership for stops — letting the clip's authored speed curve drive CMC braking so the
   capsule tracks the animation exactly — better than making the animation track the capsule? What breaks?
4. Distance matching (choosing the clip's playback time by remaining stop distance rather than by play
   rate) is a known technique. Is it a better fit here than either of the above, given the sparse content?
5. What is the correct handling of the low-speed case where no stop content exists at all — is "blend to
   idle, no stop" the right answer, or is there a standard alternative?
