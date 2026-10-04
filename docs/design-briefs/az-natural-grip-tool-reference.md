# AZ Natural Grip: tool reference

Status: 2026-10-03.
- **The tool** (plugin `Plugins/AZNaturalGrip`) is built and in use. The M16 left-hand power grasp it produced was
  applied and approved by the user in PIE ("Сейчас, я думаю, идеально").
- **The open problem** of the left hand during a weapon draw / holster has its own document:
  `left-hand-weapon-switch-problem.md`.

Companion documents:
- `natural-grip-cpp-plugin.md`: the implementation plan (rev. 3) and its status log.
- `weapon-grip-system.md` and `weapon-grip-system-plan.md`: the runtime grip design.

---

## 1. What the tool is for

The hero (a MetaHuman) plays weapon clips that were retargeted from other skeletons. In those clips the hands do not
fit the real weapon:
- the right palm sits about 2 cm inside the stock;
- the fingers are twisted by the retarget;
- the left hand only roughly touches the handguard.

The hand-to-weapon relation is (almost) constant through all the clips of one weapon. So the grasp is solved ONCE per
weapon, offline, against the real meshes. The runtime then only applies it:
- **left hand:** IK onto a socket on the weapon;
- **fingers:** a 1-frame grip pose;
- **right hand:** a re-grip correction.

The offline solver was first written in Python (`Tools/wgs/natgrip`, many scripts, hours per weapon, JSON exports in
between). **AZ Natural Grip** is its C++ port inside the editor:
- it reads the assets directly: the hero skeleton, the hand skin, the weapon mesh, the weapon's clips;
- it solves both hands with real parallelism;
- it writes the result back into the weapon's assets after an automatic backup.

One profile per weapon hand, one button per step.

**Parity:** the C++ port is bit-exact against the Python solver (max |diff| = 0 on every compared number, identical on
1 and 32 threads). It then grew new rules that Python never had: the power grasp, the hand roles, and the asset reading
and writing.

## 2. The three layers

| Layer | Where | What it does |
|---|---|---|
| Offline solver | `Plugins/AZNaturalGrip` (editor plugin) | Profiles, panel, solver stages, Apply. Writes per weapon: the `LeftHandGrip` socket, finger rotations in the grip pose, the right-hand correction, the "baked grasp" flags. |
| Weapon assets | the weapon's mesh / skeleton, its Blueprint (`AZ_BP_Rifle`, ...), its grip pose (`AS_Grip_M16`, ...) | Hold the solved grasp. |
| Runtime | `AnimNode_AZWeaponGrip`, `AnimNode_AZWeaponBodyClearance`, `UAZ_MoverAnimInstance` (game module `Source/AZ`) | Every frame: IK the left hand onto `LeftHandGrip`, apply the grip-pose fingers, re-grip the right hand, keep the arms out of the weapon; weighted by the clip curves `AZ_Grip_L` / `AZ_Grip_R`. |

## 3. Plugin layout

```
Plugins/AZNaturalGrip/
  AZNaturalGrip.uplugin                 editor plugin, enabled by default (AZ.uproject not edited)
  Source/AZNaturalGrip/
    AZNaturalGrip.Build.cs
    Public/AZNaturalGripProfile.h       the profile data asset + enums (roles, weapon types)
    Public/AZNaturalGripLibrary.h       Python/Blueprint access: RunStage, Apply
    Private/AZNaturalGripModule.cpp     Tools menu entry + nomad tab "AZ Natural Grip"
    Private/SAZNaturalGripPanel.*       the panel (Slate)
    Private/NGJobRunner.*               background UE::Tasks job, ParallelFor, cancel, progress
    Private/NGJobs.*                    profile snapshot + every solver stage
    Private/NGAssetReaders.*            mesh triangles, hero hand, marker sockets, hold sampled from clips
    Private/NGApply.*                   Apply: backup, write, save, read back
    Private/Core/                       engine-agnostic C++17 solver (no Unreal headers)
  Content/Profiles/                     NGP_M16_Left, NGP_M16_Right (mount point /AZNaturalGrip/Profiles)
  Tools/ngtest/                         standalone MSVC harness running the same Core files (parity tests)
  Tools/create_m16_profiles.py          creates the two M16 profiles
  Tools/apply_test_setup.py             makes test copies of the M16 assets for Apply tests
```

### 3.1 Core (engine-agnostic, `Private/Core`)

The core has no Unreal headers. This has three benefits:
- it compiles and is tested in the harness while the editor stays open;
- the harness and the editor run the very same code;
- it can later move into a runtime module.

