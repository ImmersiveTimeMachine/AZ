# @Description: Audit and derive P01 two-phase jump content without changing the native jump state machine.
"""main() audits; main(prepare=True) authors only owned sequences and their landing DB.

Sources are existing compatible Rifle_01 rm_W2 imports. Takeoff keeps measured
anticipation/rise and ends in a held airborne pose through a four-second tail.
Landing is a distinct asset cut from the same source's contact/recovery window.
The master chooser is intentionally a separate coordinated operation.
No retargets, source edits, PIE/tests, AnimBP compilation or AnimBP saves.
"""
import gc
import hashlib
import json
import math
from pathlib import Path
import runpy

import unreal

PROJECT = Path('C:/UnrealEngine/Games/AZ')
SOURCE_ROOT = '/Game/AZ/NoWeapons/RootMotions/'
OWNED = '/Game/AZ/Assets/RTG/Riffle_P01/DerivedJump'
DATABASE = '/Game/AZ/Blueprints/Animation/MotionMatching/RifleP01/PSD_P01_Land'
BP = '/Game/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC'
MESH = '/Game/SurvivalMan/Meshes/SKM_SurvivalMan_Mesh1'
OWNER = 'rifle_p01_jump_setup:v1'
OWNER_KEY, MANIFEST_KEY, STATE_KEY = 'AZ.P01Jump.Owner', 'AZ.P01Jump.Manifest', 'AZ.P01Jump.State'
FPS, HOLD_SECONDS, ANTICIPATION_SECONDS, PRECONTACT_FRAMES = 30, 4.0, 0.2, 2
EAL, AL, PU = unreal.EditorAssetLibrary, unreal.AnimationLibrary, unreal.AZ_PoseSearchUtils


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode('utf-8')).hexdigest()


def package(obj):
    return obj.get_path_name().split('.')[0]


def load(path, cls=None):
    obj = unreal.load_asset(path)
    require(obj is not None and (cls is None or isinstance(obj, cls)), 'Missing/wrong asset: ' + path)
    return obj


def no_game_world():
    require(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None,
            'Game world exists; do not author jump assets during PIE')


def save(obj):
    no_game_world()
    require(EAL.save_loaded_asset(obj, only_if_is_dirty=False), 'Save failed: ' + package(obj))


def roster():
    result = []
    for aim in (False, True):
        prefix = 'Aim' if aim else 'Relaxed'
        result.append(dict(key='Idle' + prefix, source=SOURCE_ROOT + 'rm_W2_Stand_' + prefix + '_Jump',
                           gait=None, aim=aim, foot=None, moving=False))
        for gait, family in (('Walk', 'Walk'), ('Run', 'Jog')):
            for foot in ('RU', 'LU'):
                result.append(dict(key=family + prefix + foot,
                                   source=SOURCE_ROOT + 'rm_W2_' + family + ('_Aim' if aim else '') + '_F_Jump_' + foot,
                                   gait=gait, aim=aim, foot=foot, moving=True))
    return result


def options(mesh):
    value = unreal.AnimPoseEvaluationOptions()
    for name, setting in dict(evaluation_type=unreal.AnimDataEvalType.RAW, should_retarget=False,
                              extract_root_motion=False, incorporate_root_motion_into_pose=True,
                              evaluate_curves=False, optional_skeletal_mesh=mesh).items():
        value.set_editor_property(name, setting)
    return value


def xyz(vector):
    return [float(vector.x), float(vector.y), float(vector.z)]


