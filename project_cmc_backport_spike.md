---
name: project_cmc_backport_spike
description: "★ LIVE spike plan (branch spike/cmc-backport from feature/NPC @ 9518095, task #16): CMC back-port — KEEP CHT+SM+BlendStack+MM (decision + evidence inside), resurrect the EXISTING v1 ACharacter hero (AAZ_HeroCharacter — discovery 2026-08-05), 5-phase plan + exit criteria. Read first for any spike work."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-18T02:03:32.286Z
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

### ★ NEXT SESSION STARTS HERE (state as of 2026-08-17 end, pushed `b732e86`)

**C++ for the MM path is COMPLETE and committed.** Batches 1-3 all built green and pushed:
contract · update chain · predicates · node-setting getters · MM node seam. Databases ingested
(27 pools / 332 clips, verified). `AZ_ABP_CmcAnimInstance` exists, correctly parented, defaults flowing.

**Nothing has run yet.** Three things stand between here and first motion, and #1 is the user's:

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
3. **Three arrays on the ABP CDO**: `Databases_Stand`, `Databases_Crouch`, `Databases_Always`
   (the 2 stance transitions).

★ Turn on `bDebugAnim` (Python name `debug_anim`) BEFORE the first PIE. The 1 Hz line now carries
speed, moving/pivot/TIP, direction, foot, all three trajectory velocities, sample count, **selected
database + SearchCost + loop + tag count**. That one line separates "trajectory not built" from
"search found nothing" from "search found something bad" without guessing.

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
