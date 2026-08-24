---
name: project_mm_state_selection_plan
description: "★★ PLAN (2026-08-22): retire the MM cost-competition model for DISCRETE events and select them by explicit state instead, via pool-narrowing in Get_DatabasesToSearch. Written after a full day where four separate bugs turned out to be one root cause: MM cannot decide between near-tied candidates in sparse pools. Contains the evidence, the v2-vs-GASP architecture diff, a 4-stage plan needing NO AnimGraph surgery, and the measured content reference table. Read before any further MM selection work on the CMC hero."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-24T00:42:04.327Z
---

# MM selection: state-driven for discrete events, cost-driven for loops

Branch `spike/cmc-backport`, CMC hero (`AZ_ABP_CmcAnimInstance` / `UAZ_CmcAnimInstance`).
Supersedes nothing; it is the concrete follow-through on the hybrid proposed in
[[project_cmc_backport_spike]] and the architecture question in [[project_architecture_rationale]].

## 1. Why — one root cause, four symptoms (all measured 2026-08-22)

| symptom | measurement | patch applied | verdict |
|---|---|---|---|
| `Start180_L/R` vs duplicate `Turn180_L/R` coin-flipping | 5 changes in 1.19 s, dt 19–153 ms, costs within 0.2 | deduped pools | fixed |
| walk arc ↔ straight loop alternating | 40+ flips at 21–96 ms, both clips cost 1.65–2.12 | `OverrideContinuingPoseCostBias −1.0` on arcs | fixed |
| four run pivot variants cycling | 3 changes in 72 ms; `Turn180_R_RU` median survival **168 ms** vs a **1.533 s** clip | bias −1.0 + removed from `Run_Loops` | partial (`R_LU` → 611 ms median) |
| `_L` start chosen regardless of turn direction | 9 of 10 starts pick `_L`, cost 0.03–0.19 | fixed INDIRECTLY by movement tuning — see below | ✅ largely resolved |

Every one is *"MM cannot separate near-tied candidates"*. GASP survives this because density makes
its cost function reliable (`Run_Pivots` 136 clips vs our 4). We have been hand-building substitutes —
gates, biases, notifies, dedup — one incident at a time. The handedness case proves the limit: cost
cannot see **intent**, so no notify or bias can fix it.

## 1a. ✅ START HANDEDNESS — resolved by MOVEMENT tuning, not by selection (2026-08-23)
Re-measured after the night's movement work and the symptom is gone. Do NOT build the pool split on the
old evidence.

| before | after |
|---|---|
| 9 of 10 starts picked `_L` | `AnimPro_WalkFwdStart` is the most common pick (7); L:R = 8:4 |
| `Start180_L` chosen at `turn=0` (a 180 clip for walking straight) | does not occur |

**Why:** at `MaxAccelerationBase = 800` the velocity snapped to the input almost instantly, so the
predicted path was straight and carried NO turn for the search to match — the handedness signal did not
exist to be found. At **500** the velocity aligns more slowly, so a turning start actually produces a
turning trajectory. **The movement change solved the selection problem.** This is the same lesson as the
arc loops: make the movement produce trajectories the content represents, rather than making selection
resist the movement.

STILL UNKNOWN: the 8:4 L/R ratio cannot be judged from the log, which does not record INTENDED direction.
If right-hand starts still read wrong in play, the deterministic fix is below — but measure first.

