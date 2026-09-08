# Rifle P01 jump + fall — what changed (handoff)

**Date:** 2026-09-07 · **Branch:** `spike/cmc-backport` · **Status:** implemented, PIE-verified

Handoff for the agent owning the rifle/aim workstream. All animation *selection* and *content*; no
movement physics was touched.

---

## 1. The problem

The P01 pack's jump clips are **CLOSED**: `rm_W2_*_Jump*` are full-RM jumps with a baked landing and
only ~0.3–0.5 s of air (0.70–2.30 s total). The unarmed jump is a **2-phase hybrid** whose Start clip has
an *open-ended 3.5 s fall tail* covering an arbitrarily long descent.

Feeding closed clips into the 2-phase design meant: clip ends mid-air → pose **freezes** until touchdown.

## 2. The mechanic (3-phase, profile-gated)

```
jump  -> RMAction rise -> TransitionToInAir : takeoff clip plays IN FULL
      -> takeoff clip runs out while still airborne
      -> InAirLoop : fall clip (reaction + settle)
      -> real floor contact -> land clip
```

**The gate is the takeoff clip's END, not the apex.** An apex gate was tried first and rejected: it cut a
0.80 s clip after 0.10 s, discarding the descent and landing the closed clip already contains. With the
clip-end gate, **an ordinary jump never reaches the air state at all** — it lands first. The fall clip is
therefore effectively the *walk-off / long-drop* animation.

Implementation: `FAZ_LocoSMInputs::bUseAirLoop` (from `UAZ_WeaponAnimationProfile::bUseAirLoop`) gates it,
so **unarmed is bit-identical**. "Clip finished" reuses the existing `TransitionEndTime`, stamped by
`NotifyTransitionClipPushed` with the clip's real remaining length minus
`TransitionAlmostCompleteThreshold` (0.15 s), so the swap starts 0.15 s early and blends under the clip's
tail. Only `bInTransition` states stamp it, so the air clip's own push cannot overwrite it.

`EAZ_StateMachineState::InAirLoop` (=4) is **produced again** (reserved/unused since 2026-06-14), and
`bHoldTakeoffPhase` is **load-bearing again, NOT vestigial** — old comments saying otherwise were wrong
and have been corrected in place.

### If you want jumps to show the fall too
Fire the gate at a *fraction* of the takeoff clip instead of at its end — one line in the `InAir` branch of
`AZ_LocomotionStateMachine.cpp`. Deliberately not done: the user specified clip-end.

## 3. Code changes

| File | Change |
|---|---|
| `AZ_WeaponAnimationProfile.h` | **new UPROPERTY** `bUseAirLoop` (default false) |
| `AZ_LocomotionStateMachine.h` | **new field** `FAZ_LocoSMInputs::bUseAirLoop` |
| `AZ_LocomotionStateMachine.cpp` | `MovementMode == InAir` branch: hold takeoff until its clip ends, then `InAirLoop` |
| `AZ_MoverAnimInstance.cpp` (~L833) | feeds `SMIn.bUseAirLoop` from the active weapon profile |
| `AZ_ChooserUtils.cpp/.h` | **bug fix** + `DuplicateRowOnSub` + column bindings in the dump |

⚠️ **The clip-end gate shipped as a Live Coding patch.** The last CLI build predates it. **Rebuild from
CLI** around any editor restart or the gate silently reverts to apex behaviour.

## 4. Content

**Two new clips** (authored procedurally, see §5):
`Riffle_P_W2_Stand_Relaxed_Fall_v2`, `Riffle_P_W2_Stand_Aim_Fall_v2` — in
`Content/AZ/Assets/RTG/Riffle_P01/Stand/Jump/IPC/`. **Gitignored** (`Content/AZ/*`), so they exist only on
the authoring machine — regenerate with the script if you need them elsewhere.

- `Loop = False`, `Enable Root Motion = False`, 2.00 s / 61 keys, curves preserved.
- **Frame 0 is bit-identical to the source pose** (0.000 cm), so entry from the takeoff clip has nothing to
  blend across. The clip then *performs* the fall reaction — this is a ramp A→B, which is why it cannot
  loop. Past 2 s it holds the settled fall pose (~20 m of freefall; and it holds a *falling* pose, not the
  standing one that caused the original bug).
- Reaction completes in **0.20 s** (`RAMP_FRAMES=6`), tread oscillation runs **3 cycles** per clip. Both
  were tuned from the measured air window — see §6.

