# Rifle aim: upper body locked to the aim idle

**Date:** 2026-09-10 · **Branch:** `spike/cmc-backport` · **Status:** lock authored in `AZ_ABP_MoverHero_MHC` (main AnimGraph) and compilable; the crouch selector needs ONE C++ float (`AimStanceAlpha`, written, awaiting an editor-closed build) — until then the standing aim idle serves both stances.

## First compile: two bindings rejected (fixed)

The first cut bound two pins through `AZ_AnimGraphNodeUtils::SetPinBinding`; the compiler rejected both, and a
rejected binding falls back to the pin's LITERAL — the lock weight literal was 1.0, so the aim idle overrode the
upper body in every state ("we start in aim pose?", "I don't see the relax poses"):

```
Two Way Blend           Cannot copy property (EAZ_Stance -> float)   -> Alpha stuck at 0 (always standing)
Layered blend per bone  Cannot copy property (float -> TArray)       -> weight stuck at 1 (always locked)
```

- `PromoteByteToFloat` exists, but a `UENUM enum class` property is an `FEnumProperty`, not an `FByteProperty`, and
  the promotion does not unwrap it. Binding an enum to a float/int pin does not work.
- `BlendWeights_0` is an array-element pin; the utility bound the whole `BlendWeights` array (no `ArrayIndex`).
- A Blend-Poses-by-enum node spawns bound to the enum (`Animation|Blends|BlendPoses(EAZ_Stance)` via Epic MCP
  `create_node`) but with only the default pose pin; exposing entries is `ExposeEnumElementAsPin`, editor C++ only.

Fix: the weight is a wired `Get AimAlpha` K2 variable node (the exact pattern the existing aim-offset layer uses);
the stance is a new eased float `UAZ_MoverAnimInstance::AimStanceAlpha` (0 stand -> 1 crouch, from
`ChooserContext.Stance`, rate `AimStanceBlendSpeed` 6/s) bound float -> float to the Two-Way Blend's Alpha
(TwoWayBlend GUID `E7730255...`, rebuilt after the editor-closed build that added the float).

## Second failure: "Use cached pose 'RifleFireBase' does not have an associated Save Cached Pose node"

Appeared 15 s after a clean compile, with the graph structurally intact (verified on both bridges: save node
present in the node array, linked, both readers pointing at it). Cause was NOT the graph: **Live Coding had
hot-patched the anim instance header edit that added the two new UPROPERTYs** (`UnrealEditor-AZ.patch_0.exe`
at 22:14, the minute of the edit; more patches 22:30/22:32). A class-layout change by Live Coding leaves the
running `UAZ_MoverAnimInstance` inconsistent with what the AnimBP compiler expects, and the cached-pose
extension then fails to register the save node. After the editor-closed full build (22:34) and relaunch, the
same saved graph loaded **BS_UP_TO_DATE** with no changes. Rule reinforced: a header edit with new UPROPERTYs
must be followed by an editor-closed build BEFORE any AnimBP compile — never Live Coding.

## Why

Firing while standing looked right; firing while moving visibly swapped in "another animation". Root cause
(adversarial review, engine-cited): the fire clip `Stand_Fire_Single_IPC` is the aim idle plus recoil (chest within
2.5° of `Stand_Aim_Idle_IPC`), but it plays on a `spine_02`-masked slot whose BASE is the moving loop's torso. That
torso deviates from the aim idle by 2.6–9° walking forward but **30–52° when strafing** (`Walk_L` 38–41°, `Jog_L` 44–52°,
`Jog_R` 33–42°). So every shot snapped the torso to the standing pose and back. Locking the base upper body to the aim
idle makes the two poses identical, and the recoil is all that reads. Bonus: the hands are always in the pose
`RightHandRifleSocketAim` was tuned against.

## What was built (main AnimGraph, four nodes)

```
UseCachedPose 'BS Output' -------------------------------> [LayeredBoneBlend_2].BasePose
  SequencePlayer_0  AZ_RTG_MH_W2_Stand_Aim_Idle_IPC    (loop) -> A  \
  SequencePlayer_1  AZ_RTG_MH_W2_Crouch_Aim_Idle_v2_IPC (loop) -> B  / [TwoWayBlend_0] -> [LayeredBoneBlend_2].BlendPoses_0
        TwoWayBlend.Alpha  <- property binding ChooserContext.Stance (byte->float promotion; 0 stand, 1 crouch)
        TwoWayBlend.AlphaScaleBiasClamp: bInterpResult, 6/s in and out   (~0.2 s stance swap)
  LayeredBoneBlend_2: BranchFilter {spine_01, BlendDepth 4} . bMeshSpaceRotationBlend . CurveBlendOption UseBasePose
        BlendWeights_0 <- property binding AimAlpha
[LayeredBoneBlend_2] -> LinkedAnimLayer 'AdditiveLeans'.InPose -> SaveCachedPose 'RifleFireBase'
                        -> (unchanged from here: RifleFire slot layer -> aim offset layer -> FullBody slot -> ...)
```

