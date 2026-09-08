# @Description: Read-only periodic foot-contact diagnostics for the four P01 exceptions.
"""No asset writes. Root executes main() in the editor and inspects the receipt.

One comparison trace borrows the matching source RM trajectory at unit scale.
An independent target trace evaluates its OWN Speed curve and integrates that
speed along the matching RM travel direction. Both remain read-only evidence,
not automatic contact approval. Cyclic derivatives transport samples through the
root's complete end/start transform, including rotation and scale.
"""
import gc
import json
import math
from pathlib import Path
import runpy

import unreal


HELPERS_PATH = 'C:/UnrealEngine/Games/AZ/Tools/rifle_p01_contacts.py'
ROOT = '/Game/AZ/Assets/RTG/Riffle_P01'
FOUR = [
    ROOT + '/Jog/Aim/IPC/Riffle_P_W2_Jog_Aim_FL_Loop_IPC',
    ROOT + '/Jog/Aim/IPC/Riffle_P_W2_Jog_Aim_R_Loop_IPC',
    ROOT + '/Jog/Locomotion/IPC/Riffle_P_W2_Jog_B_Loop_IPC',
    ROOT + '/Jog/Locomotion/IPC/Riffle_P_W2_Jog_FL_Loop_IPC',
]


def xyz(vector):
    return [float(vector.x), float(vector.y), float(vector.z)]


def periodic_speeds(points, times, root_transforms, speed_reader):
    result = speed_reader(points, times)
    first_root, final_root = root_transforms[0], root_transforms[-1]
    # The preceding cycle's penultimate sample: T(0) * inverse(T(L)) * p(L-dt).
    previous = xyz(first_root.transform_location(
        final_root.inverse_transform_location(unreal.Vector(*points[-2]))))
    # The following cycle's second sample: T(L) * inverse(T(0)) * p(dt).
    following = xyz(final_root.transform_location(
        first_root.inverse_transform_location(unreal.Vector(*points[1]))))
    dt = times[1] - times[0]
    result[0] = math.dist(points[1], previous) / (2.0 * dt)
    result[-1] = math.dist(following, points[-2]) / (2.0 * dt)
    return result


def detection(points, times, velocity, settings, interval_reader):
    floor = min(p[2] for p in points)
    heights = [p[2] - floor for p in points]
    flags = [v <= settings['maximum_plant_speed_cm_s'] and h <= settings['floor_tolerance_cm']
             for v, h in zip(velocity, heights)]
    for start, end in interval_reader(times, flags):
        if start > 0 and end < times[-1] and end - start < settings['minimum_plant_seconds']:
            flags = [False if start <= t < end else value for t, value in zip(times, flags)]
    return {'speed_cm_s': velocity, 'height_above_floor_cm': heights, 'flags': flags,
            'plant_intervals': interval_reader(times, flags), 'floor_bone_height_cm': floor,
            'endpoint_values_match': flags[0] == flags[-1]}


def summarize(trace):
    return {'plant_intervals': trace['plant_intervals'],
            'endpoint_speeds': [trace['speed_cm_s'][0], trace['speed_cm_s'][-1]],
            'endpoint_values_match': trace['endpoint_values_match']}


def evaluated_target_speed(target, mesh, times, ensure):
    options = unreal.AnimPoseEvaluationOptions()
    for name, value in {'evaluation_type': unreal.AnimDataEvalType.RAW,
                        'should_retarget': False, 'extract_root_motion': False,
                        'incorporate_root_motion_into_pose': True,
                        'optional_skeletal_mesh': mesh, 'evaluate_curves': True}.items():
        options.set_editor_property(name, value)
    values = []
    for time in times:
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(target, time, options)
        ensure(unreal.AnimPoseExtensions.is_valid(pose), 'Invalid target pose while evaluating Speed')
        names = {str(name).casefold() for name in unreal.AnimPoseExtensions.get_curve_names(pose)}
        ensure('speed' in names, 'Target has no evaluated Speed curve; no travel estimate will be guessed')
        value = float(unreal.AnimPoseExtensions.get_curve_weight(pose, 'Speed'))
        ensure(math.isfinite(value) and value > 0, 'Target Speed must be finite and positive for these moving loops')
        values.append(value)
    return values


def vector_velocities(points, times):
    result = []
    for i in range(len(times)):
        before, after = max(0, i - 1), min(len(times) - 1, i + 1)
        elapsed = times[after] - times[before]
        result.append([(points[after][j] - points[before][j]) / elapsed for j in range(3)])
    return result


