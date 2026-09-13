# @Description: Audit, measure, author and verify missing hero foot contact curves.
"""Editor authoring only. No PIE, tests, AnimBP edits or playback-rate changes.

The live main chooser, its enabled rows, their BranchIn databases, and the
active hero's native locomotion databases define the scope. Existing contact
curves are immutable here. New keys are accepted only from an explicit audit
receipt, after all target packages are backed up and their data revalidated.
"""
import gc
import hashlib
import json
import math
import re
import shutil
import statistics
from datetime import datetime, timezone
from pathlib import Path

import unreal

PROJECT = Path('C:/UnrealEngine/Games/AZ')
OUT = PROJECT / 'Saved/ProceduralContacts'
CHOOSER = '/Game/AZ/Blueprints/Animation/MotionMatching/CHT_v2_CharacterAnimations'
ANIM_BP = '/Game/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC'
MESH = '/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh'
PROFILES = (
    '/Game/AZ/Blueprints/Animation/MotionMatching/RifleP01/DA_WeaponAnim_P01',
    '/Game/AZ/Blueprints/Animation/MotionMatching/Pistol/DA_WeaponAnim_Pistol',
)
SUPPORTED_STATES = {'IdleLoop', 'IdleBreak', 'LocomotionLoop',
                    'TransitiontoIdle', 'TransitiontoLocomotion',
                    'IdleTurnLeft', 'IdleTurnRight', 'TransitionStance'}
CONTACTS = ('contact_l', 'contact_r')
BONES = ('root', 'foot_l', 'ball_l', 'foot_r', 'ball_r')
SETTINGS = dict(sample_hz=60.0, max_foot_speed=35.0, max_ball_speed=30.0,
                foot_height_tolerance=2.5, ball_height_tolerance=1.8,
                minimum_plant_seconds=0.08, minimum_root_travel=2.0,
                stationary_max_foot_range=5.0, stationary_max_ball_range=4.0)
OWNED_ROOTS = ('/Game/AZ/', '/Game/Assets/RTG_AZ/')
OWNER = 'procedural_hero_contact_setup:v1'
AL = unreal.AnimationLibrary
EAL = unreal.EditorAssetLibrary


def package(asset):
    return str(asset.get_path_name()).split('.')[0]


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def load(path, cls=None):
    asset = unreal.load_asset(path)
    if asset is None or (cls and not isinstance(asset, cls)):
        raise RuntimeError('Missing asset or wrong class: ' + path)
    return asset


def curve_keys(seq, name):
    if not AL.does_curve_exist(seq, name, unreal.RawCurveTrackTypes.RCT_FLOAT):
        return None
    times, values = AL.get_float_keys(seq, name)
    return dict(times=[float(x) for x in times], values=[float(x) for x in values])


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True), encoding='utf-8')


def chooser_inventory():
    lines = [str(x) for x in unreal.AZ_ChooserUtils.dump_chooser_full_tree(CHOOSER)]
    text = '\n'.join(lines)
    if not re.search(r'"nested":\s*\[\s*//\s*0 nested', text):
        raise RuntimeError('Chooser nesting changed; explicit scope review required')
    disabled = list(unreal.AZ_ChooserUtils.get_chooser_disabled_rows(CHOOSER))
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    dependencies = [str(x) for x in ar.get_dependencies(CHOOSER, unreal.AssetRegistryDependencyOptions())]
    by_name = {}
    for path in dependencies:
        by_name.setdefault(path.rsplit('/', 1)[-1], []).append(path)
    rows = []
    for line in lines:
        match = re.match(r'\s*\{\s*"i":\s*(\d+)', line)
        if not match:
            continue
        i = int(match.group(1))
        cells = dict(re.findall(r'"c(\d+)":\s*"([^"]*)"', line))
        result = re.search(r'"out":\s*"Asset\[([^]]+)\]:([^"]+)"', line)
        state = cells.get('0', '')
        normalized = state.removeprefix('= ').replace(' ', '') if state.startswith('= ') else None
        supported = normalized in SUPPORTED_STATES
        multi = set(cells.get('7', '').replace(' ', '').split('|'))
        if state.startswith('Any') and multi and multi.issubset(SUPPORTED_STATES):
            supported = True
        # Reactions, jumps, falls and all unconstrained/unknown state rows stay out.
        if cells.get('12') == 'True' or not cells.get('15', '').endswith('None'):
            supported = False
        record = dict(index=i, disabled=bool(disabled[i]), state=state,
                      multi_state=cells.get('7'), reaction=cells.get('15'), supported=supported)
        if result:
            cls, name = result.groups()
            matches = by_name.get(name, [])
            if len(matches) != 1:
                raise RuntimeError('Ambiguous chooser result path: ' + name + ': ' + str(matches))
            record.update(asset=matches[0], asset_class=cls)
        rows.append(record)
    if len(rows) != len(disabled):
        raise RuntimeError('Chooser diagnostic row count mismatch')
    return dict(path=CHOOSER, row_count=len(rows), rows=rows, source_digest=digest(lines),
                disabled_digest=digest(disabled))


