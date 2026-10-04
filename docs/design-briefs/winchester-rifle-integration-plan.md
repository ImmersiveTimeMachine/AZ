# RifleMega rifle integration (Winchester first) - plan

Status: **plan v1, 2026-09-27** - documents only, nothing below is started. Grip quality (hands, fingers, arms vs
weapon) is a separate plan: `weapon-grip-system.md` + `weapon-grip-system-plan.md`. Audit behind this plan: memory
`project_winchester_integration_audit_2026-09-26.md`.

## 1. Goal and rules

The Winchester (the RifleMega pack's default gun) works in the existing item / quick-slot / equipment / animation /
firearm systems the way the M16 does, using ONLY RifleMega pack clips (user rule 2026-09-27: no mixing with M16
animations; Rifle02 at the hip = relaxed, Rifle01 at the shoulder = aim, Rifle_Cr = crouch). Then the other pack guns.

Hard rules: the tuned M16, pistol and unarmed sets are protected content (see the WGS plan, "Protected content");
every phase starts with the pre-flight backup (WGS plan, task -1); M16 + pistol PIE regression after every phase that
touches C++, the hero ABP or the chooser.

## 2. Current state (2026-09-27, verified in PIE)

| part | state | path / value |
|---|---|---|
| Gameplay tag | done | `Weapon.Rifle.Winchester` (DefaultGameplayTags.ini) |
| Chooser | idle only | CHT_v2 c8 `bMatchExact` = true; rows 407 (stand idle -> `AZ_MST_Rifle02_St_Idle00`), 408 (crouch idle -> `AZ_MST_Rifle_Cr_Idle00`) |
| Weapon mesh | done | `/Game/AZ/Assets/Weapons/Winchester_Rifle/Skeletal/SK_Winchester` - bones root (identity), lever, hammer, bolt; sockets LeftHandGrip, Muzzle, Grip_<L|R>_*, StockFront, StockButt |
| Weapon BP | done | `/Game/AZ/Blueprints/Weapon/AZ_BP_Winchester` - WeaponMesh3P = SK_Winchester, Relaxed/Aim socket `RightHandWinchesterSocket`, carry `BackRifleSocket`, GripPose `AS_Grip_Winchester` |
| Animation profile | done (v1) | `/Game/AZ/Blueprints/Animation/MotionMatching/Winchester/DA_WeaponAnim_Winchester` - draw Rifle02 TakeUp @0 / holster HideDown @1.30 (+ Cr), fire Shoot_Winch (Rifle01 / Cr), reload Reload_Winch (Rifle02 relaxed, Rifle01 aim, Cr), speeds 126 / 253 / 0 / 139, aim poses Rifle01 / Cr idle, no AO |
| Pickup | done (v1) | `/Game/AZ/Blueprints/Items/Equippables/Weapons/Winchester/BP_Pickup_Winchester` - tag, class, profile, `bUsesDetachableMagazines=False`, 7 rounds, M16 abilities, PLACEHOLDER icon (AK12). Level instance placed by the user at (-3717.99, 4526.53, 22.48) - never move it |
| Works in PIE | yes | pickup -> quick slot -> equip -> Rifle02 hip idle, grip node active |
| Does not work yet | - | aim, fire, reload (firearm code accepts only detachable magazines), walking / running (no rows), sprint, jumps, draw from the back, AO |
| **Update 2026-09-27 16:50** | data done, PIE pending | R1 rows + PSD_WIN + `bNoGroundTransitionClips` (walk / run / crouch / sprint / aim loops / stance changes); the pickup now carries a contained tube magazine (`Winchester.Tube`, 7/7) and `bUsesDetachableMagazines=True`, so aim + fire use the existing magazine path (the first half of the R3 design); reload still missing (R3 reload branch); R2 aim offsets being built |

## 3. Pack facts (measured 2026-09-26)

- All styles hold the barrel level forward (pitch -2.7 deg): 01 at the shoulder (13 cm below the head), 02 at the hip
  (44 cm below), 03 at the shoulder in a lowered posture; crouch at the shoulder.
- Root speeds: walk 126, run 253, sprint 436 (01 and 03 only), crouch walk 139, crouch run 288 cm/s.
- Present: 8-dir walk/run loops (root-motion + `_IPC`), circle strafes, turns 90L/90R/180 + Linear_90L/R, St<->Cr and
  style<->style transitions, 17-pose aim sets per style + Cr, Shoot_/Reload_ Winch / ShotGun / DoubleBarrel /
  Automatic per style + Cr, TakeUp/TakeDown/HideUp/HideDown (the gun stays in the hand), jumps Idle/Walk/Run, dodges,
  pickups, stun, hits/deaths, grenade.
- Missing: locomotion starts / stops / pivots, falls, a draw from the back.
- In reloads and shots the gun moves relative to the hand (`az_weapon_r` track, up to 33 cm / 85 deg in Reload_Winch);
  lever / pump parts are not animated (one rigid gun in the pack).

## 4. Decisions for the user (recommendation first)

| id | question | recommendation |
|---|---|---|
| D1 | No starts / stops in the pack | Motion matching on the loops only; transition states resolve to the loop rows (or idle) with a blend. Authored starts later only if the blend looks bad. |
| D2 | No draw from the back | v1 (now): TakeUp with instant attach. v2: author a back draw in UE (Sequencer + FK Control Rig on top of TakeUp; timing reference only from the M16 draw, no runtime mixing). |
| D3 | Sprint exists only in 01 / 03 | `Rifle01_St_Sprint_IPC` for both relaxed and aim sprint. |
| D4 | Jumps / falls | Measure `Rifle01_St_Jump_*` (one clip = takeoff + air + land?) and map to our 3-phase jump; falls: hold the jump's air frames (no unarmed mixing). Decide after the measurement. |
| D5 | Ammo model | Tube magazine (7 rounds) + loose cartridges item; reload one shell per loop, interruptible by firing (TLOU style). |
| D6 | Aim offset | Mesh-space additive AO from the 17 poses (Rifle01 stand, Cr crouch), same pattern as `AO_Rifle_Aim`. |
| D7 | Walk / run speeds | Pack-native speeds (no foot sliding): already in the profile. |

## 5. Tasks

Owners: Opus = design / math / review; Sonnet = mechanical from an exact card; User = PIE and decisions. Every task
starts with the pre-flight backup and ends with its acceptance check + (if it touched C++ / ABP / chooser) the M16 +
pistol regression pass.

### R1 - Locomotion rows (Sonnet after Opus decisions) - see WGS plan 6.0 + 6.1 + Appendix B
**Decisions taken 2026-09-27 (lead):**
- (a) MM pools: the M16 profile has no database fields set; its loop rows search the single chooser clip through the
  clip's BranchIn notify (`PSD_P01_*`). The Winchester copies that pattern: `PSD_WIN_WalkRelaxed / WalkAim /
  RunRelaxed / RunAim / Crouch / Sprint` (duplicates of the P01 DBs, same schema) + a whole-clip BranchIn on each of
  the 41 `_IPC` loops. Without it every direction change would restart the clip at frame 0 (`[v2 MMFallback]`).
- (b) = D1: no transition rows. New profile flag `bNoGroundTransitionClips` (C++, `UAZ_WeaponAnimationProfile`) ->
  `FAZ_LocoSMInputs::bNoGroundTransitionClips` -> `UAZ_LocomotionStateMachine::Tick` remaps TransitionToLocomotion
  -> LocomotionLoop and TransitionToIdle -> IdleLoop (starts, stops, pivots, land). Default false = M16 / pistol /
  unarmed bit-identical. Without it the empty transition phase holds 1 s (idle sliding under a moving capsule).
- (c) aim turn-in-place: rows 304-307 (M16 TIP loops, the 2026-09-26 leak) are excluded for the Winchester; its own
  rows 464-467 use `Riflel01_St_Turn_Linear_90L/R` and `Riflel_Cr_Turn_Linear_90L/R` (measured: root yaw linear
  90 deg / 1.0 s, body constant relative to root, loop seam 0.1-0.2 cm -> loop + force_root_lock), profile
  `AimTurnInPlaceClipRateDegPerSec` = 90.
- Measured 2026-09-27 (seam trace before PIE): the pack `_IPC` loops had a flat root, loop=False, no root motion ->
  R17 garbage MM costs and a frozen loop. Each `_IPC` equals its root-motion twin relative to root (0.000 cm, same keys),
  so the twin's root track was copied in (`Tools/wgs/winchester_ipc_root_fix.py`) + loop / root motion / root lock on;
  the 3 idles loop=True. Clip speeds (cm/s) F / side / back: walk 130 / 95 / 110, run 265 / 178 / 192, crouch 143 /
  115 / 143, sprint 463 (profile sprint override 463; the side / back speeds are below the gait speed - strafe foot
  slide is a later tuning item, same as the M16).
- (d) jumps / falls: R7. With no takeoff rows the current loop keeps playing in the air; landing goes straight to the
  loop / idle (flag above).
- Leak rows: 25 shared rows (unarmed starts / turns / bump reactions / stops 77-102, M16 TIP 304-307) have empty c8,
  c9, c19 and matched the Winchester. Their c9 (inverted any-match) cell gets `Weapon.Rifle.Winchester` - the second
  documented exception to "existing rows unchanged": provably neutral for the M16 (`Weapon.Rifle` does not contain
  the child tag), the pistol and unarmed.
- Profile: `PlayRateLoopAssets` held 48 M16 loops (copied from the M16 profile) -> cleared, `bUseLoopPlayRate` off.
- Card: `docs/agent-tasks/wgs-6.1-winchester-rows.md`; snapshot `AZ_Backups/2026-09-27_R1`.

- 6.0 first: `UAZ_ChooserUtils::SetCellGameplayTagsOnSub` + `SetGameplayTagColumnMatchExact` (C++, additive).
- Opus decides D1 / D3 / D4 and whether rows with `bUseMM=True` need a PoseSearch database (check the v2 MM pool).
- Sonnet duplicates the M16 source rows, sets c8 = `Weapon.Rifle.Winchester` and the pack asset, `CompileAndSave`.
- **Acceptance:** chooser dump lists every new row with its asset + tag; rows 0-406 are byte-identical to the baseline
  dump `AZ_Backups/2026-09-27_pre-WGS/chooser_baseline/az_audit_cht_v2.json` (a script diff, not by eye); PIE:
  walk / run / crouch / aim with the Winchester play only `AZ_MST_*` clips (`[v2 Pick]` log).

### R2 - Aim + aim offset (Opus spec, Sonnet builds)
- Rows for aim idle / loops come from R1 (Rifle01, Cr).
- New blend spaces `AO_Winchester_Stand` (17 x `AZ_MST_Rifle01_St_Aim_*`) and `AO_Winchester_Crouch`
  (17 x `AZ_MST_Rifle_Cr_Aim_*`): axes yaw -90..90, pitch -90..90; sample grid CC (0,0), 45L/45R/45U/45D (+-45),
  90L/90R/90U/90D (+-90), 90L45U etc. (the corner names map to (yaw, pitch) directly); additive mesh-space relative to
  `_Aim_CC` frame 0 (beware the `ABPT_ANIM_FRAME` trap in `project_rifle_mh_native_migration_2026-09-09.md`).
- Profile: `StandingAimOffset`, `CrouchingAimOffset`; `bRequiresAimToFire` stays.
- **Acceptance:** AO preview in the editor covers the full range without popping; PIE: the barrel follows the reticle
  within 2 deg over the aim cone (Opus measures with a PIE probe).

### R3 - Tube magazine gameplay (Opus design -> Sonnet implements -> Opus reviews; C++)
**Design v1 written 2026-09-27: `winchester-tube-magazine.md`** (the tube = a magazine item that never leaves the gun;
fire / HUD / save / pickup reuse the magazine model unchanged; only reload becomes a per-shell cartridge transfer).
It supersedes the field list below.
Today fire (`AZ_GA_FirearmFire` ResolveSource), reload (`AZ_GA_FirearmReload` ResolveReloadSource) and readiness
(`AZ_Inv_CommonUI_EquipmentReady`) require `bUsesDetachableMagazines`. Add a second, additive branch:
- `FAZ_Inv_CommonUI_WeaponStateFragment`: `bInternalMagazine`, `InternalCapacity`, `CartridgeFamily` (FName);
  rounds stay on the item instance state (`CurrentRounds`), saved by the campaign code like magazines.
- Cartridges item: stackable fragment + `CartridgeFamily` (`Winchester.3030`), pickup BP `BP_Pickup_WinchesterAmmo`.
- Fire: consumes one round from the item state; empty -> dry fire.
- Reload: loop "load one shell" while (rounds < capacity && cartridges > 0 && not interrupted); fire input interrupts
  after the current shell; montage sections from R4.
- Ready / aim: allowed when `bInternalMagazine` (same gates otherwise).
- HUD: rounds in the tube + cartridges in the backpack (existing ammo widgets, new data source).
- The detachable-magazine code paths are NOT modified (M16 / pistol regression).
- **Acceptance:** unit of behaviour in PIE (user): 7 shots, reload of 3 shells interrupted by a shot, save/load keeps
  rounds; M16 + pistol reload / fire unchanged.

### R4 - Reload montage sections (Opus measures, Sonnet builds)
**Measured 2026-09-27** (see `winchester-tube-magazine.md` section 3): Start 0-1.733, Load 1.733-2.033 (0.300 s
push cycle, seam 0.07 cm), End 3.233-4.667; identical for Rifle01 / Rifle02 / Cr. Montages wait for the R3 go.
- Measure `AZ_MST_Rifle0{1,2}_St_Reload_Winch` and `Rifle_Cr_Reload_Winch`: shell-insert cycles from the right hand's
  distance to the loading gate (weapon space) and the `az_weapon_r` track -> times Start / Load (one cycle) / End.
- Montages `AM_Winchester_Reload_<Rifle01|Rifle02|Cr>` with sections Start -> Load (loops to itself) -> End, slot as
  the profile's reload slot; the profile points at the montages.
- **Acceptance:** section times listed in the card; the Load section loops seamlessly (pose delta at the loop seam
  <= 1 cm on the hands).

### R5 - Fire + lever action (Opus measures, Sonnet builds)
**Measured 2026-09-27** (`Rifle01_St_Shoot_Winch` and `Rifle_Cr_Shoot_Winch`, 0.833 s, identical relative to the gun):
shot + recoil 0-0.40 s (gun rises ~2.5 cm), lever cycle 0.40-0.80 (`AZ_Grip_R` 0): the right hand drops 16 cm and
moves 8 cm forward, max at 0.53 s = ~45 deg about the lever pivot (lever bone at (-0.5, -8.9, 10.2)), back on the grip
at 0.73-0.80. Lever angle keys (deg): 0.40:0, 0.47:22, 0.53:45, 0.60:38, 0.67:22, 0.73:7, 0.80:0 (wrist-based estimate,
refine with the fingertip on the lever loop when building `AS_Winchester_LeverCycle`). Fire cadence: clip 0.833 s,
item `FireRate` 1.0 shot/s (the lever cycle always completes).
- `Shoot_Winch` = shot + lever cycle. Measure the right hand's rotation relative to the weapon over the clip -> lever
  angle curve; hammer and bolt follow the lever (bolt slides back ~6 cm at full throw).
- Weapon mesh animation `AS_Winchester_LeverCycle` on `SK_Winchester_Skeleton` (lever / hammer / bolt tracks from the
  measured curve) -> `AAZ_Weapon::WeaponMeshFireAnimation`; `FireRate` = 1 / clip length.
- **Acceptance:** in PIE the lever opens and closes with the hand (<= 1 frame offset), muzzle flash at `Muzzle`.

### R6 - Draw / holster from the back (D2)
v1 unchanged (instant attach). v2 = an authored draw (UE Sequencer + FK Control Rig) saved as a master clip; carry
socket `BackWinchesterSocket` measured from the authored clip's first frame. Opus + user.

### R7 - Sprint, jumps, falls (D3, D4) - Opus decides after measuring, Sonnet adds rows.

### R8 - Item polish (Sonnet)
Icon: render `SK_Winchester` with a SceneCapture2D (base colour, transparent background, 3:2) -> `T_Icon_Winchester`,
replace the AK12 placeholder in `BP_Pickup_Winchester`; item texts; reticle check; cartridge box pickup (R3).

### R9 - Grip quality - the WGS plan (phases 0-5).

### R10 - Regression guard (Sonnet script + user PIE)
- Script: dump CHT_v2 and diff rows 0-406 against the baseline; dump `DA_WeaponAnim_P01`, `DA_WeaponAnim_Pistol`,
  `AZ_BP_Rifle`, `AZ_BP_Pistol` defaults and hero rifle / pistol sockets against a baseline taken at R-1; any
  difference = stop.
- User PIE checklist: M16 and pistol - idle, walk, run, sprint, crouch, aim, fire (single / auto), reload (stand /
  crouch / aim), switch weapons, holster.

### R11 - Next pack guns
ShotGun, DoubleBarrel, Automatic (pack has their shoot / reload sets) and our imported models (WGS plan 6.2): each =
skeletal conversion with the root fix, BP, profile (reusing the Rifle02 / Rifle01 / Cr locomotion rows through a
shared anim-set tag if the carry matches), grip data, ammo model.

## 6. Order and milestones

| milestone | contents | user check |
|---|---|---|
| M-A | R1 walk / run / crouch loops | walk around with the Winchester |
| M-B | R2 aim + AO | aim around the cone |
| M-C | R3 + R4 + R5 shooting and reloading | 7 shots, interrupted reload, lever |
| M-D | R6 v1 / R7 sprint, jumps | sprint, jump |
| M-E | R8 polish + WGS phases 0-5 | icon, hands look natural in all clips |
| every step | R10 regression | M16 + pistol unchanged |
