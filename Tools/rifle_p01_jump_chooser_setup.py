# @Description: Route verified P01 jump and landing roles through the existing master chooser.
"""Read-only main(); explicit prepare=True and native_index_verified=True to write.

Appends20 rifle role rows plus2 sprint-tag idle fallbacks. Existing sprint jump
rows remain freehand. Preserves the current master prefix, including the pivot
pass, except explicit rifle exclusions and landing-collision refinements on the
rifle ground-start/stop rows. No animation/AnimBP/native-code changes or PIE/tests.
"""
import gc
import hashlib
import json
import re
from pathlib import Path
import runpy

import unreal

PROJECT = Path('C:/UnrealEngine/Games/AZ')
MAIN = '/Game/AZ/Blueprints/Animation/MotionMatching/CHT_v2_CharacterAnimations'
OWNER = 'rifle_p01_jump_chooser_setup:v1'
OWNER_KEY, STATE_KEY, MANIFEST_KEY = 'AZ.P01JumpChooser.Owner', 'AZ.P01JumpChooser.State', 'AZ.P01JumpChooser.Manifest'
PIVOT_MANIFEST = 'AZ.P01Pivot.Manifest'
CONTENT_RECEIPT = PROJECT / 'Saved/RifleAnimationContent/p01-jump-prepare.json'
EAL, CU = unreal.EditorAssetLibrary, unreal.AZ_ChooserUtils


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def libraries():
    return tuple(runpy.run_path(str(PROJECT / 'Tools' / name), run_name='p01_jump_chooser_dependency')
                 for name in ('rifle_p01_setup.py', 'rifle_p01_activate.py', 'rifle_p01_transition_setup.py',
                              'rifle_p01_pivot_setup.py', 'rifle_p01_jump_setup.py'))


def columns_for(asset, snapshot, text, transition, pivot):
    columns, mapped = pivot['columns_for'](asset, snapshot, text, transition)
    for descriptor, column in zip(snapshot['columns'], columns):
        sig = json.loads(pivot['signature'](column, text, transition))
        if descriptor['type'] == 'BoolColumn' and sig[1] == ['choosercontext', 'bjustlanded']:
            mapped['landed'] = descriptor['index']
        if descriptor['type'] == 'GameplayTagColumn' and sig[4] == ['Movement.Sprinting'] and sig[3] == 'False':
            mapped['sprinting'] = descriptor['index']
    require('landed' in mapped and 'not_sprinting' in mapped, 'Landing and inverted Sprint filters must already exist')
    return columns, mapped


def sprint_column(marked, total):
    rows = ['(GameplayTags=((TagName="Movement.Sprinting")))' if index in marked else '()' for index in range(total)]
    value = unreal.InstancedStruct()
    require(value.import_text('/Script/Chooser.GameplayTagColumn(InputValue=/Script/Chooser.GameplayTagContextProperty('
            'Binding=(PropertyBindingChain=("ChooserContext","OwnedTags"),ContextIndex=0)),TagMatchType=Any,'
            'TagMatchDirection=RowValueInInput,bMatchExact=False,bInvertMatchingLogic=False,RowValues=('
            + ','.join(rows) + '))'), 'Cannot create positive Sprint fallback filter')
    return value


def specs_from_content(content):
    specs = []
    for record in content['records']:
        if not record['moving']:
            start, land = 32, 33
        else:
            start = (28 if record['gait'] == 'Walk' else 30) + (1 if record['foot'] == 'LU' else 0)
            land = (34 if record['gait'] == 'Walk' else 36) + (1 if record['foot'] == 'LU' else 0)
        specs += [dict(role=role, template=template, asset=record[role], aim=record['aim'], key=record['key'])
                  for role, template in (('takeoff', start), ('land', land))]
    specs += [dict(role='sprint_idle', template=template,
                   asset='/Game/Assets/RTG_AZ/MovementAnimsetPro/' + name)
              for template, name in ((32, 'AnimPro_JumpIdleStart'), (33, 'AnimPro_JumpIdleLand'))]
    require(len(specs) == 22, 'Expected ten role pairs and two Sprint idle fallbacks')
    return specs


def output_values(row, column):
    value = row['cells'].get(column, '')
    result = {}
    for field in ('bUseMM', 'StartTime', 'BlendTime'):
        match = re.search(r'\b' + field + r'=([^,}]+)', value)
        require(match is not None, 'Missing chooser output field: ' + field)
        result[field] = match.group(1)
    return {'use_mm': result['bUseMM'] == 'True', 'start_time': float(result['StartTime']),
            'blend_time': float(result['BlendTime'])}