def inventory(receipt_path=None):
    """Read-only exact scope/coverage; no pose sampling or asset writes."""
    chooser = chooser_inventory()
    refs, database_sources = {}, {}

    def include(path, source):
        refs.setdefault(path, []).append(source)

    def include_db(db, source):
        path = package(db)
        if path in database_sources:
            database_sources[path]['sources'].append(source)
            return
        members = [package(db.get_animation_asset(i)) for i in range(db.get_num_animation_assets())]
        database_sources[path] = dict(sources=[source], members=members)
        for member in members:
            include(member, 'database:' + path)

    for row in chooser['rows']:
        if row['disabled'] or not row['supported'] or 'asset' not in row:
            continue
        asset = load(row['asset'])
        if isinstance(asset, unreal.AnimSequence):
            include(row['asset'], 'chooser:' + str(row['index']))
        elif isinstance(asset, unreal.PoseSearchDatabase):
            include_db(asset, 'chooser:' + str(row['index']))
    bp = load(ANIM_BP)
    cdo = unreal.get_default_object(bp.generated_class())
    for prop in ('locomotion_loop_database', 'walk_loco_database', 'run_loco_database',
                 'strafe_walk_database', 'strafe_run_database', 'strafe_crouch_database'):
        db = cdo.get_editor_property(prop)
        if db:
            include_db(db, ANIM_BP + ':' + prop)
    for profile_path in PROFILES:
        profile = load(profile_path)
        for prop in ('walk_loco_database', 'run_loco_database', 'strafe_walk_database',
                     'strafe_run_database', 'strafe_crouch_database'):
            db = profile.get_editor_property(prop)
            if db:
                include_db(db, profile_path + ':' + prop)
    # Current raw-clip searches use BranchIn databases attached to selected clips.
    for path in list(refs):
        seq = load(path, unreal.AnimSequence)
        for event in AL.get_animation_notify_events(seq):
            state = event.get_editor_property('notify_state_class')
            if isinstance(state, unreal.AnimNotifyState_PoseSearchBranchIn):
                db = state.get_editor_property('database')
                if db:
                    include_db(db, 'branch_in:' + path)
    clips = {}
    for path, sources in sorted(refs.items()):
        seq = load(path, unreal.AnimSequence)
        rate = float(seq.get_editor_property('rate_scale'))
        existing = {name: curve_keys(seq, name) for name in CONTACTS}
        clips[path] = dict(sources=sorted(set(sources)), length=float(AL.get_sequence_length(seq)),
                           rate_scale=rate, additive=str(seq.get_editor_property('additive_anim_type')),
                           disk_sha256=hashlib.sha256((PROJECT / 'Content' / (path.removeprefix('/Game/') + '.uasset')).read_bytes()).hexdigest(),
                           existing_contacts=existing, missing=[k for k, v in existing.items() if v is None])
    result = dict(version=OWNER, mode='inventory', chooser=chooser, databases=database_sources, clips=clips,
                  mesh=MESH, summary=dict(clips=len(clips), databases=len(database_sources),
                  missing_clips=sum(bool(x['missing']) for x in clips.values()),
                  missing_curves=sum(len(x['missing']) for x in clips.values()),
                  non_unit_rates=[p for p, r in clips.items() if abs(r['rate_scale'] - 1.0) > 1e-6]))
    write_json(receipt_path or OUT / 'inventory.json', result)
    print(json.dumps(result['summary']))
    gc.collect()
    return result


