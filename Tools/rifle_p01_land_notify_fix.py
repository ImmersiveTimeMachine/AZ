# @Description: Audit or restore the missing LandComplete gameplay-event notify on ten owned P01 landing sequences.
"""main() is read-only. Root reviews the audit before explicit prepare=True.

Only ten existing DerivedJump/*_Land sequences may be saved. Their current
BranchIn events, tracks, root-motion settings, curves, native root extraction and
all raw local bone poses at every animation key are verified unchanged. The
existing working AZ_AnimNotify_SendGameplayEvent class and EventTag are reused;
a named notify is not a gameplay-event sender.

Moving beat = min(length*0.5, length-0.15-0.03). Idle beat additionally precedes
the existing 0.40s movement-resume gate by0.05s (therefore0.35s on these clips).
This restores the existing mid-recovery notify contract, not the stale GA prose
about an AnimInstance handback publisher. An arbitrarily early moving-land
interruption can still miss a positive-time event; the existing watchdog remains
the guard for that native behavior. No state-machine or physics changes here.
"""

import gc
import hashlib
import json
import math
import runpy
from pathlib import Path

import unreal


ROOT = Path('C:/UnrealEngine/Games/AZ')
CONTENT_RECEIPT = ROOT / 'Saved/RifleAnimationContent/p01-jump-prepare.json'
OWNED_ROOT = '/Game/AZ/Assets/RTG/Riffle_P01/DerivedJump/'
OWNER = 'rifle_p01_jump_setup:v1'
EVENT_TAG = 'Event.Movement.LandComplete'
NOTIFY_CLASS = '/Script/AZ.AZ_AnimNotify_SendGameplayEvent'
BRANCH_CLASS = '/Script/PoseSearch.AnimNotifyState_PoseSearchBranchIn'
LAND_DATABASE = '/Game/AZ/Blueprints/Animation/MotionMatching/RifleP01/PSD_P01_Land'
REFERENCE = '/Game/Assets/RTG_AZ/MovementAnimsetPro/AnimPro_JumpIdleLand'
MESH = '/Game/SurvivalMan/Meshes/SKM_SurvivalMan_Mesh1'
TRIGGER_FIELDS = ('trigger_weight_threshold', 'notify_trigger_chance', 'trigger_on_dedicated_server',
                  'trigger_on_follower', 'notify_filter_type', 'montage_tick_type')
AL, AP, EAL = unreal.AnimationLibrary, unreal.AnimPoseExtensions, unreal.EditorAssetLibrary


def require(value, message):
    if not value:
        raise RuntimeError(message)


def load(path, cls=unreal.AnimSequence):
    obj = unreal.load_asset(path)
    require(obj is not None and isinstance(obj, cls), 'Missing or unexpected asset: ' + path)
    # This editor's Python bridge can return empty single-tag reads until the
    # package metadata map has been materialized by the supported bulk reader.
    EAL.get_metadata_tag_values(obj)
    return obj


def package(obj):
    return str(obj.get_path_name()).split('.')[0]


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def no_game_world():
    require(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None,
            'PIE/game world exists; landing assets cannot be authored now')


def transform_values(transform):
    p, q, s = transform.translation, transform.rotation, transform.scale3d
    values = [float(p.x), float(p.y), float(p.z), float(q.x), float(q.y), float(q.z), float(q.w),
              float(s.x), float(s.y), float(s.z)]
    require(all(math.isfinite(value) for value in values), 'Nonfinite animation transform')
    return values


