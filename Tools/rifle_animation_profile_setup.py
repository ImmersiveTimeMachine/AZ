# @Description: Audit or prepare owned rifle loop databases, contact curves and aim offsets.
"""Editor authoring only; call main(target_map, prepare=True) to write assets.

target_map is an explicit {RifleAnimsetPro source package: owned target package}
mapping from rifle_animation_content_setup.py receipts. No CHT/CDO activation,
PIE, preview, index execution or tests. Newly measured contact curves and nominal
aim axis coordinates require visual validation before treating them as final.
"""
import gc
import hashlib
import json
import math
from pathlib import Path

import unreal


SOURCE_ROOT = '/Game/RifleAnimsetPro/Animations'
TARGET_ROOT = '/Game/AZ/Assets/RTG/RifleAnimsetPro'
PROFILE_ROOT = '/Game/AZ/Blueprints/Animation/MotionMatching/Rifle'
SCHEMA_PATH = '/Game/AZ/Blueprints/Animation/MotionMatching/PSS_v2_SurvivalMan_Loco'
TARGET_MESH = '/Game/SurvivalMan/Meshes/SKM_SurvivalMan_Mesh1'
OWNER = 'rifle_animation_profile_setup:v1'
OWNER_KEY = 'AZ.RifleProfile.Owner'
MANIFEST_KEY = 'AZ.RifleProfile.Manifest'
CONTENT_OWNER = 'rifle_animation_content_setup:v1'
EAL = unreal.EditorAssetLibrary
AL = unreal.AnimationLibrary
PU = unreal.AZ_PoseSearchUtils

GROUPS = {
    'Walk': ['Rifle_WalkFwdLoop'],
    'Run': ['Rifle_RunFwdLoop'],
    'StrafeWalk8': ['Rifle_WalkFwdLoop', 'Rifle_StrafeLeft45Loop', 'Rifle_StrafeLeftLoop',
                    'Rifle_StrafeLeft135Loop', 'Rifle_WalkBwdLoop', 'Rifle_StrafeRight135Loop',
                    'Rifle_StrafeRightLoop', 'Rifle_StrafeRight45Loop'],
    'StrafeRun8': ['Rifle_RunFwdLoop', 'Rifle_StrafeRun45LeftLoop', 'Rifle_StrafeRunLeftLoop',
                   'Rifle_StrafeRun135LeftLoop', 'Rifle_RunBwdLoop', 'Rifle_StrafeRun135RightLoop',
                   'Rifle_StrafeRunRightLoop', 'Rifle_StrafeRun45RightLoop'],
    'Crouch8': ['Rifle_Crouch_WalkFwd', 'Rifle_Crouch_StrafeLeftt45', 'Rifle_Crouch_WalkLt',
                'Rifle_Crouch_StrafeLeft135', 'Rifle_Crouch_WalkBwd', 'Rifle_Crouch_StrafeRight135',
                'Rifle_Crouch_WalkRt', 'Rifle_Crouch_StrafeRight45'],
    'Sprint': ['Rifle_SprintLoop'],
}
AIM = {
    'Standing': [('Aim_CC', 0, 0), ('Aim_D', 0, -1), ('Aim_U', 0, 1),
                 ('Aim_L', -1, 0), ('Aim_LD', -1, -1), ('Aim_LU', -1, 1),
                 ('Aim_R', 1, 0), ('Aim_RD', 1, -1), ('Aim_RU', 1, 1)],
    'Crouched': [('Rifle_Crouch_AimAdditive_' + suffix, x, y) for suffix, x, y in (
        ('CC', 0, 0), ('CD', 0, -1), ('CU', 0, 1), ('LC', -1, 0),
        ('LD', -1, -1), ('LU', -1, 1), ('RC', 1, 0), ('RD', 1, -1), ('RU', 1, 1))],
}
DEFAULT_CONTACT_SETTINGS = {
    'sample_rate_hz': 120.0,
    'max_foot_speed_cm_s': 45.0,
    'floor_tolerance_cm': 3.0,
    'minimum_plant_seconds': 0.04,
    'left_bone': 'foot_l',
    'right_bone': 'foot_r',
}


def package(asset):
    return asset.get_path_name().split('.')[0]


def load(path, cls=None):
    asset = unreal.load_asset(path)
    if asset is None or (cls is not None and not isinstance(asset, cls)):
        raise RuntimeError('Missing asset or unexpected class: ' + path)
    return asset