def vector_values(v):
    return [float(v.x), float(v.y), float(v.z)]


def sample_pose(seq, mesh, settings):
    length = float(AL.get_sequence_length(seq))
    if length <= 0:
        raise RuntimeError('Zero-length clip: ' + package(seq))
    count = max(3, int(math.ceil(length * settings['sample_hz'])))
    times = [length * i / count for i in range(count + 1)]
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
    options.set_editor_property('extract_root_motion', False)
    options.set_editor_property('incorporate_root_motion_into_pose', True)
    options.set_editor_property('optional_skeletal_mesh', mesh)
    positions = {b: [] for b in BONES}
    for time in times:
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, time, options)
        if not unreal.AnimPoseExtensions.is_valid(pose):
            raise RuntimeError('Invalid pose: ' + package(seq))
        if time == 0 and not set(BONES).issubset({str(x) for x in unreal.AnimPoseExtensions.get_bone_names(pose)}):
            raise RuntimeError('Required measurement bone missing: ' + package(seq))
        for bone in BONES:
            transform = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
            point = vector_values(transform.translation)
            if not all(math.isfinite(x) for x in point):
                raise RuntimeError('Nonfinite pose: ' + package(seq))
            positions[bone].append(point)
    return times, positions


def velocities(times, positions):
    result = []
    for i in range(len(times)):
        before, after = max(i - 1, 0), min(i + 1, len(times) - 1)
        dt = times[after] - times[before]
        result.append([(positions[after][axis] - positions[before][axis]) / dt for axis in range(3)])
    return result


def span(points):
    return math.sqrt(sum((max(p[a] for p in points) - min(p[a] for p in points)) ** 2 for a in range(3)))


def windows(times, flags):
    result, start = [], None
    for i, flag in enumerate(flags):
        if flag and start is None:
            start = i
        if start is not None and (not flag or i == len(flags) - 1):
            last = i if flag else i - 1
            result.append((start, last))
            start = None
    return result


