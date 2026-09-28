---
name: project-winchester-integration-audit-2026-09-26
description: "★★★ Winchester (first RifleMega gun): audit + state. 2026-09-27: R1 locomotion rows/PSDs/IPC root fix, R2 aim offsets, tube magazine as contained magazine (aim+fire) DONE, user PIE pending; R3 reload branch designed (winchester-tube-magazine.md), awaiting go."
metadata:
  node_type: memory
  type: project
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-09-27T20:35:06.949Z
---

User order 2026-09-26: "Да, начинай с винчестера, делай аудит" (rifle integration, Winchester first). Audit done, plan
presented, **waiting for go + the tube-magazine decision**. Builds on [[project-master-skeleton-2026-09-25]] (grip system).

## Hard blockers (verified in code)
1. **Weapon actor must have a SKELETAL WeaponMesh3P.** `UAZ_Inv_CommonUI_EquipmentComponent::PrepareWeaponActor`
   destroys the spawned weapon if `GetWeaponMesh3P()->GetSkeletalMeshAsset()` is null, and it HIDES `MeshComponent`
   (static). AZ_BP_Winchester (static SM_Winchester_Whole, WeaponMesh3P cleared) can never equip -> the failed
   pickup test. Fix = skeletal Winchester (root bone + Muzzle + LeftHandGrip sockets).
2. **Firearm code is magazine-only.** Fire (`AZ_GA_FirearmFire` ResolveSource), reload (`AZ_GA_FirearmReload`
   ResolveReloadSource) and ready/aim (`AZ_Inv_CommonUI_EquipmentReady`) all require
   `WeaponStateFragment.bUsesDetachableMagazines`. A tube-fed Winchester needs a new internal-magazine mode
   (recommended: shell-by-shell reload from loose ammo, TLOU style) + HUD shell display.
3. **Chooser routing is by the item's WeaponTag.** `WeaponStateFragment.WeaponTag` -> `Selection.Profile` ->
   `ChooserContext.OwnedTags`. CHT_v2 tag columns (all Any, RowValueInInput, non-exact): c8 = Weapon.Rifle (206 M16
   rows), c9 inverted = NOT Weapon.Rifle (82 unarmed rows), c19 = Weapon.Pistol (94), c20 inverted = NOT
   Weapon.Pistol (92), c17/c18 = Movement.Sprinting. Non-exact => a child tag Weapon.Rifle.X matches ALL M16 rows
   and is excluded from unarmed rows. No tag-cell setter in UAZ_ChooserUtils (DuplicateRowOnSub copies tags only).
   Tag cells are dumped by a cpp harness (DumpChooserFullTree prints "(GameplayTagColumn)").

## Item flow facts (Explore agent, key lines verified)
Manifest copied verbatim from the pickup instance at pickup (no registry). `WeaponActorClass`, `AnimationProfile`,
`WeaponTag`, magazine flags, muzzle/sounds/reticle on WeaponStateFragment; abilities on EquipmentFragment. M16 has no
item asset - inline manifest on BPAZ_CommonUI_PickupItem level instances; pistol has a class BP_Pickup_Pistol
(follow that). Reserve-ammo attribute by exact tag (Weapon.Rifle/Pistol/Shotgun).

## Pack measurements (master clips, az_weapon_r barrel = -X)
All styles hold the barrel LEVEL forward (pitch -2.7, yaw -3.2): 01 = shoulder (gun 13 cm below head), 02 = hip
(44 cm below), 03 = shoulder in a lowered posture; crouch = shoulder. **No lowered/relaxed carriage, no
starts/stops/pivots, no falls.** Root speeds: walk 126, run 253, sprint 436 (01/03 only), crouch walk 139 / run 288
(M16 profile: 120/345/-/90).

## Winchester carried in M16 clips (az_weapon_r is a child of hand_r, no track -> rides the hand)
Barrel in M16 aim clips: pitch +24.6, yaw +16.5 (pistol-grip wrist) -> unusable for aim without its own socket.
In M16 relaxed clips: pitch -19, yaw +41..62 = a natural low-ready diagonal, needs a Winchester relaxed socket aligned
to the M16 body + its own right-hand grip solve.

