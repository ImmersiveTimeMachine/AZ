# @Description: Audit, back up, author, or verify only the nine M16 magazine footprints.
"""Guarded editor helper. Importing this file never mutates Unreal assets.

main() audits. Run main('backup'), then main('author') to change the nine known
magazine GridSize values from 1x2 to 1x1. Authoring requires its verified backup
receipt and refuses PIE. After Python returns, compile only the four reported
Blueprints and save only the five reported packages using dedicated tools.
Run main('verify') afterward; it reacquires every template and placed component.

No compilation, saving, PIE control, tests, runtime inventory mutation, actor
reconstruction, or property-change notifications occur in this helper.
"""
import gc
import hashlib
import json
import runpy
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path

import unreal


ROOT = Path('C:/UnrealEngine/Games/AZ')
OUTPUT = ROOT / 'Saved/MagazineDisplayFix'
RECEIPT = OUTPUT / 'grid-backup-receipt.json'
SCHEMA = 'magazine_grid_one_cell:v1'
BASE = '/Game/AZ/Blueprints/Items/Equippables/Weapons/'
MAGAZINE = BASE + 'BP_Pickup_M16Magazine'
RIFLE = BASE + 'BPAZ_CommonUI_PickupItem'
MAP = '/Game/AZ/Maps/L_001'
BLUEPRINTS = (MAGAZINE, MAGAZINE + '_Partial', MAGAZINE + '_Empty', RIFLE)
PACKAGES = BLUEPRINTS + (MAP,)
ROUNDS = {MAGAZINE: 30, MAGAZINE + '_Partial': 17,
          MAGAZINE + '_Empty': 0, RIFLE: 30}
PLACED = {
    'BP_Pickup_M16Magazine_C_0': MAGAZINE,
    'BP_Pickup_M16Magazine_Partial_C_0': MAGAZINE + '_Partial',
    'BP_Pickup_M16Magazine_Empty_C_0': MAGAZINE + '_Empty',
    'BPAZ_CommonUI_PickupItem_C_2': RIFLE,
    'BPAZ_CommonUI_PickupItem_C_4': RIFLE,
}
GRID_FRAGMENT = '/Script/AZ.AZ_Inv_CommonUI_GridFragment'
MAGAZINE_FRAGMENT = '/Script/AZ.AZ_Inv_CommonUI_MagazineFragment'
OLD_SIZE = '(X=1,Y=2)'
NEW_SIZE = '(X=1,Y=1)'


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def helpers():
    # The old inventory foundation script executes main() on import. This
    # parser/template helper has a safe __main__ guard and preserves ExportText.
    previous = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        return runpy.run_path(str(ROOT / 'Tools/rifle_p01_activate.py'),
                              run_name='magazine_grid_manifest_helpers')
    finally:
        sys.dont_write_bytecode = previous


def write_report(name, report):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    destination = OUTPUT / name
    temporary = destination.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(report, indent=2), encoding='utf-8')
    temporary.replace(destination)


def package_file(package):
    require(package in PACKAGES, 'Unexpected package: ' + package)
    content = (ROOT / 'Content').resolve()
    target = (content / (package.removeprefix('/Game/')
                         + ('.umap' if package == MAP else '.uasset'))).resolve()
    require(target.is_relative_to(content), 'Package resolves outside project content')
    return target


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def dirty_packages():
    return sorted({p.get_path_name() for p in
                   unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
                   + unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()})


def context(H, editing=False):
    """Fresh identities each call; never retain wrappers across a native compile."""
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
    require(project == ROOT.resolve(), 'The connected editor is not this AZ workspace')
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    pie = bool(editor.get_game_world())
    require(not editing or not pie, 'Stop PIE before magazine footprint authoring or backup')
    world = editor.get_editor_world()
    if not world and not editing:
        # The saved editor world remains readable during PIE. Never use clones.
        world = unreal.find_object(None, MAP + '.L_001')
    require(world and world.get_path_name() == MAP + '.L_001',
            'The audited editor map /Game/AZ/Maps/L_001 must be open')
    blueprints = {path: unreal.load_asset(path) for path in BLUEPRINTS}
    require(all(isinstance(bp, unreal.Blueprint) and bp.generated_class()
                for bp in blueprints.values()), 'A target Blueprint is missing')
    classes = {bp.generated_class() for bp in blueprints.values()}
    actors = {a.get_name(): a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
              if a.get_class() in classes}
    require(set(actors) == set(PLACED), 'The audited five placed pickup actors changed')
    entries = []
    for path in BLUEPRINTS:
        entries.append({'key': 'template:' + path, 'package': path, 'blueprint': path,
                        'component': H['item_component_template'](blueprints[path])})
    for name, path in PLACED.items():
        actor = actors[name]
        require(actor.get_class() == blueprints[path].generated_class(),
                'Placed pickup class changed: ' + name)
        require(actor.get_path_name() == MAP + '.L_001:PersistentLevel.' + name,
                'Placed pickup is not the audited editor instance: ' + name)
        entries.append({'key': 'placed:' + name, 'package': MAP, 'blueprint': path,
                        'component': actor.get_component_by_class(unreal.AZ_Inv_CommonUI_ItemComponent)})
    require(len(entries) == 9 and all(e['component'] for e in entries),
            'Expected nine distinct item component targets')
    require(len({e['component'].get_path_name() for e in entries}) == 9,
            'A target component was duplicated')
    return {'world': world, 'blueprints': blueprints, 'actors': actors,
            'entries': entries, 'pie': pie, 'dirty_packages': dirty_packages()}


