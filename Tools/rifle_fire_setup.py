# @Description: Audit or author the M16 single-shot firing grant and measured muzzle socket.
"""Run main() to audit, main(prepare=True) after a full native build to author.

No PIE, Blueprint compilation, animation changes or gameplay tests. Existing
rifles/magazines retain their manifest fields. Native Blueprint compilation and
saved-asset readback are separate finalization steps owned by the caller.
"""
import gc
import importlib.util
import json
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path

import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
SPEC = importlib.util.spec_from_file_location('rifle_aim_helpers', ROOT / 'Tools/rifle_p01_activate.py')
H = importlib.util.module_from_spec(SPEC)
_old_bytecode = sys.dont_write_bytecode
try:
    sys.dont_write_bytecode = True
    SPEC.loader.exec_module(H)
finally:
    sys.dont_write_bytecode = _old_bytecode
ABILITY = H.FIRE_ABILITY
MESH = '/Game/AZ/Assets/M16/SKL/M16_Skeleton'
SOUND = '/Game/MilitaryWeapDark/Sound/Rifle/RifleB_Fire_Cue'
FLASH = '/Game/sA_Megapack_v1/sA_ShootingVfxPack/FX/NiagaraSystems/NS_AR_Muzzleflash_1_ONCE'
# Front bore ring measured from 75 compensator vertices in the read-only FBX.
# Current UE mesh bounds corroborate the exported FBX Y reflection.
MUZZLE = (-1.082305908203125, 60.2704086303711, 11.9348430633545)
OWNER_KEY = 'AZ.RifleFire.Owner'
OWNER = 'rifle_fire_setup:v1'


def patch_manifest(text):
    fields = H.fields(text)
    fragments = H.split_top_level(H.parenthesized(dict(fields)['Fragments']))
    out, counts = [], {H.WEAPON_FRAGMENT: 0, H.GRANT_FRAGMENT: 0}
    aim = H.object_reference('/Script/Engine.BlueprintGeneratedClass', H.ABILITY, True)
    fire = H.object_reference('/Script/Engine.BlueprintGeneratedClass', ABILITY, True)
    reload = H.object_reference('/Script/Engine.BlueprintGeneratedClass', H.RELOAD_ABILITY, True)
    for fragment in fragments:
        kind, values = H.fragment_parts(fragment)
        if kind == H.WEAPON_FRAGMENT:
            counts[kind] += 1
            current = dict(values)
            H.require(current.get('bUsesDetachableMagazines') == 'True'
                      and 'Weapon.Rifle' in current.get('WeaponTag', ''), 'Wrong rifle manifest')
            for name, value in {
                'MuzzleSocketName': '"Muzzle"', 'MaxRange': '10000.000000',
                'bRequiresAimToFire': 'True',
                'FireSound': H.object_reference('/Script/Engine.SoundCue', SOUND),
                'MuzzleFlash': H.object_reference('/Script/Niagara.NiagaraSystem', FLASH),
                'ShotNoiseLoudness': '1.000000', 'ShotNoiseMaxRange': '3000.000000',
            }.items():
                values = H.replace_field(values, name, value)
            fragment = kind + H.encode_fields(values)
        elif kind == H.GRANT_FRAGMENT:
            counts[kind] += 1
            grants = H.split_top_level(H.parenthesized(dict(values).get('AbilitiesToGrant', '()')))
            H.require(grants.count(aim) == 1 and len(grants) == len(set(grants))
                      and all(g in (aim, fire, reload) for g in grants), 'Unexpected ability grants; inspect first')
            if fire not in grants:
                grants.append(fire)
            fragment = kind + H.encode_fields(H.replace_field(values, 'AbilitiesToGrant', '(' + ','.join(grants) + ')'))
        out.append(fragment)
    H.require(all(count == 1 for count in counts.values()), 'Expected one weapon and one ability-grant fragment')
    return H.encode_fields(H.replace_field(fields, 'Fragments', '(' + ','.join(out) + ')'))


def capture_component(component):
    return dict(path=component.get_path_name(),
                manifest=component.get_editor_property('pickup_item_manifest').export_text(),
                contained=[m.export_text() for m in component.get_editor_property('initial_contained_item_manifests')])


