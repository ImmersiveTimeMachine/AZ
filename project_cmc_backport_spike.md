---
name: project_cmc_backport_spike
description: "★ LIVE spike plan (branch spike/cmc-backport from feature/NPC @ 9518095, task #16): CMC back-port — KEEP CHT+SM+BlendStack+MM (decision + evidence inside), resurrect the EXISTING v1 ACharacter hero (AAZ_HeroCharacter — discovery 2026-08-05), 5-phase plan + exit criteria. Read first for any spike work."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-19T03:20:43.810Z
---

# CMC back-port spike — plan (2026-08-05; P0 built+committed 2026-08-06)

## ★★ 2026-08-15 ADDENDUM — GASP 5.8 update changes the plan (read [[project_gasp58_update_audit]] FIRST)
GASP 5.8 imported at /Game/GameAnimationSample/ ships an official **SandboxCharacter_CMC + _CMC_ABP**
(plain ACharacter + stock CMC + BP PreCMCTick feel pass; ABP's "Experimental SM" path = exactly our
controller-SM+BlendStack+chooser doctrine) → P1's reference implementation now exists in-editor.
**PoseSearch Interaction** (multi-char MM, backend-agnostic, warp = RM override → CMC-native) replaces
CAS in all plans: P5 becomes a PSI smoke test (was CAS), task #15 repoints to PSI for executions/grab-v2.
CAS is dropped (Epic's own sample doesn't use it; still ACharacter-coupled + deeper Experimental in 5.8) —
the UContextualAnimSceneActorComponent added to AAZ_CmcCharacterBase in P0 + the ContextualAnimation
Build.cs/uproject deps are now removal candidates. P1 upgrades from "parity with v2" to "GASP-5.8 parity":
trajectory (−1/30/0.1/15 + HandleTrajectoryWorldCollisions + per-state data), PreCMCTick derived params
(braking 500/2000, sprint tapers, instant-yaw/200-falling rotation), 7-state controller rules — all values
in the audit file. MetaHuman MHC_Hero is READY (user 2026-08-15) — P-MH stays post-verdict.

## ★★★ 2026-08-21 (final) — ROOT-MOTION-FROM-EVERYTHING ON THE MM PATH: TRIED, FAILED, REVERTED. NEVER AGAIN.

Attempted the Mover hybrid natively on CMC: ABP `RootMotionFromEverything` + `bEnableRootMotion=false`
on in-place clips so only transitions RM-drive the capsule. **Catastrophic, fully reverted same day**
(mode back to MontagesOnly, all 12 clip flags back to true). Measured failure signature: character
travels at full speed with the mesh facing 82-178 degrees off travel for seconds (`ang=-178/-120/-97`
sustained, loop at cost 5-10 vs normal ~0), "camera one direction, movement another".

**Root cause — the mode is structurally incompatible with an MM blend stack:** every rm-on clip's
BLEND TAIL (a start/pivot/stance clip fading out ~0.5s) flags `HasAnimRootMotion`, and CMC's
`bAllowPhysicsRotationDuringAnimRootMotion=false` (default) FREEZES capsule rotation whenever the flag
is up, while partial-WEIGHT root motion overrides velocity with a fraction of the clip's motion.
Repeated transitions accumulate unbounded facing error. Montage RM works because a montage owns the
body at FULL weight; MM never plays anything at full weight during blends. The Mover build got away
with the hybrid because its bridge was TAG-GATED per window (Mover.SkipAnimRootMotion) and its
transitions were near-full-weight BlendStack plays.

If "capsule follows the transition clip" is ever attempted again on CMC, the candidates are
capsule-side and rotation-safe: clamp MaxWalkSpeed to the playing clip's measured `MoveData_Speed`
curve during the commitment window (curves exist, authored 2026-08-21), or a scoped RootMotionSource.
NOT the global mode.

**What SURVIVED the day and is live/baked:** velocity-based IsMoving (stops reachable), the four
intent gates (Starts<100 / Stops-on-release / Pivots>=120deg / StanceTrans-on-stance-change — the
LAST one closed the final fallback leak: crouch<->stand clips at cost 3-5 mid-turn), walk-slice
commitment (-1.0) + measured MoveData_Speed curves + PlayRate binding, role-shaped notify windows.

## ★★ 2026-08-21 (late) — INTENT-GATED MM POOLS: the selection doctrine (check EVERY pool edit against this)

Whack-a-mole lesson: removing the wrong winner from a moving-turn query just promotes the next
least-bad candidate (arc Turns -> Starts -> Stops, three costumes of ONE mechanism: our content has no
per-angle pivots, so SOMETHING wrong always bids). The fix is a complete board where every clip class
has an explicit competition condition, enforced in `Get_DatabasesToSearch` via DB Tags (deliberate GASP
deviation — GASP outbids with 130+ pivots instead of gating):

| class | searchable when | enforcement |
|---|---|---|
| Loops | always (state/gait rows) | gate rows |
| Starts | Speed2D < 100, or a Starts clip is current | C++ filter, tag "Starts" (all 4 starts DBs tagged) |
| Stops | input RELEASED (Acceleration ~ 0, IsMovingAccelerationTolerance), or a Stops clip is current | C++ filter, tag "Stops" (all 4 stops DBs tagged) |
| Pivots | always in their rows (reversals happen at speed) | gate rows |
| TIP | never (user rule: no TIP at idle/moving) | ungated |
| StanceTrans | always, any speed (crouch toggles need it) | Any-row; WATCH for leaks |

Key insight for Stops: STOPPING IS AN INPUT FACT, NOT A VELOCITY FACT — a held stick mid-turn brakes
exactly like a stop; trajectory cost cannot separate them, input can. Both filters carry a
currently-playing guard (CurrentDatabaseTags) so a filter never cuts its own clip mid-play.

Walk-pivot solution (user-invented duplicate-per-role pattern): an asset carries ONE notify/tag set,
so a clip serving two roles gets DUPLICATED — `AnimPro_WalkFwdTurn180_L/R` (duplicates of
WalkFwdStart180_L/R) live in PSD_AZ_Stand_Walk_Pivots (tag Pivots, NOT speed-gated), originals stay in
Walk_Starts (tag Starts, speed-gated). Crouch has the same hole; same pattern applies to
Crouch_WalkFwdStart180_L/R when asked. Sprint reuses Run starts/stops/pivots (row 4, unchanged).

