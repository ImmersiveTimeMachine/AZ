# CHALK — grenade stationary turning and moving Fight foot continuity

**Execution owner: Claude. Review: Codex, September 18, 2026 local time.**

This is a read-only investigation and implementation work order. Codex did not change animation code, graphs, clips, databases, retargeting, input or movement settings for this review, and did not start PIE or run tests. Separate compass work has created four isolated HUD widget copies; do not include those in this fix.

## The actual user request

Resolve both visible problems carefully, in this order:

1. **Stationary grenade:** equip/ready a grenade, stay still, rotate the camera. Legs cross or remain planted while the body rotates. Inspect preparation and the sustained ready loop, not just release.
2. **Moving Fight:** while walking/running/crouching in combat stance, change travel direction and rotate the camera. Feet cross/slide and directional animation handoffs lack continuity.

These are separate reproduction cases. The earlier [strafe handoff](C:/UnrealEngine/Games/AZ/docs/design-briefs/strafe-leg-crossing-handoff.md) explicitly excluded stationary turning. It is useful historical evidence, not a complete or fully reliable diagnosis of this request.

Deliver a coherent movement/animation result with measured evidence. Preserve existing controls, grenade preparation/release/cancel behavior, aiming, inventory, traversal, procedural terrain adaptation and the established **1× player clip playback** policy. No global retarget/scale changes, generic animation-rate compensation, or unrelated throwable rewrite.

## Evidence and corrected assumptions

### A. Stationary case: confirmed configuration and control problems

The loaded MetaHuman Blueprint template is:

`/Game/AZ/Blueprints/Character/Hero/MHC/AZ_BP_PawnMoverHero_MHC`

Its Walking movement-mode subobject currently has:

| Setting | Loaded MH template | Native class default |
|---|---:|---:|
| Aim turn-in-place enabled | true | false |
| Entry angle | 45° | 35° |
| Exit angle | 6° | 6° |
| Minimum step duration | 0.67 s | 0.67 s |
| Turn rate limit | **360°/s** | **67°/s** |
| Facing smoothing time | 0.05 s | 0.07 s |
| Ordinary aim rate cap | 540°/s | 540°/s |

The hero's `AimFacingConeDeg` is **0°**. These are loaded editor template values; verify the spawned instance in the next user-run session as well. Editing a C++ initializer does not replace serialized Blueprint overrides.

Direct sequence sampling established:

- `AZ_RTG_MH_W2_Stand_Aim_Turn_In_Place_L_Loop` and its R counterpart, under `/Game/AZ/Assets/M16/Riffle_RTG_MH/`, last **0.6666667 s**. Their root yaw progresses from 0 to approximately **−45° / +45°**, with approximately half that yaw at half the duration.
- Their `_IPC` versions also last 0.6666667 s, but root yaw is **zero** throughout the sampled points. Those are the clips selected in the current log. Mover therefore must supply the visual turn's corresponding body rotation; those IPC clips do not turn the capsule by themselves.
- The authored average is about **67.5°/s at 1×**. A 360°/s cap permits much faster body rotation. A cap is not the actual per-frame spring rate, so correlate the real turn before claiming a particular frame completed in 0.125 s.

Current source compounds this mismatch:

1. The pawn advances `AimHoldYaw` and writes `OrientationIntent` every input update before a turn is committed. With a zero cone, slow camera movement can rotate the body indefinitely while remaining below the TIP entry error. Reducing the cap alone does not fix that invariant.
2. Pawn turn eligibility/lifetime and the animation state machine each maintain their own start/deadline. The SM adds an idle dwell. Shared numbers are not one shared turn state.
3. The SM holds the original left/right animation for its minimum duration, but the pawn continues aiming the spring at the live camera. Reversing the camera can reverse body rotation under an opposite-side committed step.
4. The pawn measures held-facing minus body yaw; the SM measures camera minus body yaw. They happen to agree with the current zero cone, but will disagree if a cone is introduced without fixing the contract.
5. Grenade `bIsAiming` in the animation context is raised through the pawn's turn flag, rather than directly through Throwable.Ready/ThrowPreparing. If the two turn lifetimes disagree, the animation may lose its eligibility before its own committed step is complete.

