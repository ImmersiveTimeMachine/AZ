# @Description: Audit, author, or verify the pistol animation profile and shared chooser integration.
"""Run main(stage='audit') first in Unreal Python; authoring is explicitly staged.

content: owned copies, measured loop curves, BranchIn databases, aim offsets/profile.
chooser: append pistol selections and pistol-only exclusion gates to the current main table.
graph: bind the existing aim idle players after the native profile fields have been built.
verify: read back assets and authoring receipts. No PIE, tests, index build, or AnimBP
compile/save is performed here. Compile/save the graph using the native editor route.
Imported pistol sequences and unrelated selector cells are never overwritten.
"""
import gc
import hashlib
import json
import math
from pathlib import Path
import re
import runpy

import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUTPUT = ROOT / 'Saved/PistolAnimation'
SOURCE = '/Game/AZ/Assets/Pistol/'
ANIMS = SOURCE + 'Runtime/'
FOLDER = '/Game/AZ/Blueprints/Animation/MotionMatching/Pistol'
PROFILE = FOLDER + '/DA_WeaponAnim_Pistol'
RIFLE_PROFILE = '/Game/AZ/Blueprints/Animation/MotionMatching/RifleP01/DA_WeaponAnim_P01'
CHOOSER = '/Game/AZ/Blueprints/Animation/MotionMatching/CHT_v2_CharacterAnimations'
SCHEMA = '/Game/AZ/Blueprints/Animation/MotionMatching/PSS_v2_SurvivalMan_Loco'
BLUEPRINT = '/Game/AZ/Blueprints/Animation/MHC/AZ_ABP_MoverHero_MHC'
OWNER = 'pistol_animation_setup:v1'
KEY = 'AZ.PistolAnimation.Owner'
MANIFEST = 'AZ.PistolAnimation.Manifest'
STATE = 'AZ.PistolAnimation.State'
EAL = unreal.EditorAssetLibrary
AL = unreal.AnimationLibrary
CU = unreal.AZ_ChooserUtils
PU = unreal.AZ_PoseSearchUtils
AG = unreal.AZ_AnimGraphNodeUtils

LOOPS = {
    'Walk': ('WalkFwdLoop', 'StrafeRight45Loop', 'StrafeRightLoop', 'StrafeRight135Loop',
             'WalkBwdLoop', 'StrafeLeft135Loop', 'StrafeLeftLoop', 'StrafeLeft45Loop'),
    'Run': ('RunFwdLoop', 'StrafeRun45RightLoop', 'StrafeRunRightLoop', 'StrafeRun135RightLoop',
            'RunBwdLoop', 'StrafeRun135LeftLoop', 'StrafeRunLeftLoop', 'StrafeRun45LeftLoop'),
    'Crouch': ('Crouch_WalkFwd', 'Crouch_StrafeRight45', 'Crouch_WalkRt', 'Crouch_StrafeRight135',
               'Crouch_WalkBwd', 'Crouch_StrafeLeft135', 'Crouch_WalkLt', 'Crouch_StrafeLeftt45'),
    'Sprint': ('SprintLoop',),
}
DIRECTIONS = ('F', 'FR', 'R', 'BR', 'B', 'BL', 'L', 'FL')
# The importer incremented a numeric suffix instead of appending 1 in these four pairs.
RM_SPECIAL = {'Crouch_StrafeRight45': 'Crouch_StrafeRight46',
              'Crouch_StrafeRight135': 'Crouch_StrafeRight136',
              'Crouch_StrafeLeft135': 'Crouch_StrafeLeft136',
              'Crouch_StrafeLeftt45': 'Crouch_StrafeLeftt46',
              'TurnL_90': 'TurnL_91', 'TurnR_90': 'TurnR_91',
              'TurnL_180': 'TurnL_181', 'TurnR_180': 'TurnR_181'}
AO = {
    'Standing': [('Look_' + s, x, y) for s, x, y in (
        ('CC_Additive', 0, 0), ('CU', 0, 1), ('CD', 0, -1), ('L', -1, 0),
        ('LU', -1, 1), ('LD', -1, -1), ('R', 1, 0), ('RU', 1, 1), ('RD', 1, -1))],
    'Crouching': [('Crouch_AimAdditive_' + s, x, y) for s, x, y in (
        ('CC', 0, 0), ('CU', 0, 1), ('CD', 0, -1), ('LC', -1, 0),
        ('LU', -1, 1), ('LD', -1, -1), ('RC', 1, 0), ('RU', 1, 1), ('RD', 1, -1))],
}
AIM_NODES = {
    'standing': ('60917F59462B0879367A80A14E3E19EC', 'WeaponStandingAimPose', 'standing_aim_pose'),
    'crouching': ('7CF4FDBD4BF8C0899A8CACBF41844FD9', 'WeaponCrouchingAimPose', 'crouching_aim_pose'),
}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def libraries():
    return {key: runpy.run_path(str(ROOT / 'Tools' / filename), run_name='pistol_animation_dependency')
            for key, filename in {'text': 'rifle_p01_activate.py', 'rows': 'rifle_p01_setup.py',
                                  'columns': 'rifle_p01_transition_setup.py',
                                  'curves': 'rifle_animation_profile_setup.py',
                                  'graph': 'rifle_p01_graph_setup.py'}.items()}