Watch items left deliberately ungated: StanceTransitions in every union (pose channels keep them out so
far); 135-degree walk turns may elect the 180 pivot copy (acceptable). Role-shaped BlockTransition
windows (Pass 1, 31 clips): pivots [50%->end], starts [15%->end], stops [30%->end], stance [15%->end],
loops untouched (their BranchIn corpses feed the v2 Mover branch DBs — NEVER strip).

## ★★ 2026-08-21 — CONTENT DECISION: STAY ANIMPRO, CEILING ACCEPTED (do not re-litigate)

User chose (AskUserQuestion, four options laid out) to KEEP AnimPro content at current gait speeds
(165/375/585) for the hero MM path, accepting the density ceiling. Rejected alternatives, recorded so
they are not re-proposed: RT_NWP+GASP speeds (full density, but run 375->500 ripples into feel/Chalkie
pacing), freeze-and-advance-spike, and revisiting the v2 CHT+SM doctrine.

**The inventory fact that framed the choice (was WRONG in older notes — "332 ingested"):**
`RT_NWP_*` = **887 clips**, the ENTIRE GASP Neutral library retargeted on SKEL_SurvivalMan, including
**351 pivot-family clips** (96 Box, 60 Pivot, 3 Spin, 10 Shuffle) + 50 loops / 120 starts / 66 stops /
114 crouch / 48 jumps / 12 gait transitions. It sits unused because its authored speeds (~206/515/721)
mismatch our gait speeds — the original walk/run oscillation. It remains the ready-made density fix if
this decision is ever reopened (repopulate DBs mirroring GASP Dense + raise speeds to ~200/500/700).

**What the ceiling means concretely (measured):** AnimPro has 4 run pivots (all ~180 deg), 0 walk pivots,
0 spins, 0 shuffles, 0 gait transitions, 1 sprint loop. GASP Dense: 136/133 pivots, 18-20 loops per gait.
Turning quality therefore rests on: Starts (90/135/180 fans) + Run pivots + capsule rotation,
Steering at its ORIGINAL values (S1 ProceduralTargetTime 0.2 / DisableSteeringBelowSpeed 10; S2 0.2 —
the GASP-parity edits 0.4/1.0/1e6 were tried 2026-08-21, user judged the batch WORSE, reverted same day;
re-try only one-at-a-time with A/B), Turns DBs OUT of gates and OUT of PSN (user-approved Stage-2 state:
"lean BP works better"), OffsetRootBone (+ native RootTransform pull in C++ — the BP SetOffsetRootTransform
seam is DEAD, deprecated), and capsule rotation at uniform 180 deg/s (bGaitScaledRotationRate=false; the
per-gait 180/115/90 split was arc-clip-era and is retired).

★ PROTOCOL from the 2026-08-21 thrash: this baseline is the reference. ONE change per PIE test from here —
never batch content edits with node-settings edits; every PSD content edit requires an editor RESTART
before judging. A "restore" must restore the approved state EXACTLY, not the pre-approval state (the
2026-08-21 restore wrongly re-gated the Turns DBs the user had approved removing — that overshoot plus
batched steering edits is what made "everything worse").
Do NOT try to close the turn-quality gap by settings tuning — two sessions proved settings are at parity
and the residual is content. Open question for first PIE: does Steering's root-motion SCALING work with
bForceRootLock=true clips, or only its additive correction path.

**FINAL RESOLUTION on strafe clips in explore pools (settled 2026-08-21 after going back and forth):**
Explore (OrientToMovement) pools are **FORWARD-ONLY** — user-confirmed, and this is the fix for the
"moving right but face looks left" bug. Mechanism: GASP's orient-to-movement Dense pools DO keep every
directional loop, but ONLY because its clips carry an `Enable_Warping` curve driving the
OrientationWarping node, which rotates the pose so the clip's movement direction matches actual travel.
AnimPro clips have NO such curve -> our OW node reads alpha 0 -> a lateral clip plays with its authored
off-axis torso/head while the capsule faces velocity = the facing bug. So: lateral/backward loops in an
orient-to-movement MM pool REQUIRE working orientation warping; without it, forward-only is correct.
The strafe/backward family is RESERVED for the combat Strafe rotation mode
([[project_combat_fist_build_plan]]: combat strafe locomotion, build once, shared with aiming) — when
that slice lands it needs strafe-gated DBs (gate struct needs a RotationModes axis = UPROPERTY = build)
OR Enable_Warping curves authored onto the clips (scriptable, AnimationLibrary curve API is GC-safe).

## ★★★ 2026-08-16 — STATE OF PLAY + NEXT SESSION START (supersedes the 2026-08-15 plan below)

**USER PIVOT (2026-08-16): GASP is REFERENCE ONLY.** The "stock GASP ABP + UEFN skeleton" strategy in the
next section is DEAD — we keep our own animations, databases and ABP, and read Epic's ABP as a spec.
Consequence: the UEFN mesh swap, the `BPI_SandboxCharacter_Pawn` implementation and the combat-montage
retarget inventory all drop out of the plan entirely.

### DONE this session
- **MetaHuman hero shipped.** `metahuman_base_skel` ← compatible with `SKEL_SurvivalMan` (+ retarget-modes
  flag); 9 sockets ported to `SKM_MHC_Hero_BodyMesh`; `BP_CMC_Hero` rebuilt modular (Mesh=MetaHuman body,
  `Face`+5 grooms, 6 leader-posed SurvivalMan garments, `MetaHumanComponentUE`, `LODSync`). Animations,
  databases and choosers UNTOUCHED — the mesh is a swappable slot. See [[feedback-metahuman-modular-hero]]
  (re-assembly wipes it → run `Tools/metahuman_fixup.py`).
- **Phase 1 C++ landed** (editor-closed build green): CAS removed (component + Build.cs + uproject);
  per-frame feel pass on the HERO (`ApplyMovementFeelParams`, actor is a tick prerequisite of CMC);
  JustLanded latch + `GetAimRotation`; `ResolveGaitAndStanceFromTags` drives gait + native Crouch from
  Movement.* tags so GA_Run/GA_Crouch work on both generations untouched;
  `WireModularMeshFollowers` (leader pose — property alone is a no-op, needs the setter + bForceUpdate).
- Plan revisions from measurement: **capsule stays 25/90** (MetaHuman is 179.3cm; GASP's 86 was for a
  mannequin we dropped); hero keeps its own jump (JumpZ 420 / gravity 1.5), NOT GASP's 500.