Recent **user-run** log entries already contain grenade stationary `IdleTurnLeft/Right` selections at `moving=0`, `spd=0`, `strafe=1`, `aim=1`, followed by idle with `aim=0`. Thus the turn branch is reachable; do not describe this as “TIP never runs.” The unresolved problem is coherent entry, pace, reversal and completion. The existing trace lacks the correlated yaw/phase fields needed to attribute every crossed-leg frame.

### B. Current grenade layer is already installed

Live readback:

- Carry, Start, Loop and Cancel montages use **Throwable**.
- Close/Far release montages use **FullBody**.
- Skeleton slot groups are **Throwable → Throwable**, **RifleFire → WeaponFire**, **FullBody → DefaultGroup**. A stale graph display label said DefaultGroup for Throwable; the skeleton query is the authoritative registration. Do not “repair” isolation solely from the cached label.
- The Throwable path is wired after the pre-throw pose and before FullBody. It includes its existing torso adjustment and layered blend. The layer identified as `AnimGraphNode_LayeredBoneBlend_3` starts at **spine_01, depth 4**. Inspect the connected graph/pin bindings when resolving effective weights; a struct default is not necessarily a wired pin's value.
- Root offset and procedural feet operate downstream. The ready animation being upper-body does not prevent a lower-body yaw/foot-lock conflict.

Do not recreate the slot, move all montages to FullBody, or blame the raw grenade clip without showing where the distortion first appears. Inspect four pose stages: locomotion output, post-Throwable, post-OffsetRootBone, post-feet rig.

### C. Moving case: the previous diagnostic rule is insufficient

The earlier document treats `dir=Right + asset named StrafeLeft` as decisive proof of an incorrect search. That is not generally valid:

- `dir` is **keyboard intent relative to camera yaw**.
- PoseSearch consumes **world-space trajectory/PoseHistory**, then samples features relative to the visual root. While the body is aligning to the camera, camera-relative right can be body-relative left.
- A mirrored search result retains the original asset name. A left-named sequence can correctly represent mirrored rightward movement.

There is a **confirmed result-propagation gap**: the current MM branch publishes `SelectedAnim`, `SelectedTime` and other existing blend inputs but drops **`FPoseSearchBlueprintResult::bIsMirrored`**. The blend input struct has no mirror field. The continuing-pose path does read the rendered stack's mirror state. This gap is causal only when mirrored entries are eligible and a mirrored result actually wins. The schema has a mirror table, but this review could not read the database's private per-entry mirror options through the exposed reflection API. Do not falsely report that mirrored results were observed.

Another correction: when a weapon profile exists and its strafe database field is null, the current ternary selects that null value; it **does not automatically fall back to the AnimBP default database**. The previously assembled chooser/raw-clip candidate set remains. Determine the actual searched pool for each context before editing database weights.

### D. The live schema does contain directional features

All three inspected default strafe databases reference `PSS_v2_SurvivalMan_Loco`, use normalization, have eight loop assets each, and currently have continuing cost bias approximately **−0.01**.

| Trajectory sample time | Features | Weight |
|---:|---|---:|
| −0.05 s | Position XY | 0.3 |
| 0 s | Velocity XY + facing XY | 1 |
| +0.35 s | Position XY + facing XY | 1 |
| +0.70 s | Position XY + velocity XY + facing XY | 1 |
| +1.0 s | Velocity direction, full vector | 1.5 |

Trajectory channel weight is 1. Pose features include foot_l position relative to foot_r (weight 1), both foot velocities (0.3 each, character space), and pelvis heading (0.1). Those pose features use continuing pose. The role uses SurvivalMan skeleton and AZ_Hero_MDT; compatibility with the MH rendered/query skeleton must be checked, not assumed wrong from names.

Source root samples in sequence-local coordinates over one second:

- Left strafe: approximately **+172.64 cm X**.
- Right strafe: approximately **−172.64 cm X**.
- Forward: approximately **+172.63 cm Y**.

These axes can be correct with the mesh's relative rotation. Do not flip X/Y or rotate the world trajectory into camera space without accounting for the full visual-root transform. `force_root_lock=true` and `enable_root_motion=true` on these clips do not by themselves prove what the PoseSearch index contains.

