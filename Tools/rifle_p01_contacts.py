# @Description: Audit matching P01 in-place/root-motion phases, then optionally author target contacts.
"""Existing Riffle_P01 content only. No retargets, previews, PIE or tests.

Load with runpy.run_path() and call main(). The default is read-only audit of the
48 Walk/Jog/CrouchWalk loops in the saved P01 inventory. include_run_r=True adds
the inventory's lone Run_R loop. prepare=True backs up and writes only validated
targets' contact_l/contact_r curves. Source assets are never modified.
"""
import gc
import hashlib
import json
import math
from datetime import datetime, timezone
from pathlib import Path
import shutil

import unreal


PROJECT = Path('C:/UnrealEngine/Games/AZ')
INVENTORY = PROJECT / 'Saved/RifleAnimationContent/riffle-p01-audit.json'
TARGET_ROOT = '/Game/AZ/Assets/RTG/Riffle_P01'
RM_ROOT = '/Game/Rifle_01/Animation/Root_Motion'
IPC_ROOT = '/Game/Rifle_01/Animation/In-Place'
SOURCE_MESH = '/Game/Rifle_01/Character/Mesh/SK_Mannequin'
TARGET_MESH = '/Game/SurvivalMan/Meshes/SKM_SurvivalMan_Mesh1'
OWNER = 'rifle_p01_contacts:v1'
OWNER_KEY = 'AZ.RifleP01Contacts.Owner'
LEG_BONES = ('thigh_l', 'calf_l', 'foot_l', 'thigh_r', 'calf_r', 'foot_r')
EAL = unreal.EditorAssetLibrary
AL = unreal.AnimationLibrary
DEFAULTS = {
    'sample_rate_hz': 120.0,
    'maximum_plant_speed_cm_s': 45.0,
    'floor_tolerance_cm': 3.0,
    'minimum_plant_seconds': 0.04,
    'length_tolerance_seconds': 0.0001,
    'source_leg_angle_tolerance_degrees': 2.0,
    'minimum_rm_root_speed_cm_s': 5.0,
    'maximum_ipc_root_speed_cm_s': 5.0,
    'minimum_pattern_correlation': 0.80,
    'maximum_normalized_pattern_error': 0.20,
    'maximum_target_plant_height': 0.45,
    'maximum_plant_fraction': 0.85,
}


def package(asset):
    return asset.get_path_name().split('.')[0]


def load(path, cls):
    asset = unreal.load_asset(path)
    if not asset or not isinstance(asset, cls):
        raise RuntimeError('Missing asset or incorrect class: ' + path)
    return asset


def ensure(condition, message):
    if not condition:
        raise RuntimeError(message)


def snapshot(seq):
    curves = {}
    for name in AL.get_animation_curve_names(seq, unreal.RawCurveTrackTypes.RCT_FLOAT):
        times, values = AL.get_float_keys(seq, name)
        # FName identity is case-insensitive; editor curve writes may normalize its display spelling.
        curves[str(name).casefold()] = {'times': [float(t) for t in times], 'values': [float(v) for v in values]}
    return {'path': package(seq), 'skeleton': package(seq.get_editor_property('skeleton')),
            'length': float(AL.get_sequence_length(seq)), 'frames': int(AL.get_num_frames(seq)),
            'loop': bool(seq.get_editor_property('loop')),
            'enable_root_motion': bool(seq.get_editor_property('enable_root_motion')),
            'force_root_lock': bool(seq.get_editor_property('force_root_lock')),
            'additive': str(seq.get_editor_property('additive_anim_type')),
            'curves': curves}


def quat_angle(a, b):
    dot = abs(sum(x * y for x, y in zip(a, b)))
    denom = math.sqrt(sum(x * x for x in a) * sum(x * x for x in b))
    ensure(denom > 1e-8, 'Invalid sampled bone quaternion')
    return math.degrees(2.0 * math.acos(min(1.0, max(0.0, dot / denom))))