### DECISIONS TAKEN (do not re-litigate)
1. **Target path 0 — the Motion Matching node**, not GASP's SM+Chooser+MM+BlendStack path (Epic labels
   that one "highly experimental… far from ideal… to inform future tool development").
2. **New ABP built from scratch** with `SandboxCharacter_CMC_ABP` as reference — full teardown/spec in
   [[project-gasp-cmc-abp-spec]].
3. **ABP skeleton = `SKEL_SurvivalMan`** (the ABP follows the animation library; compatible skeletons lets
   it render on the MetaHuman mesh — already proven live).
4. **Logic split: thin C++ seam, graph + logic in BP** (mirrors GASP, whose ABP parent is plain
   `UAnimInstance` and whose pawn seam is ONE struct). NOTE: AnimBP graphs cannot be safely scripted
   (Python GC crash) → graph authoring is the user's; spec + verification + C++ + databases are mine.
5. **Do NOT parent the new ABP to `UAZ_MoverAnimInstance`** — that class IS the live v2 Mover hero's
   (`AZ_BP_PawnMoverHeroCharacter` uses `AZ_ABP_MoverAnimInstance_C`), and it sets
   `RootMotionFromEverything` in `NativeInitializeAnimation` for the Mover RM bridge — the opposite
   contract to CMC. A new ABP parented to it inherits that bug.
6. Keep `UAZ_LocomotionStateMachine` in C++ as a **gameplay** state provider that no longer drives pose.

### ✅ 2026-08-17 — `UAZ_CmcAnimInstance` BUILT (editor-closed build green)

Files: `Public/Animation/AZ_CmcAnimTypes.h` (new, `FAZ_CmcAnimContract` 19 fields) ·
`Public/Animation/AZ_CmcAnimInstance.h` + `Private/Animation/AZ_CmcAnimInstance.cpp` (new) ·
`FillAnimContract` added to `AAZ_CmcCharacterBase` (virtual) and overridden on `AAZ_CmcHeroCharacter`.

Shape: parent = plain `UAnimInstance`, `UCLASS(Abstract)` (an AnimInstance with no AnimGraph silently
evaluates the ref pose — Abstract makes the raw-native-class mistake impossible). `RootMotionMode =
RootMotionFromMontagesOnly` **in the CONSTRUCTOR** so the ABP inherits it as a CDO default and can still
override; `NativeInitializeAnimation` only WARNS on mismatch — never corrects, because a silent fix hides
the authoring error (this is the anti-pattern of `AZ_MoverAnimInstance.cpp:94`).
Threading: game thread pulls the contract only; trajectory + all derived state run in
`NativeThreadSafeUpdateAnimation`, with `bGenerateTrajectoryOnGameThread` as a one-bool escape hatch.

**Two design corrections found by reading engine source (both already applied):**
1. `FPoseSearchTrajectoryData` has NO accel/braking UPROPERTYs — its private `FDerived` reads MaxSpeed /
   BrakingDeceleration / Friction straight off the movement component every frame
   (`PoseSearchTrajectoryLibrary.h:31-48`). Predictor-vs-feel-pass divergence is **impossible by
   construction**; do not try to push those numbers in. Exposed knobs are only `RotateTowardsMovementSpeed`,
   `MaxControllerYawRate`, `BendVelocityTowardsAcceleration`.
2. `ACharacter::GetMovementBase()` and the `UPrimitiveComponent` overload of
   `MovementBaseUtility::GetMovementBaseVelocity` were **deprecated in 5.8** ("will no longer compile"
   next release). Use `GetMovementBaseInterfaceData()` + the `FMovementBaseInterfaceData*` overload.
   `BasedMovement.BoneName` is unaffected.

Left as a visible warning on purpose: `HandleTransformTrajectoryWorldCollisions` is `UE_EXPERIMENTAL(5.6)`
and emits C4996 every build of that file. Not suppressed — silencing an "may be removed" notice on a spike
whose premise is escaping experimental APIs is the wrong instinct.

Unsettled, deliberately behind a CDO bool: `bInvertFootPhase`. Default reads "left foot planted -> RIGHT
foot leads -> LR/RR". No legacy to preserve (v2's foot phase never worked), so it can only be settled by
watching it — a bool means that costs a click, not a rebuild.

### ✅ 2026-08-17 (later) — REWRITTEN as an exact GASP port (build green)

User decision: recreate GASP's `Update_Logic` in C++ **node-for-node, same function and variable names**,
AnimGraph stays BP. Done — the update half of `UAZ_CmcAnimInstance` is now a direct port. Every literal is
a CDO property (numbers + graph structure: [[project-gasp-cmc-abp-spec]] §2b). Bools carry the UE `b`
prefix and Blueprint still displays GASP's name, so no naming conflict.

**Four stated deviations (do not "fix" these):**
1. `Update_TargetRotation` NOT ported — only the experimental branch calls it, and that flag is false.
2. `Update_MovementDirection` kept but marked AZ-only — MM selects by trajectory; our CHT rows need it.
3. **Native drives the chain** (`NativeThreadSafeUpdateAnimation`), not a BP call as in GASP. A BP-owned
   call means one deleted node silently freezes all anim state. BP's only job is `SetOffsetRootTransform`;
   absent it, `RootTransform` falls back to `CharacterTransform` = GASP's own else-branch.
4. `IsPivoting` ports the MM branch only (threshold by RotationMode 45/30/0). The SM branch's
   stance×gait×speed-window table (Walk 50-200, Run 200-550, Sprint 200-700, crouch threshold 65) is
   unreachable on our path — recorded here in case the SM path is ever revisited.

★ `IsMoving` = `Velocity != 0 (tol 0.1) AND Acceleration != 0`. The `Trj_FutureVelocity` term our OLD
memory note claims is present is a node Epic left **wired to nothing**, with the AND pin at its `true`
default. Ported as it RUNS. Two frames of measured behaviour beat one line of remembered doctrine.

★ Frames differ ON PURPOSE: `RelativeAcceleration` is measured against `RootTransform` (offset root,
yaw+90) but `CalculateRelativeAccelerationAmount` against `CharacterTransform` (the capsule). GASP does
this; it is not a bug.

**Tooling built this session** (see the `az-cpp-utility-tools` skill): `UAZ_BlueprintNodeUtils::
ListFunctionNodes` now emits pin DEFAULTS, WIRING, PropertyAccess **paths** (read by reflection —
the node header is in a plugin Private folder), and recurses into **collapsed/composite subgraphs**
(that is where GASP hides its pivot conditions). This is now the only sane way to read a GASP graph;
the rider clipboard export truncates at ~5 nodes and Python cannot read protected node properties.

