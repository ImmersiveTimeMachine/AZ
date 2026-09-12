# @Description: Audit, back up, author, or verify the two firearm switch animation profiles.
"""Run main('audit'), main('backup'), main('author'), then main('verify').

Only author mutates Unreal assets, and only the two existing profile data assets.
It requires new native reflection, an unchanged audit/backup baseline, no PIE,
and clean target packages. No Blueprint compilation, tests, previews, socket
changes, clip edits, equipment changes, or quick-slot changes are performed.

The six clips already belong to AZ. They are referenced directly and preserved.
Pistol crouching descriptors deliberately remain empty: native playback falls
back to the standing clip through the existing masked upper-body slot.

Attachment times are measured gesture/approach candidates, not verified prop
contact. Existing socket origins do not coincide during these clips. Native
switch presentation preserves world transform during reattachment and blends
toward the unchanged destination socket; visual acceptance remains user-run.
"""
import gc
import hashlib
import json
import math
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal


ROOT = Path('C:/UnrealEngine/Games/AZ')
OUTPUT = ROOT / 'Saved/WeaponSwitch'
BODY = '/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh'
ANIM_BP = '/Game/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC'
MH_SKELETON = '/Game/MetaHumans/Common/Female/Medium/NormalWeight/Body/metahuman_base_skel'
RIFLE_SOURCE = '/Game/AZ/Assets/M16/Riffle_RTG_MH/AZ_RTG_MH_W2_'
OWNER = 'weapon_switch_setup:v1'
OWNER_KEY = 'AZ.WeaponSwitch.Owner'
MANIFEST_KEY = 'AZ.WeaponSwitch.Manifest'
DESCRIPTORS = ('standing_draw', 'standing_holster', 'crouching_draw', 'crouching_holster')
SETTINGS = {
    'switch_animation_slot': 'RifleFire',
    'switch_animation_play_rate': 1.0,
    'switch_animation_blend_in': 0.10,
    'switch_animation_blend_out': 0.12,
    'switch_socket_blend_duration': 0.15,
}
PROFILES = {
    'rifle': {
        'path': '/Game/AZ/Blueprints/Animation/MotionMatching/RifleP01/DA_WeaponAnim_P01',
        'weapon': '/Game/AZ/Blueprints/Weapon/AZ_BP_Rifle',
        'sockets': {'carry_socket_name': 'BackRifleSocket',
                    'relaxed_socket_name': 'RightHandRifleSocketRelaxed',
                    'aim_socket_name': 'RightHandRifleSocketAim'},
        'clips': {
            'standing_draw': (RIFLE_SOURCE + 'Stand_Rlx_Equip_Back_Get_From_MOB', 17 / 30, 46 / 30),
            'standing_holster': (RIFLE_SOURCE + 'Stand_Rlx_Equip_Back_Return_To_MOB', 20 / 30, 43 / 30),
            'crouching_draw': (RIFLE_SOURCE + 'Crouch_Equip_Back_Get_From_MOB', 16 / 30, 57 / 30),
            'crouching_holster': (RIFLE_SOURCE + 'Crouch_Equip_Back_Return_To_MOB', 17 / 30, 53 / 30),
        },
    },
    'pistol': {
        'path': '/Game/AZ/Blueprints/Animation/MotionMatching/Pistol/DA_WeaponAnim_Pistol',
        'weapon': '/Game/AZ/Blueprints/Weapon/AZ_BP_Pistol',
        'sockets': {'carry_socket_name': 'PistolHolsterSocket',
                    'relaxed_socket_name': 'RightHandPistolSocketRelaxed',
                    'aim_socket_name': 'RightHandPistolSocketAim'},
        'clips': {
            'standing_draw': ('/Game/AZ/Assets/Pistol/AZ_Pistol_Equip', 1 / 30, 26 / 30),
            'standing_holster': ('/Game/AZ/Assets/Pistol/AZ_Pistol_UnEquip', 20 / 30, 40 / 30),
            'crouching_draw': (None, 0.0, 0.0),
            'crouching_holster': (None, 0.0, 0.0),
        },
    },
}
SWITCH_FIELDS = set(DESCRIPTORS) | set(SETTINGS)
EAL = unreal.EditorAssetLibrary
AL = unreal.AnimationLibrary


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def package(asset):
    return asset.get_path_name().split('.')[0] if asset else None


def load(path, cls):
    asset = unreal.load_asset(path)
    require(isinstance(asset, cls), 'Missing or wrong asset class: ' + path)
    return asset


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode('utf-8')).hexdigest()


