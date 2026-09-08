# @Description: Measure or align the existing relaxed/aim rifle sockets while preserving each right-grip anchor.
"""main() audits without writing assets. main(prepare=True) applies reviewed math.

Two existing mesh-owned sockets keep their names, parent bones and unit scale.
Each mode uses five genuine P01 clips at normalized times 0/.25/.5/.75. A minimal
quaternion rotation aligns its fixed weapon grip span toward the mean desired
left-wrist vector in that socket parent's local space; translation compensates
the rotation so the current right-grip anchor is unchanged. No roll tuning,
weapon scaling, IK, mesh-component transforms, animation edits, previews or PIE.

The rifle WeaponMesh3P CDO relative transform must be identity. Weapon left-grip
positions include their reference bone transform (including hand_r's existing
-90-degree X rotation), not just the grip socket's raw relative location.
"""

import gc
import hashlib
import json
import math
import runpy
from pathlib import Path

import unreal


WEAPON_BP = '/Game/AZ/Blueprints/Weapon/AZ_BP_Rifle'
PHASES = (0.0, 0.25, 0.5, 0.75)
MODES = {
    'relaxed': {'aiming': False, 'socket': 'RightHandRifleSocketRelaxed', 'parent': 'hand_r', 'left_grip': 'LeftHandGrip'},
    'aim': {'aiming': True, 'socket': 'RightHandRifleSocketAim', 'parent': 'middle_01_r', 'left_grip': 'LeftHandGripAim'},
}
MAX_CORRECTION_DEGREES = 45.0
ANCHOR_TOLERANCE_CM = 0.00001
IMPROVEMENT_TOLERANCE_CM = 0.00001
AL = unreal.AnimationLibrary
AP = unreal.AnimPoseExtensions
ML = unreal.MathLibrary
EAL = unreal.EditorAssetLibrary


def require(value, message):
    if not value:
        raise RuntimeError(message)


def project_root():
    return Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()


def package(asset):
    return str(asset.get_path_name()).split('.')[0]


def load(path, cls):
    asset = unreal.load_asset(path.split('.')[0])
    require(asset is not None and isinstance(asset, cls), 'Missing asset or wrong class: ' + path)
    return asset


def setup_library():
    return runpy.run_path(str(project_root() / 'Tools/rifle_p01_setup.py'), run_name='rifle_p01_setup_library')


def v(value):
    result = [float(value.x), float(value.y), float(value.z)]
    require(all(math.isfinite(x) for x in result), 'Nonfinite vector')
    return result


def q(value):
    result = [float(value.x), float(value.y), float(value.z), float(value.w)]
    require(all(math.isfinite(x) for x in result), 'Nonfinite quaternion')
    length = math.sqrt(sum(x * x for x in result))
    require(length > 1e-12, 'Zero quaternion')
    return [x / length for x in result]


def add(a, b):
    return [x + y for x, y in zip(a, b)]


def sub(a, b):
    return [x - y for x, y in zip(a, b)]


def norm(a):
    return math.sqrt(sum(x * x for x in a))


def cross(a, b):
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def rotate(rotation, point):
    vector = rotation[:3]
    uv = cross(vector, point)
    uuv = cross(vector, uv)
    return [point[i] + 2.0 * (rotation[3] * uv[i] + uuv[i]) for i in range(3)]


def multiply(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    result = [aw * bx + ax * bw + ay * bz - az * by,
              aw * by - ax * bz + ay * bw + az * bx,
              aw * bz + ax * by - ay * bx + az * bw,
              aw * bw - ax * bx - ay * by - az * bz]
    length = norm(result)
    require(length > 1e-12, 'Degenerate composed quaternion')
    return [x / length for x in result]


def transform_record(transform):
    return {'translation': v(transform.translation), 'rotation': q(transform.rotation), 'scale': v(transform.scale3d)}


def transform_point(transform, point):
    scaled = [point[i] * transform['scale'][i] for i in range(3)]
    return add(transform['translation'], rotate(transform['rotation'], scaled))


def inverse_transform_point(transform, point):
    rotation = transform['rotation']
    local = rotate([-rotation[0], -rotation[1], -rotation[2], rotation[3]], sub(point, transform['translation']))
    require(all(abs(x) > 1e-8 for x in transform['scale']), 'Noninvertible parent scale')
    return [local[i] / transform['scale'][i] for i in range(3)]


def quaternion_angle(rotation):
    return math.degrees(2.0 * math.acos(min(1.0, max(0.0, abs(rotation[3])))))


def socket_record(socket):
    rotation = socket.get_editor_property('relative_rotation')
    return dict(path=str(socket.get_path_name()), name=str(socket.get_editor_property('socket_name')),
                parent=str(socket.get_editor_property('bone_name')),
                translation=v(socket.get_editor_property('relative_location')),
                rotation=q(ML.conv_rotator_to_quaternion(rotation)),
                rotator={'pitch': float(rotation.pitch), 'yaw': float(rotation.yaw), 'roll': float(rotation.roll)},
                scale=v(socket.get_editor_property('relative_scale')),
                force_always_animated=bool(socket.get_editor_property('force_always_animated')))


def all_sockets(mesh):
    return {str(socket.get_path_name()): socket_record(socket)
            for socket in (mesh.get_socket_by_index(i) for i in range(mesh.num_sockets())) if socket is not None}


def bone(pose, name):
    require(name in {str(n) for n in AP.get_bone_names(pose)}, 'Sampled pose lacks bone: ' + name)
    return transform_record(AP.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD))


