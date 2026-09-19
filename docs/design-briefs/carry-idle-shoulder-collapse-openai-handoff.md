# CHALK — Carry Idle "shoulder collapse" — standalone handoff

**Status:** root cause identified and verified. Fix NOT yet applied.
**Date:** 2026-09-18
**Audience:** a separate AI assistant (e.g. OpenAI/Codex) reading this file ALONE.

> This file is deliberately self-contained. It assumes no access to the project's
> other instruction files, memory, or prior conversation. Every number below was
> measured directly from the assets in the running Unreal Editor via Python, not
> inferred. Where I was wrong earlier, the wrong claim is marked and corrected.

---

## 1. Context in one paragraph

CHALK is an Unreal Engine 5.8 third-person survival-horror project.
Project root: `C:\UnrealEngine\Games\AZ`. Engine: `C:\UnrealEngine\Engine`.
The hero is a MetaHuman-based pawn (`AAZ_PawnMoverHeroCharacter`, which derives
from `APawn`, NOT from `ACharacter`) driven by the Mover plugin.
We are building a throwable (grenade) system: the player equips a grenade, a
"carry idle" pose plays, then holding RMB aims and releasing throws.

## 2. The reported symptom

User report, verbatim:

> "When I equip a grenade and begin preparing to throw it, the character's
> shoulders compress inward, making the upper body look as though it is
> shrinking. Please investigate the transition from the carry idle into the throw
> preparation and aiming animations. Check whether this comes from the source
> animation, retargeting, or upper-body blending—particularly the spine and
> clavicle transforms. The character should maintain natural body proportions
> without the shoulders collapsing during preparation. Identify the cause before
> making changes."

## 3. Verdict

**One asset is defective: `AZ_RTG_MH_Throw_CarryIdle`.**
It has bone *lengths* that do not match the skeleton every other clip uses.
Shortened clavicles plus a ~1.39x shortened arm chain IS the visible
"shoulders compressed inward / upper body shrinking".

**The perceived direction is inverted from the report.** The character is not
shrinking when preparation begins. It is *already* undersized during the carry
idle, and snaps to correct proportions the moment `Throw_Start` takes over.
The equipped idle and the transition are two views of the same single bad clip.

## 4. Evidence

All six throwable clips share one skeleton:
`/Game/MetaHumans/Common/Female/Medium/NormalWeight/Body/metahuman_base_skel`

Bone lengths are the LOCAL-space translation magnitude of each bone, sampled at
frame 0 via `unreal.AnimPoseExtensions.get_anim_pose_at_time(...)` with
`AnimPoseSpaces.LOCAL`. Bone length is **skeleton geometry**: it must be
byte-identical across every clip that targets the same skeleton. Units are cm.

```
bone         CarryIdle  Throw_Start  ThrowLoop  ThrowEndClose  ThrowEndFar  ThrowCancel
pelvis           85.00        90.26      75.54          75.54        75.54        75.54
spine_01          4.22         1.59       1.61           1.61         1.61         1.61
spine_02          0.37         4.27       4.27           4.27         4.27         4.27
spine_03          5.23         6.75       6.75           6.75         6.75         6.75
spine_04          7.27         7.90       7.90           7.90         7.90         7.90
spine_05          9.08        11.49      11.49          11.49        11.49        11.49
clavicle_l        7.30         8.78       8.78           8.78         8.78         8.78
upperarm_l        9.62        13.34      13.34          13.34        13.34        13.34
lowerarm_l       17.23        23.84      23.84          23.84        23.84        23.84
hand_l           17.05        22.97      22.97          22.97        22.97        22.97
neck_01          12.17        14.17      14.17          14.17        14.17        14.17
head              4.84         5.28       5.28           5.28         5.28         5.28
```

Five of six clips agree to the centimetre on every bone. `CarryIdle` is the sole
outlier on all of them.

Note `pelvis` is the only row where the "good" clips disagree with each other
(90.26 vs 75.54). That is expected and is NOT a defect: pelvis local translation
is a root-relative *position* (hip height for that pose), not a bone length.
Ignore the pelvis row when judging geometry.