def package(asset):
    return asset.get_path_name().split('.')[0] if asset else None


def load(path, cls=None):
    asset = unreal.load_asset(path)
    require(asset and (cls is None or isinstance(asset, cls)), 'Missing/wrong asset: ' + path)
    return asset


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode('utf-8')).hexdigest()


def src(name):
    return SOURCE + 'AZ_Pistol_' + name


def dst(name):
    return ANIMS + 'AS_Pistol_' + name


def rm_name(name):
    return RM_SPECIAL.get(name, name + '1')


def write_receipt(name, value):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    (OUTPUT / (name + '.json')).write_text(json.dumps(value, indent=2), encoding='utf-8')


def owned(path, manifest, cls, source=None, factory=None):
    expected = digest(manifest)
    if EAL.does_asset_exist(path):
        asset = load(path, cls)
        require(EAL.get_metadata_tag(asset, KEY) == OWNER
                and EAL.get_metadata_tag(asset, MANIFEST) == expected,
                'Unmanaged asset or changed manifest: ' + path)
        return asset
    if source:
        asset = EAL.duplicate_asset(source, path)
    else:
        folder, name = path.rsplit('/', 1)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, cls, factory)
    require(asset and isinstance(asset, cls), 'Asset creation failed: ' + path)
    EAL.set_metadata_tag(asset, KEY, OWNER)
    EAL.set_metadata_tag(asset, MANIFEST, expected)
    return asset


def save(asset):
    require(not isinstance(asset, unreal.AnimBlueprint), 'Use native editor save for AnimBP')
    require(EAL.save_loaded_asset(asset, only_if_is_dirty=False), 'Save failed: ' + package(asset))


def graph_readback(H):
    nodes = H['graph']['nodes']()
    result = {}
    for stance, (guid, member, field) in AIM_NODES.items():
        require(guid in nodes and nodes[guid]['class'] == 'AnimGraphNode_SequencePlayer',
                'Expected existing aim player missing: ' + stance)
        props = H['graph']['properties'](guid)
        paths = re.findall(r'/Game/[^\s\'"\\,)]+', props.get('Sequence', props.get('Node', '')))
        require(len(paths) == 1, 'Aim player literal is unavailable: ' + stance)
        sequence = load(paths[0], unreal.AnimSequence)
        result[stance] = {'guid': guid, 'member': member, 'field': field,
                          'literal': package(sequence), 'properties': props}
    return result


def column_records(H, chooser):
    result = []
    for index, column in enumerate(chooser.get_editor_property('columns_structs')):
        kind, values, key, rows = H['columns']['column_data'](column, H['text'])
        data = dict(values)
        binding = data.get('InputValue', '')
        chain_match = re.search(r'PropertyBindingChain=\(([^)]*)\)', binding)
        chain = re.findall(r'"([^"\n]+)"', chain_match.group(1)) if chain_match else []
        result.append({'index': index, 'kind': kind.rsplit('.', 1)[-1], 'chain': chain,
                       'rows': rows, 'data': data, 'column': column})
    return result


def column_index(columns, kind, leaf=None):
    matches = [c['index'] for c in columns if c['kind'] == kind
               and (leaf is None or (c['chain'] and c['chain'][-1] == leaf))]
    require(len(matches) == 1, 'Missing/ambiguous column: ' + str((kind, leaf)))
    return matches[0]


def permits_pistol(row_index, columns):
    for c in columns:
        if c['kind'] != 'GameplayTagColumn':
            continue
        tags = re.findall(r'TagName="([^"]+)"', c['rows'][row_index])
        if not tags:
            continue
        require(c['data'].get('TagMatchType') == 'Any'
                and c['data'].get('TagMatchDirection') == 'RowValueInInput',
                'Unknown tag-column match policy')
        match = any('Weapon.Pistol' == t or 'Weapon.Pistol'.startswith(t + '.') for t in tags)
        if c['data'].get('bInvertMatchingLogic') == 'True':
            match = not match
        if not match:
            return False
    return True


def row_asset(row):
    match = re.search(r'"out":\s*"Asset\[AnimSequence\]:([^"\n]+)"', row['line'])
    return match.group(1) if match else None