Node GUIDs: stand `60917F59...`, crouch `7CF4FDBD...`, blend `0B6CE0E2...`, lock `F6E87CE1...`.

## Decisions the review forced (vs the first plan)

- **Inserted BEFORE the leans layer**, so the lean additive lands on the idle torso instead of leaving leaning hips
  under an upright chest.
- **Mask ramps from `spine_01` over 4 joints** (.25/.5/.75/1) instead of a hard cut at `spine_02` depth 1, which
  would have put the whole ~58° strafe twist into one vertebra ("broken back").
- **Mesh-space rotation blend is REQUIRED, not optional**: strafe loops carry the pelvis heading 42–70° off the idle;
  a local-space blend would have the torso inherit that and face 40–70° away from the aim. Mesh-space re-derives
  the mask root's local rotation against the base spine, descendants keep the idle's locals, translation follows the
  pelvis bob. Root motion untouched (root weight 0 in the mask); contact curves survive (UseBasePose).
- **Crouch selector**: a Two-Way Blend whose Alpha is bound to the C++ float `AimStanceAlpha` (see "First
  compile" above for why neither the enum node nor an enum binding could do it without C++).

## Crouched shots: per-stance fire clips, the reload pattern (built, awaiting build + data)

The two fire clips on the profile were STANDING full poses, so a crouched shot popped the torso ~25° for the
montage's length. The first proposal (make the clips mesh-space additive) was wrong for this project: **reload
already solves the same problem** by holding four poses on the profile and selecting one from stance + aim at
start (`AZ_GA_FirearmReload.cpp:52`). Fire now mirrors it inside the weapon:

- `UAZ_WeaponAnimationProfile`: `CrouchingSingleFireAnimation`, `CrouchingAutomaticFireAnimation` (optional;
  unset = standing clip, the old behaviour).
- `AAZ_Weapon`: two replicated members copied in `ConfigureFirearmPresentation`, registered in
  `GetLifetimeReplicatedProps`, and selected in `Multicast_BeginFirearmAnimation` from the Mover's
  `IsCrouching()` read locally (the fire ability's RPC signature is untouched — another agent owns that file;
  the state is replicated and the clip is ~0.4 s, so a one-frame stance-edge mismatch is invisible).
- Data assigned on `DA_WeaponAnim_P01` (built 23:14, assigned + saved 23:17): `AZ_RTG_MH_W2_Crouch_Fire_Single_IPC`
  (1.00 s) and `AZ_RTG_MH_W2_Crouch_Fire_Continuous` (0.43 s) — same lengths/rates as their standing twins,
  AAT_None, no RM. Pass line in PIE: crouch + aim + fire logs `[FireAnim] begin ... sequence=AZ_RTG_MH_W2_Crouch_Fire_*`
  and the torso stays crouched through the shot.

Lesson recorded: grep the project for an existing implementation of the same need BEFORE proposing a mechanism.

## Watch in PIE

1. Fire while strafing left/right at walk and jog: no torso jump on the shot (this was the worst case).
2. Fire standing: unchanged.
3. Crouch aim: torso crouched, not standing; the stance swap eases over ~0.2 s.
4. Motion matching cost on `Jog_Aim_L/R`: the pose history collector sits at the END of the graph and samples
   spine_05 + hands, so it now scores the locked torso against loops whose torso differs. Check `[v2 Pick] cost`
   before/after; if it rises, move the collector to right after `Inertialization_1`.
5. The per-key camera offsets (`CameraAimingDirectional`) were compensating exactly the torso drift this removes;
   expect to reduce them.

## Facts corrected during the review (the earlier camera brief had them wrong)

- The pose history collector is near the END of the chain (after OffsetRootBone), not after the blend stack.
- The two hand ModifyBone nodes are live no-ops (rotation additive 0), not orphans; the orphan is `LinkedAnimLayer_1`.
- No TwoBoneIK in the main AnimGraph. `OffsetRootBone_1` is live with rotation=Accumulate, the configuration
  `feedback_offsetrootbone_mover_graph` says should not be here. Out of scope, flagged.
- The aim-offset node's X/Y pins are unconnected literals; it only works if property-bound (bindings unreadable by the
  Epic tool; readable through the node's `AnimGraphNodeBinding_Base_0` sub-object).


## Aim turn-in-place (2026-09-11) — "pace the body to the clip"

**Why.** In ADS the body tracks the camera continuously (no free-look hold — a 60 deg cone was tried and rejected
by the user as lag, and it is not how AAA third-person ADS works). At REST that means the capsule rotates under a
standing idle and the feet slide. AAA fixes this with turn-in-place: the rotation is ANIMATED, so the time it
takes reads as the character turning rather than as input lag.

**What happens now.** While aiming, not moving, and the body is >= 35 deg off the camera, the SM enters
`IdleTurnLeft/Right` and the chooser pushes the aim turn-in-place LOOP; the walking mode paces the facing spring
to that clip's angular rate. Exit at <= 6 deg (hysteresis), on aim ending, or the instant a move starts (a
move-start out-ranks the turn and takes the plain forward start, never a bucketed RM turn).

