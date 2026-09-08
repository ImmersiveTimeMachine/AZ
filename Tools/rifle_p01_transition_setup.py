# @Description: Audit or prepare existing P01 ground transitions in the main character chooser.
"""main() audits live assets; prepare=True explicitly writes after full preflight.

Uses existing SurvivalMan rm_W2 imports. No retargets, animation duplication,
PIE, tests, Blueprint compilation, socket edits or montage/graph changes.
Preserves the main chooser's original155 rows/cells except reviewed !Rifle tag
gates. Appends112 P01 selections and9 exact Sprint fallback clones (276 total).
Role-equivalent starts preserve existing moving filters; dedicated moving Run180
footed pivots and all explicit Sprint rows remain shared original content.
"""
import gc
import hashlib
import json
import math
from pathlib import Path
import runpy

import unreal


PROJECT = Path('C:/UnrealEngine/Games/AZ')
MAIN = '/Game/AZ/Blueprints/Animation/MotionMatching/CHT_v2_CharacterAnimations'
ANIMS = '/Game/AZ/NoWeapons/RootMotions/'
OLD_ANIMS = '/Game/Assets/RTG_AZ/MovementAnimsetPro/'
MESH = '/Game/SurvivalMan/Meshes/SKM_SurvivalMan_Mesh1'
OWNER = 'rifle_p01_transition_setup:v1'
OWNER_KEY = 'AZ.RifleP01Transitions.Owner'
STATE_KEY = 'AZ.RifleP01Transitions.State'
MANIFEST_KEY = 'AZ.RifleP01Transitions.Manifest'
BASE_ROWS, TOTAL_ROWS = 155, 276
EAL = unreal.EditorAssetLibrary
CU = unreal.AZ_ChooserUtils
AL = unreal.AnimationLibrary
COL = {'state': 0, 'stance': 1, 'gait': 2, 'direction': 3, 'foot': 4,
       'direction8': 5, 'aim': 6, 'idle_states': 7, 'positive': 8, 'negative': 9,
       'random': 10, 'start': 11, 'moving': 12, 'landed': 13, 'is_moving': 14,
       'outputs': 15, 'strafe': 16, 'reaction': 17}
INITIAL_OLD = [7, 9] + list(range(12, 24)) + list(range(42, 47)) + list(range(65, 71))
STOPS_OLD = [5, 6, 10, 11, 40, 41] + list(range(59, 65)) + list(range(71, 77))
GATED_OLD = sorted(INITIAL_OLD + STOPS_OLD + [51, 52])
SPRINT_FALLBACK = list(range(59, 68))
STARTS = ('F', 'B', 'L', 'R', 'L90_Fwd', 'R90_Fwd', 'L135_Fwd', 'R135_Fwd', 'L180_Fwd', 'R180_Fwd')
DIRECTION = {'F': ['F'], 'B': ['B'], 'L': ['LL', 'LR'], 'R': ['RL', 'RR']}
FAMILIES = (
    ('WalkExplore', 'Standing', 'Walk', False, 'W2_Stand_Relaxed_To_Walk_', 'W2_Walk_', 'Stand_Relaxed'),
    ('WalkAim', 'Standing', 'Walk', True, 'W2_Stand_Aim_To_Walk_Aim_', 'W2_Walk_Aim_', 'Stand_Aim'),
    ('JogExplore', 'Standing', 'Run', False, 'W2_Stand_Relaxed_To_Jog_', 'W2_Jog_', 'Stand_Relaxed'),
    ('JogAim', 'Standing', 'Run', True, 'W2_Stand_Aim_To_Jog_Aim_', 'W2_Jog_Aim_', 'Stand_Aim'),
    ('CrouchExplore', 'Crouching', None, False, 'W2_Crouch_To_CrouchWalk_', 'W2_CrouchWalk_', 'Crouch'),
    ('CrouchAim', 'Crouching', None, True, 'W2_Crouch_Aim_To_CrouchWalk_Aim_', 'W2_CrouchWalk_Aim_', 'Crouch_Aim'),
)


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode('utf-8')).hexdigest()


def libraries():
    return tuple(runpy.run_path(str(PROJECT / 'Tools' / filename), run_name='rifle_transition_dependency')
                 for filename in ('rifle_p01_setup.py', 'rifle_p01_activate.py', 'rifle_p01_consolidate_chooser.py'))


