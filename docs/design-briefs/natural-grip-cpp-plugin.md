# Natural Grip solver: C++ plugin with optimized math (implementation plan)

Status: proposal, 2026-10-02.
- rev. 2: optimized math and libraries added after the user's question.
- rev. 3: direct asset reading and writing back in scope, as originally proposed.

Approved 2026-10-02 ("давай, реализуй").

## 0. Status (2026-10-02)

- **A: done, bit-exact.** The C++ core (`Plugins/AZNaturalGrip/Source/AZNaturalGrip/Private/Core`) reproduces the Python
  solver with max |diff| = 0 on every compared number:
  - primitives: 5 side/weapon settings;
  - support hand: stage 1 (top 400), stage 2 (24), final L2;
  - trigger hand: place_nat (3,763 candidates), rsolve3 N2;
  - geometry: M16 parts, coarse field, both fine fields.
  - Results are identical on 1 and 32 threads. Harness: `Plugins/AZNaturalGrip/Tools/ngtest` (`build.bat all`, then
    `ngtest prims|support|trigger|geometry|fast|jsondiff`).
  - Two Python details had to be copied: CPython >= 3.12 `sum()` over floats is Neumaier-compensated (`ng::PyFloatSum`,
    also inside `QNorm`), and `x ** 2` goes through the CRT `pow`.
- **B: done, changed after measuring.**
  - A local search from a coarse grid (pattern search, NGOptim.h) missed part A's best on the rugged contact scores:
    the index alone was worse on 7-15 of 60 placements even with a 6-degree coarse grid.
  - The shipped part B is the exhaustive grid's best per finger / thumb, polished by the pattern search into continuous
    angles. Per finger it is never worse than A. On 60 placements the whole hand is better on 54, mean +0.9; it is worse on
    6 because the fingers are solved in sequence.
  - "Solve weapon hand" therefore runs A and B and keeps the higher total, so it is never worse than A.
  - The Eigen Levenberg-Marquardt was not used: the scores are hinge / reject rules, not least squares. The index fit
    already is a pattern search (`index_nat.refine`).
- **C: built.**
  - The editor module and panel exist (Tools > AZ Natural Grip): background jobs, cancel, log, parity button, and
    `UAZNaturalGripLibrary.run_stage` for Python.
  - Input = Assets reads the hero hand and the weapon mesh directly (MeshDescription -> DynamicMesh, the
    GeometryScript path); the `inputs` stage checks them against the dumps.
  - The field bake is the exact C++ port of geom.py, not GeometryCore. It already bakes the M16 fields in seconds and is
    bit-identical to the Python fields, so the approximate fast-winding path is not needed.
  - **In-editor test passed (S6).** Profiles `/AZNaturalGrip/Profiles/NGP_M16_Left|Right` exist. The `parity` stage
    passes for both hands with Input = Python files AND Input = Assets. With Assets, the `inputs` stage gives:
    - the hero hand: bones, ref pose, bind pose and 5084 skin vertices, all identical to the dumps;
    - the M16 mesh: 6988 vertices / 13479 triangles -> the same 39 parts;
    - the coarse field and both fine fields: 0 nodes differ.
  - **S7 done:** Apply with backup (`NGApply.cpp`), tested on copies.
- **M16 left-hand power grasp (2026-10-03, approved by the user in PIE).** The rules that made it work:
  - **Force closure:** the arc the hand covers around the handguard, from the finger pads through the palm to the thumb
    pad, is 220-260 degrees. The shortest-arc thumb-vs-finger angle was the bug: a low thumb satisfied it.
  - **Thumb:** it lies along the handguard pointing forward, presses with its pad, both phalanges rest, no curled joints.
  - **Seating:** the palm and the proximal phalanges sit on the handguard; the hand may roll about the handguard axis
    within a forearm-twist limit.
  - Code: `SupportSettings::SetPower`, `SupportPowerStage1` / `SolveSupportPower` (NGSupportHand), harness
    `ngtest power`. Stage 2 takes the best placements per roll angle.
  - Applied with backup `AZ_Backups/2026-10-03_152158_NGP_M16_Left`.

## 1. Goal and scope

