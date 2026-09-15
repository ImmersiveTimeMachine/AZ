# @Description: Audit, author and read back the opted-in SmartActors fence surface.
"""Run inside Unreal Editor after the native surface classes are loaded.

main('audit') records existing placement/mesh properties. main('author') adds only
the profile and mesh-owned runtime user data. main('verify') only reads them.
No actor replacement, map edits, animation edits, PIE or gameplay execution.
"""

import gc
import hashlib
import json
from pathlib import Path
import shutil
from datetime import datetime, timezone

import unreal


PROJECT = Path(unreal.Paths.project_dir()).resolve()
FOLDER = '/Game/AZ/Blueprints/SmartActors'
MESH = FOLDER + '/SM_fence_3'
PROFILE = FOLDER + '/DA_TraversalSurface_Fence3'
RECEIPTS = PROJECT / 'Saved' / 'SmartActors'
OWNER_KEY = 'AZ.SmartActors.Owner'
OWNER_VALUE = 'smart_actor_surfaces_setup:v1'
TAG_NAMES = (
    'Surface.Type.Fence',
    'Surface.Traversal.NoStandingTop',
    'Surface.Traversal.RequiresClimbOver',
    'Surface.Hazard.PointedTop',
)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def path_of(obj):
    return obj.get_path_name() if obj else None


def vector(value):
    return [round(value.x, 5), round(value.y, 5), round(value.z, 5)]


def transform(value):
    rotation = value.rotation
    return {'location': vector(value.translation), 'scale': vector(value.scale3d),
            'rotation': [round(rotation.x, 7), round(rotation.y, 7),
                         round(rotation.z, 7), round(rotation.w, 7)]}


def write_receipt(name, data):
    RECEIPTS.mkdir(parents=True, exist_ok=True)
    destination = RECEIPTS / (name + '.json')
    destination.write_text(json.dumps(data, indent=2), encoding='utf-8')
    print('SMART_ACTORS_RECEIPT', str(destination))


def mesh_file():
    return PROJECT / 'Content' / 'AZ' / 'Blueprints' / 'SmartActors' / 'SM_fence_3.uasset'


def capture():
    mesh = unreal.load_asset(MESH)
    require(isinstance(mesh, unreal.StaticMesh), 'Expected the user-provided StaticMesh: ' + MESH)
    bounds = mesh.get_bounds()
    body = mesh.get_editor_property('body_setup')
    materials = [{'slot': str(slot.material_slot_name), 'material': path_of(slot.material_interface)}
                 for slot in mesh.get_editor_property('static_materials')]
    report = {'mesh': path_of(mesh), 'bounds_center': vector(bounds.origin),
              'bounds_extent': vector(bounds.box_extent), 'materials': materials,
              'body_setup': path_of(body),
              'collision_trace_flag': str(body.get_editor_property('collision_trace_flag')) if body else None,
              'placements': []}
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            if component.get_editor_property('static_mesh') != mesh:
                continue
            report['placements'].append({
                'actor': actor.get_path_name(), 'label': actor.get_actor_label(),
                'component': component.get_path_name(),
                'actor_transform': transform(actor.get_actor_transform()),
                'component_transform': transform(component.get_world_transform()),
                'collision_enabled': str(component.get_collision_enabled()),
                'collision_profile': str(component.get_collision_profile_name()),
                'collision_object_type': str(component.get_collision_object_type()),
                'mobility': str(component.get_editor_property('mobility')),
            })
    report['placements'].sort(key=lambda item: item['component'])
    return report


def backup_target():
    stamp = datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S_%f')
    destination = RECEIPTS / ('AssetBackup_' + stamp)
    destination.mkdir(parents=True, exist_ok=False)
    files = [mesh_file(), mesh_file().with_name('DA_TraversalSurface_Fence3.uasset')]
    hashes = {}
    for source in files:
        if source.exists():
            hashes[str(source)] = hashlib.sha256(source.read_bytes()).hexdigest()
            shutil.copy2(source, destination / source.name)
    (destination / 'hashes.json').write_text(json.dumps(hashes, indent=2), encoding='utf-8')
    print('SMART_ACTORS_BACKUP', str(destination))


def reflected_classes():
    profile_class = getattr(unreal, 'AZ_TraversalSurfaceProfile', None)
    data_class = getattr(unreal, 'AZ_TraversalSurfaceData', None)
    require(profile_class and data_class, 'New native surface types are not loaded; normal build/restart required.')
    return profile_class, data_class