def start_template(stance, gait, suffix):
    if stance == 'Crouching':
        return {'F': 46, 'B': 68, 'L': 69, 'R': 70, 'L90_Fwd': 42, 'R90_Fwd': 43,
                'L135_Fwd': 46, 'R135_Fwd': 46, 'L180_Fwd': 44, 'R180_Fwd': 45}[suffix]
    if suffix in ('B', 'L', 'R'):
        return {'B': 65, 'L': 66, 'R': 67}[suffix]
    forward = {'F': 7, 'L90_Fwd': 12, 'R90_Fwd': 13, 'L135_Fwd': 14,
               'R135_Fwd': 15, 'L180_Fwd': 16, 'R180_Fwd': 17}
    running = {'F': 9, 'L90_Fwd': 18, 'R90_Fwd': 19, 'L135_Fwd': 20,
               'R135_Fwd': 21, 'L180_Fwd': 22, 'R180_Fwd': 23}
    return (running if gait == 'Run' else forward)[suffix]


def stop_template(stance, gait, direction, foot):
    if stance == 'Crouching':
        first = {'F': 40, 'B': 71, 'L': 73, 'R': 75}[direction]
    else:
        first = {'F': 10 if gait == 'Run' else 5, 'B': 59, 'L': 61, 'R': 63}[direction]
    return first + (1 if foot == 'LU' else 0)


def roster():
    rows = []
    for family, stance, gait, aim, start, stop, idle in FAMILIES:
        for suffix in STARTS:
            turning = suffix.endswith('_Fwd')
            rows.append(dict(role='start', family=family, asset=ANIMS + 'rm_' + start + suffix,
                             stance=stance, gait=gait, aim=aim, state='TransitionToLocomotion',
                             start=suffix.removesuffix('_Fwd') if turning else 'Fwd',
                             directions=[] if turning else DIRECTION[suffix],
                             template=start_template(stance, gait, suffix)))
        for direction in ('F', 'B', 'L', 'R'):
            for foot in ('RU', 'LU'):
                rows.append(dict(role='stop', family=family, asset=ANIMS + 'rm_' + stop + direction + '_to_' + idle + '_' + foot,
                                 stance=stance, gait=gait, aim=aim, state='TransitionToIdle',
                                 start=None, directions=DIRECTION[direction], foot=foot,
                                 template=stop_template(stance, gait, direction, foot)))
    for name, stance, aim, template in (
        ('W2_Stand_Relaxed_To_Crouch_v2', 'Crouching', False, 51),
        ('W2_Crouch_To_Stand_Relaxed_v2', 'Standing', False, 52),
        ('W2_Stand_Aim_To_Crouch_Aim_v2', 'Crouching', True, 51),
        ('W2_Crouch_Aim_To_Stand_Aim_v2', 'Standing', True, 52)):
        rows.append(dict(role='stance', family='Stance', asset=ANIMS + 'rm_' + name,
                         stance=stance, gait=None, aim=aim, state='TransitionStance', start=None,
                         directions=[], template=template))
    require(len(rows) == 112 and len({r['asset'] for r in rows}) == 112, 'Explicit transition roster changed')
    return rows


def column_data(column, text):
    kind, values = text['fragment_parts'](column.export_text())
    key = 'RowValuesWithAny' if kind == '/Script/Chooser.BoolColumn' else 'RowValues'
    require(key in dict(values), 'Unsupported chooser column row storage: ' + kind)
    rows = text['split_top_level'](text['parenthesized'](dict(values)[key]))
    return kind, values, key, rows


def with_rows(column, rows, text):
    kind, values, key, _ = column_data(column, text)
    encoded = kind + text['encode_fields'](text['replace_field'](values, key, '(' + ','.join(rows) + ')'))
    value = unreal.InstancedStruct()
    require(value.import_text(encoded), 'Column ImportText failed: ' + kind)
    require(len(column_data(value, text)[3]) == len(rows), 'Column row-count readback failed: ' + kind)
    return value