def capture(entry):
    component = entry['component']
    # Only authorable properties are exposed by these component templates.
    # Transient pickup state is deliberately neither accessed nor written.
    return {'key': entry['key'], 'package': entry['package'], 'blueprint': entry['blueprint'],
            'component': component.get_path_name(),
            'manifest': component.get_editor_property('pickup_item_manifest').export_text(),
            'contained': [m.export_text() for m in
                          component.get_editor_property('initial_contained_item_manifests')],
            'pickup_message': component.get_editor_property('pickup_message'),
            'pickup_radius': component.get_editor_property('pickup_radius')}


def patch_manifest(H, text, rounds):
    """Change only the exact GridSize text in a validated magazine fragment."""
    top = dict(H['fields'](text))
    require(top.get('ItemTypeTag') == '(TagName="Item.Type.Magazine.Rifle")'
            and top.get('ItemCategory') == 'Equippable', 'Expected an equippable rifle magazine')
    fragments = H['split_top_level'](H['parenthesized'](top['Fragments']))
    grids, magazines = [], []
    for fragment in fragments:
        kind, values = H['fragment_parts'](fragment)
        if kind == GRID_FRAGMENT:
            grids.append((fragment, dict(values)))
        elif kind == MAGAZINE_FRAGMENT:
            magazines.append(dict(values))
    require(len(grids) == len(magazines) == 1, 'Expected one grid and one magazine fragment')
    magazine = magazines[0]
    require(magazine.get('MagazineFamily') == '"M16.Standard"'
            and magazine.get('Capacity') == '30'
            and magazine.get('InitialRounds') == str(rounds),
            'Magazine family, capacity, or initial rounds changed')
    grid_text, grid = grids[0]
    size = grid.get('GridSize')
    require(size in (OLD_SIZE, NEW_SIZE), 'Unexpected magazine footprint: ' + str(size))
    require(grid.get('FragmentTag') == '(TagName="Item.Fragment.Grid")', 'Grid fragment tag changed')
    if size == NEW_SIZE:
        return text
    old = 'GridSize=' + OLD_SIZE
    require(grid_text.count(old) == 1 and text.count(grid_text) == 1,
            'Ambiguous GridSize replacement')
    return text.replace(grid_text, grid_text.replace(old, 'GridSize=' + NEW_SIZE, 1), 1)


def proposed_record(H, record):
    result = dict(record)
    if record['blueprint'] == RIFLE:
        require(len(record['contained']) == 1, 'Rifle must retain its one inserted magazine definition')
        # The rifle root is deliberately never imported or written.
        result['contained'] = [patch_manifest(H, record['contained'][0], 30)]
    else:
        require(record['contained'] == [], 'Standalone magazine has unexpected contained definitions')
        result['manifest'] = patch_manifest(H, record['manifest'], ROUNDS[record['blueprint']])
    return result


def detached(text):
    manifest = unreal.AZ_Inv_CommonUI_ItemManifest()
    require(manifest.import_text(text), 'Magazine manifest ImportText failed')
    require(manifest.export_text() == text,
            'Detached import changed an unrelated exported manifest field; inspect before authoring')
    return manifest


