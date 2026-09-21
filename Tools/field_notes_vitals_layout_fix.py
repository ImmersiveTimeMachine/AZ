# @Description: Remove the three empty legacy VITALS rows from Field Notes layout allocation.
"""Import-inert, VITALS-only correction; no compile, save, PIE or gameplay calls.

Offline: python Tools/field_notes_vitals_layout_fix.py --plan
Editor writer: capture(); author(); native compile VITALS; explicitly save VITALS;
verify(). Recreate the inventory preview externally to inspect the rendered result.

Every retained widget, native binding, health value, portrait resource, graph and
unsupported-vitals opt-in is preserved. Only the three empty legacy root rows'
visibility and their empty subtree slot sizing/padding are patched. Designer
visibility ignores runtime Collapsed, so empty subtree slots also need zero
padding and Automatic sizing to request zero height in the thumbnail preview.
Exact native schemas, current
properties and the saved package are backed up before the first UObject write.
"""
from __future__ import annotations

import copy
import gc
import hashlib
import importlib.util
import json
import shutil
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
ASSET = '/Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBPCharacterVitalsPanel'
OUT = ROOT / 'Saved/FieldNotesImplementation/VitalsLayoutFix'
AUDIT = ROOT / 'Saved/FieldNotesImplementation/Audit/inventory-layout-after.json'
NATIVE_SOURCE = ROOT / 'Source/AZ/Private/InventoryUI/AZ_Inv_CommonUI_CharacterVitalsPanel.cpp'
VERSION = 'field_notes_vitals_layout_fix:v1'
SCAFFOLDS = ('HorizontalBox_592', 'HorizontalBox_713', 'HorizontalBox_6')
CONTAINERS = {'/Script/UMG.HorizontalBox', '/Script/UMG.VerticalBox', '/Script/UMG.Overlay'}


def support():
    spec = importlib.util.spec_from_file_location('fn_vitals_fix_support', ROOT / 'Tools/field_notes_inventory_setup.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, allow_nan=False).encode()).hexdigest()


def _snapshot(s):
    _, cdo = s.defaults(ASSET)
    tree = s.native_tool('UMGToolSet.UMGToolSet.GetWidgets', widgetBlueprint=s.ref(ASSET))
    templates = {}
    for row in tree['widgets']:
        templates[row['widgetName']] = {'row': row}
        for part in ('widget', 'slot'):
            if isinstance(row.get(part), dict):
                templates[row['widgetName']][part] = s.read_object(s.resolve(row[part]))
    return {'asset': ASSET, 'tree': tree, 'templates': templates,
            'cdo': s.read_object(cdo), 'saved_package_sha256': s.digest(s.package_file(ASSET))}


def _inspect(snapshot):
    """Read-only structure/geometry guard, usable on an offline or live snapshot."""
    require(snapshot['asset'] == ASSET, 'Unexpected asset')
    t = snapshot['templates']
    root = t['VerticalBox']['row']['widget']
    children = [name for name, entry in t.items() if entry['row'].get('parent') == root]
    require(set(children) == set(SCAFFOLDS) | {'FN_VitalsCanvas'}, 'Unexpected root children')
    obsolete = []
    for name in SCAFFOLDS:
        pending = [name]
        while pending:
            child = pending.pop()
            row = t[child]['row']
            require(row['widgetClassPath']['refPath'] in CONTAINERS,
                    'Legacy row contains real content; refusing to hide: ' + child)
            obsolete.append(child)
            pending.extend(n for n, e in t.items() if e['row'].get('parent') == row['widget'])
    require(len(obsolete) == len(set(obsolete)), 'Legacy tree contains duplicate descendants')
    for name in ('HeroPortraitImage', 'HealthProgressBar', 'HealthIconImage', 'HealthValueText',
                 'InfectionContainer', 'MortalityContainer', 'HeartbeatImage', 'HeartbeatShadowImage'):
        require(name in t and name not in obsolete, 'A retained native binding is inside legacy scaffolding: ' + name)
    expected_slots = {
        'FN_ConditionHeading': (24, 24, 360, 32),
        'FN_PortraitAspectFit': (39, 56, 316, 444),
        'FN_HealthLabel': (28, 521, 170, 30),
        'HealthValueText': (220, 521, 158, 30),
        'HealthProgressBar': (28, 557, 350, 7),
        'InfectionContainer': (0, 601, 405, 54),
        'MortalityContainer': (0, 676, 405, 54),
    }
    for name, expected in expected_slots.items():
        values = t[name]['slot']['values']
        layout = values['layoutData']
        require(tuple(layout['offsets'][k] for k in ('left', 'top', 'right', 'bottom')) == expected,
                'Reviewed content rectangle changed: ' + name)
        require(not values['bAutoSize'] and layout['anchors'] == {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 0, 'y': 0}},
                'Reviewed content anchoring changed: ' + name)
    require(t['FN_PortraitAspectFit']['widget']['values']['stretch'] == 'ScaleToFit', 'Portrait aspect-fit changed')
    require(t['HeroPortraitImage']['row']['parent'] == t['FN_PortraitAspectFit']['row']['widget'], 'Portrait parent changed')
    require(t['FN_VitalsCanvas']['slot']['values']['size'] == {'value': 1, 'sizeRule': 'Fill'}, 'Active canvas no longer fills its parent')
    brush = t['HeroPortraitImage']['widget']['values']['brush']
    image_size = brush['imageSize']
    scale = min(316 / image_size['x'], 444 / image_size['y'])
    fitted = {'width': image_size['x'] * scale, 'height': image_size['y'] * scale}
    fitted.update({'left': 95 + (316 - fitted['width']) / 2, 'top': 206 + (444 - fitted['height']) / 2})
    return {'empty_scaffolds': obsolete, 'root_child_order': children,
            'panel_at_1920x1080': [56, 150, 405, 810],
            'heading_slot_at_1920x1080': [80, 174, 360, 32],
            'portrait_slot_at_1920x1080': [95, 206, 316, 444],
            'portrait_fitted_image_at_1920x1080': fitted,
            'portrait_resource': brush['resourceObject'],
            'health_bar_at_1920x1080': [84, 707, 350, 7],
            'native_health': 'NativePreConstruct collapses unavailable health; HandleVitalsChanged remains authoritative.',
            'unsupported_vitals_opt_in': snapshot['cdo']['values']['bShowUnsupportedVitals'],
            'preview_note': 'Designer ignores runtime Collapsed; Automatic empty slots with zero padding request zero space. No sample health is supplied.'}