def sample(seq, mesh, times, include_angles):
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
    options.set_editor_property('should_retarget', False)
    options.set_editor_property('extract_root_motion', False)
    options.set_editor_property('incorporate_root_motion_into_pose', True)
    options.set_editor_property('optional_skeletal_mesh', mesh)
    result = {'root': [], 'root_transforms': [], 'foot_l': [], 'foot_r': [],
              'angles': {bone: [] for bone in LEG_BONES}}
    for time in times:
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, time, options)
        ensure(unreal.AnimPoseExtensions.is_valid(pose), 'Invalid raw pose: ' + package(seq))
        names = {str(name) for name in unreal.AnimPoseExtensions.get_bone_names(pose)}
        required = {'root', 'foot_l', 'foot_r'} | (set(LEG_BONES) if include_angles else set())
        ensure(required <= names, 'Required bones missing: ' + package(seq) + ' ' + str(required - names))
        for bone in ('root', 'foot_l', 'foot_r'):
            transform = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
            point = transform.translation
            vector = [float(point.x), float(point.y), float(point.z)]
            ensure(all(math.isfinite(x) for x in vector), 'Nonfinite bone translation: ' + package(seq))
            result[bone].append(vector)
            if bone == 'root':
                result['root_transforms'].append(transform)
        if include_angles:
            for bone in LEG_BONES:
                transform = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.LOCAL)
                q = transform.rotation
                result['angles'][bone].append([float(q.x), float(q.y), float(q.z), float(q.w)])
    return result


def speeds(points, times):
    values = []
    for i in range(len(points)):
        before, after = max(0, i - 1), min(len(points) - 1, i + 1)
        values.append(math.dist(points[before], points[after]) / (times[after] - times[before]))
    return values


def root_motion(sampled, times):
    roots = sampled['root']
    delta = [roots[-1][i] - roots[0][i] for i in range(3)]
    travel = sum(math.dist(a, b) for a, b in zip(roots, roots[1:]))
    return {'first': roots[0], 'last': roots[-1], 'delta': delta,
            'net_planar_speed_cm_s': math.hypot(delta[0], delta[1]) / times[-1],
            'mean_path_speed_cm_s': travel / times[-1]}


def normalize(values):
    minimum, maximum = min(values), max(values)
    span = maximum - minimum
    return ([0.0] * len(values) if span < 1e-5 else [(v - minimum) / span for v in values]), span


def pattern_comparison(source, target):
    a, source_span = normalize(source)
    b, target_span = normalize(target)
    ma, mb = sum(a) / len(a), sum(b) / len(b)
    va = sum((x - ma) ** 2 for x in a)
    vb = sum((x - mb) ** 2 for x in b)
    if va < 1e-8 and vb < 1e-8:
        correlation = 1.0
    elif va < 1e-8 or vb < 1e-8:
        correlation = 0.0
    else:
        correlation = sum((x - ma) * (y - mb) for x, y in zip(a, b)) / math.sqrt(va * vb)
    return {'correlation': correlation, 'mean_absolute_error': sum(abs(x - y) for x, y in zip(a, b)) / len(a),
            'source_span': source_span, 'target_span': target_span,
            'source_normalized': a, 'target_normalized': b}


def foot_patterns(sampled, times, bone):
    relative = [[point[i] - root[i] for i in range(3)] for point, root in zip(sampled[bone], sampled['root'])]
    return {'height': [p[2] for p in relative], 'speed': speeds(relative, times)}


def plant_intervals(times, flags):
    result, start = [], None
    for i, flag in enumerate(flags):
        if flag and start is None:
            start = times[i]
        if start is not None and (not flag or i == len(flags) - 1):
            result.append([start, times[i]])
            start = None
    return result


def detect_plants(points, times, settings):
    """The same unmodified detector serves RM and reconstructed IPC paths."""
    velocity = speeds(points, times)
    floor = min(point[2] for point in points)
    heights = [point[2] - floor for point in points]
    flags = [speed <= settings['maximum_plant_speed_cm_s'] and height <= settings['floor_tolerance_cm']
             for speed, height in zip(velocity, heights)]
    for start, end in plant_intervals(times, flags):
        if start > 0 and end < times[-1] and end - start < settings['minimum_plant_seconds']:
            flags = [False if start <= time < end else flag for time, flag in zip(times, flags)]
    return {'floor_bone_height_cm': floor, 'speed_cm_s': velocity, 'height_above_floor_cm': heights,
            'flags': flags, 'plant_intervals': plant_intervals(times, flags)}


