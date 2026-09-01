---
name: project-motion-warping
description: "Motion warping on Mover pawns — how it hooks in, what silently no-ops it, and the AZ_ABP_Chalkie routing hazards found while wiring it up (2026-08-02)"
metadata: 
  node_type: memory
  type: project
  originSessionId: 5bce0c20-5866-4582-9e79-760a09865698
  modified: 2026-09-01T04:32:56.072Z
---

# Motion Warping in AZ (built 2026-08-02, branch `feature/NPC`)

## The chain — and the single gate everything hangs off

```
AnimGraph → "RootMotionDelta" mesh attribute        (requires ShouldExtractRootMotion())
  ↓
FLayeredMove_RootMotionAttribute::GenerateMove       RootMotionAttributeLayeredMove.cpp
  ├─ if (bDidAttrHaveRootMotion)                     :132   ← EVERYTHING IS INSIDE THIS
  ├─ builds FMotionWarpingUpdateContext              :100-105
  └─ MoverComp->ConvertLocalRootMotionToWorld(...)   :146
       ↓
UMoverComponent::ConvertLocalRootMotionToWorld → ProcessLocalRootMotionDelegate   MoverComponent.cpp:2081-2085
       ↓
UMotionWarpingMoverAdapter  ← auto-created if the actor has a UMotionWarpingComponent   MoverComponent.cpp:286
       ↓
UMotionWarpingComponent → URootMotionModifier::ProcessRootMotion → warped delta → FProposedMove
```

**Mover has first-class warping support** (`MotionWarping` is a *public* dep of the Mover module).
Integration cost = adding a `UMotionWarpingComponent` to the pawn. Nothing references anything.

**`bDidAttrHaveRootMotion` keys on the attribute's PRESENCE, not magnitude** — an in-place clip with
`bEnableRootMotion=true` still passes the gate.

## ★ The bug that cost an evening: RootMotionMode

`UAZ_InfectedAnimInstance : public UAnimInstance` — it derives from the **ENGINE** class, NOT from
`UAZ_AnimInstance`, so it never inherited that class's `RootMotionMode = RootMotionFromEverything` line.
Engine default is `RootMotionFromMontagesOnly` (`AnimInstance.cpp:202`), and
`ShouldExtractRootMotion()` is true **only** for `RootMotionFromEverything` / `IgnoreRootMotion`.

Consequence on a Mover pawn: the attribute is never written → no montage can EVER move the Chalkie's
capsule (knockback, step-back, death slide) AND every motion-warp window on a Chalkie montage is
silently inert (the warp hook lives inside the same gate).

Hid for months because the Rotter's old hit-react `AM_Zombie_KB_Chase_1` has **0.0cm root travel** —
nothing to see missing. Swapping to a clip with real travel exposed it.
Fixed in `UAZ_InfectedAnimInstance::NativeInitializeAnimation`.
**Check any new AnimInstance subclass for this line.**

## SkewWarp branches on the CLIP, not on your config

`RootMotionModifier_SkewWarp.cpp` picks one of two translation paths:

| clip | branch | live knob | dead knob |
|---|---|---|---|
| HAS root translation | scale + shear (the real algorithm) | `MaxSpeedClampRatio` | `AddTranslationEasingFunc` |
| in-place (zero translation) | lerp start→target over the window | `AddTranslationEasingFunc` | `MaxSpeedClampRatio` (clamps to `animSpeed × ratio` = 0) |

Rotation is independent and **injects** rotation: `Slerp(Identity, TargetRotation, Alpha)` works on
in-place clips. **But `RotationMethod = Scale` is a silent no-op on them** — its scale factor collapses
to 0 when the clip's own yaw is 0. Use `Slerp` / `SlerpWithClampedRate`.

`EWarpTargetLocationOffsetDirection::VectorFromTargetToOwner` + `LocationOffset.X` = park N cm in front
of the target on the attacker's side, re-evaluated per frame with `bFollowComponent=true`.

## What shipped

- `AZ.Build.cs` +`MotionWarping`; `UMotionWarpingComponent` on BOTH pawn classes.
- `UAZ_BTTask_ZombieAttack`: `bUseMotionWarping`, `WarpTargetName="AttackTarget"`, `WarpApproachDistance=120`.
  Registers before `TryActivateAbilityByClass` (montage can start synchronously); clears in `Cleanup()`.
  Legacy `SetFacingOverrideWorld` gated behind `!bUseMotionWarping`.
