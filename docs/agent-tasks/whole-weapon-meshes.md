# Task: build one WHOLE static mesh per imported weapon

Executor: a local model. Follow these steps exactly, in order. Do not improvise, do not "improve" anything,
do not skip steps. When something does not match what this document says, STOP and report (section 8).

## 1. Goal

Seven weapons were imported as separate part meshes (receiver, magazine, bolt, stock...). The game needs each
weapon as ONE static mesh so it can be attached to the hero's hand and given sockets. You will create exactly
these seven assets and nothing else:

| Weapon folder (`/Game/AZ/Assets/Weapons/...`) | Asset to create |
|---|---|
| `AK12_Rifle` | `/Game/AZ/Assets/Weapons/AK12_Rifle/SM_AK12_Whole` |
| `STG44_Rifle` | `/Game/AZ/Assets/Weapons/STG44_Rifle/SM_STG44_Whole` |
| `SVD_SniperRifle` | `/Game/AZ/Assets/Weapons/SVD_SniperRifle/SM_SVD_Whole` |
| `Remington870_Shotgun` | `/Game/AZ/Assets/Weapons/Remington870_Shotgun/SM_Remington870_Whole` |
| `Winchester_Rifle` | `/Game/AZ/Assets/Weapons/Winchester_Rifle/SM_Winchester_Whole` |
| `Hunter_MachineGun` | `/Game/AZ/Assets/Weapons/Hunter_MachineGun/SM_Hunter_Whole` |
| `Makeshift_Revolver` | `/Game/AZ/Assets/Weapons/Makeshift_Revolver/SM_MakeshiftRevolver_Whole` |

All decisions (which parts, which materials, rotation, scale) are already made inside the script in section 9.
Your job is to install the script, run it in the order below, check every result line, and report.

## 2. Rules - never break these

1. Do NOT modify, move, rename, re-save or delete anything inside the `Meshes`, `Materials` or `Textures`
   folders of any weapon. The script only reads them.
2. Do NOT touch any other folder of the project (especially `/Game/AZ/Assets/M16` - that is the weapon in the game).
3. Do NOT save the level. The script spawns temporary actors in the open level and removes them; the level shows
   as modified afterwards - that is expected. If the editor asks to save the level, answer "Don't Save".
4. Do NOT edit the script, the part lists, the materials or any number. If a check fails, stop and report.
5. Run ONE command per tool call and wait for its result line before the next call.
6. Never report success without the result lines that prove it. Quote the lines exactly.

## 3. Environment

- Unreal Editor 5.8 is already running with the project `C:/UnrealEngine/Games/AZ` open. Do not close or restart it.
- Python runs INSIDE the editor. In this project the tool is the MCP server `unrealclaude`, tool
  `unreal_execute_script`, parameters: `script_type` = `"python"`, `script_content` = the code.
- The tool answers immediately with `Script execution queued. Task ID: ...`. That is NOT the result.
  Wait 5 seconds, then read the result lines from the report file
  `C:/UnrealEngine/Games/AZ/Saved/whole_weapons_report.txt` (read the whole file, the newest lines are at the end).
  If the file has no new line yet, wait 5 more seconds and read again (up to 6 times for a build, 3 for other steps).
  Alternative: the editor Output Log, filtered by the text `WHOLE`.
- The first line of every script must be a comment `# @Description: ...` (the tool requires it). The description
  text must NOT contain the characters `.py` - the editor then silently runs nothing.

## 4. Step 1 - install the script

Create the file `C:/UnrealEngine/Games/AZ/Tools/whole_weapons.py` with EXACTLY the content of the code block in
section 9 (from `"""Whole-weapon builder` to the final line `run()`). Copy it character for character. If the
file already exists, overwrite it.

Check: the file must contain the line `REPORT = 'C:/UnrealEngine/Games/AZ/Saved/whole_weapons_report.txt'`
and its last line must be `run()`. If not, fix the copy before continuing.

## 5. Step 2 - preflight (read-only)

Run:

```python
# @Description: WHOLE preflight check
exec(open(r'C:/UnrealEngine/Games/AZ/Tools/whole_weapons.py').read(), {'MODE': 'preflight'})
```