def transition_source(name, row, gait_col):
    """Only explicit semantic counterparts; missing authored turns remain shared."""
    if name and name.startswith('AZ_RTG_MH_W2_') and 'Turn_In_Place_' in name:
        # Current aimed TIP uses the walking-mode facing spring: never add RM yaw again.
        if 'Crouch' in name:
            return 'CrouchLoop', False, True
        return ('TurnL_90Loop' if '_L_' in name else 'TurnR_90Loop'), False, True
    if not name or not name.startswith('AnimPro_'):
        return None
    name = name.removeprefix('AnimPro_').removesuffix('_new')
    sprint = row['cells'].get(gait_col) == '= Sprint'
    if '135_' in name or name.startswith('Crouch_WalkFwdStart90_') or name.startswith('Crouch_WalkFwdStart180_'):
        return None
    if name.startswith('RunFwdTurn180_'):
        # Pack has no footed running pivot; retain its proven shared source and filters.
        return None
    if name.startswith('Jump'):
        if 'Start' in name:
            return 'Jump_Platformer_Start', False, False
        if 'Land' in name:
            return 'Jump_Platformer_Land', False, False
        return None
    if name in ('TurnLt90_Loop', 'TurnRt90_Loop'):
        return ('TurnL_90Loop' if 'Lt' in name else 'TurnR_90Loop'), False, True
    if name == 'RunFwdStart':
        name = 'SprintStart' if sprint else 'WalkFwdStart'
    elif name.startswith('RunFwdStop_'):
        name = name.replace('RunFwdStop_', 'SprintStop_' if sprint else 'WalkFwdStop_')
    elif name.startswith('RunFwdStart'):
        name = name.replace('RunFwdStart', 'WalkFwdStart')
    elif name in ('Idle2Crouch', 'Crouch2Idle'):
        return name, False, False
    # Every source is checked in the live registry before authoring.
    if EAL.does_asset_exist(src(rm_name(name))):
        return name, True, False
    return None


def chooser_plan(H):
    snapshot = H['rows']['chooser_snapshot'](CHOOSER)
    chooser = load(CHOOSER, unreal.ChooserTable)
    columns = column_records(H, chooser)
    state = column_index(columns, 'EnumColumn', 'SMState')
    stance = column_index(columns, 'EnumColumn', 'Stance')
    gait = column_index(columns, 'EnumColumn', 'Gait')
    reaction = column_index(columns, 'EnumColumn', 'Reaction')
    eligible = [r for r in snapshot['rows'] if permits_pistol(r['index'], columns)]
    def template(state_name, stance_name, gait_name=None):
        candidates = [r for r in eligible
                      if H['rows']['enum_cell'](r, state) == ('=', state_name)
                      and H['rows']['enum_cell'](r, stance) == ('=', stance_name)
                      and (gait_name is None or H['rows']['enum_cell'](r, gait) == ('=', gait_name))
                      and H['rows']['enum_cell'](r, reaction)[1] in (None, 'None')]
        require(candidates, 'No compatible live template: ' + repr((state_name, stance_name, gait_name)))
        return candidates[0]['index']
    rows, gated, shared = [], set(), []
    for stance_name, aiming, name in (('Standing', False, 'Idle_Relaxed'), ('Standing', True, 'Idle'),
                                       ('Crouching', False, 'CrouchLoop'), ('Crouching', True, 'CrouchLoop')):
        rows.append({'role': 'idle', 'source': name, 'target': dst(name),
                     'template': template('IdleLoop', stance_name), 'stance': stance_name,
                     'aim': aiming, 'rm': False, 'loop': True})
    for family, names in LOOPS.items():
        stance_name = 'Crouching' if family == 'Crouch' else 'Standing'
        gait_name = None if family == 'Crouch' else family
        for direction, name in zip(DIRECTIONS, names):
            rows.append({'role': 'loop', 'source': name, 'target': dst(name),
                         'template': template('LocomotionLoop', stance_name, gait_name),
                         'family': family, 'direction8': None if family == 'Sprint' else direction,
                         'rm': False, 'loop': True})
    for row in eligible:
        cmp, state_name = H['rows']['enum_cell'](row, state)
        _, reaction_name = H['rows']['enum_cell'](row, reaction)
        if reaction_name not in (None, 'None'):
            continue  # Obstacle reactions remain shared and are not overwritten by normal locomotion.
        if cmp == '=' and state_name in ('IdleLoop', 'IdleBreak', 'LocomotionLoop'):
            gated.add(row['index'])
            continue
        if cmp != '=':
            continue
        match = transition_source(row_asset(row), row, gait)
        if match:
            name, root_motion, looping = match
            rows.append({'role': 'transition', 'source': name, 'rm': root_motion, 'loop': looping,
                         'target': dst(name + ('_RM' if root_motion else '')),
                         'template': row['index'], 'state': state_name})
            gated.add(row['index'])
        elif state_name.casefold() in ('transitiontolocomotion', 'transitiontoidle', 'transitionstance',
                                       'idleturnleft', 'idleturnright', 'transitiontoinair', 'inairloop'):
            shared.append({'row': row['index'], 'asset': row_asset(row), 'state': state_name})
    # The pistol jump pack supplies an explicit looping fall; retain existing real-floor touchdown logic.
    air_templates = [r for r in snapshot['rows']
                     if H['rows']['enum_cell'](r, state) == ('=', 'InAirLoop')]
    require(air_templates, 'Main chooser has no InAirLoop template')
    rows.append({'role': 'air', 'source': 'FallingLoop', 'target': dst('FallingLoop'),
                 'template': air_templates[0]['index'], 'rm': False, 'loop': True})
    return {'rows': rows, 'gated': sorted(gated), 'shared_fallbacks': shared,
            'base_rows': len(snapshot['rows']), 'snapshot': snapshot,
            'column_text': [c.export_text() for c in chooser.get_editor_property('columns_structs')],
            'disabled': list(CU.get_chooser_disabled_rows(CHOOSER))}