## Recommended plan (presented, not approved yet)
Relaxed = M16 relaxed rows (all transitions/jumps/sprint), Winchester relaxed socket fitted to the M16 hand + second
grip pose. Aim = RifleMega Rifle01 stand + Cr (exact hand, barrel on the sight line), new chooser rows only for aim,
child tag Weapon.Rifle.Winchester + exclusion of it on the M16 aim rows. Fire/reload/draw from the pack. Profile
DA_WeaponAnim_Winchester. Delegation: local model = profile DA fields + verification dumps; Sonnet = pickup BP/icon,
tag-cell C++ setter, internal-magazine spec work; me = skeletal mesh, sockets/grip solves, chooser design, review.

**2026-09-27 PIE: WORKS.** profile=Weapon.Rifle.Winchester, idle AZ_MST_Rifle02_St_Idle00, grip node holds the Winchester. User placed the level pickup AZ_Winchester_Pickup at (-3717.99, 4526.53, 22.48) - NEVER move it back / respawn elsewhere. Old AZ_Winchester_Pickup_TEST deleted (it carried Weapon.Rifle + M16 profile = the "M16 pose" report). Leak: aiming picked M16 row AZ_RTG_MH_W2_Stand_Aim_Turn_In_Place_L_Loop_IPC - exclude when wiring aim. LC harness static init runs OFF the game thread: wrap work in AsyncTask(GameThread). Next: Rifle02 walk/run 8-dir + Cr, then Rifle01 aim + TurnSet (no starts/stops).

**Blender->UE rig trap (2026-09-27):** the Blender FBX export (meters scene, apply_unit_scale, bones tails +Z) gave SK_Winchester a root bone with SCALE 100 and roll 90 -> root-attached sockets land 100x away (LeftHandGrip ~6 m off). Fixed in UE with SkeletonModifier: set root identity, children identity at same cm positions, commit (mesh does not move; bounds equal). For the next weapon rig: fix the export (cm scene / bones along +Y) or run the same post-fix. User-tuned RightHandWinchesterSocket (-32.160, 5.637, -7.098) P0.014 Y83.656 R-2.119 saved on SKM_AZ_Master + hero body + hero_sockets.json; right fingers still need a re-solve for it.

**R1 + R2 + half of R3 DONE 2026-09-27 16:00-17:10 (user: "делать так, как мы разработали план"; away 1 h, PIE
pending).** Snapshot AZ_Backups/2026-09-27_R1. C++ (full build): ChooserUtils Set/GetCellGameplayTagsOnSub,
SetGameplayTagColumnMatchExact, DescribeGameplayTagColumn; profile bNoGroundTransitionClips -> SM Tick remaps
TransitionToLocomotion/Idle (D1). Data: CHT_v2 rows 409-463 (55, pack clips) + c9=Winchester on 25 shared leak rows
(77-102, 304-307; neutral for M16/pistol/unarmed); PSD_WIN_{Walk,Run}{Relaxed,Aim}/Crouch/Sprint (BranchIn-synced,
8/8/8/8/8/1); 41 AZ_MST _IPC loops got the root track of their RM twin (pack IPC = twin with root zeroed; R17 garbage
costs otherwise) + loop/rm/force_root_lock; 3 idles loop=True; profile: PlayRateLoopAssets (48 M16) cleared, sprint
463; AO_Winchester_Stand/Crouch (17 Rifle01 / Cr poses, additive mesh-space vs CC); pickup: contained tube magazine
(Winchester.Tube 7/7) + bUsesDetachableMagazines=True -> aim + fire on the existing path, reload = R3 branch
(design winchester-tube-magazine.md, awaiting go). Cards: docs/agent-tasks/wgs-6.1-*.md, winchester-r2-*.md.

**PLANS WRITTEN 2026-09-27 (user: "сегодня ничего не реализуем, только документы"):** docs/design-briefs/weapon-grip-system.md (design), weapon-grip-system-plan.md (task cards 0.1-7.1, protected content, pre-flight robocopy backup = task -1), winchester-rifle-integration-plan.md (D1-D7 decisions, R1-R11, milestones). All committed+pushed (spike/cmc-backport up to c58e78b). Chooser regression baseline (407 rows, pre-Winchester) in AZ_Backups/2026-09-27_pre-WGS/chooser_baseline/. Start the next session from those plans; open user questions: WGS section 15 + rifle D1-D7.