**End state:** a C++ editor solver inside the project.
- It reads the assets directly: the skeleton, the hand skin, the weapon mesh, the clips. No JSON export.
- It writes the result itself, after an automatic backup: sockets, the grip pose, the weapon BP flags, the
  `AZ_Grip_R/L` curves.
- A new weapon is solved with one command in seconds instead of hours of scripts.
- JSON remains only for the parity comparison with Python and as an optional debug export.

Three parts, in this order:

- **A. Port.** Port the CURRENT offline Python solver (`Tools/wgs/natgrip`, state of 2026-10-01: M16 right hand N2 and
  left hand L2) to C++ one to one, with the same rules and the same numbers. It lives in a project plugin with a panel
  in the editor. The check is correctness: the results are identical to Python's (parity). There is no speed
  benchmarking (user, 2026-10-02: not needed).
- **B. Optimized math.** Replace the brute-force grid searches with published methods and existing libraries:
  continuous optimization with gradients, finger synergies, a BVH and fast winding numbers for the field. The A port
  stays as the reference ("oracle") that B is tested against.
- **C. Direct pipeline in the editor.** It replaces every dump/apply script:
  - read the hero skeleton, hand skin and weapon mesh;
  - sample the weapon's clips: hold picks, elbows, where the left hand holds the handguard;
  - bake the fields, solve both hands;
  - preview before/after in the level;
  - Apply: backup, then socket, grip pose, BP flags, curves.
  The whole chain runs from one button per weapon profile.

Not in this plan: new grasp rules. The M16 left-hand power grasp comes right after it, written on the B solver.

## 2. Reference results (parity goldens)

These are the existing Python outputs. On 2026-10-02 a re-run of stage 1 and of the fine field reproduced them
exactly (identical lists, field max difference 0.0), so they are valid for the current inputs.

| Stage | Golden |
|---|---|
| Support hand stage 1 (THIN 2, coarse field only) | `Tools/wgs/natgrip/lsolve_stage1.json` (top 400) |
| Support hand stage 2 (top 24, step 8, THIN 2) | `Tools/wgs/natgrip/lsolve_stage2_0.json` |
| Support hand final L2 (THIN 1) | `Tools/wgs/natgrip/lsolve_m16_L2.json` |
| Trigger hand placement (place_nat) | `Tools/wgs/natgrip/pn_all.json` (every candidate), `place_nat.json` |
| Trigger hand solve N2 | `Tools/wgs/natgrip/rsolve3_m16_N2.json` |
| Fields | `Saved/wgs/fields/m16_field.json`, `Saved/wgs/fine_field_left_m16.json`, `fine_field_right_m16.json` |

## 3. Why part B

The solver searches by brute force: it tries every combination on a grid and keeps the best.
- One finger: 17 side angles x 36 MCP x 35 PIP x 4 DIP ratios = about 86,000 poses, each checked with all its skin
  vertices against the field.
- The trigger-hand placement: 51,450 placements x 1,302 index shapes = 67 million combinations.
- The field: every node of the narrow band sums the solid angle of every triangle of the part (winding number,
  O(triangles) per point).

Published methods find the same optimum with a few hundred checks:
- the answer is continuous instead of snapped to a 3-degree grid;
- the search space can grow, for example the wider rotation the power grasp needs;
- the same optimizer can adapt the grasp per frame in the game later (section 11).

## 4. Libraries and published methods