def backup(H, C):
    require(not set(PACKAGES).intersection(C['dirty_packages']),
            'A target has unsaved edits; preserve them before creating its disk backup')
    records = [capture(e) for e in C['entries']]
    for row in records:
        proposed_record(H, row)
    sources = [package_file(p) for p in PACKAGES]
    require(all(p.is_file() for p in sources), 'A target package is missing on disk')
    destination = OUTPUT / 'Backups' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    destination.mkdir(parents=True)
    files = []
    for package, source in zip(PACKAGES, sources):
        saved = destination / source.relative_to(ROOT / 'Content')
        saved.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, saved)
        digest = sha256(source)
        require(sha256(saved) == digest, 'Backup checksum verification failed: ' + package)
        files.append({'package': package, 'source': str(source), 'backup': str(saved), 'sha256': digest})
    receipt = {'schema': SCHEMA, 'status': 'backed_up', 'backup': str(destination),
               'created_utc': datetime.now(timezone.utc).isoformat(), 'files': files,
               'components': records, 'unrelated_dirty_packages': C['dirty_packages']}
    (destination / 'receipt.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
    write_report(RECEIPT.name, receipt)
    return receipt


def baseline(C, receipt_path):
    path = Path(receipt_path) if receipt_path else RECEIPT
    require(path.is_file(), 'Run main("backup") before main("author"); a backup receipt is required')
    saved = json.loads(path.read_text(encoding='utf-8'))
    require(saved.get('schema') == SCHEMA and saved.get('status') == 'backed_up', 'Wrong backup receipt')
    require(len(saved['files']) == 5 and {r['package'] for r in saved['files']} == set(PACKAGES),
            'Backup package set differs')
    backup_root = (OUTPUT / 'Backups').resolve()
    for row in saved['files']:
        backup_file = Path(row['backup']).resolve()
        require(backup_file.is_relative_to(backup_root), 'Backup is outside the owned backup directory')
        require(backup_file.is_file() and sha256(backup_file) == row['sha256'],
                'A backup is missing or its checksum changed')
        require(Path(row['source']).resolve() == package_file(row['package']), 'Backup source path differs')
    originals = {row['key']: row for row in saved['components']}
    require(len(saved['components']) == len(originals) == 9
            and set(originals) == {e['key'] for e in C['entries']}, 'Backup component set differs')
    return saved, originals


def assert_records(H, originals, records, complete=False):
    require(len(records) == 9 and {r['key'] for r in records} == set(originals), 'Component set changed')
    for row in records:
        old = originals[row['key']]
        expected = proposed_record(H, old)
        require(row == expected if complete else row in (old, expected),
                'Unexpected manifest/state/property change: ' + row['key'])


def main(mode='audit', receipt_path=None):
    report = {'schema': SCHEMA, 'mode': mode}
    try:
        require(mode in ('audit', 'backup', 'author', 'verify'), 'Unknown mode: ' + str(mode))
        H = helpers()
        C = context(H, editing=mode in ('backup', 'author'))
        records = [capture(e) for e in C['entries']]
        proposed = [proposed_record(H, row) for row in records]
        report.update(components=records, proposed_components=proposed,
                      manifest_count=9, changes_required=sum(a != b for a, b in zip(records, proposed)),
                      explicit_target_packages=list(PACKAGES), pie_active=C['pie'],
                      dirty_packages=C['dirty_packages'])
        if mode == 'backup':
            return backup(H, C)
        if mode == 'audit':
            report['status'] = 'audit'
            write_report('grid-audit-readback.json', report)
            return report
        saved, originals = baseline(C, receipt_path)
        assert_records(H, originals, records)
        expected = {key: proposed_record(H, row) for key, row in originals.items()}
        changed = [row for row in records if row != expected[row['key']]]
        report.update(backup=saved['backup'],
                      compile_after_return=[p + '.' + p.rsplit('/', 1)[1] for p in BLUEPRINTS],
                      explicit_save_packages=list(PACKAGES))
        if mode == 'author' and changed:
            require(not set(PACKAGES).intersection(C['dirty_packages']),
                    'A target has unsaved edits; inspect before authoring')
            for row in saved['files']:
                require(sha256(package_file(row['package'])) == row['sha256'],
                        'A target changed on disk since backup; create a fresh backup receipt')
            # Preflight ALL detached candidates before the first Modify call.
            candidates = {}
            for row in changed:
                new = expected[row['key']]
                if row['blueprint'] == RIFLE:
                    candidates[row['key']] = ('initial_contained_item_manifests',
                                              [detached(text) for text in new['contained']])
                else:
                    candidates[row['key']] = ('pickup_item_manifest', detached(new['manifest']))
            for bp in C['blueprints'].values():
                bp.modify()
            C['world'].modify()
            for actor in C['actors'].values():
                actor.modify()
            for entry in C['entries']:
                if entry['key'] not in candidates:
                    continue
                field, value = candidates[entry['key']]
                component = entry['component']
                component.modify()
                # Default notifications reconstruct placed components into
                # TRASH_ objects. This data-only edit needs no reconstruction.
                component.set_editor_property(field, value,
                    notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
            report['status'] = 'authored_requires_external_compile_and_save'
        elif mode == 'author':
            report['status'] = 'already_authored'
        else:
            require(not changed, 'One or more magazines still require a 1x1 footprint')
            require(not set(PACKAGES).intersection(dirty_packages()),
                    'Save only the five reported packages externally before final verification')
            ready = (unreal.BlueprintStatus.BS_UP_TO_DATE,
                     unreal.BlueprintStatus.BS_UP_TO_DATE_WITH_WARNINGS)
            for path, bp in C['blueprints'].items():
                require(bp.get_editor_property('status') in ready,
                        'Compile the reported Blueprint externally before verifying: ' + path)
            report.update(status='verified_compiled_saved', compile_after_return=[], explicit_save_packages=[])
        # Reacquire by current Blueprint templates and canonical actor identity.
        # The same path is used on a separate verify call after native compile.
        C = context(H)
        fresh = [capture(e) for e in C['entries']]
        assert_records(H, originals, fresh, complete=True)
        report.update(components=fresh, dirty_packages=dirty_packages())
        write_report('grid-' + mode + '-readback.json', report)
        return report
    except Exception as error:
        report.update(status='failed', error=str(error))
        try:
            fresh = context(H)
            report['components_after_failure'] = [capture(e) for e in fresh['entries']]
        except Exception as capture_error:
            report['failure_readback_error'] = str(capture_error)
        write_report('grid-' + str(mode) + '-failed.json', report)
        raise
    finally:
        gc.collect()


if __name__ == '__main__':
    print(json.dumps(main(), indent=2))
