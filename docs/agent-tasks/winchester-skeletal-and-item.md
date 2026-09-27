# Task: skeletal Winchester + weapon BP + animation profile + pickup (exact spec)

You execute this spec step by step. Do exactly this, nothing else. Other agents edit this project in parallel: touch
ONLY the assets/files named here, never save the level, never run PIE, never change git state (read-only git is fine),
never write C++ or run `script_type: cpp` scripts. In `unreal_execute_script` Python headers the `@Description` must be
ASCII and must NOT contain the text ".py" (that silently runs nothing). After every save, verify it landed (reload the
asset or check the .uasset mtime). If a check below fails: STOP that part, do not improvise a fix, report numbers.

Tools:
- Unreal: MCP `mcp__unrealclaude__unreal_execute_script` (script_type `python`). Scripts run async: write results to a
  file under `C:/UnrealEngine/Games/AZ/Saved/` and read it with Bash, or read `unreal_get_output_log`.
- Blender 5.2 is OPEN; talk to it through its MCP addon socket with the client
  `C:/Users/Artur/AppData/Local/Temp/claude_bl/bl.py`:
  `python C:/Users/Artur/AppData/Local/Temp/claude_bl/bl.py code <file_with_blender_python>` (runs the code in Blender,
  prints the JSON reply; set a variable `result` or print). Keep each Blender call short (< 60 s).

Coordinate facts (verified): the Blender scene has collection `WINCH_SRC` with 8 imported parts (object scale 0.01,
vertex coordinates in cm). Unreal position (cm) = (bx, -by, bz) * 100 where (bx, by, bz) is the Blender WORLD position in
meters. Muzzle points to Blender -Y (Unreal +Y). Unreal `SM_Winchester_Whole` = these 8 parts merged, same origin,
3908 triangles.

## A. Blender rig -> FBX
A1. Create collection `WINCH_RIG`. Duplicate the 8 objects of `WINCH_SRC` into it (full copies, `obj.copy()` +
    `obj.data.copy()`), then apply scale on the copies (`bpy.ops.object.transform_apply(location=False, rotation=True,
    scale=True)` with only the copies selected). Hide `WINCH_SRC`. Never modify `WINCH_SRC`.
A2. On each copy create ONE vertex group holding all its vertices with weight 1.0, named by part:
    `SM_Winchester_ChargerBase` -> `lever`; `SM_Winchester_ShutterDet` -> `hammer`; `SM_Winchester_Shutter` -> `bolt`;
    the other 5 parts -> `root`.
A3. Pivots (Blender world, meters), computed on the copies BEFORE joining, written to
    `C:/UnrealEngine/Games/AZ/Art/CHALK_Winchester_Rig/rig_manifest.json` (also in Unreal cm via the mapping above):
    - lever = centroid of `ChargerBase` vertices with y <= min_y + 0.015 (front end of the lever arm = its pin).
    - hammer = centroid of `ShutterDet` vertices with z <= min_z + 0.010 (hammer foot).
    - bolt = centroid of all `Shutter` vertices.
    - muzzle = centroid of `SM_Winchestere_ElitBase` vertices with y <= min_y + 0.003 (barrel tip).
A4. Join the 8 copies into one mesh object named `SK_Winchester` (vertex groups survive the join). Then make it ONE
    material slot: set every polygon's `material_index = 0`, remove all other slots, and name that slot's material
    `M_winchester` (rename the material datablock if needed). Report vertex / triangle count (triangles must be 3908).
A5. Armature object named `Armature` at the origin, scale 1, data name `Armature`. Bones (edit mode, roll 0,
    use_connect False): `root` head (0,0,0) tail (0,0,0.05); `lever`, `hammer`, `bolt` heads at their A3 pivots, tails =
    head + (0,0,0.03), all parented to `root`.
A6. Parent `SK_Winchester` to `Armature` (object parent, keep transform) and add an Armature modifier (object =
    `Armature`, use vertex groups). The mesh must not move (check its bounds before/after = equal).
A7. Export ONLY `Armature` + `SK_Winchester` to `C:/UnrealEngine/Games/AZ/Art/CHALK_Winchester_Rig/SK_Winchester.fbx`:
    ```python
    bpy.ops.export_scene.fbx(filepath=..., use_selection=True, object_types={'ARMATURE','MESH'},
        global_scale=1.0, apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
        axis_forward='Y', axis_up='Z', use_space_transform=True, bake_space_transform=False,
        use_mesh_modifiers=False, mesh_smooth_type='FACE', use_mesh_edges=False, use_tspace=True,
        use_triangles=False, primary_bone_axis='Y', secondary_bone_axis='X', armature_nodetype='NULL',
        use_armature_deform_only=False, add_leaf_bones=False, bake_anim=False, path_mode='STRIP',
        embed_textures=False, use_custom_props=False)
    ```
    (scene units METRIC, scale_length 1.0).
