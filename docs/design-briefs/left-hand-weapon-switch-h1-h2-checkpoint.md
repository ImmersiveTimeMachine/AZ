# Left-hand weapon switching: checkpoints H1 and H2

Status: **implemented and built (H4, section 4); user PIE acceptance (H5) pending**. 2026-10-03.

Continues [the implementation handoff](left-hand-weapon-switch-handoff.md) (plan H1-H5). Background:
[problem statement](left-hand-weapon-switch-problem.md), [Natural Grip tool reference](az-natural-grip-tool-reference.md).

## 0. Baseline and coordination

- **Baseline of the current hand implementation** (fixes A-D as they are now), copied before any further change:
  `C:/UnrealEngine/Games/AZ_Backups/2026-10-03_214910_hand_switch_baseline/`. It holds:
  - the animation instance `.h/.cpp`, which includes the leg work as it was at 21:45;
  - the grip node and the body clearance node;
  - the equipment component and `AZ_Weapon`;
  - a SHA-1 checksum manifest (`manifest.sha1`).
- **Leg work is active in the shared files.** `AZ_MoverAnimInstance.h`, `.cpp` and `_Procedural.cpp` were modified at
  21:45 by the leg task (`FVisualMotionSample` / `FVisualMotionHistory`). They are not edited by this checkpoint.
- **Proposal to cut the shared surface.** The switch-reach code moves into its own translation unit,
  `AZ_MoverAnimInstance_WeaponReach.cpp`, which holds the gather and the plan builder. That leaves one call in the
  main `.cpp` and a declarations block in the `.h`.
- **Build.** H4 changes headers (equipment, weapon, grip node, animation instance), so it needs the editor closed and
  a full build. The leg work must be at a compiling checkpoint first.

## 1. H1 - transition ownership and per-hand weights

### 1.1 The real chain today (verified in source)

| Step | Owner | What happens |
|---|---|---|
| `BeginSelectionChange` | Equipment (authority) | Builds `FWeaponSelectionTransition`: Id, SourceItem/SourceWeapon, TargetItem/TargetWeapon, **Coordinator** (= the source weapon if any, else the target), the holster and draw `FWeaponSwitchPhase`, bCrouching |
| `BuildSwitchPhase` | Equipment | Picks the profile action (crouch falls back to stand); stores Animation, Slot, PlayRate, blends; **AttachTime = clip AttachTime / (RateScale x profile rate)**; Duration |
| `StartSwitchPhase` | Equipment | New **PhaseId**; ends the previous phase id; `Coordinator->Multicast_BeginEquipmentAnimation(PhaseId, clip, slot, rate, blendIn, blendOut)` |
| `Multicast_BeginEquipmentAnimation` | Weapon (all net roles) | Dynamic montage on the slot, `bEnableAutoBlendOut = false`; stores `EquipmentAnimationActionId = PhaseId`, `EquipmentAnimationMontage`, `EquipmentAnimationInstance` |
| `TickComponent` -> `ApplySwitchSocket` | Equipment | At AttachTime (wall seconds) the presented weapon blends to its Relaxed (draw) or Carry (holster) socket over SocketBlendDuration |
| phase end | Equipment | Holster -> `StartSwitchPhase(draw)`; draw -> `FinishSwitch` -> `ClearWeaponSwitch` (ends the PhaseId; the montage stops with BlendOut) -> `CommitSelection` |
| cancel | Equipment | `CancelWeaponSwitch` (interrupted montage, superseded request, context invalid, crouch toggled) -> `ClearWeaponSwitch` + `RefreshCarryPresentation(true, 0.1)` |

The coordinator plays **both** phases: the holster of the source weapon and then the draw of the target, on the source
weapon actor. `WeaponTransition` exists only on authority. The project is SP-first, so the authority path is the
product path.

### 1.2 What the current consumer gets wrong

| Current behavior (`AZ_MoverAnimInstance.cpp`, grip gather) | Why it violates the contract |
|---|---|
| Takes the **most recent active montage** while `IsSwitchingWeapon()` | Any other montage can become the "switch clip". During the holster->draw hand-off two switch montages coexist for 0.1 s |
| Decides draw vs holster by matching the clip against `ActiveWeaponAnimationProfile` | The profile is the committed (or draw-presentation) one, not an equipment-owned identity. A missing match silently means "no plan" |
| The plan uses `AttachTime` from the profile entry and ignores the phase play rate | Today the rate is 1, so this is harmless. It would be wrong for any rate other than 1 |
| Fix C writes the **master** `WeaponGripAlpha` | That alpha also drives the right-hand grip (`HandAlpha` = GripAlpha x curve) and `AnimNode_AZWeaponBodyClearance` (`ClearanceAlpha`). A left-hand release scales the right hand and the body clearance |
| The reach owns the left hand with `LastLeftAlpha = 0`; expiry decays only the reach alpha | When ownership returns to the normal grip, the weight jumps from the reach value to `M x curve` |
| Plan cache key = (clip, weapon class); invalid plans are cached forever | A changed grip socket, profile or hero mesh is never re-planned |

