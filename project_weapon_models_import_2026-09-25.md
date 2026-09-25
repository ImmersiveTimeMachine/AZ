---
name: project-weapon-models-import-2026-09-25
description: "2026-09-25: 7 weapon models (AK12, STG44, SVD, Remington870, Winchester, Hunter, makeshift revolver) imported as PARTS into /Game/AZ/Assets/Weapons/<W>/{Meshes,Materials,Textures}; whole-mesh build spec for a LOCAL MODEL test at docs/agent-tasks/whole-weapon-meshes.md; UnrealClaude '.py in description' trap."
metadata:
  node_type: memory
  type: project
  originSessionId: 3384aa9b-49dd-43e9-a26a-858cd5de9d54
  modified: 2026-09-25T20:05:40.622Z
---

**Imported (2026-09-25)** from `C:/UnrealEngine/Games/TO_UE` (archives unpacked there, one folder each): the UE 4.21/4.23
projects' .uasset copied at their original /Game paths, then moved with AssetTools.rename_assets into
`/Game/AZ/Assets/Weapons/<AK12_Rifle|STG44_Rifle|SVD_SniperRifle|Remington870_Shotgun|Winchester_Rifle|Hunter_MachineGun|Makeshift_Revolver>/`
(no redirectors left). M16 NOT re-imported — the game M16 is `/Game/AZ/Assets/M16/SKL/M16_Skeleton`, a user-rigged
skeletal mesh (bones per part + sockets LeftHandGrip/LeftHandGripAim/Muzzle). The PAW "5.5 Medium Gun" is a WWII
howitzer scan → `/Game/AZ/Assets/Props/Artillery_5_5_MediumGun` (own material). "Hunter" is a break-action long gun,
not a machine gun.

**Pack traps:** AK12 parts all carry the placeholder material `reference` (real: `Ak_12_base_mat`, stock
`AK12_Stock_base_mat`); STG44 stock should use `M_capitan_stock`; SVD `SM_Stock` has no material (`M_SVD_Stock`);
revolver is ~3.5x oversized (0.28 = cartridge/cylinder real size), muzzle −Y; Hunter points +X; others +Y.
Parts share one origin; Base/Mod/Elit variants overlap → a whole mesh needs ONE chosen configuration.

**Whole-mesh task (user wants to TEST A LOCAL MODEL on it):** spec `docs/agent-tasks/whole-weapon-meshes.md`
(English, embeds the tested `whole_weapons.py`: MODE preflight/build/verify/view/unview, outputs
`/Game/AZ/Assets/Weapons/<W>/SM_<Name>_Whole`, base config without optics/mods). My dry run is kept as a
reference in `/Game/AZ/Temp/WholeTest` (delete when the user says). The user saw it: "всё собралось великолепно".

**Shotgun grip — USER-FINAL (2026-09-25): `RightHandShotgunSocket` on `SKM_MHC_Hero_BodyMesh` (hand_r)
loc (−28.74, 6.71, 4.93) rot (P 0.63, Y 81.59, R 0.91)** — the user tuned it by eye after my fit ("оптимальное
положение"); do not re-derive. (My Nelder-Mead fit had been (−28.72, 5.56, 4.66)/(P −18.9, Y 87.2, R −5.3): tighter
finger contact but 19° more tilt; the user's is level with the index 2.5 cm from the trigger.) `LeftHandGrip`
static-mesh socket on `/Game/AZ/Temp/WholeTest/SM_Remington870_Whole` loc (4.45, 11.44, −5.08) rot (P 1.91,
Y 130.80, R 101.48) = hand_l bone target on the pump, fitted FOR THE USER'S placement; the animated left hand misses
it by only 1.5 cm / 12° (no finger penetration after IK). Copy both onto the final whole mesh when it exists.
**Rule (user): every weapon gets its OWN hand socket** — `RightHand<Weapon>Socket` on the body + `LeftHandGrip` on the gun.
**Winchester (2026-09-25, my fit, awaiting user eye):** `RightHandWinchesterSocket` loc (−31.99, 3.06, −6.35) rot
(P −18.62, Y 85.63, R −4.31); `LeftHandGrip` on `/Game/AZ/Temp/WholeTest/SM_Winchester_Whole` loc (3.34, 3.30, 11.95)
rot (P −36.03, Y 122.39, R 94.85); clip `AZ_RTG_MH_Rifle01_St_Shoot_Winch`. Method = the shotgun fit + a PRIOR that
keeps the barrel within a few degrees of the user's shotgun aim line (the free fit has near-flat optima: the hand can
roll around the wrist). Script scratchpad `grip_opt_winch_prior.py`; left IK need 4.5 cm / 26°. Scripts:
scratchpad `grip_opt.py` (method). **The RifleMega finger pose is ONE fixed pistol-grip pose for every clip** (mocap
without fingers) → straight-stock guns can never put the index exactly on the trigger. **Left-hand IK is NOT wired in
the Mover hero:** `AAZ_Weapon::GetLeftHandSocket` (reads `LeftHandGrip`) is only used by the legacy `UAZ_AnimInstance`.
Level demo actors `SGDEMO_*` in L_001 (temporary). The level viewport does not redraw behind the anim editor → verify
with a SceneCapture2D in `SCS_BASE_COLOR` (unlit, readable) exported via `RenderingLibrary.export_render_target`.

**Traps:** `MergeStaticMeshActorsOptions.base_package_name` gets `SM_` prepended to the asset name; the merge returns
None with spawn_merged_actor=False and the result is unsaved until `save_asset`; pivot is
`MeshMergingSettings.pivot_type = MeshMergePivotType.WORLD_ORIGIN`. **UnrealClaude: an `@Description` containing
".py" makes the `py` console command silently run nothing** (reported "completed 0.00 s", no output).