def source_measurement(seq, mesh):
    require(seq.get_editor_property('skeleton') == mesh.get_editor_property('skeleton'), 'Source skeleton mismatch')
    require(seq.get_editor_property('additive_anim_type') == unreal.AdditiveAnimationType.AAT_NONE, 'Source is additive')
    require(not seq.get_editor_property('loop') and abs(float(seq.get_editor_property('rate_scale')) - 1.0) < 1e-6,
            'Source must be a nonlooping, rate-one jump')
    imported = seq.get_editor_property('asset_import_data')
    filename = str(imported.get_first_filename()) if imported else ''
    require('Rifle_01_PRO_v27A' in filename, 'Unverified Rifle_01 source provenance')
    require(len(AL.get_animation_notify_events(seq)) == 0, 'Source has authored notifies needing an explicit remap')
    length = float(AL.get_sequence_length(seq))
    count = round(length * FPS)
    require(abs(count / FPS - length) < 1e-5 and count > 6, 'Expected frame-aligned 30fps source duration')
    samples = []
    opt = options(mesh)
    for frame in range(count + 1):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, frame / FPS, opt)
        require(unreal.AnimPoseExtensions.is_valid(pose), 'Invalid raw source pose')
        item = {'frame': frame, 'time': frame / FPS}
        for bone in ('root', 'foot_l', 'foot_r'):
            transform = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
            item[bone] = xyz(transform.translation)
            if bone == 'root':
                item['root_yaw'] = float(transform.rotation.rotator().yaw)
        samples.append(item)
    baseline = samples[0]['root'][2]
    apex = max(range(len(samples)), key=lambda i: samples[i]['root'][2])
    rise = samples[apex]['root'][2] - baseline
    require(rise > 10.0, 'Source rise cannot satisfy the existing >10cm native apex detector')
    lift = next(i for i in range(apex + 1) if samples[i]['root'][2] > baseline + 1.0)
    ground_foot = min(min(s['foot_l'][2], s['foot_r'][2]) for s in samples[:3] + samples[-3:])
    contacts = [i for i in range(apex + 1, len(samples))
                if samples[i]['root'][2] <= baseline + 0.5
                and min(samples[i]['foot_l'][2], samples[i]['foot_r'][2]) <= ground_foot + 4.0]
    require(contacts, 'No post-apex ground contact satisfies root and foot measurements')
    contact = contacts[0]
    hold = contact - PRECONTACT_FRAMES
    entry = max(0, lift - round(ANTICIPATION_SECONDS * FPS))
    require(hold > apex and min(samples[hold]['foot_l'][2], samples[hold]['foot_r'][2]) > ground_foot + 1.0,
            'Precontact hold frame is not demonstrably airborne')
    require((apex - entry) / FPS < 0.8, 'Trimmed apex lacks margin before native one-second rise safety timeout')
    require((count - hold) / FPS > 0.15, 'Landing recovery too short for native transition timing')
    require(max(abs(s['root_yaw'] - samples[0]['root_yaw']) for s in samples) < 0.5,
            'Forward jump has changing root yaw; this translation-only rebasing recipe is not applicable')
    data = dict(source=package(seq), source_file=filename, length=length, frames=count, fps=FPS,
                root_motion=bool(seq.get_editor_property('enable_root_motion')), source_entry_frame=entry,
                apex_frame=apex, first_lift_frame=lift, contact_frame=contact, hold_frame=hold,
                ground_foot_z=ground_foot, rise_cm=rise, apex_margin_cm=rise - 10.0,
                trimmed_apex_seconds=(apex - entry) / FPS, samples=samples)
    data['source_fingerprint'] = digest(data)
    return data


def owned_existing(path, manifest, resume=False):
    if not EAL.does_asset_exist(path):
        return None
    obj = load(path)
    require(EAL.get_metadata_tag(obj, OWNER_KEY) == OWNER
            and EAL.get_metadata_tag(obj, MANIFEST_KEY) == json.dumps(manifest, sort_keys=True),
            'Refusing unknown or changed-source output: ' + path)
    require(EAL.get_metadata_tag(obj, STATE_KEY) == 'complete'
            or (resume and EAL.get_metadata_tag(obj, STATE_KEY) == 'preparing'),
            'Interrupted owned output requires explicit inspected resume: ' + path)
    return obj


def claim(obj, manifest, state):
    EAL.set_metadata_tag(obj, OWNER_KEY, OWNER)
    EAL.set_metadata_tag(obj, MANIFEST_KEY, json.dumps(manifest, sort_keys=True))
    EAL.set_metadata_tag(obj, STATE_KEY, state)