| piece | where |
|---|---|
| enter/exit + state | `AZ_LocomotionStateMachine.cpp` — `IdleTurnLeft/Right` cases, constants `AimTurnInPlaceEnterDeg` 35 / `ExitDeg` 6 |
| body->camera angle | new SM input `AimYawDeltaDeg` = `ChooserContext.RotationOffset` (every tick, unlike `PendingStartAngleDeg` which only updates while moving) |
| pacing | `AZ_PawnMovementMode_Walking` — `AimTurnInPlaceRateDegPerSec` 67, latched `bAimTurningInPlace`; `FacingSmoothingTime = |delta| / rate` (a spring's peak rate is ~delta/T, so a delta-proportional T gives near-constant angular velocity) |
| clips | rows 304-307 on `CHT_v2`, duplicated from the dead unarmed rows 77/78 so every tag/gating cell is inherited; only bIsAiming, Stance and the asset differ. Rows 77/78 pinned to bIsAiming=False so the aim rows cannot lose an ordering race (duplicates append; first match wins). |
| content | the four aim TIP clips set to `loop=True` — the paced turn's length is the ANGLE, not the clip (90 deg = two 45 deg cycles); without looping the feet freeze on the last frame while the body keeps turning |

**Reused, not invented:** the `IdleTurnLeft/Right` enum values and chooser rows 77/78 already existed, vestigial
from a 2026-06 attempt that was removed because it drove rotation with ROOT MOTION (which snaps back under Mover
re-sim, see project_strafe_system). This version never uses RM: the facing spring owns rotation (deterministic,
in the networked input cmd) and the clip is cosmetic — the same split the strafe move-start turn clips use.

**The one coupling to watch:** the enter/exit thresholds exist in BOTH the SM (constants) and the mode
(UPROPERTYs). The mode decides when to PACE, the SM decides when to show the CLIP. If they drift you get a paced
turn with no clip (reads as lag again) or a clip with a snapping body.

**Not done:** the turn rate is the clip's authored 67 deg/s, so a 180 deg turn takes ~2.7 s. Raising
`AimTurnInPlaceRateDegPerSec` turns faster but slips the feet, until the clip's play rate is driven from the same
number (the `GetWeaponLoopPlayRate` path in the ABP is where that would go).


## The rifle's real problem was ground speed, not facing (2026-09-11)

**The discriminator the user gave: "pistol is perfect, rifle no".** Same movement code for both, so it had to be
data. It was.

A weapon set's loops depict whatever speed they were authored at. Measured against the live gaits (walk 165,
run 450, crouch 200), from each clip's own `speed` curve:

| set | clips depict (median) | gait | overspeed |
|---|---|---|---|
| Pistol walk loops | 159-206 | 165 | ~0%, matches — this is why it looks right |
| **Rifle walk (16 loops)** | **123** (98-130) | 165 | **+34%** |
| **Rifle jog (16 loops)** | **347** (227-399) | 450 | **+30%** |
| **Rifle crouch walk (16)** | **93** (78-102) | 200 | **+115%** |

So every rifle loop was being dragged 30-47% faster than its feet stepped, crouched more than double: a
permanent skate in all 8 directions, aiming or not. No amount of facing/spring tuning can fix that, and it was
almost certainly masking the judgement on every aim change made earlier in the session.

**Two ways to close the gap, and why we chose the second.** Raising the play rate (the profile's
`LoopPlayRateMin/Max`, which on the rifle are pinned to 1.0/1.0 while the pistol has 0.5/2.5) would need
1.32-2.22x — visible fast-motion, rejected by the user: *"play rate should be 1 in general if nor it play too
fast and it's like fast motion"*. So the CAPSULE comes down to the animation instead, which is also the honest
read: a man carrying a rifle in a ready stance walks slower than one with a pistol.

