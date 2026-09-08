# @Description: Audit and explicitly repair31 P01 enum byte/name pairs after reload.
"""SOURCE-AUTHORED tool; main() is read-only. Root coordinates prepare separately.

UE5.8 reload resolves FChooserEnumRowData.Value from ValueName. Earlier cloned
rows changed only Value, so18 Jog gates,4 crouch135 buckets and9 Sprint fallback
gates reverted. This writes BOTH fields directly on those31 exact cells, never
touches MatchAny payloads, and refreshes only the affected guard fingerprints.
No clip/AnimBP edits, PIE, tests, build or Live Coding. Native utility correction
in AZ_ChooserUtils.cpp is staged separately for subsequent editor authoring.
"""
import copy
import gc
import hashlib
import json
import re
from pathlib import Path
import runpy

import unreal

PROJECT = Path('C:/UnrealEngine/Games/AZ')
MAIN = '/Game/AZ/Blueprints/Animation/MotionMatching/CHT_v2_CharacterAnimations'
REFERENCE = PROJECT / 'Saved/RifleAnimationContent/p01-jump-chooser-prepare.json'
OWNER, OWNER_KEY, STATE_KEY = 'rifle_p01_enum_persistence_fix:v1', 'AZ.P01EnumRepair.Owner', 'AZ.P01EnumRepair.State'
MANIFEST_KEY = 'AZ.P01EnumRepair.Manifest'
PIVOT_KEY, JUMP_KEY = 'AZ.P01Pivot.Manifest', 'AZ.P01JumpChooser.Manifest'
EAL, CU = unreal.EditorAssetLibrary, unreal.AZ_ChooserUtils
RUN_ROWS = [192, 193, 194, 203, 204, 205, 206, 207, 208, 210, 211, 212, 221, 222, 223, 224, 225, 226]
REPAIRS = ([dict(row=row, field='gait', wanted='Run', wrong='Walk', anchor=9) for row in RUN_ROWS]
           + [dict(row=row, field='start', wanted=value, wrong='Fwd', anchor=14 if value == 'L135' else 15)
              for row, value in ((233, 'L135'), (234, 'R135'), (251, 'L135'), (252, 'R135'))]
           + [dict(row=row, field='gait', wanted='Sprint', wrong='Walk', anchor=85) for row in range(267, 276)])


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def libraries():
    return tuple(runpy.run_path(str(PROJECT / 'Tools' / name), run_name='p01_enum_repair_dependency')
                 for name in ('rifle_p01_setup.py', 'rifle_p01_activate.py', 'rifle_p01_transition_setup.py',
                              'rifle_p01_pivot_setup.py', 'rifle_p01_jump_chooser_setup.py'))


def reference_rows(dump):
    result = []
    for line in dump.splitlines():
        match = re.match(r'^\s*\{\s*"i"\s*:\s*(\d+)', line)
        if match:
            result.append({'index': int(match.group(1)), 'line': line,
                           'cells': {int(i): value for i, value in re.findall(r'"c(\d+)":\s*"([^"]*)"', line)}})
    return result


def enum_semantics(row, column, setup):
    comparison, value = setup['enum_cell'](row, column)
    return comparison, value.casefold() if value is not None else None


def metadata(asset, key):
    # The editor metadata interface can initially return an empty map after load.
    EAL.get_metadata_tag_values(asset)
    value = EAL.get_metadata_tag(asset, key)
    if not value:
        EAL.get_metadata_tag_values(asset)
        value = EAL.get_metadata_tag(asset, key)
    require(value, 'Required saved authoring metadata is unavailable: ' + key)
    return value


def enum_cell_fields(row, text):
    fields = text['fields'](row)
    names = {key for key, _ in fields}
    require(names <= {'Value', 'ValueName', 'Comparison', 'CompareNotEqual_DEPRECATED'}, 'Unexpected enum row fields')
    require(dict(fields).get('CompareNotEqual_DEPRECATED', 'False') == 'False', 'Deprecated comparison flag requires separate review')
    return fields


