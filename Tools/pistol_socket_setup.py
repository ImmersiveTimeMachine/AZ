# @Description: Derive and append three pistol sockets without changing rifle sockets.
"""main('audit') proposes; main('author') explicitly appends/saves; verify reads.

Uses actual MHC animation poses and the pistol MESH reference pose. The pack's
shared skeleton reference faces +X, while Pistols_B has a +90-degree root import
rotation and actually faces mesh +Y. Sampling with OptionalSkeletalMesh and then
GetRefBonePose is essential: using GetReferencePose(skeleton) turns the gun 90
degrees sideways. Socket math assumes the pistol component's identity transform.

No editor previews, PIE, Blueprint compile, animation edits, scaling or IK.
"""
import gc
import hashlib
import json
import math
import runpy
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
SOURCE_RECEIPT = ROOT / 'Saved/Pistol/socket-pose-audit.json'
OUTPUT = ROOT / 'Saved/PistolSockets'
OWNER_KEY = 'AZ.PistolSockets.Owner'
OWNER = 'pistol_socket_setup:v1'
WEAPON_MESH = '/Game/MilitaryWeapDark/Weapons/Pistols_B'
WEAPON_ANIM = '/Game/MilitaryWeapDark/Weapons/Anims/Fire_Pistol_W'
WEAPON_BP = '/Game/AZ/Blueprints/Weapon/AZ_BP_Pistol'
CLIPS = ['AZ_Pistol_Idle', 'AZ_Pistol_Idle_Relaxed', 'AZ_Pistol_CrouchLoop']
NAMES = ['RightHandPistolSocketAim', 'RightHandPistolSocketRelaxed', 'PistolHolsterSocket']
AP, ML, EAL = unreal.AnimPoseExtensions, unreal.MathLibrary, unreal.EditorAssetLibrary


def require(value, message):
    if not value:
        raise RuntimeError(message)


def helpers():
    return runpy.run_path(str(ROOT / 'Tools/rifle_p01_socket_alignment.py'), run_name='pistol_socket_math')


def load(path, cls):
    asset = unreal.load_asset(path.split('.')[0])
    require(isinstance(asset, cls), 'Missing asset or wrong class: ' + path)
    return asset


def options(mesh):
    value = unreal.AnimPoseEvaluationOptions()
    value.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
    value.set_editor_property('extract_root_motion', False)
    value.set_editor_property('incorporate_root_motion_into_pose', True)
    value.set_editor_property('optional_skeletal_mesh', mesh)
    return value


def inverse_rotation(q):
    return [-q[0], -q[1], -q[2], q[3]]


def local_socket(H, parent, world_location, world_rotation, name, bone):
    result = {'name': name, 'parent': bone, 'translation': H['inverse_transform_point'](parent, world_location),
              'rotation': H['multiply'](inverse_rotation(parent['rotation']), world_rotation), 'scale': [1, 1, 1]}
    r = ML.quat_rotator(unreal.Quat(*result['rotation']))
    result['rotator'] = {'pitch': float(r.pitch), 'yaw': float(r.yaw), 'roll': float(r.roll)}
    return result


def projected(H, parent, socket, point):
    return H['transform_point'](parent, H['transform_point'](socket, point))


def same_socket(H, actual, proposed):
    return (actual['parent'] == proposed['parent']
            and H['norm'](H['sub'](actual['translation'], proposed['translation'])) < 0.0001
            and H['quaternion_angle'](H['multiply'](inverse_rotation(actual['rotation']), proposed['rotation'])) < 0.001
            and H['norm'](H['sub'](actual['scale'], [1, 1, 1])) < 0.00001)


