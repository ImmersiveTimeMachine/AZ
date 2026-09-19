# CHALK — Combat-mode legs cross / Motion Matching picks the opposite strafe clip — standalone handoff

**Status:** narrowed to Motion Matching selection in the strafe databases. Root cause NOT found.
**Date:** 2026-09-18
**Audience:** a separate AI assistant reading this file ALONE.

> Self-contained on purpose. It assumes no access to this project's other notes, memory, or prior
> conversation. Every number below was measured from the running editor — game logs and asset dumps —
> not inferred. Where I was wrong earlier, the wrong claim is marked and corrected, because two of my
> hypotheses were falsified by testing and you should not re-walk them.

---

## 1. Context in one paragraph

CHALK is an Unreal Engine 5.8 third-person survival-horror project. Project root
`C:\UnrealEngine\Games\AZ`, engine `C:\UnrealEngine\Engine`. The hero is a MetaHuman pawn
(`AAZ_PawnMoverHeroCharacter`, derived from `APawn`, **not** `ACharacter`) driven by the Mover plugin.
Locomotion is a C++ state machine (`UAZ_LocomotionStateMachine`) feeding a Chooser table plus PoseSearch
Motion Matching, evaluated in `UAZ_MoverAnimInstance`. There are two locomotion modes: **Explore**
(orient-to-movement) and **Fight** (combat-ready strafe, body faces the camera, directional side-steps).

## 2. The symptom

In **Fight mode**, the character's legs visibly cross over each other — one leg swings across the other
mid-stride, feet skate. The user reports it happens "in any fight", not only in the specific feature we
were building. It is most obvious while walking and turning.

A related but **separate** symptom (standing still, rotating the camera, capsule yaws under a planted
idle) was diagnosed and is being addressed separately via turn-in-place work — do **not** conflate them.
This document is only about the moving case.

## 3. What the selection log shows

`[v2 Pick]` fires whenever the pushed animation asset changes. Fields used below:

- `cost` — `FPoseSearchBlueprintResult::SearchCost` for the MM branch
- `entry=t/len` — the time the new clip is entered at, and its length
- `dir` — `EAZ_MovementDirection`: **0 = Forward, 1 = Backward, 2 = Left, 5 = Right**
- `strafe=1` — combat-ready set is active (added specifically for this investigation)
- `seam=mm` — the entry time came from the search; `seam=lock` means a phase-locked transition seam

### 3.1 Baseline churn — seven clip changes in 1.65 s

```
03.00.17:581  StrafeLeftStop_RU  -> StrafeLeftLoop      cost=+0.15 entry=0.03 dir=1 strafe=1
03.00.17:595  StrafeLeftLoop     -> StrafeLeft135Loop   cost=+0.13 entry=0.07 dir=1 strafe=1
03.00.17:860  StrafeLeft135Loop  -> WalkBwdLoop         cost=+0.08 entry=0.33 dir=1 strafe=1
03.00.18:353  WalkBwdLoop        -> StrafeRight135Loop  cost=+0.13 entry=0.87 dir=5 strafe=1
03.00.18:591  StrafeRight135Loop -> StrafeRightLoop     cost=+0.07 entry=0.10 dir=5 strafe=1
03.00.18:971  StrafeRightLoop    -> StrafeRight45Loop   cost=+0.08 entry=0.50 dir=5 strafe=1
03.00.19:236  StrafeRight45Loop  -> WalkFwdLoop         cost=+0.08 entry=0.80 dir=0 strafe=1
```

Two things to note:

1. The first three swaps happen with **`dir` unchanged (1)** — 14 ms apart in one case. The direction
   bucket was stable; the churn is the search re-selecting, not the chooser re-rowing.
2. Entry phases are arbitrary (0.03, 0.07, 0.33, 0.87, 0.10, 0.50, 0.80). Left- and right-strafe loops
   are anti-phase, so an unmatched entry lands mid-stride.

### 3.2 The observation — and the reading of it that turned out to be WRONG

```
03.06.47:937  StrafeLeftStop_RU -> AnimPro_StrafeLeftLoop   dir=5 strafe=1
03.06.53:061  StrafeLeftStop_LU -> AnimPro_StrafeLeftLoop   dir=5 strafe=1
```

**✗ RETRACTED (2026-09-19).** I originally called this decisive proof that MM picks a left clip while
moving right. It proves nothing, for two reasons, both confirmed in review:

- `dir` is the **camera-relative** movement bucket; PoseSearch matches a **world-space** trajectory. The
  two are not comparable without the body/camera yaw at that instant, which the line does not carry.
- A **mirrored** database result keeps the ORIGINAL asset's name in the log. `AnimPro_StrafeLeftLoop`
  with `bIsMirrored` set IS the right-strafe pose. The printed name cannot distinguish the two.

Do not re-derive a bug from a clip name versus a `dir` value. To make this line diagnostic, log the
world-space trajectory direction and the selected result's `bIsMirrored` flag alongside the asset name.

What DOES stand from this section: competing left/right strafe entries differ in cost by only
**0.07–0.32**. These are opposite gaits and by trajectory alone ought to differ far more. That margin
is still the anomaly worth investigating first.

## 4. Hypotheses I tested and DISPROVED — do not repeat these

### 4.1 "The direction bucket has no hysteresis, so it thrashes" — WRONG

The bucket is computed with a hard 45 deg comparison in `UAZ_MoverAnimInstance` with no hysteresis, which
looked like a cause. I added a tunable margin (`MovementDirectionHysteresisDeg`, default 12 deg, still in
the code and harmless). **It did not help**: §3.1 shows the clip churning while `dir` is constant, so the
bucket was never the driver.

