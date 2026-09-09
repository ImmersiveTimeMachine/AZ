# @Description: Back up, assign, or verify only the rifle world-impact effect and scale.
"""Guarded Unreal Python helper; importing it never authors or saves assets.

Call main('backup') before assignment (a pre-build backup is supported), then
main('assign', effect_path='/Game/.../P_Impact', scale=1.0) after the reflected
WorldImpactEffect/WorldImpactScale fields are loaded. Compile the returned
rifle Blueprint and save only the returned packages through dedicated tools
AFTER this Python call returns. Finally call main('verify', effect_path=...,
scale=...). An assignment rerun with the same settings is a verified no-op.

main() audits only. No Blueprint compilation, asset save, PIE, tests, effect
editing, inventory mutation, or animation authoring occurs in this helper.
The supplied effect must be an existing Cascade ParticleSystem. The native
weapon field is checked separately so a stale or mismatched build fails closed.
"""
import gc
import hashlib
import json
import math
import runpy
import shutil
import sys
from datetime import datetime, timezone
from pathlib import Path

import unreal


ROOT = Path('C:/UnrealEngine/Games/AZ')
OUTPUT = ROOT / 'Saved/BulletImpact'
RECEIPT = OUTPUT / 'backup-receipt.json'
RIFLE = '/Game/AZ/Blueprints/Items/Equippables/Weapons/BPAZ_CommonUI_PickupItem'
MAP = '/Game/AZ/Maps/L_001'
OWNED_PACKAGES = (RIFLE, MAP)
IMPACT_FIELDS = ('WorldImpactEffect', 'WorldImpactScale')
PLACED_ACTORS = {
    MAP + '.L_001:PersistentLevel.BPAZ_CommonUI_PickupItem_C_2',
    MAP + '.L_001:PersistentLevel.BPAZ_CommonUI_PickupItem_C_4',
}
SCHEMA = 'bullet_impact_assign:v1'


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def helpers():
    # This module has a safe __main__ guard. Never import the older inventory
    # foundation helper, which automatically executes its authoring routine.
    previous = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        return runpy.run_path(str(ROOT / 'Tools/rifle_p01_activate.py'),
                              run_name='bullet_impact_manifest_helpers')
    finally:
        sys.dont_write_bytecode = previous


def package(asset):
    return asset.get_path_name().split('.')[0]


def package_file(path):
    require(path in OWNED_PACKAGES, 'Unexpected target package: ' + path)
    content = (ROOT / 'Content').resolve()
    target = (content / (path[len('/Game/'):] + ('.umap' if path == MAP else '.uasset'))).resolve()
    require(target.is_relative_to(content), 'Package resolves outside project content')
    return target


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_receipt(name, report):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    destination = OUTPUT / name
    temporary = destination.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(report, indent=2), encoding='utf-8')
    temporary.replace(destination)


def dirty_packages():
    return sorted({p.get_path_name() for p in
                   unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
                   + unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()})


def context(H):
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()
    require(project == ROOT.resolve(), 'The connected editor is not this AZ workspace')
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(not editor.get_game_world(), 'Stop PIE before bullet-impact authoring')
    world = editor.get_editor_world()
    require(world and package(world) == MAP,
            'Open the audited L_001 map before authoring its rifle overrides')
    bp = unreal.load_asset(RIFLE)
    require(isinstance(bp, unreal.Blueprint) and bp.generated_class(), 'Rifle Blueprint is missing')
    placed = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
              if a.get_class() == bp.generated_class()]
    require({a.get_path_name() for a in placed} == PLACED_ACTORS,
            'The two audited L_001 rifle actors changed; inspect before authoring')
    placed.sort(key=lambda a: a.get_path_name())
    components = [H['item_component_template'](bp)] + [
        a.get_component_by_class(unreal.AZ_Inv_CommonUI_ItemComponent) for a in placed]
    require(len(components) == 3 and all(components), 'Rifle item component set changed')
    return {'world': world, 'blueprint': bp, 'actors': placed,
            'components': components, 'dirty_packages': dirty_packages()}


def capture(component):
    return {'component': component.get_path_name(),
            'manifest': component.get_editor_property('pickup_item_manifest').export_text(),
            'contained': [m.export_text() for m in
                          component.get_editor_property('initial_contained_item_manifests')]}


