# @Description: Audit or prepare owned RifleAnimsetPro sequences on SKEL_SurvivalMan.
"""Load with runpy.run_path(), then call main(paths, prepare=True) explicitly.

Running this file directly only audits the three-clip probe. No PIE, animation
preview, AnimBP compilation, chooser edits, contact authoring or index builds.
Outputs are content preparation candidates; visual approval is still separate.
"""
import gc
import hashlib
import json
import math
from pathlib import Path

import unreal


SOURCE_ROOT = '/Game/RifleAnimsetPro/Animations'
TARGET_ROOT = '/Game/AZ/Assets/RTG/RifleAnimsetPro'
RETARGETER_PATH = '/Game/AZ/Blueprints/Animation/MotionMatching/Rifle/Rigs/RTG_RifleAnimsetPro_SurvivalMan'
SOURCE_MESH = '/Game/RifleAnimsetPro/UE4_Mannequin/Mesh/SK_Mannequin'
TARGET_MESH = '/Game/SurvivalMan/Meshes/SKM_SurvivalMan_Mesh1'
SOURCE_RIG = '/Game/Characters/Mannequin_UE4/Rigs/IK_UE4_Mannequin_MCO1'
TARGET_RIG = '/Game/SurvivalMan/Rigs/IK_Mannequin'
OWNER_KEY = 'AZ.RifleAnimation.Owner'
OWNER = 'rifle_animation_content_setup:v1'
SOURCE_KEY = 'AZ.RifleAnimation.Source'
HASH_KEY = 'AZ.RifleAnimation.SourceAuditHash'
STATE_KEY = 'AZ.RifleAnimation.Stage'
EAL = unreal.EditorAssetLibrary
AL = unreal.AnimationLibrary

PROBE = [SOURCE_ROOT + '/RootMotion/' + name for name in (
    'Rifle_Idle', 'Rifle_Idle_GunDown', 'Rifle_WalkFwdLoop')]
# Explicit playback policy: names such as ShootLoop_Additive are not enough.
LOOPS = {
    'Rifle_Idle', 'Rifle_Idle_GunDown', 'Rifle_CrouchLoop',
    'Rifle_WalkFwdLoop', 'Rifle_WalkBwdLoop', 'Rifle_RunFwdLoop',
    'Rifle_RunBwdLoop', 'Rifle_SprintLoop',
    'Rifle_StrafeLeftLoop', 'Rifle_StrafeRightLoop',
    'Rifle_StrafeLeft45Loop', 'Rifle_StrafeRight45Loop',
    'Rifle_StrafeLeft135Loop', 'Rifle_StrafeRight135Loop',
    'Rifle_StrafeRunLeftLoop', 'Rifle_StrafeRunRightLoop',
    'Rifle_StrafeRun45LeftLoop', 'Rifle_StrafeRun45RightLoop',
    'Rifle_StrafeRun135LeftLoop', 'Rifle_StrafeRun135RightLoop',
    'Rifle_Crouch_WalkFwd', 'Rifle_Crouch_WalkBwd',
    'Rifle_Crouch_WalkLt', 'Rifle_Crouch_WalkRt',
    'Rifle_Crouch_StrafeLeftt45', 'Rifle_Crouch_StrafeRight45',
    'Rifle_Crouch_StrafeLeft135', 'Rifle_Crouch_StrafeRight135',
}
STANDING_AIM = {'Aim_' + suffix for suffix in ('CC', 'D', 'L', 'LD', 'LU', 'R', 'RD', 'RU', 'U')}
CROUCH_AIM = {'Rifle_Crouch_AimAdditive_' + suffix for suffix in ('CC', 'CD', 'CU', 'LC', 'LD', 'LU', 'RC', 'RD', 'RU')}


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


def claim(asset, source, fingerprint, stage):
    EAL.set_metadata_tag(asset, OWNER_KEY, OWNER)
    EAL.set_metadata_tag(asset, SOURCE_KEY, source)
    EAL.set_metadata_tag(asset, HASH_KEY, fingerprint)
    EAL.set_metadata_tag(asset, STATE_KEY, stage)


