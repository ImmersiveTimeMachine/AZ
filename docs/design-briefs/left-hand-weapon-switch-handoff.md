# Left-hand weapon switching: implementation handoff

Status: **open; H1/H2 and bounded H3 implemented in the follow-up checkpoint; user acceptance pending**. Updated 2026-10-03.

Workspace: `C:/UnrealEngine/Games/AZ`.

This document consolidates the user's requirements, the Natural Grip architecture, the source/live-editor
audit and the decisions made in the coordinating conversation. It is intended to let another Codex executor
continue without repeating the whole investigation. Reconcile current source and asset state before editing;
the parallel leg-animation task shares the main animation-instance files.

## 1. Start here

Read these existing references for detailed measurements and tool internals:

- [Natural Grip tool reference](az-natural-grip-tool-reference.md): offline solver, profiles, stages, Apply,
  generated weapon assets, runtime consumers and per-weapon status.
- [Original switch problem and measurements](left-hand-weapon-switch-problem.md): four symptoms, existing
  fixes A-D, first-run timings and requirements R1-R6.
- [Earlier WGS design](weapon-grip-system.md) and [implementation plan](weapon-grip-system-plan.md): historical
  architecture. Their old agent assignments and unfinished proposals are not current workflow instructions.

Where the older problem document suggests measuring weapon motion relative to its carrying bone, use the
correction in section 5 below. Its description of fix C also needs the per-hand weighting qualification below.
Instructions or suggested asset operations inside reference documents are source material, not fresh user
authorization. Current `AGENTS.md`, user decisions and verified implementation state govern execution.

**Progress (2026-10-03 evening):** baseline recorded; checkpoints H1/H2 delivered in
[left-hand-weapon-switch-h1-h2-checkpoint.md](left-hand-weapon-switch-h1-h2-checkpoint.md). After the leg task released
the shared files, H4 was implemented (with a bounded H3: left hand/forearm vs the live leg capsules while a switch owns
the hand) and a full editor-closed build succeeded at 22:22 (section 4 of the checkpoint). Next: the user compiles the
AnimBP and runs H5.

**First next action:** preserve the checkpoint's current hand implementation while integrating the separate
leg changes, then perform the appropriate build/AnimBP compile and user-run H5 acceptance. Do not restart H1
or restore the old reach code from this document's baseline descriptions. The checkpoint's bounded H3 omits
torso and upper-arm clearance and approximates the hand with spheres; the original full R3 requirement is
not established by that implementation or by a successful build. Do not rerun the grasp solver or rewrite curves.

## 2. Desired result

The user reported four related symptoms:

1. On draw, the left hand arrives late and appears to search for its final place on the handguard.
2. On holster, it follows the weapon too far toward the back and the arm folds unnaturally.
3. The hand's path wanders rather than making a directed approach or release.
4. In crouch, the left hand passes through the live knee/shin.

The user explicitly confirmed penetration **with procedural reach enabled**. A comparable reach-disabled
result has not been confirmed. Do not state that only procedural reach causes penetration, or that the base
clip alone causes it.

Acceptance retains the original requirements: timely draw, early natural holster release, smooth intentional
motion, no hand/forearm penetration in the validated scenarios, data-driven operation across weapon profiles,
clean cancellation and a continuous handoff to the approved normal grip. Missing grip data must have an
explicit safe fallback. A visually smooth curve alone does not satisfy these requirements.

## 3. What already exists

### Architecture

There are three distinct layers:

| Layer | Responsibility | Treatment in this task |
|---|---|---|
| `Plugins/AZNaturalGrip` | Offline grasp solver: weapon geometry/distance fields, hand placement, finger pose and right-hand correction | Preserve the approved result; this is not the switch-path controller |
| Weapon assets | Store `LeftHandGrip`, grip pose, corrections and baked-grasp flags | Preserve approved M16 assets |
| Runtime animation | Select the live weapon, choose switch ownership/weights, move the left hand, solve arms and apply clearance | Correct this layer |