def save(asset):
    if not EAL.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Save failed: ' + package(asset))


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode('utf-8')).hexdigest()


def claim(asset, manifest):
    EAL.set_metadata_tag(asset, OWNER_KEY, OWNER)
    EAL.set_metadata_tag(asset, MANIFEST_KEY, digest(manifest))


def owned_existing(path, manifest, cls):
    if not EAL.does_asset_exist(path):
        return None
    asset = load(path, cls)
    if (EAL.get_metadata_tag(asset, OWNER_KEY) != OWNER
            or EAL.get_metadata_tag(asset, MANIFEST_KEY) != digest(manifest)):
        raise RuntimeError('Refusing unknown asset or changed profile manifest: ' + path)
    return asset


def create_owned(path, manifest, cls, factory=None):
    asset = owned_existing(path, manifest, cls)
    if asset:
        return asset
    folder, name = path.rsplit('/', 1)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, cls, factory)
    if not asset:
        raise RuntimeError('Asset creation failed: ' + path)
    claim(asset, manifest)
    return asset


def target_map_from_receipts(*receipt_paths):
    result = {}
    for path in receipt_paths:
        data = json.loads(Path(path).read_text(encoding='utf-8'))
        if data.get('status') != 'prepared_candidates':
            raise ValueError('Not a completed content preparation receipt: ' + str(path))
        for record in data['records']:
            source, target = record['source'], record['target']
            if source in result and result[source] != target:
                raise ValueError('Conflicting target mappings: ' + source)
            result[source] = target
    return result


def db_members(db):
    return [package(db.get_animation_asset(i)) for i in range(db.get_num_animation_assets())]


def branch_notifies(seq):
    result = []
    for event in AL.get_animation_notify_events(seq):
        state = event.get_editor_property('notify_state_class')
        if isinstance(state, unreal.AnimNotifyState_PoseSearchBranchIn):
            db = state.get_editor_property('database')
            result.append({'database': package(db) if db else None,
                           'start': float(AL.get_anim_notify_event_trigger_time(event)),
                           'duration': float(AL.get_anim_notify_event_duration(event))})
    return result


def curve_keys(seq, name):
    if not AL.does_curve_exist(seq, name, unreal.RawCurveTrackTypes.RCT_FLOAT):
        return None
    times, values = AL.get_float_keys(seq, name)
    return {'times': [float(t) for t in times], 'values': [float(v) for v in values]}


def intervals(times, flags):
    result, start = [], None
    for i, planted in enumerate(flags):
        if planted and start is None:
            start = times[i]
        if start is not None and (not planted or i == len(flags) - 1):
            end = times[i]
            result.append([start, end])
            start = None
    return result


