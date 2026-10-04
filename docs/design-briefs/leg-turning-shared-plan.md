# Leg turning: shared investigation and implementation plan

Status: **active, not fully fixed or validated; user testing deferred until tomorrow**. Updated 2026-10-04.
Workspace: `C:/UnrealEngine/Games/AZ`.

## Scope and current state

The user asked to solve these two remaining problems together:

1. Standing with a firearm in aim: the body rotates but small turn-step animations often do not play.
2. Walking while turning: legs cross, including relaxed unarmed locomotion and armed movement.

A prior, distinct standing-aim crossover was caused by retained world-space foot pins. The user confirmed
that crossover stopped after the selective pin-release change was built and connected to the owned rig.
That fix does not create stepping animation and does not establish that moving foot placement is correct.

| Work | Current status |
|---|---|
| Selective stationary foot-pin release | Previously built, rig/AnimBP connected and saved; user confirmed stationary crossover removed |
| Neutral support-relative visual-motion sample | Implemented, reviewed and corrected; subsequently included in Claude's reported H4 build; joint gameplay validation pending |
| Applied-turn step demand and playback completion | Reviewed v3 integrated; another agent handled the build, new parameters loaded and AnimBP compiled/saved; user tentatively reports visible steps, full acceptance pending |
| Walking-turn source/history audit | Complete; causality still needs live pose/phase evidence |
| One-shot foot-pose diagnostic | Executed in user-run PIE; six successful moving samples show pinning off and matching input/output XY; the recurring bad state was not captured |

## Latest user-run checkpoint — October 4

The user initially saw no stationary steps, then reported that they seemed to be playing. Left and right W2
turn clips were selected in the recorded run. Keep this as preliminary visual progress, not complete acceptance.
The user also initially reported normal moving turns, then corrected that report: **leg crossover returned**.
Their latest clarification explicitly removes a reliable jump association: it also reappears during ordinary
movement. Neither walking crossover nor a jump-specific cause is established as fixed/proven.

Evidence is now in `Saved/Logs/AZ-backup-2026.10.04-04.41.53.log` after the editor restarted:

- Six successful moving snapshots at lines 13912–13939, 04:33:38–04:33:40 UTC, show both pin flags false,
  both weights zero, matching native/handled release serials, and matching XY between animated toe targets,
  rig output and published mesh. These are good-state samples, not evidence of the later visible crossover.
- Later snapshot attempts at lines 16567 and 18153–18161 found no PIE player; they do not describe bad-state
  foot positions. Multiple separate PIE lifetimes prevent a causal normal-to-bad comparison across those runs.
- The three detailed debug CVars were observed at zero during the recent runs. Existing clip-selection logs
  therefore do not establish pin lifecycle or actual step-player progress in the failing interval.
- The refreshed game DLL and live step parameters were observed, and AnimBP compile/save was logged.
  This is loaded-runtime evidence; do not invent a compiler result from an unrelated/stale build log.

The user will continue testing tomorrow. Before requesting another run, execute the reviewed existing helper
`Tools/diagnostics/set_foot_turn_logging.py` and verify readback of `az.Aim.Debug=1`, `az.Cam.Debug=1`,
`az.TipRate.Debug=2`. The helper changes only existing log CVars; it does not start PIE or alter assets/poses.
It has passed static review but has not yet been run in Unreal. No further gameplay changes are justified by
the current evidence.

Next bounded check: one user-started PIE session, normal moving turns through the first visible bad state,
with pose snapshots during normal and bad behavior. No jump is required. Compare input pose, final pose,
pin flags/weights/targets, reset/suppression reasons and selected/applied phase/mirroring. This separates
upstream selection/blending from foot constraints before choosing a walking-specific fix. Disable the three
log channels after collection. Preserve the hand/weapon implementation and existing terrain adjustment.