The M16 **P4 power grasp was applied and approved by the user**. It is the destination constraint, not a target
to recompute whenever a switch looks wrong. The current result is
`Saved/NaturalGrip/NGP_M16_Left/support_final.json`; the earlier result is
`Saved/NaturalGrip/NGP_M16_Left/support_final_L2_before_power.json`. Both exist at the handoff check. Verified
backup directories under `C:/UnrealEngine/Games/AZ_Backups/` are `2026-10-03_150822_NGP_M16_Left`,
`2026-10-03_151030_NGP_M16_Left` and `2026-10-03_152158_NGP_M16_Left`. The last directory's `manifest.json`
identifies the socket owner and protected packages. Existence is not a fresh restore validation or permission to Apply.

### A-D at the original handoff baseline

| Item | Existing state | Qualification |
|---|---|---|
| A: select the physically carried incoming weapon | Implemented in grip gathering | Equipment's committed selection still changes at the end of draw; preserve the incoming-weapon handling |
| B: rewrite M16 left-grip approach curves | Script and historical 28-clip dry run exist | **Not applied**; protected clips require specific authorization. Draw work moved to D |
| C: sample the switch clip's own left-grip curve | Implemented as a fallback | It currently changes the shared master grip alpha; it is not a clean independent left-hand override |
| D: procedural switch reach | Implemented and previously built; first user run exposed remaining defects | Existing functionality to revise, not a new feature already solved by this handoff |

The initial coordinating investigation added **no new hand/weapon fix** and verified the baseline above.
The subsequent external implementation is recorded in the H1/H2 checkpoint; it supersedes that baseline's
reach/weighting behavior. Separate leg changes in shared files must still be preserved during integration.

### Source map

All paths below are relative to the workspace root above; locate by symbol because concurrent changes move line numbers.

| Source | Relevant responsibility |
|---|---|
| `Source/AZ/Private/Animation/AZ_MoverAnimInstance.cpp` | Grip gathering and presentation snapshot; now calls `UpdateWeaponSwitchReach` |
| `Source/AZ/Private/Animation/AZ_MoverAnimInstance_WeaponReach.cpp` | Follow-up extraction: switch montage/curve selection, reach plan evaluation, `BuildWeaponReachPlan`, reach console variable |
| `Source/AZ/Public/Animation/AZ_MoverAnimInstance.h` | Published reach inputs, `FWeaponReachPlan`, plan cache |
| `Source/AZ/Private/Animation/AnimNode_AZWeaponGrip.cpp` and public header | Per-hand alpha, reach target, left-arm IK, grip fingers |
| `Source/AZ/Private/Animation/AnimNode_AZWeaponBodyClearance.cpp` and public header | Existing body proxy/weapon clearance and right-arm logic |
| `Source/AZ/Private/Equipment/Components/AZ_Inv_CommonUI_EquipmentComponent.cpp` and public header | Transition/phase IDs, source/target weapon and phase authority |
| `Source/AZ/Private/Weapon/AZ_Weapon.cpp` and public header | Equipment animation montage, instance, owner and action identity |
| `Source/AZ/Public/Animation/AZ_WeaponAnimationProfile.h` | Weapon switch clips, attachment timing and presentation data |
| `Tools/wgs/set_left_curve_m16.py` | Protected-curve proposal; do not execute its writing path as part of this plan |
| `Tools/wgs/sample_clips.py`, `Tools/wgs/natgrip/dump_hand_live.py` | Existing diagnostic references; review their current coordinate/attachment assumptions before reuse |

## 4. Measurements and live contracts

### Timing evidence

The four M16 clips use the following short names under the rifle animation content:

| Clip | Duration | Attach time | Existing planned arrival |
|---|---:|---:|---:|
| `AZ_RTG_MH_W2_Stand_Rlx_Equip_Back_Get_From_MOB` | 1.533 s | 0.567 s | 1.483 s |
| `AZ_RTG_MH_W2_Crouch_Equip_Back_Get_From_MOB` | 1.900 s | 0.533 s | 1.767 s |
| `AZ_RTG_MH_W2_Stand_Rlx_Equip_Back_Return_To_MOB` | 1.433 s | 0.667 s | 0.800 s |
| `AZ_RTG_MH_W2_Crouch_Equip_Back_Return_To_MOB` | 1.767 s | 0.567 s | 0.800 s |

Historical sampling places the visible draw meeting region around **1.0-1.05 s standing** and **1.2-1.25 s
crouched**. These are validation references, not universal hard-coded arrival times. The authored settled hand
can remain about **8-10 cm and 34 degrees** from the approved solved grip because the clip was authored with a
different hold. Requiring zero distance to the solved grasp is therefore an invalid arrival criterion.