def source_roster(plan):
    clips = {}
    for row in plan['rows']:
        source_name = rm_name(row['source']) if row['rm'] else row['source']
        spec = {'source': src(source_name), 'loop': row['loop'], 'rm': row['rm'], 'additive': False}
        require(row['target'] not in clips or clips[row['target']] == spec, 'Conflicting clip roles')
        clips[row['target']] = spec
    for name in ('Idle', 'Idle_Relaxed', 'CrouchLoop', 'ShootOnce', 'Reload_2'):
        clips.setdefault(dst(name), {'source': src(name), 'loop': name in ('Idle', 'Idle_Relaxed', 'CrouchLoop'),
                                     'rm': False, 'additive': False})
    for samples in AO.values():
        for name, _, _ in samples:
            clips[dst(name)] = {'source': src(name), 'loop': False, 'rm': False, 'additive': True}
    return clips


def audit(H, measure=False):
    plan = chooser_plan(H)
    clips = source_roster(plan)
    skeletons, source_data, contact_data = set(), {}, {}
    for target, spec in clips.items():
        sequence = load(spec['source'], unreal.AnimSequence)
        skeletons.add(package(sequence.get_editor_property('skeleton')))
        length = float(AL.get_sequence_length(sequence))
        require(length > 0 and float(sequence.get_editor_property('rate_scale')) > 0, 'Invalid source timing')
        require(bool(sequence.get_editor_property('enable_root_motion')) == spec['rm'],
                'Source root-motion variant differs: ' + spec['source'])
        expected_additive = unreal.AdditiveAnimationType.AAT_ROTATION_OFFSET_MESH_SPACE if spec['additive'] else unreal.AdditiveAnimationType.AAT_NONE
        require(sequence.get_editor_property('additive_anim_type') == expected_additive,
                'Source additive type differs: ' + spec['source'])
        source_data[target] = dict(spec, length=length,
                                  ref_pose=package(sequence.get_editor_property('ref_pose_seq')))
    require(len(skeletons) == 1 and 'metahuman_base_skel' in next(iter(skeletons)), 'Pistol skeleton mismatch')
    graph = graph_readback(H)
    pending = []
    cdo = unreal.get_default_object(unreal.AZ_WeaponAnimationProfile)
    for field in ('standing_aim_pose', 'crouching_aim_pose'):
        try:
            cdo.get_editor_property(field)
        except Exception:
            pending.append('Build/restart for profile.' + field)
    for family, names in LOOPS.items():
        for name in names:
            ipc, rm = load(src(name), unreal.AnimSequence), load(src(rm_name(name)), unreal.AnimSequence)
            require(package(rm.get_editor_property('skeleton')) in skeletons and rm.get_editor_property('enable_root_motion'),
                    'RM measurement partner mismatch: ' + name)
            require(abs(AL.get_sequence_length(ipc) - AL.get_sequence_length(rm)) < 0.0001,
                    'RM/IPC timelines differ: ' + name)
            if measure:
                settings = dict(H['curves']['DEFAULT_CONTACT_SETTINGS'])
                # Source readback found valid 25-33ms left-foot plants in sprint/left strafe run;
                # the rifle's40ms rejection erased them entirely. Keep >=20ms events in this pack.
                settings['minimum_plant_seconds'] = 0.02
                measured = H['curves']['contact_measurement'](rm, None, settings)
                length = measured['length_seconds']
                speed = math.hypot(*measured['root_delta'][:2]) / length
                require(speed > 5 and math.isfinite(speed), 'No depicted loop speed: ' + name)
                for curve in measured['curves'].values():
                    require(curve['plant_intervals'], 'No detected foot contact: ' + name)
                contact_data[dst(name)] = {'source_rm': src(rm_name(name)), 'speed_cm_s': speed,
                                           'measured': measured}
    load(SCHEMA, unreal.PoseSearchSchema)
    return {'status': 'audit', 'plan': plan, 'clips': clips, 'source_data': source_data,
            'skeleton': next(iter(skeletons)), 'graph': graph, 'pending': pending, 'contacts': contact_data,
            'warnings': ['Standing ShootOnce and Reload_2 also provide masked crouching actions; verify hand/torso feel.',
                         'Missing135-degree/crouch-turn starts and footed running pivots retain shared authored transitions.',
                         'Crouched aimed turn-in-place retains the pistol crouch idle while facing rotates; no crouched pistol turn steps exist.',
                         'Loop contacts/speed are measured from matching RM imports; visual foot timing still needs user verification.']}


