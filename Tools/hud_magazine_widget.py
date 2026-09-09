"""ProgrammaticToolset script: add only the approved magazine icon/count row.

Default audit is read-only. Set MODE='author' after backing up WBP_AZ_GameHUD
and importing T_HUD_Magazine. Compile/save outside this call; no PIE or C++.
"""
import copy
import json

MODE = 'audit'
HUD = '/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD.WBP_AZ_GameHUD'
TEXTURE = '/Game/AZ/Blueprints/Menu/HUD/Art/T_HUD_Magazine.T_HUD_Magazine'
UMG = 'UMGToolSet.UMGToolSet.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'


def call(prefix, name, **kwargs):
    return execute_tool(prefix + name, json.dumps(kwargs))['returnValue']


def ref(path):
    return {'refPath': path}


def tree():
    return call(UMG, 'GetWidgets', widgetBlueprint=ref(HUD))


def read(obj, names):
    schema = json.loads(call(OBJ, 'list_properties', instance=obj))
    assert all(name in schema for name in names), str(names)
    return json.loads(call(OBJ, 'get_properties', instance=obj, properties=names))


def merged(before, changes):
    result = copy.deepcopy(before)
    for key, value in changes.items():
        result[key] = merged(result[key], value) if isinstance(value, dict) and isinstance(result.get(key), dict) else value
    return result


def set_props(obj, changes):
    values = merged(read(obj, list(changes)), changes)
    assert call(OBJ, 'set_properties', instance=obj, values=json.dumps(values))
    return read(obj, list(changes))


def snapshot():
    nodes = [node for node in tree()['widgets'] if isinstance(node['widget'], dict)]
    rows = {}
    for node in nodes:
        schema = json.loads(call(OBJ, 'list_properties', instance=node['widget']))
        wanted = [name for name in ['visibility', 'renderTransform', 'colorAndOpacity', 'font', 'text',
                                   'brush', 'justification', 'size', 'fillColorCurrent'] if name in schema]
        rows[node['widgetName']] = {'node': node, 'properties': read(node['widget'], wanted)}
        if isinstance(node['slot'], dict):
            schema = json.loads(call(OBJ, 'list_properties', instance=node['slot']))
            names = [name for name in ['layoutData', 'bAutoSize', 'zOrder', 'padding', 'size',
                                      'horizontalAlignment', 'verticalAlignment'] if name in schema]
            rows[node['widgetName']]['slot_properties'] = read(node['slot'], names)
    return rows


def run():
    before = snapshot()
    assert 'WeaponContainer' in before and 'SpareMagazinesText' in before
    if MODE == 'author' and 'MagazineCountRow' not in before:
        text = before['SpareMagazinesText']['node']
        parent = before['WeaponContainer']['node']['widget']
        old_slot = before['SpareMagazinesText']['slot_properties']
        assert text['parent'] == parent
        offsets = old_slot['layoutData']['offsets']
        tint = before['SpareMagazinesText']['properties']['colorAndOpacity']['specifiedColor']
        row = call(UMG, 'AddWidget', widgetBlueprint=ref(HUD), widgetClass=ref('/Script/UMG.HorizontalBox'),
                   widgetDisplayName='MagazineCountRow', parentWidget=parent)
        set_props(row['widget'], {'visibility': 'HitTestInvisible'})
        set_props(row['slot'], {'layoutData': {'offsets': {'left': offsets['left'] + offsets['right'],
            'top': offsets['top'], 'right': 0, 'bottom': 0},
            'anchors': old_slot['layoutData']['anchors'], 'alignment': {'x': 1, 'y': 0}},
            'bAutoSize': True, 'zOrder': old_slot['zOrder']})
        icon = call(UMG, 'AddWidget', widgetBlueprint=ref(HUD), widgetClass=ref('/Script/UMG.Image'),
                    widgetDisplayName='MagazineIcon', parentWidget=row['widget'])
        set_props(icon['widget'], {'visibility': 'HitTestInvisible', 'colorAndOpacity': tint,
            'brush': {'resourceObject': ref(TEXTURE), 'drawAs': 'Image', 'imageSize': {'x': 18, 'y': 30}}})
        set_props(icon['slot'], {'padding': {'left': 0, 'top': 0, 'right': 8, 'bottom': 0},
            'size': {'sizeRule': 'Automatic', 'value': 1}, 'verticalAlignment': 'VAlign_Center'})
        moved = call(UMG, 'MoveWidget', widgetBlueprint=ref(HUD), widget=text['widget'], newParent=row['widget'])
        set_props(moved['slot'], {'padding': {'left': 0, 'top': 0, 'right': 0, 'bottom': 0},
            'size': {'sizeRule': 'Automatic', 'value': 1}, 'verticalAlignment': 'VAlign_Center'})
    after = snapshot()
    if MODE == 'author':
        assert set(after) == set(before) | {'MagazineCountRow', 'MagazineIcon'}
        for name in before:
            if name == 'SpareMagazinesText':
                assert before[name]['properties'] == after[name]['properties'], 'Count text style changed'
            else:
                assert before[name] == after[name], 'Unrelated HUD change: ' + name
        assert after['SpareMagazinesText']['node']['bIsVariable']
        assert after['SpareMagazinesText']['node']['parent'] == after['MagazineCountRow']['node']['widget']
        assert after['MagazineCountRow']['node']['parent'] == after['WeaponContainer']['node']['widget']
        assert after['MagazineIcon']['properties']['brush']['resourceObject'] == ref(TEXTURE)
    return {'mode': MODE, 'before': before, 'after': after,
            'compile_and_save': [HUD] if MODE == 'author' else []}
