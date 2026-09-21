# @Description: Correct inherited inventory tab dimensions and duplicate column padding.
"""Root-run property-only correction after the first Field Notes preview.
No graph, input, item, capacity, widget identity or source-pack changes.
Native compile/save remain external.
"""
import importlib.util
import json
import shutil
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
MAIN = '/Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBP_GameInventoryMenu'
SWITCHER = '/Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBP_GameInventorySwitcher'


def support():
    spec = importlib.util.spec_from_file_location('fn_center_finish', ROOT/'Tools/field_notes_inventory_finish.py')
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    return module


def apply():
    f = support(); i = f.support(); s = i.support(); s.idle()
    out = s.OUT / 'CenterLayoutFix'
    before = {p: f._snapshot(p) for p in (MAIN, SWITCHER)}
    stamp = out / s.stamp()
    s.write(stamp/'before.json', before)
    for p in before:
        shutil.copy2(s.package_file(p), stamp/s.package_file(p).name)
    ops = []
    def put(p, name, values, slot=False):
        item = before[p]['templates'][name]['slot' if slot else 'widget']
        s._validate_patch(item['schema'], values)
        ops.append({'asset': p, 'object': item['object'], 'patch': values})
    # MoveWidget copied former HorizontalBoxSlot padding into Border.Padding.
    # Keep the external49px column gaps exactly once, then author content insets.
    put(MAIN, 'FN_VitalsPaperPanel', {'padding': i._margin()})
    put(MAIN, 'FN_BackpackPaperPanel', {'padding': i._margin(24, 24, 24, 24)})
    put(MAIN, 'FN_SkillsPaperPanel', {'padding': i._margin()})
    put(SWITCHER, 'CurrencyBarBgImage', {'brush': {'imageSize': {'x': 1, 'y': 40}}})
    put(SWITCHER, 'CurrencyNameText', {'padding': i._margin()}, True)
    # The old6x853 decorative strips determined the automatic row height.
    for name in ('TabSeparatorImage', 'TabSeparatorImage_0', 'TabSeparatorImage_1', 'TabSeparatorImage_2', 'TabSeparatorImage_3'):
        put(SWITCHER, name, {'brush': {'imageSize': {'x': 1, 'y': 22}, 'resourceObject': 'None',
                                      'resourceName': 'None', 'drawAs': 'Image',
                                      'tintColor': s.slate({'r': 1, 'g': 1, 'b': 1, 'a': 1})},
                             'colorAndOpacity': s.linear('Paper', 'edge')})
    for name in ('TabSeparatorVBox_0', 'TabSeparatorVBox_3'):
        put(SWITCHER, name, {'visibility': 'Collapsed'})
        put(SWITCHER, name, {'size': {'value': 0, 'sizeRule': 'Automatic'}}, True)
    # Designer visibility is independent of runtime Collapsed. Empty decorative
    # end-caps must also have zero desired size and no brush in its preview.
    for name in ('TabSeparatorImage_0', 'TabSeparatorImage_3'):
        put(SWITCHER, name, {'brush': {'drawAs': 'NoDrawType', 'imageSize': {'x': 0, 'y': 0}}})
    for name in ('Button_Equippable', 'Button_Consumable', 'Button_Craftable', 'WBPMapButton_1'):
        put(SWITCHER, name, {'minHeight': 42, 'maxHeight': 42, 'minWidth': 156})
        put(SWITCHER, name, {'size': {'value': 1, 'sizeRule': 'Automatic'}, 'verticalAlignment': 'VAlign_Center'}, True)
    for name in ('InventoryTabVBox', 'CraftingTabVBox', 'MapTabVBox', 'MapTabVBox_1'):
        put(SWITCHER, name, {'verticalAlignment': 'VAlign_Center'}, True)
    put(SWITCHER, 'HorizontalBoxMenuTabs', {'padding': i._margin(0, 8, 0, 18), 'verticalAlignment': 'VAlign_Top'}, True)
    for op in ops:
        obj = s.resolve(op['object']); obj.modify()
        i.require(s.ue().ToolsetLibrary.set_object_properties(obj, json.dumps(op['patch'])), 'Center layout write failed')
        i.require(s._contains(s.read_object(obj)['values'], op['patch']), 'Center layout readback failed')
    after = {p: f._snapshot(p) for p in before}
    for p in before:
        i.require(after[p]['tree'] == before[p]['tree'], 'Center correction changed the widget tree')
    return s.write(out/'applied.json', {'backup': str(stamp), 'operations': ops,
                                       'assets': list(before), 'after': after, 'runtime_verified': False})


def verify():
    f = support(); i = f.support(); s = i.support()
    out = s.OUT/'CenterLayoutFix'
    receipt = json.loads((out/'applied.json').read_text(encoding='utf-8'))
    expected = {}
    for op in receipt['operations']:
        expected[op['object']] = i.merge(expected.get(op['object'], {}), op['patch'])
    for path, patch in expected.items():
        i.require(s._contains(s.read_object(s.resolve(path))['values'], patch), 'Center correction changed after compile')
    for name in ('Grid_Equippables', 'Grid_Consumables', 'Grid_Craftables'):
        v = f._snapshot(SWITCHER)['templates'][name]['widget']['values']
        i.require(v['gridSize'] == {'x': 11, 'y': 7} and v['tileSize'] == 50, 'Inventory capacity changed')
    return s.write(out/'verified.json', {'assets': receipt['assets'], 'properties_verified': True,
                                        'capacity_unchanged': True, 'runtime_verified': False})