def pose_fingerprint(seq):
    model = seq.get_editor_property('data_model_interface')
    require(model is not None, 'Animation data model missing')
    names = [str(name) for name in model.get_bone_track_names()]
    rate = model.get_frame_rate()
    hz = float(rate.numerator) / float(rate.denominator)
    keys = int(model.get_number_of_keys())
    length = float(AL.get_sequence_length(seq))
    require(keys > 1 and hz > 0 and 'root' in names, 'Invalid keyed animation data')
    options = unreal.AnimPoseEvaluationOptions()
    for name, value in dict(evaluation_type=unreal.AnimDataEvalType.RAW, should_retarget=False,
                            extract_root_motion=False, incorporate_root_motion_into_pose=True,
                            evaluate_curves=False, optional_skeletal_mesh=load(MESH, unreal.SkeletalMesh)).items():
        options.set_editor_property(name, value)
    raw_hash, root_hash = hashlib.sha256(), hashlib.sha256()
    for frame in range(keys):
        time = min(frame / hz, length)
        pose = AP.get_anim_pose_at_time(seq, time, options)
        require(AP.is_valid(pose), 'Raw pose sampling failed: ' + package(seq))
        raw_values = [transform_values(AP.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)) for name in names]
        root_values = transform_values(AL.extract_root_track_transform(seq, time))
        raw_hash.update(json.dumps([frame, time, raw_values], separators=(',', ':')).encode())
        root_hash.update(json.dumps([frame, time, root_values], separators=(',', ':')).encode())
    curves = {}
    for name in AL.get_animation_curve_names(seq, unreal.RawCurveTrackTypes.RCT_FLOAT):
        times, values = AL.get_float_keys(seq, name)
        curves[str(name)] = [list(times), list(values)]
    properties = {}
    for name in ('enable_root_motion', 'force_root_lock', 'root_motion_root_lock', 'loop', 'rate_scale',
                 'additive_anim_type', 'ref_pose_type', 'ref_frame_index'):
        value = seq.get_editor_property(name)
        properties[name] = value if isinstance(value, (bool, int, float, str)) else str(value)
    base = seq.get_editor_property('ref_pose_seq')
    properties['ref_pose_seq'] = package(base) if base else None
    return dict(length=length, frame_rate=[int(rate.numerator), int(rate.denominator)], key_count=keys,
                bone_tracks=names, raw_all_bones_sha256=raw_hash.hexdigest(), native_root_sha256=root_hash.hexdigest(),
                skeleton=package(seq.get_editor_property('skeleton')), properties=properties, float_curves_sha256=digest(curves))


def events(seq):
    result = []
    tracks = [str(name) for name in AL.get_animation_notify_track_names(seq)]
    for index, track in enumerate(tracks):
        # AnimNotifyEvent.TrackIndex is not reflected; the supported per-track
        # reader establishes track identity without reaching into protected data.
        for event in AL.get_animation_notify_events_for_track(seq, track):
            notify = event.get_editor_property('notify')
            state = event.get_editor_property('notify_state_class')
            row = dict(name=str(event.get_editor_property('notify_name')),
                       time=float(AL.get_anim_notify_event_trigger_time(event)),
                       duration=float(AL.get_anim_notify_event_duration(event)),
                       notify_class=notify.get_class().get_path_name() if notify else None,
                       state_class=state.get_class().get_path_name() if state else None,
                       track_index=index, track=track)
            row['settings'] = {field: str(event.get_editor_property(field)) for field in TRIGGER_FIELDS}
            if notify and row['notify_class'] == NOTIFY_CLASS:
                row['tag'] = str(notify.get_editor_property('event_tag').get_editor_property('tag_name'))
            if state and row['state_class'] == BRANCH_CLASS:
                database = state.get_editor_property('database')
                row['database'] = package(database) if database else None
            result.append(row)
    return result


def is_completion(row):
    return row['notify_class'] == NOTIFY_CLASS and row.get('tag') == EVENT_TAG


def reference_contract():
    original = load(REFERENCE)
    matches = [row for row in events(original) if is_completion(row)]
    require(len(matches) == 1 and abs(matches[0]['time'] - 0.517) < 0.001,
            'Working idle-land completion contract changed; review it again')
    for event in AL.get_animation_notify_events(original):
        notify = event.get_editor_property('notify')
        if notify and notify.get_class().get_path_name() == NOTIFY_CLASS:
            tag = notify.get_editor_property('event_tag')
            if str(tag.get_editor_property('tag_name')) == EVENT_TAG:
                return matches[0], tag
    raise RuntimeError('Working gameplay-event notify object is missing')