| File | Content |
|---|---|
| `NGMath.h` | `V3`, `Q` (quaternion), `X` (transform with UE semantics: `A * B` = A then B), `PyFloatSum` (Neumaier-compensated sum, as CPython >= 3.12 `sum()`), helpers that copy Python's `**2` (CRT `pow`), floored `%`, first-of-equals min/max. |
| `NGJson.*`, `NGJsonDiff.*` | JSON reader/writer (shortest round-trip doubles) and a structural numeric diff for the parity checks. |
| `NGField.*` | `Lattice`: a trilinear signed-distance lattice. `Field::Sample`: fine field, then coarse, then 10 cm far away. `Obstacles`: extra capsules (the other hand) folded into the distance. |
| `NGGeometry.*` | Weapon triangle mesh, `SplitParts` (connected parts), exact distance + winding-number inside test (`SdfPart`), `BakeCoarse`, `BakeFine` (narrow band). A bit-exact port of `geom.py`. |
| `NGHand.*` | `HandModel`: bones, parents, mesh ref pose, skin bound to bone segments; FK; finger/thumb locals in absolute anatomical degrees; link penetration/gap; pad point; phalanx capsules; `HandCapsules` (a posed hand as capsules); `SegSeg`; `FingerOverlap`. |
| `NGHandData.h` | Plain input data: `HandData` (bones, ref locals, bind pose, skin vertices in hand space) and `HoldData` (where the clip holds the weapon: hand in weapon space, locals, aim/relaxed elbows). |
| `NGSetup.*` | `SetupConfig`, `Setup::Load`: builds the hand model + fields + the hold placement from either the Python data files or in-memory inputs (`SetupInputs`). |
| `NGParallel.h` | `Exec`: `For`, `Cancel`, `Progress`. Each task writes its own slot and a serial reduction follows, so any thread count gives the same picks. |
| `NGOptim.h` | `PatternMaximize`: Hooke-Jeeves pattern search, used to polish grid optima into continuous angles (part B). |
| `NGTriggerHand.*` | Trigger hand: `PlaceNat` (placement search with a natural index finger), `SolveTrigger` (fingers one against the other, the thumb), JSON output. |
| `NGSupportHand.*` | Support hand. The classic chain: stage 1 placements, stage 2 fingers on the top N, final, polish. The **power grasp**: `SupportSettings::SetPower`, `SupportPowerStage1`, `SolveSupportFingersPower`, `SolveSupportPower`, `ThumbPower`, `EnclosureDeg`, `TwistDeg`. A general grip axis (`SetAxis`) for handles. |

### 3.2 Editor module

| Unit | Responsibility |
|---|---|
| `NGJobs::Snapshot` | Game thread. Copies the profile into `FNGProfileData` (no UObject is touched off the game thread). For Input = Assets it also reads:<ul><li>the weapon triangles and the hero hand;</li><li>the marker sockets;</li><li>the hold sampled from the clips;</li><li>for the pistol second hand, the right hand's last solve as capsule obstacles.</li></ul> |
| `NGJobs::Run(P, Stage, Exec)` | Any thread. Runs one stage and writes JSON results in the Python schema to `Saved/NaturalGrip/<Profile>/`. |
| `NGJobs::StagesFor(Role)` | The diagnostic stage buttons offered for a role (section 7). |
| `NGJobRunner` | Background job, progress, cancel. |
| `NGAssetReaders` | `ReadMeshTriangles`: the MeshDescription -> DynamicMesh path that GeometryScript used, so the vertex set equals the old dumps. `ReadHand`: the logic of `dump_hand_verts.py`. `ReadMarker`: a socket's position in the weapon's component space. `SampleHold`: see 8.3. |
| `NGApply` | Section 9. |
| `SAZNaturalGripPanel` | Section 6. |
| `UAZNaturalGripLibrary` | `run_stage(profile, stage, threads)` and `apply(profile, dry_run)` for Python. |

## 4. Concepts

- **Placement** = where the hand sits relative to the clip's hold:
  - a rotation about the hold's middle knuckle (yaw, pitch, roll, in weapon axes) plus a translation (dx, dy, dz, cm);
  - the power grasp adds theta, a roll about the handguard axis.
  - It is stored in a profile as `FAZGripPlacement`.
- **Fields.**
  - **Coarse:** a signed-distance lattice of the whole weapon (0.5 cm spacing).
  - **Fine:** a narrow band around the hand's grip zone (0.25 cm). With a hold sampled from clips the fine box is
    centred on the hand (`bAutoFineBox`, +-13 cm).