def integrate_target_travel(target, mesh, rm, dst, times, ensure):
    speed_values = evaluated_target_speed(target, mesh, times, ensure)
    source_velocity = vector_velocities(rm['root'], times)
    source_first, target_first = rm['root_transforms'][0], dst['root_transforms'][0]
    directions, source_speed = [], []
    for velocity in source_velocity:
        source_speed.append(math.hypot(velocity[0], velocity[1]))
        ensure(source_speed[-1] > 1e-5, 'Matching RM direction is undefined at a sampled time')
        # Map the source travel vector into the target root's initial coordinate
        # frame. Direction only comes from RM; magnitude comes from target Speed.
        local = source_first.inverse_transform_direction(unreal.Vector(velocity[0], velocity[1], 0.0))
        world = target_first.transform_direction(local)
        magnitude = math.hypot(float(world.x), float(world.y))
        ensure(magnitude > 1e-5, 'Source travel direction could not be mapped into target root space')
        directions.append([float(world.x) / magnitude, float(world.y) / magnitude, 0.0])
    desired_velocity = [[value * component for component in direction]
                        for value, direction in zip(speed_values, directions)]
    travel = [[0.0, 0.0, 0.0]]
    for i in range(1, len(times)):
        elapsed = times[i] - times[i - 1]
        travel.append([travel[-1][j] + 0.5 * (desired_velocity[i - 1][j] + desired_velocity[i][j]) * elapsed
                       for j in range(3)])
    virtual_roots = []
    for root, translation in zip(dst['root_transforms'], travel):
        position = xyz(root.translation)
        virtual_roots.append(unreal.Transform(
            location=unreal.Vector(*[position[j] + translation[j] for j in range(3)]),
            rotation=root.rotation.rotator(), scale=root.scale3d))
    mean_speed = sum(0.5 * (a + b) * (t1 - t0)
                     for a, b, t0, t1 in zip(speed_values, speed_values[1:], times, times[1:])) / times[-1]
    source_net_speed = math.hypot(rm['root'][-1][0] - rm['root'][0][0],
                                  rm['root'][-1][1] - rm['root'][0][1]) / times[-1]
    return {'speed_values': speed_values, 'directions': directions, 'travel': travel,
            'virtual_roots': virtual_roots, 'source_speed_values': source_speed,
            'receipt': {'basis': 'own evaluated P01 Speed curve + matching RM direction; trapezoidal integration',
                        'target_speed_cm_s': speed_values, 'source_root_speed_cm_s': source_speed,
                        'target_mean_speed_cm_s': mean_speed, 'source_net_speed_cm_s': source_net_speed,
                        'mean_speed_difference_cm_s': mean_speed - source_net_speed,
                        'mean_speed_relative_difference': (mean_speed - source_net_speed) / source_net_speed,
                        'direction_in_target_space': directions, 'integrated_translation_cm': travel,
                        'cycle_translation_cm': travel[-1]}}


def target_measured_contacts(dst, bone, reconstruction, times, settings, helpers):
    points = [[p[j] + travel[j] for j in range(3)]
              for p, travel in zip(dst[bone], reconstruction['travel'])]
    velocity = periodic_speeds(points, times, reconstruction['virtual_roots'], helpers['speeds'])
    detected = detection(points, times, velocity, settings, helpers['plant_intervals'])
    inplace_velocity = vector_velocities(dst[bone], times)
    along = [sum(v * d for v, d in zip(foot_velocity, direction))
             for foot_velocity, direction in zip(inplace_velocity, reconstruction['directions'])]
    opposition = [-a / speed for a, speed in zip(along, reconstruction['speed_values'])]
    indices = [i for i, flag in enumerate(detected['flags'][:-1]) if flag]
    direct_root_relative = [xyz(root.inverse_transform_location(unreal.Vector(*point)))
                            for root, point in zip(dst['root_transforms'], dst[bone])]
    closure = math.dist(direct_root_relative[0], direct_root_relative[-1])
    intervals = detected['plant_intervals']
    # Describe a plant crossing the cycle boundary as one core contact, instead
    # of treating its two visible pieces as two separate footfalls.
    core_intervals = [{'start': a, 'end': b, 'wraps': False} for a, b in intervals if b > a]
    if (len(core_intervals) >= 2 and core_intervals[0]['start'] == 0.0
            and core_intervals[-1]['end'] == times[-1]):
        merged = {'start': core_intervals[-1]['start'], 'end': core_intervals[0]['end'], 'wraps': True}
        core_intervals = [merged] + core_intervals[1:-1]
    concerns = []
    if not indices:
        concerns.append('No low-speed low-height target plant was detected')
    if len(indices) / (len(times) - 1) > settings['maximum_plant_fraction']:
        concerns.append('Target is planted for an implausibly large fraction of the cycle')
    if not detected['endpoint_values_match']:
        concerns.append('Periodic target contact values disagree at the cycle boundary')
    return {'status': 'target_measured_candidate_review_required' if not concerns else 'unresolved',
            'basis': 'target foot poses + target Speed travel; original 45 cm/s and 3 cm detector unchanged',
            'concerns': concerns, 'root_relative_pose_closure_error_cm': closure,
            'detection': detected, 'core_plant_intervals': core_intervals,
            'candidate_keys': {'times': times, 'values': [1.0 if flag else 0.0 for flag in detected['flags']]},
            'inplace_along_travel_velocity_cm_s': along, 'backward_foot_speed_over_travel_speed': opposition,
            'core_evidence': {'sample_count': len(indices),
                              'mean_virtual_speed_cm_s': sum(velocity[i] for i in indices) / len(indices) if indices else None,
                              'max_height_above_floor_cm': max((detected['height_above_floor_cm'][i] for i in indices), default=None),
                              'mean_backward_speed_ratio': sum(opposition[i] for i in indices) / len(indices) if indices else None}}