def data_model_timing(seq):
    """Read the actual RAW evaluation grid, never cached platform sampled keys.

    AnimSequenceBase.DataModelInterface is a reflected VisibleAnywhere property;
    IAnimationDataModel.GetFrameRate/GetNumberOfFrames/GetNumberOfKeys/GetPlayLength
    are UFUNCTIONs in the local engine. If Python cannot expose the interface,
    the alternative gate remains pending rather than guessing from clip length.
    """
    try:
        model = seq.get_editor_property('data_model_interface')
        ensure(model is not None, 'DataModelInterface is null')
        rate = model.get_frame_rate()
        numerator = int(rate.get_editor_property('numerator'))
        denominator = int(rate.get_editor_property('denominator'))
        ensure(numerator > 0 and denominator > 0, 'Invalid data-model frame rate')
        return {'numerator': numerator, 'denominator': denominator,
                'sample_interval_seconds': denominator / numerator,
                'frames': int(model.get_number_of_frames()), 'keys': int(model.get_number_of_keys()),
                'length_seconds': float(model.get_play_length())}
    except Exception as error:
        raise RuntimeError('Contact equivalence pending actual data-model timing API: '
                           + package(seq) + ': ' + str(error)) from error


def percentile95(values):
    ordered = sorted(values)
    return ordered[max(0, int(math.ceil(0.95 * len(ordered))) - 1)]


def contact_equivalence(record, rm, ipc, times, source_rm, source_ipc, settings):
    diagnostic = {'passed': False, 'time_shift_applied_seconds': 0.0, 'times': times,
                  'leg_errors': {}, 'source_timing': {}, 'feet': {}}
    record['timeline']['contact_equivalence'] = diagnostic
    # Retain the breached peak and its full time series; never widen the 2-degree gate.
    for bone in LEG_BONES:
        errors = [quat_angle(a, b) for a, b in zip(rm['angles'][bone], ipc['angles'][bone])]
        diagnostic['leg_errors'][bone] = {'degrees': errors, 'p95_degrees': percentile95(errors),
                                           'mean_degrees': sum(errors) / len(errors), 'max_degrees': max(errors)}
    for key, seq in (('rm', source_rm), ('ipc', source_ipc)):
        diagnostic['source_timing'][key] = data_model_timing(seq)
    interval = min(value['sample_interval_seconds'] for value in diagnostic['source_timing'].values())
    boundary_tolerance = interval * 0.5
    diagnostic['boundary_tolerance_seconds'] = boundary_tolerance
    step_time = times[1] - times[0]
    ensure(step_time <= boundary_tolerance + 1e-9,
           'Contact diagnostic sampling is too coarse for half a source interval')
    radius = max(1, int(math.ceil(interval / step_time)))
    indices = range(radius, len(times) - 1 - radius)
    ensure(len(indices) >= 2, 'Not enough samples for zero-lag comparison')
    lag_costs = []
    for lag in range(-radius, radius + 1):
        errors = [quat_angle(rm['angles'][bone][i], ipc['angles'][bone][i + lag])
                  for bone in LEG_BONES for i in indices]
        lag_costs.append({'lag_steps': lag, 'lag_seconds': lag * step_time,
                          'rms_degrees': math.sqrt(sum(error * error for error in errors) / len(errors))})
    best = min(lag_costs, key=lambda value: (round(value['rms_degrees'], 6), abs(value['lag_steps'])))
    zero = next(value for value in lag_costs if value['lag_steps'] == 0)
    diagnostic['zero_lag'] = {'rms_degrees': zero['rms_degrees'], 'best_lag_seconds': best['lag_seconds'],
                               'is_best': best['lag_steps'] == 0, 'comparison_costs': lag_costs}
    failures = []
    if best['lag_steps'] != 0:
        failures.append('A shifted leg phase matches better than zero lag')
    for bone in ('foot_l', 'foot_r'):
        reconstructed = []
        for rm_root, ipc_root, foot in zip(rm['root_transforms'], ipc['root_transforms'], ipc[bone]):
            # RMRoot(t) * inverse(IPCRoot(t)) * IPCFoot(t), including rotation/scale.
            local = ipc_root.inverse_transform_location(unreal.Vector(*foot))
            point = rm_root.transform_location(local)
            reconstructed.append([float(point.x), float(point.y), float(point.z)])
        rm_detection = detect_plants(rm[bone], times, settings)
        ipc_detection = detect_plants(reconstructed, times, settings)
        a, b = rm_detection['flags'], ipc_detection['flags']
        agreement = sum(x == y for x, y in zip(a[:-1], b[:-1])) / (len(times) - 1)
        rm_intervals, ipc_intervals = rm_detection['plant_intervals'], ipc_detection['plant_intervals']
        matching_count = len(rm_intervals) == len(ipc_intervals) and bool(rm_intervals)
        deltas = ([[abs(x - y) for x, y in zip(a_range, b_range)]
                   for a_range, b_range in zip(rm_intervals, ipc_intervals)] if matching_count else [])
        maximum_boundary_delta = max((delta for pair in deltas for delta in pair), default=None)
        passed = (agreement >= 0.95 and matching_count and maximum_boundary_delta <= boundary_tolerance + 1e-9)
        diagnostic['feet'][bone] = {
            'passed': passed, 'state_agreement': agreement,
            'rm_plant_intervals': rm_intervals, 'reconstructed_ipc_plant_intervals': ipc_intervals,
            'corresponding_boundary_deltas_seconds': deltas,
            'maximum_boundary_delta_seconds': maximum_boundary_delta,
            'rm_flags': a, 'reconstructed_ipc_flags': b,
            'reconstructed_ipc_speed_cm_s': ipc_detection['speed_cm_s'],
            'reconstructed_ipc_height_above_floor_cm': ipc_detection['height_above_floor_cm']}
        if not passed:
            failures.append(bone + ' contact timing is not equivalent')
    diagnostic['failures'] = failures
    diagnostic['passed'] = not failures
    ensure(not failures, 'Contact equivalence failed: ' + '; '.join(failures))