def derive(H):
    recorded = json.loads(SOURCE_RECEIPT.read_text(encoding='utf-8'))
    body = load(recorded['body_mesh'], unreal.SkeletalMesh)
    skeleton = body.get_editor_property('skeleton')
    weapon = load(WEAPON_MESH, unreal.SkeletalMesh)
    bounds = weapon.get_bounds()
    lengths = [2 * value for value in H['v'](bounds.box_extent)]
    require(15 < max(lengths) < 65 and min(lengths) > 1, 'Unexpected pistol size; inspect import units')
    body_bounds = body.get_bounds()
    require(100 < 2 * body_bounds.box_extent.z < 250, 'Unexpected body height; inspect import units')
    frame = AP.get_anim_pose_at_time(load(WEAPON_ANIM, unreal.AnimSequence), 0.0, options(weapon))
    require(AP.is_valid(frame), 'Invalid pistol mesh reference context')
    reference = {name: H['transform_record'](AP.get_ref_bone_pose(frame, name, unreal.AnimPoseSpaces.WORLD))
                 for name in ('Root_Bone1', 'Grip_Bone', 'Trigger_Bone')}
    for bone in reference.values():
        require(H['norm'](H['sub'](bone['scale'], [1, 1, 1])) < 0.00001, 'Weapon reference scale must be one')
    muzzle = weapon.find_socket('MuzzleFlash')
    require(muzzle is not None and str(muzzle.get_editor_property('bone_name')) == 'Grip_Bone', 'Unexpected muzzle parent')
    muzzle_record = H['socket_record'](muzzle)
    muzzle_point = H['transform_point'](reference['Grip_Bone'], muzzle_record['translation'])
    muzzle_rotation = H['multiply'](reference['Grip_Bone']['rotation'], muzzle_record['rotation'])
    actual_forward = H['rotate'](muzzle_rotation, [1, 0, 0])
    require(H['norm'](H['sub'](actual_forward, [0, 1, 0])) < 0.001,
            'Actual pistol muzzle axis changed; remeasure before authoring')
    # Desired MuzzleFlash +X is body +Y and its +Z is body +Z. Its mesh-space
    # reference already contains the pack's root import rotation.
    target_muzzle_rotation = H['q'](ML.conv_rotator_to_quaternion(unreal.Rotator(pitch=0, yaw=90, roll=0)))
    aimed_rotation = H['multiply'](target_muzzle_rotation, inverse_rotation(muzzle_rotation))
    poses = {}
    for name in CLIPS:
        clip = load('/Game/AZ/Assets/Pistol/' + name, unreal.AnimSequence)
        pose = AP.get_anim_pose_at_time(clip, 0.0, options(body))
        require(AP.is_valid(pose), 'Invalid pistol pose: ' + name)
        poses[name] = {bone: H['bone'](pose, bone) for bone in ('root', 'pelvis', 'thigh_r', 'hand_r', 'index_03_r')}
        for bone, transform in poses[name].items():
            require(H['norm'](H['sub'](transform['scale'], [1, 1, 1])) < 0.00001, 'Body pose scale must be one: ' + bone)
        require(H['norm'](H['sub'](poses[name]['hand_r']['translation'], recorded[name]['hand_r']['translation'])) < 0.01,
                'Pistol source pose changed since the root audit: ' + name)
    aim = poses['AZ_Pistol_Idle']
    trigger = reference['Trigger_Bone']['translation']
    world_origin = H['sub'](aim['index_03_r']['translation'], H['rotate'](aimed_rotation, trigger))
    hand = local_socket(H, aim['hand_r'], world_origin, aimed_rotation, NAMES[0], 'hand_r')
    relaxed = dict(hand, name=NAMES[1])
    # Right-thigh holster: twelve cm outward from the thigh joint, four cm
    # lower in the relaxed body frame. Barrel down; gun top faces outward.
    thigh = poses['AZ_Pistol_Idle_Relaxed']['thigh_r']
    holster_origin = H['add'](thigh['translation'], [-12, 0, -4])
    # Actual mesh +Y -> body -Z; actual mesh +Z -> body -X (right/outward).
    holster_rotation = [-0.5, -0.5, 0.5, 0.5]
    holster = local_socket(H, thigh, holster_origin, holster_rotation, NAMES[2], 'thigh_r')
    samples = []
    for name in CLIPS:
        clip = load('/Game/AZ/Assets/Pistol/' + name, unreal.AnimSequence)
        length = float(unreal.AnimationLibrary.get_sequence_length(clip))
        for phase in (0.0, 0.25, 0.5, 0.75):
            pose = AP.get_anim_pose_at_time(clip, phase * length, options(body))
            parent = H['bone'](pose, 'hand_r')
            finger = H['bone'](pose, 'index_03_r')['translation']
            point = projected(H, parent, hand, trigger)
            rotation = H['multiply'](parent['rotation'], hand['rotation'])
            samples.append({'clip': name, 'phase': phase, 'trigger_error_cm': H['norm'](H['sub'](point, finger)),
                            'trigger_body': point, 'muzzle_body': projected(H, parent, hand, muzzle_point),
                            'barrel_forward_body': H['rotate'](rotation, actual_forward),
                            'pistol_up_body': H['rotate'](rotation, [0, 0, 1])})
    require(samples[0]['trigger_error_cm'] < 0.0001, 'Aimed trigger constraint did not solve')
    component_state = None
    if EAL.does_asset_exist(WEAPON_BP):
        cdo = unreal.get_default_object(load(WEAPON_BP, unreal.Blueprint).generated_class())
        components = [c for c in cdo.get_components_by_class(unreal.SkeletalMeshComponent) if c.get_name() == 'WeaponMesh3P']
        require(len(components) == 1, 'Pistol actor has no unique third-person weapon mesh')
        component_state = H['transform_record'](components[0].get_relative_transform())
        require(H['norm'](component_state['translation']) < 0.00001
                and H['quaternion_angle'](component_state['rotation']) < 0.0001
                and H['norm'](H['sub'](component_state['scale'], [1, 1, 1])) < 0.00001,
                'Pistol actor mesh must have identity transform before socket authoring')
    report = {'body_mesh': body.get_path_name(), 'skeleton': skeleton.get_path_name(), 'weapon_mesh': weapon.get_path_name(),
              'source_receipt': str(SOURCE_RECEIPT), 'weapon_dimensions_cm': lengths, 'weapon_reference': reference,
              'muzzle_reference': muzzle_record, 'weapon_component': component_state,
              'proposed': [hand, relaxed, holster], 'sampled_grip': samples, 'sockets_before': H['all_sockets'](body),
              'holster_origin_body': holster_origin, 'holster_forward_body': H['rotate'](holster_rotation, actual_forward),
              'relaxed_note': 'Same fixed hand grip; relaxed finger pose may lift index away from trigger.'}
    return body, skeleton, report