- **Skin contact.** Each bone's skin vertices (thinned by `SearchThin` / `FinalThin`) are sampled in the field. A
  vertex may touch the surface but not pierce it (tolerance 0.05 cm).
- **Joint angles** are absolute anatomical degrees. The MetaHuman mesh ref pose is already curled. Flexion = bone local
  -Z, MCP side angle = local +Y.
- **Force closure** (the lesson of 2026-10-03):
  - A firm grip covers an arc around the held part, from the finger pads through the palm to the thumb pad: 220-260
    degrees.
  - Measuring the SHORTEST arc between thumb and fingers was a bug: a thumb lying low under the handguard satisfied it.
  - Contact-only rules pick claws and hooks. That happened to the index finger and to the thumb before this rule.

## 5. Hand roles and weapon types

The profile's **Role** (`EAZGripHand`) chooses the grasp rules. The clips only say WHERE the hand is on the weapon.

| Role | Hand | Rules |
|---|---|---|
| `Trigger` | right, pistol grip | Index pad on the trigger facing the pull direction. The other fingers wrap the grip and end behind its front face (`NG_GripFront`). The thumb goes to the far side, under the receiver (`NG_ThumbLimit`). |
| `TriggerStraight` | right, straight stock wrist (shotgun, lever rifle) | The trigger rules without the pistol-grip limits (no front face, the thumb may wrap over the top) unless the markers exist. |
| `Support` | left, handguard / forend / pump | Power grasp (default on, `bPowerGrasp`):<ul><li>palm and proximal phalanges seated;</li><li>closure 220+ degrees;</li><li>thumb along the near side, pointing forward, pressing with its pad, no curled joints;</li><li>roll about the handguard axis (-60..60 degrees, 9 angles) within a 35-degree free forearm twist.</li></ul>Off = the original L2 rules. |
| `Handle` | either side (`Side`) | Knife / melee handle: the power grasp around a general axis from `NG_HandleBack` to `NG_HandleFront`, no trigger. |
| `PistolCup` | left, pistol second hand | The power grasp around the grip axis (handle markers). The right hand's last solve (`OtherHand` profile, `trigger_solve.json`) becomes capsule obstacles, so the left hand lies over the right fingers; the thumb points along `WeaponForward`. |

The panel's **Weapon type** (`EAZWeaponGripType`) is a set of roles for **Create profiles**:

| Type | Profiles created |
|---|---|
| Rifle / carbine | `_Right` = Trigger, `_Left` = Support |
| Shotgun / lever rifle | `_Right` = TriggerStraight, `_Left` = Support |
| Pistol / revolver | `_Right` = Trigger, `_Left` = PistolCup (`OtherHand` = `_Right`) |
| Knife / one-hand melee | `_Right` = Handle |
| Two-hand melee | `_Right` = Handle, `_Left` = Handle |

### 5.1 Marker sockets on the weapon mesh

With Input = Assets, the markers replace the typed numbers of the profile.

| Socket | Meaning | Read as | Required for |
|---|---|---|---|
| `NG_Trigger` | trigger pad target | `TriggerPoint`, `GripX` = its X | Trigger, TriggerStraight |
| `NG_GripFront` | the grip's front face, below the trigger guard | `WrapFrontY` = its Y | Trigger |
| `NG_ThumbLimit` | highest point of the trigger thumb tip | `ThumbZMax` = its Z | Trigger |
| `NG_Handguard` | a point on the handguard axis (axis runs along +Y) | `HandguardAxisXZ` | Support |
| `NG_HandleBack`, `NG_HandleFront` | the handle / grip axis ends | general axis | Handle, PistolCup |
| `LeftHandGrip` (or `LeftHandSocket`) | where the left hand is IK'd at runtime | written by Apply | Support (runtime) |

Since 2026-10-03, a profile whose hold comes from clips (`ClipFolder` set) REFUSES to solve when one of its role's
markers is missing. The error names the missing socket. Before, the M16 default numbers silently solved the new weapon.

## 6. The profile asset and the panel

**Tools > AZ Natural Grip** opens the panel.
- **Weapon hand profile:** asset picker plus a details view of `UAZNaturalGripProfile`.
- **Weapon type** + **Create profiles:**
  - the weapon is the SkeletalMesh selected in the Content Browser;
  - the template is the picked profile;
  - it creates `NGP_<MeshName>_<Right|Left>` in `/AZNaturalGrip/Profiles`.
  - For another weapon it clears the template's Weapon Blueprint, Grip Pose Override, Clip Folder, Weapon Socket On
    Hero and Other Hand. Apply can therefore never write into the template weapon.
  - It logs a checklist of what to fill in.