The user specifically suspects continuous circular walking, rather than only alternating left/right movement.
Include both circle directions in that same-session comparison. Source inspection confirms the existing
stationary-turn detector clears its pin-suppression flag once actual movement exceeds the stationary band
(`AZ_MoverAnimInstance_Procedural.cpp`, `UpdateProceduralFootTurnState`). Moving foot pinning is then allowed
when the other eligibility/contact-curve gates pass; actual per-foot weights remain the rig's responsibility.
Terrain IK remaining enabled is intentional and does not by itself prove incorrect pin retention. The new
post-restart journal contains graph inspection but no detailed foot trace or bad-state snapshot establishing
the cause. Do not fix circular walking by globally disabling terrain IK without discriminating evidence.

## Next-session execution plan

This is the resumption plan requested at the end of today's session. No additional source/asset changes,
logging changes or editor tests are scheduled for tonight. Resume from the existing evidence above rather
than repeating the whole investigation. The circular-walking hypothesis is supported by the scope of the
stationary-only guard in source, not by a recorded failing frame.

### 1. Prepare one useful recording

- **Owner:** Astra coordinates and runs reviewed existing helpers. Reuse the existing Sol executor for any
  bounded script correction; Medium for such a correction, with a shown specification and review first.
- **Inputs:** this checkpoint, current source state, actual loaded AZ build, the two reviewed diagnostic
  helpers, and the existing log archive. Check for concurrent writes before changing shared files later.
- **Actions:** when the user has opened the project, enable the three existing debug channels through
  `set_foot_turn_logging.py`; independently verify their readback. Do not assume they survived an editor
  restart. Record the active log filename and the single PIE session boundaries.
- **Acceptance:** verified logging values and an existing user-started PIE session before pose collection.
  Do not request another uninstrumented run. Do not start PIE, rebuild, or operate the GUI automatically.

### 2. Capture ordinary movement and the first recurrence

- **Owner:** user controls gameplay; Astra runs the reviewed pose snapshot helper and tracks the session.
- **Sequence:** relaxed unarmed walking straight briefly, then continuous circles left and right, including
  direction changes and a stop. Capture a short good-state series and a series while the visible crossover
  is present. Keep the same PIE session alive until the required reads finish; no jump is needed to trigger it.
- **Scope:** add the armed comparison after obtaining the decisive unarmed data, or earlier if the defect
  appears only armed. Confirm visible stationary aim steps separately; clip selection alone is insufficient.
- **Result:** identifiable normal and bad windows with input targets, solved/published pose, contacts,
  per-foot pin flags/weights, reset/release state, actual support-relative motion and clip/player context.
  If the defect does not occur, record that limited outcome rather than declaring it fixed.

### 3. Determine where the pose becomes wrong — review checkpoint

- **Owners:** existing Sol context handles narrow extraction at Low; new mechanical extraction may use
  Luna High. Astra owns the causal decision. Escalate uncertainty before acting on it.
- **Compare:** current animated targets versus rig output versus final mesh, with both thighs/calves/feet,
  body orientation, contacts, retained targets and pin weights. A left/right coordinate ordering by itself
  does not prove anatomical crossing. Respect the snapshot's lack of a common evaluation serial.
- **Decision A:** if the input pose is acceptable but the constrained result crosses while an old pin retains
  weight, design a bounded moving-foot release/recapture correction that preserves useful planted contact.
- **Decision B:** if the input is already wrong with pin weights zero, inspect only the implicated selection,
  blend/entry phase, direction/mirroring or transition. Do not rewrite Chooser rows from clip names alone.
- **Decision C:** if the change appears after the rig, inspect that final processing stage. If freshness or
  timing remains ambiguous, identify the exact missing observation instead of selecting a speculative fix.
- **Gate:** Astra accepts evidence locating the fault before dependent implementation. Independent source
  reads may proceed, but no broad pin-disable workaround, curve rewrite or new full-project audit is assumed.

### 4. Show and implement the smallest supported correction

- **Owner:** Astra updates and presents the concrete implementation plan, including affected files/assets,
  protected behavior and acceptance checks. Sol Medium implements a bounded simple correction; Sol High
  handles complex animation state or pin lifecycle changes. Reuse the existing suitable executor.
- **Checkpoint:** review release timing, fresh target recapture, contact lifetimes and transition/reset cases
  before integration. Sol returns evidence even without doubts and escalates every uncertain decision.