The current crouch holster holds a chest-relative target from **0.800 to 1.533 s**, while the torso continues
moving relative to the live legs. This is a concrete undesirable hold, not evidence that chest-relative
coordinates are always inappropriate.

The original detailed run has rotated to
`C:/UnrealEngine/Games/AZ/Saved/Logs/AZ-backup-2026.10.03-23.42.47.log`.
The four historical `[Reach]` plans are at lines 5315, 5355, 6694 and 7120; the reach-debug commands are at
7942-7943. At the handoff check, `Saved/Logs/AZ.log` also contained the same plans at 5735, 5805, 6082 and 6395.
Logs can rotate again: identify a run by content and session boundaries, not a bare line number alone.

### Live graph and asset findings

The live findings below come from the completed B2 inspection; the handoff pass checked source and file
evidence without repeating the live editor audit.

- Active AnimBP: `/Game/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC`.
- The inspected relevant order is foot placement, body clearance, weapon grip, paired hands, then pose history.
  The switch montage uses the upper-body `RifleFire` slot; lower-body crouch/locomotion can differ from the
  switch clip's authored legs.
- Both inspected M16 Relaxed/Aim socket settings use `RightHandM16Socket` on **`az_weapon_r`**. Runtime grip
  gathering agrees. Do not substitute the older diagnostic assumption that the hero attachment is `hand_r`.
  The weapon skeleton's `LeftHandGrip` parent is a separate relationship.
- Paired-hands normally needs a valid matching action/partner/contact window; an ordinary weapon switch does
  not establish that action. Its release tail may overlap a transition, so do not assume its weight is always zero.
- `ABP_Body_PostProcess` remains enabled. Head movement and RigLogic follow; the inspected head rig can also
  write root. DNA-specific joint influence was not exhaustively established. Final displayed-pose verification
  is required, but no postprocess rewrite is justified by current evidence.
- `PHYS_MHC_Hero` already contains thigh and calf geometry, including **two capsules per calf**. The existing
  clearance whitelist omits calves. The left hand is represented by a **box**, not a capsule; a capsule-only
  copy misses it. Body shape orientation, half-length conventions and scale must be respected.

These are audit snapshots. Recheck only contracts that a later change invalidates; do not restart the full audit.

Exact asset locations confirmed for continuation:

| Asset | Unreal package |
|---|---|
| Hero body | `/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh` |
| Body Physics Asset | `/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/PHYS_MHC_Hero` |
| Rifle animation profile | `/Game/AZ/Blueprints/Animation/MotionMatching/RifleP01/DA_WeaponAnim_P01` |
| Rifle Blueprint | `/Game/AZ/Blueprints/Weapon/AZ_BP_Rifle` |
| Approved grip pose | `/Game/AZ/Blueprints/Animation/WeaponGrip/AS_Grip_M16` |
| Weapon mesh | `/Game/AZ/Assets/M16/SKL/M16_Skeleton` |
| Weapon skeleton / socket owner | `/Game/AZ/Assets/M16/SKL/M16_Skeleton_Skeleton` |
| Four switch clips | `/Game/AZ/Assets/M16/Riffle_RTG_MH/` plus the exact clip names above |

Retain the actual `Riffle` spelling. `P01` in the profile name does not mean the equipped weapon is a pistol.
The verified socket-owner manifest records `LeftHandGrip` on the **weapon's** `hand_r`; the hero's carrying
bone is **`az_weapon_r`**. These are different skeletons and must not be conflated.

## 5. Conclusions and corrections to earlier proposals

This table records the research baseline and why H1/H2 were needed. Consult the implementation checkpoint
before treating any row as an unresolved current-code defect.

