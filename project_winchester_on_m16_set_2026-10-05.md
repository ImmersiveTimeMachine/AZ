---
name: project_winchester_on_m16_set_2026-10-05
description: ★★★ The Winchester now MOVES on the M16's W2 set (LocomotionWeaponTag = Weapon.Rifle); its own clips are only the lever fire + tube reload, delta-baked onto the W2 holds. Replaces all earlier Winchester locomotion (RifleMega loops + borrowed starts + lock). PIE pending.
metadata:
  type: project
---
**User decision 2026-10-05.** "There is no big difference between holding an assault rifle and a shotgun / rifle. Do everything like the automatic rifle, except the weapon-specific shots from the shoulder. Simplify, take what is missing and add it to the automatic rifle base, make it reliable."

**Why:** the Winchester mixed three packs: RifleMega loops, RifleAnimsetPro starts / stops, and baked pivots. A mesh-space upper-body lock toggled at every borrowed clip. Results: legs and torso out of step, a speed dip on reversals, arms dropping and rising. Two patch strikes (baked turn starts, baked run pivots) made it worse, so the approach was re-modelled. Earlier the user had rejected the M16 RELAXED look for the Winchester (09-28 pistol pattern); this new decision supersedes that.

**What was done:**
- **Code**
  - `UAZ_WeaponAnimationProfile::LocomotionWeaponTag` (new UPROPERTY).
  - In `UAZ_MoverAnimInstance` (after the profile resolve), the chooser sees that tag instead of the weapon's own. This tracks `PresentedWeaponTag` through the draw / empty-hands / throwable branches.
  - Same pattern as the throwable borrowing the pistol legs. Generic: future shotguns set Weapon.Rifle too.
- **Profile** `DA_WeaponAnim_Winchester`:
  - LocomotionWeaponTag = Weapon.Rifle.
  - Copied from DA_WeaponAnim_P01: speeds 120/345/0/90, W2 aim poses, AO_Rifle_Aim, TIP rate 67, play-rate list, M16 draw / holster + crouch rest idle.
  - Lock flag off.
  - Fire / reload = the delta bakes below.
- **Chooser:** 138 Winchester rows (c8 = Weapon.Rifle.Winchester: 407-536, 577-584) DISABLED, not deleted.
- **Hand:** socket `RightHandWinchesterW2Socket` (az_weapon_r; == hand_r in every set) on the hero mesh + Tools/hero_sockets.json. AZ_BP_Winchester relaxed + aim -> it (compiled, verified on new_object).
  - Math: Winchester barrel parallel to the M16's (both meshes muzzle +Y, up +Z). The centre of index_02 / index_03 / middle_02 of the W2 hold lands where those joints sit on the Winchester in its tuned RifleMega hold. Result: Winchester origin = M16 origin + (-1.57, 27.64, -3.81) in M16 space.
  - Expected: left hand reaches ~9 cm further than the M16's (grip IK). Butt sits like the M16's.
  - Right fingers (AS_Grip_Winchester) were solved for the straight-wrist hold: re-solve if they clip.
- **Actions** (`Tools/diagnostics/bake_winchester_actions.py`, GraftUpperBody DELTA mode), in `/Game/AZ/Assets/Winchester/Actions/`:
  - AZ_WIN_{Stand,Crouch}_Fire = Shoot_Winch (lever) on the W2 aim idles.
  - Reloads: relaxed stand = Rifle02 (hip) donor on the W2 relaxed idle; aim = Rifle01; crouch = Rifle_Cr.
  - All start / end exactly in the hold (0.00 cm). Donor curves kept: AZ_Grip_R drops during the lever / reload.
- **Backups:** AZ_Backups/2026-10-05_WinchesterOnM16_184319 and _WinchesterActions_184742. Script: Tools/diagnostics/winchester_on_m16_set.py.

**Superseded (still on disk, unused):** AZ_WIN_WalkStart* / AZ_WIN_RunPivot180_* and the AZ_OwnWeaponHold exemption (harmless). The PSD_WIN_* databases and RifleMega locomotion rows are no longer used by the Winchester.

**Step 2, same evening: the HOLD.** User, after PIE: the left hand twisted, the left arm stretched, the right elbow off the body ("compare with the real rifle pose").

*Measured:*
- The M16's W2 arms are made for an M16. With the Winchester, the left arm reaches 87 % (aim) / 100 % (relaxed; low ready 35° down) and the wrist twists 34-59°.
- RifleMega's own Winchester poses: 80 %, elbows tucked, butt in the shoulder pocket.
- Sockets cannot fix it. The line-through-both-hands placement tried offline pointed the barrel 10-36° off.

*Fix (user-approved):*
- New C++ `FAnimNode_AZWeaponHold` (AZ) + `UAnimGraphNode_AZWeaponHold` (AZEditor). Bones from clavicle_l/r down take the profile's hold pose in LOCAL space, so the arms move relative to the chest.
- Hold poses are mixed standing / crouch (AimStanceAlpha) / sprint (Gait == Sprint, not aiming, not crouched). Weight is eased by the profile's HoldPoseBlendSpeed. Data passes through `FAZ_WeaponHoldView` / `UAZ_MoverAnimInstance::GetWeaponHold()`.
- New profile fields: StandingHoldPose, CrouchingHoldPose, SprintHoldPose, HoldPoseBlendSpeed.
- Graph: Dead Blending (551130…) → AZ Weapon Hold (4117845E…) → Offset Root Bone (D93AFEF6…). Added by Tools/diagnostics/winchester_hold_layer.py; the USER compiles and saves the AnimBP.
- Winchester DA:
  - holds Rifle01 idle / Cr idle / Rifle01 Sprint_IPC;
  - aim poses Rifle01 / Cr idle, AO_Winchester_Stand/Crouch;
  - fire Shoot_Winch (lever); reload Reload_Winch (Rifle01 / Cr);
  - draw / holster: its own baked TakeUp clips (attach 0.533 / 0.8 / 0.567 / 0.767);
  - speeds and legs still M16.
- Weapon BP socket is back to RightHandWinchesterSocket (tuned for these arms). RightHandWinchesterW2Socket and the AZ_WIN_* action bakes are now unused.
- Backup: AZ_Backups/2026-10-05_WinchesterHold_193644.

*Principle for the next long guns:* legs / transitions = M16 set (LocomotionWeaponTag); hold + fire + reload = the weapon's own pack.
