---
name: project-weapon-arm-solver-2026-09-28
description: "★★★ Weapon hold system state 2026-09-28: Body Clearance node = weapon-out-of-body push + right-arm solver (exact SDF, tapered arm, elbow-out + wrist-turn along ONE ray, first clear point); Grip node = left hand + fingers only. Winchester relaxed on M16 clips, aim on pack. User: 'хорошо работает'. Next order set by the user."
metadata:
  node_type: memory
  type: project
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-09-28T20:37:33.240Z
---

**User verdict 2026-09-28:** "Хорошо работает" (after the ray solver). Plan order (user): (1) user reviews ALL Winchester
animations and says what to change; (2) shotgun + the rest of the pack guns; (3) if all good, migrate M16 + pistol
(protected — only on that explicit go); (4) knife — no good TPP knife set yet (FPS_Controller TPP knife = 10 clips,
FPPMeleeAnimset Dagger = first-person only, SwordAnimsetPro 1h = 356 TPP clips, KB_KnifeThrow).

## Architecture (hero ABP AZ_ABP_MoverHero_MHC): L2C -> AZ Weapon Body Clearance -> AZ Weapon Grip -> C2L
- **Body Clearance** (AnimNode_AZWeaponBodyClearance): (1) stock (StockFront/Butt markers) vs Physics-Asset body
  capsules -> push the right hand (2-bone IK); aiming: butt may press ButtPocketDepth 1 cm into clavicle_r/spine_04/05
  (skipping the pocket entirely hid 6-9 cm penetration). (2) **arm solver**: forearm/upper arm as tapered capsules
  (5.5/4.3/3.0 cm) sampled against the weapon's baked SDF (UAZ_WeaponGripField::SampleUnbounded - the lattice is only
  ~3 cm wider than the gun), last 7 cm at the wrist = grip zone (contact allowed), 0.5 cm sleeve contact. DOFs: elbow
  swing about shoulder->hand axis, side = AWAY from torso (anatomy, never cost); wrist turn of hand+weapon about
  "up perpendicular to forearm", side judged on BODY forward (weapon forward flipped sign mid-strafe). Both grow on ONE
  ray (elbow = R*40, wrist = R*15; wrist share 0 while aiming); first R that clears wins (scan 0.04 + 6 bisections,
  ~35 evals/frame). A 2-D cost minimum TWITCHED at idle (two solutions 0.5 % apart alternated) - never go back to it.
  Pins: Markers<-WeaponGripMarkers, WeaponBoneName<-WeaponGripBone, ClearanceAlpha<-WeaponGripAlpha,
  ShoulderContactAlpha<-AimAlpha (Tools/wgs/insert_body_clearance_node.py).
- **Grip** (AnimNode_AZWeaponGrip): left-hand IK + fingers (clip fingers fitted to the SDF; index->trigger, L thumb).
  Its old elbow logic (AvoidStockWithRightArm, ForearmRadius...) was DELETED - do not re-add arm logic there.
- Offline replica for any change: scratchpad armmodel.py / replica.py / ray.py on Saved/wgs/arm_poses*.json +
  gf_winchester.json (dump poses from the editor first). Numbers: idle ~50/19 deg, walk ~42/16, jog ~22/8, crouch 28/11.

## Winchester data
- Relaxed = the 109 M16 non-aim CHT_v2 rows (c8 += Weapon.Rifle.Winchester; M16 unaffected); pack relaxed rows
  407,408,411-418,427-434,443-450,459-461 DISABLED (SetChooserRowsDisabled, reversible). Aim = pack Rifle01/Cr rows.
- Profile: speeds 120/345/0/90 (M16), bNoGroundTransitionClipsWhileAiming=True (new C++ flag; relaxed uses M16
  starts/stops), fire = AZ_MST_Rifle01_St_Shoot_Hard / Rifle_Cr_Shoot_Hard (no lever cycle; Shoot_Winch was it).
- Backups: AZ_Backups/2026-09-28_win_relaxed (chooser + profile before these edits).
- TRAP: AnimPoseExtensions.get_relative_transform(pose, a, b) is NOT "a in b" - it cost a wrong socket. Use
  get_bone_pose(..., LOCAL/WORLD). In pack clips az_weapon_r == hand_r exactly (local identity).
- Open: pack aim run/crouch play-rate vs speeds (253/139 vs 345/90; profile PlayRate fields are dead code), relaxed
  reload is pack Rifle02 vs M16 locomotion, R3 tube reload (awaiting go), lever bone not animated, commit pending.
- Header comments of ElbowSwingCostDeg/WristTurnCostDeg still say "cost" - they are the ray ratio now (fix at the next
  full build; LC-only change 2026-09-28 16:26).

Related: [[project-winchester-integration-audit-2026-09-26]], [[project-protected-weapon-sets]],
[[feedback-stop-the-patch-loop]], [[feedback-verify-never-presume]].