def measure(record, target, source_rm, source_ipc, source_mesh, target_mesh, settings):
    original = record['original']
    for other in (record['source_rm_original'], record['source_ipc_original']):
        ensure(abs(other['length'] - original['length']) <= settings['length_tolerance_seconds'],
               'Timeline length mismatch: ' + other['path'])
        ensure(other['frames'] >= 2, 'Insufficient source frame count: ' + other['path'])
    length = original['length']
    ensure(length > 0 and original['frames'] >= 2, 'Insufficient animation duration/frames')
    count = max(3, int(math.ceil(length * settings['sample_rate_hz'])))
    times = [length * i / count for i in range(count + 1)]
    rm = sample(source_rm, source_mesh, times, True)
    ipc = sample(source_ipc, source_mesh, times, True)
    dst = sample(target, target_mesh, times, False)
    record['timeline'] = {'length_seconds': length, 'frames': original['frames'],
                          'frame_counts': {'target': original['frames'],
                                           'source_rm': record['source_rm_original']['frames'],
                                           'source_ipc': record['source_ipc_original']['frames']},
                          'frame_counts_match': len({original['frames'], record['source_rm_original']['frames'],
                                                     record['source_ipc_original']['frames']}) == 1,
                          # Live Walk_F: equal 1.2000000477 s duration, target/IPC=36
                          # frames but RM=35. Frame counts are diagnostic; the dense
                          # common-time pose comparisons below establish alignment.
                          'sample_count': len(times), 'time_step_seconds': length / count,
                          'source_rm_vs_ipc_leg_angles': {}}
    for bone in LEG_BONES:
        angles = [quat_angle(a, b) for a, b in zip(rm['angles'][bone], ipc['angles'][bone])]
        record['timeline']['source_rm_vs_ipc_leg_angles'][bone] = {
            'max_degrees': max(angles), 'mean_degrees': sum(angles) / len(angles)}
    maximum = max(info['max_degrees'] for info in record['timeline']['source_rm_vs_ipc_leg_angles'].values())
    strict_alignment = maximum <= settings['source_leg_angle_tolerance_degrees']
    record['timeline']['strict_angular_gate_passed'] = strict_alignment
    record['root_motion'] = {key: root_motion(data, times) for key, data in (('source_rm', rm), ('source_ipc', ipc), ('target', dst))}
    ensure(record['root_motion']['source_rm']['net_planar_speed_cm_s'] >= settings['minimum_rm_root_speed_cm_s'],
           'Matching RM source has no sufficient baked root translation; source name is not proof')
    ensure(record['root_motion']['source_ipc']['mean_path_speed_cm_s'] <= settings['maximum_ipc_root_speed_cm_s'],
           'Matching source IPC has unexpected root motion')
    ensure(record['root_motion']['target']['mean_path_speed_cm_s'] <= settings['maximum_ipc_root_speed_cm_s'],
           'Existing P01 target has unexpected root motion')
    if not strict_alignment:
        contact_equivalence(record, rm, ipc, times, source_rm, source_ipc, settings)
    record['target_phase'] = {}
    record['contacts'] = {}
    for bone, curve in (('foot_l', 'contact_l'), ('foot_r', 'contact_r')):
        src_patterns = foot_patterns(ipc, times, bone)
        dst_patterns = foot_patterns(dst, times, bone)
        comparisons = {kind: pattern_comparison(src_patterns[kind], dst_patterns[kind]) for kind in ('height', 'speed')}
        record['target_phase'][bone] = comparisons
        for kind, info in comparisons.items():
            ensure(info['correlation'] >= settings['minimum_pattern_correlation']
                   and info['mean_absolute_error'] <= settings['maximum_normalized_pattern_error'],
                   'Target/source IPC %s %s pattern mismatch: correlation=%.3f error=%.3f'
                   % (bone, kind, info['correlation'], info['mean_absolute_error']))
        detection = detect_plants(rm[bone], times, settings)
        rm_speed, floor = detection['speed_cm_s'], detection['floor_bone_height_cm']
        heights, flags = detection['height_above_floor_cm'], detection['flags']
        plant_indices = [i for i, flag in enumerate(flags[:-1]) if flag]
        contact = {'floor_bone_height_cm': floor, 'source_rm_speed_cm_s': rm_speed,
                   'source_rm_height_above_floor_cm': heights,
                   'plant_intervals': plant_intervals(times, flags),
                   'plant_fraction': len(plant_indices) / count,
                   'keys': {'times': times, 'values': [1.0 if f else 0.0 for f in flags]},
                   'endpoint_values_match': flags[0] == flags[-1]}
        record['contacts'][curve] = contact
        ensure(plant_indices, 'No source RM plants detected for ' + bone)
        ensure(contact['plant_fraction'] <= settings['maximum_plant_fraction'], 'Implausibly continuous plant for ' + bone)
        ensure(flags[0] == flags[-1], 'Contact endpoints disagree at loop seam for ' + bone)
        normalized_target_height = comparisons['height']['target_normalized']
        contact['target_height_during_source_plants'] = sum(normalized_target_height[i] for i in plant_indices) / len(plant_indices)
        ensure(contact['target_height_during_source_plants'] <= settings['maximum_target_plant_height'],
               'Transferred source plants occur too high in the target foot cycle: ' + bone)
    record['timeline']['verified'] = True
    record['timeline']['validation_method'] = ('strict_angular_alignment' if strict_alignment
                                                else 'validated_by_contact_equivalence')
    record['status'] = 'validated_contact_candidate' if strict_alignment else 'validated_by_contact_equivalence'