def measure(seq, source, allow_ip_inference, settings):
    times, points = sample_pose(seq, load(MESH, unreal.SkeletalMesh), settings)
    vel = {b: velocities(times, points[b]) for b in BONES}
    root_travel = span(points['root'])
    floors = {b: min(p[2] for p in points[b]) for b in BONES if b != 'root'}
    speed_curve = curve_keys(seq, 'Speed')
    speed_mean = statistics.median([abs(x) for x in speed_curve['values']]) if speed_curve else None
    compensation = [0.0, 0.0, 0.0]
    inference = dict(allowed=bool(allow_ip_inference), accepted=False)
    # In-place loops still move in the world. Estimate the backwards ground
    # stroke ONLY from low foot AND toe samples, corroborated across both feet.
    if root_travel < settings['minimum_root_travel'] and allow_ip_inference:
        candidates, per_foot = [], {}
        for side in ('l', 'r'):
            foot, ball = 'foot_' + side, 'ball_' + side
            eligible = [i for i in range(len(times))
                        if points[foot][i][2] - floors[foot] <= settings['foot_height_tolerance']
                        and points[ball][i][2] - floors[ball] <= settings['ball_height_tolerance']
                        and abs(vel[foot][i][2]) <= 15.0 and abs(vel[ball][i][2]) <= 15.0]
            per_foot[side] = len(eligible)
            candidates.extend([[0.5 * (vel[foot][i][a] + vel[ball][i][a]) for a in range(2)] for i in eligible])
        if candidates:
            stroke = [statistics.median(v[a] for v in candidates) for a in range(2)]
            speed = math.hypot(*stroke)
            residual = statistics.median(math.dist(v, stroke) for v in candidates)
            supports = all(n >= max(4, len(times) * 0.07) for n in per_foot.values())
            speed_agrees = speed_mean is None or speed_mean < 5 or 0.55 * speed_mean <= speed <= 1.8 * speed_mean
            accepted = supports and speed >= 20 and residual <= min(25.0, 0.3 * speed) and speed_agrees
            inference.update(accepted=accepted, stroke_velocity=stroke, stroke_speed=speed,
                             median_residual=residual, samples_per_foot=per_foot, speed_curve_median=speed_mean)
            if accepted:
                compensation = [-stroke[0], -stroke[1], 0.0]
    result = dict(length=times[-1], root_travel=root_travel, positions_digest=digest(points),
                  inferred_velocity=compensation, inference=inference, curves={})
    for side, name in (('l', 'contact_l'), ('r', 'contact_r')):
        foot, ball = 'foot_' + side, 'ball_' + side
        foot_speed = [math.sqrt(sum((v[a] + compensation[a]) ** 2 for a in range(3))) for v in vel[foot]]
        ball_speed = [math.sqrt(sum((v[a] + compensation[a]) ** 2 for a in range(3))) for v in vel[ball]]
        flags = [foot_speed[i] <= settings['max_foot_speed'] and ball_speed[i] <= settings['max_ball_speed']
                 and points[foot][i][2] - floors[foot] <= settings['foot_height_tolerance']
                 and points[ball][i][2] - floors[ball] <= settings['ball_height_tolerance']
                 for i in range(len(times))]
        # Ambiguous in-place moving loops must not lock a nearly still crossover
        # at the front/back of a stride. They retain terrain adaptation only.
        ambiguous_ip = allow_ip_inference and root_travel < settings['minimum_root_travel'] and not inference['accepted']
        if ambiguous_ip:
            flags = [False] * len(times)
        # A slow sliding track can pass an instantaneous speed test. Bound each
        # plant to a real foot/toe anchor; break and reacquire if it drifts away.
        anchor = None
        for i, flag in enumerate(flags):
            if not flag:
                anchor = None
                continue
            if anchor is None:
                anchor = i
            foot_delta = [(points[foot][i][a] - points[foot][anchor][a]) + compensation[a] * (times[i] - times[anchor]) for a in range(3)]
            ball_delta = [(points[ball][i][a] - points[ball][anchor][a]) + compensation[a] * (times[i] - times[anchor]) for a in range(3)]
            if math.sqrt(sum(x*x for x in foot_delta)) > settings['stationary_max_foot_range'] or math.sqrt(sum(x*x for x in ball_delta)) > settings['stationary_max_ball_range']:
                flags[i], anchor = False, None
        looping = bool(seq.get_editor_property('loop'))
        if looping and flags[0] != flags[-1]:
            flags[0] = flags[-1] = False
        for first, last in windows(times, flags):
            if times[last] - times[first] < settings['minimum_plant_seconds']:
                flags[first:last + 1] = [False] * (last - first + 1)
        if looping and flags[0] != flags[-1]:
            flags[0] = flags[-1] = False
        result['curves'][name] = dict(
            keys=dict(times=times, values=[1.0 if x else 0.0 for x in flags]),
            windows=[[times[a], times[b]] for a, b in windows(times, flags)],
            duty=sum(flags) / len(flags), foot_span=span(points[foot]), ball_span=span(points[ball]),
            floor_foot=floors[foot], floor_ball=floors[ball],
            foot_speed_min=min(foot_speed), foot_speed_max=max(foot_speed),
            ball_speed_min=min(ball_speed), ball_speed_max=max(ball_speed),
            looping=looping, loop_endpoints_equal=flags[0] == flags[-1],
            fallback_reason='ambiguous_in_place_loop' if ambiguous_ip else None,
            existing=source['existing_contacts'][name])
    return result