def tag_column(inverted, marked, total, text):
    require(all(0 <= index < total for index in marked), 'Invalid tag row index')
    rows = ['(GameplayTags=((TagName="Weapon.Rifle")))' if index in marked else '()' for index in range(total)]
    encoded = ('/Script/Chooser.GameplayTagColumn(InputValue=/Script/Chooser.GameplayTagContextProperty('
               'Binding=(PropertyBindingChain=("ChooserContext","OwnedTags"),ContextIndex=0)),'
               'TagMatchType=Any,TagMatchDirection=RowValueInInput,bMatchExact=False,bInvertMatchingLogic='
               + ('True' if inverted else 'False') + ',RowValues=(' + ','.join(rows) + '))')
    result = unreal.InstancedStruct()
    require(result.import_text(encoded), 'Tag column ImportText failed')
    found = tag_rows(result, total, text, inverted)
    require(found == set(marked), 'Tag column membership readback failed')
    return result


def tag_rows(column, total, text, inverted):
    kind, values, _, rows = column_data(column, text)
    require(kind == '/Script/Chooser.GameplayTagColumn' and len(rows) == total, 'Invalid complete tag column')
    data = dict(values)
    require(data.get('bInvertMatchingLogic') == ('True' if inverted else 'False')
            and data.get('TagMatchType') == 'Any' and data.get('TagMatchDirection') == 'RowValueInInput'
            and data.get('bMatchExact') == 'False', 'Unexpected tag matching policy')
    found = set()
    for index, row in enumerate(rows):
        payload = dict(text['fields'](row)).get('GameplayTags', '')
        entries = text['split_top_level'](text['parenthesized'](payload))
        if entries:
            require(len(entries) == 1 and dict(text['fields'](entries[0])).get('TagName', '').strip('"') == 'Weapon.Rifle',
                    'Unexpected tag in chooser filter')
            found.add(index)
    return found


def asset_audit(seq, mesh):
    length = float(AL.get_sequence_length(seq))
    require(length > 0.15, 'Sequence is too short for the native .15-second transition handoff: ' + seq.get_name())
    rate_scale = float(seq.get_editor_property('rate_scale'))
    require(math.isfinite(rate_scale) and abs(rate_scale - 1.0) < 1e-6,
            'Transition RateScale must be1: native timer uses unscaled sequence length: ' + seq.get_name())
    require(seq.get_editor_property('skeleton') == mesh.get_editor_property('skeleton'), 'Existing rm_W2 skeleton mismatch')
    require(seq.get_editor_property('additive_anim_type') == unreal.AdditiveAnimationType.AAT_NONE, 'Transition must be non-additive')
    require(not seq.get_editor_property('loop'), 'Ground transition must not loop: ' + seq.get_name())
    imported = seq.get_editor_property('asset_import_data')
    filename = str(imported.get_first_filename()) if imported else ''
    require('Rifle_01_PRO_v27A' in filename and 'W2_' in filename, 'Expected verified Rifle_01 import provenance: ' + seq.get_name())
    options = unreal.AnimPoseEvaluationOptions()
    for name, value in {'evaluation_type': unreal.AnimDataEvalType.RAW, 'should_retarget': False,
                        'extract_root_motion': False, 'incorporate_root_motion_into_pose': True,
                        'optional_skeletal_mesh': mesh}.items():
        options.set_editor_property(name, value)
    times = sorted(set([0.0, min(0.01, length), min(1.0 / 30.0, length), min(0.05, length), length / 2.0, length]))
    samples = []
    for time in times:
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, time, options)
        require(unreal.AnimPoseExtensions.is_valid(pose), 'Raw transition pose evaluation failed')
        bones = {str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)}
        require({'root', 'foot_l', 'foot_r'} <= bones, 'Required root/foot bones are absent')
        values = {'time': time}
        for bone in ('root', 'foot_l', 'foot_r'):
            transform = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
            p = transform.translation
            values[bone] = [float(p.x), float(p.y), float(p.z)]
            if bone == 'root':
                values['root_yaw'] = float(transform.rotation.rotator().yaw)
        samples.append(values)
    first, last = samples[0], samples[-1]
    delta = [last['root'][i] - first['root'][i] for i in range(3)]
    entry = {}
    probe = next(s for s in samples if s['time'] >= 1.0 / 30.0 - 1e-6)
    for bone in ('foot_l', 'foot_r'):
        entry[bone] = {'height_at_zero_cm': first[bone][2],
                       'mean_speed_first_frame_cm_s': math.dist(first[bone], probe[bone]) / probe['time'],
                       'height_above_final_rest_foot_cm': first[bone][2] - last[bone][2]}
    curves = {}
    for name in AL.get_animation_curve_names(seq, unreal.RawCurveTrackTypes.RCT_FLOAT):
        times_c, values_c = AL.get_float_keys(seq, name)
        curves[str(name).casefold()] = [list(map(float, times_c)), list(map(float, values_c))]
    return {'length_seconds': length, 'rate_scale': rate_scale, 'source_file': filename, 'skeleton': str(seq.get_editor_property('skeleton').get_path_name()),
            'enable_root_motion': bool(seq.get_editor_property('enable_root_motion')), 'loop': False,
            'force_root_lock': bool(seq.get_editor_property('force_root_lock')),
            'root_delta_cm': delta, 'root_yaw_delta_degrees': (last['root_yaw'] - first['root_yaw'] + 180) % 360 - 180,
            'entry_feet': entry, 'root_and_foot_samples': samples, 'curves_hash': digest(curves)}