def verify_appended(snapshot, columns, mapped, manifest, setup, text, transition, pivot, jump):
    raw = [transition['column_data'](column, text)[3] for column in columns]
    special = {mapped[key] for key in ('positive', 'negative', 'not_sprinting', 'sprinting', 'aim', 'outputs')}
    enum_columns = {entry['index'] for entry in snapshot['columns'] if entry['type'] == 'EnumColumn'}
    for offset, spec in enumerate(manifest['specs']):
        index = manifest['base_rows'] + offset
        row, template = snapshot['rows'][index], snapshot['rows'][spec['template']]
        require(pivot['result_name'](row) == 'Asset[AnimSequence]:' + spec['asset'].rsplit('/', 1)[1], 'Unrelated jump result changed')
        for column in range(len(columns)):
            if column in special:
                continue
            if column in enum_columns:
                require(enum_semantics(row, column, setup) == enum_semantics(template, column, setup), 'Unrelated jump enum changed')
            else:
                require(raw[column][index] == raw[column][spec['template']], 'Unrelated jump filter changed')
        require(pivot['tag_names'](raw[mapped['positive']][index], text) == {'Weapon.Rifle'}
                and not pivot['tag_names'](raw[mapped['negative']][index], text), 'Jump rifle tag ownership changed')
        sprint = spec['role'] == 'sprint_idle'
        require(pivot['tag_names'](raw[mapped['sprinting']][index], text) == ({'Movement.Sprinting'} if sprint else set())
                and pivot['tag_names'](raw[mapped['not_sprinting']][index], text) == (set() if sprint else {'Movement.Sprinting'}),
                'Unrelated jump Sprint gates changed')
        require(row['cells'].get(mapped['aim']) == (template['cells'].get(mapped['aim']) if sprint else 'True' if spec['aim'] else 'False'),
                'Unrelated jump aim filter changed')
        outputs, source_outputs = jump['output_values'](row, mapped['outputs']), jump['output_values'](template, mapped['outputs'])
        expected = dict(source_outputs)
        if spec['role'] == 'takeoff': expected['blend_time'] = 0.05
        require(outputs == expected, 'Unrelated jump output policy changed')


