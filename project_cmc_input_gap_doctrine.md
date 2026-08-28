---
name: project_cmc_input_gap_doctrine
description: "★★★ THE WORKING DESIGN for CMC locomotion input gaps — \"zero input\" is not \"the player stopped\". Ownership + grace + no-gap handback, the three landed fixes, their invariants and acceptance metrics. Supersedes project_cmc_turn_exit_stop_stab."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-27T21:49:38.628Z
---

2026-08-27, branch `spike/cmc-backport`. User verdict after the third fix: **"ok works!!!!!"**. This is the
design that made CMC locomotion stop stabbing stop-clips and walk-cycles into turns. Everything below was
measured from `Saved/Logs/AZ.log` and engine source — nothing here is inferred.

# The principle

**Zero acceleration is not one situation. It is at least three, and at the instant it happens they are
physically identical.**

1. The player genuinely released.
2. The player is **reversing** — keyboard: W released before S is pressed; stick: crossing the deadzone.
3. **An animation owns movement** and input is deliberately suppressed (the turn montage).

The whole class of bugs came from one line treating all three as "the player stopped":
`ChooserContext.bIsMoving = AccelerationAmount > MoveIntentDeadzone`. That drops the SM
`LocomotionLoop → TransitionToIdle` (`AZ_LocomotionStateMachine.cpp:264`), which opens the Stops pool, and
MM — behaving perfectly — commits a stop clip. The new direction then lands and it all unwinds.

**Nothing sampled at that instant discriminates.** Proven, not assumed: the Mover reference ORs a
near-future speed term into `bIsMoving` (`AZ_MoverAnimInstance.cpp:689`), and at every FALSE stop `futSpd`
(0.1-0.3 window) read **304-314** with speed 352-364 — exactly what a genuine stop from that speed reads.
No threshold separates them. Do not try again.

Only two things separate the cases, so the design has exactly two mechanisms — plus one rule that stops
case 3 from ever being created:

| Case | Discriminator | Mechanism |
|---|---|---|
| 3 — anim owns movement | **who owns movement** (explicit fact) | Ownership flag, one owner |
| 2 — reversal | **what happens next** (elapsed time) | Grace window before commitment |
| — | don't create the gap at all | Hand input back on the same frame |

# The three landed mechanisms

### 1. Ownership — `IsAnimDrivingMovement()` (one owner per fact)
`virtual bool AAZ_CmcCharacterBase::IsAnimDrivingMovement()` — base returns `IsPlayingRootMotion()`;
`AAZ_CmcHeroCharacter` overrides to OR in `bTurnMontageActive`. The anim instance applies the same OR to
`bMontageActive_GT`, so both sides agree on one fact.

★ **`IsPlayingRootMotion()` does NOT report a clip started with `PlaySlotAnimationAsDynamicMontage`** —
which is exactly how the turn montage plays. Every guard written against it was silently INERT for turns.
That is what let the stop contract latch mid-turn and force `SelectionGait = LatchedStopBand`
(`AZ_CmcCharacterBase.cpp:228`), dropping a running character into the WALK databases at 230 cm/s:
```
#13 RunFwdLoop -> WalkFwdLoop | spd=230 mtg=1 | cmd=Run sel=Walk | gates=[WalkMove]
#14 back to RunFwdLoop 162 ms later
```
That was the "strange anims during the rotation". (Also the "stale stop-band latch" open bug in
[project_cmc_movement_feel_tuning.md] — now closed.)

**RULE: on the CMC path never call `IsPlayingRootMotion()` directly to mean "an animation is driving".
Call `IsAnimDrivingMovement()`.**

### 2. Grace — `StopPoolGraceSeconds` (gate the POOL, never the state)
Withhold the **Stops** databases from the MM pool for the first `StopPoolGraceSeconds` of
`TransitionToIdle`. The SM still enters the state immediately, so contracts and braking are untouched —
only the irreversible commitment (picking a stop clip) waits.
- `UAZ_CmcAnimInstance::StopPoolGraceSeconds` — `EditDefaultsOnly`, default **0.14 s**, dialable on the
  ABP with no rebuild. 0 restores the old immediate behaviour.
- `UAZ_CmcAnimInstance::SMStateElapsed` — seconds in the current SM state; **one owner**,
  `Update_LocomotionStateMachine`.
- Clause `bStopGrace` in `Get_DatabasesToSearch`, guarded on `!CurrentDatabaseTags.Contains(Stops)` so a
  stop already playing is never yanked mid-clip.

Measured reversal gaps to size it against: **46, 51, 91, 92, 96, 99, 116, 117, 136, 217, 285, 778 ms**
(median ~104). 0.14 s catches the bulk. A genuine stop simply starts that much later — at a *lower* entry
speed, which MM matches better.
**Cost:** the stop clip, and the curve-driven braking it owns, begins that late (~50 cm at 363 cm/s). The
stop's time budget (`AZ_CmcCharacterBase.cpp:194`) starts at the latch, not at the clip, so the clip has
~140 ms less of it — watch `[CmcStop]` for a cut-short stop if the grace is raised much.