def asset_file(path):
    require(path.startswith('/Game/'), 'Only project packages may be inspected: ' + path)
    result = (ROOT / 'Content' / (path.removeprefix('/Game/') + '.uasset')).resolve()
    require(result.is_relative_to((ROOT / 'Content').resolve()) and result.is_file(),
            'Missing or out-of-project asset file: ' + str(result))
    return result


def file_record(path):
    source = asset_file(path)
    return {'file': str(source), 'size': source.stat().st_size,
            'mtime_ns': source.stat().st_mtime_ns,
            'sha256': hashlib.sha256(source.read_bytes()).hexdigest()}


def encode(value):
    if value is None or isinstance(value, (str, bool, int)):
        return value
    if isinstance(value, float):
        require(math.isfinite(value), 'Nonfinite reflected profile value')
        return value
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if isinstance(value, (unreal.Name, unreal.Text)):
        return str(value)
    if isinstance(value, unreal.Array):
        return [encode(item) for item in value]
    if isinstance(value, unreal.StructBase):
        return value.export_text()
    raise RuntimeError('Unsupported reflected value type: ' + type(value).__name__)


def profile_properties(profile):
    # Reflection includes future fields, so unrelated tuning is never replaced
    # by an old hand-maintained field list (including recently edited gait speeds).
    result = {}
    for field in sorted(dir(profile)):
        if field.startswith('_'):
            continue
        try:
            value = profile.get_editor_property(field)
        except Exception:
            continue
        result[field] = encode(value)
    require({'walk_speed_override', 'run_speed_override', 'crouch_speed_override',
             'fire_animation_slot', 'play_rate_loop_assets'} <= set(result),
            'Profile reflection did not return the expected existing fields')
    return result


def metadata(asset):
    return {str(k): str(v) for k, v in EAL.get_metadata_tag_values(asset).items()}


def vec(v):
    return [float(v.x), float(v.y), float(v.z)]


def socket_record(socket):
    rotation = socket.get_editor_property('relative_rotation')
    return {'path': socket.get_path_name(), 'name': str(socket.get_editor_property('socket_name')),
            'parent': str(socket.get_editor_property('bone_name')),
            'location': vec(socket.get_editor_property('relative_location')),
            'rotation': [float(rotation.pitch), float(rotation.yaw), float(rotation.roll)],
            'scale': vec(socket.get_editor_property('relative_scale')),
            'force_always_animated': bool(socket.get_editor_property('force_always_animated'))}


def dirty_packages():
    return {p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}


def capture():
    body = load(BODY, unreal.SkeletalMesh)
    skeleton = body.get_editor_property('skeleton')
    require(package(skeleton) == MH_SKELETON, 'Hero body skeleton changed')
    abp = load(ANIM_BP, unreal.AnimBlueprint)
    skeletons = {package(s): s for s in (skeleton, abp.get_editor_property('target_skeleton'))}
    result = {'body_mesh': BODY, 'anim_blueprint': ANIM_BP, 'skeleton': MH_SKELETON,
              'sockets': {}, 'slot_groups': {}, 'weapons': {}, 'clips': {}, 'profiles': {},
              'native_missing': []}
    for i in range(body.num_sockets()):
        socket = body.get_socket_by_index(i)
        if socket:
            result['sockets'][socket.get_path_name()] = socket_record(socket)
    for path, skel in skeletons.items():
        group = str(unreal.AZ_SkeletonUtils.get_animation_slot_group(skel, SETTINGS['switch_animation_slot']))
        require(group == 'WeaponFire', 'Existing masked slot is not in WeaponFire on ' + path)
        result['slot_groups'][path] = group
    if not hasattr(unreal, 'AZ_WeaponSwitchAnimation'):
        result['native_missing'].append('FAZ_WeaponSwitchAnimation')
    else:
        descriptor = unreal.AZ_WeaponSwitchAnimation()
        for field in ('animation', 'attach_time'):
            try:
                descriptor.get_editor_property(field)
            except Exception:
                result['native_missing'].append('FAZ_WeaponSwitchAnimation.' + field)
    for name, spec in PROFILES.items():
        profile = load(spec['path'], unreal.AZ_WeaponAnimationProfile)
        props = profile_properties(profile)
        result['native_missing'].extend(name + '.' + field for field in sorted(SWITCH_FIELDS - set(props)))
        result['profiles'][name] = {'path': spec['path'], 'properties': props,
                                    'metadata': metadata(profile), 'file': file_record(spec['path']),
                                    'dirty': spec['path'] in dirty_packages()}
        cdo = unreal.get_default_object(load(spec['weapon'], unreal.Blueprint).generated_class())
        sockets = {field: str(cdo.get_editor_property(field)) for field in spec['sockets']}
        require(sockets == spec['sockets'], 'Weapon socket assignments changed: ' + spec['weapon'])
        for socket_name in sockets.values():
            require(body.find_socket(socket_name) is not None, 'Hero mesh lacks ' + socket_name)
        result['weapons'][name] = {'path': spec['weapon'], 'sockets': sockets}
        for field, (path, attach_time, expected_length) in spec['clips'].items():
            if not path:
                continue
            clip = load(path, unreal.AnimSequence)
            length = float(AL.get_sequence_length(clip))
            require(abs(length - expected_length) < 0.0001, 'Source clip duration changed: ' + path)
            require(clip.get_editor_property('additive_anim_type') == unreal.AdditiveAnimationType.AAT_NONE,
                    'Switch clip is additive: ' + path)
            require(not clip.get_editor_property('enable_root_motion'), 'Switch clip root motion is enabled: ' + path)
            require(abs(float(clip.get_editor_property('rate_scale')) - 1.0) < 0.000001,
                    'Switch clip RateScale changed: ' + path)
            require(package(clip.get_editor_property('skeleton')) == MH_SKELETON,
                    'Switch clip is not native to the actual hero mesh: ' + path)
            notifies = AL.get_animation_notify_events(clip)
            require(not notifies, 'Switch clip now contains unreviewed notifies: ' + path)
            require(0 <= attach_time < length, 'Attachment time is outside source clip: ' + field)
            result['clips'][path] = {'length': length, 'rate_scale': 1.0, 'enable_root_motion': False,
                                     'additive': False, 'notify_count': 0, 'skeleton': MH_SKELETON,
                                     'file': file_record(path)}
    result['native_ready'] = not result['native_missing']
    return result


