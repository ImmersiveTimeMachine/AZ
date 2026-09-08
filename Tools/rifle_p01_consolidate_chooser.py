# @Description: Audit or consolidate validated P01 rows into the existing main character chooser.
"""SOURCE-AUTHORED editor tool. main() audits; prepare=True explicitly writes.

The existing main CHT remains the only runtime selector. The owned P01 CHT is
read-only staging: its155 complete column arrays supply the already-authored
cell data after52 exact P01 result rows are appended to the original main103.
No protected ResultsStructs property is read or assigned. All original results,
cells and disabled flags are verified and retained. Two native tag columns
route Weapon.Rifle before Randomize; empty cells also pass inverted matching.

No animation edits, Blueprint compilation, graph edits, editor tests or PIE.
The source package is backed up before the first mutation. An interrupted
partial consolidation fails explicitly on rerun; it never guesses a rollback.
"""

import gc
import hashlib
import json
import runpy
from pathlib import Path

import unreal


MAIN = '/Game/AZ/Blueprints/Animation/MotionMatching/CHT_v2_CharacterAnimations'
STAGING = '/Game/AZ/Blueprints/Animation/MotionMatching/RifleP01/CHT_P01_CharacterAnimations'
OWNER = 'rifle_p01_consolidate_chooser:v1'
OWNER_KEY = 'AZ.RifleP01Consolidation.Owner'
STATE_KEY = 'AZ.RifleP01Consolidation.State'
MANIFEST_KEY = 'AZ.RifleP01Consolidation.Manifest'
TOTAL_ROWS = 155
BASE_ROWS = 103
EAL = unreal.EditorAssetLibrary
CU = unreal.AZ_ChooserUtils


def require(value, message):
    if not value:
        raise RuntimeError(message)


def libraries():
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    setup = runpy.run_path(str(project / 'Tools/rifle_p01_setup.py'), run_name='rifle_p01_setup_library')
    text = runpy.run_path(str(project / 'Tools/rifle_p01_activate.py'), run_name='rifle_p01_activation_library')
    return setup, text


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode('utf-8')).hexdigest()


def result_description(row):
    import re
    match = re.search(r'"out":\s*"([^"]*)"', row['line'])
    require(match is not None, 'Chooser row result diagnostic is unavailable')
    return match.group(1)


def original_rows_fingerprint(snapshot, column_map):
    rows = []
    for row in snapshot['rows'][:BASE_ROWS]:
        cells = {str(old): row['cells'].get(current) for old, current in enumerate(column_map)}
        rows.append(dict(index=row['index'], cells=cells, result=result_description(row)))
    return digest(rows)


def column_signature(column, text):
    kind, values = text['fragment_parts'](column.export_text())
    # InputValue includes its exact context/property binding. Other column fields
    # and all row arrays stay in the copied struct, not reauthored from a dump.
    return kind, dict(values).get('InputValue', '')


def map_original_columns(main_columns, staging_columns, text):
    taken, mapping = set(), []
    for column in main_columns:
        signature = column_signature(column, text)
        candidates = [i for i, candidate in enumerate(staging_columns)
                      if i not in taken and column_signature(candidate, text) == signature]
        require(bool(candidates), 'Staging lost or changed an original column binding')
        index = candidates[0]
        mapping.append(index)
        taken.add(index)
    return mapping


def tag_column(inverted, rifle_rows, text):
    marked = set(rifle_rows)
    require(all(0 <= index < TOTAL_ROWS for index in marked), 'Tag-filter row index is invalid')
    values = ['(GameplayTags=((TagName="Weapon.Rifle")))' if i in marked else '()'
              for i in range(TOTAL_ROWS)]
    exported = (
        '/Script/Chooser.GameplayTagColumn('
        'InputValue=/Script/Chooser.GameplayTagContextProperty('
        'Binding=(PropertyBindingChain=("ChooserContext","OwnedTags"),ContextIndex=0)),'
        'TagMatchType=Any,TagMatchDirection=RowValueInInput,bMatchExact=False,'
        'bInvertMatchingLogic=' + ('True' if inverted else 'False') + ',RowValues=('
        + ','.join(values) + '))')
    value = unreal.InstancedStruct()
    require(value.import_text(exported), 'Native GameplayTagColumn ImportText failed')
    verify_tag_column(value, inverted, marked, text)
    return value


