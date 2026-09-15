# @Description: Author earlier moving-hurdle handoffs and the explicit fence climb/drop profile.
"""Editor asset authoring only. No PIE, gameplay activation, actor replacement or tests.

Run main('audit'), then main('author') after the normal native build/restart.
main('verify') is read-only. Existing standing hurdle/mantle/climb montages stay unchanged.
"""
import gc
import hashlib
import json
from pathlib import Path
import runpy
import shutil
from datetime import datetime, timezone

import unreal

PROJECT = Path(unreal.Paths.project_dir()).resolve()
FOLDER = '/Game/AZ/Blueprints/SmartActors'
MESH = FOLDER + '/SM_fence_3'
PROFILE = FOLDER + '/DA_TraversalSurface_Fence3_ClimbAndDrop'
ANIM_FOLDER = '/Game/AZ/Blueprints/Animation/MHC/Traversal'
RECEIPTS = PROJECT / 'Saved' / 'ParkourFlow'
HANDOFF = 'Event.Traversal.Handoff'
OWNER_KEY = 'AZ.SmartActors.Owner'
OWNER_VALUE = 'parkour_flow_and_fence_drop_setup:v1'
TAG_NAMES = ('Surface.Type.Fence', 'Surface.Traversal.NoStandingTop',
             'Surface.Hazard.PointedTop', 'Surface.Traversal.ClimbAndDrop')


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def save_receipt(name, report):
    RECEIPTS.mkdir(parents=True, exist_ok=True)
    destination = RECEIPTS / (name + '.json')
    destination.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('PARKOUR_FLOW_RECEIPT', destination)


def moving_hurdles():
    assets = []
    for path in unreal.EditorAssetLibrary.list_assets(ANIM_FOLDER, recursive=False):
        name = path.rsplit('/', 1)[-1].split('.')[0]
        if name.startswith('AM_AZ_Hurdle_1_0_') and ('_Walk_' in name or '_Run_' in name):
            asset = unreal.load_asset(path)
            require(isinstance(asset, unreal.AnimMontage), 'Expected montage: ' + path)
            assets.append(asset)
    assets.sort(key=lambda asset: asset.get_name())
    require(len(assets) == 16, 'Expected sixteen moving hurdle variants; found ' + str(len(assets)))
    return assets


def handoff_plan(montage):
    blend_starts = []
    warp_ends = []
    back_floor_ends = []
    native_events = []
    for event in unreal.AnimationLibrary.get_animation_notify_events(montage):
        start = unreal.AnimationLibrary.get_anim_notify_event_trigger_time(event)
        duration = unreal.AnimationLibrary.get_anim_notify_event_duration(event)
        state = event.get_editor_property('notify_state_class')
        notify = event.get_editor_property('notify')
        if state and state.get_class().get_name() in (
                'BP_NotifyState_TraversalBlendOut_C', 'BP_NotifyState_MontageBlendOut_C'):
            blend_starts.append(start)
        if state and isinstance(state, unreal.AnimNotifyState_MotionWarping):
            modifier = state.get_editor_property('root_motion_modifier')
            target = str(modifier.get_editor_property('warp_target_name'))
            warp_ends.append(start + duration)
            if target == 'BackFloor':
                back_floor_ends.append(start + duration)
        if notify and isinstance(notify, unreal.AZ_AnimNotify_SendGameplayEvent):
            if HANDOFF in notify.get_editor_property('event_tag').export_text():
                native_events.append(start)
    require(len(blend_starts) == 1 and back_floor_ends, 'Missing/ambiguous authored landing handoff: ' + montage.get_name())
    # Keep all required warp windows complete and allow one 60 Hz frame for the final delta.
    marker = max(blend_starts[0], max(warp_ends) + 1.0 / 60.0)
    require(marker < montage.get_play_length() - 0.1, 'No useful handoff room: ' + montage.get_name())
    return {'montage': montage.get_path_name(), 'length': montage.get_play_length(),
            'gasp_blend_start': blend_starts[0], 'last_warp_end': max(warp_ends),
            'back_floor_end': max(back_floor_ends), 'marker': marker,
            'native_handoffs': native_events}


def fence_envelope():
    mesh = unreal.load_asset(MESH)
    description = mesh.get_static_mesh_description(0)
    require(description, 'Fence source mesh description is unavailable.')
    points = []
    for index in range(description.get_vertex_count()):
        vertex = unreal.VertexID()
        vertex.import_text('(IDValue=%d)' % index)
        if description.is_vertex_valid(vertex):
            point = description.get_vertex_position(vertex)
            # The existing 31 cm bounds include the concrete plinth. The metal fence above
            # it is only ~5.85 cm deep; contact edges must describe that actual upper part.
            if point.z >= 30.0:
                points.append((point.x, point.y, point.z))
    require(points, 'No upper fence geometry was measured.')
    minimum = [min(p[axis] for p in points) for axis in range(3)]
    maximum = [max(p[axis] for p in points) for axis in range(3)]
    return {'upper_min': minimum, 'upper_max': maximum,
            'center': [(minimum[0] + maximum[0]) / 2,
                       (minimum[1] + maximum[1]) / 2, maximum[2] / 2],
            'extent': [(maximum[0] - minimum[0]) / 2,
                       (maximum[1] - minimum[1]) / 2, maximum[2] / 2],
            'sample_count': len(points)}


def original_scene():
    module = runpy.run_path(str(PROJECT / 'Tools' / 'smart_actor_surfaces_setup.py'))
    return module['capture']()


def package_file(asset_path):
    package = asset_path.split('.')[0]
    require(package.startswith('/Game/'), 'Unexpected package: ' + package)
    return PROJECT / 'Content' / (package[6:] + '.uasset')