| Finding | Consequence for the fix |
|---|---|
| The quintic/Hermite and minimum-jerk basis is reasonable; timing, targets and constraints are wrong | Keep useful interpolation math; change semantic inputs and ownership |
| The current chest-speed detector waits too late on draw | Validate an authored hand/weapon engagement signal that accounts for the calibrated grasp offset |
| A weapon's transform relative to its rigid carrying bone becomes constant after attachment/blend | **Do not use that signal alone as arrival of the weapon in front of the body** |
| Holster targets the clip's end pose early, then holds it | Meet the appropriate current-time clip trajectory and relinquish reach without the artificial hold |
| Gathering scans active montages rather than identifying the exact equipment phase's montage | Use the equipment-owned transition/phase/animation identity; unrelated montages must not drive the reach |
| C writes `WeaponGripAlpha`; the grip node still applies pose-side hand curves | Define independent left-hand switch weighting while preserving right-hand and body-clearance ownership |
| The reach is planned from raw clip samples while the displayed pose is blended | Validate the entry/handoff against the evaluated pose and phase ownership; do not assume raw clip and live pose agree |
| Existing clearance handles stock/body and right-arm/weapon constraints | Reuse suitable geometric math, but implement a left-arm/body constraint explicitly |
| Moving only the hand target does not protect the forearm or elbow | Validate the whole reachable arm chain, hand volume and transitions through time |
| Cache keys currently center on clip and weapon class | Include or invalidate on relevant profile, skeleton/mesh, socket/grip and generation changes; avoid permanently caching an invalid plan |
| Cancellation/full-body takeover can leave a reach tail | Audit ownership release and clean fallback at interruption boundaries |

The old suggestion that a left-arm fix is simply the existing right-arm logic copied across understates the
work. In particular, current body geometry must be evaluated against the live lower-body pose after foot
placement, and the existing shared master alpha must not disable unrelated right-side work.

`az.Weapon.Reach 0` is **not raw animation with all IK disabled**: fix C and normal grip/clearance remain.
Modes 0/1 comparisons must record the actual weights and ownership. A smooth interpolation function also does
not by itself prove continuous pose/velocity at a change of owner.

## 6. Protected work and coordination

- Preserve the approved M16 P4 grasp, `LeftHandGrip`, grip fingers, baked flags and right-hand correction.
- Preserve source animation motion and curves; curve authoring/solver Apply is outside this implementation.
- Preserve gameplay equipment transactions, selection commit, inventory behavior and weapon attachment events.
  Animation consumes a read-only presentation snapshot rather than becoming gameplay authority.
- Do not alter paired-hands, MetaHuman postprocess or the Physics Asset speculatively.
- Leg work currently owns `AZ_MoverAnimInstance.h`, `.cpp` and `_Procedural.cpp`. It adds support-relative
  movement sampling for stationary turn steps and investigates walking-turn crossover. Coordinate a file
  handoff or integrate after its reviewed checkpoint; never overwrite these changes with an older hand baseline.
  The [shared leg-turning plan](leg-turning-shared-plan.md) records that work's current scope and review gates.
- One owner performs shared source/asset writes. User handles mouse/keyboard and PIE. No new automated tests
  are authorized. Builds and existing diagnostics follow the current project skills and authorization rules.

## 7. Implementation plan and intermediate checkpoints

This is the original continuation plan. H1/H2 and the bounded H3/H4 described in the follow-up checkpoint have
since been implemented by the user's external Claude agent; H5 and remaining coverage still require acceptance.
Retain these gates as the requirements and review history, not a request to implement completed stages again.
Follow the current user-assigned ownership and model/effort rules. Existing evidence gathering can proceed
independently of protected source writes.

### H1 — Exact transition ownership and per-hand weights

Inputs: equipment transition/phase state, montage instance, phase clip/slot/rate, physically presented weapon,
existing grip-node weighting and cancellation rules.

Define a read-only cosmetic snapshot with unambiguous transition/phase/weapon/animation identity and actual
phase progress. Reuse the owning equipment/weapon data instead of selecting the last active montage. Define
left-hand switch weight separately from the master and right-hand weights. Specify missing/expired snapshot
behavior and ownership during blends, interrupted switches and full-body actions.

**Checkpoint evidence:** source-to-consumer field map, lifetime/thread ownership, weighting equations/table,
and affected files. Acceptance: unrelated montages cannot hijack reach; left-hand release cannot silently
disable right-hand grip/clearance; equipment remains authoritative. Review before dependent path changes.

### H2 — Revalidated draw arrival and holster handoff

Sample the current four clips using the verified hero socket/bone and current weapon grip transforms. Reuse
existing readers where correct; do not trust an older dump that assumed another attachment bone. Measure
authored hand-to-weapon convergence relative to its terminal authored hold, then account for the approved
grasp correction. Evaluate position and orientation separately so a late orientation settling threshold does
not recreate the current delay. Validate the chosen signal rather than baking the historical times above.