def audit(paths=None, receipt_path=None):
    """Read-only measured proposal. Pass explicit paths for bounded audit batches."""
    current = inventory()
    selected = sorted(paths if paths is not None else [p for p, r in current['clips'].items() if r['missing']])
    moving_rows = {str(r['index']) for r in current['chooser']['rows'] if r['state'] == '= Locomotion Loop'}
    report = dict(version=OWNER, mode='audit', settings=SETTINGS, inventory=current, measurements={}, skipped={})
    for path in selected:
        source = current['clips'][path]
        if not path.startswith(OWNED_ROOTS):
            report['skipped'][path] = 'outside_verified_AZ_owned_roots'
            continue
        seq = load(path, unreal.AnimSequence)
        if abs(source['rate_scale'] - 1.0) > 1e-6:
            raise RuntimeError('Refuse non-unit-rate source: ' + path)
        if seq.get_editor_property('additive_anim_type') != unreal.AdditiveAnimationType.AAT_NONE:
            report['skipped'][path] = 'additive_clip'
            continue
        moving = any(s.startswith('database:') or s.removeprefix('chooser:') in moving_rows for s in source['sources'])
        report['measurements'][path] = measure(seq, source, moving, SETTINGS)
        write_json(receipt_path or OUT / 'audit.json', report)
    report['summary'] = dict(measured=len(report['measurements']), skipped=len(report['skipped']),
                            planted_clips=sum(any(c['duty'] > 0 for c in x['curves'].values()) for x in report['measurements'].values()),
                            inferred_ip=sum(x['inference']['accepted'] for x in report['measurements'].values()),
                            full_plant_curves=sum(c['duty'] == 1 for x in report['measurements'].values() for c in x['curves'].values()))
    write_json(receipt_path or OUT / 'audit.json', report)
    print(json.dumps(report['summary']))
    gc.collect()
    return report


def preserved_fingerprint(seq):
    """Raw local poses at every native key; deprecated raw-track APIs are empty
    for UE5.8 Sequencer-backed data models and cannot prove preservation.
    """
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property('evaluation_type', unreal.AnimDataEvalType.RAW)
    options.set_editor_property('extract_root_motion', False)
    options.set_editor_property('incorporate_root_motion_into_pose', True)
    model = seq.get_editor_property('data_model_interface')
    count = int(model.get_number_of_keys())
    length = float(AL.get_sequence_length(seq))
    if count < 2:
        raise RuntimeError('Expected at least two source pose keys: ' + package(seq))
    bones_hash = hashlib.sha256()
    names = None
    for i in range(count):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, length * i / (count - 1), options)
        if not unreal.AnimPoseExtensions.is_valid(pose):
            raise RuntimeError('Invalid raw preservation pose: ' + package(seq))
        if names is None:
            names = unreal.AnimPoseExtensions.get_bone_names(pose)
            if len(names) < 10:
                raise RuntimeError('Unexpectedly empty raw bone data: ' + package(seq))
            bones_hash.update(json.dumps([str(n) for n in names]).encode())
        for name in names:
            transform = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
            # Unreal's canonical transform export is deterministic and includes
            # all local translation, rotation and scale components.
            bones_hash.update(transform.export_text().encode())
    curves = {}
    for kind, getter in ((unreal.RawCurveTrackTypes.RCT_FLOAT, AL.get_float_keys),
                         (unreal.RawCurveTrackTypes.RCT_TRANSFORM, AL.get_transformation_keys)):
        for name in AL.get_animation_curve_names(seq, kind):
            if str(name) in CONTACTS:
                continue
            times, values = getter(seq, name)
            # Sequencer may canonicalize FName display case when adding a
            # channel (Speed -> speed); FName identity/lookup is case-insensitive.
            curves[str(kind) + ':' + str(name).casefold()] = dict(times=[float(t) for t in times],
                    values=[float(v) if isinstance(v, (float, int)) else v.export_text() for v in values])
    props = {}
    for name in ('rate_scale', 'loop', 'enable_root_motion', 'root_motion_root_lock',
                 'force_root_lock', 'use_normalized_root_motion_scale', 'additive_anim_type',
                 'ref_pose_type', 'ref_pose_seq', 'ref_frame_index', 'skeleton'):
        value = seq.get_editor_property(name)
        props[name] = value.get_path_name() if isinstance(value, unreal.Object) else str(value)
    notify = [e.export_text() for e in AL.get_animation_notify_events(seq)]
    sync = {str(track): [m.export_text() for m in AL.get_animation_sync_markers_for_track(seq, track)]
            for track in AL.get_animation_notify_track_names(seq)}
    metadata = {str(k): str(v) for k, v in EAL.get_metadata_tag_values(seq).items()
                if not str(k).startswith('AZ.ProceduralContacts.')}
    return dict(bone_pose_sha256=bones_hash.hexdigest(), bone_count=len(names), native_pose_keys=count,
                other_curves_sha256=digest(curves), other_curve_count=len(curves),
                notifies_sha256=digest(notify), notify_count=len(notify), sync_sha256=digest(sync),
                settings=props, metadata_sha256=digest(metadata))