def save_receipt(name, value):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    (OUTPUT / (name + '.json')).write_text(json.dumps(value, indent=2), encoding='utf-8')


def read_receipt(name):
    path = OUTPUT / (name + '.json')
    require(path.is_file(), 'Run the required prior stage first: ' + str(path))
    return json.loads(path.read_text(encoding='utf-8'))


def editable(snapshot):
    require(snapshot['native_ready'], 'Build/restart for native reflection: ' + ', '.join(snapshot['native_missing']))
    require(not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(),
            'Authoring requires PIE to be stopped by the user')
    require(not any(p['dirty'] for p in snapshot['profiles'].values()),
            'A target profile has unsaved edits; preserve them and run a fresh audit')


def unchanged(snapshot, baseline):
    require(digest(snapshot) == digest(baseline),
            'Profiles, source clips, reflection, sockets, or saved files changed since audit; run a fresh audit/backup')


def backup(snapshot):
    editable(snapshot)
    audit = read_receipt('audit-readback')
    unchanged(snapshot, audit['snapshot'])
    folder = OUTPUT / 'Backups' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    copies = {}
    for name, spec in PROFILES.items():
        source = asset_file(spec['path'])
        target = folder / source.relative_to((ROOT / 'Content').resolve())
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        require(hashlib.sha256(target.read_bytes()).hexdigest() == snapshot['profiles'][name]['file']['sha256'],
                'Backup bytes differ: ' + str(target))
        copies[name] = str(target)
    report = {'status': 'backed_up_two_profiles', 'snapshot': snapshot, 'files': copies,
              'manifest': digest(PROFILES), 'settings': SETTINGS}
    save_receipt('backup-readback', report)
    return report


def verify_values():
    for name, spec in PROFILES.items():
        profile = load(spec['path'], unreal.AZ_WeaponAnimationProfile)
        for field, (path, time, _) in spec['clips'].items():
            value = profile.get_editor_property(field)
            require(package(value.get_editor_property('animation')) == path,
                    'Switch clip assignment differs: ' + name + '.' + field)
            require(abs(float(value.get_editor_property('attach_time')) - time) < 0.000001,
                    'Switch attachment time differs: ' + name + '.' + field)
        for field, expected in SETTINGS.items():
            actual = profile.get_editor_property(field)
            require(str(actual) == expected if isinstance(expected, str) else abs(float(actual) - expected) < 0.000001,
                    'Switch setting differs: ' + name + '.' + field)


