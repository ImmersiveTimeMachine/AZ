"""Approved Fight/Explore HUD option 1, via Unreal's registered native UMG tools.

audit() is read-only. author() backs up the existing HUD and adds four widgets;
it does not compile, save, alter gameplay, or start/stop PIE. Use the dedicated
CompileWidgetBlueprint tool after this call returns, then save this package.
"""
import copy
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path
import shutil
import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'Saved/HUDMode'
HUD_PACKAGE = '/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD'
HUD = HUD_PACKAGE + '.WBP_AZ_GameHUD'
UMG = 'UMGToolSet.UMGToolSet.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
EDITOR = 'EditorToolset.EditorAppToolset.'
NEW_NAMES = ('ModeContainer', 'FightModeIcon', 'ExploreModeIcon', 'ModeNameText')
WHITE = {'r': 0.8549926280975342, 'g': 0.8227857351303101,
         'b': 0.7454041838645935, 'a': 1.0}
FIELDS = ('visibility', 'renderTransform', 'renderTransformPivot', 'brush', 'font',
          'colorAndOpacity', 'text', 'justification', 'autoWrapText', 'textOverflowPolicy',
          'stretch', 'stretchDirection', 'size', 'currentPercent', 'targetPercent',
          'fillColorCurrent', 'backgroundTint', 'blendMask', 'bIsFocusable')


def call(name, **arguments):
    toolset, tool = name.rsplit('.', 1)
    result = unreal.ToolsetRegistry.execute_tool(toolset, tool, json.dumps(arguments))
    if not result.is_complete or result.error:
        raise RuntimeError(name + ': ' + (result.error or 'asynchronous result requires a separate continuation'))
    return json.loads(result.value)['returnValue']


def ref(path):
    return {'refPath': path}


def merge(before, changes):
    result = copy.deepcopy(before)
    for name, value in changes.items():
        result[name] = merge(result[name], value) if isinstance(value, dict) and isinstance(result.get(name), dict) else copy.deepcopy(value)
    return result


def props(obj, fields=None, changes=None):
    schema = json.loads(call(OBJ + 'list_properties', instance=obj))
    names = list(changes) if changes is not None else [n for n in (fields or schema) if n in schema]
    if set(names) - set(schema):
        raise RuntimeError('Unsupported fields: ' + str(set(names) - set(schema)))
    before = json.loads(call(OBJ + 'get_properties', instance=obj, properties=names))
    if changes is not None:
        desired = merge(before, changes)
        if not call(OBJ + 'set_properties', instance=obj, values=json.dumps(desired)):
            raise RuntimeError('Property write failed: ' + str(obj))
        return json.loads(call(OBJ + 'get_properties', instance=obj, properties=names))
    return before


def rows():
    result = call(UMG + 'GetWidgets', widgetBlueprint=ref(HUD))
    return {r['widgetName']: r for r in result['widgets'] if isinstance(r['widget'], dict)}


def snapshot():
    result = {}
    for name, row in rows().items():
        result[name] = {
            'parent': row['parent'], 'class': row['widgetClassPath'],
            'variable': row['bIsVariable'],
            'properties': props(row['widget'], FIELDS),
            'slot': props(row['slot']) if isinstance(row['slot'], dict) else None,
        }
    return result


def idle():
    if call(EDITOR + 'IsPIERunning'):
        raise RuntimeError('PIE is active; let the user finish testing before asset authoring.')


def audit():
    OUT.mkdir(exist_ok=True)
    captured = snapshot()
    (OUT / 'widgets-audit.json').write_text(json.dumps(captured, indent=2), encoding='utf-8')
    return {'widgets': len(captured), 'existing_mode_widgets': [n for n in NEW_NAMES if n in captured]}


def slot(x, y, w, h):
    return {'layoutData': {'offsets': {'left': x, 'top': y, 'right': w, 'bottom': h},
            'anchors': {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 0, 'y': 0}},
            'alignment': {'x': 0, 'y': 0}}, 'bAutoSize': False, 'zOrder': 1}


