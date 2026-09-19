# Throwable Milestone A — requirement ledger

**Updated:** 2026-09-18, after the exclusivity/movement-lock build.

Evidence levels: **implemented** (source written) · **built-loaded** (in the running
editor's DLL, verified by reflection probe) · **asset-saved** (on disk, verified by
mtime) · **user-verified** (Artur exercised it) · **blocked**.

## A.3 — Action locomotion lock

| Item | File | Evidence |
|---|---|---|
| `bActionLocomotionLock` on `FAZ_MoverCustomInputs` | `Source/AZ/Public/Animation/AZ_LocomotionTypes.h` | **built-loaded** — Python probe reports `action_locomotion_lock` on the loaded struct |
| `ShouldReconcile` / `Interpolate` / `Merge` (OR) / `NetSerialize` (1 bit) | same | **built-loaded** |
| Producer: derive from `Ability.State.ThrowPreparing`, zero `WorldMove` | `Source/AZ/Private/Character/AZ_PawnMoverHeroCharacter.cpp` (~line 1012) | **built-loaded** |
| Consumer: suppress residual planar velocity in the sim | `Source/AZ/Private/Character/AZ_PawnMovementMode_Walking.cpp` (after `Super::GenerateWalkMove`) | **built-loaded** |
| | | **user-verified: NO** |

Design notes that must survive future edits:
- The bit rides the **InputCmd**, never a movement-mode member: the MM trajectory
  predictor runs `GenerateWalkMove` ~60x/frame through the same mode instance, so a
  latch there is mutated by prediction steps and the live tick inherits garbage.
- It is **derived every frame from the state tag**, not latched. Any end of the
  ability — normal, LMB cancel, Run cancel, death, grab, avatar loss, a montage whose
  release cue never fired — drops the tag and therefore the lock. Nothing has to
  remember to unlock, and a late callback from a finished action cannot unlock a
  different action that is still planted (review Q3.3).
- Planar only. Gravity, supported moving-base motion and action-owned root motion are
  deliberately untouched, so an authored throw step would still play.

## A.4 — Exclusivity and Run cancellation

| Item | File | Evidence |
|---|---|---|
| Run/Sprint cancels the aim **before** GAS dispatch | `Source/AZ/Private/Player/AZ_PlayerController.cpp` (`AbilityInputTagPressed`) | **built-loaded** |
| All other voluntary input swallowed while aiming | same | **built-loaded** |
| Swallowed presses tracked; matching releases consumed | same + `Source/AZ/Public/Player/AZ_PlayerController.h` (`ThrowSuppressedInputTags`) | **built-loaded** |
| Held path gated (kills the held-retry re-activation trap) | `AbilityInputTagHeld` | **built-loaded** |
| | | **user-verified: NO** |

Why this is at the input layer rather than `BlockAbilitiesWithTag` on asset tags:
- **Verified**, not assumed: most voluntary abilities in this project declare **no
  asset tags at all** (`AZ_GA_Jump`, `AZ_GA_Crouch`, `AZ_GA_Interact` have none).
  Blocking by asset tag would have silently blocked nothing.
- `BlockAbilitiesWithTag` only stops **new activations**. Crouch is the case the
  review calls out: its already-active `WaitInputPress` would toggle the captured
  stance mid-aim. The toggle must be consumed before it reaches the task.
- Forced/system actions (death, hit react, grab) do not arrive through player input,
  so they still preempt the throw — exclusivity is voluntary-only, as required.

## A.1 — Carry routing — DONE (asset-saved)

| Item | Evidence |
|---|---|
| `SetMontageSlotName` native utility | **built-loaded** — probe reports `set_montage_slot_name` present |
| Carry montage → `Throwable` slot | **asset-saved** — readback `Throwable`, file 20:32 |
| Start/Loop → `FullBody` (standing exclusive policy) | **asset-saved** — readback `FullBody`, files 20:32 |
| Carry idle rebaked with correct geometry | **asset-saved** — v2 at 20:32, bone lengths == `Throw_Start` exactly |
| | **user-verified: NO** |