| What | Where it is used | Source and status |
|---|---|---|
| **Eigen 3.4** (linear algebra; `unsupported`: `AutoDiff`, `LevenbergMarquardt`) | B: the optimizer, Jacobians, PCA of the synergies | Already in the engine (`Engine/Source/ThirdParty/Eigen`), header-only, MPL2. Works both in the harness and in the plugin; nothing to download. |
| **Levenberg-Marquardt** (least squares with a damped Gauss-Newton step) | B: fingers, thumb, placement as one energy | Eigen's implementation; joint limits by clamping to the range. |
| **Analytic Jacobians**: a skin point on a rotating joint moves by `axis x (p - pivot)`; the field gradient comes straight from the trilinear cell | B: gradients at the price of one evaluation | Standard IK and SDF math; `AutoDiff` as a cross-check. |
| **Finger synergies / eigengrasps** (Santello 1998, GraspIt!) | B: global search in 1-2 parameters per finger instead of 4 | PCA over the finger poses of OUR clips (about 1,000 M16 and Winchester clips). Not MANO: its license forbids commercial use. |
| **Grasp energy** (DexGraspNet, ContactOpt): penetration + contact gap + joint limits + plausibility, minimized by gradient | B: our rules written as one energy | Only the formulation is taken. Our rules are already that list: rest, no penetration, DIP ~0.65 PIP, no hook, adjacency, wrap, thumb, palm, wrist bend. |
| **BVH nearest triangle** + **Fast Winding Numbers** (Barill et al. 2018) | Field bake: exact distance and inside test in O(log n) instead of O(n) | `GeometryCore` in the engine: `TMeshAABBTree3`, `TFastWindingTree`, `FMeshConnectedComponents`. The same algorithms are in libigl; the engine copy needs no new dependency. |
| Ceres Solver (Google) | Only if Eigen's LM turns out too weak | Needs a download and a build: ask first. |
| libigl | Not needed | Duplicates GeometryCore. |
| CUDA | No | NVIDIA only. See section 11. |

## 5. Architecture

Project plugin `Plugins/AZNaturalGrip` (tracked by git, enabled by default, so `AZ.uproject` is not edited):

| Part | What | Depends on |
|---|---|---|
| `Source/AZNaturalGrip/Private/Core/` | The solver core: math, hand model, field sampling, A brute-force solvers, B optimizer. Plain C++17 + Eigen, no Unreal headers. Gets a `ParallelFor` callback from outside. | Eigen |
| `Source/AZNaturalGrip/` (Editor module) | Asset readers (meshes, clips), field bake on GeometryCore, asset writers (sockets, grip pose, BP flags, curves, backups), preview actors, `ParallelFor`, JSON, the profile asset, the menu and the tab, background jobs, a Python-callable library. | Core, GeometryCore, MeshConversion, AnimationBlueprintLibrary, Engine, UnrealEd, Kismet, Slate |
| `Tools/ngtest/` | Standalone console harness (`main.cpp` + `build.bat`, MSVC). Compiles the same Core files and runs the parity tests against the Python goldens. | Core |

Why the core has no Unreal headers:
1. It can be built and tested while the editor stays open. A new Unreal module builds only with the editor closed, and
   Live Coding cannot add modules.
2. The harness and the editor run the very same code, so what passes in the harness passes in the editor.
3. It stays reusable later: a runtime module (section 10), other tools.

The field bake uses GeometryCore, an engine module, so it lives in the editor module and is tested in the editor.

Data flow:
- **Parity path (parts A and B, harness):** the inputs are the same JSON files the Python reads, so the C++ results
  can be compared number for number with the Python goldens.
- **Direct path (part C, editor). Every Python script is replaced by an asset reader or writer:**

| Today (script, JSON) | Part C (C++, direct) |
|---|---|
| `weapon_skm_dump.py` + `skm_parts.py` | weapon triangles from the SkeletalMesh (the MeshDescription to DynamicMesh path that GeometryScript used, so the vertex set is the same), parts by `FMeshConnectedComponents` |
| `dump_hand_verts.py` | hand skin vertices, bones and ref pose from the hero SkeletalMesh |
| `bake_grip_field.py`, `fine_field.py`, `import_grip_field.py` | field bake on GeometryCore; the coarse field written straight into the `UAZ_WeaponGripField` asset |
| `dump_left_m16.py`, `m16_hold.py`, `dump_wrist.py`, the medoid picks | clip sampling in C++ (`UAnimPoseExtensions`, as the scripts did): hand, elbow and finger transforms in weapon space; medoid aim/relaxed holds |
| `apply_nat_grip.py` (pose) | grip pose written through `IAnimationDataController` |
| `apply_nat_grip.py` (weapon), `set_weapon_left_grip.py`, `add_hand_socket.py` | weapon mesh sockets; BP defaults (correction, bBaked flags, GripPose, socket names) through reflection by property name, so the plugin does not depend on the AZ module; BP compiled in C++ (a regular Blueprint, not an AnimBP) |
| `add_grip_curves.py`, `set_left_curve_m16.py` | `AZ_Grip_R/L` curves per clip from where the clip's hand is (same on/off radii, minimum hold time, ramps); motion untouched |
| `nat_grip_preview.py` | before/after preview actors in the level (the level is not saved) |