def main(prepare=False):
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    H.require(not prepare or not editor.get_game_world(), 'Stop PIE before asset authoring')
    native_class = getattr(unreal, 'AZ_GA_FirearmFire', None)
    H.require(not prepare or native_class is not None, 'Full native build and editor restart required')
    mesh = H.load(MESH, unreal.SkeletalMesh)
    skeleton = mesh.get_editor_property('skeleton')
    bp = H.load(H.RIFLE, unreal.Blueprint)
    components = [H.item_component_template(bp)]
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if actor.get_class() == bp.generated_class():
            components.append(actor.get_component_by_class(unreal.AZ_Inv_CommonUI_ItemComponent))
    H.require(all(components), 'A rifle pickup has no inventory component')
    before = [capture_component(c) for c in components]
    for record in before:
        record['proposed'] = patch_manifest(record['manifest'])
    pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
    parent = unreal.AnimPoseExtensions.get_bone_pose(pose, 'UE4_M16_CompensatorMod', unreal.AnimPoseSpaces.WORLD)
    location = parent.inverse_transform_location(unreal.Vector(*MUZZLE))
    rotation = parent.inverse_transform_rotation(unreal.Rotator(pitch=0, yaw=90, roll=0))
    bounds = mesh.get_bounds()
    H.require(abs(bounds.origin.y + bounds.box_extent.y - MUZZLE[1]) < 0.1,
              'Mesh muzzle bound changed; remeasure geometry')
    socket = mesh.find_socket('Muzzle')
    if socket:
        H.require(str(socket.get_editor_property('bone_name')) == 'UE4_M16_CompensatorMod', 'Existing Muzzle has another parent')
        actual = parent.transform_location(socket.get_editor_property('relative_location'))
        H.require((actual - unreal.Vector(*MUZZLE)).length() < 0.01, 'Existing Muzzle differs from measured frame')
    world = editor.get_editor_world()
    packages = [H.RIFLE, H.package(skeleton), ABILITY]
    map_path = world.get_path_name().split('.')[0] if len(components) > 1 else None
    if map_path:
        packages.append(map_path)
    dirty = [p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
             + unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    H.require(not prepare or not set(packages).intersection(dirty), 'An owned target has unsaved changes; preserve/review them first')
    report = dict(status='audit', native_class_available=native_class is not None, components=before,
                  muzzle_mesh_space=MUZZLE, muzzle_parent='UE4_M16_CompensatorMod',
                  muzzle_local_location=list(location.to_tuple()), muzzle_local_rotation=str(rotation),
                  socket_exists=bool(socket), native_compile_after=[ABILITY, H.RIFLE], map=map_path)
    if prepare:
        sound, flash = H.load(SOUND, unreal.SoundBase), H.load(FLASH, unreal.NiagaraSystem)
        backup = ROOT / 'Saved/Backups/RifleFire' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')
        backup.mkdir(parents=True)
        for package in packages:
            relative = package.removeprefix('/Game/')
            source = ROOT / 'Content' / (relative + ('.umap' if package == map_path else '.uasset'))
            if source.exists():
                dest = backup / source.relative_to(ROOT / 'Content')
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, dest)
        ability = unreal.load_asset(ABILITY) if unreal.EditorAssetLibrary.does_asset_exist(ABILITY) else None
        if ability is None:
            folder, name = ABILITY.rsplit('/', 1)
            factory = unreal.BlueprintFactory()
            factory.set_editor_property('parent_class', native_class)
            ability = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Blueprint, factory)
            H.require(ability, 'Cannot create firing Blueprint')
            unreal.EditorAssetLibrary.set_metadata_tag(ability, OWNER_KEY, OWNER)
        H.require(unreal.EditorAssetLibrary.get_metadata_tag(ability, OWNER_KEY) == OWNER, 'Unowned firing Blueprint exists')
        cdo = unreal.get_default_object(ability.generated_class())
        H.require(isinstance(cdo, native_class), 'Wrong firing Blueprint parent')
        cdo.set_editor_property('input_tag', H.tag('Input.Action.PrimaryAttack'))
        cdo.set_editor_property('activation_required_tags', H.tags('Weapon.Rifle'))
        cdo.set_editor_property('activation_owned_tags', H.tags())
        cdo.set_editor_property('source_object_must_equal_current_weapon_to_activate', False)
        cdo.set_editor_property('activate_ability_on_granted', False)
        if not socket:
            H.require(unreal.AZ_SkeletonUtils.add_socket(skeleton, 'Muzzle', 'UE4_M16_CompensatorMod', location, rotation, False),
                      'Cannot add measured muzzle socket')
        for component, original in zip(components, before):
            manifest = component.get_editor_property('pickup_item_manifest')
            H.require(manifest.import_text(original['proposed']), 'Manifest import failed')
            component.set_editor_property('pickup_item_manifest', manifest)
            actual = capture_component(component)
            H.require(patch_manifest(actual['manifest']) == patch_manifest(original['proposed']), 'Manifest readback differs')
            H.require(actual['contained'] == original['contained'], 'Contained magazine defaults changed')
        for package in [H.package(skeleton), ABILITY, H.RIFLE]:
            H.require(unreal.EditorAssetLibrary.save_asset(package, only_if_is_dirty=False), 'Asset save failed: ' + package)
        if map_path:
            H.require(unreal.EditorLoadingAndSavingUtils.save_map(world, map_path), 'Map save failed')
        report.update(status='authored_requires_native_blueprint_compile', backup=str(backup),
                      after=[capture_component(c) for c in components], sound=sound.get_path_name(), flash=flash.get_path_name())
    output = ROOT / 'Saved/RifleFire'
    output.mkdir(parents=True, exist_ok=True)
    (output / ('prepared.json' if prepare else 'audit.json')).write_text(json.dumps(report, indent=2), encoding='utf-8')
    gc.collect()
    return report