def verify(asset, manifest, setup, text, transition, pivot):
    snapshot = setup['chooser_snapshot'](MAIN)
    columns, mapped = columns_for(asset, snapshot, text, transition, pivot)
    require(len(snapshot['rows']) >= manifest['total_rows'], 'Jump rows disappeared')
    by_signature = {pivot['signature'](column, text, transition): column for column in columns}
    for saved in manifest['prefix_columns']:
        require(saved['signature'] in by_signature, 'An original column binding changed')
        values = transition['column_data'](by_signature[saved['signature']], text)[3]
        require(digest(values[:manifest['base_rows']]) == saved['expected_hash'], 'An original prefix cell changed outside the reviewed jump patch')
    require([pivot['result_name'](row) for row in snapshot['rows'][:manifest['base_rows']]] == manifest['original_results'], 'Original results changed')
    disabled = setup['chooser_disabled_rows'](MAIN)
    require(disabled[:manifest['base_rows']] == manifest['original_disabled'], 'Original row enable states changed')
    values = [transition['column_data'](column, text)[3] for column in columns]
    for offset, spec in enumerate(manifest['specs']):
        index = manifest['base_rows'] + offset
        row, template = snapshot['rows'][index], snapshot['rows'][spec['template']]
        require(not disabled[index] and pivot['result_name'](row) == 'Asset[AnimSequence]:' + spec['asset'].rsplit('/', 1)[1], 'Jump role result/enable mismatch')
        for key in ('state', 'gait', 'foot', 'moving', 'landed', 'start'):
            require(row['cells'].get(mapped[key]) == template['cells'].get(mapped[key]), 'Jump template semantics changed: ' + key)
        require('Weapon.Rifle' in pivot['tag_names'](values[mapped['positive']][index], text)
                and not pivot['tag_names'](values[mapped['negative']][index], text), 'Jump rifle ownership gate differs')
        outputs = output_values(row, mapped['outputs'])
        original_outputs = output_values(template, mapped['outputs'])
        require(outputs['start_time'] == 0.0, 'Jump role must enter at its derived frame zero')
        if spec['role'] == 'sprint_idle':
            require(outputs == original_outputs, 'Sprint idle fallback output settings changed')
            require('Movement.Sprinting' in pivot['tag_names'](values[mapped['sprinting']][index], text)
                    and not pivot['tag_names'](values[mapped['not_sprinting']][index], text), 'Sprint idle fallback is unreachable')
        else:
            require('Movement.Sprinting' in pivot['tag_names'](values[mapped['not_sprinting']][index], text)
                    and not pivot['tag_names'](values[mapped['sprinting']][index], text), 'Rifle jump leaked into sprint')
            require(row['cells'].get(mapped['aim']) == ('True' if spec['aim'] else 'False'), 'Aim/relaxed jump selection differs')
            if spec['role'] == 'takeoff':
                require(not outputs['use_mm'] and abs(outputs['blend_time'] - 0.05) < 1e-6,
                        'Takeoff must remain direct play with the authored 0.05s incoming blend')
            else:
                expected_mm = spec['template'] != 33  # Existing idle land is direct; moving lands use single-clip MM.
                require(outputs['use_mm'] == expected_mm and outputs['blend_time'] == original_outputs['blend_time'],
                        'Landing direct/MM or blend policy differs from the existing source role')
    require(max(index for key, index in mapped.items() if key not in ('random', 'outputs')) < mapped['random'], 'A selector remains after Randomize')
    require(EAL.get_metadata_tag(asset, PIVOT_MANIFEST) == manifest['pivot_manifest_after'], 'Coordinated pivot manifest changed')
    pivot['audit']()  # Must continue to pass after the reviewed prefix refinement.
    return snapshot