For holster, target the clip trajectory at the chosen handoff time, not its end pose brought forward in time.
Define position/velocity and rotation continuity into the live blended pose, removing the chest-fixed hold.
Keep release from the live grip early enough to avoid following the rifle behind the back. Avoid an initial
velocity pointing away from a feasible path; a magnitude cap alone is not an anatomical constraint.

**Checkpoint evidence:** four timing/trajectory summaries, coordinate definitions, entry/meeting/handoff
conditions, cache invalidation and interruption behavior. Acceptance: no unexplained late meeting, no fixed
holster plateau and no invalid rigid-attachment timing signal. Review before applying the runtime path change.

### H3 — Clearance for the live left arm

Design body proxy extraction and evaluation using the current pose at the appropriate animation stage.
Cover pelvis/torso, thighs and both calf shapes; represent the hand box and arm segments appropriately.
Use consistent component-space transforms, actual shape rotation/scale and reachable shoulder-elbow-hand
geometry. Check wrist, elbow and forearm, plus movement between successive targets where needed to prevent
crossing an obstacle between otherwise valid endpoints. Preserve stable elbow bend and avoid frame-to-frame
oscillation between competing corrections.

Decide how collision constraints remain effective during reach-to-clip handoff if the live upper-body/lower-body
combination itself intersects. A reach-only solver that fades to an intersecting pose does not meet R3.

**Checkpoint evidence:** exact proxy shapes/bones, node ordering, thread-safe pose/data ownership, bounded
solver/fallback design and continuity checks. If the fixed grip is unreachable or lies inside a body obstacle,
report the competing constraints and propose a coordinated fallback; do not claim both exact grip and zero
penetration are simultaneously achievable in that pose. Proxy clearance is not proof of final skin/clothing
clearance; preserve final visual verification.

### H4 — Integration and build

Integrate the reviewed ownership, weighting, path and clearance changes while preserving concurrent leg work.
Ensure cache invalidation, stale transition rejection and fallback paths are bounded and observable. Use
existing reach diagnostics first; add only needed debug-gated evidence. Do not run the protected curve-writing
script or solver Apply.

Review the diff and compile through the project workflow. Reflected/layout changes require the appropriate
full editor-close build/restart; do not substitute an unsafe Live Coding path. Do not report a successful
build without the authoritative compiler result. This document does not claim any build of these new fixes.

### H5 — User-run acceptance

After a successful appropriate build, the user performs the relevant checks; no automatic PIE/test launch.

| Scenario | Check |
|---|---|
| Standing draw and holster | Directed arrival, early release, no search, no arm following the gun behind the back |
| Crouched draw and holster | Hand, elbow and forearm clear the live thighs/knees/shins and torso |
| Switching while moving/turning | Targets follow the live pose without a chest-fixed penetration or unstable elbow |
| Interrupted/replaced switch; full-body action takeover | Correct owner, no stale target, no retained invalid tail |
| End of draw and subsequent relaxed/aim/fire | Continuous handoff and unchanged approved M16 grip/right-hand behavior |
| Missing grip data / another supported profile | Explicit safe fallback; no M16-specific constants masquerading as a general solution |

Use existing `az.Weapon.Reach` modes 0, 1 and 2 as appropriate: 2 enables the reach and per-frame logging.
Record the exact scenario, phase/weapon/montage identity, actual weights and relevant pose stage. Compare
matching phases rather than arbitrary screenshots. Preserve and report the user's observations of the final
rendered MetaHuman, including postprocess effects. Regress the existing foot fix when shared code is integrated.

Completion requires both relevant build evidence and the user's visual acceptance; neither a finished
research chat nor a mathematically smooth target path closes this issue.

## 8. Remaining unknowns

- Comparable reach-off crouch penetration and the respective contributions of the clip layering and procedural path.
- User acceptance of the implemented 8 cm arrival rule and its applicability to other supported weapon clips.
- Live-pose/velocity handoff behavior and the difference between the bounded implemented clearance and full R3 coverage.
- Final MetaHuman/postprocess and skin/clothing clearance in the user's scenarios.
- Current shared-file integration point after the concurrent leg-animation changes.

These are targeted checkpoints. The existing offline solver, approved grasp and completed graph/geometry
audit do not need to be rediscovered to begin H1.