- **Protection:** one writer for shared animation files and one owner of editor asset writes. Preserve Claude's
  hand/weapon changes, approved M16 grip, terrain IK, pelvis smoothing and the accepted stationary release.
  Do not conflate terrain IK with world-space foot pinning.

### 5. Build and verify only the resulting change

- **Owner:** existing-context Sol Low collects the appropriate build result against a fixed source state;
  Astra reviews integration. Coordinate with any agent already building instead of launching a duplicate.
- **User validation:** repeat the same circular-walking case in both directions, then armed/unarmed movement,
  stop/start and reversal. Verify stationary aimed steps remain visible and old stationary crossover stays
  absent. Landing/jump, stance changes and uneven terrain are regression checks, not presumed causes.
- **Acceptance:** no visible crossover in the tested cases, no persistent stale pin pulling a foot across the
  body, useful terrain adaptation preserved, and no regression to hand/weapon behavior. State the actual
  coverage and remaining limits; finite successful runs do not prove correctness in all possible movements.
- **Finish:** disable only the diagnostic channels enabled for the run, record the log/evidence and update
  this plan and the linked technical-debt entry. Add or run no automated tests without an explicit request.

## Established evidence

### Standing turn

The useful rifle run in `Saved/Logs/AZ.log` spans 01:24:44-01:25:57 UTC on October 4
(the evening of October 3 locally). The following PIE session was primarily debug-command cleanup.
The profile asset name `DA_WeaponAnim_P01` does **not** imply a pistol was equipped; the user used a rifle.

- During sustained turns, positive log records show SM idle plus W2 aim-idle, even with body rotation around
  200-400 degrees/second. Camera-to-body residual remains below the configured 45-degree TIP entry gate.
- A sufficiently large residual did enter left TIP: the actual W2 left IPC turn loop was selected and played.
  This confirms a gate problem, not absent animation assets. Continuous playback through that entire episode
  was not established by the old entry-only playback log.
- The current turn assets were sampled and contain foot translation/lift. The current sampled right-contact
  curve is zero in both inspected turn clips; preserve this observation for validation, not a blind curve rewrite.
- In decisive idle-while-turning records, native pinning was already disabled and turn suppression active.
  Missing step selection in those intervals therefore precedes foot-rig correction.

### Walking turn

[The September handoff](strafe-leg-crossing-handoff.md) preserves measured selection churn and entry phases.
Its section 3.2 retracts the inference that an opposite-looking clip name proves wrong direction. Direction
hysteresis did not fix the problem; stronger continuing bias worsened the user's result and was reverted.
Neither approach is a justified default fix now.

Current source audit establishes:

- Global `bIsMoving` is intent/prediction based and owns normal start/stop transitions. Do not replace it with
  the actual support-relative speed needed by foot planting and the new visual-motion sample.
- Explore and strafe resolve direction in different frames and follow different pivot/state rules.
- Moving loop-to-loop entry remains Motion Matching owned. Existing phase locking applies to eligible
  transition-to-loop seams, not every moving clip change.
- A present weapon profile with null database properties retains the chooser-selected pool/clip; it does not
  fall back to the AnimBP's unarmed databases. Live inspection confirmed all five rifle-profile DB properties
  are null, while the MHC AnimBP has unarmed walking/strafe databases assigned.
- `PSD_v2_WalkLoco` and `PSD_v2_StrafeWalk` use `PSS_v2_SurvivalMan_Loco`, sample rate 30, with
  `AZ_Hero_MDT` assigned. Their continuing/loop biases read -0.01/-0.005. This is configuration evidence,
  not proof that a mirrored sample was selected in a failing frame.
- Source publication appears to omit incoming PoseSearch mirroring. Its runtime effect remains a hypothesis:
  private `DatabaseAnimationAssets` could not be read through the available object getter, and no existing
  read-only AZ index getter was found. Actual selected/applied mirror telemetry is the next bounded check.

A wrong pose can originate in selection/blending, foot constraints or their interaction across frames.
Rapid selections alone do not prove the cause of crossover.

## Shared implementation and review gates

### P1 — Neutral physical observation (source review accepted)