def backup_target(target_path, backup_root):
    source_file = (PROJECT / 'Content' / (target_path.removeprefix('/Game/') + '.uasset')).resolve()
    allowed = (PROJECT / 'Content/AZ/Assets/RTG/Riffle_P01').resolve()
    ensure(source_file.is_relative_to(allowed), 'Backup source outside the authorized P01 folder')
    ensure(source_file.is_file(), 'Target package must exist on disk before authoring: ' + str(source_file))
    result = []
    for extension in ('.uasset', '.uexp', '.ubulk'):
        source = source_file.with_suffix(extension)
        if source.exists():
            destination = backup_root / source.relative_to(PROJECT)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
            source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
            ensure(hashlib.sha256(destination.read_bytes()).hexdigest() == source_hash, 'Backup hash mismatch: ' + str(destination))
            result.append({'path': str(destination), 'sha256': source_hash})
    return result


def same_keys(a, b):
    return (a is not None and len(a['times']) == len(b['times'])
            and all(abs(x - y) < 1e-5 for x, y in zip(a['times'], b['times']))
            and all(abs(x - y) < 1e-5 for x, y in zip(a['values'], b['values'])))


def author(record, target, settings, backup_root):
    original = snapshot(target)
    ensure(original == record['original'], 'Target changed since its audit; rerun audit before writing')
    changes = []
    for name, contact in record['contacts'].items():
        prior = original['curves'].get(name)
        owned = EAL.get_metadata_tag(target, OWNER_KEY + '.' + name) == OWNER
        if prior is not None and not owned:
            contact['action'] = 'preserved_preexisting_unmanaged_curve'
        elif same_keys(prior, contact['keys']):
            contact['action'] = 'already_matches'
        else:
            changes.append(name)
    if not changes:
        record['status'] = 'preserved_or_already_prepared'
        return
    record['backups'] = backup_target(record['target'], backup_root)
    metadata_keys = [OWNER_KEY + '.' + name for name in changes] + [
        'AZ.RifleP01Contacts.SourceRM', 'AZ.RifleP01Contacts.Settings']
    prior_metadata = {key: EAL.get_metadata_tag(target, key) for key in metadata_keys}
    try:
        for name in changes:
            if name in original['curves']:
                AL.remove_curve(target, name)
            AL.add_curve(target, name, unreal.RawCurveTrackTypes.RCT_FLOAT)
            keys = record['contacts'][name]['keys']
            AL.add_float_curve_keys(target, name, keys['times'], keys['values'])
        after = snapshot(target)
        for name in changes:
            ensure(same_keys(after['curves'].get(name), record['contacts'][name]['keys']), 'Authored contact readback mismatch: ' + name)
        before_other = {k: v for k, v in original['curves'].items() if k not in changes}
        after_other = {k: v for k, v in after['curves'].items() if k not in changes}
        ensure(before_other == after_other, 'An unrelated curve changed')
        ensure({k: v for k, v in original.items() if k != 'curves'} == {k: v for k, v in after.items() if k != 'curves'},
               'Sequence flags or timing changed')
        for name in changes:
            EAL.set_metadata_tag(target, OWNER_KEY + '.' + name, OWNER)
        EAL.set_metadata_tag(target, 'AZ.RifleP01Contacts.SourceRM', record['source_rm'])
        EAL.set_metadata_tag(target, 'AZ.RifleP01Contacts.Settings', json.dumps(settings, sort_keys=True))
        ensure(EAL.save_loaded_asset(target, only_if_is_dirty=False), 'Target save failed')
        for name in changes:
            record['contacts'][name]['action'] = 'authored_from_verified_matching_rm_timeline'
        record['status'] = 'prepared_contact_candidate'
    except Exception:
        # Restore just this script's in-memory curve edits; disk backup is retained.
        for name in changes:
            if AL.does_curve_exist(target, name, unreal.RawCurveTrackTypes.RCT_FLOAT):
                AL.remove_curve(target, name)
            if name in original['curves']:
                AL.add_curve(target, name, unreal.RawCurveTrackTypes.RCT_FLOAT)
                keys = original['curves'][name]
                AL.add_float_curve_keys(target, name, keys['times'], keys['values'])
        for key, value in prior_metadata.items():
            EAL.set_metadata_tag(target, key, value)
        raise