def statistics(samples, key):
    values = [sample[key] for sample in samples]
    return {'rms_cm': math.sqrt(sum(x * x for x in values) / len(values)),
            'maximum_cm': max(values), 'minimum_cm': min(values),
            'maximum_absolute_cm': max(abs(value) for value in values),
            'mean_cm': sum(values) / len(values)}


def make_transform(record):
    # FTransform fields accept FVector/FQuat directly. No unavailable make_transform API.
    result = unreal.Transform()
    result.translation = unreal.Vector(*record['translation'])
    result.rotation = unreal.Quat(*record['rotation'])
    result.scale3d = unreal.Vector(1.0, 1.0, 1.0)
    return result


def audit():
    saved = project_root() / 'Saved/RifleAnimationContent'
    asset_reference = json.loads((saved / 'p01-socket-assets-before.json').read_text(encoding='utf-8'))
    historical = json.loads((saved / 'p01-socket-alignment-samples.json').read_text(encoding='utf-8'))
    body_mesh = load(asset_reference['body_mesh'], unreal.SkeletalMesh)
    weapon_mesh = load(asset_reference['weapon_mesh'], unreal.SkeletalMesh)
    weapon_bp = load(WEAPON_BP, unreal.Blueprint)
    weapon_cdo = unreal.get_default_object(weapon_bp.generated_class())
    mesh_components = [component for component in weapon_cdo.get_components_by_class(unreal.SkeletalMeshComponent)
                       if str(component.get_name()) == 'WeaponMesh3P']
    require(len(mesh_components) == 1, 'Expected the native WeaponMesh3P CDO component')
    weapon_component = mesh_components[0]
    require(weapon_component.get_editor_property('skeletal_mesh_asset') == weapon_mesh, 'WeaponMesh3P mesh differs from the reviewed asset')
    relative = transform_record(weapon_component.get_relative_transform())
    require(norm(relative['translation']) <= 1e-6 and quaternion_angle(relative['rotation']) <= 0.00001
            and norm(sub(relative['scale'], [1.0, 1.0, 1.0])) <= 1e-6,
            'WeaponMesh3P CDO relative transform must be identity; do not compensate it in sockets')
    reference_pose = AP.get_reference_pose(weapon_mesh.get_editor_property('skeleton'))
    require(AP.is_valid(reference_pose), 'Weapon reference pose is invalid')
    right_bone = bone(reference_pose, 'hand_r')
    right_grip = right_bone['translation']
    body_sockets = all_sockets(body_mesh)
    library = setup_library()['library_rows']()
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
    options.set_editor_property('extract_root_motion', False)
    options.set_editor_property('incorporate_root_motion_into_pose', True)
    options.set_editor_property('optional_skeletal_mesh', body_mesh)
    report = dict(mode='audit', status='audit_complete', body_mesh=package(body_mesh), weapon_mesh=package(weapon_mesh),
                  weapon_relative=relative, weapon_right_bone_reference=right_bone, right_grip_weapon=right_grip,
                  socket_snapshot=body_sockets, modes={}, phases=list(PHASES),
                  reference_files=[str(saved / 'p01-socket-assets-before.json'), str(saved / 'p01-socket-alignment-samples.json')],
                  no_ik=True, unchanged_weapon_scale=True)
    for mode, specification in MODES.items():
        socket = body_mesh.find_socket(specification['socket'])
        require(socket is not None and socket.get_outer() == body_mesh, 'Required socket is not owned by the actual body mesh: ' + specification['socket'])
        original = socket_record(socket)
        require(original['parent'] == specification['parent'], 'Socket parent differs from reviewed mode contract')
        require(norm(sub(original['scale'], [1.0, 1.0, 1.0])) <= 1e-6, 'Socket scale must remain one')
        grip_socket = weapon_mesh.find_socket(specification['left_grip'])
        require(grip_socket is not None, 'Missing weapon grip socket: ' + specification['left_grip'])
        grip_definition = socket_record(grip_socket)
        grip_parent = bone(reference_pose, grip_definition['parent'])
        left_grip = transform_point(grip_parent, grip_definition['translation'])
        span = sub(left_grip, right_grip)
        require(norm(span) > 1e-6, 'Weapon grip span is degenerate')
        anchor = transform_point(original, right_grip)
        selected = [row for row in library if row['aiming'] == specification['aiming']
                    and (not row['use_mm'] or row['direction'] == 'F')]
        require(len(selected) == 5, 'Expected two idles and three forward moving loops per mode')
        samples = []
        for row in selected:
            sequence = load(row['asset'], unreal.AnimSequence)
            length = float(AL.get_sequence_length(sequence))
            require(length > 0.0, 'Cannot sample zero-length animation')
            for fraction in PHASES:
                pose = AP.get_anim_pose_at_time(sequence, length * fraction, options)
                require(AP.is_valid(pose), 'Invalid actual-MHC pose: ' + row['asset'])
                parent = bone(pose, specification['parent'])
                wrist = bone(pose, 'hand_l')['translation']
                knuckle = bone(pose, 'middle_01_l')['translation']
                local_target = inverse_transform_point(parent, wrist)
                desired = sub(local_target, anchor)
                before_grip_body = transform_point(parent, transform_point(original, left_grip))
                samples.append(dict(asset=row['asset'], phase=fraction, time_seconds=length * fraction,
                                    parent_transform=parent, wrist_body=wrist, knuckle_body=knuckle,
                                    desired_vector_parent=desired, desired_span_cm=norm(desired),
                                    span_residual_cm=norm(desired) - norm(span),
                                    before_left_grip_body=before_grip_body,
                                    before_wrist_error_cm=norm(sub(before_grip_body, wrist)),
                                    before_knuckle_error_cm=norm(sub(before_grip_body, knuckle))))
        mean_desired = [sum(sample['desired_vector_parent'][i] for sample in samples) / len(samples) for i in range(3)]
        require(norm(mean_desired) > 1e-6, 'Mean desired wrist direction is degenerate')
        current_vector = rotate(original['rotation'], span)
        delta = q(ML.quat_find_between_vectors(unreal.Vector(*current_vector), unreal.Vector(*mean_desired)))
        new_rotation = multiply(delta, original['rotation'])
        proposed = {'translation': sub(anchor, rotate(new_rotation, right_grip)),
                    'rotation': new_rotation, 'scale': [1.0, 1.0, 1.0]}
        anchor_error = norm(sub(transform_point(proposed, right_grip), anchor))
        for sample in samples:
            after_grip_body = transform_point(sample['parent_transform'], transform_point(proposed, left_grip))
            sample.update(after_left_grip_body=after_grip_body,
                          after_wrist_error_cm=norm(sub(after_grip_body, sample['wrist_body'])),
                          after_knuckle_error_cm=norm(sub(after_grip_body, sample['knuckle_body'])),
                          right_anchor_error_cm=norm(sub(transform_point(sample['parent_transform'], transform_point(proposed, right_grip)),
                                                         transform_point(sample['parent_transform'], anchor))))
        before, after = statistics(samples, 'before_wrist_error_cm'), statistics(samples, 'after_wrist_error_cm')
        angle = quaternion_angle(delta)
        improved = after['rms_cm'] < before['rms_cm'] - IMPROVEMENT_TOLERANCE_CM
        unchanged = angle < 0.00001 and abs(after['rms_cm'] - before['rms_cm']) <= IMPROVEMENT_TOLERANCE_CM
        eligible = angle < MAX_CORRECTION_DEGREES and anchor_error <= ANCHOR_TOLERANCE_CM and (improved or unchanged)
        report['modes'][mode] = dict(socket=specification['socket'], parent=specification['parent'],
                                     old_socket=original, proposed_socket=proposed, delta_quaternion=delta,
                                     correction_degrees=angle, right_anchor_parent=anchor,
                                     right_anchor_error_cm=anchor_error, right_grip_weapon=right_grip,
                                     left_grip_weapon=left_grip, weapon_grip=grip_definition,
                                     weapon_grip_parent_reference=grip_parent, weapon_grip_span_cm=norm(span),
                                     mean_desired_vector_parent=mean_desired, mean_desired_vector_length_cm=norm(mean_desired),
                                     mean_desired_span_cm=sum(s['desired_span_cm'] for s in samples) / len(samples),
                                     unavoidable_span_residual=statistics(samples, 'span_residual_cm'),
                                     before=before, after=after, samples=samples,
                                     historical_idle_measurements=[sample for sample in historical
                                                                  if sample['body_socket'] == specification['socket']],
                                     improved=improved, unchanged=unchanged, eligible=eligible)
    report['eligible'] = all(mode['eligible'] for mode in report['modes'].values())
    report['fingerprint'] = hashlib.sha256(json.dumps({key: report[key] for key in (
        'body_mesh', 'weapon_mesh', 'weapon_relative', 'socket_snapshot', 'modes')}, sort_keys=True).encode('utf-8')).hexdigest()
    return report