def audit():
    setup, text, transition, pivot, jump = libraries()
    require(len(REPAIRS) == 31, 'Expected31 explicit repair cells')
    reference = json.loads(REFERENCE.read_text(encoding='utf-8'))
    baseline = reference['manifest']
    expected_rows = reference_rows(reference['original_dump'])
    require(baseline['base_rows'] == 280 and baseline['total_rows'] == 302 and len(expected_rows) == 280, 'Unexpected authoritative302 reference')
    asset = setup['load'](MAIN, unreal.ChooserTable)
    for key in ('AZ.P01JumpChooser.State', 'AZ.P01Pivot.State', 'AZ.RifleP01Transitions.State'):
        require(metadata(asset, key) == 'complete', 'An earlier authoring pass is incomplete')
    current_pivot_text, current_jump_text = metadata(asset, PIVOT_KEY), metadata(asset, JUMP_KEY)
    current_pivot, current_jump = json.loads(current_pivot_text), json.loads(current_jump_text)
    snapshot = setup['chooser_snapshot'](MAIN)
    columns, mapped = jump['columns_for'](asset, snapshot, text, transition, pivot)
    require(len(snapshot['rows']) == 302 and len(columns) == len(baseline['prefix_columns']) + 1, 'Master surface changed; coordinate before repair')
    require([pivot['result_name'](row) for row in snapshot['rows'][:280]] == baseline['original_results'], 'An original result changed')
    require(setup['chooser_disabled_rows'](MAIN) == baseline['original_disabled'] + [False] * 22, 'A row enable state changed')
    raw = [transition['column_data'](column, text)[3] for column in columns]
    require(all(len(rows) == 302 for rows in raw), 'Incomplete current column array')
    signatures = [pivot['signature'](column, text, transition) for column in columns]
    require(len(signatures) == len(set(signatures)), 'Ambiguous current column signature')
    by_signature = {sig: index for index, sig in enumerate(signatures)}
    affected = {mapped['gait'], mapped['start']}
    direction = setup['column_for_enum'](snapshot, 'EAZ_MovementDirection')
    comparable = affected | {direction}
    repair_keys = {(spec['row'], mapped[spec['field']]): spec for spec in REPAIRS}
    ignored = []
    for old_index, saved in enumerate(baseline['prefix_columns']):
        require(saved['signature'] in by_signature, 'A saved core column binding changed')
        column = by_signature[saved['signature']]
        if column not in comparable:
            require(digest(raw[column][:280]) == saved['expected_hash'], 'Unrelated prefix column changed: ' + saved['signature'])
            continue
        for index in range(280):
            enum_cell_fields(raw[column][index], text)
            expected = enum_semantics(expected_rows[index], old_index, setup)
            actual = enum_semantics(snapshot['rows'][index], column, setup)
            spec = repair_keys.get((index, column))
            if spec:
                require(expected == ('=', spec['wanted'].casefold()), 'Repair differs from authoritative saved selection')
                require(actual in (expected, ('=', spec['wrong'].casefold())), 'Unexpected active enum change at repair row ' + str(index))
            else:
                require(actual == expected, 'Unreviewed active enum change at row ' + str(index))
            if expected[0] == 'Any' and snapshot['rows'][index]['cells'][column] != expected_rows[index]['cells'][old_index]:
                ignored.append({'row': index, 'column': column, 'before_display': expected_rows[index]['cells'][old_index],
                                'current_display': snapshot['rows'][index]['cells'][column], 'action': 'preserve_ignored_payload'})
    verify_appended(snapshot, columns, mapped, baseline, setup, text, transition, pivot, jump)
    planned = [list(values) for values in raw]
    repairs = []
    for spec in REPAIRS:
        column, index = mapped[spec['field']], spec['row']
        require(enum_semantics(snapshot['rows'][spec['anchor']], column, setup) == ('=', spec['wanted'].casefold()), 'Stable enum-name anchor changed')
        anchor = dict(enum_cell_fields(raw[column][spec['anchor']], text))
        require(anchor.get('ValueName', '').strip('"').split('::')[-1] == spec['wanted'], 'Anchor has no canonical matching ValueName')
        before_fields = enum_cell_fields(raw[column][index], text)
        require(dict(before_fields).get('Comparison', 'MatchEqual').split('::')[-1] == 'MatchEqual', 'Repair must remain an exact-match cell')
        after_fields = text['replace_field'](text['replace_field'](before_fields, 'Value', anchor.get('Value', '0')),
                                           'ValueName', anchor['ValueName'])
        after_row = text['encode_fields'](after_fields)
        planned[column][index] = after_row
        repairs.append(dict(spec, column=column, before=raw[column][index], after=after_row,
                            current_selection=snapshot['rows'][index]['cells'][column],
                            semantic_change=enum_semantics(snapshot['rows'][index], column, setup) != ('=', spec['wanted'].casefold())))
    # Canonicalize just the two edited columns in temporary value structs. No asset mutation.
    proposed_columns = list(columns)
    for column in affected:
        proposed_columns[column] = transition['with_rows'](columns[column], planned[column], text)
        planned[column] = transition['column_data'](proposed_columns[column], text)[3]
    for repair in repairs:
        repair['after'] = planned[repair['column']][repair['row']]
    for column, (before, after) in enumerate(zip(raw, planned)):
        for index, (a, b) in enumerate(zip(before, after)):
            require(a == b or (index, column) in repair_keys, 'Proposed repair touches unrelated or MatchAny payload')
    # Guard refreshes accept only the three already-proven enum columns. No old
    # hashes are trusted as evidence of correctness; the semantic checks above are.
    baseline_pivot = json.loads(baseline['pivot_manifest_after'])
    pivot_after = copy.deepcopy(current_pivot)
    require({k: v for k, v in current_pivot.items() if k != 'prefix_columns'}
            == {k: v for k, v in baseline_pivot.items() if k != 'prefix_columns'}, 'Unrelated pivot manifest fields changed')
    require(len(current_pivot['prefix_columns']) == len(baseline_pivot['prefix_columns']), 'Pivot guard count changed')
    refreshed = []
    for i, (current, saved) in enumerate(zip(current_pivot['prefix_columns'], baseline_pivot['prefix_columns'])):
        require({k: v for k, v in current.items() if k != 'hash'} == {k: v for k, v in saved.items() if k != 'hash'}, 'Pivot guard identity changed')
        column = by_signature[current['signature']]
        new_hash = digest(planned[column][:current_pivot['base_rows']])
        if column in comparable:
            require(current['hash'] in (saved['hash'], new_hash), 'Pivot guard was refreshed to unreviewed data')
            pivot_after['prefix_columns'][i]['hash'] = new_hash
            if current['hash'] != new_hash: refreshed.append({'owner': 'pivot', 'column': column, 'before': current['hash'], 'after': new_hash})
        else:
            require(current == saved, 'Unrelated pivot guard changed')
    jump_after = copy.deepcopy(current_jump)
    excluded = {'prefix_columns', 'pivot_manifest_after'}
    require({k: v for k, v in current_jump.items() if k not in excluded}
            == {k: v for k, v in baseline.items() if k not in excluded}, 'Unrelated jump manifest fields changed')
    require(json.loads(current_jump['pivot_manifest_after']) == current_pivot, 'Jump/pivot guard records disagree')
    require(len(current_jump['prefix_columns']) == len(baseline['prefix_columns']), 'Jump guard count changed')
    for i, (current, saved) in enumerate(zip(current_jump['prefix_columns'], baseline['prefix_columns'])):
        require({k: v for k, v in current.items() if k != 'expected_hash'}
                == {k: v for k, v in saved.items() if k != 'expected_hash'}, 'Jump guard identity/history changed')
        column = by_signature[current['signature']]
        new_hash = digest(planned[column][:baseline['base_rows']])
        if column in comparable:
            require(current['expected_hash'] in (saved['expected_hash'], new_hash), 'Jump guard was refreshed to unreviewed data')
            jump_after['prefix_columns'][i]['expected_hash'] = new_hash
            if current['expected_hash'] != new_hash: refreshed.append({'owner': 'jump', 'column': column, 'before': current['expected_hash'], 'after': new_hash})
        else:
            require(current == saved, 'Unrelated jump guard changed')
    pivot_after_text = json.dumps(pivot_after, sort_keys=True)
    jump_after['pivot_manifest_after'] = pivot_after_text
    return {'status': 'audit_complete', 'mode': 'audit', 'game_world': pivot['game_world'](), 'expected_semantic_repairs': 31,
            'actual_semantic_repairs': sum(item['semantic_change'] for item in repairs), 'repairs': repairs,
            'ignored_payload_differences_preserved': ignored, 'guard_hash_updates': refreshed,
            'row_count': 302, 'original_dump': snapshot['text'],
            'column_signatures': signatures, 'original_column_hashes': [digest(values) for values in raw],
            'planned_column_hashes': [digest(values) for values in planned],
            'pivot_manifest_before': current_pivot_text, 'pivot_manifest_after': pivot_after_text,
            'jump_manifest_before': current_jump_text, 'jump_manifest_after': json.dumps(jump_after, sort_keys=True),
            'reference_receipt': str(REFERENCE)}