def verify_complete(asset, manifest, setup, text, consolidation):
    snapshot = setup['chooser_snapshot'](MAIN)
    require(len(snapshot['rows']) == TOTAL_ROWS, 'Completed transition chooser row count changed')
    columns = list(asset.get_editor_property('columns_structs'))
    require(len(columns) == 19, 'Completed chooser column count changed')
    for old, new in enumerate(manifest['old_to_new']):
        rows = column_data(columns[new], text)[3]
        require(len(rows) == TOTAL_ROWS and digest(rows[:BASE_ROWS]) == manifest['preserved_prefix_hashes'][old],
                'Original chooser cells changed: column ' + str(old))
    require([consolidation['result_description'](row) for row in snapshot['rows'][:BASE_ROWS]] == manifest['original_results'],
            'An original chooser result changed')
    require(setup['chooser_disabled_rows'](MAIN) == manifest['original_disabled'] + [False] * (TOTAL_ROWS - BASE_ROWS),
            'Original/new chooser disabled flags changed')
    mapped = manifest['mapped_columns']
    direction_values = column_data(columns[mapped['direction4']], text)[3]
    require(len(direction_values) == TOTAL_ROWS and all(
        int(dict(text['fields'](value)).get('Value', '0')) == 0 for value in direction_values[:BASE_ROWS]),
        'The new direction filter must remain Any on every original row')
    require(tag_rows(columns[mapped['positive']], TOTAL_ROWS, text, False) == set(manifest['positive_rows']), 'Positive tag routing changed')
    require(tag_rows(columns[mapped['negative']], TOTAL_ROWS, text, True) == set(manifest['negative_rows']), 'Negative tag routing changed')
    for row, spec in zip(snapshot['rows'][BASE_ROWS:], manifest['new_rows']):
        require(consolidation['result_description'](row) == 'Asset[AnimSequence]:' + spec['asset'].rsplit('/', 1)[1],
                'Appended transition result mismatch')
        template = snapshot['rows'][spec['template']]
        if spec['role'] == 'sprint_fallback':
            require(setup['enum_cell'](row, mapped['gait']) == ('=', 'Sprint'), 'Sprint fallback gait differs')
            unchanged = [key for key in COL if key not in ('gait', 'positive', 'negative')]
        else:
            seq = setup['load'](spec['asset'], unreal.AnimSequence)
            require(bool(seq.get_editor_property('enable_root_motion')) == (spec['role'] != 'stance')
                    and not seq.get_editor_property('loop')
                    and abs(float(seq.get_editor_property('rate_scale')) - 1.0) < 1e-6,
                    'Prepared transition playback flags changed: ' + spec['asset'])
            for key in ('state', 'stance', 'gait'):
                comparison, value = setup['enum_cell'](row, mapped[key])
                expected = spec[key]
                require((comparison == 'Any' if expected is None else comparison == '=' and value.casefold() == expected.casefold()),
                        'Appended transition enum differs: ' + key)
            require(setup['enum_cell'](row, mapped['direction'])[0] == 'Any', 'Old direction filter unexpectedly restricts P01 row')
            require(row['cells'].get(mapped['aim']) == ('True' if spec['aim'] else 'False'), 'P01 aim filter differs')
            mask = int(dict(text['fields'](direction_values[row['index']])).get('Value', '0'))
            expected_mask = sum(1 << {'F': 0, 'B': 1, 'LL': 2, 'LR': 3, 'RL': 4, 'RR': 5}[name] for name in spec['directions'])
            require(mask == expected_mask, 'P01 four-way direction mask differs')
            if spec['role'] == 'start':
                require(setup['enum_cell'](row, mapped['start']) == ('=', spec['start']), 'P01 start bucket differs')
            unchanged = ['foot', 'direction8', 'idle_states', 'random', 'moving', 'landed', 'is_moving', 'outputs', 'strafe', 'reaction']
            if spec['role'] != 'start':
                unchanged.append('start')
        for key in unchanged:
            require(row['cells'].get(mapped[key]) == template['cells'].get(mapped[key]),
                    'Source template filter changed on appended row: ' + key)
    require(mapped['random'] > max(index for key, index in mapped.items() if key not in ('outputs', 'random')),
            'A filter remains after Randomize')
    return snapshot