def add(name, cls, parent, widget_properties, bounds):
    info = call(UMG + 'AddWidget', widgetBlueprint=ref(HUD), widgetClass=ref(cls),
                widgetDisplayName=name, parentWidget=parent)
    if info.get('widgetName') != name:
        raise RuntimeError('Unexpected widget name: ' + str(info))
    props(info['widget'], changes=widget_properties)
    props(info['slot'], changes=slot(*bounds))
    return info['widget']


def author():
    idle()
    OUT.mkdir(exist_ok=True)
    before = snapshot()
    if any(n in before for n in NEW_NAMES):
        raise RuntimeError('Mode widgets already exist; verify/resume instead of adding duplicates.')
    if HUD_PACKAGE in [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]:
        raise RuntimeError('HUD has existing unsaved changes; preserve them before authoring.')
    icons = {}
    for state, filename in [('Fight', 'T_HUD_Fists'), ('Explore', 'T_HUD_Explore')]:
        path = '/Game/AZ/Blueprints/Menu/HUD/Art/' + filename + '.' + filename
        if not unreal.load_asset(path):
            raise RuntimeError('Missing approved symbol: ' + path)
        icons[state] = path
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    backup = OUT / ('Backup_' + stamp)
    backup.mkdir()
    source = ROOT / 'Content/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD.uasset'
    shutil.copy2(source, backup / source.name)
    (OUT / 'widgets-before.json').write_text(json.dumps(before, indent=2), encoding='utf-8')
    (OUT / 'backup.json').write_text(json.dumps({'file': str(backup / source.name),
        'sha256': hashlib.sha256(source.read_bytes()).hexdigest()}, indent=2), encoding='utf-8')
    core = rows()['CoreHUD']['widget']
    # CoreHUD begins at (1455,820) at the approved 1920x1080 reference size.
    # Match option 1's (1506,900) icon frame; preserve the existing health slot.
    mode = add('ModeContainer', '/Script/UMG.CanvasPanel', core,
               {'visibility': 'Collapsed'}, (51, 80, 282, 70))
    for state in ['Fight', 'Explore']:
        add(state + 'ModeIcon', '/Script/UMG.Image', mode,
            {'visibility': 'Collapsed', 'colorAndOpacity': WHITE,
             'brush': {'resourceObject': ref(icons[state]), 'imageSize': {'x': 128, 'y': 128},
                       'drawAs': 'Image'}}, (0, 0, 70, 70))
    font = copy.deepcopy(before['WeaponNameText']['properties']['font'])
    font['size'] = 27 * .75  # Oswald: 96-DPI pixels to Slate points, same HUD convention.
    add('ModeNameText', '/Script/UMG.TextBlock', mode,
        {'visibility': 'HitTestInvisible', 'text': '', 'font': font,
         'colorAndOpacity': {'specifiedColor': WHITE, 'colorUseRule': 'UseColor_Specified'},
         'autoWrapText': False, 'textOverflowPolicy': 'Ellipsis'}, (86, 20, 180, 42))
    return verify()


def verify():
    before = json.loads((OUT / 'widgets-before.json').read_text(encoding='utf-8'))
    after = snapshot()
    if set(after) - set(before) != set(NEW_NAMES) or set(before) - set(after):
        raise RuntimeError('Unexpected widget additions/removals.')
    changed = [n for n in before if before[n] != after[n]]
    if changed:
        raise RuntimeError('Existing widget presentation changed: ' + str(changed))
    mode = after['ModeContainer']
    if mode['slot']['layoutData']['offsets'] != slot(51, 80, 282, 70)['layoutData']['offsets']:
        raise RuntimeError('Approved mode position differs.')
    for state, resource in [('Fight', 'T_HUD_Fists'), ('Explore', 'T_HUD_Explore')]:
        data = after[state + 'ModeIcon']
        if not data['properties']['brush']['resourceObject']['refPath'].endswith('.' + resource):
            raise RuntimeError('Wrong mode symbol: ' + state)
        if data['slot']['layoutData']['offsets'] != slot(0, 0, 70, 70)['layoutData']['offsets']:
            raise RuntimeError('Wrong mode icon size: ' + state)
    (OUT / 'widgets-verified.json').write_text(json.dumps(after, indent=2), encoding='utf-8')
    return {'status': 'verified', 'new_widgets': list(NEW_NAMES),
            'preserved_widgets': len(before), 'total_widgets': len(after),
            'compile_then_save': HUD_PACKAGE}