Expected last line: `WHOLE PREFLIGHT OK, problems: 0`

Any other result: STOP and report the lines (section 8). Do not build anything.

## 6. Step 3 - build the seven weapons, one call each, in this order

Run these seven commands one by one. After each one, read the report file and check its new line.

```python
# @Description: WHOLE build AK12
exec(open(r'C:/UnrealEngine/Games/AZ/Tools/whole_weapons.py').read(), {'MODE': 'build', 'WEAPON': 'AK12_Rifle'})
```
```python
# @Description: WHOLE build STG44
exec(open(r'C:/UnrealEngine/Games/AZ/Tools/whole_weapons.py').read(), {'MODE': 'build', 'WEAPON': 'STG44_Rifle'})
```
```python
# @Description: WHOLE build SVD
exec(open(r'C:/UnrealEngine/Games/AZ/Tools/whole_weapons.py').read(), {'MODE': 'build', 'WEAPON': 'SVD_SniperRifle'})
```
```python
# @Description: WHOLE build Remington870
exec(open(r'C:/UnrealEngine/Games/AZ/Tools/whole_weapons.py').read(), {'MODE': 'build', 'WEAPON': 'Remington870_Shotgun'})
```
```python
# @Description: WHOLE build Winchester
exec(open(r'C:/UnrealEngine/Games/AZ/Tools/whole_weapons.py').read(), {'MODE': 'build', 'WEAPON': 'Winchester_Rifle'})
```
```python
# @Description: WHOLE build Hunter
exec(open(r'C:/UnrealEngine/Games/AZ/Tools/whole_weapons.py').read(), {'MODE': 'build', 'WEAPON': 'Hunter_MachineGun'})
```
```python
# @Description: WHOLE build Makeshift revolver
exec(open(r'C:/UnrealEngine/Games/AZ/Tools/whole_weapons.py').read(), {'MODE': 'build', 'WEAPON': 'Makeshift_Revolver'})
```

Expected new line after each build (example for the first one):
`WHOLE BUILD OK /Game/AZ/Assets/Weapons/AK12_Rifle/SM_AK12_Whole parts=7 saved_on_disk=True leftover_actors_removed=0`

Expected `parts=` per weapon: AK12 7, STG44 7, SVD 8, Remington870 5, Winchester 8, Hunter 11, Makeshift revolver 12.

- `WHOLE BUILD OK ...` with the right `parts=` -> continue with the next weapon.
- `WHOLE BUILD SKIP ... already exists` -> the asset was built before. Do not delete it. Continue; step 4 will check it.
- `WHOLE BUILD FAIL ...` or any Python error -> STOP, report.

## 7. Step 4 - verify, then show the result

Run:

```python
# @Description: WHOLE verify all seven
exec(open(r'C:/UnrealEngine/Games/AZ/Tools/whole_weapons.py').read(), {'MODE': 'verify'})
```

Expected: seven lines starting with `WHOLE VERIFY PASS`, then `WHOLE VERIFY ALL PASS, failed checks: 0`.
The verify step compares every asset with the known-good numbers (triangles, size, materials, file on disk),
checks that no temporary actor is left and that no part asset was modified. Any `FAIL` -> STOP, report.

Only if verify is ALL PASS, show the result to the user:

```python
# @Description: WHOLE show the seven weapons
exec(open(r'C:/UnrealEngine/Games/AZ/Tools/whole_weapons.py').read(), {'MODE': 'view'})
```

Expected: `WHOLE VIEW placed (bottom to top): AK12_Rifle, STG44_Rifle, SVD_SniperRifle, Remington870_Shotgun,
Winchester_Rifle, Hunter_MachineGun, Makeshift_Revolver`. The seven weapons now hang in a vertical stack in front
of the editor camera. If you have the tool `unreal_capture_viewport`, take one screenshot. Leave them there: the
user looks at them and will ask for removal. Removal command (only when the user asks):

```python
# @Description: WHOLE remove the displayed weapons
exec(open(r'C:/UnrealEngine/Games/AZ/Tools/whole_weapons.py').read(), {'MODE': 'unview'})
```

## 8. Report to the user

