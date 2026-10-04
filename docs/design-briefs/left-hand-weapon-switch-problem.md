# Problem: the left hand during a weapon draw / holster

Status: open, 2026-10-03. The tool and the runtime pieces are described in `az-natural-grip-tool-reference.md`. This
document is the problem, what is known about it, what has been tried, and what we want.

Continuation: [implementation handoff](left-hand-weapon-switch-handoff.md) consolidates the subsequent source/live
audit and checkpoints. In particular, it corrects section 9's weapon-to-carry-bone arrival suggestion and qualifies
fix C's shared-master weighting; retain the measurements below as historical evidence.

## 1. Context

The hero is a MetaHuman playing retargeted weapon clips. The left hand holds the weapon through runtime IK:
- **The node.** `AnimNode_AZWeaponGrip` IKs `hand_l` onto the weapon's `LeftHandGrip` socket.
- **The grasp.** The socket holds the hand placement solved offline by AZ Natural Grip: the M16 power grasp, approved
  by the user.
- **The weight.** Weight = the master `WeaponGripAlpha` x the clip curve `AZ_Grip_L` (1 = the clip's hand is on the
  handguard).

**The switch.** The rifle hangs on the back. A switch plays a holster clip, then a draw clip, as dynamic montages in
the upper-body slot `RifleFire`. The legs keep the locomotion (crouch idle when crouched).
- **Attach.** At the profile's `AttachTime` the weapon moves between the back socket and the hand socket.
- **Commit.** The equipment selection (`GetActiveWeapon()`) commits only when the draw clip ends.

M16 switch clips (profile `DA_WeaponAnim_P01`):

| Clip | Length | AttachTime |
|---|---|---|
| `AZ_RTG_MH_W2_Stand_Rlx_Equip_Back_Get_From_MOB` (standing draw) | 1.533 s | 0.567 |
| `AZ_RTG_MH_W2_Crouch_Equip_Back_Get_From_MOB` (crouching draw) | 1.9 s | 0.533 |
| `AZ_RTG_MH_W2_Stand_Rlx_Equip_Back_Return_To_MOB` (standing holster) | 1.433 s | 0.667 |
| `AZ_RTG_MH_W2_Crouch_Equip_Back_Return_To_MOB` (crouching holster) | 1.767 s | 0.567 |

## 2. Symptoms (user reports, 2026-10-03)

1. **Draw: "searching".** "When I take the rifle, for a fraction of a second the left hand is settling onto the
   handguard ... as if it is searching for the place, instead of it being known."
2. **Holster: the arm folds behind the back.** "The left hand stays attached to the left-hand socket too long, so
   through the IK it breaks and goes all the way to the back. It should let go much earlier."
3. **Draw and holster: no clear direction.** "It is unclear where the hand is going. When we take the weapon the hand
   should go, little by little, to the point where the weapon's final pose will be. That is more natural: our brain
   foresees where the rifle will lie and moves the left hand to that point."
4. **Crouch: the hand passes through the leg** (screenshot, after fix D).
   - The hero is crouched, the right hand above the right shoulder with the rifle (the gun on its way between the back
     and the front).
   - The left hand is down at the right lower leg, inside the shin / knee.
   - On-screen debug label "Drawing or hol...".

## 3. Measurements

**Standing draw**, the clip's own left hand vs the solved grip target:

| t (s) | 0.90 | 0.933 | 0.967 | 1.00 | 1.033 | 1.10 | 1.20 | 1.533 |
|---|---|---|---|---|---|---|---|---|
| distance to target (cm) | 34.0 | 23.8 | 15.5 | 10.2 | 8.2 | 8.6 | 8.7 | 8.1 |
| angle to target (deg) | 69 | 59 | 46 | 36 | 32 | 34 | 34 | 33 |
| hand speed in weapon space (cm/s) | 389 | 354 | 275 | 183 | 105 | 23 | 1 | 2 |
| old `AZ_Grip_L` | 0 | 0 | 0 | 0.16 | 0.38 | 0.83 | 1.0 | 1.0 |

- **The hand misses the target.** The clip's hand stops 8-10 cm and 34 degrees away from the solved power grasp; the
  clip was authored with another hold.
- **The curve is late.** The old curve rises only after the hand has stopped.

**Crouching draw.** The clip's hand settles at 1.2-1.47 s while still turning (94 -> 33 degrees); the old curve is full
only at 1.57 s.

**Holster** (standing and crouching):
- the clip's `AZ_Grip_L` is 0.91 at 0, 0.46 at 0.07 s and 0 at 0.14 s: the clip lets go at once;
- the grip target, riding the gun, travels 70-80 cm toward the back and comes within 23-25 cm of the left shoulder at
  0.55-0.67 s.

