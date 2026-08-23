---
name: project_cmc_movement_feel_tuning
description: "★ CMC hero movement-feel tuning (2026-08-22/23): measured stop-clip deceleration profiles that set what braking SHOULD be, the current live values and why each is what it is, the [CmcFeel] instrumentation, gait-scaled braking (SHIPPED — keys off SPEED not gait, and why), and the play-rate-warping safety net. Read before touching BP_CMC_Hero movement values or diagnosing foot slide on stops."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-23T03:30:52.009Z
---

# CMC hero movement feel — measured values and queued work

Branch `spike/cmc-backport`. All values live on `BP_CMC_Hero` and are pushed onto the CMC every tick by
`AAZ_CmcHeroCharacter::ApplyMovementFeelParams()`. The CMC component's own CDO values are IRRELEVANT —
they are overwritten before they are ever used. Read the live values from the `[CmcFeel]` log, not the
component defaults (that mistake cost a full diagnostic round on 2026-08-22).

## ★ The measurement that governs braking — stop-clip deceleration profiles

Root-motion speed sampled at 16 points per clip (2026-08-23). This is what the ANIMATION depicts:

| clip | len | peak | reaches 0 at | implied decel |
|---|---|---|---|---|
| `AnimPro_WalkFwdStop_LU` | 1.333 | 147 cm/s | 0.92 s | **160 cm/s²** |
| `AnimPro_WalkFwdStop_RU` | 1.533 | 162 cm/s | 0.86 s | **188 cm/s²** |
| `AnimPro_RunFwdStop_LU` | 1.267 | 353 cm/s | 0.95 s | **372 cm/s²** |
| `AnimPro_RunFwdStop_RU` | 1.500 | 388 cm/s | 1.03 s | **377 cm/s²** |

**Foot slide on a stop is exactly `clipStopTime − (Speed / BrakingDecelNoInput)`.** The sum is fixed, so
the only choice is WHERE inside ~0.95 s the character decelerates — not whether it slides.
At the old 2000 the capsule stopped in 0.19 s (run) and slid for 0.76 s.

## Current live values (2026-08-23)

| property | value | why |
|---|---|---|
| `BrakingDecelNoInput` | 500 | FALLBACK only — used when `bGaitScaledBraking` is off. |
| `bGaitScaledBraking` | **true** | on by default; selects by SPEED, not gait |
| `Walk/Run/SprintBrakingDecel` | **190 / 375 / 615** | = gait speed ÷ the clip's own ~0.95 s stop time |
| `BrakingDecelWithInput` | 500 | GASP's value |
| `MaxAccelerationBase` | 500 | (user-set) timeToSpeed walk 0.33 s / run 0.81 s. Owns ~2/3 of turn authority — see below. |
| `MaxAccelerationAtTopSpeed` | 300 | tapers over speed 300→700 |
| `GroundFrictionMax/Min` | 2.5 / 1.5 | dir-change tau 0.41–0.57 s (was 5/3 = 0.23–0.29 s) |
| `FrictionTaperSpeedMax` | 500 | |
| `GroundedRotationRateYaw` | −1 (instant) | walk; user prefers it |
| `RunRotationRateYaw` | **115** | user-set; my 135/160 writes were reverted — 115 is the tuned value |
| `SprintRotationRateYaw` | 90 | no sprint arc content, widest carve |
| `bGaitScaledRotationRate` | true | |

## ★ Direction authority — friction is NOT the main dial
Two terms rotate velocity each frame in `CalcVelocity`:
`Velocity -= (Velocity - AccelDir*|Velocity|) * min(dt*Friction, 1)` then `Velocity += Acceleration * dt`.
At walk 165 cm/s, 60 fps, perpendicular input: the acceleration term contributes **4.6°/frame** and
friction (at 2.17) only **2.1°/frame**. So **`MaxAcceleration` owns ~2/3 of turn authority.** Halving
friction alone changed turn rate 540°/s → 400°/s and read as "no effect". If you want inertia, move
`MaxAccelerationBase` first.
Honest inertia scalar = `Speed / MaxAcceleration` (time to reach speed). GASP feels heavier at the same
acceleration only because its speeds are higher: GASP walk 200/800 = 0.25 s vs ours 165/500 = 0.33 s.