A8. Save a copy of the Blender work: `bpy.ops.wm.save_as_mainfile(filepath=".../Art/CHALK_Winchester_Rig/Winchester_Rig.blend", copy=True)`.

## B. Unreal import of SK_Winchester
B1. Import with the legacy FBX factory into `/Game/AZ/Assets/Weapons/Winchester_Rifle/Skeletal`, name `SK_Winchester`,
    NEW skeleton (leave `ui.skeleton` unset): `task.factory = unreal.FbxFactory()`, `ui = unreal.FbxImportUI()`,
    `automated_import_should_detect_type=False, import_mesh=True, import_animations=False, import_materials=False,
    import_textures=False, create_physics_asset=False, import_as_skeletal=True,
    mesh_type_to_import=FBXIT_SKELETAL_MESH`; `ui.skeletal_mesh_import_data`: `update_skeleton_reference_pose=False,
    use_t0_as_ref_pose=False, import_mesh_lo_ds=False, preserve_smoothing_groups=True, import_translation=(0,0,0),
    import_rotation=(0,0,0), import_uniform_scale=1.0, convert_scene=True, force_front_x_axis=False,
    convert_scene_unit=False, normal_import_method=FBXNIM_IMPORT_NORMALS_AND_TANGENTS`; `task.automated=True,
    replace_existing=False, save=False`.
B2. CHECKS (report all numbers): bone names via `s = unreal.SkeletonModifier(); s.set_skeletal_mesh(skm);
    s.get_all_bone_names()` must be exactly root, lever, hammer, bolt (root first). Bounds of `skm` (imported bounds /
    `get_bounds()`) must equal `SM_Winchester_Whole.get_bounding_box()` within 0.3 cm on every axis - if they are
    mirrored, rotated or 100x off: STOP. LOD0 triangles must be 3908.
B3. Material slot 0 -> `/Game/AZ/Assets/Weapons/Winchester_Rifle/Materials/M_winchester` (get `materials` array, set
    `material_interface` on element 0, set the array back). Verify with a fresh get.
B4. Sockets on `SK_Winchester`, bone `root` (see `Tools/metahuman_fixup.py` for creating a `unreal.SkeletalMeshSocket`
    and adding it to a skeletal mesh):
    - `LeftHandGrip`: copy `relative_location`, `relative_rotation`, `relative_scale` verbatim from the `LeftHandGrip`
      socket of `/Game/AZ/Assets/Weapons/Winchester_Rifle/SM_Winchester_Whole` (`sm.find_socket("LeftHandGrip")`).
    - `Muzzle`: location = the muzzle point from A3 in Unreal cm; rotation pitch 0, yaw 90, roll 0.
B5. Save `SK_Winchester` and its new skeleton; verify.

## C. Weapon BP `/Game/AZ/Blueprints/Weapon/AZ_BP_Winchester`
C1. On the class default object (`unreal.get_default_object(bp.generated_class())`): component property
    `WeaponMesh3P` (USkeletalMeshComponent) -> skeletal mesh asset = `SK_Winchester`; component `MeshComponent`
    (UStaticMeshComponent, from AAZ_Item) -> static mesh = None. Do not touch any other component or property. Report
    `GripPose` (must be `/Game/AZ/Blueprints/Animation/WeaponGrip/AS_Grip_Winchester`), `RelaxedSocketName` /
    `AimSocketName` (must be `RightHandWinchesterSocket`), `CarrySocketName` (`BackRifleSocket`), `LeftHandGripSocket`
    (`LeftHandGrip`).
C2. `unreal.BlueprintEditorLibrary.compile_blueprint(bp)` (a regular BP - safe), save, then reload and re-read the two
    mesh values to prove they persisted.