- `UAZ_GA_MeleeAttack`: `FindWarpTarget()` (mirrors the hit sweep's cone/team/corpse filters),
  `WarpTargetName="MeleeTarget"`, `WarpSearchDistance=280`, `WarpApproachDistance`,
  and **`RootMotionSeconds`** (0 = whole montage) to cut the capsule loose early on lunge clips whose
  recovery tail keeps travelling.
- `AZ_MontageUtils`: `AddMotionWarpingNotify`, `RemoveMotionWarpingNotifies`, `AddGameplayEventNotify`,
  `AddNamedNotify`, `DumpMontageNotifies`.
- Zombie claws `AM_Zombie_Atk_L/R`: window 0.000–0.850 (hit @1.100 ⇒ 0.25s commit), Facing +
  SlerpWithClampedRate @180°/s, CircularOut easing.

## Gotchas that burned real time

1. **A bare named notify does NOT reach GAS.** `UAZ_AT_PlayMontageAndWaitForEvent` subscribes to gameplay
   event *tags* (`AddGameplayEventTagContainerDelegate`) — it never binds notify names. Hit windows must be
   `UAZ_AnimNotify_SendGameplayEvent` objects carrying an `EventTag`. That class's
   `GetNotifyName_Implementation()` returns the tag string, so a bare notify named
   "Event.Montage.Melee.Hit" looks identical in a name-only dump and does nothing.
   `DumpMontageNotifies` now prints `[SendGameplayEvent tag=…]` vs `[BARE NAME - sends no gameplay event]`.
2. **Python cannot build an `FGameplayTag`** — `TagName` is read-only, no string→tag path. Script-facing
   APIs must take `FName` and resolve via `FGameplayTag::RequestGameplayTag`.
3. `UAnimSequenceBase::Notifies` is **protected** — no Python access at all; every notify edit needs a C++ bridge.
4. `FAnimNotifyEvent::LinkMontage` doesn't exist; it's `Link(Montage, Time)`.
5. `AnimNotifyTracks` / `RefreshCacheData()` / `PostEditChange()` are editor-only — guard with
   `#if WITH_EDITORONLY_DATA` or Shipping breaks.
6. `MaxSpeedClampRatio` is **protected** on `URootMotionModifier_SkewWarp` — reach it by reflection.
7. **No API removes a bare notify.** `AddNamedNotify`'s replace matches `NotifyName`, which the real
   sender shares, so it always strips both and re-adds a bare one. Delete + rebuild the asset and
   re-point referencing BPs instead.
8. `AnimationLibrary.get_bone_pose_for_time` returns **LOCAL** bone transforms. Useless for measuring
   reach/impact on rotation-animated bones (a hand reads as a constant offset). Root-motion sampling is
   valid; bone-space measurement is not.

## AZ_ABP_Chalkie routing — latent hazards for ANY montage-driven reaction

```
Locomotion SM → LinkedAnimLayer "AdiativeCombatGrabbed" [ InPose → Slot 'DefaultSlot' → Output ] → LocoCache
StateMachine_14 → "AttackingCache"      StateMachine_74 → "TurnCache"

LayeredBoneBlend   BlendPoses_0 ← AttackingCache, weight ← var AttackAlphaBlend, BranchFilter spine_01
BlendListByBool    bActiveValue ← var Turning:  TRUE → TurnCache (NO slot) / FALSE → LayeredBoneBlend
                   → Output Pose        (blend 0.5s to true, 0.2s to false)
```

- **The slot is INSIDE the layer graph**, not the main AnimGraph — main-graph node probes won't find it.
- While `Turning` is true the whole slot path is bypassed ⇒ montages invisible + 0.2s blend back.
- `AttackAlphaBlend > 0` overlays the attack SM from **spine_01 up**, overriding a montage's upper body.
- Durable fix (still TODO): a full-body `Slot 'DefaultSlot'` **after** the `BlendListByBool`, last before
  Output Pose — same placement the grab layer needs.

## Level fact

3 Chalkies, each with a **per-instance `AnimSet` override**: `BP_AZ_Chalkie2`=A_Standard,
`3`=C_Rotter, `4`=B_Runner. Editing one DA only affects its instances.

## Verdict on the heavy-punch experiment

`RTG_RM_Fists_Punch_Heavy2Idle` (2.667s, 202cm travel) is an excellent *warping test* — it exercises the
scale/shear branch — but a poor primary attack: it moves the camera 2m through the shot and swallows the
victim's 25cm stumble. `AM_Fists_Punch_L` (0.3cm travel) is the working left click. If the heavy clip is
wanted, give it its own input/ability at range.

## ★★ BOUNDED BACK-STEP (2026-09-01) — the close-range fix the Min() clamp could not make

`Min(WarpApproachDistance, TargetGapLatched)` fixed the moonwalk but froze the everyday case: a standing
jab from gap 60 holds at 60 and buries the knuckle 18cm past the victim's centre, and NO stand-off value
changes that. New clamp in `UAZ_GA_MeleeAttack::ActivateAbility`:
`dest = Min(standoff, gap + MaxBackstepDistance)`; for IN-PLACE clips also `dest = Max(dest, gap −
MaxInPlaceApproachDistance)`. Both properties default 15cm; at 0 it is exactly the old Min().
Why the in-place bound exists: in-place clips take SkewWarp's LERP branch where `MaxSpeedClampRatio` is
DEAD, and `BP_GA_Punch_R` has no lunge slot, so a standing jab could be selected at 250cm and would lerp
120cm in 0.17s. A jab may CORRECT its distance, never close it. `ClipRootMotion`/`bClipTravels` are now
hoisted to right after `SelectMontage()` so the clamp and the RM drive read one value.
Log line = the pass/fail: `[MeleeWarp] <hero> clip=… gap=60 standoff=130 -> dest=75 (back 15cm, inPlace=1)`.
Content that went with it (all via `AZ_MontageUtils`, hit+cancel windows verified intact):
`AM_Fists_Punch_Move_L/R` warp windows 0→contact (0.275 / 0.300, translation ON, clamp 2.0 — these
TRAVEL 70cm, scale/shear branch); `AM_Fists_Punch_L/R` warp windows 0→contact (0.168 / 0.184, HermiteCubic
easing — in-place, lerp branch). Stand-offs derived from the clips that now warp: `BP_GA_Punch_L` 120
(Move_L ∩ Heavy_L = 116.8..124.7), `BP_GA_Punch_R` 130 (Move_R 124.6..136.6). Heavy hit window trimmed
0.25→1.95 to 0.25→0.55. UNTESTED in PIE at the time of writing.

## ★ 2026-09-01 — READ THIS FILE BEFORE TOUCHING PUNCH CONTENT (it already had the answers)

Two mistakes made in one session by building before reading it:
1. Claimed the heavy's motion-warping window needed a MANUAL editor drag — false,
   `AZ_MontageUtils::AddMotionWarpingNotify` / `RemoveMotionWarpingNotifies` are listed above.
2. Added a second `AddGameplayEventNotify` to `UAZ_PoseSearchUtils` (burning a closed-editor build) when
   `AZ_MontageUtils::AddGameplayEventNotify(Montage, FName EventTagName, ...)` already existed — and it
   already takes an **FName**, which is exactly what gotcha 2 prescribes. **The AZ_PoseSearchUtils copy is
   a DUPLICATE and should be deleted**; its "invalid tag = Event.Combat.BeatEnd" bridge is a worse
   workaround for a solved problem. Use AZ_MontageUtils for ALL notify authoring.
Also confirmed still true: the "poor primary attack" verdict below — `Heavy2Idle` is currently wired as
`BP_GA_Punch_L.PunchLunge_L` (gated at `LungeMinDistance` 110), and its warp window (0→1.65) spreads the
approach across the walk-to-idle tail, so only ~14% of the gap is closed by the 0.34 contact frame.

Related: [[feedback-seam-trace-before-pie]], [[project-chalkie-fight-rules]], [[project-grab-grapple-design]],
[[reference_punch_reaction_content_inventory]]

**Method note:** three wrong diagnoses this session (BlendListByBool/`Turning` suppression, warp distance
out of reach, "no slot node") all came from reasoning over graph exports and reach arithmetic instead of
instrumenting. The `[MeleeHit]` / `[Flinch]` log lines settled it in one PIE run. Add the log first.