def verify(H, body, skeleton, report, baseline=None):
    for proposed in report['proposed']:
        socket = body.find_socket(proposed['name'])
        require(socket is not None and socket.get_outer() == skeleton, 'Missing or mesh-shadowed pistol socket: ' + proposed['name'])
        require(same_socket(H, H['socket_record'](socket), proposed), 'Pistol socket transform differs: ' + proposed['name'])
    if baseline:
        actual = H['all_sockets'](body)
        for path, record in baseline['sockets_before'].items():
            if record['name'] not in NAMES:
                require(actual.get(path) == record, 'An existing non-pistol socket changed: ' + record['name'])


def main(mode='audit'):
    require(mode in ('audit', 'author', 'verify'), 'Mode must be audit, author or verify')
    try:
        H = helpers()
        body, skeleton, report = derive(H)
        OUTPUT.mkdir(parents=True, exist_ok=True)
        report['mode'] = mode
        package = skeleton.get_path_name().split('.')[0]
        file = ROOT / 'Content' / (package.removeprefix('/Game/') + '.uasset')
        if mode == 'author':
            require(not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE before authoring sockets')
            dirty = {p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
            require(package not in dirty, 'Shared skeleton has unsaved changes; preserve them before socket authoring')
            for proposed in report['proposed']:
                socket = body.find_socket(proposed['name'])
                if socket:
                    require(EAL.get_metadata_tag(skeleton, OWNER_KEY) == OWNER and socket.get_outer() == skeleton,
                            'Foreign or mesh-owned socket; inspect before changing: ' + proposed['name'])
                    require(same_socket(H, H['socket_record'](socket), proposed),
                            'Existing pistol socket was tuned; refusing to overwrite: ' + proposed['name'])
            backup = ROOT / 'Saved/Backups/PistolSockets' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
            backup.mkdir(parents=True)
            require(file.is_file(), 'Skeleton package must exist on disk before authoring')
            shutil.copy2(file, backup / file.name)
            (backup / 'before.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
            # Record baseline before any operation so an interrupted run remains reviewable.
            (OUTPUT / 'baseline.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
            EAL.set_metadata_tag(skeleton, OWNER_KEY, OWNER)
            for proposed in report['proposed']:
                if body.find_socket(proposed['name']):
                    continue
                r = proposed['rotator']
                require(unreal.AZ_SkeletonUtils.add_socket(skeleton, proposed['name'], proposed['parent'],
                        unreal.Vector(*proposed['translation']), unreal.Rotator(pitch=r['pitch'], yaw=r['yaw'], roll=r['roll']), False),
                        'Cannot append pistol socket: ' + proposed['name'])
            verify(H, body, skeleton, report, report)
            require(EAL.save_loaded_asset(skeleton, only_if_is_dirty=False), 'Skeleton save failed')
            report.update(status='authored_saved', backup=str(backup))
        elif mode == 'verify':
            require(EAL.get_metadata_tag(skeleton, OWNER_KEY) == OWNER, 'Pistol sockets have no authoring marker')
            baseline = json.loads((OUTPUT / 'baseline.json').read_text(encoding='utf-8'))
            verify(H, body, skeleton, report, baseline)
            require(package not in {p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()},
                    'Shared skeleton still needs saving')
            report['status'] = 'verified_saved'
        else:
            report['status'] = 'proposed_only'
        report['sockets_after'] = H['all_sockets'](body)
        if file.is_file():
            report['skeleton_sha256'] = hashlib.sha256(file.read_bytes()).hexdigest()
        receipt = OUTPUT / (mode + '-readback.json')
        receipt.write_text(json.dumps(report, indent=2), encoding='utf-8')
        print('PISTOL_SOCKETS ' + json.dumps({'mode': mode, 'status': report['status'], 'receipt': str(receipt), 'proposed': report['proposed']}))
        return report
    finally:
        gc.collect()


if __name__ == '__main__':
    main('audit')