**Hand path in the chest frame** (`spine_04`):
- standing draw from attach: 32 cm of path for 27 cm straight (x1.2), ending at the clip's point, not the grip;
- crouching draw: 24 cm for 6 cm (x4.1, it wanders);
- holster after release: x2.0 standing, x2.8 crouching.

## 4. Root causes found

1. **The grip was OFF for the whole draw.** The grip gather took the weapon from `GetActiveWeapon()`, which is the OLD
   selection until the draw clip ends. The IK came on only after the clip, easing in at 6/s with the hand already
   still, 8.5 cm / 34 degrees off. That slide is the "searching".
2. **The curve rose after the hand stopped.** `AZ_Grip_L` rose 0.98 -> 1.18 s on a still hand.
3. **On a holster the curve at the node never reached 0.**
   - The node reads `AZ_Grip_L` from the pose. Upper-body layers fade out exponentially when a switch starts
     (`WeaponRelaxedAlpha` FInterpTo 8/s, `TransitionLockAlpha` 12/s).
   - `LayeredBoneBlend` blends curves with the default `Override`, so any relevant weight replaces the clip's 0 with
     the layer's 1 (`FAnimationRuntime::BlendPosesPerBoneFilter` -> `BlendCurves`: `Override(base)`, then
     `Combine(child)` regardless of weight).
   - The hand stayed IK'd on the handguard until the gun reached the back socket (0.667 s), so the arm folded behind
     the back.

## 5. What has been done

| | Fix | Where | State |
|---|---|---|---|
| A | During a draw, grip the weapon physically on its Relaxed/Aim socket (the pawn's attached actors) instead of the committed selection. | `AZ_MoverAnimInstance.cpp`, grip gather | in (Live Coding) |
| B | `AZ_Grip_L` APPROACH rule. The rising edge also follows the distance to the IK target (0 at 30 cm, 1 at 12 cm), capped at the run's peak; holster clips skipped. DRY run: 28 M16 clips would change. | `Tools/wgs/set_left_curve_m16.py` | **not written** (protected clips, needs the user's go). For draws superseded by D; may still matter for reload ends / unjams. |
| C | During a switch, the grip weight follows the switch clip's OWN `AZ_Grip_L`, read from the clip at the montage position, bypassing the overridden pose curve. The holster then lets go at 0.14 s. | grip gather | in (Live Coding). Now the fallback when D has no plan. |
| D | **Switch reach** (section 6) | `AZ_MoverAnimInstance.h/.cpp`, `AnimNode_AZWeaponGrip.h/.cpp` | built (full build), first PIE run gave symptom 4 |

## 6. Fix D: the switch reach as built

**The plan.** It is built once per (switch clip, weapon class) by `UAZ_MoverAnimInstance::BuildWeaponReachPlan`:
- The clip is sampled with `UAnimSequence::GetBoneTransform` along the bone chains.
- Everything is expressed in the chest frame (`spine_04`).
- The grip point = the weapon's `LeftHandGrip` relative to its mesh x the hero's in-hand socket (`RelaxedSocketName`),
  on that socket's bone.

**Draw:**
- **Start** = AttachTime.
- **Arrive** = the first time after the swing that the grip point's speed in the chest frame falls below 20 cm/s
  (after a peak over 50 cm/s).
- **The path** goes from the clip hand at Start (with its velocity, capped) to the grip point at Arrive: a quintic
  Hermite path with the rotation slerped on a minimum-jerk profile.
- **Blend in:** 0.08 s.
- **Meet:** in the last 0.12 s before Arrive the target hands over to the live weapon grip and the fingers close.

**Holster:**
- the hand leaves the live grip in 0.12 s;
- the path goes from the grip point at 0 to the clip hand's end pose;
- **Arrive** = the clip hand's rest time, clamped to 0.45-0.8 s;
- the target is held there until `max(Arrive, ClipRest)`, then handed back to the clip over 0.15 s.

**The node** ignores the pose's `AZ_Grip_L` while the reach owns the hand. Its target = the path (chest frame, live
`spine_04`) blended toward the live weapon grip.

**Controls:**
- `az.Weapon.Reach` 0 = fix C only, 1 = reach, 2 = reach + a per-frame log;
- a `[Reach]` line per built plan.

**Plans built in the first PIE run** (chest frame, cm; from the log):