### Resulting world-space spans (frame 0)

```
span            CarryIdle   Throw_Start   ratio
clavicle L<->R       0.98          1.63   x1.66 too narrow
upperarm L<->R      19.47         27.21   x1.40 too narrow
hand     L<->R      32.29         44.76   x1.39 too narrow
```

### Why it is not a uniform scale

- Every one of the 342 bones has **unit local scale** (1.0, 1.0, 1.0) in both
  CarryIdle and Throw_Start. There is no scale track. (I initially claimed "the
  clip is scaled down" — **that was wrong**, and checking the scale tracks
  disproved it.)
- `spine_01` and `spine_02` nearly *swap* values between the clips
  (CarryIdle 4.22/0.37 vs correct 1.59/4.27). A uniform scale cannot do that.

This signature — differing bone lengths, unit scale, a near-swapped spine pair —
means the clip's animation tracks carry the proportions of a **different rest
hierarchy / source rig** that was never conformed to `metahuman_base_skel`.

### Provenance

```
AZ_RTG_MH_Throw_CarryIdle  import source: .../MovementAnimsetPro/SourceFiles/EXPORT/RootMotion/ThrowLoop.fbx
AZ_RTG_MH_Throw_Start      import source: .../MovementAnimsetPro/SourceFiles/EXPORT/RootMotion/Throw_Start.fbx
retarget_source: None      (on BOTH clips)
```

Two things stand out:

1. CarryIdle's import filename is `ThrowLoop.fbx` — not a carry-specific source.
   The carry idle was hand-baked from the loop clip during three earlier rebake
   attempts, and one of those bakes picked up the wrong body proportions.
2. `retarget_source` is `None`, so nothing rescales those non-conforming
   translations onto the real skeleton at evaluation time. The bad numbers pass
   straight through to the pose.

### Ruling out the three candidates the user named

- **Source animation / bake — YES, this is the cause.** The defect is static:
  CarryIdle's measurements are identical at frame0 / 25% / mid / end (it is a
  single-pose clip), so nothing is animating the shrink.
- **Retargeting — contributing, not causal.** `retarget_source: None` means the
  bad geometry is not corrected at evaluation. Setting a retarget source is a
  possible mitigation but does not remove the bad data.
- **Upper-body blending — RULED OUT.** The upper-body mask is `spine_01` at
  BlendDepth 1; a layered blend cannot change bone lengths. The defect
  reproduces on the raw asset with no blending involved at all.

## 5. Current state of the files

```
Content/AZ/Assets/Throwables/Anims/
  AZ_RTG_MH_Throw_CarryIdle.uasset                      <- STILL DEFECTIVE (upperarm_l = 9.62)
  AZ_RTG_MH_Throw_CarryIdle.bad-bone-lengths.uasset.bak <- backup of the same defective data
  AZ_RTG_MH_Throw_Start.uasset                          <- correct (13.34), untouched
  AZ_RTG_MH_ThrowLoop / ThrowEndClose / ThrowEndFar / ThrowCancel  <- correct, untouched
```

All six assets load cleanly. Nothing is lost or corrupted.
`AM_AZ_Throw_Carry` (the montage) still references `AZ_RTG_MH_Throw_CarryIdle`
on slot `RifleFire`, unchanged.

## 6. Fix attempts so far, and why they failed

The user chose "route 1": re-bake the carry pose from `Throw_Start` frame 0 onto
the correct skeleton. Two mechanisms were tried.

**Attempt A — fresh asset via `AnimSequenceFactory` (VERIFIED CORRECT, then lost).**
Created `AZ_RTG_MH_Throw_CarryIdle_Rebake`, wrote all 342 bone tracks from
`Throw_Start` frame 0, and verified: every bone length matched `Throw_Start`
exactly, spans became 1.63 / 27.21 / 44.76, pose static across the clip.
This approach WORKS. I then deleted this asset while trying attempt B, before
confirming B worked — a mistake. It is cheap to recreate.