Every write follows the same rules:
- automatic backup to `AZ_Backups/<date>_<weapon>` before any write;
- refused while PIE runs;
- a dry-run list of what would change, shown first;
- verification by reading back plus the file mtime.

M16/pistol/unarmed clips are protected. For them only the curves are allowed, and only with the user's explicit go
per run. The unarmed AnimPro clips are never touched without a new instruction.

## 6. Part A: Python to C++ map

| Python | Content | C++ unit |
|---|---|---|
| `ql.py` | quaternions, transform with UE semantics (`A*B` = A then B) | `Core/NGMath.h` |
| `hand.py`, `skin.py`, `grasp.py`, `grasp2.pens`, `grasp3` (`segs`, `seg_seg`, `finger_overlap`, `RAD`) | bones, parents, mesh ref pose, skin bound to the nearest bone segment, vertex groups with THIN, finger and thumb locals, FK, link gap/penetration, pad point, phalanx capsules | `Core/NGHand.h/.cpp` |
| `views.field`, `_fine`, `coarse_field` | trilinear lattice, fine then coarse then 10 cm | `Core/NGField.h/.cpp` |
| `geom.py`, `skm_parts.py`, `bake_grip_field.py`, `fine_field.py` | parts, exact distance, inside test, lattice bake, narrow band | editor module, `NGFieldBake.cpp` on GeometryCore (section 4) |
| `place2.py` | placement = rotation about the middle knuckle + translation, PALM samples, palm clearance | `Core/NGPlacement.h/.cpp` |
| `index_nat.py`, `place_nat.py`, `rsolve.py` (constants, `Lc`), `rsolve2.py` (`_opt_job`, `thumb_rest`), `rsolve3.py` | trigger hand: natural index, placement search, fingers one against the other, thumb | `Core/NGTriggerHand.h/.cpp` |
| `lsolve_m16.py`, `left_solve.thumb_left` | support hand: stage 1/2/final, wrap angle around the handguard, wrist bend, thumb along the handguard | `Core/NGSupportHand.h/.cpp` |
| `finger_gaps.py` | skin gaps between neighbouring fingers | `Core/NGReport.h/.cpp` |
| environment variables (`NATGRIP_*`) | trigger point, pull direction, grip X, thumb side and height, wrap front, handguard axis, field boxes, THIN, steps | `FSolveSettings`: the profile asset in the editor, flags in the harness |

## 7. Part B: optimized solver

1. **One energy.** Every rule that is now a score or a rejection becomes a smooth term (squared hinge instead of
   "reject if pen > 0"):
   - skin penetration (SDF < 0);
   - each phalanx resting (gap - CONTACT);
   - DIP ~0.65 PIP, no hook, no flat bend, small side angle;
   - neighbour adjacency (~1.5 mm);
   - wrap around the grip / the pad on the trigger facing the pull;
   - thumb rules;
   - palm clearance and wrist bend.
   The weights are the ones the scorers use today.
2. **Gradients.** Analytic: joint rotation `axis x (p - pivot)` times the field gradient of the trilinear cell. One
   gradient costs about one evaluation. Checked against Eigen `AutoDiff`.
3. **Local solve.** Levenberg-Marquardt (Eigen) over the finger angles, the thumb and the 6 placement parameters
   together. This follows the right-hand lesson: the placement is searched together with natural finger shapes.
4. **Global search.** Multi-start from a few seeds:
   - the natural poses;
   - a coarse brute-force grid (the A code at a large step);
   - for the fingers, the synergy space (PCA of our clips, 1-2 parameters per finger).
   Each start runs a few dozen iterations; the best result wins.
5. **Validation against the A oracle.** On many placements (for example 1,000 random ones around the M16 holds),
   compare B's best with A's exhaustive best under the SAME scoring.
   - Acceptance: B is at least as good in 99% or more of the cases.
   - The M16 cases look the same or better in the renders.
   - Then B becomes the default button and A stays as "Reference".