def verify_tag_column(column, inverted, rifle_rows, text):
    kind, raw = text['fragment_parts'](column.export_text())
    require(kind == '/Script/Chooser.GameplayTagColumn', 'Expected a native GameplayTagColumn')
    values = dict(raw)
    require(values.get('TagMatchType') == 'Any', 'Tag column must use Any matching')
    require(values.get('TagMatchDirection') == 'RowValueInInput', 'Tag column match direction changed')
    require(values.get('bMatchExact') == 'False', 'Tag column must allow hierarchical tags')
    require(values.get('bInvertMatchingLogic') == ('True' if inverted else 'False'), 'Tag inversion changed')
    parameter_type, parameter = text['fragment_parts'](values['InputValue'])
    require(parameter_type == '/Script/Chooser.GameplayTagContextProperty', 'Tag column input parameter type changed')
    binding = dict(text['fields'](dict(parameter)['Binding']))
    chain = text['split_top_level'](text['parenthesized'](binding['PropertyBindingChain']))
    require([part.strip('"') for part in chain] == ['ChooserContext', 'OwnedTags']
            and binding.get('ContextIndex') == '0', 'Tag context must bind AnimInstance.ChooserContext.OwnedTags')
    rows = text['split_top_level'](text['parenthesized'](values.get('RowValues', '')))
    require(len(rows) == TOTAL_ROWS, 'Tag column must contain exactly155 cells')
    found = set()
    for index, row in enumerate(rows):
        payload = dict(text['fields'](row)).get('GameplayTags', '')
        entries = text['split_top_level'](text['parenthesized'](payload))
        if entries:
            require(len(entries) == 1 and dict(text['fields'](entries[0])).get('TagName', '').strip('"') == 'Weapon.Rifle',
                    'Unexpected tag in chooser row ' + str(index))
            found.add(index)
    require(found == set(rifle_rows), 'Tag-filter row membership differs from the reviewed plan')


def verify_consolidated(asset, manifest, setup, text):
    snapshot = setup['chooser_snapshot'](MAIN)
    require(len(snapshot['rows']) == TOTAL_ROWS, 'Consolidated main row count changed')
    require(original_rows_fingerprint(snapshot, manifest['original_column_map']) == manifest['original_rows_hash'],
            'An original main chooser row cell or result changed')
    columns = list(asset.get_editor_property('columns_structs'))
    require(len(columns) == manifest['final_column_count'], 'Consolidated column count changed')
    verify_tag_column(columns[manifest['positive_tag_column']], False, range(BASE_ROWS, TOTAL_ROWS), text)
    verify_tag_column(columns[manifest['negative_tag_column']], True, manifest['normal_source_rows'], text)
    randoms = [column['index'] for column in snapshot['columns'] if column['type'] == 'RandomizeColumn']
    require(len(randoms) == 1 and manifest['positive_tag_column'] < randoms[0]
            and manifest['negative_tag_column'] < randoms[0], 'Weapon tag filters must precede Randomize')
    disabled = setup['chooser_disabled_rows'](MAIN)
    require(disabled[:BASE_ROWS] == manifest['original_disabled_rows'] and not any(disabled[BASE_ROWS:]),
            'Original disabled flags or appended row enable states changed')
    for row, specification in zip(snapshot['rows'][BASE_ROWS:], setup['library_rows']()):
        expected_name = specification['asset'].rsplit('/', 1)[1]
        require(result_description(row) == 'Asset[AnimSequence]:' + expected_name, 'Appended P01 result changed')
    return snapshot