- **Main buttons:**
  - **Solve weapon hand:** the whole chain for the role (trigger: placement + solve; support: power or classic chain;
    handle / pistol cup: power).
  - **Apply: dry run:** the plan only.
  - **Apply...:** asks first; then backup, write, save, read back.
- **Diagnostic stage buttons** (per role, section 7), a **Threads** box (0 = all), **Cancel**, a progress bar and a log.

Main profile fields:

| Group | Fields |
|---|---|
| Weapon | `WeaponMesh`, `WeaponKey` (the file key in the data folder) |
| Hand | `Role`, `Side` (handle only), `OtherHand` (pistol cup), `WeaponForward` (muzzle direction in mesh space), `HeroMesh` |
| Input | `Input` (Python data files = parity / Assets = direct), `DataDir` (default `Saved/wgs`), `LeftPick`, `SearchThin` 2, `FinalThin` 1 |
| Input / Clips | `ClipFolder`, `WeaponSocketOnHero` (e.g. `RightHandM16Socket`), `AimClipFilter` "Aim", `AimClipExclude` "Relaxed", `RelaxedClipFilter` "Relaxed,Walk", `FramesPerClip` 8 |
| Field | `CoarseSpacing` 0.5, `CoarseMargin` 3, `FineBoxMin/Max`, `FineSpacing` 0.25, `FineBand` 1.3, `bAutoFineBox`, `FineBoxHalfSize` 13 |
| Weapon Markers | the socket names of 5.1 |
| Trigger Hand | `TriggerPoint`, `PullDirection`, `GripX`, `ThumbSide`, `ThumbZMax`, `WrapFrontY`, `SearchCentre`, `TriggerPlacement`, `IndexStart` |
| Support Hand | `HandguardAxisXZ`, `bPowerGrasp`, `PowerCandidates` 54, `Stage2Top` 24, `Stage2Step` 8, `SupportPlacement` |
| Apply | `WeaponBlueprint`, `GripPoseOverride`, `LeftHandSocket` "LeftHandGrip", `BackupDir`, property names on the weapon BP (`GripPose`, `RightHandGripCorrection`, `bBakedRightHandGrasp`, `bBakedLeftHandGrasp`) |

## 7. Solver stages

Every stage runs in the background and writes its result under `Saved/NaturalGrip/<Profile>/`.

| Stage (panel label) | Roles | What it does | Output |
|---|---|---|---|
| `fields` (Bake fields) | all | Splits the weapon into parts and bakes the coarse and fine fields. With Python inputs it also compares them against the Python fields. | `field_coarse.json`, `field_fine_<left/right>.json` |
| `stage1` (Stage 1: placements) | Support | Every placement scored by palm clearance, wrist bend, deviation (thin 2, coarse field). | `support_stage1.json` (top 400) |
| `stage2` (Stage 2: fingers top N) | Support | Stage 1, then fingers for the top `Stage2Top`. | `support_stage2.json` |
| `final` / `polish` | Support | The final solve at `SupportPlacement`. Polish = grid best + pattern search (part B). | `support_final.json` |
| `power` (Power grasp) | Support, Handle, PistolCup | Three steps:<ol><li>power stage 1: palm seated, bend, twist, roll;</li><li>the best placements of EVERY roll angle get coarse fingers and a closing thumb;</li><li>the final on the winner.</li></ol> | `support_final.json` |
| `support` | Support | Power chain when `bPowerGrasp`, else stage 1 -> 2 -> final+polish on the best stage-2 placement. | `support_final.json` |
| `placement` | Trigger | `PlaceNat` over the whole grid around `SearchCentre`. | `trigger_placement_all.json` |
| `solve` | Trigger | `SolveTrigger` at `TriggerPlacement` / `IndexStart`. | `trigger_solve.json` |
| `trigger` | Trigger | Placement, then solve on the best. | both |
| `parity` | Trigger, Support | Runs the Python reference runs and compares number for number with the goldens in `Tools/wgs/natgrip` (`lsolve_stage1.json`, `lsolve_stage2_0.json`, `lsolve_m16_L2.json`, `pn_all.json`, `rsolve3_m16_N2.json`). Prints `PARITY: PASS/FAIL`. | |
| `inputs` (Check asset inputs) | all, Input = Assets | What was read from the assets (hand, mesh, fields) vs the Python dump files. | log |