def prepare_authoring(audit_path=None, session_path=None):
    """Validate the reviewed proposal and back up every target BEFORE any edit."""
    audit_path = Path(audit_path or OUT / 'audit.json')
    session_path = Path(session_path or OUT / 'author-session.json')
    if session_path.exists():
        raise RuntimeError('Existing author session must be resumed/reviewed: ' + str(session_path))
    proposal = json.loads(audit_path.read_text(encoding='utf-8'))
    if proposal['version'] != OWNER or proposal['settings'] != SETTINGS or proposal['mode'] != 'audit':
        raise RuntimeError('Review a fresh audit after tool/settings changes')
    current_chooser = chooser_inventory()
    for field in ('source_digest', 'disabled_digest'):
        if current_chooser[field] != proposal['inventory']['chooser'][field]:
            raise RuntimeError('Chooser changed after audit: ' + field)
    for path, record in proposal['inventory']['databases'].items():
        database = load(path, unreal.PoseSearchDatabase)
        members = [package(database.get_animation_asset(i)) for i in range(database.get_num_animation_assets())]
        if members != record['members']:
            raise RuntimeError('Database membership changed after audit: ' + path)
    targets = sorted(proposal['measurements'])
    dirty = {str(p.get_path_name()) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    if dirty.intersection(targets):
        raise RuntimeError('Unsaved target packages: ' + str(sorted(dirty.intersection(targets))))
    originals = []
    for path in targets:
        if not path.startswith(OWNED_ROOTS):
            raise RuntimeError('Target outside owned roots: ' + path)
        source = proposal['inventory']['clips'][path]
        seq = load(path, unreal.AnimSequence)
        if not source['missing'] or any(curve_keys(seq, n) != source['existing_contacts'][n] for n in CONTACTS):
            raise RuntimeError('Contacts changed since audit: ' + path)
        stem = PROJECT / 'Content' / path.removeprefix('/Game/')
        main = stem.with_suffix('.uasset')
        if hashlib.sha256(main.read_bytes()).hexdigest() != source['disk_sha256']:
            raise RuntimeError('Saved package changed since audit: ' + path)
        for extension in ('.uasset', '.uexp', '.ubulk'):
            file = stem.with_suffix(extension)
            if file.exists():
                originals.append((path, file))
    backup = OUT / 'Backups' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    if backup.exists():
        raise RuntimeError('Backup destination already exists: ' + str(backup))
    records = []
    for path, source in originals:
        destination = backup / source.relative_to(PROJECT / 'Content')
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        expected = hashlib.sha256(source.read_bytes()).hexdigest()
        if hashlib.sha256(destination.read_bytes()).hexdigest() != expected:
            raise RuntimeError('Backup copy mismatch: ' + str(source))
        records.append(dict(package=path, source=str(source), backup=str(destination), sha256=expected))
    write_json(backup / 'backup-manifest.json', records)
    session = dict(version=OWNER, audit=str(audit_path), audit_digest=digest(proposal),
                   backup=str(backup), targets=targets, records={}, status='backed_up')
    write_json(session_path, session)
    print(json.dumps(dict(targets=len(targets), backups=len(records), session=str(session_path), backup=str(backup))))
    return session


def require_keys(actual, expected, path, name):
    if actual is None or len(actual['times']) != len(expected['times']):
        raise RuntimeError('Missing/mismatched authored keys: ' + path + ' ' + name)
    if actual['values'] != expected['values'] or any(abs(a - b) > 1e-5 for a, b in zip(actual['times'], expected['times'])):
        raise RuntimeError('Authored key readback mismatch: ' + path + ' ' + name)


def author_batch(limit=8, session_path=None):
    """Resume bounded mutations from a reviewed and fully backed-up session."""
    session_path = Path(session_path or OUT / 'author-session.json')
    session = json.loads(session_path.read_text(encoding='utf-8'))
    proposal = json.loads(Path(session['audit']).read_text(encoding='utf-8'))
    if digest(proposal) != session['audit_digest'] or proposal['settings'] != SETTINGS:
        raise RuntimeError('Author proposal changed after backup')
    pending = [p for p in session['targets'] if p not in session['records']]
    for path in pending[:limit]:
        dirty = {str(p.get_path_name()) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
        if path in dirty:
            raise RuntimeError('Unsaved target; previous partial mutation requires review: ' + path)
        source = proposal['inventory']['clips'][path]
        seq = load(path, unreal.AnimSequence)
        disk = PROJECT / 'Content' / (path.removeprefix('/Game/') + '.uasset')
        if hashlib.sha256(disk.read_bytes()).hexdigest() != source['disk_sha256']:
            raise RuntimeError('Package changed after backup: ' + path)
        if any(curve_keys(seq, n) != source['existing_contacts'][n] for n in CONTACTS):
            raise RuntimeError('Contact changed after backup: ' + path)
        before = preserved_fingerprint(seq)
        session['in_progress'] = dict(path=path, before=before)
        write_json(session_path, session)
        measured = proposal['measurements'][path]
        for name in source['missing']:
            keys = measured['curves'][name]['keys']
            AL.add_curve(seq, name, unreal.RawCurveTrackTypes.RCT_FLOAT)
            # Engine AnimationBlueprintLibrary.cpp SetControllerCurveKeys uses
            # FRichCurveKey(Time,Value), whose default is RCIM_Linear. Adjacent
            # 1.0 keys form true plateaus above the rig's 0.97 lock threshold.
            AL.add_float_curve_keys(seq, name, keys['times'], keys['values'])
            EAL.set_metadata_tag(seq, 'AZ.ProceduralContacts.' + name, OWNER)
            require_keys(curve_keys(seq, name), keys, path, name)
        EAL.set_metadata_tag(seq, 'AZ.ProceduralContacts.Settings', json.dumps(SETTINGS, sort_keys=True))
        after = preserved_fingerprint(seq)
        if after != before:
            raise RuntimeError('Preserved animation data changed; refusing save: ' + path)
        if not EAL.save_loaded_asset(seq, only_if_is_dirty=False):
            raise RuntimeError('Save failed: ' + path)
        session['records'][path] = dict(before=before, after=after, curves=source['missing'],
                                        saved_sha256=hashlib.sha256(disk.read_bytes()).hexdigest())
        session.pop('in_progress', None)
        write_json(session_path, session)
    session['status'] = 'saved' if len(session['records']) == len(session['targets']) else 'authoring'
    write_json(session_path, session)
    print(json.dumps(dict(status=session['status'], saved=len(session['records']), targets=len(session['targets']))))
    gc.collect()
    return session


def resume_verified_pending_save(session_path=None):
    """Retry only this session's already authored, verified asset after an editor
    modal or save cancellation. No curves/data are changed during this recovery.
    """
    session_path = Path(session_path or OUT / 'author-session.json')
    session = json.loads(session_path.read_text(encoding='utf-8'))
    pending = session.get('in_progress')
    if not pending:
        raise RuntimeError('No session-owned pending asset to recover')
    proposal = json.loads(Path(session['audit']).read_text(encoding='utf-8'))
    if digest(proposal) != session['audit_digest']:
        raise RuntimeError('Recovery proposal changed')
    path = pending['path']
    seq = load(path, unreal.AnimSequence)
    source = proposal['inventory']['clips'][path]
    for name in CONTACTS:
        expected = source['existing_contacts'][name]
        if expected is None:
            expected = proposal['measurements'][path]['curves'][name]['keys']
        require_keys(curve_keys(seq, name), expected, path, name)
    after = preserved_fingerprint(seq)
    if after != pending['before']:
        raise RuntimeError('Pending asset changed; preserve it for manual review: ' + path)
    if not EAL.save_loaded_asset(seq, only_if_is_dirty=False):
        raise RuntimeError('Editor still refuses verified pending save: ' + path)
    disk = PROJECT / 'Content' / (path.removeprefix('/Game/') + '.uasset')
    session['records'][path] = dict(before=pending['before'], after=after, curves=source['missing'],
                                    saved_sha256=hashlib.sha256(disk.read_bytes()).hexdigest())
    session.pop('in_progress')
    session['status'] = 'saved' if len(session['records']) == len(session['targets']) else 'authoring'
    write_json(session_path, session)
    print(json.dumps(dict(recovered=path, saved=len(session['records']))))
    gc.collect()
    return session


def verify(session_path=None):
    """Read back every contact, original contact, package hash and unchanged data
    receipts after saving. Does not start editor preview, PIE or gameplay tests.
    """
    session_path = Path(session_path or OUT / 'author-session.json')
    session = json.loads(session_path.read_text(encoding='utf-8'))
    if session['status'] != 'saved':
        raise RuntimeError('Authoring must complete before final verification')
    proposal = json.loads(Path(session['audit']).read_text(encoding='utf-8'))
    preserved, authored, fallbacks = 0, 0, []
    for path, source in proposal['inventory']['clips'].items():
        seq = load(path, unreal.AnimSequence)
        if float(seq.get_editor_property('rate_scale')) != source['rate_scale']:
            raise RuntimeError('Animation playback rate changed: ' + path)
        for name in CONTACTS:
            expected = source['existing_contacts'][name]
            if expected is not None:
                if curve_keys(seq, name) != expected:
                    raise RuntimeError('Original contact curve changed: ' + path + ' ' + name)
                preserved += 1
            elif path in session['records']:
                require_keys(curve_keys(seq, name), proposal['measurements'][path]['curves'][name]['keys'], path, name)
                authored += 1
        if path in session['records']:
            record = session['records'][path]
            disk = PROJECT / 'Content' / (path.removeprefix('/Game/') + '.uasset')
            if record['before'] != record['after'] or hashlib.sha256(disk.read_bytes()).hexdigest() != record['saved_sha256']:
                raise RuntimeError('Saved preservation receipt mismatch: ' + path)
            if not any(c['duty'] for c in proposal['measurements'][path]['curves'].values()):
                fallbacks.append(path)
    report = dict(status='saved_readback_verified', authored_curves=authored, preserved_curves=preserved,
                  changed_assets=len(session['records']), covered_clips=len(proposal['inventory']['clips']),
                  terrain_only_fallbacks=fallbacks, backup=session['backup'],
                  verification='All native-key raw local bone poses, notify events, sync markers, noncontact curve keys, animation settings and metadata unchanged immediately around mutation; saved packages and all contact keys read back.',
                  gameplay_validation='User-performed gameplay review pending; no PIE/tests started.')
    write_json(OUT / 'verification.json', report)
    print(json.dumps(report))
    gc.collect()
    return report