Final verified routes and geometry (every clip `upperarm_l = 13.34`):

```
AM_AZ_Throw_Carry   Throwable <- AZ_RTG_MH_Throw_CarryIdle_v2   13.34
AM_AZ_Throw_Start   FullBody  <- AZ_RTG_MH_Throw_Start          13.34
AM_AZ_Throw_Loop    FullBody  <- AZ_RTG_MH_ThrowLoop            13.34
AM_AZ_Throw_Close   FullBody  <- AZ_RTG_MH_ThrowEndClose        13.34
AM_AZ_Throw_Far     FullBody  <- AZ_RTG_MH_ThrowEndFar          13.34
AM_AZ_Throw_Cancel  FullBody  <- AZ_RTG_MH_ThrowCancel          13.34
```

**Python cannot re-route a montage slot.** `FSlotAnimationTrack` lives in a
`TArray<FStruct>` and the bindings hand back a *copy*: setting `SlotName` reports
success and writing the whole array back still leaves the asset unchanged. Verified
directly on `AM_AZ_Throw_Carry` — `before: RifleFire / set on local copy -> Throwable /
asset now: RifleFire / after array write-back: RifleFire`. Hence the native utility.
The same trap applies to `AnimSegment.AnimReference` (and `StartPos` is read-only), so
the clip swap was done with `consolidate_assets` instead of struct editing.

### Carry idle rebake (shoulder-collapse fix)

`AZ_RTG_MH_Throw_CarryIdle` carried bone *lengths* from a different rest hierarchy —
arms ×1.39 short, clavicles ×1.20 short — which was the reported "shoulders compress
inward". Rebaked from `Throw_Start` frame 0 via `AnimSequenceFactory` (342 tracks,
26 constant keys @30fps, 0.8333 s) and verified before use:

```
                 rebake   Throw_Start
clavicle_l         8.78          8.78
upperarm_l        13.34         13.34
lowerarm_l        23.84         23.84
hand_l            22.97         22.97
spans   clav=1.63  uarm=27.21  hand=44.76   (defective clip: 0.98/19.47/32.29)
static across clip: True
```

Swapped in with `EditorAssetLibrary.consolidate_assets(v2, [old])`, which redirects
every referencer engine-side. `AZ_RTG_MH_Throw_CarryIdle.uasset` is now a 1,511-byte
redirector (was 396,842) and has **zero** remaining referencers. The defective data is
preserved at `AZ_RTG_MH_Throw_CarryIdle.bad-bone-lengths.uasset.bak`.

**Open:** the carry pose is now `Throw_Start` frame 0 (hands 44.76 cm apart — the
authored ready stance). Artur previously wanted carry and held-aim to be *distinct*
presentations and rejected three earlier carry poses. If a distinct relaxed hold is
wanted, re-sample that pose — but only from a clip with correct geometry (any of the
five good clips), never the old CarryIdle.

## Corrections to earlier claims

- **Slot group is fine.** The MetaHuman skeleton resolves `Throwable -> Throwable` (a
  real distinct group); `RifleFire -> WeaponFire`; `FullBody -> DefaultGroup`. The
  `DefaultGroup` caption on the Throwable node in the AnimBP comes from the ABP
  targeting `SKEL_SurvivalMan`, exactly as the splice decision predicted. It is a
  caption artifact, **not** a runtime grouping fault. No retarget, no action.
- **Graph splice verified intact** after compile+save: 52 nodes, all seven splice nodes
  present, ABP saved 2026-09-18 17:23.

## Not touched, deliberately

- `PreviewStyle.PulseWidthPixels` reads **1.0**; the last explicitly requested value was
  **7.5** with `Brightness=1.5`. Who changed it is unverified. Flagged, preserved, not
  reset — art is Codex-owned.
- Grenade fuse/detonation, knife, weapon-context profiles, HUD producers: later
  milestones, untouched.