## 8. Inputs read from assets

1. **Weapon mesh.** `ReadMeshTriangles`: LOD 0 source model, all vertex IDs. On the M16: 6,988 vertices, 13,479
   triangles, 39 parts, identical to the old dump.
2. **Hero hand.** `ReadHand`:
   - the solver bone order and parents;
   - the finger ref locals and the bind pose in hand space;
   - the skin vertices in hand space (box filter, 1e-3 rounding).
   On the M16: 5,084 vertices, identical to the dump.
3. **Hold from clips.** `SampleHold`, which replaces `dump_left_m16.py` and the medoid picks:
   - samples `FramesPerClip` frames of every clip in `ClipFolder`;
   - places the weapon each frame at `WeaponSocketOnHero` on its bone and puts the hand in weapon space;
   - takes the medoid frame of the aim clips (distance = hand position + 0.1 x rotation degrees);
   - also takes the elbows of the aim medoid and of the relaxed medoid (the wrist-bend term).
4. **Markers.** `ReadMarker`: the socket position in the weapon mesh's component space (ref pose).

## 9. Apply

`NGApply::Apply(Profile, bDryRun, Log)` reads the last result: `support_final.json` for support / handle / pistol cup,
`trigger_solve.json` for trigger roles.

**Support hand writes:**
- the `LeftHandSocket` transform = the solved hand in weapon space. The socket may live on the mesh or on its
  skeleton; the M16's lives on the skeleton asset `M16_Skeleton_Skeleton`. The package that owns it is backed up and
  saved;
- the grip pose's LEFT finger rotations (through `IAnimationDataController`);
- `bBakedLeftHandGrasp = true` on the weapon Blueprint's class defaults; the Blueprint is compiled in C++.

**Trigger hand writes:**
- the weapon Blueprint's `RightHandGripCorrection`: the solved hand vs the clip hold, hand-local. AZ Weapon Body
  Clearance applies it at runtime;
- the RIGHT finger rotations;
- `bBakedRightHandGrasp`.

**Safety:**
- refused while PIE runs;
- refused when a target package already has unsaved changes (Apply saves whole packages);
- the dry-run plan lists each change as "now" / "new" and each package to write;
- backup: every owning `.uasset` is copied to `<BackupDir>/<date>_<time>_<profile>/` with a `manifest.json` of the
  old values. The default `BackupDir` is `<Project>/../AZ_Backups`;
- the packages are saved, then read back. The log reports the socket position/rotation error, the largest grip-pose
  rotation error, the Blueprint flag, and that the file timestamp changed. It ends with `APPLIED`, or with
  `APPLY CHECK FAILED (backup: ...)`.

Protected content: the M16, pistol and unarmed clips are tuned. Their assets are written only with the user's explicit
go per run.

## 10. Scripting, harness, build workflow

- **Python:**
  ```
  unreal.AZNaturalGripLibrary.run_stage(unreal.load_asset("/AZNaturalGrip/Profiles/NGP_M16_Left"), "power", 0)
  unreal.AZNaturalGripLibrary.apply(profile, True)
  ```
  The second call is a dry run; `False` writes.
- **Harness:** `Plugins/AZNaturalGrip/Tools/ngtest/build.bat all`. Run it from PowerShell: cmd launched from Git Bash
  can hang.
  - Then `Intermediate/all/ngtest.exe prims | support | fast | power | trigger | geometry | jsondiff`.
  - Run it after ANY core change; parity must stay exact.
- **Compile a plugin or game .cpp WITHOUT closing the editor:**
  ```
  Build.bat AZEditor Win64 Development -Project=<AZ.uproject> -SingleFile=<abs path .cpp>
  ```
  This is a compile check only. Then apply it with Live Coding: console `LiveCoding.Compile` (from Python
  `unreal.SystemLibrary.execute_console_command(None, "LiveCoding.Compile")`).
- **A header / new UPROPERTY / new member** needs the editor closed and a full build. Live Coding a header silently
  breaks AnimBPs.
- **Python in the editor** never compiles or saves AnimBPs and never calls `reconstruct_node`.

## 11. Adding a new weapon (procedure)

1. **Weapon mesh.** Add the marker sockets for its roles (5.1) and a `LeftHandGrip` socket for a support hand. Check
   the muzzle direction in mesh space (`WeaponForward`; +Y on the M16).
