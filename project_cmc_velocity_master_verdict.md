---
name: project-cmc-velocity-master-verdict
description: "★★★ THE ARCHITECTURE VERDICT (2026-08-26, commit 9362c22): CMC owns the capsule 100% during locomotion; montage RM retires to committed actions; instant capsule yaw + OffsetRootBone carries visual debt; state-gated MM; distance-to-plant stops. Three independent AI consultations converged. Plus the AnimPro census: 50/199 clips wired, full 8-way sets unused on disk. START HERE for the A-retest build."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-08-26T01:37:16.240Z
---

# The velocity-master verdict — locomotion architecture settled (2026-08-26, `9362c22`)

Three independent AI consultations + our own analysis converged unanimously after the RM-montage
chapter (commits `343d553` → `9362c22`) was made deterministic and STILL judged "works but not
smooth, far from the initial image". The structural diagnosis all four reached:

**Capsule authority transfer is itself the seam.** CMC → montage RM → CMC ping-pong at every
start/stop/cancel = a seam per boundary; interruptibility multiplies boundaries. We fixed ~9 seams,
each measured and real, and the structure kept producing more. GASP is smooth because the capsule
has ONE owner 100% of the time and the mesh absorbs disagreement procedurally.

## The architecture (settled, do not relitigate without new evidence)

| system / event | ownership model |
|---|---|
| loops + steered turns | velocity-driven CMC + continuous MM trajectory |
| starts | velocity CMC + state-gated MM, latched direction bucket |
| stops | analytical braking (existing contract) + **distance-to-plant** anim phase |
| hard pivots ≥135° ≥200cm/s | state-gated MM + OffsetRootBone angular accumulation |
| shallower redirects 60–120° | NO discrete clip — steered loop + OrientationWarping |
| TIP | mesh-driven yaw offset via OffsetRootBone |
| combat / traversal / staggers | montage RM, committed by design (unchanged) |

**The enabling conditions, all three consultants independently:**
1. **Instant (or >2000°/s) capsule yaw.** Finite rates (our 165–400) carve r=v/ω arcs no clip
   depicts — THAT overwhelmed the warpers, not MM. The visible mesh keeps weight via OffsetRootBone
   **Accumulate** absorbing the snap and paying it back. Accumulate + instant yaw is GASP's pairing;
   Accumulate + finite yaw was our broken pairing (the "never converges" finding was the pairing's
   fault). ⚠ This reverses a tuning direction the user drove — body-rotation feel lives in the MESH
   layer now, not the capsule.
2. **Fix the `Get_TrajectoryTurnAngle` mis-port**: must be SignedAngle2D(Trj_FutureVelocity,
   Velocity) — ours compares Acceleration vs Velocity and caps ~33°.
3. **Per-clip metadata is load-bearing**: Enable_Warping (missing = warp silently OFF), BranchIn
   notifies for PSD membership, foot-contact curves. Assume commercial clips have NONE until
   generated and verified.
4. **Movement must never read live anim selection** (prediction nondeterminism, saved-move replay);
   distance matching is clip-follows-capsule, so collision/prediction/floors stay authoritative.
5. Correction hierarchy: distance matching/state gating ~80% → stride warp + play-rate (±15%) ~15%
   → foot-IK lock the last 5% (<10cm only; more = knee hyperextension and release pops).

**Scoped quality contracts** (zero-slide is mathematically overconstrained globally — scope it):
starts nominal <50cm/s within ±30° of latched bucket, hard redirect >60° aborts to loop; stops
in-band = perfect via distance matching, re-input breaks the plant instantly; pivots only ≥135°;
TIP <5cm/s. (Maps ~1:1 onto our shipped gate values.)

## Why C (universal curve-follow) was rejected

Double friction/braking; saved-move replay can't sample anim curves (rubber-banding); collision
invalidates clip displacement (wall = forced slide); trajectory predictor doesn't know the profile
→ MM selection feedback loop; blend regions have no single correct curve. **The stop contract
survives** because it's movement-owned, analytical, latched at entry — C's safe restricted form.

## What survives from the RM chapter (nothing wasted)

- **Toggle**: `bRootMotionStarts=false` / `bRootMotionStops=false` → `OwnsRootMotionStarts()`
  returns false → MM Starts+Pivots pools return AUTOMATICALLY (D1 strips are ownership-derived).
- **State-gating infra** (D1 strips, FAZ_DatabaseGate, snapshots) = exactly the consultants'
  "state-gated MM for sparse content". Keeps.
- **Stop contract** (−1..−10cm) = the movement half of distance matching. Anim half: drive stop
  clip phase from remaining-distance-to-plant (`AnimationLocomotionLibrary` plugin already loaded).