**Attempt B — overwrite the original asset in place (FAILED SILENTLY).**
Called `remove_all_bone_tracks` + `add_bone_curve` + `set_bone_track_keys` on the
existing `AZ_RTG_MH_Throw_CarryIdle` controller. It reported "342 tracks
rewritten" and the data model key count did change (50 -> 26), but evaluation
still returned the OLD geometry (9.62). Reading the raw track back showed its
`name` property as `None`, i.e. the newly added curves are not resolving to real
skeleton bones, so evaluation falls back to the previous/ref data.
**Do not retry attempt B unmodified.** A controller write that "succeeds" is not
evidence the evaluated pose changed — verify by re-evaluating the pose.

## 7. Recommended fix

Use **attempt A**, then repoint the montage:

1. Create a fresh `UAnimSequence` via `AnimSequenceFactory` with
   `target_skeleton = metahuman_base_skel`.
2. Sample `AZ_RTG_MH_Throw_Start` at t=0.0 in `AnimPoseSpaces.LOCAL`.
3. Write all 342 bone tracks as constant keys (26 keys @ 30fps = 0.8333s,
   matching the original carry length).
   API notes for 5.8: the controller is a **property** (`get_editor_property("controller")`),
   not `get_controller()`. `set_number_of_frames` needs a `unreal.FrameNumber`
   struct, not an int.
4. **Verify by re-evaluating the pose**, not by trusting return values:
   assert every bone length equals `Throw_Start`'s, and that the spans are
   1.63 / 27.21 / 44.76.
5. Repoint `AM_AZ_Throw_Carry`'s segment to the new asset.
   Caveat: `AnimSegment.StartPos` is read-only from Python, and writing back the
   `slot_anim_tracks` array by value does NOT stick (struct-array copy). Verify
   the montage's `anim_reference` by readback before saving; if Python cannot do
   it, reassign the segment by hand in the Montage editor.
6. Save by path and verify by file mtime. (`save_loaded_asset` can return False
   without saving; `EditorLoadingAndSavingUtils.save_packages` is reliable.)

**Open question for whoever applies this:** step 2 assumes the carry pose should
be `Throw_Start` frame 0 (hands 44.76 cm apart — the authored ready stance). The
user previously wanted carry and held-aim to be *distinct* presentations, and
rejected three earlier carry poses. If a distinct relaxed hold is wanted instead,
sample that pose — but it MUST come from a clip with correct geometry, i.e. any
of the five good clips, never the current CarryIdle.

## 8. Hard constraints for anyone touching this

- Never call AnimBP `compile_blueprint`, `ReconstructNode`, or
  `save_loaded_asset` on AnimBPs from Python in this project — it crashes the
  editor via a Python/UE GC collision. Compile AnimBPs in-editor.
- Do not retarget or reparent the working AnimBP, and do not change its target
  skeleton.
- Do not apply a blanket scale change to the character or props to compensate.
  An earlier "1.65x character scale" claim in this project was an over-claim
  derived from a single bone and is retracted; do not act on it.
- Do not add automated tests unless explicitly asked.
- Ask before starting Play-In-Editor; the user runs editor tests and shares logs.
- Source art packs stay unmodified; create project-owned copies.

## 9. Reproduce the measurement yourself

```python
import unreal, math
BASE = "/Game/AZ/Assets/Throwables/Anims/"
CHAIN = ["spine_01", "spine_02", "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "neck_01", "head"]
for n in ["AZ_RTG_MH_Throw_CarryIdle", "AZ_RTG_MH_Throw_Start", "AZ_RTG_MH_ThrowLoop"]:
    a = unreal.EditorAssetLibrary.load_asset(BASE + n)
    p = unreal.AnimPoseExtensions.get_anim_pose_at_time(a, 0.0, unreal.AnimPoseEvaluationOptions())
    out = []
    for b in CHAIN:
        t = unreal.AnimPoseExtensions.get_bone_pose(p, b, unreal.AnimPoseSpaces.LOCAL).translation
        out.append("%s=%.2f" % (b, math.sqrt(t.x ** 2 + t.y ** 2 + t.z ** 2)))
    print(n, " ".join(out))
```

Expected: `Throw_Start` and `ThrowLoop` agree on every value; `CarryIdle` differs
on every value. If that is no longer true, the fix has been applied.