def main(target_paths=None, sample_rate_hz=240.0, receipt_path=None):
    """Read and compare only; never author contacts or adjust acceptance thresholds."""
    helpers = runpy.run_path(HELPERS_PATH)
    result = {'status': 'running', 'mode': 'read_only_diagnostic', 'records': [],
              'target_virtual_root_scale': 1.0,
              'warning': 'Target-Speed contact candidates remain read-only measurements for review; no automatic contact approval.'}
    try:
        ensure = helpers['ensure']
        ensure(math.isfinite(sample_rate_hz) and 120 <= sample_rate_hz <= 480, 'Use a finite sample rate in [120,480] Hz')
        paths = list(FOUR if target_paths is None else target_paths)
        ensure(paths and len(paths) <= 4 and len(paths) == len(set(paths)) and all(p in FOUR for p in paths),
               'Select a subset of the four known contact exceptions')
        settings = dict(helpers['DEFAULTS'])
        settings['sample_rate_hz'] = float(sample_rate_hz)
        result['settings'] = settings
        source_mesh = helpers['load'](helpers['SOURCE_MESH'], unreal.SkeletalMesh)
        target_mesh = helpers['load'](helpers['TARGET_MESH'], unreal.SkeletalMesh)
        for target_path in paths:
            record = {'target': target_path, 'status': 'running', 'feet': {}}
            result['records'].append(record)
            try:
                target = helpers['load'](target_path, unreal.AnimSequence)
                leaf = target_path.rsplit('/', 1)[1].removeprefix('Riffle_P_')
                rm_path = helpers['RM_ROOT'] + '/' + leaf.removesuffix('_IPC')
                ipc_path = helpers['IPC_ROOT'] + '/' + leaf
                rm_asset = helpers['load'](rm_path, unreal.AnimSequence)
                ipc_asset = helpers['load'](ipc_path, unreal.AnimSequence)
                record.update({'source_rm': rm_path, 'source_ipc': ipc_path,
                               'timing': {'rm': helpers['data_model_timing'](rm_asset),
                                          'ipc': helpers['data_model_timing'](ipc_asset),
                                          'target': helpers['data_model_timing'](target)}})
                length = float(unreal.AnimationLibrary.get_sequence_length(target))
                ensure(all(abs(float(unreal.AnimationLibrary.get_sequence_length(seq)) - length)
                           <= settings['length_tolerance_seconds'] for seq in (rm_asset, ipc_asset)),
                       'Diagnostic timelines differ')
                count = max(3, int(math.ceil(length * sample_rate_hz)))
                times = [length * i / count for i in range(count + 1)]
                record['times'] = times
                rm = helpers['sample'](rm_asset, source_mesh, times, False)
                ipc = helpers['sample'](ipc_asset, source_mesh, times, False)
                dst = helpers['sample'](target, target_mesh, times, False)
                reconstruction = integrate_target_travel(target, target_mesh, rm, dst, times, ensure)
                record['own_speed_reconstruction'] = reconstruction['receipt']
                record['target_measured_contacts'] = {}
                for bone in ('foot_l', 'foot_r'):
                    record['target_measured_contacts'][bone] = target_measured_contacts(
                        dst, bone, reconstruction, times, settings, helpers)
                    reconstructed_ipc, reconstructed_target = [], []
                    relative_ipc, relative_target = [], []
                    for i in range(len(times)):
                        ipc_local = ipc['root_transforms'][i].inverse_transform_location(unreal.Vector(*ipc[bone][i]))
                        dst_local = dst['root_transforms'][i].inverse_transform_location(unreal.Vector(*dst[bone][i]))
                        relative_ipc.append(xyz(ipc_local))
                        relative_target.append(xyz(dst_local))
                        reconstructed_ipc.append(xyz(rm['root_transforms'][i].transform_location(ipc_local)))
                        reconstructed_target.append(xyz(rm['root_transforms'][i].transform_location(dst_local)))
                    traces = {}
                    for name, points in (('source_rm', rm[bone]), ('reconstructed_ipc', reconstructed_ipc),
                                         ('target_on_source_root_unit_scale', reconstructed_target)):
                        raw_velocity = helpers['speeds'](points, times)
                        cyclic_velocity = periodic_speeds(points, times, rm['root_transforms'], helpers['speeds'])
                        local_first = rm['root_transforms'][0].inverse_transform_location(unreal.Vector(*points[0]))
                        local_last = rm['root_transforms'][-1].inverse_transform_location(unreal.Vector(*points[-1]))
                        traces[name] = {
                            'root_relative_pose_closure_error_cm': math.dist(xyz(local_first), xyz(local_last)),
                            'one_sided': detection(points, times, raw_velocity, settings, helpers['plant_intervals']),
                            'periodic': detection(points, times, cyclic_velocity, settings, helpers['plant_intervals'])}
                    source_patterns = {'height': [p[2] for p in relative_ipc],
                                       'speed': helpers['speeds'](relative_ipc, times)}
                    target_patterns = {'height': [p[2] for p in relative_target],
                                       'speed': helpers['speeds'](relative_target, times)}
                    comparison = {kind: helpers['pattern_comparison'](source_patterns[kind], target_patterns[kind])
                                  for kind in ('height', 'speed')}
                    # Both ends and the disputed late-plant region, sampled directly
                    # on the target. Heights are bone heights relative to its own root;
                    # target-relative speeds are not mislabeled as world plant speed.
                    windows = []
                    for i, time in enumerate(times):
                        if time <= 0.085 or time >= length - 0.125:
                            windows.append({'time': time,
                                            'target_root_relative_foot_cm': relative_target[i],
                                            'target_root_relative_speed_cm_s': target_patterns['speed'][i],
                                            'target_normalized_height': comparison['height']['target_normalized'][i],
                                            'rm_periodic_speed_cm_s': traces['source_rm']['periodic']['speed_cm_s'][i],
                                            'ipc_periodic_speed_cm_s': traces['reconstructed_ipc']['periodic']['speed_cm_s'][i],
                                            'target_unit_scale_periodic_speed_cm_s': traces['target_on_source_root_unit_scale']['periodic']['speed_cm_s'][i]})
                            windows[-1]['own_speed_target_periodic_speed_cm_s'] = record['target_measured_contacts'][bone]['detection']['speed_cm_s'][i]
                            windows[-1]['own_speed_target_contact'] = record['target_measured_contacts'][bone]['detection']['flags'][i]
                    record['feet'][bone] = {'traces': traces, 'target_phase_patterns': comparison,
                                            'target_relative_endpoint_error_cm': math.dist(relative_target[0], relative_target[-1]),
                                            'seam_and_late_plant_samples': windows}
                record['status'] = 'measured_not_authored'
                print('[P01ContactDiagnostic] ' + json.dumps({'target': target_path,
                    'own_speed_mean_cm_s': record['own_speed_reconstruction']['target_mean_speed_cm_s'],
                    'source_mean_cm_s': record['own_speed_reconstruction']['source_net_speed_cm_s'],
                    'target_contact_candidates': {bone: {'status': info['status'],
                                                        'core_plant_intervals': info['core_plant_intervals'],
                                                        'core_evidence': info['core_evidence']}
                                                  for bone, info in record['target_measured_contacts'].items()},
                    'feet': {bone: {'closure_cm': info['target_relative_endpoint_error_cm'],
                                   'traces': {name: summarize(trace['periodic']) for name, trace in info['traces'].items()}}
                             for bone, info in record['feet'].items()}}, sort_keys=True))
            except Exception as error:
                record['status'] = 'failed'
                record['error'] = str(error)
                print('[P01ContactDiagnostic] ' + target_path + ' failed: ' + str(error))
        result['status'] = 'completed_with_failures' if any(r['status'] == 'failed' for r in result['records']) else 'diagnostic_complete'
        return result
    finally:
        if receipt_path:
            destination = Path(receipt_path)
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_text(json.dumps(result, indent=2), encoding='utf-8')
        gc.collect()