- Engine facts hard-won: `GetCurrentActiveMontage()` filters `IsStopped()` instances
  (AnimMontage.h:535) so its true→false edge IS the release frame, `mtg=0` through blend-out;
  `EPoseSearchInterruptMode` never gates searching, only continuing-pose survival; inertial montage
  stop forces blend time 0 + one FInertializationRequest (AnimMontage.cpp:1576-1586); an
  Inertialization node only absorbs discontinuities that flow THROUGH it (ours: Slot →
  Inertialization → OffsetRootBone, added `9362c22`).

## ★★ The AnimPro census — the "sparse content" premise is half false

**50 of 199 clips wired.** Unused ON DISK (all bone-sampled 2026-08-26):
- **Walk 8-way**: Fwd/Bwd/StrafeL/R/±45/±135 loops — ALL 1.00s, ALL 172.5 cm/s (coherent set).
- **Run 8-way**: fwd-family 375, bwd-family ~229 cm/s.
- **Strafe starts/stops** (L/R + 4 stops), **WalkBwd start/stops**.
- **`WalkFwdTurn180_L/R` exist while `PSD_AZ_Stand_Walk_Pivots` is EMPTY** (the 919f revert casualty).
- **Crouch directional set** (±45/135/Lt/Rt + starts + 6 stops, `_new`).
- **9-pose AO grid** (Idle_U/D/L/R/LU/RU/LD/RD/CC) — the unbuilt aim offset.
- `/Game/Assets/RM_Movement` (RTG_RM_*, 216) = SAME content re-exported (travel within 1%);
  its 28 uniques worth importing: 18 idle breaks, jump in-air clips, neutral crouch idles.

**Genuinely missing** (authoring list, in value order): 90–135° pivots, second stop-speed tier,
sprint set (one loop only), gait transitions, plant-break transitions.

## ★★ ADDENDUM 2026-08-26 late: v2 IS the proof — the retest is a PARITY PORT, not research

User observation: the v2 Mover hero "works perfectly". Census confirms it is Option A already
built, on the SAME AnimPro clips (same skeleton, zero retarget):
- capsule velocity-driven ALWAYS (no locomotion RM anywhere) — one owner;
- discrete events via SM + CHT + BlendStack (state decides, chooser picks, blend stack plays with
  dynamic play rate) — NOT montages, NOT raw-MM;
- MM over the FULL 8-way sets: PSD_v2_StrafeWalk/StrafeRun/StrafeCrouch (8 clips each),
  PSD_v2_Loco_Loops/Jump; the full GASP warp stack.
The spike never decided against porting this — it DRIFTED (minimal ABP → impulse → RM montages).

Only two pawn-touching seams block "exactly the same on CMC":
1. trajectory source — UCharacterTrajectoryComponent (already on BP_CMC_Hero) or the 5.8 library
   path; the AZ_AnimInstance.cpp:206/688/878 dual-path edit ALREADY SPECIFIED in the P0 plan;
2. movement-state reads (mode/falling/speed/accel) — CMC equivalents exist, UAZ_CmcAnimInstance
   computes most of the contract already.
Plus one config read-off: the v2 Mover pawn's rotation semantics, replicated on CMC.

⇒ ★★ USER RULE (2026-08-26, emphatic, reaffirmed twice): **NEVER assign AZ_ABP_MoverAnimInstance to
the CMC hero.** "CMC for CMC" — the Mover ABP is owned by the Mover pawn and must not be shared or
touched; BP_CMC_Hero keeps AZ_ABP_CmcAnimInstance. Parity therefore means porting the v2
ARCHITECTURE (SM+CHT+BlendStack selection, warp stack, 8-way DBs, velocity capsule) INTO
UAZ_CmcAnimInstance / AZ_ABP_CmcAnimInstance — replicate the design, never the asset. The dual-path
edits to AZ_MoverAnimInstance.cpp were reverted (git checkout, work preserved only in
scratchpad/v2_parity_port_report.md — the seam inventory, τ→rate table, and RootMotionMode findings
there remain valid inputs for the CMC-native port). BP_CMC_Hero restored: AZ_ABP_CmcAnimInstance_C,
bRootMotionStarts/Stops=True, GroundedRotationRateYaw=180. The 10 jump clips' bForceRootLock=True
was KEPT (proven Mover-inert: extraction resets the root when bEnableRootMotion, AnimSequence.cpp:1862).

## ★★★ PHASE 1+2a LANDED (commit `fde7c55`, 2026-08-26) — the in-place port, measured

Executed exactly the rule above: NO new class, NO duplicated asset. `UAZ_CmcAnimInstance` /
`AZ_ABP_CmcAnimInstance` upgraded in place; every Mover file/asset/pawn untouched (verified via git
status before commit — two Mover .uasset files showed spurious byte diffs from PIE/an abandoned
dual-path experiment, reverted to HEAD, excluded from the commit).

