# @Description: Back up, author, or verify the rifle HUD reticle definition and manifest assignment.
"""Ordinary Unreal Python; default main() is read-only.

Sequence: main('backup'), native build/restart, author/compile the leaf+HUD via
hud_reticle_assets.py and dedicated BlueprintTools, then main('assign'). The
caller compiles the rifle BP and saves only the returned explicit packages.
Run main('verify') after those calls return. This script never saves packages,
compiles Blueprints, starts PIE, or modifies inventory/animation/input assets.
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
OUTPUT = ROOT / 'Saved/HUDReticle'
HUD = '/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD'
FOLDER = '/Game/AZ/Blueprints/Menu/HUD/Reticles'
LEAF = FOLDER + '/WBP_AZ_Reticle_Rifle'
DEFINITION = FOLDER + '/DA_HUDReticle_Rifle'
RIFLE = '/Game/AZ/Blueprints/Items/Equippables/Weapons/BPAZ_CommonUI_PickupItem'
MAP = '/Game/AZ/Maps/L_001'
FIELD = 'ReticleDefinition'
OWNER_KEY = 'AZ.HUDReticle.Owner'
OWNER = 'hud_reticle_assign:v1'
RECEIPT = OUTPUT / 'backup-receipt.json'


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def helpers():
    # Only the guarded pure-helper module is loaded. The earlier inventory
    # foundation auto-runs main() and must never be imported here.
    previous = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        return runpy.run_path(str(ROOT / 'Tools/rifle_p01_activate.py'), run_name='hud_reticle_helpers')
    finally:
        sys.dont_write_bytecode = previous


def package_file(package):
    require(package.startswith('/Game/'), 'Unexpected package root: ' + package)
    content = (ROOT / 'Content').resolve()
    target = (content / (package[len('/Game/'):] + ('.umap' if package == MAP else '.uasset'))).resolve()
    require(target.is_relative_to(content), 'Package resolves outside project content')
    return target


def package(asset):
    return asset.get_path_name().split('.')[0]


def capture(component):
    return {'component': component.get_path_name(),
            'manifest': component.get_editor_property('pickup_item_manifest').export_text(),
            'contained': [m.export_text() for m in component.get_editor_property('initial_contained_item_manifests')]}


def context(H):
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(not editor.get_game_world(), 'Stop PIE before reticle authoring')
    world = editor.get_editor_world()
    require(package(world) == MAP, 'Open the audited L_001 map before authoring its rifle overrides')
    bp = unreal.load_asset(RIFLE)
    require(isinstance(bp, unreal.Blueprint), 'Rifle Blueprint missing')
    components = [H['item_component_template'](bp)]
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    for actor in actors:
        if actor.get_class() == bp.generated_class():
            components.append(actor.get_component_by_class(unreal.AZ_Inv_CommonUI_ItemComponent))
    require(len(components) == 3 and all(components), 'Rifle template/placed instance set changed; inspect before authoring')
    dirty = [p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
             + unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    return {'world': world, 'blueprint': bp, 'components': components, 'dirty_packages': dirty}


def patch_manifest(H, text):
    fields = H['fields'](text)
    fragments = H['split_top_level'](H['parenthesized'](dict(fields)['Fragments']))
    reference = H['object_reference']('/Script/AZ.AZ_HUDReticleDefinition', DEFINITION)
    result, count = [], 0
    for fragment in fragments:
        kind, values = H['fragment_parts'](fragment)
        if kind == H['WEAPON_FRAGMENT']:
            count += 1
            current = dict(values)
            require(current.get('bUsesDetachableMagazines') == 'True'
                    and 'Weapon.Rifle' in current.get('WeaponTag', ''), 'Unexpected rifle manifest')
            fragment = kind + H['encode_fields'](H['replace_field'](values, FIELD, reference))
        result.append(fragment)
    require(count == 1, 'Expected exactly one existing weapon-state fragment')
    return H['encode_fields'](H['replace_field'](fields, 'Fragments', '(' + ','.join(result) + ')'))


def assert_preserved(H, before, after, require_assignment=True):
    def retained(text):
        values = dict(H['fields'](text))
        fragments = H['split_top_level'](H['parenthesized'](values.pop('Fragments')))
        kept, assignment, count = [], None, 0
        for fragment in fragments:
            kind, fields = H['fragment_parts'](fragment)
            if kind == H['WEAPON_FRAGMENT']:
                count += 1
                assignment = dict(fields).get(FIELD)
                fragment = kind + H['encode_fields']([(k, v) for k, v in fields if k != FIELD])
            kept.append(fragment)
        require(count == 1, 'Expected one weapon-state fragment during readback')
        return values, kept, assignment
    old_top, old_fragments, _ = retained(before)
    new_top, new_fragments, assignment = retained(after)
    require(old_top == new_top, 'Unrelated top-level manifest fields changed')
    require(old_fragments == new_fragments, 'Unrelated fragment, firing, animation or ability fields changed')
    if require_assignment:
        require(assignment == H['object_reference']('/Script/AZ.AZ_HUDReticleDefinition', DEFINITION),
                'ReticleDefinition reference did not resolve')


def backup(H, C):
    owned = [HUD, RIFLE, MAP, LEAF, DEFINITION]
    require(not set(owned).intersection(C['dirty_packages']),
            'A reticle target package has unsaved changes; preserve/review before backup')
    records = [capture(c) for c in C['components']]
    for record in records:
        assert_preserved(H, record['manifest'], patch_manifest(H, record['manifest']))
    destination = ROOT / 'Saved/Backups/HUDReticle' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    destination.mkdir(parents=True)
    files = []
    for asset in owned:
        source = package_file(asset)
        entry = {'package': asset, 'existed': source.exists()}
        if source.exists():
            saved = destination / source.relative_to(ROOT / 'Content')
            saved.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, saved)
            entry.update(source=str(source), backup=str(saved), sha256=hashlib.sha256(source.read_bytes()).hexdigest())
        files.append(entry)
    report = {'status': 'backed_up', 'backup': str(destination), 'files': files,
              'components': records, 'unrelated_dirty_packages': C['dirty_packages']}
    OUTPUT.mkdir(parents=True, exist_ok=True)
    (destination / 'receipt.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    RECEIPT.write_text(json.dumps(report, indent=2), encoding='utf-8')
    return report


def baseline(C, check_disk=False):
    require(RECEIPT.exists(), 'Run main("backup") before authoring')
    report = json.loads(RECEIPT.read_text(encoding='utf-8'))
    original = {row['component']: row for row in report['components']}
    require(set(original) == {c.get_path_name() for c in C['components']}, 'Placed rifle component set changed since backup')
    if check_disk:
        require(not {RIFLE, MAP, DEFINITION}.intersection(C['dirty_packages']),
                'Rifle/map/definition has unsaved changes; inspect before assignment')
        for entry in report['files']:
            if entry['package'] in (RIFLE, MAP):
                source = package_file(entry['package'])
                require(entry['existed'] and source.exists()
                        and hashlib.sha256(source.read_bytes()).hexdigest() == entry['sha256'],
                        'Rifle/map package changed since backup; refresh the audit before assignment')
    return report, original


def author_definition():
    require(hasattr(unreal, 'AZ_HUDReticleDefinition') and hasattr(unreal, 'AZ_HUDReticleWidget'),
            'Build the native reticle classes and restart the editor before assignment')
    leaf = unreal.load_asset(LEAF)
    require(isinstance(leaf, unreal.WidgetBlueprint), 'Author and compile the rifle reticle widget first')
    leaf_class = leaf.generated_class()
    require(leaf_class and isinstance(unreal.get_default_object(leaf_class), unreal.AZ_HUDReticleWidget),
            'Reticle leaf generated class is missing or has the wrong native parent')
    asset = unreal.load_asset(DEFINITION) if unreal.EditorAssetLibrary.does_asset_exist(DEFINITION) else None
    if asset is None:
        folder, name = DEFINITION.rsplit('/', 1)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.AZ_HUDReticleDefinition, None)
        require(asset, 'Could not create native reticle definition')
        unreal.EditorAssetLibrary.set_metadata_tag(asset, OWNER_KEY, OWNER)
    require(isinstance(asset, unreal.AZ_HUDReticleDefinition)
            and unreal.EditorAssetLibrary.get_metadata_tag(asset, OWNER_KEY) == OWNER,
            'An unowned/wrong-type reticle definition exists; inspect instead of overwriting')
    asset.modify()
    def channel(value):
        value /= 255.0
        return value / 12.92 if value <= .04045 else ((value + .055) / 1.055) ** 2.4
    asset.set_editor_property('widget_class', leaf_class)
    asset.set_editor_property('size', unreal.Vector2D(40, 40))
    asset.set_editor_property('tint', unreal.LinearColor(channel(238), channel(234), channel(224), 1))
    asset.set_editor_property('aim_only', True)
    return asset


def verify_definition():
    asset = unreal.load_asset(DEFINITION)
    require(asset and isinstance(asset, unreal.AZ_HUDReticleDefinition), 'Reticle definition is missing')
    leaf_class = unreal.load_asset(LEAF).generated_class()
    size = asset.get_editor_property('size')
    require(asset.get_editor_property('widget_class') == leaf_class and size.x == 40 and size.y == 40
            and asset.get_editor_property('aim_only'), 'Reticle definition defaults differ')
    tint = asset.get_editor_property('tint')
    expected_rgb = [((value / 255.0 + .055) / 1.055) ** 2.4 for value in (238, 234, 224)]
    require(all(abs(actual - expected) < .00001 for actual, expected in
                zip((tint.r, tint.g, tint.b, tint.a), expected_rgb + [1])), 'Reticle definition tint differs')
    return {'asset': asset.get_path_name(), 'widget_class': leaf_class.get_path_name(),
            'size': [size.x, size.y], 'tint': str(asset.get_editor_property('tint')), 'aim_only': True}


def main(mode='audit'):
    try:
        H = helpers()
        C = context(H)
        if mode == 'backup':
            return backup(H, C)
        if mode == 'audit':
            return {'status': 'audit', 'components': [capture(c) for c in C['components']],
                    'dirty_packages': C['dirty_packages'], 'definition': DEFINITION,
                    'native_available': hasattr(unreal, 'AZ_HUDReticleDefinition')}
        require(mode in ('assign', 'verify'), 'Unknown mode: ' + mode)
        saved, originals = baseline(C, check_disk=mode == 'assign')
        if mode == 'assign':
            # Preflight every current manifest before touching definition or pickup.
            records = [capture(c) for c in C['components']]
            candidates = []
            for record in records:
                original = originals[record['component']]
                # A full native build adds the new default ReticleDefinition=None
                # to ExportText even when the backup predates this property.
                assert_preserved(H, original['manifest'], record['manifest'], require_assignment=False)
                require(record['contained'] == original['contained'], 'Live contained magazines differ from the backup')
                # Import into a detached copy. Importing into the borrowed live
                # property can bypass the setter's change tracking.
                manifest = unreal.AZ_Inv_CommonUI_ItemManifest()
                require(manifest.import_text(patch_manifest(H, record['manifest'])), 'Reticle manifest import failed')
                assert_preserved(H, record['manifest'], manifest.export_text())
                candidates.append(manifest)
            author_definition()
            C['blueprint'].modify()
            C['world'].modify()
            for component, manifest in zip(C['components'], candidates):
                component.modify()
                component.set_editor_property('pickup_item_manifest', manifest,
                    notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        C = context(H)
        after = [capture(c) for c in C['components']]
        for record in after:
            original = originals[record['component']]
            assert_preserved(H, original['manifest'], record['manifest'])
            require(record['contained'] == original['contained'], 'Contained magazine defaults changed')
        report = {'status': 'authored_requires_compile_and_save' if mode == 'assign' else 'readback_passed',
                  'definition': verify_definition(), 'components': after, 'backup': saved['backup'],
                  'compile_after_return': [RIFLE], 'explicit_save_packages': [LEAF, HUD, DEFINITION, RIFLE, MAP],
                  'unrelated_assets': 'Do not save dirty packages outside the explicit list', 'no_pie': True}
        OUTPUT.mkdir(parents=True, exist_ok=True)
        (OUTPUT / (mode + '-readback.json')).write_text(json.dumps(report, indent=2), encoding='utf-8')
        return report
    finally:
        gc.collect()


if __name__ == '__main__':
    print(json.dumps(main(), indent=2))
