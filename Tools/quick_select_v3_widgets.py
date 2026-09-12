"""Prepare the compact eight-cell selector and focused inventory-composite panel.

Use ProgrammaticToolset.execute_tool_script. Importing defines helpers only;
MODE='audit' is the default and performs no asset mutations. After backing up
the existing root widget, finishing the native build/restart, importing the
arrow texture, and authoring/compiling quick_select_focus_details.py assets,
run MODE='author' with BACKUP_READY=True. Compile/save the returned root asset
outside this script, then run MODE='verify'. Authoring refuses active PIE.

This changes only WBP_AZ_QuickSelect. Existing first-slot USizeBoxes retain
their names/classes and design sizes; authored ScaleBoxes fit their unchanged
168x128 entry/composite content into 116x96 cells. Four second hosts extend
the cross. Inventory, item data, input mappings, existing entry/composite BPs,
Fists BP, textures, gameplay actors and maps are never modified here.
"""
import copy
import json


MODE = 'audit'
BACKUP_READY = False
UMG = 'UMGToolSet.UMGToolSet.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
ASSET = 'editor_toolset.toolsets.asset.AssetTools.'
EDITOR = 'EditorToolset.EditorAppToolset.'
ROOT = '/Game/AZ/Blueprints/Menu/HUD/QuickSelect/WBP_AZ_QuickSelect.WBP_AZ_QuickSelect'
ROOT_PARENT = '/Script/AZ.AZ_QuickSelectWidget'
FOCUS_CLASS = '/Game/AZ/Blueprints/Menu/HUD/QuickSelect/WBP_AZ_QuickSelectFocusDetails.WBP_AZ_QuickSelectFocusDetails_C'
ARROW = '/Game/AZ/Blueprints/Menu/HUD/Art/T_HUD_ArrowUp.T_HUD_ArrowUp'
FONT = '/Game/InventorySystemPro/ExampleContent/Common/Art/Fonts/Oswald-Light_Font.Oswald-Light_Font'
BODY = '/Engine/EngineFonts/Roboto.Roboto'
OUTPUT = 'C:/UnrealEngine/Games/AZ/Saved/QuickSelect'
HOST_BOXES = {
    'LeftSlot': (124, 192, 116, 96), 'LeftSlotSecond': (0, 192, 116, 96),
    'RightSlot': (312, 192, 116, 96), 'RightSlotSecond': (436, 192, 116, 96),
    'UpSlot': (218, 108, 116, 96), 'UpSlotSecond': (218, 4, 116, 96),
    'DownSlot': (218, 276, 116, 96), 'DownSlotSecond': (218, 380, 116, 96),
}
ARROWS = {
    'ArrowUp': ((271.5, 212, 9, 9), 0), 'ArrowRight': ((295, 235.5, 9, 9), 90),
    'ArrowDown': ((271.5, 259, 9, 9), 180), 'ArrowLeft': ((248, 235.5, 9, 9), 270),
}
FIRST_HOSTS = ('CenterSlot', 'LeftSlot', 'RightSlot', 'UpSlot', 'DownSlot')
SLOT_PROPS = ['layoutData', 'bAutoSize', 'zOrder']
SIZE_PROPS = ['widthOverride', 'heightOverride', 'bOverride_WidthOverride', 'bOverride_HeightOverride']


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def call(prefix, name, **kwargs):
    result = execute_tool(prefix + name, json.dumps(kwargs))
    require(isinstance(result, dict), 'Tool failed: ' + name)
    return result.get('returnValue')


def ref(path):
    return {'refPath': path}


def merge(before, changes):
    result = copy.deepcopy(before)
    for key, value in changes.items():
        result[key] = merge(result[key], value) if isinstance(value, dict) and isinstance(result.get(key), dict) else copy.deepcopy(value)
    return result


def properties(instance, changes=None, names=None):
    schema = json.loads(call(OBJ, 'list_properties', instance=instance))
    selected = list(changes) if changes is not None else names
    if selected is None:
        return schema
    require(not set(selected) - set(schema), 'Unknown properties on ' + str(instance) + ': ' + str(set(selected) - set(schema)))
    before = json.loads(call(OBJ, 'get_properties', instance=instance, properties=selected))
    if changes is None:
        return before
    require(call(OBJ, 'set_properties', instance=instance, values=json.dumps(merge(before, changes))),
            'Could not set properties on ' + str(instance))
    return json.loads(call(OBJ, 'get_properties', instance=instance, properties=selected))