Measure the actual visual mesh before the locomotion SM: support-relative signed heading change and planar
displacement/speed, with valid/reseed reasons and raw/observed elapsed time. This measurement must not depend
on IK enable, LOD, contact curves, aim state or private pin suppression.

Pure affine/projection/angle/speed helpers are shared, but consumer histories intentionally remain separate.
The proven pin consumer retains its own reset/relevance, zero-delta and serial lifetimes. Neutral history holds
the full selected mesh axis, carries it by support rotation and only then projects it onto current Mover Up.
A known support with no valid transform yields an unqualified sample; recovery seeds without a motion event.
The Up guard compares actual gravity/measurement Up, not a support-rotated Up vector.

Independent neutral guards start at 0.25 seconds and 80 cm. These are conservative continuity guards, not
measured animation-demand thresholds: legitimate fast/large vertical motion can reseed, and rotation-only
teleports need a separate skip/identity/continuity signal. Unqualified/zero time does not advance motion or quiet timers.

Before-state and reviewed diff:
`C:/UnrealEngine/Games/AZ_Backups/2026-10-03_TIP_Phase1_Before_4ef36581/manifest.json`
and `Phase1-reviewed-v2.patch`. Narrow comparisons preserved the prior pin, hand and weapon code.
Source review and whitespace checks are not a successful compile or gameplay test.

### P2/P3 — Standing firearm step demand and completion (in progress)

- Add an explicit opt-in SM input, default false for legacy/CMC consumers. Scope the new path to stationary
  Mover firearm aim; preserve throwable behavior and existing higher-priority movement/action transitions.
- Initial independent animation-demand tuning: stationary enter/exit 2/5 cm/s, accumulated own-yaw entry
  5 degrees, meaningful opposite rotation 5 degrees, quiet window 0.15 seconds with range at most 0.25 degrees.
  These are starting design values for user validation, not measured noise bounds or foot-pin settings.
- Slow motion accumulates; quiet-window updates must not erase its long-term angular evidence. Continuous
  same-direction turns do not restart the clip each frame. Meaningful reversal waits for a step boundary.
- Base step duration on the confirmed playing clip and its effective clock. Convert the configured minimum
  to whole cycles with a declared 0.005-second nominal tolerance: 0.67 seconds against a 0.66667-second clip
  means one full cycle; against a 0.5-second clip it means two. Never truncate a real cycle to use the tolerance.
- Use the actual inner asset player's delta record, phase, unbounded main-player age, pose-link identity,
  committed request epoch and update freshness. The outer BlendStack's inherited delta record is not assumed
  valid merely because its accumulated-time getter delegates to the inner player.
- Worker-to-game-thread feedback requires a verified animation-task fence. Observation before node update
  refers to the preceding completed update, not a newly requested clip. Reset, stale, ambiguous or skipped
  feedback earns no cycle credit. After 0.25 seconds of qualified missing feedback, leave cosmetic TIP with
  an observable reason and block immediate repeated entry until the defined recovery condition.
- Review corrections: retain an independent observed inner-clock tuple across cursor invalidation; advancing
  outer-player age alone cannot prove inner progress. Identical positive tuples receive no credit or timeout
  reset. Exact whole-period aliasing without an observable update identity is explicitly ambiguous and uses
  bounded fallback. Count boundaries in `(previous, previous + delta]`: arriving at exact clip end counts once,
  departing that same endpoint does not count again. Every newly trusted cursor, including recovery, seeds
  without cycle credit; an endpoint seed opens accounting only for subsequent intervals.
- Preserve gameplay aim responsiveness, the legacy Pawn facing/pacing latch and authored weapon playback
  speed. Visible steps are not a promise of zero sliding at every capsule turn speed on the current content.

**Checkpoint passed:** lead integration review and an independent focused playback review accepted v3 after
the stale-inner-clock, exact-end double-count and recovery-seed cases were corrected. Review was static;
there was no automated test or PIE run at that review checkpoint. Subsequent user-run evidence is recorded
above; gameplay acceptance remains pending.

### P4 — Joint observation, then the walking-specific fix