def audit(resume=False):
    mesh = load(MESH, unreal.SkeletalMesh)
    bp = load(BP, unreal.AnimBlueprint)
    cdo = unreal.get_default_object(bp.generated_class())
    seed_db = cdo.get_editor_property('jump_database')
    require(seed_db is not None, 'Current AnimBP has no authoritative JumpDatabase schema anchor')
    schema = seed_db.get_editor_property('schema')
    require(schema is not None, 'Existing jump database has no schema')
    report = {'status': 'auditing', 'schema': package(schema), 'database': DATABASE, 'records': [], 'pending': []}
    for spec in roster():
        record = dict(spec)
        report['records'].append(record)
        try:
            measured = source_measurement(load(spec['source'], unreal.AnimSequence), mesh)
            record['measurement'] = measured
            record['takeoff'] = OWNED + '/AS_P01_Jump_' + spec['key'] + '_Takeoff'
            record['land'] = OWNED + '/AS_P01_Jump_' + spec['key'] + '_Land'
            record['manifest'] = {'source': spec['source'], 'source_fingerprint': measured['source_fingerprint'],
                                  'entry_frame': measured['source_entry_frame'], 'hold_frame': measured['hold_frame'],
                                  'source_frames': measured['frames'], 'fps': FPS, 'takeoff_seconds': HOLD_SECONDS,
                                  'indexed_land_seconds': 4 / FPS, 'owner': OWNER}
            for role in ('takeoff', 'land'):
                owned_existing(record[role], dict(record['manifest'], role=role), resume)
        except Exception as error:
            record['error'] = str(error)
            report['pending'].append(spec['key'] + ': ' + str(error))
    report['database_manifest'] = {'schema': report['schema'], 'lands': [r.get('land') for r in report['records']],
                                   'entry_seconds': 4 / FPS, 'membership': 'BranchIn only'}
    owned_existing(DATABASE, report['database_manifest'], resume)
    report['status'] = 'audit_complete' if not report['pending'] else 'audit_pending'
    return report