def author_content(H, report):
    require(not report['pending'], '; '.join(report['pending']))
    require(report['contacts'], 'Measure loop counterparts before content authoring')
    assets = {}
    for target, spec in report['clips'].items():
        seq = owned(target, spec, unreal.AnimSequence, source=spec['source'])
        seq.set_editor_property('loop', spec['loop'])
        seq.set_editor_property('enable_root_motion', spec['rm'])
        seq.set_editor_property('rate_scale', 1.0)
        assets[target] = seq
    for stance, samples in AO.items():
        base = assets[dst(samples[0][0])]
        for name, _, _ in samples:
            seq = assets[dst(name)]
            seq.set_editor_property('additive_anim_type', unreal.AdditiveAnimationType.AAT_ROTATION_OFFSET_MESH_SPACE)
            seq.set_editor_property('ref_pose_type', unreal.AdditiveBasePoseType.ABPT_ANIM_FRAME)
            seq.set_editor_property('ref_pose_seq', base)
            seq.set_editor_property('ref_frame_index', 0)
    schema = load(SCHEMA, unreal.PoseSearchSchema)
    databases = {}
    for family, names in LOOPS.items():
        manifest = {'schema': SCHEMA, 'assets': [dst(n) for n in names], 'branch_in_owned': True}
        database = owned(FOLDER + '/PSD_Pistol_' + family, manifest, unreal.PoseSearchDatabase)
        database.set_editor_property('schema', schema)
        databases[family] = database
        for name in names:
            seq = assets[dst(name)]
            measured = report['contacts'][dst(name)]
            for curve_name, record in measured['measured']['curves'].items():
                if AL.does_curve_exist(seq, curve_name, unreal.RawCurveTrackTypes.RCT_FLOAT):
                    AL.remove_curve(seq, curve_name)
                AL.add_curve(seq, curve_name, unreal.RawCurveTrackTypes.RCT_FLOAT)
                AL.add_float_curve_keys(seq, curve_name, record['keys']['times'], record['keys']['values'])
            if AL.does_curve_exist(seq, 'Speed', unreal.RawCurveTrackTypes.RCT_FLOAT):
                AL.remove_curve(seq, 'Speed')
            AL.add_curve(seq, 'Speed', unreal.RawCurveTrackTypes.RCT_FLOAT)
            AL.add_float_curve_keys(seq, 'Speed', [0.0, measured['measured']['length_seconds']],
                                    [measured['speed_cm_s']] * 2)
            branches = H['curves']['branch_notifies'](seq)
            if branches:
                require(len(branches) == 1 and branches[0]['database'] == package(database), 'Foreign BranchIn on owned clip')
            else:
                require(PU.add_branch_in_notify(seq, database, 0.0, 0.0), 'BranchIn write failed: ' + name)
            save(seq)
        actual = H['curves']['db_members'](database)
        require(sorted(actual) == sorted(manifest['assets']), 'BranchIn database membership not synchronized: ' + family)
        require(PU.set_disable_reselection_on_database(database, True) == len(actual), 'DB reselection write failed')
        save(database)
    aim_offsets = {}
    for stance, samples in AO.items():
        manifest = {'samples': [(dst(n), x * 90.0, y * 90.0) for n, x, y in samples], 'skeleton': report['skeleton']}
        factory = unreal.AimOffsetBlendSpaceFactoryNew()
        factory.set_editor_property('target_skeleton', load(report['skeleton'], unreal.Skeleton))
        ao = owned(FOLDER + '/AO_Pistol_' + stance, manifest, unreal.AimOffsetBlendSpace, factory=factory)
        params = list(ao.get_editor_property('blend_parameters'))
        for index, title in enumerate(('Yaw', 'Pitch')):
            param = unreal.BlendParameter()
            for field, value in {'display_name': title, 'min': -90.0, 'max': 90.0, 'grid_num': 2,
                                 'snap_to_grid': True, 'wrap_input': False}.items():
                param.set_editor_property(field, value)
            params[index] = param
        ao.set_editor_property('blend_parameters', params)
        points = []
        for name, x, y in samples:
            point = unreal.BlendSample()
            for field, value in {'animation': assets[dst(name)], 'sample_value': unreal.Vector(x * 90, y * 90, 0),
                                 'rate_scale': 1.0, 'use_single_frame_for_blending': True,
                                 'frame_index_to_sample': 0}.items():
                point.set_editor_property(field, value)
            points.append(point)
        ao.set_editor_property('sample_data', points)
        require(AG.rebuild_blend_space(ao), 'Aim-offset rebuild failed: ' + stance)
        save(ao)
        aim_offsets[stance] = ao
    for seq in assets.values():
        save(seq)
    profile = owned(PROFILE, {'type': 'pistol', 'fire': dst('ShootOnce'), 'reload': dst('Reload_2')}, unreal.AZ_WeaponAnimationProfile)
    values = {'standing_aim_pose': assets[dst('Idle')], 'crouching_aim_pose': assets[dst('CrouchLoop')],
              'single_fire_animation': assets[dst('ShootOnce')], 'automatic_fire_animation': None,
              'crouching_single_fire_animation': assets[dst('ShootOnce')], 'crouching_automatic_fire_animation': None,
              'standing_aim_offset': aim_offsets['Standing'], 'crouching_aim_offset': aim_offsets['Crouching'],
              'relaxed_upper_body_pose': None, 'fire_animation_slot': 'RifleFire', 'use_air_loop': True,
              'phase_locked_transition_to_loop': False, 'use_unarmed_leans': False, 'use_loop_play_rate': True,
              'speed_curve_name': 'Speed', 'play_rate_loop_assets': [assets[dst(n)] for names in LOOPS.values() for n in names],
              'reload_animation_play_rate': 1.0}
    for field in ('standing_reload_animation', 'standing_aim_reload_animation',
                  'crouching_reload_animation', 'crouching_aim_reload_animation'):
        values[field] = assets[dst('Reload_2')]
    for field in ('walk_loco_database', 'run_loco_database', 'strafe_walk_database', 'strafe_run_database', 'strafe_crouch_database'):
        values[field] = None  # Chosen raw clips own their BranchIn vocabulary, matching current rifle behavior.
    for field, value in values.items():
        profile.set_editor_property(field, value)
    save(profile)
    report['database_paths'] = [package(d) for d in databases.values()]
    report['status'] = 'content_authored'