**`CHT_v2` rows 302 / 303** — `c0 = InAirLoop`, `c6` (`bIsAiming`) False → relaxed, True → aim;
`bUseMM=False`, `BlendTime=0.2`. Created with `DuplicateRowOnSub` from takeoff rows 280/290 so they inherit
the weapon-tag gating; `c1` (Stance) and `c13` (`Is Moving`) then widened to **Any** — 280/290 are the
STANDING variants, so a plain duplicate would have left every *running* jump with no air row.

**`DA_WeaponAnim_P01`** — `bUseAirLoop = True`.

## 5. The pose authoring script

`scratchpad/fall_pose.py` (not in the repo — copy it out if you want to keep it). Parameterised:
`LIFT_DEG`, `ELBOW_UP/OUT`, `THIGH_*`, `CALF_*`, `TWIST`, `ROLL`, `SPLAY`, `RAMP_FRAMES`, `CYCLES`, `ARMS`.

Two properties worth preserving if you rewrite it:

- **GRIP SAFETY.** The rifle is held in BOTH hands, so the hands must keep their exact relative transform.
  One rigid rotation is applied to both hand transforms about a chest pivot, then two-bone IK solves each
  arm to follow. The grip is preserved *by construction* at any lift — measured 0.38 cm worst case on the
  relaxed clip. Rotating each bone independently (the first attempt) breaks the grip visibly.
- **`ARMS=False`** leaves the arm chain + clavicles at source values, giving **0.0000 cm** grip error. Used
  for the aim variant, where a shouldered rifle should not travel. There is an unexplained ~1.6 cm grip
  perturbation from routing the aim clip's arms through the IK round-trip *even at zero lift*; three
  hypotheses (elbow wobble, spine, twist bones) were tested and disproved. `ARMS=False` sidesteps it and is
  also the correct look. **If you need the aim arms to move, that 1.6 cm is unsolved.**

Pose is specified in **component space** (`+Y` forward, `−X` character's right, `+Z` up — matches the
project convention), so signs are verifiable rather than guessed.

## 6. Verification (PIE 03:40)

| | |
|---|---|
| air phases | 3, all **ledge walk-offs** (entered from a locomotion loop, not a jump) |
| duration each | 0.61 – 0.63 s |
| exits | all → correct foot-matched `AS_P01_Jump_*_Land` |
| `[v2 Replay]` (freeze / rewind) | **0** |
| non-rifle `SM=4` picks | **0** — unarmed untouched |

**The air window is only ~0.63 s.** An earlier build had a 0.60 s reaction ramp and a 2.0 s tread period,
so in game the player saw the first third of a slow ramp and it read as static — while the asset preview,
looping the full 2 s, looked fine. That mismatch is the trap: **tune the clip against the measured air
window, not against how it looks in the asset editor.** Current values reach full reaction at 0.20 s.

## 7. Traps found (these will bite anyone touching this area)

1. **`AddEmptyRowToSub` was silently broken on CHT_v2.** Its cell filler had no `FGameplayTagColumn`
   branch, and CHT_v2 has four tag columns. A row added that way came out shorter than those columns, and
   `FGameplayTagColumn::TestRow` returns **false** for a missing cell — the row is filtered out on every
   evaluation and can never match, with no error anywhere. Fixed via the engine's `SetNumRows` virtual.
2. **`DumpChooserFullTree` now prints each column's bound property.** Previously a bool column was an
   anonymous `"c11"`. This immediately revealed `c13 = Is Moving` and prevented a wrong row edit.
3. **`EditorAssetLibrary.save_loaded_asset(obj)` can return False and silently not save.**
   `save_asset(path)` works. **Verify saves by file mtime**, not by the return value.
4. **`delete_asset` fails once the asset is referenced** (e.g. by the chooser). A rebuild script that does
   delete → duplicate → edit then silently *stacks* edits on the previous output: feet went 21 → 47 → 77 cm
   over three runs and the pose looked wildly wrong. Fix: read the pose from the **source** every frame so
   the operation is idempotent, and never rely on delete+recreate. Verify by running twice and comparing.
5. **`delete_asset` on a referenced clip nulls the chooser cell** — row 303 became `Asset[null]` this way.
   Re-check rows after any asset delete.
6. Sequencer/Control Rig pose values are **not reliably readable from Python** outside an active
   evaluation (`get_local_control_rig_*` returned 0 for everything while the section held 60,878 keys).
   Read the section's channels instead, and note `channel.get_name()` appends a counter that changes
   between fetches — match by prefix.

## 8. Open items

- Ordinary jumps never show the fall clip (§2) — change the gate if that is wanted.
- Aim-arm 1.6 cm grip perturbation unexplained (§5).
- `[v2 Lean]` / `[v2 Mode]` diagnostics still enabled and noisy.
- The new clips are gitignored; only `CHT_v2` and the C++ are versioned.