### 4.2 "Motion Matching switches too eagerly; make it sticky" — WRONG, AND IT MADE THINGS WORSE

`ContinuingPoseCostBias` was `-0.01` on every locomotion loop database, against an observed cost spread of
0.07–0.32 — i.e. the current clip had effectively no protection. (For contrast, the landing databases
`PSD_AZ_Stand_Run_Lands` / `PSD_AZ_Stand_Idle_Lands_Move` use `-5.0`.) I raised the three strafe databases
to `-0.3`.

Result: churn **did** drop (a swap every 1.5–3.5 s instead of seven in 1.65 s), but the user reported the
crossing got **visibly worse**, and §3.2 explains why: stickiness makes the character *hold* a
wrong-direction clip instead of stumbling out of it quickly. **This is the key insight** — the defect is
not "switches too often", it is "picks the wrong clip". Frequent switching was masking it.

**Reverted.** All three strafe databases are back at `-0.01`. Do not re-raise it before the selection
itself is correct; it will amplify the bug.

### 4.3 "Combat mode is not engaging, so Explore clips play" — WRONG

I claimed this from clip names and it is false. The strafe databases also contain the forward clips, so
`AnimPro_WalkFwdLoop` at `dir=0` is what **both** an Explore walk and a combat walk look like in the log.
I added a `strafe=` field to `[v2 Pick]` to settle it: it reads `1` throughout the combat sections.

## 5. The assets involved

Assigned on the AnimBP CDO `/Game/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC`. Weapon animation
profiles (`DA_WeaponAnim_P01`, `DA_WeaponAnim_Pistol`) leave all three **None**, so the ABP defaults are
always what is searched:

| Slot on the ABP | Database |
|---|---|
| `StrafeWalkDatabase` | `PSD_v2_StrafeWalk` |
| `StrafeRunDatabase` | `PSD_v2_StrafeRun` |
| `StrafeCrouchDatabase` | `PSD_v2_StrafeCrouch` |

Selection code path: `UAZ_MoverAnimInstance`, the branch guarded by
`ChooserContext.bStrafe && SMState == LocomotionLoop && !bStandingSprintLoop`. It replaces
`AssetsToSearch` with the single gait-appropriate strafe database, so MM searches the whole 8-way set by
trajectory rather than refining inside one chooser-picked clip. A null database silently falls through to
the ordinary single-clip path.

Content inventory for the unarmed set: **Walk Strafe = 12 clips** — Left/Right plus 45/135 loops, starts,
and stops (LU/RU). There is no turning-while-strafing content; strafe loops carry no yaw of their own.

## 6. Where I would look next, in order

1. **The schema of `PSD_v2_StrafeWalk`.** Why is the cost gap between an opposite-direction pair only
   ~0.2? Inspect the feature channels and their weights — specifically whether trajectory (future
   velocity / facing) is weighted meaningfully against pose channels, and whether the trajectory sample
   times are long enough to distinguish left from right at walk speed.
2. **Mirroring / indexing.** Check whether any strafe entry is indexed with a mirror flag that
   contradicts its clip name, which would make "best match for moving right" legitimately return the
   left-stepping asset.
3. **The trajectory actually fed to the search in strafe mode.** In combat mode the reference frame is
   the camera yaw, not the actor: `RefForward`/`RefRight` come from `ChooserContext.AimingRotation`, and
   `Dir2D` from `MoveIntentWS` (falling back to `PredictedFutureVelocity`). Confirm the trajectory handed
   to PoseSearch is in the space the database was sampled in. A space mismatch would explain both the
   tiny cost gaps and the inverted picks.
4. **Phase continuity across loop→loop swaps.** The project has a phase-locked seam (internally "R13") but
   it only applies to transition→loop pushes, never loop→loop, which always go through the search with
   `seam=mm`. Even with correct direction selection, anti-phase entries will read badly.

One more observation, seen **once**, possibly unrelated and possibly an index artifact: a pick logged
`cost=+3.40282e38` (FLT_MAX), which in this codebase means "no index / empty search". It appeared shortly
after I had edited and re-saved those databases twice, so it may simply have been a transient re-index.
Worth confirming the strafe database indices are valid and current before drawing conclusions from cost
numbers.

## 7. Hard constraints for anyone touching this project

- Never call `compile_blueprint`, `ReconstructNode`, or `save_loaded_asset` on AnimBPs from Python — it
  crashes the editor through a Python/UE garbage-collection collision. Compile AnimBPs in-editor.
- Writing a `TArray<FStruct>` back from Python **silently does not stick** (verified on montage
  `SlotAnimTracks`). Always verify by re-reading the property, and save by path then verify by file mtime;
  `save_loaded_asset` can return False without saving. `EditorLoadingAndSavingUtils.save_packages` is
  reliable.
- Live Coding cannot add or remove a `UPROPERTY` or a class member safely — those need a full build with
  the editor closed. Function-body edits are fine.
- Ask before starting Play-In-Editor; the user runs the editor tests and shares logs.
- Do not apply blanket scale or retarget changes to the character to compensate for animation problems.
- Do not add automated tests unless asked.

## 8. Reproducing the measurement

Enable the pick log (it is on by default in editor builds) and read:

```
C:/UnrealEngine/Games/AZ/Saved/Logs/AZ.log
```

Filter for `[v2 Pick]` while walking in combat mode and turning. The fields that matter are `dir`,
`strafe`, `cost`, and `entry`. The defect is present whenever a `Strafe*Loop` clip's stepping direction
disagrees with `dir` (0 = Forward, 1 = Backward, 2 = Left, 5 = Right), or whenever consecutive swaps
enter at unrelated phases while `dir` holds steady.