### 1.3 The snapshot

A new read-only struct (equipment header, or a small `AZ_WeaponSwitchPresentation.h`):

```cpp
/** Game-thread cosmetic view of the running switch phase. Equipment stays the only gameplay authority. */
struct FAZ_WeaponSwitchPresentation
{
    FGuid TransitionId;
    FGuid PhaseId;
    bool bHolster = false;
    bool bCrouching = false;
    bool bSocketApplied = false;
    TWeakObjectPtr<const AAZ_Weapon> PresentedWeapon;      // holster: SourceWeapon, draw: TargetWeapon
    TWeakObjectPtr<const AAZ_Weapon> Coordinator;          // plays the montage
    TWeakObjectPtr<const UAnimSequence> Animation;
    TWeakObjectPtr<const UAZ_WeaponAnimationProfile> Profile; // the presented weapon's profile
    float ClipAttachTime = 0.f;                            // seconds on the clip timeline (profile entry)
};
bool UAZ_Inv_CommonUI_EquipmentComponent::TryGetSwitchPresentation(FAZ_WeaponSwitchPresentation& Out) const;
```

Two producers are added:
- **Weapon getter.** `const UAnimMontage* AAZ_Weapon::GetEquipmentAnimationMontage(const FGuid& ActionId) const`
  returns the montage only while `EquipmentAnimationActionId == ActionId`.
- **Stored clip attach time.** `FWeaponSwitchPhase` stores `ClipAttachTime` (the `Action.AttachTime` that
  `BuildSwitchPhase` already reads).

### 1.4 Field map

| Snapshot field | Source | Consumer |
|---|---|---|
| PhaseId, TransitionId | `WeaponTransition.PhaseId/Id` | Validation: `Coordinator->GetEquipmentAnimationMontage(PhaseId)` must be non-null |
| bHolster | `WeaponTransition.bHolsterPhase` | Plan kind (replaces the profile clip match) |
| PresentedWeapon | `bHolster ? SourceWeapon : TargetWeapon` | GripWeapon for the switch; its `GripPose`, `LeftHandGrip` and sockets |
| Animation, Profile, ClipAttachTime | the phase plus the presented item's `WeaponStateFragment->AnimationProfile` | Plan builder: an explicit profile instead of `ActiveWeaponAnimationProfile` |
| bSocketApplied | `WeaponTransition.bSocketApplied` | Draw: the weapon is physically in the hand. Holster: the weapon has left the hand |
| montage instance | `GetActiveInstanceForMontage(montage)` in the animation instance | Clip time (segment `GetAnimationData`), weight W, blending out |

### 1.5 Lifetime and threads

- **Game thread, `NativeUpdateAnimation`:** the animation instance reads the snapshot and the montage instance. It
  copies plain values (clip time, weights, transforms) into `FAZ_WeaponSwitchReach`. No UObject pointer is
  dereferenced off the game thread.
- **Worker thread:** the grip node copies `FAZ_WeaponSwitchReach` in `UpdateInternal`. This is the same fence as
  today: `NativeUpdateAnimation` finishes before the parallel update.
- **Snapshot validity (checked every frame):** the snapshot must be returned by equipment, and the coordinator montage
  must match the PhaseId. The montage instance must also be active, and its current segment must be the snapshot's
  Animation.
- **When validation fails:** the switch path is off for that frame, and the ownership blend in 1.7 takes the hand back.
- **Proxies and remote pawns:** they have no `WeaponTransition`, so they get no reach. They fall back to the normal
  grip (`M x curve`), so the result is safe but not refined. This is consistent with SP-first.

### 1.6 Weights

Symbols:
- M = `WeaponGripAlpha` (master);
- cR, cL = the pose curves `AZ_Grip_R/L` read in the node;
- W = the exact switch montage instance weight;
- k(t) = the phase clip's own `AZ_Grip_L` at clip time t;
- A, G, F = reach alpha, to-grip and fingers;
- tau = the switch ownership of the left hand.