def separated(H, text):
    """Retain exact fragment text/order except the two explicitly owned fields."""
    top = dict(H['fields'](text))
    require('Fragments' in top, 'Existing manifest has no Fragments field')
    fragments = H['split_top_level'](H['parenthesized'](top.pop('Fragments')))
    kept, impact, count = [], {}, 0
    for fragment in fragments:
        kind, values = H['fragment_parts'](fragment)
        if kind == H['WEAPON_FRAGMENT']:
            count += 1
            current = dict(values)
            require(current.get('bUsesDetachableMagazines') == 'True'
                    and current.get('WeaponTag') == '(TagName="Weapon.Rifle")',
                    'Expected the established detachable-magazine rifle')
            impact = {key: current[key] for key in IMPACT_FIELDS if key in current}
            fragment = kind + H['encode_fields']([(k, v) for k, v in values if k not in IMPACT_FIELDS])
        kept.append(fragment)
    require(count == 1, 'Expected exactly one existing weapon-state fragment')
    return top, kept, impact


def assert_preserved(H, before, after, expected=None):
    old_top, old_fragments, _ = separated(H, before)
    new_top, new_fragments, impact = separated(H, after)
    require(old_top == new_top, 'Unrelated top-level manifest fields changed')
    require(old_fragments == new_fragments,
            'An unrelated fragment, animation, reticle, muzzle, sound, ammo, or ability field changed')
    if expected is not None:
        require(impact == expected, 'WorldImpactEffect/WorldImpactScale readback differs')


def settings(H, effect_path, scale):
    require(isinstance(effect_path, str) and effect_path.startswith('/Game/'),
            'Provide an existing /Game/ Cascade effect path')
    require(not isinstance(scale, bool) and isinstance(scale, (int, float))
            and math.isfinite(scale) and 0 < scale <= 3.4028234663852886e38,
            'Impact scale must be a finite positive float')
    effect = unreal.load_asset(effect_path)
    require(isinstance(effect, unreal.ParticleSystem), 'Effect is not an existing Cascade ParticleSystem')
    require(hasattr(unreal, 'AZ_Inv_CommonUI_WeaponStateFragment'), 'Native weapon fragment is unavailable')
    # Read and set both fields on a disposable struct BEFORE any asset mutation.
    # This verifies loaded reflection and the actual native object property type.
    probe = unreal.AZ_Inv_CommonUI_WeaponStateFragment()
    try:
        probe.get_editor_property('world_impact_effect')
        probe.get_editor_property('world_impact_scale')
        probe.set_editor_property('world_impact_effect', effect)
        probe.set_editor_property('world_impact_scale', float(scale))
    except Exception as error:
        raise RuntimeError('Build/restart with the ParticleSystem WorldImpactEffect and float '
                           'WorldImpactScale fields before assignment: ' + str(error)) from error
    exported = dict(H['fields'](probe.export_text()))
    require(all(key in exported for key in IMPACT_FIELDS), 'Impact fields are absent from reflected export')
    expected = {key: exported[key] for key in IMPACT_FIELDS}
    require(float(expected['WorldImpactScale']) > 0,
            'Impact scale is too small for manifest ExportText; choose a larger value')
    return {'effect': effect.get_path_name(), 'scale': probe.get_editor_property('world_impact_scale'),
            'fields': expected}


def patch_manifest(H, text, expected):
    separated(H, text)  # Validate the rifle identity and fragment count first.
    values = H['fields'](text)
    fragments = H['split_top_level'](H['parenthesized'](dict(values)['Fragments']))
    output = []
    for fragment in fragments:
        kind, fields = H['fragment_parts'](fragment)
        if kind == H['WEAPON_FRAGMENT']:
            for key in IMPACT_FIELDS:
                fields = H['replace_field'](fields, key, expected[key])
            fragment = kind + H['encode_fields'](fields)
        output.append(fragment)
    return H['encode_fields'](H['replace_field'](values, 'Fragments', '(' + ','.join(output) + ')'))