The first appropriate build and user-run pass must cover both branches. Reuse existing aim/camera/selection
logs; debug level 2 adds actual step progression and selected/applied mirror evidence without changing MM.
`Tools/diagnostics/capture_foot_pose.py` provides one read-only snapshot in an existing user-started PIE session:
native and rig contacts, pin flags/weights/serials, parent-checked saved animated targets, rig outputs and final
mesh component-space bones. It installs no callback and requests no animation evaluation. Existing work is
joined before reads, but the output explicitly does not prove identical rig/postprocess evaluation serials.

Compare evidence around the first visible crossover before changing walking selection or plant policy.
Preserve contact-based walking support; do not extend stationary blanket pin suppression to all moving turns.
Review and show any materially different fix required by the walking evidence before implementation.

### P5 — Acceptance

User-run scenarios: stationary firearm aim at slow/fast turn rates, reversal and stop; relaxed unarmed and
armed walking while turning; walking-to-stop and stop-to-walking; crouch/stance and weapon transitions.
Check the approved grasp/right arm and terrain foot adjustment where shared code was touched.
Both leg defects remain open until the relevant visual result and build are verified. No automated tests or
automatic PIE launch are authorized by this plan.

## Ownership and integration history

The write-hold and build-wait descriptions below are historical. Ownership was released, integration applied,
and another agent subsequently handled the build. The latest user-run checkpoint above takes precedence.

The user reported that an **external Claude agent was changing the hand/weapon implementation**.
The leg executor therefore stopped all writes to the shared project's source. No live changes were reverted.
The shared tree contains accepted-but-unbuilt P1 and partial P2 demand/SM code; new step inputs remain
default-false and unwired. Static inspection found no obvious new dangling calls, but this is not build evidence.

Leg implementation continues only in the five-file, source-only copy at:
`C:/UnrealEngine/Games/AZ_Backups/2026-10-03_TIP_SourceScratch_6c0f59db/Source/AZ/`.
Its `baseline/Source/AZ/` is the accepted P1 starting point; `isolation-manifest.json` records matching
held/live-before/live-after hashes for the copied files. The lead independently checked all copied and baseline
hashes. This is not a full Unreal checkout and must not be presented as a buildable project.

The isolated scope is `AZ_MoverAnimInstance.h/.cpp`, `_Procedural.cpp` and the locomotion SM pair. Preserve
the write-hold snapshot and all baseline/own-patch evidence. Do not copy whole isolated files back over Claude's
work. Once shared-file ownership is released, review a three-way integration of the baseline, leg changes and
current shared hand changes; then build the fixed integrated state. No shared build or editor restart is being
launched by the leg task during this hold.

**Ownership release received:** the user subsequently said Claude's work was finished and supplied
[the H1/H2 implementation checkpoint](left-hand-weapon-switch-h1-h2-checkpoint.md). The leg task may prepare
integration after its isolated-code review. Preserve the new `AZ_MoverAnimInstance_WeaponReach.cpp`, the exact
equipment presentation API, the changed reach struct/cache and per-hand ownership. Do not restore old reach
blocks, removed properties or the old console-variable definition from the isolated baseline. The checkpoint
reports a full H4 build of the then-current shared tree; it does not cover the later isolated turn-step feature.

**Integration applied:** after the v3 review, the leg executor applied only the narrow delta to the three
animation-instance files. The SM pair already matched the reviewed result and was not rewritten. Eight
protected hand/equipment/weapon file hashes remained unchanged. The three applied files match the reviewed
text exactly after CRLF-to-LF normalization; only Git's Windows line-ending conversion changed their raw
hashes. Actual source bytes were preserved. Receipt:
`C:/UnrealEngine/Games/AZ_Backups/2026-10-03_TIP_Apply_v3_9517c405/apply-verified-receipt.json`
(the initial receipt is preserved separately).
At integration time the source was frozen for a full build and the editor was still open. The user later
assigned the build to another agent and reopened the editor; the latest checkpoint records subsequent
runtime observations without claiming full gameplay acceptance.

Research/diagnostic tasks do not edit these C++ files. Coordinate the separate
[left-hand handoff](left-hand-weapon-switch-handoff.md) before resuming shared-source writes. Preserve source
animation assets, Natural Grip outputs, existing user edits and already connected rig pins.