2. **Weapon Blueprint.** It needs a grip pose. For example, `AZ_BP_Pistol` has none: duplicate `AS_Grip_M16` ->
   `AS_Grip_Pistol` and set it as `GripPose`.
3. **Create profiles.** In the panel:
   - pick `NGP_M16_Right` as the template;
   - choose the weapon type;
   - select the weapon mesh in the Content Browser;
   - press **Create profiles**.
4. **Fill in each new profile:** `WeaponBlueprint`, `ClipFolder` + `WeaponSocketOnHero` (the hold), `WeaponForward`.
5. **Right hand first:** **Solve weapon hand** -> **Apply: dry run**. Check that the targets are THIS weapon's assets.
   Then **Apply...** -> PIE.
6. **Left hand next:** the same steps. The pistol second hand needs the right hand solved first.

Pistol facts:
- hero sockets `RightHandPistolSocketAim` / `RightHandPistolSocketRelaxed`;
- clips `/Game/AZ/Assets/Pistol`;
- mesh `/Game/MilitaryWeapDark/Weapons/Pistols_B` (skeleton `Pistols_B_Skeleton`);
- draw clip `AZ_Pistol_Equip`.

## 12. Runtime consumers

- **`FAnimNode_AZWeaponGrip`** ("AZ Weapon Grip", in `AZ_ABP_MoverHero_MHC`):
  - two-bone IK of `upperarm_l` / `lowerarm_l` / `hand_l` onto `LeftHandInWeaponBone * WeaponBone`, resolved on the
    current pose (no lag). The elbow pole keeps the input pose's elbow side;
  - the grip-pose fingers of both hands (baked: applied as they are; otherwise fitted to the weapon's field or IK'd to
    fingertip markers).
  - **Per-hand weight** = `GripAlpha` x the pose curve `AZ_Grip_L` / `AZ_Grip_R`, or `DefaultHandAlpha` when the
    curve is missing.
  - **Weapon switch reach** (`FAZ_WeaponSwitchReach`): during a draw / holster the anim instance plans the left hand's
    path from the switch clip (`UAZ_MoverAnimInstance::BuildWeaponReachPlan`). While it owns the hand, the node follows
    that path instead of the pose curve. Console `az.Weapon.Reach` 0 / 1 / 2 (2 = per-frame log). Details, status and
    open defects are in `left-hand-weapon-switch-problem.md`.
- **`FAnimNode_AZWeaponBodyClearance`:** the body push, the right-arm solver and the right-hand re-grip
  (`RightHandGripCorrection`); master weight = `WeaponGripAlpha`.
- **`UAZ_MoverAnimInstance` grip gather** (game thread, `NativeUpdateAnimation`):
  - the held weapon = attached on its Relaxed/Aim socket;
  - it supplies `WeaponGripPose`, `WeaponGripLeftHandInBone`, `WeaponGripBone`, `WeaponGripMarkers`;
  - `WeaponGripAlpha` eases at `WeaponGripBlendSpeed` 6/s;
  - during a weapon switch the grip follows the switch clip itself (see `left-hand-weapon-switch-problem.md`).
- **Clip curves `AZ_Grip_L` / `AZ_Grip_R`:**
  - written by scripts: `Tools/wgs/set_left_curve_m16.py` for the M16 left hand, `add_grip_curves.py`;
  - `AZ_Grip_L` = 1 where the clip's own left hand is on the handguard (within 12.5-16 cm of the axis, Y 5-45 cm),
    0 where it leaves it (reload, holster, melee);
  - 3-tap smoothed; "on" runs shorter than 0.35 s dropped; 0.15 s ramps.

## 13. State per weapon (2026-10-03)

| Weapon | Right hand | Left hand |
|---|---|---|
| M16 | N2 solve applied earlier (Python era); C++ parity exact | **Power grasp P4 applied and approved** (enclosure 242 degrees, thumb along the upper near side). Backups `AZ_Backups/2026-10-03_150822`, `_151030`, `_152158_NGP_M16_Left`. Result `Saved/NaturalGrip/NGP_M16_Left/support_final.json`; previous L2 kept as `support_final_L2_before_power.json`. |
| Pistol | to do via profiles (section 11) | PistolCup role built, untested |
| Knife | Handle role built, untested | n/a |
| Winchester / shotgun | TriggerStraight role built, untested | Support role |

Test copies of the M16 assets in `/Game/AZ/Tests/NaturalGripApply` (profiles `NGP_M16_*_ApplyTest`) can be deleted.
Nothing of this work is committed yet.