## 8. Parallelism and determinism

What runs in parallel:
- the placement grids and the B starts;
- the stage-2 candidates;
- the side-angle x MCP rows inside one A finger search;
- the field slabs (one X slice per task).

What stays sequential by nature: the fingers in A, one after another, because each lies against the previous one.
The parallelism is inside each finger.

Same result with any thread count:
- Every task writes its own slot. The reduction walks the slots in Python's loop order with the same strict `>`.
- In A, loop arithmetic is copied literally: `a += step` accumulation and the same formula order. The doubles then
  match bit for bit wherever the CRT does.

In the editor:
- `ParallelFor` runs inside a background `UE::Tasks` job, so the game thread stays free.
- The inputs are copied into plain structs on the game thread before the job starts; no UObject is touched off the
  game thread.
- A cancel flag is checked per row. Progress goes to the UI through atomic counters.

The harness uses a small `std::thread` pool behind the same callback.

## 9. Editor UI

Tools menu, then **AZ Natural Grip**, opens a tab with:
- **Profile.** An asset picker plus a details view. `UAZNaturalGripProfile` holds:
  - weapon mesh, side, input files;
  - trigger, pull, grip and handguard parameters;
  - field boxes and search ranges.
  The defaults are today's Python values. Profiles for the M16 right and left hands are created with them.
- **Main buttons:**
  - **Solve weapon:** read the assets, bake the fields, sample the clips, solve both hands;
  - **Preview:** before/after actors in the level;
  - **Apply:** a dry-run list first, then the backup and the writes.
- **Diagnostic buttons:**
  - Reference solve (A, by stage);
  - Parity check vs Python;
  - Compare B vs A.
- **Progress bar** with the stage and percent, Cancel, elapsed time.
- **Results:**
  - top candidates: J, palm, wrist bend, finger angles, wrap, thumb;
  - the final report;
  - Save JSON in the Python schema, so the apply scripts work as they are.
- **Python access:** `UAZNaturalGripLibrary::RunStage(Profile, Stage, Threads)` is BlueprintCallable, so runs can
  be scripted and verified.

## 10. Tests and acceptance

Part A (the goldens are the Python outputs re-run today):
1. **Primitives.** 200 random finger configurations per side: world positions, link gaps and penetrations, capsules,
   `seg_seg`. Difference below 1e-9.
2. **Field sampling.** 10,000 random points, coarse and fine. Difference below 1e-12.
3. **Field bake** (editor) vs `fine_field_left_m16.json`: at most 5e-4, because Python rounds to 3 decimals. The fast
   winding number is an approximation, but only its sign at 0.5 is used, so it differs only right at the surface,
   where the distance is about 0 anyway.
4. **Solvers.**
   - Stage 1: the top-400 list identical (placement, J1 within 1e-9).
   - Stage 2: the same picks.
   - Final L2: the same angles, side angles and thumb; F within 1e-9.
   - place_nat: every candidate identical.
   - rsolve3 N2: identical, including the index refine.
5. **Threads.** A run on all threads gives exactly the same picks as a run on 1 thread.

Part B: section 7, point 5.

If a pick differs: find the first diverging number by instrumenting, not by guessing.

## 11. GPU: decision

Not in this plan.
- **Offline search.** After part B it is a few hundred checks per start, which a CPU does in milliseconds. A GPU pays
  off only on millions of checks, which is exactly the brute force that B removes.
- **Real time in the game.** The grasp is solved once per weapon (the hand relative to the weapon is constant in the
  clips). Per frame only a small adaptation is needed: the B optimizer warm-started from the baked grasp, a few
  iterations. That fits on the CPU in the animation worker thread (estimated well under a millisecond per hand).
- **Latency.** A GPU result comes back one or more frames late, and waiting for it stalls the frame. That is bad for
  animation, which is evaluated on the CPU.
- **If ever needed:** compute shaders through Unreal's RHI/RDG (HLSL). They run on any GPU in the shipped game.
  CUDA only works on NVIDIA cards. The machine has an RTX 4080 SUPER, so editor-only experiments are possible. The
  core keeps its data in flat arrays (field as a flat 3D grid, candidates as independent indices) so a compute-shader
  backend stays possible.