def audit():
    content = json.loads(CONTENT_RECEIPT.read_text())
    records = content['records']
    require(len(records) == 10 and len({row['land'] for row in records}) == 10, 'Expected ten unique derived landing assets')
    reference, _ = reference_contract()
    report = dict(mode='audit', status='audit_complete', reference_asset=REFERENCE, reference_notify=reference,
                  records=[], limitation='Arbitrarily early moving-land interruption can precede any positive-time notify; existing watchdog remains the guard.')
    for record in records:
        path = record['land']
        require(path.startswith(OWNED_ROOT) and path.endswith('_Land'), 'Refusing a non-owned landing path')
        seq = load(path)
        expected_manifest = json.dumps(dict(record['manifest'], role='land'), sort_keys=True)
        require(EAL.get_metadata_tag(seq, 'AZ.P01Jump.Owner') == OWNER
                and EAL.get_metadata_tag(seq, 'AZ.P01Jump.State') == 'complete'
                and EAL.get_metadata_tag(seq, 'AZ.P01Jump.Manifest') == expected_manifest,
                'Derived landing ownership/source manifest changed: ' + path)
        current = events(seq)
        completion = [row for row in current if is_completion(row)]
        untouched = [row for row in current if not is_completion(row)]
        require(len(completion) <= 1, 'Duplicate LandComplete senders require explicit review')
        require(len(untouched) == 1 and untouched[0]['state_class'] == BRANCH_CLASS
                and untouched[0].get('database') == LAND_DATABASE and abs(untouched[0]['time']) < 1e-6
                and abs(untouched[0]['duration'] - 4 / 30) < 1e-6,
                'Unexpected non-completion notify data; preserve and review: ' + path)
        tracks = [str(name) for name in AL.get_animation_notify_track_names(seq)]
        require('1' in tracks, 'Existing notification track 1 is missing')
        length = float(AL.get_sequence_length(seq))
        beat = min(length * 0.5, length - 0.15 - 0.03) if record['moving'] else min(length * 0.5, 0.40 - 0.05)
        require(0 < beat < length, 'Recovery beat is outside the landing')
        if record['moving']:
            require(beat > 4 / 30 + 0.001 and beat < length - 0.15,
                    'Beat can be skipped by the indexed entry or normal loop handback')
        else:
            require(beat < 0.40 and beat < length - 0.15, 'Idle beat misses earliest movement resume/normal handback')
        if completion:
            require(abs(completion[0]['time'] - beat) < 1e-6 and completion[0]['settings'] == reference['settings']
                    and completion[0]['track'] == '1', 'Existing LandComplete differs from the reviewed calibration')
        report['records'].append(dict(key=record['key'], path=path, moving=record['moving'], beat=beat,
                                      frame_zero_entry_limit=4 / 30 if record['moving'] else 0.0,
                                      normal_exit_clip_time=length - 0.15,
                                      earliest_idle_resume_seconds=0.40 if not record['moving'] else None,
                                      tracks=tracks, original_other_events=untouched, existing_completion=completion,
                                      before_pose=pose_fingerprint(seq)))
    report['fingerprint'] = digest(dict(reference=reference, records=report['records']))
    return report


def main(prepare=False, expected_fingerprint=None, receipt_path=None):
    report = None
    try:
        if prepare:
            no_game_world()
        report = audit()
        if not prepare:
            return report
        if expected_fingerprint is not None:
            require(report['fingerprint'] == expected_fingerprint, 'Audit changed since root review')
        changed = [record for record in report['records'] if not record['existing_completion']]
        if not changed:
            report.update(mode='prepare', status='already_prepared')
            return report
        no_game_world()
        setup = runpy.run_path(str(ROOT / 'Tools/rifle_p01_setup.py'), run_name='p01_land_notify_backup')
        report['backup'] = setup['backup_existing']([record['path'] for record in changed])
        contract, event_tag = reference_contract()
        require(contract == report['reference_notify'], 'Reference notify changed after audit')
        report.update(mode='prepare', status='preparing')
        for record in changed:
            no_game_world()
            seq = load(record['path'])
            require(events(seq) == record['original_other_events'], 'Landing notifies changed after preflight')
            require(pose_fingerprint(seq) == record['before_pose'], 'Landing pose/root data changed after preflight')
            sender = AL.add_animation_notify_event(seq, '1', record['beat'], unreal.AZ_AnimNotify_SendGameplayEvent)
            require(sender is not None and sender.get_class().get_path_name() == NOTIFY_CLASS, 'Gameplay-event sender creation failed')
            sender.set_editor_property('event_tag', event_tag)
            after_events = events(seq)
            completion = [event for event in after_events if is_completion(event)]
            require([event for event in after_events if not is_completion(event)] == record['original_other_events'],
                    'An existing BranchIn/event was altered')
            require(len(completion) == 1 and abs(completion[0]['time'] - record['beat']) < 1e-6
                    and completion[0]['duration'] == 0.0 and completion[0]['track'] == '1'
                    and completion[0]['settings'] == contract['settings'], 'New sender time/tag/trigger settings differ from contract')
            require([str(name) for name in AL.get_animation_notify_track_names(seq)] == record['tracks'], 'Notify tracks changed')
            no_game_world()
            require(EAL.save_loaded_asset(seq, only_if_is_dirty=False), 'Landing sequence save failed')
            record['after_pose'] = pose_fingerprint(seq)
            require(record['after_pose'] == record['before_pose'], 'Native root extraction, raw poses, curves or flags changed')
            record['after_events'] = events(seq)
            require(record['after_events'] == after_events, 'Saved notify readback differs')
        report['status'] = 'land_completion_notifies_prepared'
        return report
    except Exception as error:
        if report is not None:
            report.update(status='failed', error=str(error))
        raise
    finally:
        if report is not None:
            if receipt_path:
                path = Path(receipt_path)
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(json.dumps(report, indent=2), encoding='utf-8')
            print('[P01LandNotify] ' + json.dumps({key: report.get(key) for key in ('mode', 'status', 'fingerprint', 'error')}))
        gc.collect()


if __name__ == '__main__':
    main()