def bake_role(source, target, measured, role, mesh):
    no_game_world()
    model = source.get_editor_property('data_model_interface')
    bones = list(model.get_bone_track_names())
    require('root' in [str(bone) for bone in bones], 'Source lacks root track')
    source_times, rate = role_times(source, measured, role)
    opt, poses = options(mesh), {}
    for time in sorted(set(source_times)):
        poses[time] = unreal.AnimPoseExtensions.get_anim_pose_at_time(source, time, opt)
    root_origin = unreal.AnimPoseExtensions.get_bone_pose(poses[source_times[0]], 'root', unreal.AnimPoseSpaces.LOCAL).translation
    controller = target.get_editor_property('controller')
    target_model = target.get_editor_property('data_model_interface')
    if controller is None or controller.get_model_interface() != target_model:
        original_controller = source.get_editor_property('controller')
        require(original_controller is not None, 'Source has no compatible data controller')
        controller = type(original_controller)(outer=target)
        controller.set_model(target_model)
    controller.open_bracket('Derive P01 two-phase jump ' + role, False)
    try:
        # Imported Sequencer models can have fractional rates (e.g.29.552fps).
        # Preserve that exact rate and sample at its key times; SetFrameRate rejects
        # unrelated rates, and forcing one would also retime the measured rise.
        controller.set_number_of_frames(unreal.FrameNumber(value=len(source_times) - 1), False)
        for bone in bones:
            samples = {frame: unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.LOCAL)
                       for frame, pose in poses.items()}
            positions, rotations, scales = [], [], []
            for frame in source_times:
                transform = samples[frame]
                p, q, s = transform.translation, transform.rotation, transform.scale3d
                positions.append(unreal.Vector(p.x - root_origin.x, p.y - root_origin.y, p.z - root_origin.z)
                                 if str(bone) == 'root' else unreal.Vector(p.x, p.y, p.z))
                rotations.append(unreal.Quat(q.x, q.y, q.z, q.w))
                scales.append(unreal.Vector(s.x, s.y, s.z))
            require(controller.set_bone_track_keys(bone, positions, rotations, scales, False), 'Bone bake failed: ' + str(bone))
        # Source metadata curves do not describe a held-air or cropped-land timeline. Contacts are
        # intentionally absent, preserving the native last-grounded-foot latch through both roles.
        controller.remove_all_curves_of_type(unreal.RawCurveTrackTypes.RCT_FLOAT, False)
    finally:
        controller.close_bracket(False)
    target.set_editor_property('loop', False)
    target.set_editor_property('rate_scale', 1.0)
    AL.set_root_motion_enabled(target, True)
    AL.set_root_motion_lock_type(target, unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
    AL.set_is_root_motion_lock_forced(target, True)
    return source_times


def role_times(source, measured, role):
    frame_rate = source.get_editor_property('data_model_interface').get_frame_rate()
    rate = float(frame_rate.numerator) / float(frame_rate.denominator)
    require(math.isfinite(rate) and rate > 0, 'Invalid source model frame rate')
    start = measured['source_entry_frame'] / FPS if role == 'takeoff' else measured['hold_frame'] / FPS
    end = measured['hold_frame'] / FPS if role == 'takeoff' else measured['length']
    duration = HOLD_SECONDS if role == 'takeoff' else end - start
    count = math.ceil(duration * rate)
    return [min(start + index / rate, end) for index in range(count + 1)], rate


def role_readback(seq, measured, role, mesh):
    length = float(AL.get_sequence_length(seq))
    source = load(measured['source'], unreal.AnimSequence)
    source_times, rate = role_times(source, measured, role)
    expected = (len(source_times) - 1) / rate
    require(abs(length - expected) < 1e-5 and seq.get_editor_property('enable_root_motion')
            and not seq.get_editor_property('loop') and float(seq.get_editor_property('rate_scale')) == 1.0,
            'Derived role flags/duration mismatch: ' + package(seq))
    opt = options(mesh)
    points = [0, min(3, len(source_times) - 1), len(source_times) - 1]
    if role == 'takeoff':
        points += [round((measured['apex_frame'] - measured['source_entry_frame']) / FPS * rate),
                   math.ceil((measured['hold_frame'] - measured['source_entry_frame']) / FPS * rate), round(3.0 * rate)]
    errors, roots = [], []
    origin_time = source_times[0]
    source_origin = AL.extract_root_track_transform(source, origin_time).translation
    for frame in sorted(set(points)):
        time, source_time = frame / rate, source_times[frame]
        actual = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, time, opt)
        expected_pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(source, source_time, opt)
        raw_root = unreal.AnimPoseExtensions.get_bone_pose(actual, 'root', unreal.AnimPoseSpaces.LOCAL).translation
        native_root = AL.extract_root_track_transform(seq, time).translation
        source_root = AL.extract_root_track_transform(source, source_time).translation
        expected_root = [getattr(source_root, axis) - getattr(source_origin, axis) for axis in ('x', 'y', 'z')]
        require(math.dist(xyz(raw_root), expected_root) < 0.02 and math.dist(xyz(native_root), expected_root) < 0.1,
                'Derived native/raw root track differs from unscaled measured source')
        roots.append({'time': time, 'source_time': source_time, 'raw_root': xyz(raw_root), 'native_root': xyz(native_root)})
        for bone in ('pelvis', 'foot_l', 'foot_r', 'hand_l', 'hand_r'):
            a = unreal.AnimPoseExtensions.get_bone_pose(actual, bone, unreal.AnimPoseSpaces.LOCAL)
            b = unreal.AnimPoseExtensions.get_bone_pose(expected_pose, bone, unreal.AnimPoseSpaces.LOCAL)
            error = math.dist(xyz(a.translation), xyz(b.translation))
            q1, q2 = a.rotation, b.rotation
            rotation_error = 1.0 - abs(q1.x*q2.x + q1.y*q2.y + q1.z*q2.z + q1.w*q2.w)
            require(error < 0.01 and rotation_error < 1e-5, 'Derived local pose differs from sampled source')
            errors.append(error)
    if role == 'takeoff':
        require(max(item['native_root'][2] for item in roots) > 10.0, 'Baked native takeoff root does not pass apex threshold')
        require(math.dist(roots[-1]['native_root'], roots[-2]['native_root']) < 0.02, 'Root moves during held-air tail')
    return dict(path=package(seq), length=length, model_fps=rate, root_samples=roots,
                max_sampled_local_position_error_cm=max(errors),
                source_window=[measured['source_entry_frame'] / FPS if role == 'takeoff' else measured['hold_frame'] / FPS,
                               measured['hold_frame'] / FPS if role == 'takeoff' else measured['frames'] / FPS])