**What changed:**
- `UAZ_LocomotionStateMachine` (shared, backend-agnostic) is now the SELECTION OWNER for CMC — ticked
  GAME THREAD ONLY. ⚠ Caught in review: the first draft's defensive re-create called `NewObject` from
  the worker thread (`Update_Logic`) — a latent crash on every Live-Coding re-instance. Fixed: worker
  early-outs on null, only `NativeUpdateAnimation` allocates.
- Old condition-window MM gates (start/pivot/TIP speed-angle windows) RETIRED — SM decides once at
  the event edge; the tag-based pool filter reads SM state instead. ⚠ Also caught in review: the
  start-vs-pivot split used TWO DIFFERENT thresholds (135° in the post-filter vs ~157.5° bucket
  boundary in the gate config) — a live disagreement band that could empty the pool. Aligned both to
  the bucket boundary.
- `bRootMotionStarts/Stops` default OFF. Rotation: Mover's spring-damper facing-time law (τ 0.2 idle /
  0.4→0.8 moving / camera-snap shortened toward 180°) ported to CMC's per-frame `RotationRate.Yaw`,
  all τ live-tunable.
- `PSD_AZ_Stand_Walk_Pivots` filled with `WalkFwdTurn180_L/R`. ⚠ Scripted `add_sequence_to_database`
  entries left the search index PERMANENTLY UNBUILT — 373 silently-skipped searches per PIE session,
  invisible in any single-frame check. Root cause: scripted property adds don't fire the full
  PostEditChange notify chain the DDC rebuild needs. Fix: delete + manually drag the clips into the
  asset editor (indexes immediately, visible entry count) — not a workaround, the actual fix.
- All `AZ_Cmc*` headers/cpps stripped of comments (user request) — Sonnet 5, token-verified
  code-identical, zero lint errors. Reasoning history lives in memory + prior commits only now.

**Measured (fresh PIE, [CmcSel]/[CmcRatio]):** selection costs 0.04-0.62 (was 1-8). Straight
loops/starts/stops: capsule/clip ratio ~1.00 GREEN. Reversals now correctly elect the pivot via the
SM edge (was: no pivot content → forward loop → ratio 3.7×). **Residual RED, fully characterized**:
pivot windows (ratio 1.24-1.44, spikes to 10, one frame measured 88° mesh-behind-capsule mid-turn)
and off-nominal-entry starts (ratio 2.3-27.6) — both are FIXED-RATE clip playback into a body already
moving. Selection is solved; playback adaptation is not.

**⇒ Phase 2b, next session: CHT + BlendStack dynamic-play-rate path in `AZ_ABP_CmcAnimInstance`.**
This is the one v2 subsystem not yet replicated (`Get_DynamicPlayRate` scales clip rate to actual
capsule speed) and the numbers above are the quantified case for building it — not preference.
Loops stay on MM (proven ~1.00 fidelity); discrete events (starts/pivots/stops) move to chooser→
BlendStack. Read `scratchpad/v2_parity_port_report.md` (seam table, τ table) and
`scratchpad/cmc_v2_inplace_phase1_report.md` (Phase-2 runbook, struct field reference) first.

## The retest roadmap (next session — order matters)

1. **Editor-closed build first** — `9362c22`'s last changes (D1 pivots strip, inertial stop,
   92% stop release, pending edge) were Live-Coding-only in the 2026-08-26 editor session.
2. Content pass: wire walk/run 8-way into Loops DBs, restore walk pivots, strafe/bwd starts+stops
   — every added clip needs Enable_Warping curve + BranchIn notify (batch script; see
   [[feedback-posesearch-branchin-db-sync]]).
3. Flip `bRootMotionStarts/Stops` false; instant capsule yaw; fix turn-angle mis-port; verify
   OffsetRootBone modes + Steering target time + warp alphas.
4. Test matrix: fwd/90/180 starts, both-foot stops, 180 pivots both sides, interrupts at
   20/50/80% — log capsule-vs-clip ratio, ORB debt, warp alphas, planted-foot world error.
5. Then distance-to-plant stops. 6. Author the genuinely-missing list only after measuring.

Related: [[project-cmc-rm-locomotion]] (superseded for locomotion, mechanism keeps for committed
actions), [[project_cmc_curve_driven_turns]] (absorbed: capsule rotation stays instant; clip yaw is
reference, not authority), [[project_mm_state_selection_plan]] (vindicated), [[project_locomotion_quality_standard]]
(quality bar now scoped by contracts), [[project_gasp_cmc_abp_spec]] (density blocker premise revised).