def tag_column(H, inverted, marked, total):
    rows = ['(GameplayTags=((TagName="Weapon.Pistol")))' if i in marked else '()' for i in range(total)]
    encoded = ('/Script/Chooser.GameplayTagColumn(InputValue=/Script/Chooser.GameplayTagContextProperty('
               'Binding=(PropertyBindingChain=("ChooserContext","OwnedTags"),ContextIndex=0)),'
               'TagMatchType=Any,TagMatchDirection=RowValueInInput,bMatchExact=False,bInvertMatchingLogic='
               + ('True' if inverted else 'False') + ',RowValues=(' + ','.join(rows) + '))')
    column = unreal.InstancedStruct()
    require(column.import_text(encoded), 'Pistol tag column import failed')
    return column


def author_chooser(H, report):
    plan = report['plan']
    chooser = load(CHOOSER, unreal.ChooserTable)
    require(not EAL.get_metadata_tag(chooser, KEY), 'Chooser already authored; run verify instead of append again')
    require(CU.get_row_count(CHOOSER) == plan['base_rows'], 'Chooser changed after audit')
    columns = column_records(H, chooser)
    indexes = {leaf: column_index(columns, kind, leaf) for leaf, kind in (
        ('SMState', 'EnumColumn'), ('Stance', 'EnumColumn'), ('Gait', 'EnumColumn'),
        ('MovementDirection', 'EnumColumn'), ('MovementDirection8', 'EnumColumn'),
        ('bIsAiming', 'BoolColumn'), ('bLeftFootDown', 'BoolColumn'), ('bStrafe', 'BoolColumn'),
        ('bIsMoving', 'BoolColumn'), ('bMovingTransition', 'BoolColumn'), ('bJustLanded', 'BoolColumn'))}
    indexes['states'] = column_index(columns, 'MultiEnumColumn', 'SMState')
    indexes['directions'] = column_index(columns, 'MultiEnumColumn', 'MovementDirection')
    indexes['outputs'] = column_index(columns, 'OutputStructColumn')
    # Preload ALL result assets before the first row write; a partial run must never append twice.
    for row in plan['rows']:
        load(row['target'], unreal.AnimSequence)
    EAL.set_metadata_tag(chooser, KEY, OWNER)
    EAL.set_metadata_tag(chooser, STATE, json.dumps({'status': 'authoring', 'base_rows': plan['base_rows'],
                                                  'plan_hash': digest(plan['rows'])}))
    def enum(index, leaf, value, comparison=0):
        require(CU.set_cell_enum_on_sub(CHOOSER, '', index, indexes[leaf], value, comparison), 'Enum write failed: ' + leaf)
    def boolean(index, leaf, value):
        require(CU.set_cell_bool_on_sub(CHOOSER, '', index, indexes[leaf], value), 'Bool write failed: ' + leaf)
    appended = []
    for row in plan['rows']:
        index = CU.duplicate_row_on_sub(CHOOSER, '', row['template'])
        require(index == plan['base_rows'] + len(appended), 'Chooser append returned unexpected row')
        require(CU.set_cell_asset_on_sub(CHOOSER, '', index, load(row['target'], unreal.AnimSequence)), 'Chooser asset write failed')
        appended.append(index)
        if row['role'] in ('idle', 'loop', 'air'):
            enum(index, 'MovementDirection', 'F', 2)
            enum(index, 'MovementDirection8', row.get('direction8') or 'F', 0 if row.get('direction8') else 2)
            require(CU.set_cell_multi_enum_on_sub(CHOOSER, '', index, indexes['directions'], []), 'Direction mask reset failed')
            for leaf in ('bIsAiming', 'bLeftFootDown', 'bStrafe', 'bIsMoving', 'bMovingTransition', 'bJustLanded'):
                boolean(index, leaf, 2)
        if row['role'] == 'idle':
            enum(index, 'SMState', 'IdleLoop', 2)
            require(CU.set_cell_multi_enum_on_sub(CHOOSER, '', index, indexes['states'], ['IdleLoop', 'IdleBreak']), 'Idle state mask failed')
            boolean(index, 'bIsAiming', 1 if row['aim'] else 0)
        elif row['role'] == 'air':
            enum(index, 'SMState', 'InAirLoop')
            enum(index, 'Stance', 'Standing', 2)
            enum(index, 'Gait', 'Walk', 2)
            require(CU.set_cell_multi_enum_on_sub(CHOOSER, '', index, indexes['states'], []), 'Air state mask reset failed')
        use_mm = row['role'] == 'loop'
        require(CU.set_cell_output_struct_field_on_sub(CHOOSER, '', index, indexes['outputs'], 'bUseMM', 'True' if use_mm else 'False'), 'MM output write failed')
        require(CU.set_cell_output_struct_field_on_sub(CHOOSER, '', index, indexes['outputs'], 'StartTime', '0.0'), 'StartTime write failed')
    # Every existing column cell remains byte-for-byte equivalent for all original rows.
    current = list(chooser.get_editor_property('columns_structs'))
    new_columns = []
    for index, column in enumerate(current):
        kind, _, _, values = H['columns']['column_data'](column, H['text'])
        old = columns[index]['rows']
        require(values[:plan['base_rows']] == old, 'Original chooser cells changed')
        if kind.endswith('.GameplayTagColumn'):
            values = [v if i < plan['base_rows'] else '()' for i, v in enumerate(values)]
            column = H['columns']['with_rows'](column, values, H['text'])
        new_columns.append(column)
    random_index = column_index(columns, 'RandomizeColumn')
    total = CU.get_row_count(CHOOSER)
    new_columns[random_index:random_index] = [tag_column(H, False, set(appended), total),
                                             tag_column(H, True, set(plan['gated']), total)]
    chooser.set_editor_property('columns_structs', new_columns)
    EAL.set_metadata_tag(chooser, KEY, OWNER)
    EAL.set_metadata_tag(chooser, STATE, json.dumps({'status': 'complete', 'base_rows': plan['base_rows'], 'appended': appended,
                                                  'gated': plan['gated'], 'plan_hash': digest(plan['rows'])}))
    require(CU.compile_and_save(CHOOSER), 'Chooser compile/save failed')
    report['status'] = 'chooser_authored'
    report['appended'] = appended