def prepare_content(report, resume=False):
    no_game_world()
    require(not report['pending'], 'Resolve jump measurement gaps before authoring')
    setup = runpy.run_path(str(PROJECT / 'Tools/rifle_p01_setup.py'), run_name='p01_jump_backup')
    existing = [DATABASE] + [r[role] for r in report['records'] for role in ('takeoff', 'land')]
    existing = [path for path in existing if EAL.does_asset_exist(path)
                and EAL.get_metadata_tag(load(path), STATE_KEY) == 'complete']
    if existing:
        report['backup'] = setup['backup_existing'](existing)
    mesh = load(MESH, unreal.SkeletalMesh)
    db = owned_existing(DATABASE, report['database_manifest'], resume)
    if db is None:
        folder, name = DATABASE.rsplit('/', 1)
        db = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.PoseSearchDatabase, None)
        require(db is not None, 'Cannot create owned landing database')
        db.set_editor_property('schema', load(report['schema'], unreal.PoseSearchSchema))
        claim(db, report['database_manifest'], 'preparing')
        save(db)
    for record in report['records']:
        source = load(record['source'], unreal.AnimSequence)
        require(source_measurement(source, mesh) == record['measurement'], 'Source changed since jump audit')
        record['readback'] = {}
        for role in ('takeoff', 'land'):
            no_game_world()
            manifest = dict(record['manifest'], role=role)
            target = owned_existing(record[role], manifest, resume)
            if target is None or EAL.get_metadata_tag(target, STATE_KEY) == 'preparing':
                if target is None:
                    target = EAL.duplicate_asset(record['source'], record[role])
                require(target is not None, 'Duplicate failed: ' + record[role])
                claim(target, manifest, 'preparing')
                bake_role(source, target, record['measurement'], role, mesh)
                if role == 'land':
                    require(PU.add_branch_in_notify(target, db, 0.0, 4 / FPS), 'Landing BranchIn failed')
                record['readback'][role] = role_readback(target, record['measurement'], role, mesh)
                claim(target, manifest, 'complete')
                save(target)
            else:
                record['readback'][role] = role_readback(target, record['measurement'], role, mesh)
    save(db)  # BranchIn-owned membership is synchronized here; no explicit entry insertion.
    members = [package(db.get_animation_asset(i)) for i in range(db.get_num_animation_assets())]
    require(sorted(members) == sorted(report['database_manifest']['lands']), 'Landing DB membership mismatch')
    require(PU.set_disable_reselection_on_database(db, False) == 10, 'Landing entry count mismatch')
    claim(db, report['database_manifest'], 'complete')
    save(db)
    report['database_members'] = members
    report['status'] = 'content_prepared_master_unchanged'
    report['index_status'] = 'requires native index-build confirmation before master activation'


def main(prepare=False, receipt_path=None, resume=False):
    report = None
    try:
        report = audit(resume)
        if prepare:
            prepare_content(report, resume)
        return report
    except Exception as error:
        if report is not None:
            report['error'] = str(error)
            report['status'] = 'failed'
        raise
    finally:
        if report is not None:
            if receipt_path:
                path = Path(receipt_path)
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(json.dumps(report, indent=2), encoding='utf-8')
            print('[P01Jump] ' + json.dumps({key: report.get(key) for key in ('status', 'pending', 'error')}))
        gc.collect()


if __name__ == '__main__':
    main()