def backup(H, C):
    require(not set(OWNED_PACKAGES).intersection(C['dirty_packages']),
            'Rifle/map has unsaved edits; preserve those before creating the disk backup')
    records = [capture(c) for c in C['components']]
    for row in records:
        separated(H, row['manifest'])
    sources = [package_file(path) for path in OWNED_PACKAGES]
    require(all(source.is_file() for source in sources), 'A rifle/map package is missing on disk')
    destination = ROOT / 'Saved/Backups/BulletImpact' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    destination.mkdir(parents=True)
    files = []
    for path, source in zip(OWNED_PACKAGES, sources):
        saved = destination / source.relative_to(ROOT / 'Content')
        saved.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, saved)
        require(sha256(saved) == sha256(source), 'Backup copy verification failed')
        files.append({'package': path, 'source': str(source), 'backup': str(saved), 'sha256': sha256(source)})
    report = {'schema': SCHEMA, 'status': 'backed_up', 'backup': str(destination),
              'files': files, 'components': records, 'unrelated_dirty_packages': C['dirty_packages']}
    (destination / 'receipt.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    write_receipt('backup-receipt.json', report)
    return report


def baseline(C):
    require(RECEIPT.is_file(), 'Run main("backup") before assigning impact defaults')
    saved = json.loads(RECEIPT.read_text(encoding='utf-8'))
    require(saved.get('schema') == SCHEMA, 'Unexpected bullet-impact backup receipt')
    require({row['package'] for row in saved['files']} == set(OWNED_PACKAGES), 'Backup target set differs')
    originals = {row['component']: row for row in saved['components']}
    require(len(originals) == 3 and set(originals) == {c.get_path_name() for c in C['components']},
            'Rifle component identity changed since backup')
    return saved, originals


def assert_records(H, originals, records, expected=None):
    for row in records:
        original = originals[row['component']]
        assert_preserved(H, original['manifest'], row['manifest'], expected)
        require(original['contained'] == row['contained'], 'Contained magazine manifests changed')


def main(mode='audit', effect_path=None, scale=1.0):
    report = None
    try:
        require(mode in ('audit', 'backup', 'assign', 'verify'), 'Unknown mode: ' + str(mode))
        H = helpers()
        C = context(H)
        if mode == 'backup':
            return backup(H, C)
        records = [capture(c) for c in C['components']]
        if mode == 'audit':
            for row in records:
                separated(H, row['manifest'])
            report = {'status': 'audit', 'components': records, 'dirty_packages': C['dirty_packages'],
                      'explicit_target_packages': list(OWNED_PACKAGES), 'no_pie': True}
            write_receipt('audit-readback.json', report)
            return report
        requested = settings(H, effect_path, scale)
        saved, originals = baseline(C)
        assert_records(H, originals, records)
        expected = requested['fields']
        already_assigned = all(separated(H, row['manifest'])[2] == expected for row in records)
        report = {'schema': SCHEMA, 'mode': mode, 'settings': requested, 'backup': saved['backup'],
                  'components_before': records,
                  'compile_after_return': [RIFLE] if mode == 'assign' else [],
                  'explicit_save_packages': list(OWNED_PACKAGES) if mode == 'assign' else [],
                  'no_pie': True, 'unrelated_assets': 'Do not save packages outside the explicit list'}
        if mode == 'assign' and not already_assigned:
            require(not set(OWNED_PACKAGES).intersection(C['dirty_packages']),
                    'Rifle/map contains unsaved edits; inspect before assignment')
            for entry in saved['files']:
                source = package_file(entry['package'])
                require(source.is_file() and sha256(source) == entry['sha256'],
                        'Rifle/map changed on disk since backup; capture a fresh baseline')
            # Preflight every imported copy before modifying any live component.
            candidates = []
            for row in records:
                candidate = unreal.AZ_Inv_CommonUI_ItemManifest()
                require(candidate.import_text(patch_manifest(H, row['manifest'], expected)),
                        'Impact manifest ImportText failed')
                assert_preserved(H, row['manifest'], candidate.export_text(), expected)
                candidates.append(candidate)
            C['blueprint'].modify()
            C['world'].modify()
            for actor in C['actors']:
                actor.modify()
            for component, candidate in zip(C['components'], candidates):
                component.modify()
                # Default editor notifications reconstruct placed BP components,
                # invalidating cached wrappers. This data-only change needs no
                # construction script; Modify marks the explicit packages dirty.
                component.set_editor_property('pickup_item_manifest', candidate,
                    notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
            report['status'] = 'authored_requires_compile_and_save'
        elif mode == 'assign':
            # No Modify calls or writes on a repeat; unrelated live data still
            # had to match the captured baseline, including every contained item.
            report['status'] = 'already_assigned'
            if not set(OWNED_PACKAGES).intersection(C['dirty_packages']):
                report['compile_after_return'] = []
                report['explicit_save_packages'] = []
        else:
            require(already_assigned, 'Impact defaults do not match the requested effect/scale')
            require(not set(OWNED_PACKAGES).intersection(dirty_packages()),
                    'Save the explicit rifle/map packages through dedicated tools before final verification')
            report['status'] = 'readback_passed'
        C = context(H)
        after = [capture(c) for c in C['components']]
        assert_records(H, originals, after, expected)
        report.update(components=after, dirty_packages=dirty_packages())
        write_receipt(mode + '-readback.json', report)
        return report
    except Exception as error:
        if report is not None:
            report.update(status='failed', error=str(error))
            # Preserve partial live readback for recovery if a setter or the
            # post-write preservation check failed. Never save a failed result.
            try:
                C = context(H)
                report['components_after_failure'] = [capture(c) for c in C['components']]
            except Exception as capture_error:
                report['failure_readback_error'] = str(capture_error)
            write_receipt(mode + '-failed.json', report)
        raise
    finally:
        gc.collect()


if __name__ == '__main__':
    print(json.dumps(main(), indent=2))