**Per-weapon gait speeds.** `UAZ_WeaponAnimationProfile` gains `WalkSpeedOverride` / `RunSpeedOverride` /
`SprintSpeedOverride` / `CrouchSpeedOverride` (0 = "this weapon has no opinion", keep the mode's value).
`AAZ_PawnMoverHeroCharacter::UpdateWeaponGaitSpeeds` applies them to the live walking mode when the committed
weapon CHANGES (early-out on the common frame), captures the mode's authored values once as the baseline so
unequipping restores what a designer tuned rather than a constant, and logs `[WeaponSpeed]`. Game-thread and
equipment-driven rather than through the sim input: equipment is replicated state, so every machine converges
and a rollback re-simulates with the values already in place — the same argument as the per-stance fire clips.

Rifle values to set: walk **120**, run **345**, crouch **90**. Pistol left empty.

**Leave `LoopPlayRateMin/Max` pinned at 1.0** on the rifle. That is the user's standing preference and, once the
speeds match, it is also correct.


## Aim turn-in-place, final shape (2026-09-11 evening) — verified in the log

Three rounds of wrong diagnosis on the "sway from side to side at the end" before the trace settled it; the
mechanism that shipped:

| piece | where | why |
|---|---|---|
| decision state | pawn `ProduceInput` (game thread): `bAimTurningInPlace` latch, `AimTurnYawRateLimitInput` | the Motion-Matching trajectory predictor runs the walking mode ~60 steps per frame; any latch on the mode object is overwritten by those steps |
| transport | `FAZ_MoverCustomInputs::AimTurnYawRateLimit` (0 = none) + reconcile/interp/serialize | every consumer of the sim (live tick, predictor, any future rollback) sees the same value |
| turn speed | `UAZ_PawnMovementMode_Walking`: fast flat spring (`AimFacingTime`) + **yaw-rate clamp after `Super::GenerateWalkMove`** | pacing via T = angle/rate ramped up over 0.3 s at large angles; the clamp is at full speed on tick one and leaves a short natural tail |
| release mid-turn | the aim branch runs while `bAiming \|\| bAimTurningInPlace`; RotationMode stays Aiming; the anim side ORs `IsAimTurningInPlace()` into `ChooserContext.bIsAiming` | stopping mid-turn handed the explore idle hold a body with 170 deg/s of spring velocity: overshoot + swing back = the sway |
| feet | `GetWeaponLoopPlayRate`: TIP clips at \|body yaw rate\| / `AimTurnInPlaceClipRateDegPerSec` (67) | feet step as fast as the capsule turns at any rate; `[v2 TipRate]` is the readback |
| clips | chooser rows 304-307 on the **_IPC** turn loops (yaw removed), looping | the non-IPC twins carry 45 deg of baked yaw: double rotation + a 45 deg pop at every loop seam |
| entry dwell | SM: 0.2 s of true idle before a turn may start | a turn right after a stop closed in 70 ms and only flashed its clip |

Measured on the shipped build (rate 240): turns of 0.62-0.79 s finishing WHILE aiming, exit delta -0.4 / -0.5
deg, body at the clamp (239-241 deg/s on the game thread), turn clip at 3.6x. Frames above the clamp exist
only OUTSIDE the turn (camera sweeps under the 35 deg enter angle, plain 0.05 s tracking, 87 of 7811 frames).

Knobs: `AimTurnInPlaceRateDegPerSec` (pawn walking mode, 240) for the turn; `AimFacingTime` (0.05) for the
sub-35 deg tracking; `AimTurnInPlaceEnterDeg/ExitDeg` must match the SM constants (35 / 6).

## Next session (user, 2026-09-11 end of day)

Use the rifle's own `AZ_RTG_MH_W2_*_Aim_Turn_In_Place_*_IPC` loops for the LOWER BODY only, only while
aiming (never relaxed). The upper-body lock already makes any turn clip lower-body-only. The machinery is
built and off behind `bAimTurnInPlaceEnabled`; the constraints are in memory
`project_aim_tip_next_session_2026-09-12`.

## Status 2026-09-12: aim turn-in-place ON

Enabled on the MHC pawn's walking mode with the rifle `*_Aim_Turn_In_Place_{L,R}_Loop_IPC` loops as legs under the
aim lock. Yaw-rate clamp tuned by the user to 360 deg/s (loop plays at 1.0, feet skate accepted for the speed),
enter 45, exit 6. Log proof at rate 67: body yaw rate pinned at exactly 67 for 142 frames, correct rows picked;
the pistol picks its own `AS_Pistol_TurnL/R_90Loop`. The telemetry console variables now default to 0
(`[v2 Facing]` logged ~60x per frame and caused in-game lag); `az.Lean.Debug` gates the lean diagnostic.
Follow-ups if the skate matters: per-weapon rate from the profile clip rate, discrete root-motion turns.