def audit():
    setup, text, transition, pivot, jump = libraries()
    asset = setup['load'](MAIN, unreal.ChooserTable)
    if EAL.get_metadata_tag(asset, OWNER_KEY):
        require(EAL.get_metadata_tag(asset, OWNER_KEY) == OWNER and EAL.get_metadata_tag(asset, STATE_KEY) == 'complete', 'Unknown/interrupted jump chooser authoring')
        manifest = json.loads(EAL.get_metadata_tag(asset, MANIFEST_KEY))
        snapshot = verify(asset, manifest, setup, text, transition, pivot)
        return {'status': 'already_prepared', 'row_count': len(snapshot['rows']), 'manifest': manifest}
    pivot['audit']()  # Strict before-hash verification, prior to refreshing its one affected prefix hash.
    content = json.loads(CONTENT_RECEIPT.read_text(encoding='utf-8'))
    require(content['status'] == 'content_prepared_master_unchanged', 'Owned two-phase content is not prepared')
    mesh = jump['load'](jump['MESH'], unreal.SkeletalMesh)
    for record in content['records']:
        for role in ('takeoff', 'land'):
            sequence = jump['owned_existing'](record[role], dict(record['manifest'], role=role))
            require(sequence is not None, 'Owned jump content missing')
            jump['role_readback'](sequence, record['measurement'], role, mesh)
    db = jump['owned_existing'](jump['DATABASE'], content['database_manifest'])
    require(db is not None and sorted(setup['members'](db)) == sorted(content['database_manifest']['lands']), 'Landing DB membership changed')
    snapshot = setup['chooser_snapshot'](MAIN)
    columns, mapped = columns_for(asset, snapshot, text, transition, pivot)
    base, specs = len(snapshot['rows']), specs_from_content(content)
    require(base >= 280, 'Expected the existing completed ground/pivot prefix')
    raw = [transition['column_data'](column, text)[3] for column in columns]
    require(snapshot['rows'][32]['cells'][mapped['state']] == '= Transition to in Air'
            and snapshot['rows'][33]['cells'][mapped['landed']] == 'True', 'Shared idle jump/land surface changed')
    for index in range(28, 38):
        require(not pivot['tag_names'](raw[mapped['negative']][index], text), 'Old jump rows already rifle-gated')
    # Only ground roles that can actually collide with standing Fwd-bucket landing rows.
    ground_manifest = json.loads(EAL.get_metadata_tag(asset, 'AZ.RifleP01Transitions.Manifest'))
    refinements = []
    for offset, spec in enumerate(ground_manifest['new_rows'][:112]):
        index = 155 + offset
        if (spec['role'] in ('start', 'stop') and spec['stance'] == 'Standing'
                and (spec['role'] == 'stop' or spec.get('start') == 'Fwd')
                and snapshot['rows'][index]['cells'].get(mapped['landed']) == 'Any'):
            refinements.append(index)
    require(len(refinements) == 36, 'Reviewed standing directional landing-collision surface changed')
    planned = [list(values) for values in raw]
    for index in range(28, 38): planned[mapped['negative']][index] = '(GameplayTags=((TagName="Weapon.Rifle")))'
    false_cell = raw[mapped['landed']][5]
    require(snapshot['rows'][5]['cells'].get(mapped['landed']) == 'False', 'Known typed false cell is unavailable')
    for index in refinements: planned[mapped['landed']][index] = false_cell
    pivot_before = EAL.get_metadata_tag(asset, PIVOT_MANIFEST)
    pivot_after = json.loads(pivot_before)
    landed_signature = pivot['signature'](columns[mapped['landed']], text, transition)
    matching = [saved for saved in pivot_after['prefix_columns'] if saved['signature'] == landed_signature]
    require(len(matching) == 1, 'Pivot landing prefix hash is not uniquely identified')
    require(matching[0]['hash'] == digest(raw[mapped['landed']][:pivot_after['base_rows']]), 'Pivot preflight landing hash mismatch')
    matching[0]['hash'] = digest(planned[mapped['landed']][:pivot_after['base_rows']])
    manifest = dict(base_rows=base, total_rows=base+22, specs=specs, landing_refinements=refinements,
                    original_results=[pivot['result_name'](row) for row in snapshot['rows']], original_disabled=setup['chooser_disabled_rows'](MAIN),
                    prefix_columns=[{'signature': pivot['signature'](column, text, transition), 'before_hash': digest(raw[index]),
                                     'expected_hash': digest(planned[index])} for index, column in enumerate(columns)],
                    pivot_manifest_before=pivot_before, pivot_manifest_after=json.dumps(pivot_after, sort_keys=True))
    return dict(status='audit_complete', original_dump=snapshot['text'], game_world=pivot['game_world'](), manifest=manifest)