### ★ NEXT SESSION STARTS HERE (state as of 2026-08-18 end: gates + PSN built green, pushed `3ae0f01`)

**C++ for the MM path is COMPLETE.** Batches 1-3 pushed (`b732e86`); 2026-08-18 added the
**database-gate rework (committed `3ae0f01`, editor-closed build GREEN 2026-08-18)**:

- **`FAZ_DatabaseGate`** (`AZ_CmcAnimTypes.h`): typed row model of GASP's `CHT_PoseSearchDatabases_Dense`
  — (MovementModes, Stances, MovementStates, Gaits) each TArray, **empty = Any**, + Databases + `Label`.
  `Get_DatabasesToSearch` unions all matching rows (= EvaluateChooserMulti semantics). REPLACES the three
  flat arrays `Databases_Stand/_Crouch/_Always` (deleted). Decision record: **NO chooser asset** — GASP's
  meta-CHT is only a `DDCvar.MMDatabaseLOD` debug tier switch (verified: binding chain `;MMDatabaseLOD;`
  in the uasset, written solely by `Update_CVarDrivenVariables` reading the CVar); tier tables are just
  4-enum gates; v1/v2 both abandoned their DB choosers (`CHT_NoWeapon_Locomotion` has **0 rows**, live v2
  `UAZ_MoverAnimInstance` hard-picks single-DB properties). Second density tier later = second gate array
  behind an FAZModule CVar.
- **Empty-union guard**: no matching gate → do NOT call `SetDatabasesToSearch` (node persists last pool)
  + warn once. **No change-detection gating** — engine node just stores array + NextUpdateInterruptMode;
  `InterruptOnDatabaseChange` does its own membership check, so push every update like GASP.
- **`PSN_AZ_CMC` created + assigned to all 27 DBs** (was: ALL our PSDs — v2's too — had
  `NormalizationSet=None`, so cross-DB costs were never comparable; GASP uses one PSN per tier).
  `PoseSearchNormalizationSetFactory` works from Python directly (no modal, unlike the DB factory).

**Nothing has run yet.** Between here and first motion:

1. **Stage A AnimGraph** (manual — AnimBP graphs cannot be safely scripted; the user confirmed the
   utilities did not work for this and will hand-place):
   `Motion Matching → Pose History → Output Pose`
   - Pose History: `Tag=PoseHistory`, `SamplingInterval=0`, CollectedBones = foot_r, foot_l, thigh_r,
     thigh_l, spine_05, pelvis; CollectedCurves = `Phase`; `RootBoneRecoveryTime=0.3`
   - ★ `TransformTrajectory` pin → **PROPERTY-BIND to `Trajectory`**, context **Thread Safe**. This is a
     BINDING, not a wire. Missing it = MM has no query and the pose never leaves frame 0, which reads
     like a broken database.
   - MM node: `BlendTime 0.5`, `NotifyRecencyTimeOut 0.2`
2. **Two node-function bindings** on the MM node: `Update_MotionMatching` on *On Update*,
   `Update_MotionMatching_PostSelection` on *On Update after selection*.
3. **Author the 7 `DatabaseGates` rows on the ABP CDO** (replaces the old three-arrays step):
   | Label | Modes | Stances | States | Gaits | Databases |
   |---|---|---|---|---|---|
   | StandIdle | Ground | Stand | Idle | Any | Stand_Idles, Stand_IdleBreaks, Stand_TurnInPlace |
   | CrouchIdle | Ground | Crouch | Idle | Any | Crouch_Idles, Crouch_IdleBreaks, Crouch_TurnInPlace |
   | StandMove | Ground | Stand | Moving | Any | Stand_Walk_* + Stand_Run_* (10) |
   | SprintMove | Ground | Stand | Moving | Sprint | Stand_Sprint_Loops/Starts/Stops/Turns |
   | CrouchMove | Ground | Crouch | Moving | Any | Crouch_Walk_* (5) |
   | GaitTrans | Ground | Stand | Any | Any | Stand_GaitTransitions |
   | StanceTrans | Any | Any | Any | Any | StanceTransitions |
   ★ Deliberately NOT GASP's strict per-gait split: we have **no Sprint_Pivots**, so StandMove leaves
   walk+run pools reachable while sprinting. Safe only while `IsMoving()` needs vel AND accel ≈ 0
   (stops stay matched through deceleration) — revisit gates if that formula loosens.

★ Turn on `bDebugAnim` (Python name `debug_anim`) BEFORE the first PIE. The 1 Hz line now carries
speed, moving/pivot/TIP, direction, foot, all three trajectory velocities, sample count, **matched gate
labels + selected database + SearchCost + loop + tag count**. That line separates "trajectory not built"
from "gate hole" from "search found nothing" from "search found something bad" without guessing.

**PROPOSED next content pass (user aware, not yet approved): notify-mirror.** Our 332 ingested clips have
**ZERO functional PoseSearch notifies** — the 7-11 events they carry are null-class corpses (GASP foley BPs
stripped at retarget). Loop flags + curves are correct. Every `RT_NWP_M_Neutral_X` has a same-length GASP
5.8 twin `M_Neutral_X`: script = read twin's PS windows → apply via `AZ_PoseSearchUtils` helpers
(AddBlockTransitionNotify/AddExcludeFromDatabaseNotify/AddModifyCostNotify/AddOverrideContinuingPoseCostBiasNotify),
clean dead events in the same pass. 5.8 semantics (from `PoseSearchAnimNotifies.h`, CORRECTS the 5.5-era
catalog): **BlockTransition = "Block Transition IN"** (search can't RETURN results in-window; playback
advances through fine) — NOT "prevents leaving". **BranchIn in 5.8 GASP points at the SM-path DBs**
(`PSD_SM_CMC_Loops`/`_Transitions`) = experimental-SM machinery, NOT needed for our MM path (membership =
DB asset list). ModifyCost = "Override Base Cost Bias", negative CostAddend = more likely (land heavy -0.3).

Then, in order: Stage B `DefaultSlot` → C `OffsetRootBone` (+ the `SetOffsetRootTransform` wire) →
D foot placement/LegIK → E the two additives (need BlendSpaces built from our `AO_Stand`/`AO_Crouch` 21
each and the `Lean_M_Neutral_Run_Lean_Pose_*` clips). **Skip RemapCurves entirely.**
OffsetRootBone config is NOT drop-in: GASP runs TranslationMode Interpolate / RotationMode **Accumulate**
/ halflife 0.2 / MaxTranslationError **30**, and leaves CollisionTestingMode Disabled — note collision
needs BOTH a mode AND a finite MaxTranslationError or it is dead code.