| Weight | Equation | Feeds | Changed by the switch? |
|---|---|---|---|
| Master M | `FInterpConstantTo(M, Target, dt, WeaponGripBlendSpeed)`, where Target = 1 iff the presented/active weapon hangs on Relaxed/Aim with grip data | Grip node `GripAlpha`; body clearance `ClearanceAlpha` | **Never** (fix C's write is removed) |
| Right hand | `wR = M x cR` | Right fingers | No |
| Left hand, normal | `wN = M x cL`, goal `gN = lerp(hand, grip, wN)` | Left arm IK and left fingers | - |
| Left hand, switch with a valid plan | goal `gS = lerp(hand, lerp(path(t), grip, G), A)`, fingers `A x F` | Left arm IK and left fingers | cL ignored (it is overridden by the upper-body layers, see the problem document section 4) |
| Left hand, switch without a plan (Reach 0, no grip data, plan invalid) | `wS = M x [(1 - W) + W x k(t)]` if the clip has the curve, else `M x (1 - W)`; goal `lerp(hand, grip, wS)` | Left arm IK and left fingers | cL ignored; the clip's own curve times the hand |
| Final left goal | `g = lerp(gN, gS, tau)`, with tau moving to 1 (valid switch) or 0 (none) at 10/s | Left arm IK | - |

- **Result 1:** a left-hand release can no longer scale the right hand or the body clearance.
- **Result 2:** an ownership change cannot jump. At a draw's end both goals are the grip, so the mix is exact. On a
  cancel mid-reach the hand blends from where the reach was to the normal goal over 0.1 s.

### 1.7 Interruptions

| Event | Snapshot | Left hand |
|---|---|---|
| Holster -> draw (new PhaseId) | Switches to the draw phase | The holster has already handed back (see 2.4). The draw plan's A = 0 until the attach frame |
| Switch finished (commit) | Invalid in the commit frame | tau -> 0 over 0.1 s. Both goals target the grip, so the hand-over is continuous |
| Cancel (interrupted montage, superseded, crouch toggled, context invalid) | Invalid | tau -> 0 from the last goal over 0.1 s. Equipment restores the sockets over 0.1 s |
| Full-body montage or traversal (`bArmsOwnedElsewhere`) | Ignored | M eases out (existing behavior); tau -> 0 |
| Unrelated montage in the same slot | Cannot match the PhaseId montage | No effect |
| Proxy pawn | None | Normal grip |

### 1.8 Plan cache

- **Key:** (Animation, presented weapon class, hero skeletal mesh asset, Profile).
- **Stamp:** the grip-in-bone transform (rounded to 0.1 cm / 0.1 deg), ClipAttachTime, body bone and contact radius.
- **Rebuild:** whenever the stamp differs.
- **Invalid plans:** stored with their stamp, so they are retried when an input changes rather than cached forever.

### 1.9 Files for H4

| File | Change | Header? |
|---|---|---|
| `Equipment/.../AZ_Inv_CommonUI_EquipmentComponent.h/.cpp` | The snapshot struct, `TryGetSwitchPresentation`, `ClipAttachTime` in the phase | yes |
| `Weapon/AZ_Weapon.h` | Inline `GetEquipmentAnimationMontage(ActionId)` | yes |
| `Animation/AnimNode_AZWeaponGrip.h/.cpp` | `FAZ_WeaponSwitchReach` gains tau and the no-plan left weight; the node mixes gN/gS | yes |
| `Animation/AZ_MoverAnimInstance.h` (shared with legs) | Plan struct (stamp), cache key, the new TU's declarations | yes |
| `Animation/AZ_MoverAnimInstance.cpp` (shared with legs) | The gather block is replaced by one call; fix C's master write is removed | no |
| `Animation/AZ_MoverAnimInstance_WeaponReach.cpp` (new) | The switch gather, the plan builder, and the plan evaluation (H2 rules) | - |

## 2. H2 - measurements and the chosen signals

### 2.1 How it was measured

The measurement is read-only:
- the editor Python `AnimPose` API, sampling at 30 Hz, in component space, without retargeting;
- the script is in the session scratchpad and is not a project tool.

Frames and transforms used:
- **Hero carrying bone:** `az_weapon_r`. Its socket `RightHandM16Socket` has local location (-5.66, 4.66, -1.83) and
  rotation (P -3.73, Y 102.63, R 18.36).
- **Approved grip:** `hand_in_weapon` from `NGP_M16_Left/support_final.json`, composed with that socket. This gives
  the solved grip in the `az_weapon_r` frame.
- **Authored hold** (the clip's own hand relative to `az_weapon_r`):
  - draw: the terminal frame;
  - holster: frame 0.

The **solved grip sits 8.1 cm and 33 deg from the authored hold** in all four clips. This agrees with the handoff's
8-10 cm and 34 deg, and confirms the frames. So an arrival criterion must never require zero distance to the solved
grasp.

### 2.2 Per-clip summary

| Clip | Length | Attach | Authored contact (hand within 8 cm of its hold, stays) | Current plan "arrive" | Clip release (hand leaves its hold by 3 cm) |
|---|---:|---:|---:|---:|---:|
| Stand draw | 1.533 | 0.567 | **0.987** | 1.483 | - |
| Crouch draw | 1.900 | 0.533 | **1.207** | 1.733 | - |
| Stand holster | 1.433 | 0.667 | - | 0.800 (hold until the clip rests) | **0.108** |
| Crouch holster | 1.767 | 0.567 | - | 0.800 (hold to 1.533) | **0.059** |

**Sensitivity of the contact time:**
- with a 4 / 6 / 8 cm threshold, the stand draw gives 1.016 / 0.998 / 0.987 s;
- the crouch draw gives 1.450 / 1.319 / 1.207 s.

**Why the crouch is so sensitive.** In crouch the hand touches the handguard about 9 cm from its final hold and slides
along it until about 1.63 s. The orientation also settles late (79 deg at 1.2 s, 7 deg at 1.47 s).

**Cross-check with the clip's own `AZ_Grip_L` curve.** It crosses 0.5 at 1.067 s (stand) and 1.400 s (crouch). This is
after the contact and close to the end of the slide.

### 2.3 Draw: arrival rule

- **Arrive.** The first time after `ClipAttachTime + 0.2 s` from which the authored hand stays within the **contact
  radius (8 cm, about one palm length)** of its terminal hold, in position, in the weapon frame. The value is linearly
  interpolated between samples.
- **Orientation is not a criterion.** It blends during the meet window (`G` over `WeaponReachMeetTime`). After that
  the IK holds the solved grip while the clip's hand finishes its slide underneath.
- **Effect:** the arrival comes **0.50 s earlier standing** (1.483 -> 0.987) and **0.53 s earlier crouched**
  (1.733 -> 1.207). This is the late-meeting defect that the first PIE run showed.
- **Generality:** the rule reads only the clip's own hand-to-weapon relation. Nothing is specific to the M16, so a
  clip without a terminal hold produces an invalid plan, which falls back to the no-plan weighting.
- **Unchanged:** the path math (quintic from the clip hand at the attach frame, with the clip velocity capped, to the
  solved grip at Arrive; min-jerk slerp).
- **Path deviation:** it now leads the clip hand by up to 20 cm (stand, 0.93 s) because it heads for the weapon's
  final grip earlier. That is the intended "the hand goes to where the gun will lie".

The stand draw, current plan compared with the proposed plan:

| t (s) | Current plan: hand moved from the clip hand | Proposed plan: hand moved from the clip hand |
|---:|---:|---:|
| 0.733 | 5.5 cm | 7.3 cm |
| 0.933 | 14.4 cm | 19.6 cm (to-grip G = 0.58) |
| 1.000 | 17.0 cm | 10.2 cm, on the grip |
| 1.200 | 14.0 cm, still travelling | 8.7 cm, on the grip |

### 2.4 Holster: no path

The measurement changes the holster design:
- **The clip's own left hand never follows the gun.** It leaves the hold at 0.06-0.11 s and goes straight to the
  relaxed pose. In the body (`spine_04`) frame it travels about 20 cm or less (standing) and 12 cm or less
  (crouching), at 70 cm/s or less. Meanwhile the hand-to-gun distance grows to 80 cm as the gun goes to the back.
- **So the old arm fold was purely the IK** holding the grip.
- **The current chest-frame path adds the problem.** It goes to the clip's end pose at 0.8 s and holds it until
  1.533 s in crouch.

Proposed holster:
- G and A both go 1 -> 0 over `[0, max(WeaponReachReleaseTime, clip release)]`, which is 0.12 s for both M16 clips.
- The hand blends from the live grip onto the clip's own hand.
- No path, no rest target and no hold.
- After the release, the left override stays 0 until the phase ends. Any `AZ_Grip_L` in the pose is ignored.
- **Content note (protected clip, not touched):** the crouch holster's own `AZ_Grip_L` returns to 1 from 1.1 s
  to the end, while the weapon is on the back. The override makes this harmless.

### 2.5 Leg check (approximation, input to H3)

The check approximates the live pose:
- the clip's upper body placed on the live pelvis (local layering above the pelvis);
- the legs taken from `AZ_RTG_MH_W2_Crouch_Idle`. All six rifle crouch idle variants have identical frame-0 legs.
- proxy radii: thigh 8 cm, calf 6 cm, hand 4 cm.

Clearance results (negative = penetration):

| Case | Minimum clearance |
|---|---:|
| Crouch draw, raw clip hand over the live legs | **-1.4 cm** at 1.0-1.07 s. The draw clip's own knees are 15-20 cm from the idle's |
| Crouch draw, current reach | -1.5 cm at 0.73-0.87 s |
| Crouch draw, proposed reach | -0.4 cm (0.73 s), then 4 cm or more |
| Crouch holster, current reach (hold) | 0.6 cm at 1.40-1.47 s, against the clip's own hand at 2.5 cm or more |
| Crouch holster, proposed (no path) | Equal to the clip's own hand |

What this shows:
- Both the base clip layering and the old path bring the hand to the left thigh/knee, and the proposed timing
  reduces it.
- The deep penetration in the user's screenshot is larger than this approximation. Several things that the offline
  model does not include can explain it:
  - the unarmed crouch legs before the draw presentation, and their cross-fade;
  - foot placement;
  - the real layer blend space;
  - the elbow and forearm under IK.
- Therefore H3 (runtime clearance against the **live** post-foot-placement legs) remains required. It must not be
  replaced by tuning these numbers.

## 3. Next

1. **Review this checkpoint:** the snapshot, the weights table, the 8 cm contact rule and the holster without a path.
2. **H3 design:**
   - left-arm clearance in the grip node, using `PHYS_MHC_Hero` thigh and both calf capsules, pelvis and torso;
   - the hand as a box; hand, wrist, elbow and forearm segment checks;
   - applied to both gN and gS, so the clip's own layering is covered as well.
3. **H4:** after the leg task's compiling checkpoint, take the shared files, implement, then do a full build with the
   editor closed.
4. **H5:** user PIE per the handoff table, with `az.Weapon.Reach 2` logs.

## 4. Implementation (H4), 2026-10-03

The user released the shared files ("legs finished") and asked for implementation and a build. Implemented as
designed in sections 1-2, plus a bounded H3:

| File | What changed |
|---|---|
| `Equipment/.../AZ_Inv_CommonUI_EquipmentComponent.h/.cpp` | `FAZ_WeaponSwitchPresentation`, `TryGetSwitchPresentation`; the phase keeps its `Profile` and `ClipAttachTime` |
| `Weapon/AZ_Weapon.h/.cpp` | `GetEquipmentAnimationMontage(ActionId)`: the phase montage, or null once the phase has ended |
| `Animation/AZ_MoverAnimInstance.cpp` | The grip gather takes the presented weapon from the snapshot (replaces fix A's search). The master `WeaponGripAlpha` only eases to the in-hands target (fix C's master write is removed), then calls `UpdateWeaponSwitchReach` |
| `Animation/AZ_MoverAnimInstance_WeaponReach.cpp` (new) | `az.Weapon.Reach`, the exact-montage gather, plan cache, plan builder and per-frame evaluation, all moved out of the shared file |
| `Animation/AZ_MoverAnimInstance.h` | Plan struct with build inputs and a per-phase guard; 4-part cache key; new tuning (`WeaponReachContactRadius` 8 cm, `WeaponReachHoldTolerance` 15 cm, `WeaponReachOwnershipBlendTime` 0.1 s); `WeaponReachRestTime` and `WeaponReachBlendOut` removed |
| `Animation/AnimNode_AZWeaponGrip.h/.cpp` | `FAZ_WeaponSwitchReach.Ownership` (tau) replaces `bOwnsLeftHand`. The node mixes the normal goal and the switch goal by tau and adds the left-arm clearance |

How the runtime behaves:

- **Draw.** `Arrive` is the authored contact (8 cm from the clip hand's final hold, in the weapon frame). The path is
  unchanged: quintic plus min-jerk.
- **Holster.** No path. The grip is let go of over `max(WeaponReachReleaseTime, clip release)`, and the clip's own hand
  takes over. The switch keeps owning the hand until the phase ends, so the pose's `AZ_Grip_L` is ignored.
- **No plan** (Reach 0, no grip data, or a clip that is not two-handed): left weight = `M x [(1 - W) + W x k(t)]`.
  Only the left hand is affected.
- **Plans** are built at most once per switch phase. Their inputs are re-checked at each new phase, and an invalid plan
  is retried when an input changes.

**Left-arm clearance (the bounded H3)** lives in the grip node:
- It is active only while a switch owns the hand (weight tau), so ordinary holding, aiming and the approved grip are
  untouched.
- **Obstacles:** the Physics Asset bodies whose bone name starts with `thigh_`, `calf_` or `foot_` (all capsules,
  spheres and boxes), taken from the live pose at this node's place in the graph (after foot placement), with radii
  x 0.8 plus a 0.5 cm margin.
- **The torso is excluded:** the arms rest against it in ordinary poses, and no torso penetration was reported.
- **Correction:**
  1. The hand goal is pushed out (wrist and palm spheres of 4 cm, up to 3 relaxation passes, at most 20 cm).
  2. Then the elbow swings about the shoulder-to-hand line if the forearm (4 cm) is still inside.
  3. Both pushes are smoothed at 30/s and weighted by tau.
- **Coverage:** the correction also covers the clip's own arm during a switch, because the base layering alone measured
  about -1.4 cm (2.5).
- **Diagnostics:**
  - `az.Weapon.Reach 2` logs the penetration, the pushes and the residual;
  - `3` also draws the obstacle axes and the goal sphere;
  - the node's debug line shows switch ownership and the left-arm residual.

**Build:**
- **Result:** a full editor-closed build succeeded with 0 errors, and the second build added the per-phase plan guard.
  It also compiled the leg task's unbuilt code (`AZ_MoverAnimInstance_Procedural.cpp`).
- **Before PIE:** the user must compile `AZ_ABP_MoverHero_MHC` (Ctrl+F7), because the grip node and the animation
  instance changed layout.

Not done:
- the Physics Asset radii were not read live (the editor was closed), so the real proxy sizes are verified only in PIE;
- the torso and upper-arm clearance;
- proxy and remote pawns (SP-first).

## 5. User PIE of H4 (2026-10-03 22:25-22:28) and the redesign

**Result:** rejected. "The hand jerks and jumps." The user suggested playing the plain clip and correcting only at the
very last moment.

### 5.1 Evidence (`Saved/Logs/AZ.log`, `az.Weapon.Reach 3`)

- **The clearance fights itself.** In the crouch draw (0.74-1.03 s) and the crouch holster (0.4-1.3 s), the hand push
  and the elbow push alternate from frame to frame. Examples:
  - hand push 4.9 -> 5.5 -> 9.2 -> 7.7 -> 5.7 -> 7.5 -> 8.8 cm;
  - elbow push 10.9 -> 2.0 -> 1.3 cm;
  - the residual stays at 5-8 cm.

  The solver never clears the arm. This is the visible jerk.
- **The clip's own arm is inside the live legs.** In the crouch holster after the release (IK weight 0, so the pure
  clip arm), the forearm is up to 9.6 cm inside the `calf_l` / `thigh_l` proxies. Those proxies are real-sized:
  - calf capsules r 6.8 / 5.5 cm;
  - thigh r 8.5 cm (`PHYS_MHC_Hero`).

  So the penetration is not caused by the IK. It comes from the upper-body switch clip over different live crouch
  legs, and an IK push cannot fix it cleanly.
- **The predictive path outruns the animation.** The standing draw plan moves the hand 44.9 cm in 0.43 s, a min-jerk
  peak of about 195 cm/s, while the clip's own hand moves at 95 cm/s or less. The hand visibly jumps ahead of the
  motion.

### 5.2 Redesign (recommended): clip first, settle at contact

1. **The clip moves the arm**, with no predictive path and no per-frame leg push.
2. **The left IK weight is the switch clip's own `AZ_Grip_L`**, read from the exact switch montage (section 1), for the
   left hand only. The window falls exactly where the clip's hand touches or leaves the gun:
   - draw: 1.00 -> 1.15 s standing, 1.30 -> 1.57 s crouched (the remaining approach plus the 8 cm / 33 deg settle);
   - holster: 0 -> 0.13 s.

   The holster release is **latched**: once released, the hand stays the clip's until the phase ends. This guards
   against the crouch holster curve returning to 1 at 1.1 s.
3. **Clips without the curve** get the same windows from their geometry:
   - contact = 8 cm from the final hold, ramp 0.12 s;
   - release = 3 cm from the start hold.
4. **Kept from H4:** the exact montage identity, the left-only weight (the master is untouched), and the 0.1 s ownership
   blend at start, end and cancel.
5. **Why the contact and not "0.1 s before the clip end":** the standing clip's hand rests on the handguard from 1.0 s to
   1.53 s. A settle at the end would show the authored hold (8 cm off, fingers not on the gun) for half a second and
   then shift, which is the original "searching".

### 5.3 Legs: fix at the source, after one measurement

A one-shot diagnostic at the deepest arm-in-leg frame of a crouched switch logs the pelvis-relative hand, elbow, knee
and ankle of the live pose against the clip at the same clip time. The prediction is falsifiable:

| Diagnostic result | Fix |
|---|---|
| The live knee differs from the clip's knee by more than 5 cm (the base legs are the cause) | During a crouched switch while standing still, the legs come from the switch clip: an AnimGraph lower-body weight driven from C++, with locomotion legs while moving |
| The knees agree but the hand differs (the layering moves the torso) | Correct the switch slot's layering |

A per-frame IK push stays out either way.

### 5.4 User direction (2026-10-03, after 5.2/5.3): bake the correction into the animation

The user's principle: **the correction lives in the animation, not in per-frame runtime fixes.**
- Each situation (stand / crouch x draw / holster) gets one clip that is already correct for that situation.
- The protected originals stay untouched.

Agreed pipeline (an extension of the Natural Grip workflow: simulate -> solve -> review -> apply):

1. **Record the real situation.** In PIE, a recorder captures the grip node's input pose every frame of a switch, with
   the left IK off: the switch clip's upper body over the live legs, after foot placement and the real layering. It
   also captures the clip time.
2. **Solve offline per clip.** Left arm:
   - **Hand target:** the clip hand, blended onto the solved grip over the authored contact window.
   - **Clearance:** elbow swivel plus a small hand offset keep the hand, forearm and elbow at least 1 cm from the
     thigh, knee, calf and torso. Skin radii come from the body mesh.
   - **Optimisation:** the whole trajectory is optimised at once (minimal correction plus smoothness).
   - **Fingers:** the grip pose, by the same weight.
3. **Review:** before/after images, plus per-frame clearance, correction and speed.
4. **Apply** with a backup:
   - write new clips (originals + suffix) with the corrected left-arm local keys;
   - switch `DA_WeaponAnim_P01` to them, with the user's go;
   - rollback = point the profile back.
5. **Runtime:** plain playback. The left IK keeps only the clip's own `AZ_Grip_L` as a safety net, which is a no-op when
   the bake matches. No path and no per-frame clearance.
6. **Limits:** a situation that was not baked (a crouched switch while walking, slopes) plays the nearest baked clip.
   Add a variant if it is ever visible.

### 5.5 Step 1 built (2026-10-04 00:22): clip-first runtime and the recorder

- **Removed:** the predictive path, the plan cache and the left-arm leg push. The node's "Left Arm Clearance"
  properties and `FAZ_WeaponSwitchReach` path fields are gone.
- **Left IK during a switch:** `master x [(1 - W) + W x k(t)]`.
  - k is the exact switch clip's own `AZ_Grip_L`, or geometric windows when the clip has no curve.
  - A holster release is latched per phase.
  - `az.Weapon.Reach 0` turns the switch grip off; `2` logs each frame.
- **Recorder:** `az.Weapon.RecordSwitch 1` writes the grip node's input pose for every frame of each switch phase:
  - 36 bones in component space (spine, both arms with twist bones, both legs, `az_weapon_r`);
  - clip time, W, ownership, alpha and master;
  - the left-hand-in-weapon-bone target.

  The file goes to `Saved/NaturalGrip/SwitchRecordings/<clip>_<time>.json`.
- **Next:** the user compiles the AnimBP, checks that the jerks are gone, and records crouch and stand draws and
  holsters. Then comes the offline switch-arm solver (5.4).

### 5.6 Recordings and first solve (2026-10-04)

**Recordings.** Ten recordings are in `Saved/NaturalGrip/SwitchRecordings`; repeated recordings agree to within
0.2 cm. User: the jerks are gone, but the hand still passes through the leg in crouch.

**Clearance** (body proxies = `PHYS_MHC_Hero` capsules, true radii; arm capsules x 0.85; hand box as an r = 4 capsule):

| Switch | Arm over the live legs (as played) | Clip arm over its own legs |
|---|---|---|
| Stand draw / holster | clean, 7 cm or more | - |
| Crouch draw | **-12.7 cm**, forearm in `thigh_l`, 0.74-1.27 s | **+1.6 cm or more** (clean) |
| Crouch holster | **-11.1 cm**, forearm in `calf_l` / `thigh_l`, 0.32 s to the end | **+1.2 cm or more** (clean) |

**Cause.** The crouch switch clips are full-body motions:
- in the draw the pelvis rises from 40 to 56 cm and the torso leans;
- in the holster the pelvis drops to 32 cm.

The game keeps the crouch-idle pelvis at 40 cm and the crouch-idle legs: the knees sit 6-25 cm away from the clip's
knees, in the pelvis frame. The upper body arrives 20-28 deg more bent relative to the pelvis, so the chest folds over
the left knee (chest-to-thigh gap about 0) and the arm hangs through the thigh. A pelvis-relative remap of the clip arm
is still -8.8 cm, because the live knees are elsewhere.

**Arm-only bake (`Tools/wgs/switcharm/switch_arm_solve.py`).** No feasible correction exists from 0.80 to 0.96 s within
84 deg of elbow swivel and 14 cm of hand offset. The wrist would have to detour 12-18 cm around the knee while the chest
is folded over it.

**Conclusion.** An arm-only correction against these legs is a large, unnatural detour. The penetration comes from
playing a full-body crouch clip on different legs.

**Recommended fix.** During a crouched switch while standing still, the legs and pelvis come from the switch clip itself:
the authored motion, which is clean (+1.2 cm or more). While moving, they come from locomotion.

Implementation sketch:
- **C++:** an eased `SwitchLowerBodyAlpha` = crouched switch phase owned x not moving.
- **AnimGraph:** blend the RifleFire slot's full pose over the upper-body-layered result by that alpha (cached slot pose
  plus a two-way blend).

**Decision:** approved by the user (2026-10-04).

### 5.7 Crouch switch on the whole body: built (2026-10-04)

**New node** `AnimNode_AZWeaponSwitchFullBody` (runtime) and `AnimGraphNode_AZWeaponSwitchFullBody` (editor), titled
"AZ Weapon Switch Full Body":
- it takes one pose link;
- it blends every bone except the root toward the switch clip's local pose at its montage time, by
  `FAZ_WeaponSwitchReach::FullBodyAlpha`;
- curves are untouched;
- at weight 0 it passes the pose through.

**C++ weight** (`UpdateWeaponSwitchReach`, step 5):
- **On:** the switch phase owns the hand, the switch is crouched, there is no move input and the ground speed is below
  `WeaponSwitchFullBodyMaxSpeed` (10 cm/s).
- **Blend:** eased over `WeaponSwitchFullBodyBlendTime` (0.15 s); the clip and time freeze while it fades out.
- **A/B:** `az.Weapon.SwitchFullBody 0/1`.

**AnimBP (`AZ_ABP_MoverHero_MHC`), inserted by script, not compiled or saved by script:**
- The chain is: RifleFire Slot -> Layered blend per bone (73E21D1A) -> **AZ Weapon Switch Full Body (5C52A710)** ->
  Save cached pose 'AdiativePoses'.
- Downstream of the node: AO, the throwable slot, the FullBody slot, Dead Blending, Offset Root Bone, foot placement
  Control Rig, body clearance, weapon grip, paired hands and pose history.
- **Next:** the user compiles and checks in PIE.

### 5.8 User PIE (2026-10-04): works. Open follow-up for the next session: legs swap after a crouched switch

**Result.** The crouch switch on the whole body was confirmed: "everything seems to work fine".

**Remaining issue (user).** After a crouched holster one crouch clip plays, then another, and the legs change (left
knee forward, then right, or the reverse).

**Diagnosis so far.** Read-only; knee heights are component-space z (cm). The log at the holster commit shows
`[v2 Pick] AZ_RTG_MH_W2_Crouch_Idle_IPC -> AnimPro_CrouchLoop_new blend=0.50`.

| Pose | Left knee | Right knee | Pelvis |
|---|---|---|---|
| Rifle crouch idle `AZ_RTG_MH_W2_Crouch_Idle_IPC` = crouch holster frame 0 | up (38) | down (15) | 35.1 |
| Crouch holster END ("MOB" crouch) = crouch draw start | down (17) | **up** (34) | 32.4 |
| Unarmed crouch `AnimPro_CrouchLoop_new` (MovementAnimsetPro) | up (46) | down (1) | 41.8 |

What this means:
- **The holster clip swaps the knees, as authored.** It ends in the RifleMega "MOB" unarmed crouch.
- **The game's unarmed crouch has the other knee forward.** Our unarmed crouch is the AnimPro loop.
- **What the player sees after the commit.** The clip's right-knee-up end pose is followed by the full-body fade
  (0.15 s) and the chooser blend (0.5 s) from the rifle crouch idle to AnimPro (both left-knee-up), so the legs swap
  back. The crouch draw does the reverse at its start (AnimPro left-up -> clip right-up).

**Leads for the next session:**
1. An unarmed crouch idle that matches the clips' "MOB" end pose:
   - `DMO_MOB1_Crouch_Idle_V2_IPC` exists for Manny (Game Animation Sample demo) and could be retargeted with the
     existing MH pipeline;
   - check that its pose equals the RifleMega MOB crouch first.
2. Otherwise start the base crossfade to the target profile under the holster (as the draw presentation already does),
   so the base is never the rifle idle at the end.
3. Match the full-body fade to the chooser blend.

Not started; the user asked for this to be looked at next session.