def tree():
    return call(UMG, 'GetWidgets', widgetBlueprint=ref(ROOT))


def widgets(data=None):
    return {row['widgetName']: row for row in (data or tree())['widgets'] if isinstance(row.get('widget'), dict)}


def widget(rows, name, expected_class=None):
    require(name in rows, 'Missing widget: ' + name)
    row = rows[name]
    if expected_class:
        expected_paths = {expected_class}
        if expected_class.endswith('_C'):
            expected_paths.add(expected_class[:-2])  # GetWidgets reports Blueprint asset class paths.
        require(row['widgetClassPath'].get('refPath') in expected_paths, 'Wrong widget class: ' + name)
    return row


def equal(actual, expected):
    if isinstance(expected, dict):
        return isinstance(actual, dict) and set(actual) == set(expected) and all(equal(actual[k], v) for k, v in expected.items())
    if isinstance(expected, (int, float)) and not isinstance(expected, bool):
        return isinstance(actual, (int, float)) and abs(actual - expected) < .0001
    return actual == expected


def color(rgb, alpha=1.0):
    channels = [int(rgb[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    channels = [v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4 for v in channels]
    return dict(zip(('r', 'g', 'b', 'a'), channels + [alpha]))


def slate(value):
    return {'specifiedColor': value, 'colorUseRule': 'UseColor_Specified'}


def canvas_slot(x, y, width, height, z=0, anchor=(0, 0), align=(0, 0)):
    return {'layoutData': {'offsets': {'left': x, 'top': y, 'right': width, 'bottom': height},
            'anchors': {'minimum': {'x': anchor[0], 'y': anchor[1]}, 'maximum': {'x': anchor[0], 'y': anchor[1]}},
            'alignment': {'x': align[0], 'y': align[1]}}, 'bAutoSize': False, 'zOrder': z}


def add(name, widget_class, parent, props, slot, variable=False):
    row = call(UMG, 'AddWidget', widgetBlueprint=ref(ROOT), widgetClass=ref(widget_class),
               widgetDisplayName=name, parentWidget=parent)
    require(row and isinstance(row.get('widget'), dict), 'Could not add ' + name)
    if row['widgetName'] != name:
        row = call(UMG, 'RenameWidget', widgetBlueprint=ref(ROOT), widget=row['widget'], newDisplayName=name)
    properties(row['widget'], props)
    if isinstance(row.get('slot'), dict):
        properties(row['slot'], slot)
    call(UMG, 'ToggleWidgetAsVariable', widgetBlueprint=ref(ROOT), widget=row['widget'], bIsVariable=variable)
    return row


def ensure_widget(name, widget_class, parent, props, slot, variable=False):
    rows = widgets()
    if name not in rows:
        return add(name, widget_class, parent, props, slot, variable)
    row = widget(rows, name, widget_class)
    require(row['parent'] == parent, 'Unexpected parent for ' + name)
    properties(row['widget'], props)
    properties(row['slot'], slot)
    if variable:
        call(UMG, 'ToggleWidgetAsVariable', widgetBlueprint=ref(ROOT), widget=row['widget'], bIsVariable=True)
    return row


def text_props(px, body=False, wrap=False, tint='EEEAE0'):
    return {'text': '', 'font': {'fontObject': ref(BODY if body else FONT),
            'typefaceFontName': 'Regular' if body else 'Default', 'size': px * .75,
            'letterSpacing': 0, 'skewAmount': 0, 'outlineSettings': {'outlineSize': 0}},
            'colorAndOpacity': slate(color(tint)), 'shadowColorAndOpacity': color('000000', .72),
            'shadowOffset': {'x': 1, 'y': 1}, 'justification': 'Center', 'autoWrapText': wrap,
            'textOverflowPolicy': 'Ellipsis', 'visibility': 'HitTestInvisible'}


def audit():
    data = tree()
    require(data['info']['parentClass'] == ref(ROOT_PARENT), 'Unexpected root native parent')
    rows = widgets(data)
    widget(rows, 'CrossLayout', '/Script/UMG.CanvasPanel')
    for name in FIRST_HOSTS:
        widget(rows, name, '/Script/UMG.SizeBox')
    for name in ('HintText', 'StatusText', 'HeaderText'):
        widget(rows, name, '/Script/UMG.TextBlock')
    return {'root': data, 'geometry': {name: properties(row['slot'], names=SLOT_PROPS)
            for name, row in rows.items() if isinstance(row.get('slot'), dict)
            and row['parent'] == rows['CrossLayout']['widget']},
            'requirements': {'compiled_focus_class': FOCUS_CLASS, 'up_arrow_texture': ARROW},
            'target_bbox': [552, 664], 'target_arrow_center': [276, 240]}


def preflight():
    report = audit()
    properties(ref(FOCUS_CLASS))  # Validate the generated class before editing any widgets.
    require(call(ASSET, 'exists', path=ARROW), 'Import the native GIMP upward arrow texture first')
    rows = widgets(report['root'])
    for name in HOST_BOXES:
        if name in rows:
            row = widget(rows, name, '/Script/UMG.SizeBox')
            require(row['parent'] == rows['CrossLayout']['widget'] or
                    row['parent'] == ref(ROOT + ':WidgetTree.' + name + 'Scale'),
                    'Unexpected slot parent: ' + name)
        wrapper_name = name + 'Scale'
        if wrapper_name in rows:
            widget(rows, wrapper_name, '/Script/UMG.ScaleBox')
    return report


def author_cells(cross):
    for name, box in HOST_BOXES.items():
        wrapper = ensure_widget(name + 'Scale', '/Script/UMG.ScaleBox', cross,
            {'stretch': 'ScaleToFit', 'stretchDirection': 'Both', 'visibility': 'SelfHitTestInvisible'},
            canvas_slot(*box, z=2))
        rows = widgets()
        size_props = {'widthOverride': 168, 'heightOverride': 128,
                      'bOverride_WidthOverride': True, 'bOverride_HeightOverride': True,
                      'visibility': 'SelfHitTestInvisible'}
        child_slot = {'horizontalAlignment': 'HAlign_Center', 'verticalAlignment': 'VAlign_Center'}
        if name not in rows:
            add(name, '/Script/UMG.SizeBox', wrapper['widget'], size_props, child_slot, True)
        else:
            host = rows[name]
            if host['parent'] != wrapper['widget']:
                host = call(UMG, 'MoveWidget', widgetBlueprint=ref(ROOT), widget=host['widget'], newParent=wrapper['widget'])
            properties(host['widget'], size_props)
            properties(host['slot'], child_slot)
            call(UMG, 'ToggleWidgetAsVariable', widgetBlueprint=ref(ROOT), widget=host['widget'], bIsVariable=True)


def author_arrows(cross):
    for name, (box, angle) in ARROWS.items():
        ensure_widget(name, '/Script/UMG.Image', cross,
            {'brush': {'drawAs': 'Image', 'resourceObject': ref(ARROW),
                       'imageSize': {'x': 9, 'y': 9}, 'tintColor': slate(color('FFFFFF'))},
             'colorAndOpacity': color('8E9992'), 'renderTransform': {'angle': angle},
             'visibility': 'HitTestInvisible'}, canvas_slot(*box, z=3))


def author_footer(cross):
    ensure_widget('FocusDetails', FOCUS_CLASS, cross, {'visibility': 'HitTestInvisible'},
                  canvas_slot(0, 492, 552, 100, 4), True)
    ensure_widget('FocusNameText', '/Script/UMG.TextBlock', cross, text_props(34),
                  canvas_slot(0, 492, 552, 42, 5), True)
    ensure_widget('FocusDescriptionText', '/Script/UMG.TextBlock', cross, text_props(20, body=True, wrap=True, tint='B9B9B9'),
                  canvas_slot(0, 536, 552, 54, 5), True)
    ensure_widget('ModeText', '/Script/UMG.TextBlock', cross, text_props(14, tint='B9B9B9'),
                  canvas_slot(366, 460, 186, 20, 5), True)
    rows = widgets()
    properties(rows['HeaderText']['widget'], {'visibility': 'Collapsed'})
    properties(rows['StatusText']['slot'], canvas_slot(0, 596, 552, 28, 4))
    properties(rows['HintText']['slot'], canvas_slot(0, 632, 552, 28, 4))


def verify():
    report = audit()
    rows = widgets(report['root'])
    expected_cross = canvas_slot(0, -2, 552, 664, anchor=(.5822916667, .5), align=(.5, 240 / 664))
    require(equal(properties(rows['CrossLayout']['slot'], names=SLOT_PROPS), expected_cross), 'Cross center/bounds differ')
    for name, box in HOST_BOXES.items():
        host = widget(rows, name, '/Script/UMG.SizeBox')
        wrapper = widget(rows, name + 'Scale', '/Script/UMG.ScaleBox')
        require(host['bIsVariable'] and host['parent'] == wrapper['widget'], 'Host binding/scale parent differs: ' + name)
        require(equal(properties(wrapper['slot'], names=SLOT_PROPS), canvas_slot(*box, z=2)), 'Cell geometry differs: ' + name)
        require(properties(wrapper['widget'], names=['stretch', 'stretchDirection']) == {'stretch': 'ScaleToFit', 'stretchDirection': 'Both'},
                'Cell scaling differs: ' + name)
        require(properties(host['widget'], names=SIZE_PROPS) == {'widthOverride': 168, 'heightOverride': 128,
                'bOverride_WidthOverride': True, 'bOverride_HeightOverride': True}, 'Cell design size differs: ' + name)
    require(equal(properties(rows['CenterSlot']['slot'], names=SLOT_PROPS), canvas_slot(396, 360, 92, 96, 2)), 'Mode-card position differs')
    for name, (box, angle) in ARROWS.items():
        arrow = widget(rows, name, '/Script/UMG.Image')
        require(equal(properties(arrow['slot'], names=SLOT_PROPS), canvas_slot(*box, z=3)), 'Arrow position differs: ' + name)
        values = properties(arrow['widget'], names=['brush', 'renderTransform', 'colorAndOpacity'])
        require(values['brush']['resourceObject'] == ref(ARROW) and equal(values['renderTransform']['angle'], angle)
                and equal(values['colorAndOpacity'], color('8E9992')), 'Arrow brush/rotation/tint differs: ' + name)
    for name, kind, box in [('FocusDetails', FOCUS_CLASS, (0, 492, 552, 100)),
                           ('FocusNameText', '/Script/UMG.TextBlock', (0, 492, 552, 42)),
                           ('FocusDescriptionText', '/Script/UMG.TextBlock', (0, 536, 552, 54)),
                           ('ModeText', '/Script/UMG.TextBlock', (366, 460, 186, 20))]:
        row = widget(rows, name, kind)
        require(row['bIsVariable'], 'Footer BindWidget is not exposed: ' + name)
        require(equal(properties(row['slot'], names=SLOT_PROPS), canvas_slot(*box, z=4 if name == 'FocusDetails' else 5)),
                'Footer geometry differs: ' + name)
        if name != 'FocusDetails':
            require(properties(row['widget'], names=['text'])['text'] == '', 'Illustrative runtime footer text remains')
    require(properties(rows['HeaderText']['widget'], names=['visibility'])['visibility'] == 'Collapsed', 'Old header remains visible')
    report['status'] = 'widget_readback_passed'
    return report


def run():
    require(MODE in ('audit', 'author', 'verify'), 'Unknown MODE: ' + str(MODE))
    if MODE == 'audit':
        return audit()
    if MODE == 'verify':
        result = verify()
        call(ASSET, 'write_file', file_path=OUTPUT + '/v3-widgets-verify.json', content=json.dumps(result, indent=2))
        return result
    require(BACKUP_READY, 'Back up the explicit root widget package before authoring')
    require(not call(EDITOR, 'IsPIERunning'), 'Stop PIE before authoring the widget')
    before = preflight()
    call(ASSET, 'write_file', file_path=OUTPUT + '/v3-widgets-before.json', content=json.dumps(before, indent=2))
    rows = widgets(before['root'])
    cross = rows['CrossLayout']['widget']
    properties(rows['CrossLayout']['slot'], canvas_slot(0, -2, 552, 664, anchor=(.5822916667, .5), align=(.5, 240 / 664)))
    author_cells(cross)
    center = widgets()['CenterSlot']
    properties(center['widget'], {'widthOverride': 92, 'heightOverride': 96,
                                'bOverride_WidthOverride': True, 'bOverride_HeightOverride': True})
    properties(center['slot'], canvas_slot(396, 360, 92, 96, 2))
    author_arrows(cross)
    author_footer(cross)
    result = {'status': 'authored_requires_compile_and_save', 'readback': verify(),
              'compile_after_return': [ROOT], 'explicit_save_assets': [ROOT],
              'native_contract': 'Nine slot definitions; ordinal 0/1 per direction, ordinal 0 for center',
              'scope': 'Selector root layout only; physical entry/composites, inventory, game state and input assets preserved'}
    call(ASSET, 'write_file', file_path=OUTPUT + '/v3-widgets-author.json', content=json.dumps(result, indent=2))
    return result