| Clip | Start | Arrive | ClipRest | P0 | P1 | dist | turn |
|---|---|---|---|---|---|---|---|
| stand draw | 0.567 | **1.483** | 1.483 | (-25.4, 18.7, -31.3) V0 46 cm/s | (-15.1, 23.7, -6.9) | 27.0 | 147 deg |
| crouch draw | 0.533 | **1.767** | 1.767 | (-10.1, 28.5, -19.5) V0 23 cm/s | (-5.1, 32.8, -0.4) | 20.3 | 137 deg |
| stand holster | 0 | 0.800 | 1.100 | (-15.7, 23.1, -6.6) | (-32.8, 10.4, -26.0) | 28.8 | 161 deg |
| crouch holster | 0 | 0.800 | **1.533** | (-6.8, 31.2, -0.6) | (-7.8, 36.8, -16.6) | 16.9 | 149 deg |

## 7. New findings from that run

1. **Arrival detected far too late on the draws.**
   - The gun reaches the front at about 1.0 s standing (the clip hand arrives at 1.0-1.05 s) and at about 1.2-1.25 s
     crouching.
   - The detector waits for the grip point to drop below 20 cm/s in the CHEST frame, and the chest keeps moving
     relative to the gun until 1.48 s (standing) or the clip end (crouching).
   - So the reach takes 0.92 s standing and 1.23 s crouching: slower than the clip and slower than the gun.
2. **The crouch holster holds a fixed point for 0.73 s.**
   - From 0.8 s to 1.533 s the target stays at the end pose (-7.8, 36.8, -16.6) in the chest frame.
   - The clip keeps moving the torso relative to the legs during that time, so a chest-fixed point near the legs can end
     up inside them.
3. **Symptom 4, the hand in the leg, has two candidate causes.** They can act together:
   - **(a) Upper-body montage over other legs.** The switch montage plays in an upper-body slot. The live legs come
     from the crouch locomotion, not from the clip. A hand authored next to the clip's own knee lands where the LIVE
     knee is. This would happen even without the reach (`az.Weapon.Reach 0`).
   - **(b) The reach ignores the legs.** Its targets and straight paths live in the chest frame and know nothing about
     where the legs are, especially when crouched with the knees up in front, and with finding 2.
   - **To tell them apart:** crouch draw and holster in PIE with `az.Weapon.Reach 0` and `1`, a screenshot at the same
     moment, and the `az.Weapon.Reach 2` log. Then compare the clip's knee positions with the live crouch idle's in
     the chest frame.

## 8. What we want (requirements)

- **R1. Draw.**
  - From the moment the right hand takes the gun, the left hand moves with intent toward where the handguard will be
    and meets it as the gun arrives. Arrival is at the gun's arrival (about 1.0 s standing).
  - No slide or "search" after the hand stops; the fingers close as it meets the gun.
- **R2. Holster.**
  - The left hand lets go when the clip's hand does (about 0.1-0.15 s); it never follows the gun behind the back and
    the arm never folds.
  - It then goes to a natural rest pose.
- **R3. No penetration.** At no time during a switch may the left hand or forearm pass through the torso, the legs
  (crouched included: knees, thighs, shins) or the weapon.
- **R4. Natural motion.** One smooth movement per phase (a reach: smooth start and stop), no wandering, no detours, no
  pops; it reads the same standing, crouched and while moving or turning.
- **R5. Data-driven.**
  - It works for every weapon from its switch clips and its `LeftHandGrip`, without per-clip hand-tuning.
  - Protected clips keep their motion untouched; curves change only with the user's go.
- **R6. Robust.**
  - A cancelled switch releases cleanly.
  - The end of a draw hands over to the normal grip with no jump.
  - Weapons without grip data keep the clip's hand.

Acceptance:
- the user's PIE check of standing and crouching draw and holster, standing still and moving, with screenshots at the
  frames of symptoms 1-4;
- the `[Reach]` plans make sense: arrival near the gun's arrival, and no hold longer than the clip's own rest.

## 9. Proposed next steps

1. **Discriminate symptom 4** (7.3) with the A/B run.
2. **Fix the arrival time.** Take Arrive from the gun's motion relative to the hand bone it is carried by, or from the
   clip hand's own arrival on the handguard. This is the 8-10 cm zone measured in section 3: about 1.03 s standing,
   about 1.25 s crouching. Do not use the chest-frame speed.
3. **Holster.** Hand back to the clip as soon as the path arrives, aiming at the clip hand AT Arrive rather than holding
   the end pose.
4. **Body clearance for the left arm (R3), on the LIVE pose.**
   - In the grip node, push the IK goal (and check the forearm) out of capsules on the live thighs, shins, pelvis and
     torso. Radii come from the hero's physics asset.
   - This is the same idea as AZ Weapon Body Clearance for the right arm.
   - It covers both causes (a) and (b) and any clip.
5. **Re-check.** Replay every plan in the editor and check the path's distance to the body before the next PIE.