def main(prepare=False, expected_fingerprint=None, backup_dir=None, receipt_path=None):
    report = None
    try:
        report = audit()
        if not prepare:
            return report
        if expected_fingerprint is not None:
            require(report['fingerprint'] == expected_fingerprint, 'Measurements changed since the reviewed audit')
        require(report['eligible'], 'One or both modes failed improvement, correction-angle or right-anchor requirements; inspect audit')
        changed_modes = [mode for mode in report['modes'].values() if mode['improved']]
        if not changed_modes:
            report.update(mode='prepare', status='rotation_already_optimal')
            return report
        body_mesh = load(report['body_mesh'], unreal.SkeletalMesh)
        report['backup'] = setup_library()['backup_existing']([report['body_mesh']], backup_dir)
        require(all_sockets(body_mesh) == report['socket_snapshot'], 'Body sockets changed after preflight')
        transforms = [(mode, make_transform(mode['proposed_socket'])) for mode in changed_modes]
        report.update(mode='prepare', status='preparing')
        for mode, transform in transforms:
            socket = body_mesh.find_socket(mode['socket'])
            require(socket is not None and socket.get_outer() == body_mesh, 'Socket ownership changed')
            socket.set_socket_local_transform(transform)
        after = all_sockets(body_mesh)
        modified_paths = {mode['old_socket']['path'] for mode in changed_modes}
        require(set(after) == set(report['socket_snapshot']), 'Socket set changed unexpectedly')
        for path, original in report['socket_snapshot'].items():
            current = after[path]
            if path not in modified_paths:
                require(current == original, 'An unrelated socket changed: ' + path)
            else:
                require(current['name'] == original['name'] and current['parent'] == original['parent']
                        and current['force_always_animated'] == original['force_always_animated']
                        and norm(sub(current['scale'], [1.0, 1.0, 1.0])) <= 1e-6,
                        'Socket identity, parent, animation flag or unit scale changed')
        for mode in changed_modes:
            actual = after[mode['old_socket']['path']]
            error = norm(sub(transform_point(actual, mode['right_grip_weapon']), mode['right_anchor_parent']))
            require(error <= ANCHOR_TOLERANCE_CM, 'Right-grip anchor moved during transform readback')
            require(norm(sub(actual['translation'], mode['proposed_socket']['translation'])) <= 0.00001,
                    'Socket translation readback differs from proposal')
            delta = multiply(actual['rotation'], [-x for x in mode['proposed_socket']['rotation'][:3]] + [mode['proposed_socket']['rotation'][3]])
            require(quaternion_angle(delta) < 0.0001, 'Socket rotation readback differs from proposal')
            mode['applied_socket'] = actual
            mode['applied_right_anchor_error_cm'] = error
            applied_errors = []
            for sample in mode['samples']:
                actual_grip = transform_point(sample['parent_transform'], transform_point(actual, mode['left_grip_weapon']))
                applied_errors.append({'error_cm': norm(sub(actual_grip, sample['wrist_body']))})
            mode['applied_after'] = statistics(applied_errors, 'error_cm')
            require(mode['applied_after']['rms_cm'] < mode['before']['rms_cm'] - IMPROVEMENT_TOLERANCE_CM,
                    'Applied socket did not improve this mode\'s RMS wrist error')
        require(EAL.save_loaded_asset(body_mesh, only_if_is_dirty=False), 'Body mesh package save failed')
        report['socket_snapshot_after'] = after
        report['status'] = 'prepared_socket_alignment'
        return report
    except Exception as error:
        if report is not None:
            report.update(status='failed', error=str(error))
        raise
    finally:
        if report is not None:
            if receipt_path:
                destination = Path(receipt_path)
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_text(json.dumps(report, indent=2), encoding='utf-8')
            print('[P01SocketAlignment] ' + json.dumps({
                'mode': report.get('mode'), 'status': report.get('status'), 'eligible': report.get('eligible'),
                'error': report.get('error'), 'modes': {name: {key: values[key] for key in (
                    'before', 'after', 'correction_degrees', 'right_anchor_error_cm', 'eligible')}
                    for name, values in report.get('modes', {}).items()}}))
        gc.collect()


if __name__ == '__main__':
    main()