def existing(path, source, fingerprint, stage=None):
    if not EAL.does_asset_exist(path):
        return None
    asset = load(path)
    expected = {OWNER_KEY: OWNER, SOURCE_KEY: source, HASH_KEY: fingerprint}
    if stage is not None:
        expected[STATE_KEY] = stage
    for key, value in expected.items():
        if EAL.get_metadata_tag(asset, key) != value:
            raise RuntimeError('Refusing unrecognized, changed-source or incomplete output: ' + path + ' (' + key + ')')
    return asset


def output_paths(source):
    relative = source[len(SOURCE_ROOT) + 1:]
    group, name = relative.rsplit('/', 1)
    return (TARGET_ROOT + '/_RetargetRaw/' + group + '/RAW_RAP_' + name,
            TARGET_ROOT + '/' + group + '/AS_RAP_' + name)


def aim_center(source):
    name = source.rsplit('/', 1)[1]
    if name in STANDING_AIM:
        return SOURCE_ROOT + '/AimOffsets/Aim_CC'
    if name in CROUCH_AIM:
        return SOURCE_ROOT + '/AimOffsets/Rifle_Crouch_AimAdditive_CC'
    if '/AimOffsets/' in source:
        raise ValueError('Unrecognized aim pose; supply an explicit policy before authoring: ' + source)
    return None


def base_path(center):
    stance = 'Standing' if center.endswith('/Aim_CC') else 'Crouched'
    return TARGET_ROOT + '/AimOffsets/Bases/AS_RAP_' + stance + '_Base'


def root_motion(seq, mesh):
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
    options.set_editor_property('extract_root_motion', False)
    options.set_editor_property('incorporate_root_motion_into_pose', True)
    options.set_editor_property('optional_skeletal_mesh', mesh)
    length = float(AL.get_sequence_length(seq))
    samples = []
    for time in (0.0, length * 0.5, length):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, time, options)
        if not unreal.AnimPoseExtensions.is_valid(pose):
            raise RuntimeError('Invalid sampled pose: ' + package(seq))
        if 'root' not in {str(name) for name in unreal.AnimPoseExtensions.get_bone_names(pose)}:
            raise RuntimeError('No root bone: ' + package(seq))
        transform = unreal.AnimPoseExtensions.get_bone_pose(pose, 'root', unreal.AnimPoseSpaces.WORLD)
        p, q = transform.translation, transform.rotation
        yaw = math.degrees(math.atan2(2 * (q.w * q.z + q.x * q.y), 1 - 2 * (q.y * q.y + q.z * q.z)))
        samples.append({'time': time, 'translation': [float(p.x), float(p.y), float(p.z)], 'yaw': yaw})
    first, last = samples[0], samples[-1]
    delta = [last['translation'][i] - first['translation'][i] for i in range(3)]
    return {'samples': samples, 'delta_translation': delta,
            'delta_yaw': (last['yaw'] - first['yaw'] + 180.0) % 360.0 - 180.0,
            'mean_planar_speed': math.hypot(delta[0], delta[1]) / length if length else 0.0}


def audit(seq, mesh):
    flags = {}
    for name in ('enable_root_motion', 'force_root_lock', 'loop', 'rate_scale', 'ref_frame_index'):
        flags[name] = seq.get_editor_property(name)
    for name in ('additive_anim_type', 'ref_pose_type'):
        flags[name] = str(seq.get_editor_property(name))
    base = seq.get_editor_property('ref_pose_seq')
    flags['ref_pose_seq'] = package(base) if base else None
    curves = {}
    for name in AL.get_animation_curve_names(seq, unreal.RawCurveTrackTypes.RCT_FLOAT):
        times, values = AL.get_float_keys(seq, name)
        curves[str(name)] = {'times': [float(t) for t in times], 'values': [float(v) for v in values]}
    return {'asset': package(seq), 'skeleton': package(seq.get_editor_property('skeleton')),
            'length_seconds': float(AL.get_sequence_length(seq)), 'flags': flags,
            'float_curves': curves, 'root_motion': root_motion(seq, mesh)}