def main(prepare=False, native_index_verified=False, receipt_path=None):
    report = None
    try:
        report = audit()
        if not prepare or report['status'] == 'already_prepared': return report
        require(native_index_verified, 'Caller must confirm fresh native PSD_P01_Land BuildIndex Succeeded before activation')
        setup, text, transition, pivot, jump = libraries()
        jump['no_game_world']()
        report['backup'] = setup['backup_existing']([MAIN])
        asset = setup['load'](MAIN, unreal.ChooserTable)
        before = setup['chooser_snapshot'](MAIN)
        require(before['text'] == report['original_dump'], 'Master changed after jump preflight')
        columns, mapped = columns_for(asset, before, text, transition, pivot)
        originals = [(column, list(transition['column_data'](column, text)[3])) for column in columns]
        manifest = report['manifest']; base = manifest['base_rows']; total = manifest['total_rows']; specs = manifest['specs']
        EAL.set_metadata_tag(asset, OWNER_KEY, OWNER); EAL.set_metadata_tag(asset, STATE_KEY, 'preparing')
        EAL.set_metadata_tag(asset, MANIFEST_KEY, json.dumps(manifest, sort_keys=True)); setup['save'](asset)
        for offset, spec in enumerate(specs):
            require(CU.add_empty_row_to_sub(MAIN, '', setup['load'](spec['asset'], unreal.AnimSequence)) == base+offset, 'Unexpected jump append index')
        complete = []
        false_cell = originals[mapped['landed']][1][5]
        for index, (column, values) in enumerate(originals):
            values.extend(values[spec['template']] for spec in specs)
            if index == mapped['positive']:
                for row in range(base, total): values[row] = '(GameplayTags=((TagName="Weapon.Rifle")))'
            elif index == mapped['negative']:
                for row in range(28,38): values[row] = '(GameplayTags=((TagName="Weapon.Rifle")))'
                for row in range(base,total): values[row] = '()'
            elif index == mapped['not_sprinting']:
                for offset, spec in enumerate(specs): values[base+offset] = '()' if spec['role']=='sprint_idle' else '(GameplayTags=((TagName="Movement.Sprinting")))'
            elif index == mapped.get('sprinting'):
                for offset, spec in enumerate(specs): values[base+offset] = '(GameplayTags=((TagName="Movement.Sprinting")))' if spec['role']=='sprint_idle' else '()'
            elif index == mapped['landed']:
                for row in manifest['landing_refinements']: values[row] = false_cell
            complete.append(transition['with_rows'](column, values, text))
        if 'sprinting' not in mapped: complete.append(sprint_column(set(range(base+20,total)), total))
        order = [i for i in range(len(complete)) if i not in (mapped['random'],mapped['outputs'])]+[mapped['random'],mapped['outputs']]
        asset.set_editor_property('columns_structs',[complete[i] for i in order])
        _, mapped = columns_for(asset, setup['chooser_snapshot'](MAIN), text, transition, pivot)
        for offset,spec in enumerate(specs):
            row=base+offset
            if spec['role']=='sprint_idle': continue
            require(CU.set_cell_bool_on_sub(MAIN,'',row,mapped['aim'],1 if spec['aim'] else 0),'Aim role write failed')
            if spec['role']=='takeoff':
                require(CU.set_cell_output_struct_field_on_sub(MAIN,'',row,mapped['outputs'],'BlendTime','0.05'),'Takeoff blend write failed')
        EAL.set_metadata_tag(asset,PIVOT_MANIFEST,manifest['pivot_manifest_after'])
        require(CU.compile_and_save(MAIN),'Jump chooser compile/save failed')
        final=verify(asset,manifest,setup,text,transition,pivot)
        EAL.set_metadata_tag(asset,STATE_KEY,'complete'); require(CU.compile_and_save(MAIN),'Completion save failed')
        report.update(status='prepared',row_count=len(final['rows']))
        return report
    except Exception as error:
        if report is not None: report.update(status='failed',error=str(error))
        raise
    finally:
        if report is not None:
            if receipt_path:
                path=Path(receipt_path); path.parent.mkdir(parents=True,exist_ok=True)
                path.write_text(json.dumps(report,indent=2),encoding='utf-8')
            print('[P01JumpChooser] '+json.dumps({k:report.get(k) for k in ('status','row_count','game_world','error')}))
        gc.collect()


if __name__=='__main__': main()