### Held in reserve — deterministic handedness (only if play shows it is still wrong)
Split `Walk_Starts` / `Run_Starts` into `_L` / `_R` databases; latch the side at start onset (same latch
pattern as `UpdateSelectionGait`'s stop band, already built and verified); gate on `StartDirections`
(the axis is already in `FAZ_DatabaseGate`, inert). ~2h + row authoring + a reindex.
NOT mirroring: `AZ_Hero_MDT` has 261 BONE rows and **zero curve rows**, so a mirrored clip reports
`FootSpeed_L` from the original left foot and `bLeftFootDown` inverts — which feeds `MovementDirection`
and the stop/pivot variant choice. Mirroring also only makes handedness *probabilistic* (it removes the
445-vs-214 deg/s bias so the ~10 deg signal can decide) and is a coin flip at exactly 180.
Mirroring is still worth doing LATER for density: 4→8 pivots, run-turn ceiling 162→202 deg/s.

## 2. The two architectures, precisely

**v2 / Mover (`AZ_MoverAnimInstance.cpp:801`)** — chooser decides, MM refines:
- chooser addressed by `(SMState, Stance, Gait, MovementDirection, bLeftFootDown, Reaction)` returns ONE clip
- `MotionMatch(AssetsToSearch = {ChosenAnim})` — a one-element set; MM picks only the entry FRAME
- edge-driven push (`bSelectionChanged` vs `LastPushed*`, plus `TransitionSerial` so one transition pushes once)
- `MMCostLimit` — if the best cost exceeds the row's limit, KEEP the current anim
- two pose-identical clips can never compete; the row decides which one

**CMC / GASP (current)** — MM decides, gates prune:
- `Get_DatabasesToSearch()` returns a UNION of pools matched on `(MovementMode, Stance, MovementState, Gait)`
- MM searches the whole union EVERY FRAME and picks both clip and frame
- no edge gate, no cost limit; cheapest candidate wins, whatever it is

## 3. The design — pool narrowing, NOT graph surgery

Key realisation: we already own `Get_DatabasesToSearch()`. **Restricting the pool to one event's
database IS "the chooser picks the clip"**, expressed through the existing seam. No BlendStack
rewiring, no replacing the MM node, no new AnimGraph nodes.

- `EPoseSearchInterruptMode` is NOT the commitment lever — it only governs whether the *continuing
  pose* is considered (`PoseSearchLibrary.h:35-53`). It cannot stop a fresh re-pick.
- Commitment = the pool contains only what we are willing to play.

Split of responsibility:
- **Discrete events** (starts, stops, pivots, stance transitions, turn-in-place) → chosen by STATE.
  Pools of 2–7 clips; MM clip-selection adds no quality there, only the chance to pick wrong.
- **Loops** (straight + arc) → stay on MM. Continuous trajectory cost genuinely earns its keep:
  blending straight↔arc by predicted curvature is exactly what MM is good at, and it works today.

## 4. Stages  (REVISED 2026-08-22 after Fable review — see §4a for what was wrong)

### Stage A — REUSE `UAZ_LocomotionStateMachine`, do NOT write a new state owner
It already exists (`AZ_LocomotionStateMachine.h:105`), is live on v2, and is a PURE decision class:
`Tick(FAZ_LocoSMInputs) -> FAZ_LocoSMOutputs`, zero Mover API surface — inputs are a plain struct
fillable from the CMC contract. It already owns everything Stage A was going to invent:
- `PendingStartAngleDeg` + `BucketStartDirection()` → the latched turn-side (`AZ_LocomotionTypes.h:111`
  documents the latch on the transition-entry edge)
- `NotifyTransitionClipPushed(WorldNow, ClipRemainingSeconds, AlmostCompleteThreshold)` +
  `TransitionEndTime` → commitment keyed to the clip's REAL remaining length, not a magic number
- `NotifyIdleBreakClipPushed`, `NotifyReactionClipPushed`, `GetSettledStance`, `IsReactionActive`

SEAM: v2's `bIsMoving` input is INTENT-based; CMC's `IsMoving()` is deliberately VELOCITY-based
(the stop-reachability fix). Feed the SM the acceleration-based flag and the gates the velocity-based
one. Seam-trace this with real numbers before PIE.

### Stage B — positive/exclusive pool selection
`Get_DatabasesToSearch()` returns the matching row's pool only (`bExclusive`), retiring the three
subtractive gates. Gate rows gain `States[]` (SM state), `Sides[]`, and `RotationModes[]` — the last
is already recorded as needed for the combat strafe slice, so it goes in the SAME build or costs a
second editor-closed cycle.

### Stage C — commitment via `SearchThrottleTime`, NOT via a pool hold
`FAnimNode_MotionMatching::SearchThrottleTime` (`AnimNode_MotionMatching.h:133-135`, `PinHiddenByDefault`
so bindable). Verified in `.cpp:189-197`: inside the window, if the continuing pose can advance,
**`bSearch` is false — no search runs at all**. Churn becomes impossible rather than cost-suppressed.
Drive the window from `NotifyTransitionClipPushed`'s remaining-length.
TWO VERIFIED TRAPS:
1. Inside the window a POOL CHANGE does not trigger a search — the interrupt mode is only consulted
   inside `if (bSearch)`. The getter MUST return 0 on the state-exit frame or the new pool is ignored.
2. A non-looping clip reaching its end flips `bCanAdvance` false → search fires. The commit therefore
   self-caps at clip length for free.
On state-change frames prefer `InterruptOnDatabaseChangeAndInvalidateContinuingPose`; plain
`InterruptOnDatabaseChange` (current, `AZ_CmcAnimInstance.cpp:873`) is inert inside a throttle window.

### Stage D — DROPPED. `MaxAcceptableCost` is unimplementable on the MM-node path
v2's `MMCostLimit` works because the code owns the push and can decline it
(`AZ_MoverAnimInstance.cpp:886-890`). The MM node commits internally before
`Update_MotionMatching_PostSelection` reads the result — there is no veto hook. Do not add the field.

## 4a. What the first draft of this plan got WRONG (keep, so it is not re-derived a fourth time)
1. **`CommitSeconds` as a pool hold does not fix intra-family churn.** The node re-searches inside the
   held pool every frame. Evidence #3 disproves it directly: four variants in ONE database cycled at
   24 ms while the pool never changed. Per-DB `ContinuingPoseCostBias` defaults to −0.01
   (`PoseSearchDatabase.h:516`); our −1.0 notify is 100× that and STILL only held `R_LU` for 611 ms of
   a 1433 ms clip (43%). Bias is not commitment.
2. **The locality argument does not discriminate A from B.** v2's chooser can return candidate ARRAYS
   and already does (`AZ_MoverAnimInstance.cpp:806-812`, plus the strafe/per-gait whole-DB cases at
   :815-855). "84 rows addressing clips" is an authoring decomposition, not a substrate property.
   What actually separates them is the ACTUATOR: B pushes on an edge with a transition lock
   (`AZ_MoverAnimInstance.cpp:764`); A re-searches every frame. `SearchThrottleTime` is what closes
   that gap — it is the mechanism the first draft missed.
3. **Stage A was a third re-derivation.** `UAZ_LocomotionStateMachine` already existed.
4. Authoring cost is CONSERVED, not saved: the table stops growing with content, but the work moves
   into PSD membership — which has the worst edit loop in the project (restart before judging, PSN
   membership re-keys the DDC for all members). "Table stops growing" is true; "cheaper to scale
   overall" is NOT proven.

## 5. Keep / retire

KEEP: MM node + BlendStack as-is; `DatabaseGates` data table (repurposed to state→pool);
`OverrideContinuingPoseCostBias −1.0` on turning clips (GASP convention, still correct);
Steering + OffsetRootBone chain; arc-loop content; the `[CmcSel]` change-triggered log.

RETIRE at Stage B: the three subtractive gates in `Get_DatabasesToSearch`; the `Tags`-based removal
idiom (`Starts`/`Stops`/`StanceTrans`).

## 6. Risks / failure axes

1. **Committed duration too long** → unresponsive; too short → churn returns. Instrument with the
   existing `[CmcSel]` survival measurement; target = clip length for one-shots.
2. **Pool splitting multiplies databases** → each must join `PSN_AZ_CMC` (normalization set); every
   membership edit re-keys the DDC for ALL members. Expect slow first loads; see the RESTART GOTCHA
   in [[feedback_posesearch_branchin_db_sync]].
3. **A clip in two searched pools is the twin bug.** Enforce: every clip in exactly ONE pool per
   gait row. This caused three separate failures today.
4. **State machine becomes a second source of truth** about movement. It must READ `MovementState`/
   `Gait`/`Stance`, never re-derive them.
5. Latched handedness can latch wrong on a fast double-reversal. Accept for v1; exit on state exit.

## 7. Verification

- `[CmcSel]` median survival per discrete clip ≥ 80% of clip length (today: 168 ms / 1533 ms = 11%).
- Zero selection changes < 200 ms apart within one event.
- Right-hand turns select `_R` clips ≥ 90% of the time (today: ~0%).
- Costs committed to stay under the Stage-D limit.
- ★ Seam-trace with real numbers before every PIE ([[feedback_seam_trace_before_pie]]).

## 7a. ★★ 2026-08-23 — THE ENGINE FACT that shapes the whole plan, plus measured entry/exit

**`BlockTransition` CANNOT touch the continuing pose.** It is applied through `FSearchFilters`, built ONLY
in the candidate-search paths (`PoseSearchDatabase.cpp:2371 / 2490 / 2693`). `SearchContinuingPose`
(`:1874`) never builds them — a continuing pose is exempt from EVERY filter. So tail-blocking clips can
stop MM *jumping into* a late frame but can never stop a spent clip being inherited.

Two distinct sub-problems, needing different levers:
| case | signature | lever |
|---|---|---|
| spent clip inherited as continuing pose | `clipTime` 0.5-0.99 on rapid re-stops | interrupt mode |
| fresh search picks a late frame | `clipTime` 0.10-0.33, values REPEAT exactly | BlockTransition |

**SHIPPED:** `Get_MMInterruptMode` returns `InterruptOnDatabaseChangeAndInvalidateContinuingPose` on the
`bStopActive` false->true EDGE (one frame only — holding it re-searches every frame and brings back churn).
Result: max `clipTime` 0.99 -> 0.30, and stops still get selected (the feared "loop wins instead" did not
happen). The remaining 0.10-0.33 entries REPEAT EXACTLY, which proves they are genuine foot-phase matches,
not leftovers — MM enters where the clip's foot phase matches the loop's, which with 2 variants per gait
can be a quarter-cycle off.

### ★ MM PLAYS POSES, NOT CLIPS — the measured cost
Entry `clipTime` 0.10-0.33 and survival 830-1018ms against 1.267-1.533s clips = **we play the middle
55-70% of every stop**. Turns are worse (168ms median vs a 1.533s clip = 11%, 611ms after fixes = 43%).
Starts were 16-48%, now 55-65% after `MaxAccelerationBase` was lowered.
This is not a bug — it is Motion Matching working as designed, and it is invisible in GASP because density
means some clip's BEGINNING always matches. **This is the single strongest argument for the push path:**
"play this clip whole" is native to a chooser+BlendStack actuator and only achievable under protest on the
MM node (BlockTransition to force entry + SearchThrottleTime to prevent exit).

The distance consequence is closed and quantified in [[project_cmc_movement_feel_tuning]]: capsule travel
lost equals the clip fraction skipped. No movement-side model can recover it.

## 7b. ★★★ `MaxWalkSpeed` IS AN INTENT SIGNAL TO THE SEARCH — do not let animation own it

Verified 2026-08-23 (three independent reviews agreed; engine source checked):
`PoseSearchTrajectoryLibrary.cpp:73` sets `TrajectoryDataDerived.MaxSpeed = Max(GetMaxSpeed() *
AnalogInputModifier, GetMinAnalogSpeed())`, and `:189` clamps EVERY predicted step:
`OutVelocity = OutVelocity.GetClampedToMaxSize(TrajectoryDataDerived.MaxSpeed)`.

`MaxSpeed` there is not "current speed" — it is the HEADROOM the prediction may grow into. The predictor
is seeded from actual velocity and integrates acceleration forward; this clamp is the only thing bounding
it. So `SetGait -> MaxWalkSpeed` is how INTENT reaches the search: write 585 and the predictor simulates
the ramp, and MM selects sprint content BEFORE the body is fast. That anticipation is a central benefit of
Motion Matching and it is bought entirely with this one write.

★ **Therefore: never write an animation-derived speed into anything `GetMaxSpeed()` returns.** Doing so
sets headroom to zero and reverses the information flow:
```
correct:   intent -> trajectory -> clip -> realized motion
inverted:  clip -> speed ceiling -> trajectory -> same clip
```
From a 172 cm/s walk loop a 375 future becomes unrepresentable, so run clips carry permanently bad
trajectory cost. (Precisely: unreachable *through trajectory cost*. A commanded-gait pool swap could still
surface a run clip — but it would receive a walk-speed future and score its entry frame against the wrong
acceleration profile. Workaround, not fix.)

**Why curve-driven STOPS are the legitimate exception:** a stop is a COMMITTED TERMINAL EVENT. After the
stick is released there is no higher-speed future intent for the selected clip to make unreachable, so
there is no anticipation left to destroy. The stop system also writes `BrakingDecelerationWalking`, which
the predictor reads via `GetMaxBrakingDeceleration()` — so prediction MATCHES the clip instead of being
capped by it. That is why it works and why it does not generalise.

If animation-owned locomotion speed is ever wanted, it needs TWO channels — `GetMaxSpeed()` stays
gait-owned for the query, and a private movement target drives the capsule in a custom CMC ground path.
That does NOT require forking PoseSearch. See [[project_cmc_movement_feel_tuning]] for the measured content.

Related trap: a graph-BLENDED movement-authority curve needs schema-complete coverage across every pose
contributor that can carry non-zero weight (idle, stance transitions, landing, mirrored, additive,
montages) — the blend path treats a missing curve as 0, not as absent (`LerpToValid` exists precisely
because plain `LerpTo` does not preserve the valid side). Per-clip `EvaluateCurveData` sampling (play rate,
stops) does not have this problem.

## 8. Measured content reference (2026-08-22, root-motion measured — do NOT trust filenames)

Handedness verified by mid-clip yaw; `±180` endpoints are wrap-ambiguous and mislead.

| clip | len | yaw | °/s | travel | note |
|---|---|---|---|---|---|
| `AnimPro_WalkFwdStart180_L` | 0.900 | −180 L | 445 | 87 | |
| `AnimPro_WalkFwdStart180_R` | 1.400 | +180 R | 214 | 119 | 1.55× slower → `_L` always wins |
| `AnimPro_WalkArchLoop_L/R` | 1.000 | ∓180 | 180 | 74 / 83 | loop, `Disable_AdditiveLeans`=1.0 |
| `AnimPro_WalkFwdLoop` | 1.000 | 0 | 0 | 173 | no rotation → cannot unwind offset |
| `AnimPro_RunArchLoop_L` | 0.767 | −71 L | **92.7** | 297 | binding constraint on run turn rate |
| `AnimPro_RunArchLoop_R` | 0.767 | +88 R | 115.4 | 263 | |
| `AnimPro_RunFwdStart180_L/R` | 0.867 / 1.133 | L / R | 207 / 159 | 155 / 213 | same `_L` bias |
| `AnimPro_RunFwdTurn180_L_LU/_L_RU` | 0.967 / 1.067 | L | 186 / 169 | 13 / 0 | near-stationary |
| `AnimPro_RunFwdTurn180_R_LU/_R_RU` | 1.433 / 1.533 | R | 126 / 117 | 82 / 38 | |

Live tuning that depends on the above:
- `RunRotationRateYaw = 160`, ceiling = `92.7 × MaxScaleRatio 1.75 = 162` — 2°/s of headroom, on the
  LEFT arc only. Mirroring `RunArchLoop_R` to replace `_L` would raise it to 202.
- `GroundedRotationRateYaw = −1` (walk, instant), `SprintRotationRateYaw = 90`, `bGaitScaledRotationRate = true`.
- `OffsetRootBone`: `MaxRotationError = −1` (GASP parity; was 90 and clamping every 180 start),
  `RotationMode = Accumulate` while no montage — so **only animation root rotation moves the mesh**.
- MM node `BlendProfile` repointed `SK_UEFN_Mannequin` → `SKEL_SurvivalMan : FastFeet+Root_Weight`.
- Additive lean is wired to the **Sprint pin only** of `Blend Poses (EAZ_Gait)`; `BS_AZ_Sprint_Leans`
  samples are correct `AAT_ROTATION_OFFSET_MESH_SPACE` additives, so one wire to the Run pin enables
  run banking. Arc clips carry `Disable_AdditiveLeans = 1.0` (flat) and bank from baked animation.

## 9. Mirroring — do it, but it is NOT a substitute for the state latch
`MirrorOption` is per-entry config (`PoseSearchDatabase.h:112`, default `UnmirroredOnly`) and the
schema already carries `AZ_Hero_MDT` for `SKEL_SurvivalMan`, so it is config, not authoring.
Worth doing regardless of A/B: doubles pivot density (4→8), equalizes the L/R dynamics asymmetry, and
raises the run-turn ceiling from 162 to ~202 deg/s.
**It does NOT make cost-based handedness work at 180 deg.** Walk rotation is instant
(`GroundedRotationRateYaw = -1`), so the predicted facing snaps and the intermediate trajectory samples
carry no turn-direction signal at a reversal. With exact mirrors the measured "9 of 10 pick `_L`"
becomes a 50/50 coin flip at 180 — correct only at 90/135 where the position channel curves.
The latched side from the SM stays necessary.
GOTCHA: `AZ_Hero_MDT` needs `FootSpeed_L` ↔ `FootSpeed_R` CURVE rows, or `bLeftFootDown` inverts on
mirrored plays. Mirrored entries also re-index the DBs (DDC).

## 10. Exit criterion — when to abandon this and move to v2 CHT+SM
B is the only option with MEASURED stability; everything load-bearing here was verified by reading
engine source, not by running it, and this project ships about one bug per novel seam
([[feedback_seam_trace_before_pie]]). If, after the seam-trace and ONE honest tuning pass, §7's bar is
missed (median survival ≥ 80% of clip length; ≥ 90% correct-side turns), move to B. Sunk cost is small:
the gate axes and the SM shim are reusable there, because B needs the same state owner and the same
side latch — which is also the proof that the two options were never far apart.