Always finish with this report (in Russian, the user speaks Russian), filled with the REAL lines:

```
Итог: <все 7 собраны и проверены | остановился на шаге N>
Строки отчёта:
<all WHOLE ... lines of this session, copied exactly from the report file>
Что сделал: <steps done, one line each>
Проблемы: <none | the exact failing line and what you did NOT do because of it>
```

If you stopped because of a problem: do not try alternative approaches, do not delete anything, do not edit the
script. Report and wait.

## 9. The script (copy exactly into `C:/UnrealEngine/Games/AZ/Tools/whole_weapons.py`)

```python
"""Whole-weapon builder for the AZ project (Unreal Editor 5.8, editor Python).

Merges the chosen part meshes of each imported weapon into ONE static mesh, so the weapon can be attached to the
hero's hand and given sockets. The part assets themselves are never modified.

Run inside the editor with:
    exec(open(r'C:/UnrealEngine/Games/AZ/Tools/whole_weapons.py').read(), {'MODE': '<mode>', 'WEAPON': '<folder>'})

MODE:
    'preflight' - read-only check that every part and material exists and no output exists yet
    'build'     - build ONE weapon (WEAPON = a key of CONFIG) and save it
    'verify'    - read-only check of all seven results against the expected numbers
    'view'      - place the seven results in the level in a vertical stack and aim the viewport camera at them
    'unview'    - remove the actors placed by 'view'
Every run appends its result lines to REPORT and prints them; every line starts with 'WHOLE'.
"""
import os
import unreal

MODE = globals().get('MODE', 'preflight')
WEAPON = globals().get('WEAPON', '')
OUT_DIR = globals().get('OUT_DIR', '')     # testing only - leave unset
ROOT = '/Game/AZ/Assets/Weapons/'
CONTENT_DIR = 'C:/UnrealEngine/Games/AZ/Content/'
REPORT = 'C:/UnrealEngine/Games/AZ/Saved/whole_weapons_report.txt'

# folder: (output name, yaw, uniform scale, [(part mesh, material)], expected triangles, expected bounds min, max,
#          expected material names)
CONFIG = {
    'AK12_Rifle': ('AK12_Whole', 0.0, 1.0, [
        ('Ak_12_UE_AK12_BasePart', 'Ak_12_base_mat'),
        ('Ak_12_UE_AK12_ElitBase', 'Ak_12_base_mat'),
        ('Ak_12_UE_AK12_ElitBase_Shutter', 'Ak_12_base_mat'),
        ('Ak_12_UE_AK12_MagazBase', 'Ak_12_base_mat'),
        ('Ak_12_UE_AK12_Compensator_Base', 'Ak_12_base_mat'),
        ('Ak_12_UE_AK12_Sight_Plank', 'Ak_12_base_mat'),
        ('Ak_12_UE_AK12_Stock', 'AK12_Stock_base_mat')],
        17684, (-4.0, -52.6, -0.3), (3.3, 47.6, 31.9), ['Ak_12_base_mat', 'AK12_Stock_base_mat']),
    'STG44_Rifle': ('STG44_Whole', 0.0, 1.0, [
        ('SM_Captain_BasePart_LOD0', 'M_Capitan_base'),
        ('SM_Captain_Compensator_Base_LOD0', 'M_Capitan_base'),
        ('SM_Captain_ElitBase_LOD0', 'M_Capitan_base'),
        ('SM_Captain_MagazBase_LOD0', 'M_Capitan_base'),
        ('SM_Captain_Shutter_LOD0', 'M_Capitan_base'),
        ('SM_Captain_Sight_Plank_LOD0', 'M_Capitan_base'),
        ('SM_Captain_Stock_LOD0', 'M_capitan_stock')],
        17460, (-3.9, -46.1, -15.8), (5.2, 59.3, 17.7), ['M_Capitan_base', 'M_capitan_stock']),
    'SVD_SniperRifle': ('SVD_Whole', 0.0, 1.0, [
        ('SM_SVD_Base', 'M_SVD_Base'),
        ('SM_SVD_Compensator', 'M_SVD_Base'),
        ('SM_SVD_ElitBase', 'M_SVD_Base'),
        ('SM_SVD_MagazBase', 'M_SVD_Base'),
        ('SM_SVD_Shutter', 'M_SVD_Base'),
        ('SM_SVD_Sight', 'M_SVD_Base'),
        ('SM_SVD_Sight_Plank', 'M_SVD_Base'),
        ('SM_Stock', 'M_SVD_Stock')],
        11584, (-6.2, -67.6, -7.5), (3.3, 80.4, 15.9), ['M_SVD_Base', 'M_SVD_Stock']),
    'Remington870_Shotgun': ('Remington870_Whole', 0.0, 1.0, [
        ('Remington_870_Remigton_BasePart_LOD0', 'Remington_Base_mat'),
        ('Remington_870_Remigton_ElitBase_Shutter_LOD0', 'Remington_Base_mat'),
        ('Remington_870_Remigton_Magaz_Base_LOD0', 'Remington_Base_mat'),
        ('Remington_870_Remigton_SightPlanks_LOD0', 'Remington_Base_mat'),
        ('Remington_870_Remigton_StockBase_LOD0', 'Remington_StockMod_mat')],
        5745, (-3.1, -55.5, -16.6), (2.8, 57.7, 4.7), ['Remington_Base_mat', 'Remington_StockMod_mat']),
    'Winchester_Rifle': ('Winchester_Whole', 0.0, 1.0, [
        ('SM_Winchester_BasePart', 'M_winchester'),
        ('SM_Winchestere_ElitBase', 'M_winchester'),
        ('SM_Winchester_ChargerBase', 'M_winchester'),
        ('SM_Winchester_MazzleBase', 'M_winchester'),
        ('SM_Winchester_Shutter', 'M_winchester'),
        ('SM_Winchester_ShutterDet', 'M_winchester'),
        ('SM_Winchester_SightPlank', 'M_winchester'),
        ('SM_Winchester_StockBase', 'M_winchester')],
        3908, (-2.9, -64.4, -3.9), (2.0, 59.7, 17.8), ['M_winchester']),
    'Hunter_MachineGun': ('Hunter_Whole', 90.0, 1.0, [
        ('SM_BasePart_front_LOD0', 'M_hunter_base'),
        ('SM_Hunter_BasePart_rear_LOD0', 'M_hunter_base'),
        ('SM_Hunter_BasePart_Mec_1_LOD0', 'M_hunter_base'),
        ('SM_Hunter_BasePart_Mec_2_LOD0', 'M_hunter_base'),
        ('SM_Hunter_BasePart_lever_LOD0', 'M_hunter_base'),
        ('SM_Hunter_BasePart_Striker_1_LOD0', 'M_hunter_base'),
        ('SM_Hunter_BasePart_Striker_2_LOD0', 'M_hunter_base'),
        ('SM_Hunter_Elit_Base_LOD0', 'M_hunter_base'),
        ('SM_Hunter_hundler_Elit_Base_LOD0', 'M_hunter_base'),
        ('SM_Hunter_SightPlanks_LOD0', 'M_hunter_base'),
        ('SM_Hunter_Stock_Base_LOD0', 'M_hunter_base')],
        8024, (-4.7, -52.2, -6.6), (3.6, 42.5, 14.3), ['M_hunter_base']),
    'Makeshift_Revolver': ('MakeshiftRevolver_Whole', 180.0, 0.28, [
        ('Makeshift_Revolver_fbx_Body', 'Makeshift_Pistol'),
        ('Makeshift_Revolver_fbx_Cylinder', 'Makeshift_Pistol'),
        ('Makeshift_Revolver_fbx_Ejector', 'Makeshift_Pistol'),
        ('Makeshift_Revolver_fbx_Hammer', 'Makeshift_Pistol'),
        ('Makeshift_Revolver_fbx_Switch', 'Makeshift_Pistol'),
        ('Makeshift_Revolver_fbx_Trigger', 'Makeshift_Pistol'),
        ('Makeshift_Revolver_fbx__357Magnum_Bullet', 'Makeshift_Pistol'),
        ('Makeshift_Revolver_fbx__357Magnum_Bullet_1', 'Makeshift_Pistol'),
        ('Makeshift_Revolver_fbx__357Magnum_Bullet_2', 'Makeshift_Pistol'),
        ('Makeshift_Revolver_fbx__357Magnum_Bullet_3', 'Makeshift_Pistol'),
        ('Makeshift_Revolver_fbx__357Magnum_Bullet_4', 'Makeshift_Pistol'),
        ('Makeshift_Revolver_fbx__357Magnum_Bullet_5', 'Makeshift_Pistol')],
        22326, (-2.1, -11.1, 0.1), (2.1, 13.8, 14.6), ['Makeshift_Pistol']),
}

EAL = unreal.EditorAssetLibrary
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def out_folder(folder):
    return OUT_DIR if OUT_DIR else ROOT + folder


def target_of(folder):
    # the merge tool prepends 'SM_' to the name it is given
    return out_folder(folder) + '/SM_' + CONFIG[folder][0]


def on_disk(asset_path):
    return os.path.exists(CONTENT_DIR + asset_path[len('/Game/'):] + '.uasset')


def remove_labelled(prefix):
    removed = 0
    for actor in EAS.get_all_level_actors():
        if actor.get_actor_label().startswith(prefix):
            actor.destroy_actor()
            removed += 1
    return removed


def preflight():
    lines, problems = [], 0
    for folder, cfg in CONFIG.items():
        for mesh_name, mat_name in cfg[3]:
            for kind, sub, name in (('mesh', 'Meshes', mesh_name), ('material', 'Materials', mat_name)):
                path = ROOT + folder + '/' + sub + '/' + name
                if not EAL.does_asset_exist(path):
                    problems += 1
                    lines.append('WHOLE PREFLIGHT MISSING %s %s' % (kind, path))
        if EAL.does_asset_exist(target_of(folder)):
            problems += 1
            lines.append('WHOLE PREFLIGHT OUTPUT ALREADY EXISTS %s' % target_of(folder))
    lines.append('WHOLE PREFLIGHT %s, problems: %d' % ('OK' if problems == 0 else 'FAILED', problems))
    return lines


def build(folder):
    if folder not in CONFIG:
        return ['WHOLE BUILD FAIL unknown WEAPON %r, allowed: %s' % (folder, ', '.join(CONFIG))]
    name, yaw, scale, parts = CONFIG[folder][:4]
    target = target_of(folder)
    if EAL.does_asset_exist(target):
        return ['WHOLE BUILD SKIP %s already exists - not overwritten' % target]
    actors = []
    try:
        for mesh_name, mat_name in parts:
            mesh = unreal.load_asset(ROOT + folder + '/Meshes/' + mesh_name)
            mat = unreal.load_asset(ROOT + folder + '/Materials/' + mat_name)
            if mesh is None or mat is None:
                return ['WHOLE BUILD FAIL %s: cannot load %s or %s' % (folder, mesh_name, mat_name)]
            actor = EAS.spawn_actor_from_object(mesh, unreal.Vector(0.0, 0.0, 0.0),
                                                unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
            actor.set_actor_label('WHOLE_TMP_' + mesh_name)
            actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
            component = actor.static_mesh_component
            for index in range(component.get_num_materials()):
                component.set_material(index, mat)
            actors.append(actor)
        settings = unreal.MeshMergingSettings()
        settings.set_editor_property('pivot_type', unreal.MeshMergePivotType.WORLD_ORIGIN)
        settings.set_editor_property('merge_materials', False)
        settings.set_editor_property('lod_selection_type', unreal.MeshLODSelectionType.SPECIFIC_LOD)
        settings.set_editor_property('specific_lod', 0)
        options = unreal.MergeStaticMeshActorsOptions()
        options.set_editor_property('base_package_name', out_folder(folder) + '/' + name)
        options.set_editor_property('destroy_source_actors', True)
        options.set_editor_property('spawn_merged_actor', False)
        options.set_editor_property('mesh_merging_settings', settings)
        # returns None when spawn_merged_actor is False - success is judged by the asset below, not by this value
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).merge_static_mesh_actors(actors, options)
    finally:
        leftover = remove_labelled('WHOLE_TMP_')
    if not EAL.does_asset_exist(target):
        return ['WHOLE BUILD FAIL %s: merge produced no asset at %s (leftover actors removed: %d)' % (folder, target, leftover)]
    EAL.save_asset(target, only_if_is_dirty=False)
    return ['WHOLE BUILD %s %s parts=%d saved_on_disk=%s leftover_actors_removed=%d'
            % ('OK' if on_disk(target) else 'FAIL', target, len(parts), on_disk(target), leftover)]


def verify():
    lines, failed = [], 0
    for folder, cfg in CONFIG.items():
        tris, bmin, bmax, mats = cfg[4], cfg[5], cfg[6], cfg[7]
        target = target_of(folder)
        mesh = unreal.load_asset(target) if EAL.does_asset_exist(target) else None
        if mesh is None:
            failed += 1
            lines.append('WHOLE VERIFY FAIL %s: asset missing' % target)
            continue
        box = mesh.get_bounding_box()
        got = (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)
        slots = [s.material_interface.get_name() if s.material_interface else None for s in mesh.static_materials]
        checks = [
            ('static_mesh', isinstance(mesh, unreal.StaticMesh)),
            ('one_lod', mesh.get_num_lods() == 1),
            ('triangles=%d' % tris, mesh.get_num_triangles(0) == tris),
            ('bounds', all(abs(g - e) <= 0.5 for g, e in zip(got, tuple(bmin) + tuple(bmax)))),
            ('materials', None not in slots and sorted(slots) == sorted(mats)),
            ('on_disk', on_disk(target)),
        ]
        bad = [n for n, ok in checks if not ok]
        failed += bool(bad)
        lines.append('WHOLE VERIFY %s %s | tris %d | min (%.1f %.1f %.1f) max (%.1f %.1f %.1f) | materials %s%s'
                     % ('PASS' if not bad else 'FAIL', target, mesh.get_num_triangles(0), got[0], got[1], got[2],
                        got[3], got[4], got[5], slots, (' | failed: ' + ', '.join(bad)) if bad else ''))
    tmp = [a.get_actor_label() for a in EAS.get_all_level_actors() if a.get_actor_label().startswith('WHOLE_TMP_')]
    dirty = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    touched = [d for d in dirty if d.startswith(ROOT) and ('/Meshes/' in d or '/Materials/' in d or '/Textures/' in d)]
    if tmp:
        failed += 1
        lines.append('WHOLE VERIFY FAIL temporary actors left in the level: %s' % tmp)
    if touched:
        failed += 1
        lines.append('WHOLE VERIFY FAIL source part assets were modified: %s' % touched)
    lines.append('WHOLE VERIFY %s, failed checks: %d' % ('ALL PASS' if failed == 0 else 'FAILED', failed))
    return lines


def view():
    remove_labelled('WHOLE_VIEW_')
    base = unreal.Vector(-2103.0, 497.0, 40.0)
    placed = []
    for index, folder in enumerate(CONFIG):
        mesh = unreal.load_asset(target_of(folder)) if EAL.does_asset_exist(target_of(folder)) else None
        if mesh is None:
            placed.append('%s missing' % folder)
            continue
        actor = EAS.spawn_actor_from_object(mesh, unreal.Vector(base.x, base.y, base.z + index * 30.0),
                                            unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
        actor.set_actor_label('WHOLE_VIEW_%d_%s' % (index, folder))
        placed.append(folder)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
        unreal.Vector(base.x + 330.0, base.y, 150.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=180.0))
    return ['WHOLE VIEW placed (bottom to top): %s' % ', '.join(placed)]


def run():
    if MODE == 'preflight':
        lines = preflight()
    elif MODE == 'build':
        lines = build(WEAPON)
    elif MODE == 'verify':
        lines = verify()
    elif MODE == 'view':
        lines = view()
    elif MODE == 'unview':
        lines = ['WHOLE UNVIEW removed %d actors' % remove_labelled('WHOLE_VIEW_')]
    else:
        lines = ['WHOLE ERROR unknown MODE %r' % MODE]
    with open(REPORT, 'a') as handle:
        handle.write('\n'.join(lines) + '\n')
    for line in lines:
        print(line)


run()
```