def author_graph(H, report):
    require(not report['pending'], '; '.join(report['pending']))
    rifle = load(RIFLE_PROFILE, unreal.AZ_WeaponAnimationProfile)
    pistol = load(PROFILE, unreal.AZ_WeaponAnimationProfile)
    for stance, data in report['graph'].items():
        literal = load(data['literal'], unreal.AnimSequence)
        current = rifle.get_editor_property(data['field'])
        require(current is None or current == literal, 'Rifle aim pose already changed: ' + stance)
        require(pistol.get_editor_property(data['field']) is not None, 'Pistol aim pose unassigned: ' + stance)
        rifle.set_editor_property(data['field'], literal)
    save(rifle)  # Preserve exact prior rifle literals BEFORE changing shared bindings.
    for stance, data in report['graph'].items():
        require(AG.set_pin_binding(BLUEPRINT, data['guid'], 'Sequence', data['member']), 'Aim pose binding failed: ' + stance)
        require(H['graph']['properties'](data['guid']).get('Binding:Sequence') == data['member'] + ' (prop)',
                'Aim pose binding readback failed: ' + stance)
    report['status'] = 'graph_authored_native_compile_save_required'
    report['compile_after_return'] = [BLUEPRINT + '.AZ_ABP_MoverHero_MHC']