### 3. No-gap handback — `TickTurnMontage` re-applies the held stick
`OnMoveTriggered` early-returns while the turn owns the capsule, so the frame the flag clears had no
`AddMovementInput` at all. `TickTurnMontage` now re-applies `LastMoveInputDir` if it is within
`AZ::RmMontage::InputHeldWindow` (0.1 s). Double-adding is harmless — `ScaleInputAcceleration` clamps the
accumulated input vector to unit size. Logs `handback=1`.
*Not sufficient alone:* it depends on CMC ticking after the character, and **no engine prerequisite
enforces that** (`Character.cpp` has no `AddTickPrerequisite`). The order-independent half is
`bIsMoving` ORing in the existing `bMontageJustReleased_GT`, which holds the moving claim for exactly the
release frame and cannot delay a genuine stop.

# Acceptance metrics (re-check these after any locomotion change)

| Invariant | Where |
|---|---|
| `cmd=` and `sel=` stay EQUAL through a turn; `gates=` never flips to `[WalkMove]` while running | `[CmcSel]` |
| `saved=1` on every `TransitionToIdle -> LocomotionLoop` edge caused by a reversal | `[CmcSM]` |
| `handback=1` on every `[CmcTurn] complete` | `[CmcTurn]` |
| **no** stop pick at `SM=LocomotionLoop` — every stop is `SM=TransitionToIdle accel=0.000` | `[CmcPick]` |

Verified 2026-08-27: 11/11 handback; 3/3 grace windows (dwell 22/61/66 ms) produced zero stop picks while
both genuine stops still fired (first at exactly 139 ms = the grace expiring on schedule); `cmd`/`sel`
agree through turns. One accepted leak: a 339 ms dwell — 339 ms of zero input at speed IS a stop.

# Diagnostic doctrine earned here (cost ~a day)

- **Engine order (verified):** `FAnimInstanceProxy::UpdateAnimation_WithRoot` calls
  `NativeThreadSafeUpdateAnimation` (`AnimInstanceProxy.cpp:1350`) and only THEN traverses the graph via
  `UpdateAnimationNode` (`:1395`). So `Update_Logic` — SMState, AccelerationAmount — is already current
  when an AnimGraph node binding runs. A long-standing comment claiming the opposite was load-bearing for
  two gates and was **false**.
- **A log line is only authoritative for values sampled at the same point in the frame as the event it
  reports.** `[CmcSel]` prints from `Update_Logic` (step 1), pairing LAST frame's selection with THIS
  frame's state — which manufactured the impossible `stop @ SM=LocomotionLoop accel=1.00` and killed three
  correct-looking hypotheses. Instrument **at the decision point**: `[CmcPick]` lives inside
  `Update_MotionMatching_PostSelection`, `[CmcSM]` on the SM edge itself.
- **MM was never at fault.** Gates, normalization set (`PSN_AZ_CMC`, 25 members) and trajectory prediction
  were all correct the entire time. Three "MM ignores the gate" theories were artifacts of the skew.

# PoseSearch database facts (2026-08-27, live asset registry)

- 17 `PSD_AZ_*` are referenced by `AZ_ABP_CmcAnimInstance` (the live `DatabaseGates`) — these are searched.
- 8 are in `PSN_AZ_CMC` but in NO gate row, so never searched: `Crouch_Walk_Pivots`, `Crouch_Walk_Turns`,
  `Stand_GaitTransitions`, `Stand_IdleBreaks`, `Stand_Sprint_Starts`, `Stand_Sprint_Stops`,
  `Stand_Sprint_Turns`, `Stand_TurnInPlace`.
  ★ **Do not remove them from the normalization set to "clean up".** A `UPoseSearchNormalizationSet`
  computes shared deviation statistics across all members — dropping members **shifts every MM cost in the
  project**. They cost nothing at runtime. Note `Stand_TurnInPlace` and `Stand_IdleBreaks` are not dead
  code: `ShouldTurnInPlace()` and the `IdleBreak` SM state are live — they are features wired in C++ but
  **missing from the gate table**.
- DELETED (0 referencers, not in PSN, superseded by the turn-montage system): `PSD_AZ_Stand_Run_Turns`,
  `PSD_AZ_Stand_Walk_Turns`. Recoverable via git.

See [project_cmc_velocity_master_verdict.md], [project_cmc_turn_day_2026-08-24.md],
[project_cmc_movement_feel_tuning.md], [project_cmc_mm_content_verdict.md],
[feedback_verify_never_presume.md], [reference_rider_mcp_new_tools.md].