def surface_tags():
    # GameplayTag/Container fields are read-only to Python. ImportText uses their
    # native serializer and the registered tag dictionary rather than mutating internals.
    tags = unreal.GameplayTagContainer()
    text = '(GameplayTags=(' + ','.join('(TagName="' + tag + '")' for tag in TAG_NAMES) + '))'
    tags.import_text(text)
    exported = tags.export_text()
    require(all(tag in exported for tag in TAG_NAMES), 'Surface tag dictionary/ImportText did not retain all tags.')
    return tags


def author():
    profile_class, data_class = reflected_classes()
    before = capture()
    write_receipt('before-author', before)
    backup_target()
    profile = unreal.load_asset(PROFILE)
    if profile:
        require(isinstance(profile, profile_class), 'Profile path is occupied by a different class.')
        require(unreal.EditorAssetLibrary.get_metadata_tag(profile, OWNER_KEY) == OWNER_VALUE,
                'Existing profile is not owned by this authoring script: ' + PROFILE)
    else:
        profile = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'DA_TraversalSurface_Fence3', FOLDER, profile_class, None)
        require(profile, 'Could not create the native traversal profile.')
    profile.modify()
    values = {
        'allow_mantle': False,
        'allow_hurdle': True,
        'allow_climb': False,
        'supports_standing': False,
        'supports_crossing': True,
        'geometry_source': unreal.AZ_TraversalSurfaceGeometrySource.CUSTOM_BOUNDS,
        'bounds_center': unreal.Vector(*before['bounds_center']),
        'bounds_extent': unreal.Vector(*before['bounds_extent']),
        'edge_end_margin': 5.0,
        'surface_tags': surface_tags(),
    }
    for name, value in values.items():
        profile.set_editor_property(name, value)
    unreal.EditorAssetLibrary.set_metadata_tag(profile, OWNER_KEY, OWNER_VALUE)
    require(unreal.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False), 'Profile save failed.')

    mesh = unreal.load_asset(MESH)
    mesh.modify()
    data = mesh.get_asset_user_data_of_class(data_class)
    if not data:
        require(mesh.add_asset_user_data_of_class(data_class), 'Could not attach runtime surface data.')
        data = mesh.get_asset_user_data_of_class(data_class)
    require(data, 'Runtime surface data was not installed.')
    data.modify()
    data.set_editor_property('profile', profile)
    data.set_editor_property('enabled', True)
    require(unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False), 'Mesh save failed.')
    after = capture()
    require(after == before, 'Mesh geometry/material/collision or placed actor state changed unexpectedly.')
    write_receipt('after-author', after)
    return verify()


def verify():
    profile_class, data_class = reflected_classes()
    mesh = unreal.load_asset(MESH)
    profile = unreal.load_asset(PROFILE)
    require(isinstance(profile, profile_class), 'Missing expected profile.')
    data = mesh.get_asset_user_data_of_class(data_class)
    require(data and data.get_editor_property('profile') == profile, 'Mesh does not resolve to the expected profile.')
    require(data.get_editor_property('enabled'), 'Surface metadata is disabled.')
    flags = {name: bool(profile.get_editor_property(name)) for name in
             ('allow_mantle', 'allow_hurdle', 'allow_climb', 'supports_standing', 'supports_crossing')}
    require(flags == {'allow_mantle': False, 'allow_hurdle': True, 'allow_climb': False,
                      'supports_standing': False, 'supports_crossing': True}, 'Fence safety flags differ.')
    require(str(profile.get_editor_property('geometry_source')).endswith('CUSTOM_BOUNDS: 1>')
            or profile.get_editor_property('geometry_source') == unreal.AZ_TraversalSurfaceGeometrySource.CUSTOM_BOUNDS,
            'Fence must use the calibrated custom bounds.')
    tags = profile.get_editor_property('surface_tags').export_text()
    require(all(tag in tags for tag in TAG_NAMES), 'Fence tags are missing.')
    descriptions = []
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            if component.get_editor_property('static_mesh') == mesh:
                descriptions.append({'component': component.get_path_name(),
                                     'resolved': data_class.describe_surface(component)})
    report = {'profile': path_of(profile), 'surface_data': path_of(data), 'flags': flags,
              'tags': tags, 'scene': capture(), 'resolved_placements': descriptions,
              'status': 'metadata_saved_and_read_back',
              'limitation': 'Full-size pointed fence needs climb-over animation; current platform-climb and <=110cm hurdle are not a fit.'}
    write_receipt('verified', report)
    print('SMART_ACTORS_VERIFIED', json.dumps({'profile': PROFILE, 'placements': len(descriptions), 'flags': flags}))
    return report


def main(operation='audit'):
    try:
        if operation == 'audit':
            result = capture()
            write_receipt('audit', result)
            return result
        if operation == 'author':
            return author()
        if operation == 'verify':
            return verify()
        raise ValueError('Unknown operation: ' + operation)
    finally:
        gc.collect()