def get_retargeter(source_mesh, target_mesh, source_rig, target_rig, fingerprint):
    asset = existing(RETARGETER_PATH, SOURCE_RIG + ' -> ' + TARGET_RIG, fingerprint, 'ready')
    if asset is None:
        folder, name = RETARGETER_PATH.rsplit('/', 1)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.IKRetargeter, unreal.IKRetargetFactory())
        if not asset:
            raise RuntimeError('Retargeter creation failed')
        controller = unreal.IKRetargeterController.get_controller(asset)
        for side, rig, mesh in ((unreal.RetargetSourceOrTarget.SOURCE, source_rig, source_mesh),
                                (unreal.RetargetSourceOrTarget.TARGET, target_rig, target_mesh)):
            controller.set_ik_rig(side, rig)
            controller.set_preview_mesh(side, mesh)
        controller.add_default_ops()
        controller.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, source_rig)
        controller.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, target_rig)
        controller.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
        found_root_op = False
        for index in range(controller.get_num_retarget_ops()):
            op = controller.get_op_controller(index)
            if isinstance(op, unreal.IKRetargetRootMotionController):
                op.set_source_root_bone('root')
                op.set_target_root_bone('root')
                op.set_target_pelvis_bone('pelvis')
                settings = op.get_settings()
                settings.set_editor_property('root_motion_source', unreal.RootMotionSource.COPY_FROM_SOURCE_ROOT)
                op.set_settings(settings)
                found_root_op = True
        if not found_root_op:
            raise RuntimeError('Default retarget operations did not include a root motion controller')
        claim(asset, SOURCE_RIG + ' -> ' + TARGET_RIG, fingerprint, 'ready')
        save(asset)
    controller = unreal.IKRetargeterController.get_controller(asset)
    for side, rig, mesh in ((unreal.RetargetSourceOrTarget.SOURCE, source_rig, source_mesh),
                            (unreal.RetargetSourceOrTarget.TARGET, target_rig, target_mesh)):
        if controller.get_ik_rig(side) != rig or controller.get_preview_mesh(side) != mesh:
            raise RuntimeError('Owned retargeter configuration changed: ' + RETARGETER_PATH)
    return asset


def retarget_raw(source, record, source_mesh, target_mesh, retargeter):
    path = record['raw_target']
    asset = existing(path, source, record['source_hash'], 'raw')
    if asset is None:
        inputs = unreal.IKRetargetBatchOperationInputs()
        values = {'assets_to_retarget': [EAL.find_asset_data(source)],
                  'source_mesh': source_mesh, 'target_mesh': target_mesh,
                  'ik_retarget_asset': retargeter, 'prefix': 'RAW_RAP_',
                  'target_path': path.rsplit('/', 1)[0], 'use_source_path': False,
                  'include_referenced_assets': False, 'overwrite_existing_files': False,
                  'retain_additive_flags': False}
        for name, value in values.items():
            inputs.set_editor_property(name, value)
        outputs = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
        output_assets = [data.get_asset() for data in outputs]
        if len(output_assets) != 1 or package(output_assets[0]) != path:
            raise RuntimeError('Unexpected retarget output: ' + str([package(a) for a in output_assets]))
        asset = output_assets[0]
        claim(asset, source, record['source_hash'], 'raw')
        save(asset)
    if asset.get_editor_property('skeleton') != target_mesh.get_editor_property('skeleton'):
        raise RuntimeError('Retarget produced the wrong skeleton: ' + path)
    return asset


def duplicate(source_asset, destination, origin, fingerprint, stage):
    asset = EAL.duplicate_asset(package(source_asset), destination)
    if not asset:
        raise RuntimeError('Duplicate failed: ' + destination)
    claim(asset, origin, fingerprint, stage)
    return asset