## ✅ DONE 2026-08-23 — gait-scaled braking (shipped; see the SPEED-not-gait section below)
One `BrakingDecelNoInput` cannot serve three gaits. At 500:
- walk stops in 0.33 s vs a 0.92 s clip → **0.59 s slide, still too fast**
- run 0.75 s vs 0.95 s → 0.20 s slide, good
- sprint 1.17 s vs 0.95 s → **overshoots 0.22 s** — capsule still gliding after the feet have planted

Mirror the existing `bGaitScaledRotationRate` switch in `ApplyMovementFeelParams`:
`bGaitScaledBraking` + `WalkBrakingDecel ≈ 190` / `RunBrakingDecel ≈ 375` / `SprintBrakingDecel ≈ 615`
(each = gait speed ÷ ~0.95 s, the clip's own stop time). Sprint reuses the run stop clips, so its number
is derived, not measured.

## ★ Braking keys off SPEED, not gait (bug found + fixed 2026-08-23)
`CurrentGait` comes from GAMEPLAY TAGS (`ResolveGaitAndStanceFromTags`: Movement.Sprinting / .Running,
else Walk). Releasing the sprint input drops the tag to Walk **on that frame** while the body is still
travelling at sprint speed. Gait-keyed braking therefore applied `WalkBrakingDecel` (190) at
**spd=558** — a **2.9 second** stop. Caught by `[CmcFeel]`, which showed `braking=190 rotYaw=-1` at 558.

**The rule: rotation rate legitimately follows INTENT (so `bGaitScaledRotationRate` keying off gait is
correct); braking must follow MOMENTUM, which only Speed2D knows.** They are not symmetric and must not
be copied from each other. `ApplyMovementFeelParams` now selects the braking value by `Speed2D` against
`WalkSpeed`/`RunSpeed`.

### ❌ REJECTED — the "one StopTimeSeconds parameter" idea is WRONG
It looked clean (`braking = Speed2D / StopTimeSeconds` reproduces 190 / 375 / 615 exactly) but
`a = v/T` makes deceleration PROPORTIONAL TO SPEED, i.e. exponential decay: it never reaches zero and
travels `v0 * T`. The stop clips decelerate LINEARLY (constant deceleration), so braking must be a
CONSTANT for the duration of a stop. Do not revive this.

### ✅ THE ACTUAL FIX (shipped 2026-08-23) — latch the band at the stop edge
Recomputing the band from the CURRENT speed each frame decays it as the character slows. Measured: one
stop from sprint stepped **615 -> 375 -> 190** and travelled **493 cm over 2.15 s** against a clip
depicting **167 cm over 0.95 s**. It also re-classified the POOL mid-stop, so a single stop played
`RunFwdStop_LU` for 770 ms and then `WalkFwdStop_LU` for 1237 ms — two clips for one stop.

`AAZ_CmcCharacterBase::UpdateSelectionGait()` now captures the band ONCE, on the no-input edge, and holds
it (`bStopBandLatched` / `LatchedStopBand`). Both consumers read it: `ApplyMovementFeelParams` picks the
braking value from `SelectionGait`, and the contract publishes `SelectionGait` for the gate rows. One
latch, two readers — constant deceleration AND one pool per stop.
Outside a stop, `SelectionGait = max(commanded, BandForSpeed(Speed2D))`, so acceleration is unchanged.

## ✅ DONE 2026-08-23 — anim-side Gait follows SPEED, not the tag
Third instance of the same root cause in one session. `AZ_CmcCharacterBase.cpp:151` puts the COMMANDED
gait in the contract (`Out.Gait = CurrentGait`, tag-derived) and `AZ_CmcAnimInstance.cpp:489` copies it
(`Gait = CharacterProperties.Gait`). The DATABASE GATE ROWS key on it. So releasing sprint drops the tag
to Walk on that frame and the gate row narrows to `WalkMove` **while the body is still at 565 cm/s**:

```
#338 SprintFwdLoop1 -> WalkFwdLoop    spd=565 gates=[WalkMove,...]
#339 WalkFwdLoop -> WalkFwdStop_RU    spd=395 gates=[WalkMove,...]  cost=+1.46
#340  57ms WalkFwdStop_RU -> WalkFwdLoop   spd=365
#341  74ms WalkFwdLoop -> WalkFwdStop_RU   spd=337
```
`AnimPro_WalkFwdStop_RU` depicts **162 cm/s**. Offered to a body at 565 it wins on a bad cost and then
flip-flops with the walk loop every 57-74 ms — user-visible as "no stop animation plays".

**SHIPPED:** `FAZ_CmcAnimContract` now publishes `WalkSpeed`/`RunSpeed`/`SprintSpeed` to `FAZ_CmcAnimContract`,
the gait->speed table; the anim instance derives a speed-implied gait (classified at the MIDPOINTS
480 / 270) and uses `max(commanded, speedImplied)`. Pools narrow only as the body actually slows;
acceleration behaviour is unchanged because a commanded sprint still wins the max. Commanded gait still drives `MaxWalkSpeed` — that part
is correctly intent-driven; do NOT change `SetGait`.

**THE RULE, now three-for-three:** anything acting on MOMENTUM (braking, friction, which animation pool
can plausibly match) must key off `Speed2D`. Only things expressing INTENT (rotation rate, target max
speed) may key off the gait tag. When adding any new gait-keyed behaviour, ask which one it is first.

## ★ Play-rate warping on stops is a SAFETY NET, not a licence for arbitrary braking
`MoveData_Speed` + `Enable_PlayRateWarping` curves were authored on all six stop clips (2026-08-23) from
each clip's own measured root-motion speed, sampled at 30 Hz. `Get_DynamicPlayRate` then drives the
stride at `groundSpeed / clipSpeed`, so the feet match the ground at ANY braking value — no slide.
`Get_DynamicPlayRate` also now uses a wider floor (`StopsMinPlayRate = 0.2` vs the loop's 0.8) while a
Stops-tagged clip is selected, so the clip can settle into its plant instead of striding in place.

BUT it does NOT guarantee the clip REACHES the plant. A slower rate keeps the clip at earlier, faster
frames, so the ratio spirals toward the floor:
| RunBrakingDecel | stop | distance | play-rate band | look |
|---|---|---|---|---|
| 375 | 1.00 s | 1.87 m | ~1.0 | plays naturally, reaches the plant |
| 500 | 0.75 s | 1.41 m | 0.6–1.0 | mild compression, absorbs cleanly |
| 700 | 0.53 s | 1.00 m | floors at 0.2 | no slide, but the stride visibly stalls |
Past ~500 the honest answer is shorter stop CONTENT, not more braking.
NOTE `AnimPro_Crouch_WalkFwdStop_RU_new` has a 9 cm/s root-motion peak (essentially in place), so it
falls through the `MoveDataSpeed <= SMALL` guard and stays at rate 1.0 — a content gap, not a tuning one.

## ★★ DESIGNED, NOT BUILT — animation-driven stop/start (user proposal 2026-08-23)

**The idea (user's):** let the CLIP carry the deceleration/acceleration profile as curve data, and have
the movement follow it — instead of hand-tuning braking to approximate what the animation depicts.
The data ALREADY EXISTS: `MoveData_Speed` was authored on all six stop clips from their own root motion
at 30 Hz. Today it only feeds play rate.

### Option A — curve-driven braking (movement follows animation)
```cpp
// ApplyMovementFeelParams, ONLY while a Stops-tagged clip is selected
const float ClipSpeed = Mesh->GetAnimInstance()->GetCurveValue(TEXT("MoveData_Speed"));
Move->BrakingDecelerationWalking = FMath::Max(0.f, (Speed2D - ClipSpeed) / DeltaTime);
```
Brake exactly hard enough to reach the clip's speed this frame → the capsule tracks the authored curve
exactly. **Zero slide by construction**, and stopping DISTANCE becomes a property of the content instead
of a tuned number. Same shape works for starts with an acceleration curve.

- Smaller and more certain of the two. Uses data we already have.
- COST: it **inverts the ownership doctrine**. Today CMC drives and animation follows (which is why
  `RootMotionMode` is montages-only, and the whole of 2026-08-23 was spent making animation match
  movement). This makes STOPS animation-authoritative. Legitimate — many shipped games do exactly this
  for stops — but it is a doctrine change and will interact with network prediction later.
- RISK: on the frame the stop is selected the character's speed may not equal the clip's speed at the
  chosen entry frame → a velocity step. MM picks the entry frame by trajectory so they should be close;
  clamp the per-frame correction and measure the step before trusting it.
- RISK: play-rate warping and this feed each other (rate depends on speed, speed now depends on the
  clip). Pick ONE to own the sync — if braking tracks the curve, force play rate to 1.0 on stops.

### Option B — distance matching (animation follows movement) — architecturally consistent
Choose the clip's PLAYBACK TIME by remaining stop distance rather than letting it run at a rate. Same
zero-slide result, but CMC stays authoritative and no doctrine changes. PoseSearch can do this natively:
the predicted trajectory already contains the deceleration (`StepCharacterMovementGroundPrediction` uses
`BrakingDeceleration`), so a distance/velocity channel in the schema makes the search pick the frame
whose remaining travel matches. Bigger piece; overlaps with [[project_mm_state_selection_plan]].

### Recommendation
A for stops and starts specifically, where the clip genuinely knows better than a tuned constant; B if
the ownership inversion turns out to matter for prediction or for the NPCs. Do NOT do both — they are
two answers to the same question and would fight.

## ✅ STOPS — final state 2026-08-23 (working; read this before touching stop selection)

Chain, all verified in one PIE: **every stop reaches Idle, zero no-animation frames, survival 61-77%.**

Four mechanisms, in the order they were needed. Each fixed a real failure, and the LAST one made two of
the earlier problems disappear:
1. `KeepPlayingOneShotSearchable` — re-adds the playing one-shot's DB to the pool. At Speed2D 0 the gate
   row flips to StandIdle and Walk/Run_Stops leaves the union, so `InterruptOnDatabaseChange` evicted the
   clip by SET MEMBERSHIP (survival was 82ms median against 0.93-1.53s clips). Release at 0.9 of clip
   length for Stops, 0.7 for other one-shots.
2. `OverrideContinuingPoseCostBias = -1.0` on all 6 stop clips. Inert on its own — a cost bias cannot help
   a candidate that is not in the search — but load-bearing once (1) puts the DB back.
3. `Get_DynamicPlayRate` returns **1.0** once a Stops clip is selected and `Speed2D <= 2`. The remaining
   frames are settle, not travel, so matching stride to ground speed is meaningless there; the 0.2 floor
   made the clip crawl and never reach its release point.
4. **Idle suppression BOUNDED BY SPEED** (`bIsStop && Speed2D > 2`), never allowed to empty the pool.

### ⚠ The two regressions I caused here, and why (do not repeat)
- Suppressing Idles unconditionally EMPTIED the pool at Speed2D 0 — StandIdle's only DB is Idles-tagged —
  giving frames with NO animation at all (`-> None, db=None gates=[]`) for ~480ms. Worse than the bug
  being fixed. Guard: only suppress when a non-Idle alternative remains.
- Releasing on a fraction of the CURRENT clip TRAPPED the character: a stop handing off to its
  opposite-foot variant RESETS SelectedTime, so the release point was never reached and the two stop
  clips traded forever with Idle locked out. A speed bound cannot loop, because speed only decreases
  while stopping.

### ★ The lesson that generalises
The `Stop_RU -> Stop_LU` foot handoff was **never a competition between the variants**. With Idle
suppressed there was nothing else in the pool, so the search took the only other candidate. Restoring
Idle made it vanish. I had queued `SearchThrottleTime` to "fix" it — that would have cost input
responsiveness to suppress a symptom of my own gating. **Before adding a mechanism to stop a bad pick,
check what is actually left in the pool to pick from.**

### Known and accepted
Survival 61-77%, so the last third of the settle is cut once the body halts — the cost of letting Idle
compete again, and the alternative was being trapped. Raising it means bounding the suppression by
elapsed time rather than speed, which needs state; not worth it unless it reads badly in play.

## Instrumentation
`[CmcFeel]` — once a second, GAME THREAD, guarded on `IsGameWorld()` (without the guard an unpossessed or
ABP-preview instance logs its untouched CMC CDO — friction 8 / maxAccel 800 / braking 2048 / rotYaw 360 —
at spd=0 forever and buries the real samples 81-to-17). Prints friction, maxAccel, braking, rotYaw,
timeToSpeed, dirTau, orientToMove, ctrlDesired. Use it instead of reasoning about whether a value applied.

## PARALLEL-EDIT HAZARD (hit twice on 2026-08-22)
The user tunes these values in the editor at the same time. `MaxScaleRatio` and `MaxAccelerationBase` were
already at their target when scripted writes ran (no-ops), and `RunRotationRateYaw` 160 was reverted to 115.
ALWAYS read-before-write and print BEFORE/AFTER; never assume a scripted CDO write survived.
See [[feedback_parallel_editor_edits]].

Related: [[project_mm_state_selection_plan]] (the selection side), [[project_cmc_backport_spike]].
