# @Description: Apply the reviewed tab-selection policy to four owned inventory widget templates only.
"""Import-inert. apply() authors; native compilation/save are separate root operations."""
import gc
import importlib.util
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'Saved/FieldNotesImplementation/AcceptanceFixes/TabSelectionAssetRun'
PLAN = ROOT / 'Saved/FieldNotesImplementation/AcceptanceFixes/TabSelectionProposal/asset-patch.json'
PACKAGE = '/Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBP_GameInventorySwitcher'


def support():
    spec = importlib.util.spec_from_file_location('fn_tab_acceptance_support', ROOT / 'Tools/field_notes_styles_setup.py')
    value = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(value)
    return value


def apply():
    S = support()
    S.idle()
    S.require(not (OUT / 'authored.json').exists(), 'Already authored; inspect/verify rather than replay.')
    plan = json.loads(PLAN.read_text(encoding='utf-8'))
    S.require(plan['asset'] == PACKAGE and len(plan['operations']) == 4, 'Unexpected tab scope')
    expected_names = {'Button_Equippable', 'Button_Consumable', 'Button_Craftable', 'WBPMapButton_1'}
    S.require({row['widget'] for row in plan['operations']} == expected_names, 'Unexpected tab identities')
    bp, _ = S.defaults(PACKAGE)
    tree_before = S.native_tool('UMGToolSet.UMGToolSet.GetWidgets', widgetBlueprint=S.ref(PACKAGE))
    before = {}
    for row in plan['operations']:
        S.require(row['object'] == bp.get_path_name() + ':WidgetTree.' + row['widget'], 'Unexpected template path')
        current = S.read_object(S.resolve(row['object']))
        S.require(all(current['values'].get(k) == v for k, v in row['expected_before'].items()),
                  'Tab policy changed since review: ' + row['widget'])
        before[row['object']] = current
    OUT.mkdir(parents=True, exist_ok=True)
    backup = OUT / ('Before-' + datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
    backup.mkdir()
    shutil.copy2(S.package_file(PACKAGE), backup / 'AZ_WBP_GameInventorySwitcher.uasset.bak')
    (backup / 'live.json').write_text(json.dumps({'tree': tree_before, 'objects': before}, indent=2), encoding='utf-8')
    for row in plan['operations']:
        obj = S.resolve(row['object'])
        S.ue().ToolsetLibrary.set_object_properties(obj, json.dumps(row['patch']))
        after = S.read_object(obj)['values']
        expected = dict(before[row['object']]['values'])
        expected.update(row['patch'])
        S.require(after == expected, 'Unexpected property drift on ' + row['widget'])
    tree_after = S.native_tool('UMGToolSet.UMGToolSet.GetWidgets', widgetBlueprint=S.ref(PACKAGE))
    S.require(tree_after == tree_before, 'Tab policy changed hierarchy')
    result = {'asset': PACKAGE, 'backup': str(backup), 'authored': True, 'compiled': False, 'saved': False}
    (OUT / 'authored.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    gc.collect()
    return result


def verify():
    S = support()
    plan = json.loads(PLAN.read_text(encoding='utf-8'))
    result = {'asset': PACKAGE, 'verified': [], 'runtime_verified': False}
    for row in plan['operations']:
        current = S.read_object(S.resolve(row['object']))['values']
        S.require(all(current.get(k) == v for k, v in row['patch'].items()), 'Unexpected policy: ' + row['widget'])
        result['verified'].append({'widget': row['widget'], 'policy': {k: current[k] for k in row['patch']}})
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'verified.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    gc.collect()
    return result