def _operations(snapshot):
    s = support().support()
    result = []
    for name in SCAFFOLDS:
        obj = snapshot['templates'][name]['widget']
        patch = {'visibility': 'Collapsed'}
        s._validate_patch(obj['schema'], patch)
        require(obj['object'].split('.')[0] == ASSET, 'Patch target is not VITALS-owned')
        result.append({'widget': name, 'part': 'widget', 'object': obj['object'], 'patch': patch})
    # All twelve old widgets are verified empty layout containers by _inspect.
    # Two nested HorizontalBox slots retain bottom50/side80 padding from the old
    # circular meters; root Automatic sizing alone would still reserve50px in
    # Designer, where Collapsed is deliberately ignored by UWidget.
    for name in _inspect(snapshot)['empty_scaffolds']:
        obj = snapshot['templates'][name]['slot']
        patch = {'size': {'value': 0, 'sizeRule': 'Automatic'},
                 'padding': {'left': 0, 'top': 0, 'right': 0, 'bottom': 0}}
        s._validate_patch(obj['schema'], patch)
        require(obj['object'].split('.')[0] == ASSET, 'Patch target is not VITALS-owned')
        result.append({'widget': name, 'part': 'slot', 'object': obj['object'], 'patch': patch})
    return result


def plan():
    """File-only plan; safe outside Unreal and does not write files."""
    snapshot = json.loads(AUDIT.read_text(encoding='utf-8-sig'))[ASSET]
    return {'version': VERSION, 'asset': ASSET, 'geometry': _inspect(snapshot),
            'operations': _operations(snapshot), 'editor_calls': False, 'runtime_verified': False}


def _objects(snapshot):
    result = {snapshot['cdo']['object']: snapshot['cdo']}
    for entry in snapshot['templates'].values():
        for part in ('widget', 'slot'):
            if isinstance(entry.get(part), dict):
                result[entry[part]['object']] = entry[part]
    return result


def _check(current, baseline, operations):
    i = support()
    require(i._tree_identity(current['tree']) == i._tree_identity(baseline['tree']), 'Widget tree/binding identity changed')
    expected = _objects(baseline)
    actual = _objects(current)
    require(set(actual) == set(expected), 'Template/CDO object set changed')
    patches = {op['object']: op['patch'] for op in operations}
    for path, original in expected.items():
        require(i._exact_value(actual[path]['values'], i.merge(original['values'], patches.get(path, {}))),
                'Unexpected object/property difference: ' + path)


def _state():
    path = OUT / 'state.json'
    require(path.is_file(), 'Run capture() before author()')
    state = json.loads(path.read_text(encoding='utf-8'))
    require(state['version'] == VERSION, 'Wrong recipe state version')
    require(state['recipe_sha256'] == support().support().digest(Path(__file__)), 'Recipe changed since capture')
    return state