def audit():
    setup, text, consolidation = libraries()
    asset = setup['load'](MAIN, unreal.ChooserTable)
    owner = EAL.get_metadata_tag(asset, OWNER_KEY)
    if owner:
        require(owner == OWNER and EAL.get_metadata_tag(asset, STATE_KEY) == 'complete',
                'Unknown or interrupted transition setup; inspect its backup before rerun')
        manifest = json.loads(EAL.get_metadata_tag(asset, MANIFEST_KEY))
        verify_complete(asset, manifest, setup, text, consolidation)
        return {'mode': 'audit', 'status': 'already_prepared', 'already_prepared': True, 'manifest': manifest}
    consolidation['audit']()  # Validates current155 original results/cells and native tag routes.
    snapshot = setup['chooser_snapshot'](MAIN)
    columns = list(asset.get_editor_property('columns_structs'))
    require(len(snapshot['rows']) == BASE_ROWS and len(columns) == 18, 'Expected the live155-row18-column master')
    require(snapshot['context_count'] == 2, 'Native chooser context contract changed')
    require(len(GATED_OLD) == 45, 'Covered source-row plan changed')
    expected_types = ['EnumColumn'] * 4 + ['BoolColumn', 'EnumColumn', 'BoolColumn', 'MultiEnumColumn',
                      'GameplayTagColumn', 'GameplayTagColumn', 'RandomizeColumn', 'EnumColumn',
                      'BoolColumn', 'BoolColumn', 'BoolColumn', 'OutputStructColumn', 'BoolColumn', 'EnumColumn']
    require([c['type'] for c in snapshot['columns']] == expected_types, 'Master column layout changed; explicit remapping required')
    for column in columns:
        require(len(column_data(column, text)[3]) == BASE_ROWS, 'Incomplete existing column row array')
    specs = roster()
    for spec in specs:
        row = snapshot['rows'][spec['template']]
        comp, state = setup['enum_cell'](row, COL['state'])
        require(comp == '=' and state.casefold() == spec['state'].casefold(), 'Source template phase differs: ' + str(spec['template']))
        moving = row['cells'].get(COL['moving'])
        if spec['role'] == 'start':
            require(moving in ('Any', 'False'), 'Refusing to replace a dedicated moving pivot with an idle turn-start')
        spec['preserved_moving_filter'] = moving
    for index in SPRINT_FALLBACK:
        row = snapshot['rows'][index]
        require(setup['enum_cell'](row, COL['gait'])[0] == 'Any', 'Sprint partition expected Gait=Any source row')
        description = consolidation['result_description'](row)
        require(description.startswith('Asset[AnimSequence]:AnimPro_'), 'Unexpected shared fallback result')
        path = OLD_ANIMS + description.split(':', 1)[1]
        setup['load'](path, unreal.AnimSequence)
        specs.append(dict(role='sprint_fallback', asset=path, template=index, gait='Sprint'))
    require(len(specs) == TOTAL_ROWS - BASE_ROWS, 'Final append count changed')
    positive = tag_rows(columns[COL['positive']], BASE_ROWS, text, False) | set(range(BASE_ROWS, TOTAL_ROWS))
    negative = tag_rows(columns[COL['negative']], BASE_ROWS, text, True) | set(GATED_OLD)
    report = {'mode': 'audit', 'status': 'auditing', 'already_prepared': False, 'sequences': [], 'pending': [],
              'original_dump': snapshot['text'], 'original_columns': [c.export_text() for c in columns],
              'warnings': ['P01 stop entry-foot measurements are reported; LU/RU row mapping follows the reviewed existing native convention.',
                           'Role-equivalent starts retain source moving filters; dedicated Run180 footed pivots and Sprint remain shared.',
                           'This does not establish P01 transition-to-loop phase-lock equivalence; playback validation remains separate.']}
    mesh = setup['load'](MESH, unreal.SkeletalMesh)
    for spec in specs[:112]:
        record = dict(asset=spec['asset'], role=spec['role'])
        report['sequences'].append(record)
        try:
            seq = setup['load'](spec['asset'], unreal.AnimSequence)
            record['audit'] = asset_audit(seq, mesh)
            if spec['role'] != 'stance':
                require(math.hypot(*record['audit']['root_delta_cm'][:2]) > 0.1, 'Ground source has no baked planar root travel')
                if spec.get('start') not in (None, 'Fwd'):
                    require(abs(record['audit']['root_yaw_delta_degrees']) > 1, 'Turn-start source has no baked root yaw')
            else:
                require(not record['audit']['enable_root_motion'], 'Stance source must remain cosmetic (RM disabled)')
            record['status'] = 'audited'
        except Exception as error:
            record.update(status='pending', error=str(error))
            report['pending'].append(spec['asset'] + ': ' + str(error))
    # Reorder all existing filters and the new direction filter before Randomize.
    order = [i for i in range(18) if i not in (COL['random'], COL['outputs'])] + [18, COL['random'], COL['outputs']]
    mapped = {key: order.index(index) for key, index in COL.items()}
    mapped['direction4'] = order.index(18)
    old_to_new = [order.index(index) for index in range(18)]
    preserved = [digest(column_data(column, text)[3]) for column in columns]
    preserved[COL['negative']] = digest(column_data(tag_column(True, negative, TOTAL_ROWS, text), text)[3][:BASE_ROWS])
    manifest = {'new_rows': specs, 'gated_original_rows': GATED_OLD, 'sprint_fallback_originals': SPRINT_FALLBACK,
                'positive_rows': sorted(positive), 'negative_rows': sorted(negative),
                'original_results': [consolidation['result_description'](row) for row in snapshot['rows']],
                'original_disabled': setup['chooser_disabled_rows'](MAIN), 'old_to_new': old_to_new,
                'mapped_columns': mapped, 'column_order': order, 'preserved_prefix_hashes': preserved}
    tag_column(False, positive, TOTAL_ROWS, text)
    report['manifest'] = manifest
    report['status'] = 'audit_complete' if not report['pending'] else 'audit_pending'
    return report