Deferred content work: step 3 strafe/combat slice (`Box`/`Diamond`/`Hourglass` ≈285 clips), jumps/lands,
traversal. And `Get_StrafeYawRotationOffset` ships with that slice (we have all six `StrafeOffset_*` curves).

Known thin spot to watch, not fix blind: standing turn-in-place has 2 clips vs crouch's 8.

Open item: `BP_CMC_Hero`'s `Mesh` was last seen with the RAW C++ class `AZ_AnimInstance` assigned as its
anim class (no `_C` — not a Blueprint), and the garments still carry `AZ_ABP_MoverAnimInstance_C` in the
template (harmless; C++ clears it at runtime). Verify/clean when the new ABP lands.

## [SUPERSEDED 2026-08-16] PLAN — next session (written 2026-08-15; strategy: stock GASP ABP + UEFN skeleton)

USER DECISION: hero anim stack = **Epic's SandboxCharacter_CMC_ABP UNMODIFIED + all GASP databases**, driven
through its own seam — `BP_CMC_Hero` implements `BPI_SandboxCharacter_Pawn.Get_PropertiesForAnimation`
(the ONE call the ABP makes per frame). Hero mesh = `SKM_UEFN_Mannequin` for the spike. MetaHuman MHC_Hero
display via Epic's RetargetedCharacters pattern at P-MH; combat-montage retarget (SurvivalMan→UEFN)
inventoried AFTER locomotion proves. All reference values: [[project_gasp58_update_audit]].

**Phase 0 — gates (editor open, ~15 min).** (1) Re-set L_001 WorldSettings GameModeOverride =
`BP_CMC_GameMode_C` if it didn't survive (map was never saved). (2) PIE the P0 scaffold AS-IS (SurvivalMan +
AZ_MoverAnimInstance CMC branch): `[CmcAnim]` log + WASD + camera + jump. This is the P0 acceptance gate —
run it BEFORE the ABP swap so later failures attribute to the new anim stack, not the pawn plumbing.

**Phase 1 — C++ batch (editor closed, ONE build).**
1. CAS removal: `ContextualAnim` member + CreateDefaultSubobject out of AZ_CmcCharacterBase; drop
   `ContextualAnimation` from AZ.Build.cs + AZ.uproject. (MotionWarping component STAYS.)
2. PreCMCTick feel pass in AAZ_CmcCharacterBase: derived-params update each frame BEFORE CMC ticks
   (tick prereq: CMC after actor tick, mesh after actor — GASP does this via AC_PreCMCTick component).
   Formulas (audit file): braking 500 w/input / 2000 idle; accel walk/run 800, sprint taper 800→300 over
   speed 300→700; friction 5, sprint taper 5→3 over 0→500; RotationRate yaw −1 (instant) grounded / 200
   falling; rotation mode fixed OrientToMovement for v0 (strafe/aim later). Speeds v0 = GASP forward scalars
   (walk 200 / run 500 / sprint 700 / crouch 225) — the DBs are authored for these; directional
   vector+curve model DEFERRED until strafe. CMC details: JumpZ 500, AirControl 0.25,
   bUseFlatBaseForFloorChecks, MinAnalogWalkSpeed 150, BrakingFrictionFactor 0, PerchRadiusThreshold 20,
   CrouchedHalfHeight 60, bCanWalkOffLedgesWhenCrouching.
3. JustLanded latch: OnLanded → bJustLanded + LandVelocity, 0.3 s retriggerable clear; BlueprintPure
   getters for the BP seam (JustLanded, LandVelocity, aim rotation = ControlRotation local / BaseAimRotation
   remote) — the GASP chooser inputs JustLanded_Light/Heavy (threshold 700) read these.
4. Reroute GA_Run/GA_Crouch off Mover: inspect current impls, route through a backend-agnostic seam
   (SetGait / native Crouch()) so sprint+crouch inputs work on the CMC hero.
Gate: editor-closed CLI build green (standard command, MaxParallelActions=4).

**Phase 2 — editor data pass (MCP; the GASP ABP asset itself is NEVER edited — no AnimBP-save risk).**
1. BP_CMC_Hero: add Implemented Interface `BPI_SandboxCharacter_Pawn`; implement
   `Get_PropertiesForAnimation` building `S_CharacterPropertiesForAnimation` from the new C++ getters.
   ★ Seam-trace enum mappings with real ints BEFORE PIE (EAZ_Gait→E_Gait, stance, E_MovementMode:
   OnGround/InAir from IsMovingOnGround/IsFalling — do NOT trust display names).
2. Mesh swap: SKM_UEFN_Mannequin + AnimClass `SandboxCharacter_CMC_ABP_C`; adopt GASP capsule 30/86 +
   mesh (0,0,−88) yaw −90 as BP overrides (C++ stays 25/90 for the Chalkie). Combat stand-off numbers
   re-measure later anyway (P4).
3. Add `AC_FoleyEvents` component (free footsteps — GASP clips carry the notifies). Optional if noisy.
4. CVar defaults: flip AZ `DDCvar.FootPlacementMode` 0→1 (GASP parity). LocomotionSetupCMC already
   mirrored (default 0 = full MM).
5. Compile+save BP_CMC_Hero (regular BP — required after scripted edits).
6. PIE: full GASP locomotion expected (idle/walk/run/sprint/crouch, starts/stops/pivots, TIP, jump via our
   GAS→native Jump()). Then live `DDCVar.LocomotionSetupCMC 1` → A/B the Experimental-SM path.

**Phase 3 — assess + commit.** Side-by-side vs v2 (GameMode override flip = generation toggle). Record
verdict notes, commit, update memory/tasks.