def audit():
    setup, text = libraries()
    asset = setup['load'](MAIN, unreal.ChooserTable)
    metadata = {str(key): str(value) for key, value in EAL.get_metadata_tag_values(asset).items()}
    owner = metadata.get(OWNER_KEY, '')
    if owner:
        require(owner == OWNER, 'Main chooser has an unrecognized consolidation owner')
        require(metadata.get(STATE_KEY) == 'complete', 'Partial consolidation needs inspection of its backup before rerun')
        manifest = json.loads(metadata[MANIFEST_KEY])
        snapshot = verify_consolidated(asset, manifest, setup, text)
        return dict(mode='audit', status='already_consolidated', main=MAIN, manifest=manifest,
                    row_count=len(snapshot['rows']), no_work_remaining=True)
    # The staging builder's own audit verifies owner/hash, source103, exact52
    # sequence identities, six database memberships and required native helpers.
    prepared = setup['audit'](build_chooser=True)
    require(not prepared['pending'], 'Staging authoring dependencies are pending: ' + json.dumps(prepared['pending']))
    require(prepared['chooser']['existing'], 'Validated owned staging chooser is missing')
    main = setup['chooser_snapshot'](MAIN)
    staging = setup['chooser_snapshot'](STAGING)
    require(len(main['rows']) == BASE_ROWS and len(staging['rows']) == TOTAL_ROWS, 'Expected source103 and staging155 rows')
    staging_asset = setup['load'](STAGING, unreal.ChooserTable)
    main_columns = list(asset.get_editor_property('columns_structs'))
    staging_columns = list(staging_asset.get_editor_property('columns_structs'))
    mapping = map_original_columns(main_columns, staging_columns, text)
    original_hash = original_rows_fingerprint(main, list(range(len(main_columns))))
    require(original_rows_fingerprint(staging, mapping) == original_hash, 'Staging no longer preserves the original103 source row cells/results')
    for row, specification in zip(staging['rows'][BASE_ROWS:], setup['library_rows']()):
        require(result_description(row) == 'Asset[AnimSequence]:' + specification['asset'].rsplit('/', 1)[1],
                'Staging appended results do not match the explicit P01 library')
    randoms = [column['index'] for column in staging['columns'] if column['type'] == 'RandomizeColumn']
    require(len(randoms) == 1, 'Staging must have one Randomize column')
    insertion = randoms[0]
    normal_rows = prepared['chooser']['plan']['disable_rows']
    # Construct and read back native value structs during audit; no asset is mutated.
    tag_column(False, range(BASE_ROWS, TOTAL_ROWS), text)
    tag_column(True, normal_rows, text)
    manifest = dict(original_rows_hash=original_hash,
                    original_column_map=[i + 2 if i >= insertion else i for i in mapping],
                    original_disabled_rows=setup['chooser_disabled_rows'](MAIN),
                    positive_tag_column=insertion, negative_tag_column=insertion + 1,
                    final_column_count=len(staging_columns) + 2, normal_source_rows=normal_rows,
                    preserve_reaction_rows=prepared['chooser']['plan']['preserve_reaction_rows'],
                    shared_sprint_row=prepared['chooser']['plan']['shared_sprint_row'],
                    staging_dump_hash=digest(staging['text']))
    return dict(mode='audit', status='audit_complete', main=MAIN, staging=STAGING,
                manifest=manifest, original_main_dump=main['text'], no_work_remaining=False)


def main(prepare=False, backup_dir=None, receipt_path=None):
    report = None
    try:
        report = audit()
        if not prepare or report['no_work_remaining']:
            return report
        setup, text = libraries()
        report['backup'] = setup['backup_existing']([MAIN], backup_dir)
        asset = setup['load'](MAIN, unreal.ChooserTable)
        staging_asset = setup['load'](STAGING, unreal.ChooserTable)
        manifest = report['manifest']
        # Validate the source fingerprint again immediately before any mutation.
        current = setup['chooser_snapshot'](MAIN)
        require(current['text'] == report['original_main_dump'], 'Main chooser changed after preflight')
        EAL.set_metadata_tag(asset, OWNER_KEY, OWNER)
        EAL.set_metadata_tag(asset, STATE_KEY, 'preparing')
        EAL.set_metadata_tag(asset, MANIFEST_KEY, json.dumps(manifest, sort_keys=True))
        setup['save'](asset)
        report.update(mode='prepare', status='preparing')
        for offset, specification in enumerate(setup['library_rows']()):
            result = CU.add_empty_row_to_sub(MAIN, '', setup['load'](specification['asset'], unreal.AnimSequence))
            require(result == BASE_ROWS + offset, 'P01 result did not append at the expected row')
        # Copy only fully-authored column value structs. Original results remain
        # untouched; appended results were supplied via supported native AddRow.
        columns = list(staging_asset.get_editor_property('columns_structs'))
        insertion = manifest['positive_tag_column']
        columns[insertion:insertion] = [
            tag_column(False, range(BASE_ROWS, TOTAL_ROWS), text),
            tag_column(True, manifest['normal_source_rows'], text),
        ]
        asset.set_editor_property('columns_structs', columns)
        require(CU.set_chooser_rows_disabled(MAIN, list(range(TOTAL_ROWS)), False), 'Could not initialize consolidated row enable states')
        original_disabled = [i for i, disabled in enumerate(manifest['original_disabled_rows']) if disabled]
        if original_disabled:
            require(CU.set_chooser_rows_disabled(MAIN, original_disabled, True), 'Could not restore original disabled flags')
        require(CU.compile_and_save(MAIN), 'Main chooser compile/save failed')
        snapshot = verify_consolidated(asset, manifest, setup, text)
        EAL.set_metadata_tag(asset, STATE_KEY, 'complete')
        require(CU.compile_and_save(MAIN), 'Consolidation completion save failed')
        report.update(status='consolidated', row_count=len(snapshot['rows']), final_dump=snapshot['text'])
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
            print('[RifleP01Consolidation] ' + json.dumps({key: report.get(key) for key in ('mode', 'status', 'row_count', 'error')}))
        gc.collect()


if __name__ == '__main__':
    main()