def main(target_paths=None, prepare=False, include_run_r=False, settings=None,
         receipt_path=None, inventory_path=str(INVENTORY)):
    """Return per-asset audit/authoring results; failed assets are explicitly skipped.

    Default target list is the inventory's 48 Walk/Jog/CrouchWalk loop packages.
    Explicit target_paths can select a smaller subset; they must be in that audit.
    prepare=True still skips every asset whose timeline/contact gates do not pass.
    Loop flags/MM membership/aim bases remain separate preparation operations.
    """
    result = {'mode': 'prepare' if prepare else 'audit', 'status': 'running', 'records': [],
              'settings': {}, 'warnings': ['Contacts are content measurements; runtime visual validation remains separate.']}
    try:
        options = dict(DEFAULTS)
        for key, value in (settings or {}).items():
            ensure(key in options, 'Unknown setting: ' + key)
            options[key] = value
        for key, value in options.items():
            ensure(isinstance(value, (int, float)) and math.isfinite(value) and value > 0, 'Setting must be finite and positive: ' + key)
        ensure(options['sample_rate_hz'] <= 240, 'Sampling is bounded to 240 Hz')
        ensure(options['minimum_pattern_correlation'] <= 1 and options['maximum_plant_fraction'] < 1,
               'Correlation and plant fraction settings must fit their unit ranges')
        result['settings'] = options
        inventory = json.loads(Path(inventory_path).read_text(encoding='utf-8'))
        allowed = {row['path'] for row in inventory if row['path'].endswith('_Loop_IPC')}
        default_paths = sorted(path for path in allowed if include_run_r or '/Run/' not in path)
        paths = default_paths if target_paths is None else list(target_paths)
        ensure(0 < len(paths) <= 49 and len(paths) == len(set(paths)), 'Provide 1-49 distinct audited loop paths')
        ensure(all(path in allowed and path.startswith(TARGET_ROOT + '/') and '..' not in path for path in paths),
               'Every target must be an audited existing P01 loop')
        ensure(include_run_r or not any('/Run/' in path for path in paths), 'Run_R requires include_run_r=True')
        source_mesh = load(SOURCE_MESH, unreal.SkeletalMesh)
        target_mesh = load(TARGET_MESH, unreal.SkeletalMesh)
        backup_root = PROJECT / 'Saved/Backups/RifleP01Contacts' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
        for target_path in paths:
            leaf = target_path.rsplit('/', 1)[1]
            ensure(leaf.startswith('Riffle_P_W2_'), 'Unexpected P01 source naming: ' + leaf)
            source_name = leaf.removeprefix('Riffle_P_')
            rm_path = RM_ROOT + '/' + source_name.removesuffix('_IPC')
            ipc_path = IPC_ROOT + '/' + source_name
            record = {'target': target_path, 'source_rm': rm_path, 'source_ipc': ipc_path, 'status': 'running'}
            result['records'].append(record)
            try:
                target = load(target_path, unreal.AnimSequence)
                source_rm = load(rm_path, unreal.AnimSequence)
                source_ipc = load(ipc_path, unreal.AnimSequence)
                ensure(target.get_editor_property('skeleton') == target_mesh.get_editor_property('skeleton'), 'P01 target skeleton mismatch')
                for source in (source_rm, source_ipc):
                    ensure(source.get_editor_property('skeleton') == source_mesh.get_editor_property('skeleton'), 'Rifle_01 source skeleton mismatch')
                record.update({'original': snapshot(target), 'source_rm_original': snapshot(source_rm),
                               'source_ipc_original': snapshot(source_ipc)})
                measure(record, target, source_rm, source_ipc, source_mesh, target_mesh, options)
                if prepare:
                    author(record, target, options, backup_root)
                ensure(snapshot(source_rm) == record['source_rm_original'] and snapshot(source_ipc) == record['source_ipc_original'],
                       'Source metadata changed during operation')
            except Exception as error:
                record['status'] = 'failed_skipped'
                record['error'] = str(error)
            print('[RifleP01Contacts] ' + target_path + ' ' + record['status'] + (' ' + record['error'] if 'error' in record else ''))
        result['failed_count'] = sum(record['status'] == 'failed_skipped' for record in result['records'])
        result['status'] = 'completed_with_skips' if result['failed_count'] else ('prepared_candidates' if prepare else 'audit_complete')
        return result
    except Exception as error:
        result['status'] = 'failed'
        result['error'] = str(error)
        raise
    finally:
        if receipt_path:
            destination = Path(receipt_path)
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_text(json.dumps(result, indent=2), encoding='utf-8')
        print('[RifleP01Contacts] ' + json.dumps({'status': result['status'], 'count': len(result['records']),
                                                'failed_count': result.get('failed_count'), 'receipt_path': receipt_path}))
        gc.collect()