def prepare_chooser(report, setup, text, consolidation):
    asset = setup['load'](MAIN, unreal.ChooserTable)
    require(setup['chooser_snapshot'](MAIN)['text'] == report['original_dump'], 'Master changed since preflight')
    manifest = report['manifest']
    EAL.set_metadata_tag(asset, OWNER_KEY, OWNER)
    EAL.set_metadata_tag(asset, STATE_KEY, 'preparing')
    EAL.set_metadata_tag(asset, MANIFEST_KEY, json.dumps(manifest, sort_keys=True))
    setup['save'](asset)
    new_direction = CU.add_multi_enum_column_to_sub(MAIN, '', 'MovementDirection', 'EAZ_MovementDirection')
    require(new_direction == 18, 'Direction filter did not append at the expected index')
    require(CU.set_column_binding_chain(MAIN, new_direction, ['ChooserContext', 'MovementDirection'], 0), 'Direction binding failed')
    originals = list(asset.get_editor_property('columns_structs'))
    for offset, spec in enumerate(manifest['new_rows']):
        require(CU.add_empty_row_to_sub(MAIN, '', setup['load'](spec['asset'], unreal.AnimSequence)) == BASE_ROWS + offset,
                'Transition result appended at an unexpected row')
    final_columns = []
    for index, original in enumerate(originals):
        if index == COL['positive']:
            value = tag_column(False, set(manifest['positive_rows']), TOTAL_ROWS, text)
        elif index == COL['negative']:
            value = tag_column(True, set(manifest['negative_rows']), TOTAL_ROWS, text)
        else:
            original_rows = column_data(original, text)[3][:BASE_ROWS]
            require(len(original_rows) == BASE_ROWS, 'Lost original chooser cells during append')
            extended = original_rows + [original_rows[spec['template']] for spec in manifest['new_rows']]
            value = with_rows(original, extended, text)
        final_columns.append(value)
    asset.set_editor_property('columns_structs', [final_columns[index] for index in manifest['column_order']])
    mapped = manifest['mapped_columns']
    for offset, spec in enumerate(manifest['new_rows']):
        index = BASE_ROWS + offset
        if spec['role'] == 'sprint_fallback':
            require(CU.set_cell_enum_on_sub(MAIN, '', index, mapped['gait'], 'Sprint', 0), 'Sprint fallback gait write failed')
            continue
        for key, value in (('state', spec['state']), ('stance', spec['stance']), ('gait', spec['gait'])):
            require(CU.set_cell_enum_on_sub(MAIN, '', index, mapped[key], value or 'Walk', 0 if value else 2), 'Transition enum write failed: ' + key)
        require(CU.set_cell_enum_on_sub(MAIN, '', index, mapped['direction'], 'F', 2), 'Old direction must be Any for new rows')
        require(CU.set_cell_multi_enum_on_sub(MAIN, '', index, mapped['direction4'], spec['directions']), 'Four-way direction filter failed')
        require(CU.set_cell_bool_on_sub(MAIN, '', index, mapped['aim'], 1 if spec['aim'] else 0), 'Aim filter failed')
        if spec['role'] == 'start':
            require(CU.set_cell_enum_on_sub(MAIN, '', index, mapped['start'], spec['start'], 0), 'Start-direction filter failed')
        # Preserve source template moving/just-landed/strafe/reaction gates and
        # timing fields. Every appended P01 selection is an explicit direct play.
        require(CU.set_cell_output_struct_field_on_sub(MAIN, '', index, mapped['outputs'], 'bUseMM', 'False'), 'Direct-play flag failed')
        require(CU.set_cell_output_struct_field_on_sub(MAIN, '', index, mapped['outputs'], 'StartTime', '0'), 'StartTime write failed')
    require(CU.compile_and_save(MAIN), 'Main chooser compile/save failed')
    verify_complete(asset, manifest, setup, text, consolidation)
    EAL.set_metadata_tag(asset, STATE_KEY, 'complete')
    require(CU.compile_and_save(MAIN), 'Transition completion save failed')