def backup(paths):
    directory = RECEIPTS / ('Backup_' + datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S_%f'))
    directory.mkdir(parents=True, exist_ok=False)
    hashes = {}
    for path in paths:
        source = package_file(path)
        if source.exists():
            target = directory / source.name
            require(not target.exists(), 'Duplicate backup filename: ' + str(target))
            shutil.copy2(source, target)
            hashes[str(source)] = hashlib.sha256(source.read_bytes()).hexdigest()
    (directory / 'hashes.json').write_text(json.dumps(hashes, indent=2), encoding='utf-8')
    return str(directory)


def author():
    profile_class = unreal.AZ_TraversalSurfaceProfile
    data_class = unreal.AZ_TraversalSurfaceData
    require(hasattr(profile_class(), 'allow_climb_and_drop'), 'Native climb/drop profile field is not loaded.')
    montages = moving_hurdles()
    plan = [handoff_plan(montage) for montage in montages]
    envelope = fence_envelope()
    before = original_scene()
    record = {'scene': before, 'fence_envelope': envelope, 'handoffs': plan,
              'backup': backup([MESH, PROFILE] + [row['montage'] for row in plan])}
    save_receipt('before', record)

    # Add an AZ event at the source's supported post-landing blend point. Keep source
    # GASP notifies/data intact and avoid modifying any standing or raw sequence asset.
    for montage, row in zip(montages, plan):
        require(unreal.AZ_MontageUtils.add_gameplay_event_notify(
            montage, HANDOFF, row['marker'], 'AZ_Handoff', True),
            'Could not author handoff: ' + montage.get_name())
        require(unreal.EditorAssetLibrary.save_loaded_asset(montage, only_if_is_dirty=False),
                'Montage save failed: ' + montage.get_name())

    profile = unreal.load_asset(PROFILE)
    if profile:
        require(isinstance(profile, profile_class), 'Profile path contains another asset type.')
        require(unreal.EditorAssetLibrary.get_metadata_tag(profile, OWNER_KEY) == OWNER_VALUE,
                'Existing profile is not owned by this authoring script.')
    else:
        profile = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            PROFILE.rsplit('/', 1)[-1], FOLDER, profile_class, None)
        require(profile, 'Profile creation failed.')
    tags = unreal.GameplayTagContainer()
    tags.import_text('(GameplayTags=(' + ','.join('(TagName="' + name + '")' for name in TAG_NAMES) + '))')
    require(all(name in tags.export_text() for name in TAG_NAMES), 'Surface tags are not registered.')
    profile.modify()
    values = {'allow_mantle': False, 'allow_hurdle': True, 'allow_climb': True,
              'allow_climb_and_drop': True, 'supports_standing': False, 'supports_crossing': True,
              'geometry_source': unreal.AZ_TraversalSurfaceGeometrySource.CUSTOM_BOUNDS,
              'bounds_center': unreal.Vector(*envelope['center']),
              'bounds_extent': unreal.Vector(*envelope['extent']),
              'edge_end_margin': 5.0, 'surface_tags': tags}
    for field, value in values.items():
        profile.set_editor_property(field, value)
    unreal.EditorAssetLibrary.set_metadata_tag(profile, OWNER_KEY, OWNER_VALUE)
    require(unreal.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False), 'Profile save failed.')
    mesh = unreal.load_asset(MESH)
    data = mesh.get_asset_user_data_of_class(data_class)
    require(data, 'The existing mesh surface binding is missing.')
    mesh.modify()
    data.modify()
    data.set_editor_property('profile', profile)
    data.set_editor_property('enabled', True)
    require(unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False), 'Mesh binding save failed.')
    require(original_scene() == before, 'Fence geometry/material/collision or actor placement changed.')
    return verify()


def verify():
    plan = [handoff_plan(montage) for montage in moving_hurdles()]
    for row in plan:
        require(len(row['native_handoffs']) == 1
                and abs(row['native_handoffs'][0] - row['marker']) < 0.002,
                'Handoff readback mismatch: ' + row['montage'])
    mesh = unreal.load_asset(MESH)
    data = mesh.get_asset_user_data_of_class(unreal.AZ_TraversalSurfaceData)
    profile = unreal.load_asset(PROFILE)
    require(data and data.get_editor_property('profile') == profile, 'Fence profile binding differs.')
    require(profile.get_editor_property('allow_climb_and_drop') and not profile.get_editor_property('supports_standing'),
            'Fence climb/drop must be explicit without inventing standing support.')
    descriptions = []
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            if component.get_editor_property('static_mesh') == mesh:
                descriptions.append({'component': component.get_path_name(),
                                     'surface': unreal.AZ_TraversalSurfaceData.describe_surface(component)})
    record = {'profile': PROFILE, 'handoffs': plan, 'fence_envelope': fence_envelope(),
              'scene': original_scene(), 'surface_readback': descriptions,
              'status': 'saved_readback_complete', 'gameplay_check': 'user pending; no PIE started'}
    save_receipt('verified', record)
    print('PARKOUR_FLOW_VERIFIED', len(plan), 'moving hurdle handoffs;', PROFILE)
    return record


def main(operation='audit'):
    try:
        if operation == 'audit':
            report = {'handoffs': [handoff_plan(montage) for montage in moving_hurdles()],
                      'fence_envelope': fence_envelope(), 'scene': original_scene()}
            save_receipt('audit', report)
            return report
        if operation == 'author':
            return author()
        if operation == 'verify':
            return verify()
        raise ValueError(operation)
    finally:
        gc.collect()