## 12. Steps

| Step | Content | Editor |
|---|---|---|
| S1 | A: core math, hand model, field sampling, harness, primitive parity | open |
| S2 | A: solvers (support hand stage 1/2/final, trigger hand place_nat/index/rsolve3), parity with the goldens. **Checkpoint for the user.** | open |
| S3 | B: energy, analytic gradients (checked by AutoDiff), Levenberg-Marquardt, multi-start, synergies from the clips | open |
| S4 | B vs A on 1,000 placements, M16 renders. **Checkpoint for the user.** | open |
| S5 | C: plugin module, profile asset, menu and tab, background jobs, progress and cancel. Readers for the skeleton, hand skin, weapon mesh and clips, plus the GeometryCore field bake. Checked against the JSON dumps: same vertices, same holds, same field. | **closed once for the build** |
| S6 | C: in-editor solve of the M16 right and left hands from the panel, both solvers, parity. **Checkpoint for the user.** | open |
| S7 | C: Apply (backup, socket, grip pose, BP flags, curves) and the before/after preview. Tested first on COPIES of the M16 assets in a test folder: the result must reproduce what is applied today. The real assets are written only after the user's go. | open (Live Coding for .cpp) |
| S8 | C: "Solve weapon" = the whole chain in one button. Then the shotgun as the first new weapon. **Checkpoint for the user.** | open |

Who does what:
- I write the interfaces and specs and verify everything against the tests.
- Mechanical porting of single A modules goes to Sonnet subagents with these specs.
- The boilerplate (`.uplugin`, `Build.cs`) goes to the local model if LM Studio is up.
- The B math (energy, gradients, optimizer) stays with me.

## 13. Risks and failure axes

- **Floating point (A).** The formulas are identical, in double, with the MSVC CRT math on both sides. Python's JSON
  writes the shortest round-trip repr, which is read back exactly.
- **Hidden Python behaviour (A):**
  - `min()`/`max()` keep the first of equal values;
  - `[::THIN]` slicing;
  - float accumulation in the loops;
  - `sort` is stable, so the port uses `std::stable_sort` with the same key;
  - dict insertion order (bone order from the JSON, skin vertex order).
- **Local minima (B).** Gradient methods stop in the nearest valley; the multi-start seeds and the A oracle comparison
  cover this. If B misses the A best on hard cases, add seeds from a coarse A grid instead of tuning weights blindly.
- **Smooth vs hard rules (B).** A squared hinge allows a tiny penetration the hard rule rejected. The final pose is
  checked with the A hard checks (no vertex beyond TOL, no finger overlap); if it fails, it is projected out and
  re-checked.
- **Unreal build.** The shared PCH is force-included into the core files, so the core avoids UE macro names (`PI`,
  `check`, `verify`, `TEXT`). Shadowed variables are errors. The core lives in its own namespace for unity builds.
  Eigen inside UE needs the engine's usual warning wrappers (`THIRD_PARTY_INCLUDES_START/END`).
- **Editor safety.** No UObject is touched off the game thread. The inputs are read into plain structs first, and the
  writes happen on the game thread after the job.
- **Asset writes (S7).** They follow the rules in section 5: backup, PIE check, dry run, read-back verification. A
  weapon BP is compiled in C++. No AnimBP is compiled or saved by the tool.
- **One command needs a few weapon marks.** The trigger point and the grip and handguard axes come from the weapon's
  bones and sockets when they exist (the M16 has a `trigger` bone). Otherwise they are set once in the profile.
- **Parallel sessions.** Everything new lives in `Plugins/AZNaturalGrip` and `docs/`. No shared file is edited.
- **Machine instability.** Parallel CPython crashed here before. C++ threads do not involve CPython. If a multi-thread
  run ever disagrees with the 1-thread run, that points at the hardware and is reported.

## 14. After this plan

- The M16 left-hand power grasp on the B solver: handguard seated in the hand, wrap of 180 degrees or more, thumb
  opposition.
- The pistol and the other pack guns through profiles.
- Runtime adaptation: the B optimizer per frame in the Grip node, warm-started from the baked grasp. This is a separate
  decision; the ABPs reference those classes.