Small normalized total costs do not establish missing directional information. Different numeric entry times are not inherently mismatched phases. Compare feature residuals, actual contact/pose state, and the pose rendered by BlendStack.

## Execution order and review gates

### 0. Freeze a reproducible baseline

- Record source revision/uncommitted changes, loaded DLL/build, active hero/mesh/AnimBP, concrete movement-mode instance, montage slots and source hashes before editing. Other work is in progress; preserve it.
- Capture effective MH values above, active weapon profile, actual strafe pool, active BlendStack mirror table/bindings, and selected turn assets. Keep records per player/instance.
- Keep both reproduction cases separate. Ask Artur to perform editor checks; do not start PIE or add automated tests without explicit permission.
- Add only focused, opt-in, rate-limited diagnostics needed below. No every-frame string formatting in shipping and no broad log spam.

**Gate:** the baseline describes what actually executes, not just defaults or intended wiring.

### 1. Capture the stationary turn as one timeline

Record movement intent/speed, camera yaw, actor yaw, visual-root yaw, held/committed target yaw, signed angular velocity, turn request/commit serial, side, start/end phase, pawn turn flag, SM state, selected/rendered clip and time, actual turn-rate settings, Throwable/FullBody weights and evaluated foot contacts/pinning.

Use a consistent timestamp/frame or simulation time. Do not equate a search result time or `[v2 Pick]` entry time with the current rendered time. The existing log only reports asset changes and can omit consequential state/time changes.

**Gate:** identify whether crossing starts before the lower-body turn, at entry, on camera reversal, on step completion, or only after procedural feet. Do not permanently disable IK to hide a movement/phase mismatch.

### 2. Repair the stationary turn contract

Required invariant: **Mover rotation and the chosen lower-body clip share a coherent side, yaw target, phase and completion boundary**. One candidate for the existing in-place content is to hold stationary foot facing until a discrete turn is committed, while aim/view moves within a supported upper-body range. Before implementing that candidate, verify that the actual grenade upper-body pose/aim layer supports the residual angle and that preview/release alignment stays correct. This is not authorization to introduce a new 45° body/camera separation or change the intended grenade-facing behavior. If the existing upper body cannot support that residual, preserve the intended facing through a coherent continuous 1× stepping policy instead of inserting a dead zone that aims the grenade incorrectly.

- Correct the MH **serialized movement-mode settings**, not only native defaults. Base the selected profile on measured 45°/0.6667 s content at 1×. Approximately 67.5°/s is the measured average; verify the yaw curve and foot plant/lift timing rather than treating a constant clamp as a complete solution.
- Stop unconditional pre-step body yaw that leaves stationary feet in idle. Accumulate a signed view-to-held-body error and use clear entry/exit thresholds. Trigger by accumulated angle, not mouse speed.
- Use one turn request/commit record or equivalent coherent existing state. Mover and animation consume that record. Remove disagreement between the pawn's timer, SM timer/dwell and separate angle calculations.
- Make actual rotation follow the committed step phase. For IPC clips, use the corresponding source yaw progression or calibrated step profile to drive Mover's angular intent. Do not apply source root yaw and a second capsule rotation simultaneously.
- Camera reversal during a committed step queues/re-evaluates a subsequent turn at a valid support boundary. It must not reverse the capsule underneath the still-playing previous-side step. Camera itself must remain responsive.
- Stopping camera motion must let the current step settle without extra unanimated yaw or an early lifted-foot cutoff. A same-side repeated step must remain phase-continuous, not repeatedly push frame zero.
- Movement input, airborne/reaction/death, stance change, grenade release and cancel need explicit handoffs. Emergency/gameplay interruptions retain their priority. Normal aim release should not leave independently held rotation/animation timers behind.
- Preserve the existing shoulder/grenade upper-body result. Verify the upper-body residual aim range and launch/preview agreement if stationary lower-body holding introduces additional view/body separation; this must not change throw controls or launch direction accidentally.
- Keep turn state Mover-predictable. Simulation/prediction must consume stable input/sync state; repeated trajectory predictions must not mutate a live gameplay timer/commit counter. Do not let an AnimBP worker or a camera tick directly rotate the actor.