def contact_measurement(seq, mesh, settings):
    length = float(AL.get_sequence_length(seq))
    if length <= 0:
        raise RuntimeError('Cannot sample a zero-length loop: ' + package(seq))
    steps = max(3, int(math.ceil(length * settings['sample_rate_hz'])))
    times = [i * length / steps for i in range(steps + 1)]
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
    options.set_editor_property('extract_root_motion', False)
    options.set_editor_property('incorporate_root_motion_into_pose', True)
    options.set_editor_property('optional_skeletal_mesh', mesh)
    positions = {'contact_l': [], 'contact_r': []}
    roots = []
    for time in times:
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, time, options)
        if not unreal.AnimPoseExtensions.is_valid(pose):
            raise RuntimeError('Invalid pose: ' + package(seq))
        names = {str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)}
        for curve, bone in (('contact_l', settings['left_bone']), ('contact_r', settings['right_bone']), ('root', 'root')):
            if bone not in names:
                raise RuntimeError('Missing measured bone ' + bone + ': ' + package(seq))
            transform = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
            vector = transform.translation
            point = [float(vector.x), float(vector.y), float(vector.z)]
            if not all(math.isfinite(value) for value in point):
                raise RuntimeError('Nonfinite pose data: ' + package(seq))
            if curve == 'root':
                roots.append(point)
            else:
                positions[curve].append(point)
    result = {'length_seconds': length, 'settings': settings,
              'root_delta': [roots[-1][i] - roots[0][i] for i in range(3)],
              'time_step_seconds': length / steps, 'curves': {}}
    for name, points in positions.items():
        # Root-incorporated component positions make a planted foot nearly still.
        # Use centered differences internally and one-sided differences at endpoints;
        # never difference across the root-motion discontinuity at a loop wrap.
        speeds = []
        for i in range(len(times)):
            before, after = max(0, i - 1), min(len(times) - 1, i + 1)
            speed = math.dist(points[before], points[after]) / (times[after] - times[before])
            speeds.append(speed)
        floor = min(point[2] for point in points)
        flags = [speed <= settings['max_foot_speed_cm_s']
                 and point[2] - floor <= settings['floor_tolerance_cm']
                 for speed, point in zip(speeds, points)]
        # Reject very brief interior threshold crossings. Boundary plants are kept:
        # their duration may continue across the next/previous loop.
        for start, end in intervals(times, flags):
            if start > 0 and end < length and end - start < settings['minimum_plant_seconds']:
                flags = [False if start <= t < end else flag for t, flag in zip(times, flags)]
        keys = {'times': times, 'values': [1.0 if flag else 0.0 for flag in flags]}
        result['curves'][name] = {
            'floor_bone_height_cm': floor,
            'minimum_speed_cm_s': min(speeds), 'maximum_speed_cm_s': max(speeds),
            'speed_cm_s': speeds, 'height_above_floor_cm': [p[2] - floor for p in points],
            'plant_intervals': intervals(times, flags), 'keys': keys,
            'seam_values_match': flags[0] == flags[-1],
            'existing_keys': curve_keys(seq, name),
            'owned': EAL.get_metadata_tag(seq, 'AZ.RifleProfile.Contact.' + name) == OWNER,
        }
    return result


def write_contacts(seq, measured):
    changed = False
    for name, record in measured['curves'].items():
        if record['existing_keys'] is not None and not record['owned']:
            record['action'] = 'preserved_unmanaged_existing_curve'
            continue
        keys = record['keys']
        if record['existing_keys'] == keys:
            record['action'] = 'reused'
            continue
        if record['existing_keys'] is not None:
            AL.remove_curve(seq, name)
        AL.add_curve(seq, name, unreal.RawCurveTrackTypes.RCT_FLOAT)
        AL.add_float_curve_keys(seq, name, keys['times'], keys['values'])
        EAL.set_metadata_tag(seq, 'AZ.RifleProfile.Contact.' + name, OWNER)
        actual = curve_keys(seq, name)
        if (actual is None or len(actual['times']) != len(keys['times'])
                or any(abs(a - b) > 1e-5 for a, b in zip(actual['times'], keys['times']))
                or actual['values'] != keys['values']):
            raise RuntimeError('Contact curve readback mismatch: ' + package(seq) + ' ' + name)
        record['action'] = 'authored_measured_candidate'
        changed = True
    if changed:
        EAL.set_metadata_tag(seq, 'AZ.RifleProfile.ContactSettings', json.dumps(measured['settings'], sort_keys=True))
        save(seq)