def main(prepare=False, receipt_path=None):
    report = None
    try:
        report = audit()
        if not prepare: return report
        setup, text, transition, pivot, jump = libraries()
        require(pivot['game_world']() is None, 'No enum repair writes while a game world exists')
        asset = setup['load'](MAIN, unreal.ChooserTable)
        require(setup['chooser_snapshot'](MAIN)['text'] == report['original_dump'], 'Master changed after strict preflight')
        require(metadata(asset, PIVOT_KEY) == report['pivot_manifest_before'] and metadata(asset, JUMP_KEY) == report['jump_manifest_before'],
                'Audit manifests changed after preflight')
        report['backup'] = setup['backup_existing']([MAIN])
        require(pivot['game_world']() is None, 'Game world appeared before mutation')
        columns = list(asset.get_editor_property('columns_structs'))
        raw = [transition['column_data'](column, text)[3] for column in columns]
        for repair in report['repairs']:
            require(raw[repair['column']][repair['row']] == repair['before'], 'Repair cell changed after preflight')
            raw[repair['column']][repair['row']] = repair['after']
        EAL.set_metadata_tag(asset, OWNER_KEY, OWNER)
        EAL.set_metadata_tag(asset, STATE_KEY, 'preparing')
        setup['save'](asset)
        for column in {repair['column'] for repair in report['repairs']}:
            columns[column] = transition['with_rows'](columns[column], raw[column], text)
        asset.set_editor_property('columns_structs', columns)
        EAL.set_metadata_tag(asset, PIVOT_KEY, report['pivot_manifest_after'])
        EAL.set_metadata_tag(asset, JUMP_KEY, report['jump_manifest_after'])
        require(CU.compile_and_save(MAIN), 'Repaired chooser compile/save failed')
        actual = list(asset.get_editor_property('columns_structs'))
        require([pivot['signature'](column, text, transition) for column in actual] == report['column_signatures'], 'Column bindings/order changed')
        require([digest(transition['column_data'](column, text)[3]) for column in actual] == report['planned_column_hashes'], 'A cell changed outside the exact repair plan')
        verified = audit()
        require(verified['actual_semantic_repairs'] == 0 and not verified['guard_hash_updates'], 'Enum repair or coordinated guard refresh is incomplete')
        jump['audit']()  # Current saved302 whole-table contract must now pass again.
        EAL.set_metadata_tag(asset, STATE_KEY, 'complete')
        EAL.set_metadata_tag(asset, MANIFEST_KEY, json.dumps({'repairs': REPAIRS, 'planned_column_hashes': report['planned_column_hashes']}, sort_keys=True))
        require(CU.compile_and_save(MAIN), 'Repair completion save failed')
        report.update(status='prepared_requires_later_reload_readback', mode='prepare')
        return report
    except Exception as error:
        if report is not None: report.update(status='failed', error=str(error))
        raise
    finally:
        if report is not None:
            if receipt_path:
                path = Path(receipt_path); path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(json.dumps(report, indent=2), encoding='utf-8')
            print('[P01EnumPersistence] ' + json.dumps({k: report.get(k) for k in (
                'status', 'mode', 'game_world', 'expected_semantic_repairs', 'actual_semantic_repairs', 'row_count', 'error')}))
        gc.collect()


if __name__ == '__main__': main()
