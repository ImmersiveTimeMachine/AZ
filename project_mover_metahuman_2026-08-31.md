---
name: project_mover_metahuman_2026-08-31
description: "★★★ START HERE (2026-08-31): spike VERDICT = stay on Mover + MetaHuman hero. Branch feature/mover-metahuman, the 3 new assets, what is validated vs open, the IK-off decision, and why CMC is parked (with its resume points)."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-31T00:59:46.012Z
---

# Mover + MetaHuman hero — the resume point (2026-08-31, session 5bce0c20)

## The decision
The CMC back-port spike (task #16) reached its verdict: **stay on Mover, put the MetaHuman body on the
Mover hero.** Reasoning recorded in-session: the 3-week spike proved the CMC *port* works (capsule,
prediction, GAS jump, trajectory all fine in ~2 weeks) but the remaining time went into making
Motion Matching select discrete clips — a problem Mover's v2 stack solved by never asking MM to
(CHT picks one-shots with `bUseMM=False`). Mover stays Experimental for all of UE5 (5.8 is the
last planned release; a 5.9 is conditional and maintenance-only; UE6 EA end of 2027), which is a
known, frozen quantity rather than churn. CMC remains the better *ecosystem* (CAS needs
`Cast<ACharacter>`, ragdoll is 3 lines vs the Mover detach fight) — re-evaluate if CAS-driven
finishers become the core loop.

## Where things live
| what | where |
|---|---|
| Mover + MetaHuman work | branch **`feature/mover-metahuman`** off `feature/NPC` @ `9518095` (pre-CMC base) |
| commits | `e813093` body transplant · `ed8cae6` GameMode · `0398375` cloth+face — all made with git plumbing while the editor was open on another branch |
| hero pawn | `Content/AZ/Blueprints/Character/Hero/MHC/AZ_BP_PawnMoverHero_MHC` ← dup of `AZ_BP_PawnMoverHeroCharacter` (the LIVE hero `BP_AZ_GameMode` spawns; NOT `AZ_BP_HeroPawn`) |
| ABP | `Content/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC` ← dup of `AZ_ABP_MoverAnimInstance` |
| GameMode | `Content/AZ/Blueprints/Game/MHC/BP_AZ_GameMode_MHC` (default pawn = the MHC hero; original GM untouched) |
| CMC spike | **parked** at `spike/cmc-backport` @ `210247b` (jump landing commit); CHT spine on `spike/cmc-blendstack-spine` @ `01e0ac3` (has `CHT_CMC_CharacterAnimations`, 99 rows, and `SetDisableReselectionOnDatabase`) |

⚠ The C++ for tonight (see below) was written on `spike/cmc-backport`'s working tree via Live
Coding and is **uncommitted** at the time of writing — `AZ_MoverAnimInstance.cpp`,
`AZ_PawnMoverHeroCharacter.cpp`, `AZ_InfectedAnimInstance.cpp`. It belongs on
`feature/mover-metahuman`. LC patches die on editor restart → CLI build required after any restart.

## Pawn component set (verified from the asset)
`Mesh` = `SKM_MHC_Hero_BodyMesh` (skeleton **metahuman_base_skel**, ABP `AZ_ABP_MoverHero_MHC`),
relLoc Z=-92 / yaw -90 (identical to both existing heroes) · `Face` = `SKM_MHC_Hero_FaceMesh` +
`ABP_Face_C` (own ABP, never leader-posed) · `Cloth_Hoodie/Pants/Boots/Belt/Backpack/Bag` =
SurvivalMan garments, `NoCollision`, **no ABP**, leader-posed to `Mesh` at runtime by
`AZ_PawnMoverHeroCharacter.cpp::WireModularMeshFollowers_Mover` (file-local, called from `BeginPlay`;
log `[MoverMesh] … wired 6 … (1 excluded)` — "wired 0" is the whole diagnosis, see
[[feedback_metahuman_modular_hero]]). Skeleton link = `metahuman_base_skel.CompatibleSkeletons =
[SKEL_SurvivalMan]` (one-directional; lives in gitignored `Content/MetaHumans`, wiped by re-assembly).

## Validated in PIE (2026-08-31)
- Crouch idle twitch / feet-in-floor gone (pawn Tick vs Mover base offset, 03:40); Ctrl while walking
  switches to the crouch loop within a frame and back (context-keyed crouch pool). Diagnostics gated off
  (`bCrouchDiagnostics=false` in AZ_MoverAnimInstance.cpp) — flip it to get [v2 CrouchTrace/CrouchEnd] back.
- Body animates on the Mover ABP; cloth follows after the runtime `SetLeaderPoseComponent(…, true)`.
- Foot-correct landings (`Lfoot=1 → *_RU_Land2Walk`) after the curve-guard fix.
- Loop re-cut thrash gone with real continuity: 02:03 run, index healthy, `[v2 Snap]`=0 over ~100 picks.
- **"Leg plays from the start after the idle jump" = the start->loop SEAM, fixed by the phase lock**
  (R13 in [[feedback_posesearch_mm_mechanism_rules]]): `WalkFwdStart -> WalkFwdLoop entry=0.86 seam=lock
  rem=0.14 blend=0.14` (was entry=1.00 = frame 0), run 0.62/0.77, Land2Walk 0.86, crouch 0.76/0.90; user
  confirmed. Three earlier jump/land patches were aimed at the wrong seam.
- The "second anim-instance stream" in `[v2 Play]` is `AnimationEditorPreviewActor_0` (an open ABP tab),
  not a stray pawn. `BP_CMC_Hero_C_0` in LogTick lines is the CMC GameMode's pawn from an earlier map load.

## Open / not yet validated
1. **The last "clean" run is NOT proof**: `PSD_v2_Loco_Loops` was saved with `bDisableReselection`
   → derived-data rebuild → `LogPoseSearch: … PreCancelled because of PSD_v2_Loco_Loops` → every loop
   search that run returned NOTHING (`cost=FLT_MAX`, 962 `[v2 MMFallback]`). Needs an editor
   restart + CLI build, then a real re-measure: expect `[v2 Snap]`=0, `[v2 MMFallback]`=0, finite costs.
2. Height: MetaHuman body bounds 149 cm vs SKM_SurvivalMan 184 cm against a 90 cm half-height
   capsule — feet-on-ground and stride/foot-slide not yet checked by eye.
3. `LogMover: RootMotionAttr … excessive speed Z Vel ~2500` fires 90–100 ms after EVERY PIE start
   on the MHC pawn (8/8 runs) — a spawn-time root-motion pop, unexplained.
4. Run-land costs: `JumpRun_*_Land2Run` +700…+59,000 on Mover (walk lands +15). Content, not code.
5. Garment LOD bone reduction at distance ([[feedback_leaderpose_lod_bone_reduction]]).
6. `Content/AZ/Blueprints/Character/AZ_MHC_Hero/` (516 MB) is untracked-but-not-ignored; `git add -A`
   would try to commit it without LFS. `Content/MetaHumans` (1.8 GB) is ignored. Decide.
7. ~~Crouch, orientation mode: every loop search is EMPTY~~ **BYPASSED 03:37**: the `LocomotionLoop &&
   Crouching` pool is now `StrafeCrouchDatabase` from context (like strafe), so crouch loops search a real
   index (costs +0.03…+0.11) and Ctrl-while-walking works. The raw-clip BranchIn gap (R14) still exists for
   any row that passes `Crouch_WalkFwd_new` directly — irrelevant while the override stands.
7a. **Crouch idle "twitch" + feet 4 cm in the floor = `UpdateGrabMeshAnchor` fighting Mover's visual base**
   — FIXED + verified 03:40 ([[feedback_mover_visual_component_two_writers]]). NOT the clip, NOT render.
7z. (was) **Crouch, orientation mode: every loop search is EMPTY** — `AnimPro_Crouch_WalkFwd_new` is an explicit
   `PSD_v2_StrafeCrouch` entry with no BranchIn, and the non-strafe crouch row passes the raw clip
   (R14). Strafe-mode crouch (row passes the DB) works. Fix = remove the explicit entry, `AddBranchInNotify
   (clip, PSD_v2_StrafeCrouch, 0, 0)`, save, RESTART (stale index in the live session).
7b. **Crouch mixes TWO incompatible crouch sets** (measured 2026-08-31, pelvis-local pose distance):
   OLD set = `Crouch_WalkFwdStart`, `Crouch_WalkFwdStart90_L/R`, `180_L/R`, `Crouch_WalkFwdLoop` — self-consistent
   (start/pivots end on the OLD loop's frame 0, d≈1) but they START d≈101 from the crouch idle `CrouchLoop_new`
   and END d≈52 from `Crouch_WalkFwd_new`. NEW set = `*_new` (start d=0 from the idle, ends on the NEW loop's
   frame 0, stops leave the NEW loop at d≈0–1). Old vs new loop mean pose distance 65 = different walks.
   CHT_v2 rows 42–45 (old 90/180 pivots) hand into the NEW loop → a 52-unit pop, which the R13 lock now
   crosses in 0.14 s instead of the old 0.5 s smear ("very small shake" after crouch turns). No `_new`
   moving pivots exist (`Crouch_Turn90L/R_new` are in-place). Decision pending: retire rows 42–45 (NEW set
   only) or run orientation-mode crouch entirely on the OLD set (needs a DB/BranchIn for the old loop and
   old-loop-compatible stops, which do not exist).
   R13 gate now also requires the outgoing clip to have been pushed at TransitionToLocomotion — a stop
   re-pressed near its end (`Crouch_WalkFwdStop_LU_new -> loop seam=lock rem=0.22`) was locking an idle
   pose into mid-stride; stops resume by search/fallback only (`GLastPushSMStateByInstance`, LC-static).
8. Idle jump with the stick held lands into `JumpWalk_LU_Land2Walk` (cost +31): CHT_v2 has no row for
   `AnimPro_JumpIdleLand2Walk` and no "launched from idle" discriminator (candidate: c7 bMovingTransition).
9. Walk<->run gait switches inside LocomotionLoop cost +2,400…+13,800 (single-clip pool, trajectory
   mismatch) — entry is still sane (0.43/0.33) but it is a cost contest MM is not built for.

## Decisions taken this session
- **All procedural IK OFF** (user: fights will use a different technique). The only IK on the Mover
  path was the grab hand-IK (2×TwoBoneIK + 2×ModifyBone in anim layer `AdiativeCombatGrabbed`, hero and
  Chalkie), gated by `GrabIKAlpha`. Switch = `static constexpr bool bGrabIKEnabled = false;` at file
  scope in `AZ_MoverAnimInstance.cpp` and `AZ_InfectedAnimInstance.cpp` (LC-safe; promote to a
  UPROPERTY at an editor-closed build). No foot IK exists anywhere on the Mover ABPs.
- Acceleration first pass on the CMC hero only (`InputRampStartScale` .30→.65, `MaxAccelerationBase`
  400→700, `AccelTaperSpeedMin` 300→450) — CMC-side, parked with the spike.
- GASP's `SandboxCharacter_Mover` analysed: same Mover core; worth taking `BP_MovementMode_Slide` (+2
  transitions) and `FoleyEventComponent` later; skip GameplayCamera/VisualOverride; its `Traversing`
  mode is a `FlyingMode` placeholder.

## Instruments now on the Mover spine (`AZ_MoverAnimInstance.cpp`)
`[v2 Pick]` (asset change: SM, useMM, cost, **entry=t/len**, **seam=lock|mm rem= blend=**, Lfoot, justLanded), `[v2 Snap]` (same-clip
time jump, loop-wrap excluded, plus `cont=<asset>@<t>` and `nodeRefIsBlendStack/loopAtSearch`),
`[v2 MMFallback]` (search returned nothing), and a 7-line on-screen HUD behind `bDebugTrajectory`
(ANIM / PICK / state / spd / foot / lean / MONTAGE). Debug carry-overs are file-scope statics
(single-hero assumption). Details and the bug chain they diagnosed: [[feedback_mover_spine_search_continuity]].

Related: [[project_cmc_backport_spike]], [[project_cmc_chooser_spine_landed]],
[[project_cmc_mm_content_verdict]], [[feedback_log_reading_traps]], [[feedback_planted_foot_curve_guard]].