def main(target_map, prepare=False, branch_in=None, contact_settings=None,
         receipt_path=None, build_aim=True, aim_yaw_degrees=90.0, aim_pitch_degrees=90.0):
    """Audit by default. branch_in maps explicit source package paths to GROUPS keys.

    For the current chooser plan pass {source_root + '/RootMotion/Rifle_SprintLoop':
    'Sprint'}; other groups are returned as whole databases. No database activation
    or index build is performed. Contact thresholds use target mesh centimeters.
    """
    receipt = {'mode': 'prepare' if prepare else 'audit', 'status': 'running',
               'databases': {}, 'contacts': {}, 'aim_offsets': {}, 'warnings': []}
    try:
        if not isinstance(target_map, dict) or not target_map:
            raise ValueError('Provide an explicit nonempty source -> target package map')
        branch_in = dict(branch_in or {})
        settings = dict(DEFAULT_CONTACT_SETTINGS)
        for key, value in (contact_settings or {}).items():
            if key not in settings:
                raise ValueError('Unknown contact setting: ' + key)
            settings[key] = value
        for key in ('sample_rate_hz', 'max_foot_speed_cm_s', 'floor_tolerance_cm', 'minimum_plant_seconds'):
            if not isinstance(settings[key], (int, float)) or not math.isfinite(settings[key]) or settings[key] <= 0:
                raise ValueError('Contact settings must be finite and positive: ' + key)
        if settings['sample_rate_hz'] > 240:
            raise ValueError('Contact sampling is bounded to 240 Hz')
        if not all(math.isfinite(x) and 0 < x <= 180 for x in (aim_yaw_degrees, aim_pitch_degrees)):
            raise ValueError('Aim axis limits must be within (0,180] degrees')
        mesh = load(TARGET_MESH, unreal.SkeletalMesh)
        skeleton = mesh.get_editor_property('skeleton')
        schema = load(SCHEMA_PATH, unreal.PoseSearchSchema)
        sources = sorted({SOURCE_ROOT + '/RootMotion/' + name for group in GROUPS.values() for name in group})
        if build_aim:
            sources += sorted({SOURCE_ROOT + '/AimOffsets/' + name for group in AIM.values() for name, _, _ in group})
        missing = [source for source in sources if source not in target_map]
        if missing:
            raise ValueError('Missing required target mappings: ' + json.dumps(missing))
        sequences = {}
        for source in sources:
            path = target_map[source]
            if not isinstance(path, str) or not path.startswith(TARGET_ROOT + '/') or '/_RetargetRaw/' in path or '..' in path:
                raise ValueError('Expected an owned final target package: ' + str(path))
            seq = load(path, unreal.AnimSequence)
            if (EAL.get_metadata_tag(seq, 'AZ.RifleAnimation.Owner') != CONTENT_OWNER
                    or EAL.get_metadata_tag(seq, 'AZ.RifleAnimation.Source') != source
                    or EAL.get_metadata_tag(seq, 'AZ.RifleAnimation.Stage') != 'prepared'):
                raise RuntimeError('Unrecognized prepared target: ' + path)
            if seq.get_editor_property('skeleton') != skeleton:
                raise RuntimeError('Target skeleton mismatch: ' + path)
            if '/RootMotion/' in source and not seq.get_editor_property('loop'):
                raise RuntimeError('Expected an authored looping sequence: ' + path)
            sequences[source] = seq
        for source, key in branch_in.items():
            if key not in GROUPS or source not in [SOURCE_ROOT + '/RootMotion/' + name for name in GROUPS[key]]:
                raise ValueError('BranchIn source must belong to its requested database: ' + source)
        manifests, databases = {}, {}
        for key, names in GROUPS.items():
            group_sources = [SOURCE_ROOT + '/RootMotion/' + name for name in names]
            path = PROFILE_ROOT + '/PSD_Rifle_' + key
            manifest = {'schema': SCHEMA_PATH, 'targets': [target_map[s] for s in group_sources],
                        'branch_in': [s for s in group_sources if branch_in.get(s) == key]}
            manifests[key] = manifest
            db = owned_existing(path, manifest, unreal.PoseSearchDatabase)
            members = db_members(db) if db else []
            if len(members) != len(set(members)) or any(m not in manifest['targets'] for m in members):
                raise RuntimeError('Unexpected or duplicate database entries: ' + path)
            databases[key] = db
            receipt['databases'][key] = {'path': path, 'manifest': manifest, 'existing_members': members}
        normalization_path = PROFILE_ROOT + '/PSN_RifleGaits'
        normalization_manifest = {'databases': [receipt['databases'][k]['path'] for k in ('Walk', 'Run', 'Sprint')]}
        owned_existing(normalization_path, normalization_manifest, unreal.PoseSearchNormalizationSet)
        for source, seq in sequences.items():
            if '/RootMotion/' not in source:
                continue
            notifies = branch_notifies(seq)
            expected_db = receipt['databases'][branch_in[source]]['path'] if source in branch_in else None
            if notifies and (not expected_db or len(notifies) != 1 or notifies[0]['database'] != expected_db):
                raise RuntimeError('Unexpected pre-existing BranchIn; refusing to strip notifies: ' + package(seq))
            if notifies and (abs(notifies[0]['start']) > 0.001
                             or abs(notifies[0]['duration'] - AL.get_sequence_length(seq)) > 0.001):
                raise RuntimeError('BranchIn must cover the complete loop: ' + package(seq))
        if build_aim:
            for stance, samples in AIM.items():
                sample_specs = []
                expected_base = TARGET_ROOT + '/AimOffsets/Bases/AS_RAP_' + stance + '_Base'
                for name, x, y in samples:
                    source = SOURCE_ROOT + '/AimOffsets/' + name
                    seq = sequences[source]
                    base = seq.get_editor_property('ref_pose_seq')
                    if (seq.get_editor_property('additive_anim_type') != unreal.AdditiveAnimationType.AAT_ROTATION_OFFSET_MESH_SPACE
                            or seq.get_editor_property('ref_pose_type') != unreal.AdditiveBasePoseType.ABPT_ANIM_FRAME
                            or seq.get_editor_property('ref_frame_index') != 0 or not base or package(base) != expected_base
                            or base.get_editor_property('skeleton') != skeleton
                            or base.get_editor_property('additive_anim_type') != unreal.AdditiveAnimationType.AAT_NONE
                            or EAL.get_metadata_tag(base, 'AZ.RifleAnimation.Owner') != CONTENT_OWNER):
                        raise RuntimeError('Aim sample/base preparation mismatch: ' + package(seq))
                    sample_specs.append({'target': target_map[source], 'x': x * aim_yaw_degrees, 'y': y * aim_pitch_degrees})
                path = PROFILE_ROOT + '/AO_Rifle_' + stance
                manifest = {'skeleton': package(skeleton), 'base': expected_base, 'samples': sample_specs,
                            'yaw_limit': aim_yaw_degrees, 'pitch_limit': aim_pitch_degrees}
                owned_existing(path, manifest, unreal.AimOffsetBlendSpace)
                receipt['aim_offsets'][stance] = {'path': path, 'manifest': manifest}
        receipt['native_helpers'] = {
            'disable_reselection': hasattr(PU, 'set_disable_reselection_on_database'),
            'rebuild_blend_space': hasattr(unreal.AZ_AnimGraphNodeUtils, 'rebuild_blend_space')}
        if prepare and (not receipt['native_helpers']['disable_reselection']
                        or (build_aim and not receipt['native_helpers']['rebuild_blend_space'])):
            raise RuntimeError('Required native helpers are not loaded; complete the closed editor build first')
        # Pose sampling is read-only. Every collision/ownership check above precedes writes.
        for source, seq in sequences.items():
            if '/RootMotion/' not in source:
                continue
            measured = contact_measurement(seq, mesh, settings)
            receipt['contacts'][package(seq)] = measured
            for curve, record in measured['curves'].items():
                if not record['plant_intervals']:
                    receipt['warnings'].append(package(seq) + ' ' + curve + ': no plants detected; inspect thresholds/retarget')
                if not record['seam_values_match']:
                    receipt['warnings'].append(package(seq) + ' ' + curve + ': sampled endpoint contacts differ')
                record['action'] = ('preserve_unmanaged_existing_curve' if record['existing_keys'] is not None
                                    and not record['owned'] else 'proposed_measured_curve')
        if not prepare:
            receipt['status'] = 'audit_complete'
            return receipt
        for source, seq in sequences.items():
            if '/RootMotion/' in source:
                write_contacts(seq, receipt['contacts'][package(seq)])
        for key in GROUPS:
            db = create_owned(receipt['databases'][key]['path'], manifests[key], unreal.PoseSearchDatabase)
            db.set_editor_property('schema', schema)
            databases[key] = db
        for key, names in GROUPS.items():
            db = databases[key]
            for name in names:
                source = SOURCE_ROOT + '/RootMotion/' + name
                seq = sequences[source]
                members = db_members(db)
                if branch_in.get(source) == key:
                    if not branch_notifies(seq):
                        if package(seq) in members:
                            raise RuntimeError('Explicit entry would duplicate new BranchIn membership: ' + package(seq))
                        if not PU.add_branch_in_notify(seq, db, 0.0, 0.0):
                            raise RuntimeError('BranchIn authoring failed: ' + package(seq))
                        save(seq)
                    # BranchIn owns this DB entry. Never add it explicitly.
                elif package(seq) not in members:
                    if not PU.add_sequence_to_database(db, seq):
                        raise RuntimeError('Database entry authoring failed: ' + package(seq))
            members = db_members(db)
            if sorted(members) != sorted(manifests[key]['targets']):
                raise RuntimeError('Database membership mismatch (BranchIn sync may be pending): ' + package(db))
            updated = PU.set_disable_reselection_on_database(db, True)
            if updated != len(members):
                raise RuntimeError('Disable-reselection helper did not update every entry: ' + package(db))
            receipt['databases'][key].update({'members': members, 'disable_reselection_count': updated,
                                               'index_status': 'not_built_by_this_script'})
        normalization = create_owned(normalization_path, normalization_manifest, unreal.PoseSearchNormalizationSet)
        normalization.set_editor_property('databases', [databases[k] for k in ('Walk', 'Run', 'Sprint')])
        if [package(db) for db in normalization.get_editor_property('databases')] != normalization_manifest['databases']:
            raise RuntimeError('Normalization set readback mismatch')
        save(normalization)
        for key, db in databases.items():
            expected = normalization if key in ('Walk', 'Run', 'Sprint') else None
            db.set_editor_property('normalization_set', expected)
            if db.get_editor_property('normalization_set') != expected or db.get_editor_property('schema') != schema:
                raise RuntimeError('Database schema/normalization readback mismatch: ' + package(db))
            save(db)
        receipt['normalization'] = normalization_manifest
        if build_aim:
            for stance, record in receipt['aim_offsets'].items():
                factory = unreal.AimOffsetBlendSpaceFactoryNew()
                factory.set_editor_property('target_skeleton', skeleton)
                factory.set_editor_property('preview_skeletal_mesh', mesh)
                asset = create_owned(record['path'], record['manifest'], unreal.AimOffsetBlendSpace, factory)
                params = list(asset.get_editor_property('blend_parameters'))
                for i, title, limit in ((0, 'Yaw', aim_yaw_degrees), (1, 'Pitch', aim_pitch_degrees)):
                    param = unreal.BlendParameter()
                    for name, value in {'display_name': title, 'min': -limit, 'max': limit,
                                        'grid_num': 2, 'snap_to_grid': True, 'wrap_input': False}.items():
                        param.set_editor_property(name, value)
                    params[i] = param
                asset.set_editor_property('blend_parameters', params)
                samples = []
                for spec in record['manifest']['samples']:
                    sample = unreal.BlendSample()
                    sample.set_editor_property('animation', load(spec['target'], unreal.AnimSequence))
                    sample.set_editor_property('sample_value', unreal.Vector(spec['x'], spec['y'], 0.0))
                    sample.set_editor_property('rate_scale', 1.0)
                    sample.set_editor_property('use_single_frame_for_blending', True)
                    sample.set_editor_property('frame_index_to_sample', 0)
                    samples.append(sample)
                asset.set_editor_property('sample_data', samples)
                if not unreal.AZ_AnimGraphNodeUtils.rebuild_blend_space(asset):
                    raise RuntimeError('AimOffset validation/triangulation failed: ' + record['path'])
                actual = asset.get_editor_property('sample_data')
                expected = {(spec['target'], spec['x'], spec['y']) for spec in record['manifest']['samples']}
                found = {(package(s.get_editor_property('animation')), float(s.get_editor_property('sample_value').x),
                          float(s.get_editor_property('sample_value').y)) for s in actual}
                if len(actual) != 9 or found != expected:
                    raise RuntimeError('AimOffset samples changed unexpectedly: ' + record['path'])
                save(asset)
                record['sample_count'] = len(actual)
                record['rebuilt'] = True
        receipt['status'] = 'prepared_candidates'
        receipt['warnings'].append('No CHT/CDO activation or index build performed. Require BuildIndex Succeeded evidence before runtime use.')
        receipt['warnings'].append('Foot curves use measured threshold candidates; aim coordinates use nominal signed limits. Both need visual validation.')
        return receipt
    except Exception as error:
        receipt['status'] = 'failed'
        receipt['error'] = str(error)
        raise
    finally:
        if receipt_path:
            destination = Path(receipt_path)
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_text(json.dumps(receipt, indent=2), encoding='utf-8')
        # Dense sample measurements stay in the receipt rather than flooding the log.
        print('[RifleAnimationProfile] ' + json.dumps({
            'mode': receipt['mode'], 'status': receipt['status'], 'error': receipt.get('error'),
            'database_count': len(receipt['databases']), 'contact_clip_count': len(receipt['contacts']),
            'aim_offset_count': len(receipt['aim_offsets']), 'warnings': receipt['warnings'],
            'receipt_path': str(receipt_path) if receipt_path else None}, sort_keys=True))
        gc.collect()