def main(source_paths=None, prepare=False, receipt_path=None):
    """Return a JSON-serializable receipt. Only prepare=True permits asset writes.

    source_paths is an explicit list of up to 96 RifleAnimsetPro sequence package
    paths. Aim requests also include their center pose (at most two dependencies).
    Existing recognized outputs are reused; unknown outputs are never overwritten.
    receipt_path optionally writes this audit/partial-failure receipt to disk.
    """
    receipt = {'mode': 'prepare' if prepare else 'audit', 'status': 'running',
               'retargeter': RETARGETER_PATH, 'records': [], 'warnings': []}
    try:
        requested = list(PROBE if source_paths is None else source_paths)
        if not requested or len(requested) > 96 or len(requested) != len(set(requested)):
            raise ValueError('Provide 1-96 distinct source package paths')
        for path in requested:
            if not isinstance(path, str) or not path.startswith(SOURCE_ROOT + '/') or '.' in path or '..' in path:
                raise ValueError('Expected a RifleAnimsetPro package path: ' + str(path))
            if path[len(SOURCE_ROOT) + 1:].split('/')[0] not in ('RootMotion', 'InPlace', 'AimOffsets'):
                raise ValueError('Unsupported source folder: ' + path)
        paths = list(requested)
        centers = set()
        for path in requested:
            center = aim_center(path)
            if center:
                centers.add(center)
                if center not in paths:
                    paths.append(center)
        receipt['requested_sources'] = requested
        receipt['added_aim_centers'] = [p for p in paths if p not in requested]
        source_mesh, target_mesh = load(SOURCE_MESH, unreal.SkeletalMesh), load(TARGET_MESH, unreal.SkeletalMesh)
        source_rig, target_rig = load(SOURCE_RIG, unreal.IKRigDefinition), load(TARGET_RIG, unreal.IKRigDefinition)
        for rig, mesh in ((source_rig, source_mesh), (target_rig, target_mesh)):
            if not unreal.IKRigController.get_controller(rig).is_skeletal_mesh_compatible(mesh):
                raise RuntimeError('Rig incompatible with requested mesh: ' + package(rig))
        rig_hash = digest([SOURCE_RIG, TARGET_RIG, SOURCE_MESH, TARGET_MESH, OWNER])
        existing(RETARGETER_PATH, SOURCE_RIG + ' -> ' + TARGET_RIG, rig_hash, 'ready')
        records = {}
        # Complete every source audit/collision check before the first asset write.
        for source in paths:
            seq = load(source, unreal.AnimSequence)
            if seq.get_editor_property('skeleton') != source_mesh.get_editor_property('skeleton'):
                raise RuntimeError('Source sequence/preview skeleton mismatch: ' + source)
            data = audit(seq, source_mesh)
            if not aim_center(source) and seq.get_editor_property('additive_anim_type') != unreal.AdditiveAnimationType.AAT_NONE:
                raise ValueError('Non-aim additive sequence needs a separate explicit base policy: ' + source)
            raw_path, target_path = output_paths(source)
            record = {'source': source, 'source_audit': data, 'source_hash': digest(data),
                      'raw_target': raw_path, 'target': target_path,
                      'set_loop': source.rsplit('/', 1)[1] in LOOPS,
                      'aim_base': base_path(aim_center(source)) if aim_center(source) else None}
            existing(raw_path, source, record['source_hash'], 'raw')
            existing(target_path, source, record['source_hash'], 'prepared')
            records[source] = record
            receipt['records'].append(record)
        for center in centers:
            existing(base_path(center), center, records[center]['source_hash'], 'aim_base')
        if not prepare:
            receipt['status'] = 'audit_complete'
            return receipt
        retargeter = get_retargeter(source_mesh, target_mesh, source_rig, target_rig, rig_hash)
        raw = {source: retarget_raw(source, records[source], source_mesh, target_mesh, retargeter) for source in paths}
        bases = {}
        for center in sorted(centers):
            fingerprint = records[center]['source_hash']
            base = existing(base_path(center), center, fingerprint, 'aim_base')
            if base is None:
                base = duplicate(raw[center], base_path(center), center, fingerprint, 'aim_base')
                base.set_editor_property('additive_anim_type', unreal.AdditiveAnimationType.AAT_NONE)
                base.set_editor_property('ref_pose_type', unreal.AdditiveBasePoseType.ABPT_NONE)
                base.set_editor_property('ref_pose_seq', None)
                base.set_editor_property('enable_root_motion', False)
                base.set_editor_property('loop', False)
                save(base)
            if (base.get_editor_property('skeleton') != target_mesh.get_editor_property('skeleton')
                    or base.get_editor_property('additive_anim_type') != unreal.AdditiveAnimationType.AAT_NONE):
                raise RuntimeError('Owned aim base must remain a full pose on the target skeleton: ' + base_path(center))
            bases[center] = base
        for source in paths:
            record = records[source]
            seq = existing(record['target'], source, record['source_hash'], 'prepared')
            reused = seq is not None
            if seq is None:
                # Final duplication is deliberate: project RM-retarget bake workaround.
                seq = duplicate(raw[source], record['target'], source, record['source_hash'], 'preparing')
                if record['set_loop']:
                    seq.set_editor_property('loop', True)
                center = aim_center(source)
                if center:
                    seq.set_editor_property('additive_anim_type', unreal.AdditiveAnimationType.AAT_ROTATION_OFFSET_MESH_SPACE)
                    seq.set_editor_property('ref_pose_type', unreal.AdditiveBasePoseType.ABPT_ANIM_FRAME)
                    seq.set_editor_property('ref_pose_seq', bases[center])
                    seq.set_editor_property('ref_frame_index', 0)
                    seq.set_editor_property('enable_root_motion', False)
                save(seq)
            data = audit(seq, target_mesh)
            if data['skeleton'] != package(target_mesh.get_editor_property('skeleton')):
                raise RuntimeError('Prepared sequence has wrong skeleton: ' + record['target'])
            if record['set_loop'] and not data['flags']['loop']:
                raise RuntimeError('Prepared loop flag was changed: ' + record['target'])
            if record['aim_base'] and data['flags']['ref_pose_seq'] != record['aim_base']:
                raise RuntimeError('Prepared aim base mismatch: ' + record['target'])
            if record['aim_base'] and (
                    seq.get_editor_property('additive_anim_type') != unreal.AdditiveAnimationType.AAT_ROTATION_OFFSET_MESH_SPACE
                    or seq.get_editor_property('ref_pose_type') != unreal.AdditiveBasePoseType.ABPT_ANIM_FRAME
                    or data['flags']['ref_frame_index'] != 0):
                raise RuntimeError('Prepared aim additive settings changed: ' + record['target'])
            record.update({'target_audit': data, 'reused': reused,
                           'raw_audit': audit(raw[source], target_mesh)})
            source_speed = record['source_audit']['root_motion']['mean_planar_speed']
            if source_speed > 1.0 and data['root_motion']['mean_planar_speed'] < 0.01:
                raise RuntimeError('Moving source lost root translation after duplication: ' + record['target'])
            source_yaw = abs(record['source_audit']['root_motion']['delta_yaw'])
            if source_yaw > 1.0 and abs(data['root_motion']['delta_yaw']) < 0.01:
                raise RuntimeError('Turning source lost root yaw after duplication: ' + record['target'])
            if not reused:
                claim(seq, source, record['source_hash'], 'prepared')
                save(seq)
        # Confirm this script did not alter source flags/curves/sampled poses.
        for source in paths:
            if digest(audit(load(source), source_mesh)) != records[source]['source_hash']:
                raise RuntimeError('Source audit changed unexpectedly: ' + source)
        receipt['status'] = 'prepared_candidates'
        receipt['warnings'].append('Foot curves, PoseSearch indexes, chooser wiring, aim axis/grip alignment and visual review are separate steps.')
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
        print('[RifleAnimationContent] ' + json.dumps(receipt, sort_keys=True))
        gc.collect()


if __name__ == '__main__':
    main()