def verify(H):
    profile = load(PROFILE, unreal.AZ_WeaponAnimationProfile)
    rifle = load(RIFLE_PROFILE, unreal.AZ_WeaponAnimationProfile)
    for field, name in [('standing_aim_pose', 'Idle'), ('crouching_aim_pose', 'CrouchLoop'),
                        ('single_fire_animation', 'ShootOnce'), ('crouching_single_fire_animation', 'ShootOnce'),
                        ('standing_reload_animation', 'Reload_2'), ('standing_aim_reload_animation', 'Reload_2'),
                        ('crouching_reload_animation', 'Reload_2'), ('crouching_aim_reload_animation', 'Reload_2')]:
        require(package(profile.get_editor_property(field)) == dst(name), 'Profile readback failed: ' + field)
    require(profile.get_editor_property('automatic_fire_animation') is None
            and profile.get_editor_property('crouching_automatic_fire_animation') is None, 'Pistol must have no auto fire clip')
    for stance, samples in AO.items():
        ao = load(FOLDER + '/AO_Pistol_' + stance, unreal.AimOffsetBlendSpace)
        points = list(ao.get_editor_property('sample_data'))
        actual = {(package(p.get_editor_property('animation')), float(p.get_editor_property('sample_value').x),
                   float(p.get_editor_property('sample_value').y)) for p in points}
        require(len(points) == 9 and actual == {(dst(n), x * 90.0, y * 90.0) for n, x, y in samples}, 'AO sample mismatch')
        for name, _, _ in samples:
            seq = load(dst(name), unreal.AnimSequence)
            require(package(seq.get_editor_property('ref_pose_seq')) == dst(samples[0][0]), 'Foreign additive base remains')
    for family, names in LOOPS.items():
        db = load(FOLDER + '/PSD_Pistol_' + family, unreal.PoseSearchDatabase)
        require(sorted(H['curves']['db_members'](db)) == sorted(dst(n) for n in names), 'DB member mismatch: ' + family)
        for name in names:
            seq = load(dst(name), unreal.AnimSequence)
            require(seq.get_editor_property('loop') and not seq.get_editor_property('enable_root_motion'), 'Loop flags mismatch')
            require(all(AL.does_curve_exist(seq, c, unreal.RawCurveTrackTypes.RCT_FLOAT)
                        for c in ('Speed', 'contact_l', 'contact_r')), 'Required loop curve missing')
    chooser = load(CHOOSER, unreal.ChooserTable)
    require(EAL.get_metadata_tag(chooser, KEY) == OWNER, 'Pistol chooser integration missing')
    state = json.loads(EAL.get_metadata_tag(chooser, STATE))
    require(state.get('status') == 'complete', 'Interrupted chooser authoring requires backup review; do not append again')
    require(CU.get_row_count(CHOOSER) == state['base_rows'] + len(state['appended']), 'Chooser row count changed after authoring')
    graph = graph_readback(H)
    for data in graph.values():
        require(package(rifle.get_editor_property(data['field'])) == data['literal'], 'Rifle original aim pose not retained')
        require(data['properties'].get('Binding:Sequence') == data['member'] + ' (prop)', 'Dynamic aim pose binding missing')
    baseline_path = OUTPUT / 'chooser.json'
    require(baseline_path.is_file(), 'Chooser baseline receipt missing')
    baseline = json.loads(baseline_path.read_text(encoding='utf-8'))['plan']
    current_columns = column_records(H, chooser)
    # Compile may reorder columns. Match their complete original-row content and binding, not positions.
    used = set()
    for encoded in baseline['column_text']:
        column = unreal.InstancedStruct()
        require(column.import_text(encoded), 'Cannot decode original chooser column')
        kind, values, key, original_rows = H['columns']['column_data'](column, H['text'])
        signature = [(k, v) for k, v in values if k != key]
        candidates = []
        for c in current_columns:
            if c['index'] in used:
                continue
            other_kind, other_values, other_key, other_rows = H['columns']['column_data'](c['column'], H['text'])
            if (kind == other_kind and signature == [(k, v) for k, v in other_values if k != other_key]
                    and original_rows == other_rows[:baseline['base_rows']]):
                candidates.append(c['index'])
        require(candidates, 'An original chooser column/binding/cell changed')
        used.add(candidates[0])
    now = H['rows']['chooser_snapshot'](CHOOSER)
    require([row_asset(r) for r in now['rows'][:baseline['base_rows']]]
            == [row_asset(r) for r in baseline['snapshot']['rows']], 'Original chooser assets changed')
    require(list(CU.get_chooser_disabled_rows(CHOOSER))[:baseline['base_rows']] == baseline['disabled'],
            'Original chooser enablement changed')
    return {'status': 'verified_assets', 'chooser': state, 'graph': graph,
            'pending': ['Build all four pistol PoseSearch indexes and confirm success.',
                        'User gameplay/visual checks remain.']}


def main(stage='audit', measure=False):
    require(stage in ('audit', 'content', 'chooser', 'graph', 'verify'), 'Unknown stage')
    report = {'status': 'starting', 'stage': stage}
    try:
        H = libraries()
        if stage == 'verify':
            report = verify(H)
        else:
            if stage == 'graph':
                # The chooser has already gated its old pistol-compatible rows.
                # Graph binding needs the live graph and native fields, not a new
                # migration plan inferred from those now-replaced templates.
                report = {'status': 'audit', 'graph': graph_readback(H), 'pending': []}
                native = unreal.get_default_object(unreal.AZ_WeaponAnimationProfile)
                for field in ('standing_aim_pose', 'crouching_aim_pose'):
                    native.get_editor_property(field)
            else:
                report = audit(H, measure=measure or stage == 'content')
            report['stage'] = stage
            if stage != 'audit':
                require(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None,
                        'Stop PIE before pistol animation authoring')
                # Shared source packages must be clean; backups include every existing target to be edited.
                paths = [CHOOSER] if stage == 'chooser' else ([BLUEPRINT, RIFLE_PROFILE] if stage == 'graph' else
                         [p for p in report['clips'] if EAL.does_asset_exist(p)]
                         + [p for p in [PROFILE] + [FOLDER + '/AO_Pistol_' + s for s in AO]
                            + [FOLDER + '/PSD_Pistol_' + f for f in LOOPS] if EAL.does_asset_exist(p)])
                if paths:
                    report['backup'] = H['rows']['backup_existing'](paths)
                {'content': author_content, 'chooser': author_chooser, 'graph': author_graph}[stage](H, report)
        return report
    except Exception as error:
        report['status'] = 'failed'
        report['error'] = str(error)
        raise
    finally:
        write_receipt(stage, report)
        print('[PistolAnimation] ' + json.dumps({'stage': stage, 'status': report['status'],
                                               'error': report.get('error'), 'pending': report.get('pending', [])}))
        gc.collect()