**Gate:** slow sweeps, fast sweeps, camera stop and mid-step reversals all show deliberate foot steps with rotation that agrees with the committed side/phase. A lower cap alone, an increased minimum timer alone, or a larger cone alone does not pass this gate.

### 3. Verify layering and procedural foot behavior

- Confirm actual lower-body contribution through the ready slot. Preserve the current upper-body branch and group isolation unless a specific incorrect property is demonstrated.
- Read effective OffsetRootBone bindings and runtime mode; graph literals alone are insufficient. Do not blindly switch Accumulate/Release to compensate for the yaw defect.
- Inspect `contact_l/contact_r` through the final blend, including transition tails. Verify the rig releases a lifting foot and does not pin both feet against a rotation incompatible with the authored step.
- If necessary, perform a temporary user-approved diagnostic comparison with foot pinning disabled, then restore it. This identifies whether IK introduces/amplifies the error; it is not the final fix.
- Preserve ground adaptation on slopes and stairs. Correct an erroneous contact curve or transition reset only for measured affected content.

**Gate:** distortion is absent before and after the procedural stage, with the normal rig enabled.

### 4. Establish the moving MM contract before tuning

For each relevant context, record:

- Active profile and exact candidate pool/database; database index availability and per-entry mirror options.
- Search result `(SelectedDatabase, SelectedAnim, SelectedTime, bIsMirrored, BlendParameters, cost)`.
- Rendered outer-stack `(asset, current time, mirror flag, mirror table, blend weight)`.
- Camera/body/visual-root yaw, world input/velocity, root-local measured and predicted travel; a few trajectory samples already used by the search.
- Outgoing/incoming left/right foot contact and pose phase, state/transition serial, and whether an MM result was actually pushed.

Use per-channel query/selected residuals or PoseSearch debugging where available. Sample at selection/side/phase events, including **same asset with changed mirror**, rather than only asset-name changes.

**Gate:** distinguish a wrong search, a correct search rendered incorrectly, a coordinate-frame misunderstanding, a stale/empty index, and a correct direction with an incompatible crossfade.

### 5. Fix lost search-result information when relevant

- If mirrored results are eligible, wire `bIsMirrored` into the **outer BlendStack** and provide the compatible MirrorDataTable. Complete the result-to-playback contract; do not fix just a log label.
- Carry mirror identity through caches, continuing pose, push conditions and diagnostics. A same-asset mirror change is a different pose source.
- Explicitly reset mirror false on direct non-mirrored/fallback/idle/transition paths. If mirroring contact-dependent logic, left/right semantics must follow the effective pose/table.
- Pass BlendParameters where the searched result can be a BlendSpace; preserve the established 1× playback policy rather than blindly copying WantedPlayRate.
- Newly reflected blend-input fields require a normal build/restart and AnimBP binding update, not Live Coding alone. Verify the loaded outer node after restart.
- If every effective entry is unmirrored-only and no mirrored result occurs, record this as a latent gap, not the demonstrated cause. Continue the coordinate/phase investigation; do not change database eligibility merely to force this hypothesis.

**Gate:** every valid selected pose is played with the same asset/time/mirror semantics, with no stale flag leaking across paths.

### 6. Validate trajectory, schema and pool policy

- Confirm that predictor → PoseHistory → PoseSearch uses one coherent world/visual-root convention. Check mesh-relative orientation, retargeted root axes, prediction facing and schema role/skeleton compatibility.
- Compare stored/indexed left/right root travel against the source measurements. Verify current built indices, valid sample counts and index-ready behavior after saves.
- Resolve the intended null-profile database policy explicitly. If null means inherit, implement a true fallback to the AnimBP default; if null deliberately means use profile chooser clips, preserve it and document the actual pool. Avoid routing armed characters into unrelated unarmed pose sets merely to enlarge the database.
- Tune trajectory/pose weights only if feature residuals show a valid frame and playback contract but incorrect directional discrimination. Rebuild affected indices and compare the same reproduction.
- Keep continuing bias at the restored baseline while correcting direction/phase. Strong stickiness must not conceal a wrong pose.

**Gate:** effective selected/rendered movement agrees with root-local travel, including camera/body misalignment. Asset name versus camera-relative `dir` is not an acceptance rule.