def capture():
    """Take a fresh saved-package backup and exact live property/schema snapshot."""
    s = support().support()
    try:
        s.idle()
        require(not (OUT / 'state.json').exists(), 'Capture already exists; continue its author/verify stage')
        dirty = {package.get_name() for package in s.ue().EditorLoadingAndSavingUtils.get_dirty_content_packages()}
        require(ASSET not in dirty, 'Explicitly save the known VITALS changes before this fresh backup')
        before = _snapshot(s)
        geometry = _inspect(before)
        operations = _operations(before)
        backup = OUT / ('BeforeLayoutFix_' + s.stamp())
        backup.mkdir(parents=True)
        shutil.copy2(s.package_file(ASSET), backup / s.package_file(ASSET).name)
        s.write(backup / 'live-before.json', before)
        state = {'version': VERSION, 'asset': ASSET, 'state': 'captured', 'backup': str(backup),
                 'operations': operations, 'operation_sha256': digest(operations),
                 'recipe_sha256': s.digest(Path(__file__)), 'native_source_sha256': s.digest(NATIVE_SOURCE),
                 'saved_package_sha256': before['saved_package_sha256'], 'geometry': geometry}
        s.write(OUT / 'state.json', state)
        return {'state': 'captured', 'backup': str(backup), 'operations': len(operations), 'next': 'author()'}
    finally:
        gc.collect()


def author():
    """Apply fifteen exact patches (two already satisfied) to one owned asset."""
    i = support(); s = i.support()
    try:
        s.idle()
        state = _state()
        require(state['state'] in ('captured', 'authored'), 'Interrupted write; inspect backup and state before recovery')
        before = json.loads((Path(state['backup']) / 'live-before.json').read_text(encoding='utf-8'))
        operations = state['operations']
        require(digest(operations) == state['operation_sha256'], 'Captured operations changed')
        current = _snapshot(s)
        if state['state'] == 'authored':
            _check(current, before, operations)
            return {'state': 'already authored', 'native_compile_required': [ASSET], 'explicit_save_required': [ASSET]}
        require(current['saved_package_sha256'] == state['saved_package_sha256'], 'Saved package changed after capture')
        _check(current, before, [])
        _inspect(current)
        s.defaults(ASSET)[0].modify()
        state['state'] = 'writing'; state['completed'] = []
        s.write(OUT / 'state.json', state)
        for operation in operations:
            obj = s.resolve(operation['object'])
            require(obj.get_path_name().split('.')[0] == ASSET, 'Only VITALS is writable')
            live = s.read_object(obj)
            s._validate_patch(live['schema'], operation['patch'])
            if not s._contains(live['values'], operation['patch']):
                obj.modify()
                require(s.ue().ToolsetLibrary.set_object_properties(obj, json.dumps(operation['patch'], allow_nan=False)),
                        'Native property write failed: ' + operation['object'])
            after = s.read_object(obj)
            require(i._exact_value(after['values'], i.merge(live['values'], operation['patch'])),
                    'Full-property readback failed: ' + operation['object'])
            state['completed'].append(operation['object'])
            s.write(OUT / 'state.json', state)
        after = _snapshot(s)
        _check(after, before, operations)
        state['state'] = 'authored'
        s.write(OUT / 'authored.json', after)
        s.write(OUT / 'state.json', state)
        return {'state': 'authored', 'asset': ASSET, 'native_compile_required': [ASSET],
                'explicit_save_required': [ASSET], 'geometry': state['geometry'], 'runtime_verified': False}
    finally:
        gc.collect()


def verify():
    """Read-only verification after the editor writer compiles/saves explicitly."""
    s = support().support()
    try:
        s.idle()
        state = _state()
        require(state['state'] == 'authored', 'Complete author() first')
        before = json.loads((Path(state['backup']) / 'live-before.json').read_text(encoding='utf-8'))
        current = _snapshot(s)
        _check(current, before, state['operations'])
        require(s.digest(NATIVE_SOURCE) == state['native_source_sha256'], 'Native VITALS source changed during layout correction')
        bp, _ = s.defaults(ASSET)
        for graph in s.ue().BlueprintEditorLibrary.list_graphs(bp):
            require(not s.ue().BlueprintGraphEditor.get_graph_editor(graph).list_nodes_with_errors(),
                    'VITALS Blueprint graph reports errors')
        dirty = {package.get_name() for package in s.ue().EditorLoadingAndSavingUtils.get_dirty_content_packages()}
        saved = ASSET not in dirty
        require(saved, 'Explicit native compile and package save remain required')
        return s.write(OUT / 'verified.json', {'asset': ASSET, 'saved': saved, 'snapshot': current,
                                              'geometry': _inspect(current), 'runtime_verified': False,
                                              'all_non_layout_properties_preserved': True})
    finally:
        gc.collect()


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plan', action='store_true', required=True)
    parser.parse_args()
    print(json.dumps(plan(), indent=2, ensure_ascii=False))
