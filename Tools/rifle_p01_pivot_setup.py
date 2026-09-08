# @Description: Audit or append an explicit rifle turn-start fallback for moving Run180.
"""Read-only by default. No sequence edits, retargets, PIE or editor tests.

Four existing P01 turn-starts cover rifle moving180 with Foot=Any. They preserve
the full turn at StartTime0; they are slower from-rest fallbacks, not fabricated
LU/RU momentum-preserving pivots. Explicit Run + NOT Movement.Sprinting gates
keep the user's shared sprint/back-carry policy. Appends to the CURRENT master,
including a prior jump pass, and preserves all existing non-tag cells/results.
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
ROOT = '/Game/AZ/NoWeapons/RootMotions/rm_W2_'
MESH = '/Game/SurvivalMan/Meshes/SKM_SurvivalMan_Mesh1'
OWNER = 'rifle_p01_pivot_setup:v1'
OWNER_KEY, STATE_KEY, MANIFEST_KEY = 'AZ.P01Pivot.Owner', 'AZ.P01Pivot.State', 'AZ.P01Pivot.Manifest'
OLD_ROWS = (24, 25, 26, 27)
SPECS = [dict(side=side, aim=aim, template=24 if side == 'L' else 26,
              asset=ROOT + ('Stand_Aim_To_Jog_Aim_' if aim else 'Stand_Relaxed_To_Jog_') + side + '180_Fwd')
         for aim in (False, True) for side in ('L', 'R')]
EAL = unreal.EditorAssetLibrary
CU = unreal.AZ_ChooserUtils


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def libraries():
    return tuple(runpy.run_path(str(PROJECT / 'Tools' / name), run_name='p01_pivot_dependency')
                 for name in ('rifle_p01_setup.py', 'rifle_p01_activate.py', 'rifle_p01_transition_setup.py'))


def game_world():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    return world.get_path_name() if world else None


def result_name(row):
    import re
    match = re.search(r'"out":\s*"([^"]*)"', row['line'])
    require(match is not None, 'Missing chooser result diagnostic')
    return match.group(1)


def parts(column, text, transition):
    return transition['column_data'](column, text)


def tag_names(row, text):
    payload = dict(text['fields'](row)).get('GameplayTags', '')
    return {dict(text['fields'](entry))['TagName'].strip('"')
            for entry in text['split_top_level'](text['parenthesized'](payload))}


def signature(column, text, transition):
    kind, fields, _, rows = parts(column, text, transition)
    data = dict(fields)
    parameter = data.get('InputValue', 'None')
    chain, context = [], None
    if parameter != 'None':
        _, values = text['fragment_parts'](parameter)
        binding = dict(values).get('Binding')
        if binding:
            binding = dict(text['fields'](binding))
            chain = [x.strip('"').casefold() for x in text['split_top_level'](
                text['parenthesized'](binding.get('PropertyBindingChain', '')))]
            context = binding.get('ContextIndex', '0')
    tags = sorted(set().union(*(tag_names(row, text) for row in rows))) if kind.endswith('.GameplayTagColumn') else []
    return json.dumps([kind, chain, context, data.get('bInvertMatchingLogic'), tags], sort_keys=True)


def columns_for(asset, snapshot, text, transition):
    columns = list(asset.get_editor_property('columns_structs'))
    found = {}
    for descriptor, column in zip(snapshot['columns'], columns):
        index, kind = descriptor['index'], descriptor['type']
        sig = json.loads(signature(column, text, transition))
        if kind == 'EnumColumn':
            key = {'EAZ_Gait': 'gait', 'EAZ_StartDirection': 'start', 'EAZ_StateMachineState': 'state'}.get(descriptor['enum'])
            if key:
                require(key not in found, 'Ambiguous core enum column: ' + key)
                found[key] = index
        elif kind == 'BoolColumn' and sig[1]:
            key = {'bisaiming': 'aim', 'bmovingtransition': 'moving', 'bleftfootdown': 'foot'}.get(sig[1][-1])
            if key:
                require(sig[1] == ['choosercontext', sig[1][-1]] and sig[2] == '0', 'Unexpected bool context binding')
                require(key not in found, 'Ambiguous core bool column')
                found[key] = index
        elif kind == 'GameplayTagColumn' and sig[4] == ['Weapon.Rifle']:
            found['negative' if sig[3] == 'True' else 'positive'] = index
        elif kind == 'GameplayTagColumn' and sig[4] == ['Movement.Sprinting'] and sig[3] == 'True':
            found['not_sprinting'] = index
        elif kind in ('RandomizeColumn', 'OutputStructColumn'):
            found['random' if kind == 'RandomizeColumn' else 'outputs'] = index
    require(set(('gait', 'start', 'state', 'aim', 'moving', 'foot', 'positive', 'negative', 'random', 'outputs')) <= found.keys(),
            'Required existing chooser columns are unavailable')
    return columns, found


def guard_column(tag, marked, total):
    rows = ['(GameplayTags=((TagName="' + tag + '")))' if i in marked else '()' for i in range(total)]
    value = unreal.InstancedStruct()
    encoded = ('/Script/Chooser.GameplayTagColumn(InputValue=/Script/Chooser.GameplayTagContextProperty('
               'Binding=(PropertyBindingChain=("ChooserContext","OwnedTags"),ContextIndex=0)),TagMatchType=Any,'
               'TagMatchDirection=RowValueInInput,bMatchExact=False,bInvertMatchingLogic=True,RowValues=('
               + ','.join(rows) + '))')
    require(value.import_text(encoded), 'Sprint guard native ImportText failed')
    return value


def measure(seq, mesh):
    length = float(unreal.AnimationLibrary.get_sequence_length(seq))
    require(seq.get_editor_property('skeleton') == mesh.get_editor_property('skeleton'), 'Rifle fallback skeleton mismatch')
    require(seq.get_editor_property('enable_root_motion') and not seq.get_editor_property('loop')
            and abs(float(seq.get_editor_property('rate_scale')) - 1.0) < 1e-6, 'Fallback must be RM-enabled, non-looping, RateScale1')
    imported = seq.get_editor_property('asset_import_data')
    provenance = imported.get_first_filename() if imported else ''
    require('Rifle_01_PRO_v27A' in provenance, 'Fallback import provenance is not Rifle_01')
    model = seq.get_editor_property('data_model_interface')
    rate = model.get_frame_rate()
    interval = float(rate.denominator) / float(rate.numerator)
    require(interval > 0 and length > 0.15, 'Invalid actual source timing')
    times = sorted(set([min(i * interval, length) for i in range(model.get_number_of_frames() + 1)] + [length]))
    options = unreal.AnimPoseEvaluationOptions()
    for key, value in {'evaluation_type': unreal.AnimDataEvalType.RAW, 'should_retarget': False,
                       'extract_root_motion': False, 'incorporate_root_motion_into_pose': True,
                       'optional_skeletal_mesh': mesh}.items():
        options.set_editor_property(key, value)
    frames = []
    for time in times:
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, time, options)
        require(unreal.AnimPoseExtensions.is_valid(pose), 'Invalid fallback raw pose')
        transform = unreal.AnimPoseExtensions.get_bone_pose(pose, 'root', unreal.AnimPoseSpaces.WORLD)
        p = transform.translation
        frames.append({'time': time, 'root_cm': [float(p.x), float(p.y), float(p.z)], 'yaw': float(transform.rotation.rotator().yaw)})
    yaw = sum((b['yaw'] - a['yaw'] + 180) % 360 - 180 for a, b in zip(frames, frames[1:]))
    require(abs(abs(yaw) - 180) <= 1.0, 'Fallback does not contain the complete180-degree root turn')
    early = next(frame for frame in frames if frame['time'] >= 0.15)
    return {'length_seconds': length, 'model_frames': model.get_number_of_frames(), 'sample_interval_seconds': interval,
            'source_file': provenance, 'root_frames': frames, 'root_yaw_delta': yaw,
            'travel_first_150ms_cm': math.dist(frames[0]['root_cm'], early['root_cm']),
            'entry_time_seconds': 0.0, 'role': 'from-rest turn-start used as explicit moving fallback', 'foot_filter': 'Any'}


def verify(asset, manifest, setup, text, transition):
    snapshot = setup['chooser_snapshot'](MAIN)
    columns, mapped = columns_for(asset, snapshot, text, transition)
    require(len(snapshot['rows']) >= manifest['total_after'], 'Previously appended pivot rows are missing')
    current_by_sig = {signature(column, text, transition): column for column in columns}
    for old in manifest['prefix_columns']:
        require(old['signature'] in current_by_sig, 'An existing chooser column binding changed')
        values = parts(current_by_sig[old['signature']], text, transition)[3]
        if old['negative_rifle']:
            # The coordinated jump pass may add more !Rifle cells later. It must
            # not remove previous gates, including these four pivot exclusions.
            actual = {i for i, row in enumerate(values) if 'Weapon.Rifle' in tag_names(row, text)}
            require(set(manifest['required_negative_rows']) <= actual, 'A required rifle exclusion was removed')
        else:
            require(digest(values[:manifest['base_rows']]) == old['hash'], 'A pre-existing chooser cell changed')
    require([result_name(row) for row in snapshot['rows'][:manifest['base_rows']]] == manifest['original_results'], 'A pre-existing result changed')
    disabled = setup['chooser_disabled_rows'](MAIN)
    require(disabled[:manifest['base_rows']] == manifest['original_disabled'], 'A pre-existing row enable state changed')
    require('not_sprinting' in mapped and mapped['not_sprinting'] < mapped['random'], 'Pivot sprint guard is missing or too late')
    require(max(mapped[key] for key in ('aim', 'foot', 'moving', 'gait', 'start', 'positive', 'negative')) < mapped['random'],
            'A pivot selector runs after Randomize')
    for offset, spec in enumerate(SPECS):
        index = manifest['base_rows'] + offset
        row = snapshot['rows'][index]
        require(not disabled[index] and result_name(row) == 'Asset[AnimSequence]:' + spec['asset'].rsplit('/', 1)[1], 'Fallback row identity/enable state differs')
        require(setup['enum_cell'](row, mapped['gait']) == ('=', 'Run')
                and setup['enum_cell'](row, mapped['start']) == ('=', spec['side'] + '180'), 'Fallback gait/turn selection differs')
        require(row['cells'].get(mapped['foot']) == 'Any' and row['cells'].get(mapped['moving']) == 'True'
                and row['cells'].get(mapped['aim']) == ('True' if spec['aim'] else 'False'), 'Fallback bool gates differ')
        require('Weapon.Rifle' in tag_names(parts(columns[mapped['positive']], text, transition)[3][index], text)
                and not tag_names(parts(columns[mapped['negative']], text, transition)[3][index], text)
                and 'Movement.Sprinting' in tag_names(parts(columns[mapped['not_sprinting']], text, transition)[3][index], text),
                'Fallback rifle/sprint tag gates differ')
    return snapshot


def audit():
    setup, text, transition = libraries()
    asset = setup['load'](MAIN, unreal.ChooserTable)
    if EAL.get_metadata_tag(asset, OWNER_KEY):
        require(EAL.get_metadata_tag(asset, OWNER_KEY) == OWNER and EAL.get_metadata_tag(asset, STATE_KEY) == 'complete', 'Unknown/interrupted pivot authoring state')
        manifest = json.loads(EAL.get_metadata_tag(asset, MANIFEST_KEY))
        snapshot = verify(asset, manifest, setup, text, transition)
        return {'status': 'already_prepared', 'mode': 'audit', 'manifest': manifest, 'row_count': len(snapshot['rows'])}
    require(EAL.get_metadata_tag(asset, 'AZ.RifleP01Transitions.State') == 'complete', 'The existing transition pass must already be complete')
    snapshot = setup['chooser_snapshot'](MAIN)
    base = len(snapshot['rows'])
    require(base >= 276, 'Expected the existing transition master prefix')
    columns, mapped = columns_for(asset, snapshot, text, transition)
    for index, side, foot in ((24, 'L', 'RU'), (25, 'L', 'LU'), (26, 'R', 'RU'), (27, 'R', 'LU')):
        row = snapshot['rows'][index]
        require(result_name(row) == 'Asset[AnimSequence]:AnimPro_RunFwdTurn180_' + side + '_' + foot
                and setup['enum_cell'](row, mapped['gait']) == ('=', 'Run')
                and row['cells'].get(mapped['moving']) == 'True', 'Original pivot surface differs')
        require(not tag_names(parts(columns[mapped['negative']], text, transition)[3][index], text), 'Original pivot is already rifle-gated by another pass')
    report = {'status': 'audit_complete', 'mode': 'audit', 'base_rows': base, 'expected_total_rows': base + 4,
              'game_world': game_world(), 'original_dump': snapshot['text'], 'measurements': [],
              'limitations': ['No authored P01 footed moving180 variants were found; these are explicit turn-start fallbacks.',
                              'First150ms travel is about1.7-4.1cm versus44-54cm for the shared moving pivots: anticipate a slower stop-and-turn.',
                              'StartTime0 preserves full root yaw. FootAny is intentional; no fabricated LU/RU variant or new blend trick.',
                              'Explicit Run plus NOT Movement.Sprinting keeps shared Sprint/back-carry behavior.']}
    mesh = setup['load'](MESH, unreal.SkeletalMesh)
    for spec in SPECS:
        report['measurements'].append(dict(asset=spec['asset'], **measure(setup['load'](spec['asset'], unreal.AnimSequence), mesh)))
    negative = {i for i, row in enumerate(parts(columns[mapped['negative']], text, transition)[3]) if tag_names(row, text)} | set(OLD_ROWS)
    manifest = {'base_rows': base, 'total_after': base + 4, 'specs': SPECS,
                'original_results': [result_name(row) for row in snapshot['rows']],
                'original_disabled': setup['chooser_disabled_rows'](MAIN), 'required_negative_rows': sorted(negative),
                'prefix_columns': [{'signature': signature(c, text, transition), 'hash': digest(parts(c, text, transition)[3]),
                                    'negative_rifle': i == mapped['negative']} for i, c in enumerate(columns)]}
    report['manifest'] = manifest
    return report


def main(prepare=False, backup_dir=None, receipt_path=None):
    report = None
    try:
        report = audit()
        if not prepare or report['status'] == 'already_prepared':
            return report
        require(game_world() is None, 'No pivot asset writes while a game world/PIE is active')
        setup, text, transition = libraries()
        report['backup'] = setup['backup_existing']([MAIN], backup_dir)
        asset = setup['load'](MAIN, unreal.ChooserTable)
        before = setup['chooser_snapshot'](MAIN)
        require(before['text'] == report['original_dump'] and game_world() is None, 'Master/world changed after preflight')
        columns, mapped = columns_for(asset, before, text, transition)
        originals = [(column, list(parts(column, text, transition)[3])) for column in columns]
        manifest = report['manifest']; base = manifest['base_rows']; total = manifest['total_after']
        EAL.set_metadata_tag(asset, OWNER_KEY, OWNER)
        EAL.set_metadata_tag(asset, STATE_KEY, 'preparing')
        EAL.set_metadata_tag(asset, MANIFEST_KEY, json.dumps(manifest, sort_keys=True))
        setup['save'](asset)
        for offset, spec in enumerate(SPECS):
            require(CU.add_empty_row_to_sub(MAIN, '', setup['load'](spec['asset'], unreal.AnimSequence)) == base + offset, 'Unexpected pivot append index')
        completed = []
        for index, (column, values) in enumerate(originals):
            values.extend(values[spec['template']] for spec in SPECS)
            if index == mapped['positive']:
                for row in range(base, total): values[row] = '(GameplayTags=((TagName="Weapon.Rifle")))'
            elif index == mapped['negative']:
                for row in OLD_ROWS: values[row] = '(GameplayTags=((TagName="Weapon.Rifle")))'
                for row in range(base, total): values[row] = '()'
            elif index == mapped.get('not_sprinting'):
                for row in range(base, total): values[row] = '(GameplayTags=((TagName="Movement.Sprinting")))'
            completed.append(transition['with_rows'](column, values, text))
        if 'not_sprinting' not in mapped:
            completed.append(guard_column('Movement.Sprinting', set(range(base, total)), total))
        order = [i for i in range(len(completed)) if i not in (mapped['random'], mapped['outputs'])] + [mapped['random'], mapped['outputs']]
        asset.set_editor_property('columns_structs', [completed[i] for i in order])
        snapshot = setup['chooser_snapshot'](MAIN)
        _, mapped = columns_for(asset, snapshot, text, transition)
        for offset, spec in enumerate(SPECS):
            row = base + offset
            for key, value in (('foot', 2), ('moving', 1), ('aim', 1 if spec['aim'] else 0)):
                require(CU.set_cell_bool_on_sub(MAIN, '', row, mapped[key], value), 'Fallback bool write failed')
            require(CU.set_cell_enum_on_sub(MAIN, '', row, mapped['gait'], 'Run', 0), 'Fallback Run gate write failed')
            require(CU.set_cell_output_struct_field_on_sub(MAIN, '', row, mapped['outputs'], 'StartTime', '0'), 'Fallback StartTime write failed')
        require(CU.compile_and_save(MAIN), 'Pivot chooser compile/save failed')
        verified = verify(asset, manifest, setup, text, transition)
        EAL.set_metadata_tag(asset, STATE_KEY, 'complete')
        require(CU.compile_and_save(MAIN), 'Pivot completion save failed')
        report.update(mode='prepare', status='prepared', row_count=len(verified['rows']))
        return report
    except Exception as error:
        if report is not None: report.update(status='failed', error=str(error))
        raise
    finally:
        if report is not None:
            if receipt_path:
                path = Path(receipt_path); path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(json.dumps(report, indent=2), encoding='utf-8')
            print('[P01PivotSetup] ' + json.dumps({k: report.get(k) for k in ('status', 'mode', 'base_rows', 'expected_total_rows', 'row_count', 'game_world', 'error')}))
        gc.collect()


if __name__ == '__main__':
    main()