## D. Animation profile `/Game/AZ/Blueprints/Animation/MotionMatching/Winchester/DA_WeaponAnim_Winchester`
Duplicate `/Game/AZ/Blueprints/Animation/MotionMatching/RifleP01/DA_WeaponAnim_P01` to that path, then set ONLY these
(clip paths: `/Game/AZ/Assets/Master/RifleMega/<folder>/AZ_MST_<name>.AZ_MST_<name>`):
| property | value |
|---|---|
| standing_draw | animation `Rifle_TakeHide/Rifle02_St_TakeUp`, attach_time 0.0 |
| standing_holster | animation `Rifle_TakeHide/Rifle02_St_HideDown`, attach_time 1.30 |
| crouching_draw | animation `Rifle_TakeHide/Rifle_Cr_TakeUp`, attach_time 0.0 |
| crouching_holster | animation `Rifle_TakeHide/Rifle_Cr_HideDown`, attach_time 1.30 |
| single_fire_animation | `Rifle_ShootingSet/Winchester/Rifle01_St_Shoot_Winch` |
| crouching_single_fire_animation | `Rifle_ShootingSet/Winchester/Rifle_Cr_Shoot_Winch` |
| automatic_fire_animation, crouching_automatic_fire_animation | None |
| automatic_fire_animation_shots_per_cycle | 1 |
| standing_reload_animation | `Rifle_ReloadingSet/Winchester/Rifle02_St_Reload_Winch` |
| standing_aim_reload_animation | `Rifle_ReloadingSet/Winchester/Rifle01_St_Reload_Winch` |
| crouching_reload_animation, crouching_aim_reload_animation | `Rifle_ReloadingSet/Winchester/Rifle_Cr_Reload_Winch` |
| walk / run / sprint / crouch _speed_override | 126 / 253 / 0 / 139 |
| standing_aim_pose | `Rifle_Styly01_St/Rifle01_IdleSet/Rifle01_St_Idle00` |
| crouching_aim_pose | `Rifle_Cr/Rifle_Cr_IdleSet/Rifle_Cr_Idle00` |
| standing_aim_offset, crouching_aim_offset, relaxed_upper_body_pose | None |
The switch-animation fields are structs (`FAZ_WeaponSwitchAnimation`): build a fresh struct, set `animation` and
`attach_time`, assign it. Save; then dump every property listed above from a fresh load and include it in the report.

## E. Pickup BP `/Game/AZ/Blueprints/Items/Equippables/Weapons/Winchester/BP_Pickup_Winchester`
E1. Duplicate `/Game/AZ/Blueprints/Items/Equippables/Weapons/Pistol/BP_Pickup_Pistol` to that path.
E2. Find its item component template (class `BPAZ_Inv_CommonUI_ItemComponent_C`; it may be inherited - use
    `unreal.SubobjectDataSubsystem` k2_gather_subobject_data_for_blueprint and get each handle's object). Read
    `pickup_item_manifest`, `export_text()` it to `Saved/az_winch_pickup_before.txt`, then edit THE TEXT (not a
    hand-written struct) and `import_text` it back:
    - text fragments: every `"Pistol"` -> `"Winchester"`; both descriptions -> `"Lever-action rifle."`
    - GridFragment GridSize -> (X=3,Y=2)
    - ImageFragment Icon -> `/Script/Engine.Texture2D'/Game/AZ/Assets/Weapons/AK12_Rifle/UI/Textures/Rifle_PrimaryIcon.Rifle_PrimaryIcon'`, IconDimensions (X=128,Y=64) (placeholder)
    - EquipmentFragment EquipmentType -> `Item.Type.Weapon.Rifle`
    - WeaponStateFragment: bUsesDetachableMagazines=False, MagazineFamily="None", WeaponActorClass ->
      `/Game/AZ/Blueprints/Weapon/AZ_BP_Winchester.AZ_BP_Winchester_C`, AnimationProfile -> the D asset,
      CurrentClipAmmo=7, MaxClipAmmo=7, FireRate=1.0, BaseDamage=40.0, MuzzleSocketName="Muzzle",
      SupportedFireModes=(Single), DefaultFireMode=Single, WeaponTag=(TagName="Weapon.Rifle.Winchester"),
      FireSound -> `/Game/MilitaryWeapDark/Sound/Rifle/RifleB_Fire_Cue.RifleB_Fire_Cue`, MuzzleFlash ->
      `/Game/sA_Megapack_v1/sA_ShootingVfxPack/FX/NiagaraSystems/NS_AR_Muzzleflash_1_ONCE.NS_AR_Muzzleflash_1_ONCE`,
      CascadeMuzzleFlash=None
    - AbilityGrantFragment AbilitiesToGrant -> the three M16 abilities `BP_AZ_GA_FirearmAim`, `BP_AZ_GA_FirearmFire`,
      `BP_AZ_GA_FirearmReload` (`/Game/AZ/Blueprints/AbilitySystem/Hero/Abilities/<name>.<name>_C`)
    - ItemTypeTag -> `Item.Type.Weapon.Rifle`; PickupActorClass -> `BP_Pickup_Winchester_C` (the new BP)
    Also on the component: `initial_contained_item_manifests` -> empty array; `pickup_message` ->
    "Press E to pick up Winchester"; `pickup_caption` -> "Pick up Winchester".
    Export the result to `Saved/az_winch_pickup_after.txt` and verify the WeaponTag / WeaponActorClass /
    AnimationProfile / bUsesDetachableMagazines values in it.
E3. Visual: list the BP's mesh components; the one showing the pistol gets `SM_Winchester_Whole` if it is a
    StaticMeshComponent or `SK_Winchester` if it is a SkeletalMeshComponent. Compile, save, verify.
E4. Spawn ONE instance of `BP_Pickup_Winchester` in the open editor level at (270, -50, 240), label
    `AZ_Winchester_Pickup`. Do NOT save the level.

## Report
Per part A-E: done / stopped, the check numbers, every asset path created or modified, anything that did not match.