def main(prepare=False, backup_dir=None, receipt_path=None):
    report = None
    try:
        report = audit()
        if not prepare or report['already_prepared']:
            return report
        require(not report['pending'], 'All112 assets must pass static live audit before preparation: ' + json.dumps(report['pending']))
        setup, text, consolidation = libraries()
        report['backup'] = setup['backup_existing']([MAIN] + [r['asset'] for r in report['sequences']], backup_dir)
        report.update(mode='prepare', status='preparing')
        mesh = setup['load'](MESH, unreal.SkeletalMesh)
        for record in report['sequences']:
            seq = setup['load'](record['asset'], unreal.AnimSequence)
            before = asset_audit(seq, mesh)
            require(before == record['audit'], 'Transition asset changed after audit: ' + record['asset'])
            if record['role'] != 'stance' and not before['enable_root_motion']:
                seq.set_editor_property('enable_root_motion', True)
                setup['save'](seq)
            after = asset_audit(seq, mesh)
            expected = dict(before)
            if record['role'] != 'stance':
                expected['enable_root_motion'] = True
            require(after == expected, 'Unexpected transition data changed: ' + record['asset'])
            record['prepared_root_motion'] = after['enable_root_motion']
        prepare_chooser(report, setup, text, consolidation)
        report.update(status='prepared', final_row_count=TOTAL_ROWS)
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
            print('[P01Transitions] ' + json.dumps({key: report.get(key) for key in ('mode', 'status', 'pending', 'final_row_count', 'error')}))
        gc.collect()


if __name__ == '__main__':
    main()