Known risk axes: (a) enum mapping drift (seam-trace); (b) RootMotionMode comes from the stock ABP class
defaults (their content, their setting — our GAS montages on DefaultSlot still RM-from-montages); (c) editor
"Accessed None TryGetPawnOwner" noise from GASP assets loading without a pawn = benign preview noise;
(d) GA_Run/GA_Crouch reroute may reveal deeper Mover coupling — timebox, stub direct input binds if needed.
DEFERRED explicitly: traversal (needs Traversable collision channel — conflict with AZ GTC1 'Ability'),
GASP GameplayCameras (keep our SpringArm stances), directional speed model, MetaHuman display, combat
retarget inventory, PSI smoke test (task #15), Chalkie (unchanged — classic ABP).

## ★★ RESUME HERE (session end 2026-08-06) — P0 COMPLETE @ `56dda9d`, AWAITING FIRST PIE
Code built green (editor-closed CLI, 106s) + data pass done via unreal-mcp toolsets, committed+pushed on
`spike/cmc-backport` (upstream set). Content (all compiled+saved, values READ BACK verified):
- `BP_CMC_Hero` @ /Game/AZ/Blueprints/Character/Hero/CMC (USER-CHOSEN folder, not Spike/): full v2 mirror —
  AZ_IMC_RT_PawnInputs, AZ_IA_RT_Move/Look, StartupAbilities Jump/Run/Crouch, BP_GA_PlayerGrabbed,
  BP_GA_HitReact_Hero, SKM_SurvivalMan_Mesh3 + AZ_ABP_MoverAnimInstance_C; camera = v2 values (boom
  RelLoc Z=70 ★, arm 220, socket (0,70,0), lag 8/10, FOV 90 — ctor had 80, BP overrides it).
- `BP_CMC_Chalkie` @ Character/Infected/CMC: SK_ZombieAC_A + AZ_ABP_Chalkie_C, mesh rel (10,0,-94),
  DA_ChalkieAnims_C_Rotter, 4 ability BP classes. NOT placed in any map yet — user places like BP_AZ_Chalkie.
- `BP_CMC_GameMode` @ Game/CMC: child of BP_AZ_GameMode_C, only DefaultPawnClass swapped = THE generation
  toggle. USER DECISION: NO test map (deleted) — test in L_001 via WorldSettings GameModeOverride.
  ★ The override was set on L_001 IN-EDITOR but the MAP WAS NOT SAVED — next session it may be gone;
  re-set = one property (WorldSettings.DefaultGameMode = BP_CMC_GameMode_C), clear it = back to v2.
- v2 camera stance configs recorded for P2: Explore 220/(0,70,0)/90/8 · Strafe 100/(0,70,10)/90/8 ·
  Aiming 120/(0,55,5)/55/8 · Grabbed 120/(0,0,-10)/35/5 · GrabOutcome 320/(0,40,25)/90/7.
- ★ Kellan MetaHuman (Content/MetaHumans, BP_Kellan) = LEGACY `metahuman_base_skel` body → must be rebuilt
  through the in-engine MetaHuman Creator (Beta, ships in our 5.8) at the P-MH phase. Face-match route:
  MetaHuman Identity → Mesh-to-MetaHuman from the SurvivalMan head (module verified present); single photo
  is NOT a first-class identity input (footage/depth is; photo = manual sculpt reference). Wardrobe
  (hoodie/cargo) already fits the civilian look. P-MH stays AFTER the spike verdict.
- NPCLevel copy revealed placed GASP SandboxCharacter_CMC_C ×3 + SandboxCharacter_Mover_C ×2 in that level
  (reference chars; irrelevant now the test map is dead, but good to know NPCLevel carries them).
NEXT: (1) user PIEs L_001 → expect `[CmcAnim] … driving a CMC character` log + WASD + camera + MM loops;
jump SHOULD work (GA→IAZ_JumpRequester→native Jump()); stops/starts cross-fade only; GA_Crouch/GA_Run
likely inert (Mover-coupled — P2). (2) Then P1: real phase machine on CMC branch + RM transitions
(RootMotionFromEverything already correct on hero path) + gait input + crouch/sprint rerouting.
MCP note: this session the unrealclaude execute_script surface was ABSENT; drove everything via the
`unreal-mcp` router toolsets (BlueprintTools/ObjectTools/AssetTools/SceneTools — refPaths need
`/Path/Pkg.Asset` form; CDO via get_default_object; ACharacter mesh subobject = `:CharacterMesh0`;
level-actor refs go stale the moment the user switches levels — re-find_actors after any level change).

## ★ P0 IMPLEMENTED 2026-08-06 (first CLI build pending at write time) — decisions + deviations
User pivots incorporated: (a) classes FROM SCRATCH, not v1 resurrection (v1 = GASShooter-era baggage);
(b) naming `AZ_Cmc*` prefix (AskUserQuestion choice) in `Source/AZ/{Public,Private}/Character/Cmc/`;
(c) **CAS + Motion Warping committed** — base class carries `UMotionWarpingComponent` +
`UContextualAnimSceneActorComponent`; ContextualAnimation plugin enabled in AZ.uproject + Build.cs.
What exists: `AAZ_CmcCharacterBase` (ACharacter + IAbilitySystem/TagAsset/Team/JumpRequester/CombatAvatar;
gait→MaxWalkSpeed one-owner SetGait, v2 capsule/mesh conventions), `AAZ_CmcHeroCharacter` (PlayerState ASC
+ v2 grant pattern, EnhancedInput self-pushed IMC in PawnClientRestart, camera boom 220/70, CMC feel
starting values), `AAZ_CmcInfectedCharacter` (own ASC Minimal + Vitals, AAZ_InfectedAIController reused,
native `AnimSet` UPROPERTY — exact reflection name, Death+HitReact grants; Melee/Grab = P3),
`IAZ_CombatAvatar` (Public/Character/AZ_CombatAvatar.h).
DEVIATIONS from the approved plan, each for cause:
1. **NEW interface `IAZ_CombatAvatar`** instead of extending `IAZ_CombatInterface` — the existing one is a
   GASShooter graveyard (all commented out but one delegate) that v2 pawns never implemented; a purpose-built
   seam with default no-ops is the cleaner cut. v2 implementations + GA call-site swaps still land in P4.
2. **NO UCharacterTrajectoryComponent** — the hero ABP generates trajectory itself via
   `UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectory` (production 5.8 for-Character path,
   CMC-simulated prediction, BlueprintThreadSafe). One trajectory owner; component would be a second.
3. **AZ_AnimInstance.cpp legacy-cast swaps DROPPED** — discovery: the LIVE hero ABP is
   `AZ_ABP_MoverAnimInstance` → `UAZ_MoverAnimInstance` (Mover-only, v2, NO AnimGraph SM — C++ phase enum);
   the dual-path `UAZ_AnimInstance` belongs to the OLD v1-era ABP the CMC hero never runs. The real seam:
   `UAZ_MoverAnimInstance` got `Cached_CmcCharacter` + a compact `UpdateAnimation_Cmc` (velocity/gait/stance/
   tags/trajectory + LOOPS-ONLY phase pick; real phase machine + RM transitions = P1). Mover body untouched.
4. `UAZ_InfectedAnimInstance` dual-pathed (Cached_CmcPawn): RootMotionMode EXPLICIT per branch — Mover =
   FromEverything (RM-attribute bridge), CMC = **FromMontagesOnly** (montage RM native; FromEverything would
   feed the loco sequence players' RM into the capsule). Lazy re-resolve sets the mode too (silent-default trap).
Known P0 limitation (logged in-game via [CmcAnim]): transitions cross-fade instead of playing authored
stop/start clips until P1. RootMotionFromEverything on the hero CMC path is correct and becomes the P1 RM
route (CMC consumes graph RM natively in that mode).

Branch `spike/cmc-backport` from feature/NPC @ 9518095 (main is 61 commits behind — never use it). Task #16.
Context: 5.8 = final UE5 release, Mover experimental forever on this line ([[project_architecture_rationale]] addendum).

## ★ DECISION — anim stack: KEEP Chooser(CHT) + SM + BlendStack + MM for the hero
User asked CHT+MM vs classical blendspaces+RM-montages. Evidence-based verdict: KEEP, because:
1. **Stability fact-check:** Chooser + PoseSearch are PRODUCTION plugins in 5.8 (uplugin flags verified; Chooser
   moved out of Experimental/ entirely). Only Mover was experimental. Dropping MM removes no risk.
2. **Docs/info:** CMC+GASP-style MM is Epic's flagship sample — the most-documented modern locomotion setup.
   Our 191-clip GASP content is AUTHORED for MM (discrete per-foot starts/stops/pivots). The L/R-foot walk
   stops the user wants come from MM pose matching + chooser rows; blendspaces can't pick a stop by foot
   phase without hand-built sync-marker logic. Blendspace rebuild = re-author the graph AND lose that.
3. **Measured seam (not assumed):** MM consumes `FTransformTrajectory` — movement-source-agnostic.
   `UAZ_AnimInstance` KEPT the complete legacy-ACharacter dual path (init :84-88, trajectory :204-208 via
   `CharacterTrajectoryComponent`, essential values :136, ~15 more sites to :1151). Engine 5.8 PoseSearch has
   `UpdatePrediction_SimulateCharacterMovement` (CMC-native prediction). CHT/SM/BlendStack sit ABOVE trajectory
   — zero changes.
4. Rationale doc Decision 1 (CHT+SM+BlendStack) was always Mover-independent; its revisit clause (<30 clips,
   no weapons, no MM) does not fire.
NPC: KEEP classic SM + sequence players (already classical; no MM). Combat: KEEP montage+RM rail — CMC makes
RM native (delete the bridge). Blendspaces stay a per-state fallback option later (SM owns logical states).
Co-op: ACharacter+CMC+GAS = THE canonical replicated stack (CMC prediction battle-tested; ASC montage
replication already our doctrine; ABP/MM is client-side cosmetic off replicated movement). v1 hero even ships
weapon Server-RPCs already. Mover co-op (NetworkPrediction fixed-tick) had ~zero community examples.

## ★ DISCOVERY 2026-08-05 — v1 ACharacter hero STILL EXISTS AND COMPILES
`AAZ_HeroCharacter : AAZ_CharacterBase : ACharacter` (+IAZ_CombatInterface, IAbilitySystemInterface,
IAZ_JumpRequester) — GASShooter-lineage v1 hero with: CMC tunables (WalkSpeed 195, accel 450, jump, step 30,
slope 38), full camera rig (boom/lag/FOV/aim/crouch offsets), GAS possession wiring (PossessedBy/
OnRep_PlayerState, ASC on PlayerState), `UCharacterTrajectoryComponent* CharacterTrajectory` for MM, FP mesh,
replicated weapon/inventory RPCs. Referenced today by GA_MeleeAttack/ChalkieGrab/PlayerGrabbed/Crouch/
GameplayAbility/PlayerController — both hero paths compile side by side. **Spike Phase 1 = RESURRECT v1, not
build new.** No v1 infected exists — Chalkie CMC class is new spike work.

## 5-phase plan (1-week time-box)
- **P0 (½d) resurrection audit:** test map + GameMode default pawn = AZ_HeroCharacter (BP child, editor-assigned
  assets per no-hardcoded-paths rule); possess; verify the 21-IA IMC stack reaches AddMovementInput (v1 may
  predate the RT mirror [[project_input_stack_rt_mirror]]); verify ABP takes the legacy branch (log which);
  ABP asset on the v1 mesh.
- **P1 (2d) locomotion+MM parity:** trajectory via CharacterTrajectoryComponent OR the 5.8 library path
  (PoseSearchGenerateTrajectory + SimulateCharacterMovement — what GASP 5.5+ uses; decide here if the
  component is deprecated); port v2 gait semantics (Walk165/Run375/Sprint585) onto MaxWalkSpeed switching;
  TIP; L/R stops; native CMC crouch + our anim chain; jump = CMC native + hybrid 2-clip.
- **P2 (1d) gates + camera:** State.Grabbed/MeleeAttacking movement zeroing on CMC (GetMaxSpeed override or
  input consume — pick ONE owner); camera stances (CameraGrabOutcome etc.) port or accept v1 camera for spike.
- **P3 (1d) Chalkie on CMC:** new `AAZ_InfectedCharacter : ACharacter`; REUSE AAZ_InfectedAIController+BB/BT
  (standard MoveTo); gait→MaxWalkSpeed map (values = authored clip speeds to avoid slide); classic ABP reads
  CMC velocity; RM-lite curve-follow MUST be gated OFF (CMC RootMotionFromMontagesOnly, else double-speed).
- **P4 (1d) combat proof:** melee exchange — motion warping NATIVE on ACharacter (no DriveRootMotion; keep
  warp-window gate logic, drop the Mover drive); hit reacts both sides; one full grab cycle (paired montages;
  close-in = look-at + MoveComponentTo or velocity nudge).

## Exit criteria (measure & instrument)
Side-by-side feel vs feature/NPC build; ported [MeleeRM]/[MeleeHit]/[GrabIK] numbers; full breakage list;
LOC deleted vs added. THEN the migrate/stay decision — with data.

## Known failure axes
1. v1 is ~4 months stale — v2-era systems (crouch chain, jump hybrid, obstacle reactions, clearance, camera
   stances, grab framing, ProduceInput gates) reference AAZ_PawnMoverHeroCharacter directly; the GAs need
   pawn-agnostic seams or CMC branches. This IS the spike's main measurement.
2. CharacterTrajectoryComponent possibly deprecated in 5.8 → library path fallback (both exist in engine).
3. Feature freeze on feature/NPC during the spike week (or rebase spike) — no divergence.
4. Chalkie foot slide if nav speed ≠ clip speed — set from authored fwd-vel values.
5. Iris untouched (SP PIE unaffected).

## If migration goes ahead — gets DELETED
RM-attribute bridge, DriveRootMotion generations, grab-anchor layered move, custom crouch/jump/RMAction modes,
RM-lite curve-follow, NavMover, warp-drive glue; task #15 (Mover CAS runtime) dies — CAS works native on ACharacter.

Related: [[project_architecture_rationale]] (fired revisit clauses), [[project_contextual_anim_mover_assessment]],
[[project_sp_first_coop_extensible]] (ALWAYS/AVOID lists still apply on CMC).

---

## ★★ 2026-08-19 — THE CONTENT/SPEED MISMATCH (root cause of walk/run/walk churn)

**Measured root motion speeds (authored, cm/s):**

| clip set | Walk | Run | Sprint | Crouch walk |
|---|---|---|---|---|
| **AnimPro** (`/Game/Assets/RTG_AZ/MovementAnimsetPro`, 197 clips) | **172.6** | **375.7** | 641.8 | 168.7 |
| RT_NWP (GASP retargets, what the 27 CMC DBs held) | 206.0 | 514.9 | 720.9 | — |
| `AAZ_CmcCharacterBase` gait constants | 165 | 375 | 585 | 90 |

Our gait speeds were tuned to **AnimPro** (Run matches to 0.2%). Filling the databases with GASP retargets
put every clip 25-37% off the pawn's real speed, and 375 lands almost exactly BETWEEN GASP's walk (206) and
run (515) -> no loop matches, so the search oscillated walk/run and preferred `Stand_GaitTransitions`
(whose clips sweep the middle speeds). Confirmed live: steady 375 playing `Transition_Walk_to_Run`.
Cost-bias tuning (GaitTrans base +0.2 etc., applied 2026-08-19) treats the symptom; the CONTENT is the cause.

**Curve consequence of moving to AnimPro:** RT_NWP carries 6 GASP curves
(`FootSpeed_L/R`, `MoveData_Speed`, `Phase`, `Enable_OrientationWarping`, `Enable_PlayRateWarping`).
AnimPro carries **`contact_l`/`contact_r` on only 2 clips, nothing else**. Effects: `Get_DynamicPlayRate`
-> honest 1.0 no-op; `Get_AOValue` Disable_AO fade -> always full; foot phase dead (`bLeftFootDown` false,
same as the v2 stack — direction still works, foot-phase variants of L/R just never separate); Pose History
`CollectedCurves=["Phase"]` collects nothing; the blend-stack `Enable_OrientationWarping` alpha reads 0.
Curve NAMES are `EditDefaultsOnly` FNames on the anim instance (h:490-503) + `bInvertFootPhase` (h:482) and
`FootPlantedSpeedThreshold` (h:506) -> repointing to contact_l/contact_r is a CDO click, not a rebuild
(contact is a 0/1 flag inverted vs a speed, so threshold 0.5 + bInvertFootPhase=true).

**CHT_v2_CharacterAnimations = the reference** (84 rows, 13 columns, 82 anim refs): 75 AnimPro + 4
`RTG_RM_M_Neutral_Crouch_Idle_Break_v02..v05` (AnimPro has no crouch breaks) + 3 `AZ_Bump_*` reactions.
v2 chose `_new` variants for every crouch clip. v2 referenced NO sprint and NO gait-transition clips —
**AnimPro has no gait-transition content at all**, which matches GASP 5.8 having no such database either.

**The mapping (validated: 93 clips, 0 missing, 20 databases filled, 7 to clear):**
Idles<-Idle | IdleBreaks<-Idle3/4/6 | TIP<-TurnLt90_Loop,TurnRt90_Loop,TurnLt180,TurnRt180 |
Walk_Loops<-WalkFwd/BwdLoop+StrafeLeft/Right(+45/135) | Walk_Starts<-WalkFwdStart(+90/135/180 L/R),
WalkBwdStart,StrafeLeft/RightStart | Walk_Stops<-WalkFwd/Bwd+StrafeLeft/Right Stop_LU/RU |
Walk_Turns<-WalkArchLoop_L/R | Run_Loops<-RunFwd/Bwd/Lt/RtLoop+RunStrafe45/135 L/R |
Run_Starts<-RunFwdStart(+90/135/180 L/R) | Run_Stops<-RunFwdStop_LU/RU |
Run_Pivots<-RunFwdTurn180_L/R_LU/RU | Run_Turns<-RunArchLoop_L/R | Sprint_Loops<-SprintFwdLoop1 |
StanceTransitions<-Idle2Crouch_new,Crouch2Idle_new | Crouch_Idles<-CrouchLoop_new |
Crouch_IdleBreaks<-RTG_RM crouch breaks v02-v05 | Crouch_TIP<-Crouch_Turn90L/R_new |
Crouch_Walk_Loops/Starts/Stops<-the 8 `_new` clips each.
**CLEAR + ungate (no AnimPro content):** Stand_Walk_Pivots, Sprint_Starts/Stops/Turns,
**Stand_GaitTransitions**, Crouch_Walk_Pivots, Crouch_Walk_Turns.
Loop flag must be forced TRUE on: SprintFwdLoop1, Walk/RunArchLoop_L/R (authored unflagged).

**Open tuning decisions (NOT applied):** SprintSpeed 585 -> 642 to match the one sprint loop;
CrouchSpeed 90 -> ~169 (AnimPro crouch walk is authored at 169, so 90 = 47% foot slide);
WalkSpeed 165 -> 172 (minor). All three are `EditDefaultsOnly` on `AAZ_CmcCharacterBase` — but
**BP_CMC_Hero already serialized its own CDO copy**, so change them on the BP, not in C++ (doctrine rule 1).

**Editor hazard hit:** `EditorAssetLibrary.save_loaded_asset` on an AnimSequence hung the game thread at
"Generating thumbnails" (AnimPro_SprintFwdLoop1) — every subsequent Python call timed out while the editor
stayed "connected". Suspect a modal behind the main window. Set loop flags via the asset editor instead,
or save with a different path.