### 7. Repair phase continuity where evidence shows a bad seam

- Compare bilateral foot position, velocity and contact state, not equal normalized timestamps or a blanket half-cycle shift. Include mirror state in this comparison.
- Verify the actual outgoing start/stop → selected loop pair before applying the existing `(loop length − remaining)` seam. The existing phase-zero assumption is pair-dependent when MM selects another directional loop.
- Keep loop → loop handoffs under a coherent pose/contact-aware selection. If the query pose/history is stale or contaminated by upper-body/procedural output, fix the query sampling contract first; do not override correct MM times with arbitrary values.
- Bound the blend to an appropriate support transition and retain a stable outgoing pose. Do not force frame zero on every direction change, freeze all reselection, or add long blends that mix incompatible support legs.
- Treat a request that truly needs a pivot/stop-start as that supported transition; do not fabricate turning motion that the eight-way loops lack. Use the existing movement/state architecture and available clips.
- Confirm no hidden nested full BlendStack or explicit time-zero override has returned. Current inner graph inspection shows a BlendStackInput, which is the intended baseline.

**Gate:** direction reversals and adjacent direction changes keep plausible foot support without preventing responsive input changes.

### 8. User-run acceptance and delivery

Do not add automated tests. Ask before any PIE/editor test and normally let Artur drive the cases.

| Case | Required result |
|---|---|
| Grenade, stationary, slow camera sweep | No indefinite body yaw under planted idle; coherent steps when needed. |
| Fast sweep / wrap across ±180° | Correct shortest signed intent and controlled stepping; no snap or repeated restart. |
| Reverse camera mid-step | Committed body turn and leg side remain compatible; next step responds at a valid boundary. |
| Stop camera mid-step | Foot settles and turn completes coherently; no timer-induced cutoff or extra yaw. |
| Grenade start → loop, release, cancel | Existing inputs/preview/projectile direction preserved; no lower-body ownership leak. |
| Standing and crouched, level and sloped floor | Normal feet rig remains functional. |
| Fight walk/run/crouch, adjacent and opposite directions | Correct effective travel, stable foot phase and responsive transitions. |
| Camera turns while moving | Selection evaluated in the correct local frame; no false camera-bucket diagnosis. |
| Armed/unarmed/throwable context changes | Intended pool and mirror/phase state reset correctly. |
| Explore, ordinary jump/traversal, weapon aim | No regression outside the corrected paths. |

Deliver changed files/assets, before/after settings, actual loaded build, a concise reason for each fix, and correlated clips/logs from the same scenarios. Separate **implemented**, **compiled/saved**, and **user visually verified**. Do not claim the leg-crossing issue solved solely because a build succeeded or clip names now match a direction enum.

## Source map and receipts

- [Pawn input/facing and TIP request](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMoverHeroCharacter.cpp:1025)
- [Walking angular spring/rate clamp](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Character/AZ_PawnMovementMode_Walking.cpp:281)
- [Movement-mode defaults](C:/UnrealEngine/Games/AZ/Source/AZ/Public/Character/AZ_PawnMovementMode_Walking.h:144)
- [Animation context and MM path](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_MoverAnimInstance.cpp:305)
- [SM committed turn hold](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_LocomotionStateMachine.cpp:499)
- [BlendStack input struct](C:/UnrealEngine/Games/AZ/Source/AZ/Public/Animation/AZ_LocomotionTypes.h:538)
- [Procedural feet eligibility/contact gating](C:/UnrealEngine/Games/AZ/Source/AZ/Private/Animation/AZ_MoverAnimInstance_Procedural.cpp:137)
- [Independent source review](C:/UnrealEngine/Games/AZ/Saved/CompassPlanning/strafe-code-review.md)

Live readback receipts under `C:/UnrealEngine/Games/AZ/Saved/CompassPlanning/`: `grenade-stationary-source.json`, `grenade-walking-loaded.json`, `grenade-turn-assets.json`, `grenade-branch-bones.json`, `strafe-detailed-assets.json`, `strafe-root-and-grenade-blend.json`. Earlier failed reflection fields in supplementary receipts are explicitly errors, not evidence of absent data. No full runtime causal capture was performed by Codex.