def retained(snapshot, baseline):
    for key in ('body_mesh', 'anim_blueprint', 'skeleton', 'sockets', 'slot_groups', 'weapons', 'clips'):
        require(snapshot[key] == baseline[key], 'Unrelated state changed: ' + key)
    for name in PROFILES:
        for side in ('properties', 'metadata'):
            excluded = SWITCH_FIELDS if side == 'properties' else {OWNER_KEY, MANIFEST_KEY}
            actual = {k: v for k, v in snapshot['profiles'][name][side].items() if k not in excluded}
            before = {k: v for k, v in baseline['profiles'][name][side].items() if k not in excluded}
            require(actual == before, 'Unrelated profile ' + side + ' changed: ' + name)


def main(mode='audit'):
    try:
        require(mode in ('audit', 'backup', 'author', 'verify'), 'Unknown mode')
        snapshot = capture()
        report = {'mode': mode, 'snapshot': snapshot, 'status': 'audited',
                  'timestamp_utc': datetime.now(timezone.utc).isoformat(),
                  'proposed_profiles': PROFILES, 'proposed_settings': SETTINGS,
                  'notes': ['No dedicated crouch pistol switch clips exist; empty descriptors select masked standing fallback.',
                            'Attachment times are measured gesture/approach candidates; socket blending and user visual review are required.',
                            'Only the two profile data assets are authored; existing clips, sockets and unrelated profile settings are preserved.']}
        if mode == 'backup':
            return backup(snapshot)
        if mode in ('author', 'verify'):
            baseline = read_receipt('backup-readback')
            require(baseline['manifest'] == digest(PROFILES) and baseline['settings'] == SETTINGS,
                    'Helper configuration changed since backup; repeat audit/backup')
            for name, path in baseline['files'].items():
                require(hashlib.sha256(Path(path).read_bytes()).hexdigest() == baseline['snapshot']['profiles'][name]['file']['sha256'],
                        'Profile backup no longer matches baseline: ' + name)
            if mode == 'author':
                editable(snapshot)
                unchanged(snapshot, baseline['snapshot'])
                save_receipt('author-progress', {'status': 'starting', 'backup': baseline['files']})
                for name, spec in PROFILES.items():
                    profile = load(spec['path'], unreal.AZ_WeaponAnimationProfile)
                    profile.modify()
                    for field, (path, attach_time, _) in spec['clips'].items():
                        value = unreal.AZ_WeaponSwitchAnimation()
                        value.set_editor_property('animation', load(path, unreal.AnimSequence) if path else None)
                        value.set_editor_property('attach_time', attach_time)
                        profile.set_editor_property(field, value)
                    for field, value in SETTINGS.items():
                        profile.set_editor_property(field, value)
                    EAL.set_metadata_tag(profile, OWNER_KEY, OWNER)
                    EAL.set_metadata_tag(profile, MANIFEST_KEY, digest({'profile': spec, 'settings': SETTINGS}))
                verify_values()
                retained(capture(), baseline['snapshot'])
                save_results = {}
                for name, spec in PROFILES.items():
                    saved = EAL.save_asset(spec['path'], only_if_is_dirty=False)
                    after = file_record(spec['path'])
                    require(spec['path'] not in dirty_packages(), 'Profile remains dirty after save: ' + spec['path'])
                    require(after['mtime_ns'] > baseline['snapshot']['profiles'][name]['file']['mtime_ns'],
                            'Profile save did not update its file timestamp: ' + spec['path'])
                    save_results[name] = {'save_return': bool(saved), 'file': after}
                    save_receipt('author-progress', {'status': 'saving', 'results': save_results,
                                                    'backup': baseline['files']})
                report['save_results'] = save_results
            verify_values()
            report['snapshot'] = capture()
            retained(report['snapshot'], baseline['snapshot'])
            for name, spec in PROFILES.items():
                state = report['snapshot']['profiles'][name]
                require(not state['dirty'], 'Profile still needs saving: ' + spec['path'])
                require(state['metadata'].get(OWNER_KEY) == OWNER
                        and state['metadata'].get(MANIFEST_KEY) == digest({'profile': spec, 'settings': SETTINGS}),
                        'Profile ownership/configuration receipt differs: ' + spec['path'])
            report['backup'] = baseline['files']
            report['status'] = 'authored_saved' if mode == 'author' else 'verified_saved'
        save_receipt(mode + '-readback', report)
        print('WEAPON_SWITCH ' + json.dumps({'mode': mode, 'status': report['status'],
              'native_ready': report['snapshot']['native_ready'],
              'receipt': str(OUTPUT / (mode + '-readback.json'))}))
        return report
    finally:
        gc.collect()


if __name__ == '__main__':
    main('audit')
