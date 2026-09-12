"""Author CHALK's separate, editable CommonUI quick-selector widgets.

Run through ProgrammaticToolset.execute_tool_script, which injects execute_tool.
MODE='audit' is read-only. After quick_select_assign.main('backup') and a native
build/restart, set BACKUP_READY=True and use MODE='entry' then MODE='root'. No
input, controller, inventory, maps, vendor assets, Blueprint graph logic,
compilation, saves, PIE or tests are changed here. Use dedicated BlueprintTools
to compile the two returned assets after this script returns; then assign the
classes/input/defaults through quick_select_assign.py.

The five independently positioned hosts follow the approved v03 cross. Runtime
creates only configured entries and collapses unused hosts. All text, fills,
edges, scale boxes and groups remain individual UMG widgets. No flattened
mockup image, generated scenery, sample item count or unsupported action is
imported. Native entry/root classes own presentation data and input.
"""
import copy
import json


MODE = 'audit'
BACKUP_READY = False
UMG = 'UMGToolSet.UMGToolSet.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
FOLDER = '/Game/AZ/Blueprints/Menu/HUD/QuickSelect'
ENTRY_NAME = 'WBP_AZ_QuickSelectEntry'
ROOT_NAME = 'WBP_AZ_QuickSelect'
ENTRY = FOLDER + '/' + ENTRY_NAME + '.' + ENTRY_NAME
ROOT = FOLDER + '/' + ROOT_NAME + '.' + ROOT_NAME
ENTRY_PARENT = '/Script/AZ.AZ_QuickSelectEntryWidget'
ROOT_PARENT = '/Script/AZ.AZ_QuickSelectWidget'
HUD = '/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD.WBP_AZ_GameHUD'
FONT = '/Game/InventorySystemPro/ExampleContent/Common/Art/Fonts/Oswald-Light_Font.Oswald-Light_Font'
BODY = '/Engine/EngineFonts/Roboto.Roboto'
HOSTS = {'CenterSlot': (188, 227), 'LeftSlot': (0, 227), 'RightSlot': (376, 227),
         'UpSlot': (188, 79), 'DownSlot': (188, 375)}
ENTRY_VARIABLES = {'HighlightBorder': '/Script/UMG.Border', 'Icon': '/Script/UMG.Image',
                   **{name: '/Script/UMG.TextBlock' for name in
                      ('CategoryText', 'NameText', 'AmmoText', 'KeyText', 'StateText')}}
ROOT_VARIABLES = {**{name: '/Script/UMG.SizeBox' for name in HOSTS},
                  **{name: '/Script/UMG.TextBlock' for name in ('HeaderText', 'HintText', 'StatusText')}}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def call(prefix, name, **kwargs):
    data = execute_tool(prefix + name, json.dumps(kwargs))
    require(isinstance(data, dict) and 'returnValue' in data, 'Tool failed: ' + name + ' ' + str(data))
    return data['returnValue']


def ref(path):
    return {'refPath': path}


def merge(before, changes):
    result = copy.deepcopy(before)
    for key, value in changes.items():
        result[key] = merge(result[key], value) if isinstance(value, dict) and isinstance(result.get(key), dict) else copy.deepcopy(value)
    return result


def properties(instance, changes=None, names=None):
    # Mandatory UMG property discovery for every widget AND slot. Never guess
    # an inherited property name and accept a silently ignored write.
    schema = json.loads(call(OBJ, 'list_properties', instance=instance))
    selected = list(changes) if changes is not None else names
    if selected is None:
        return schema
    require(not set(selected) - set(schema), 'Unknown properties: ' + str(set(selected) - set(schema)))
    before = json.loads(call(OBJ, 'get_properties', instance=instance, properties=selected))
    if changes is None:
        return before
    require(call(OBJ, 'set_properties', instance=instance, values=json.dumps(merge(before, changes))),
            'Property write failed: ' + str(instance))
    after = json.loads(call(OBJ, 'get_properties', instance=instance, properties=selected))
    # Some setters normalize sizes, enum serialization or brush defaults; the
    # exact geometry readbacks below validate those critical final values.
    return after


def tree(path):
    return call(UMG, 'GetWidgets', widgetBlueprint=ref(path))


def real_widgets(value):
    return {row['widgetName']: row for row in value['widgets'] if isinstance(row.get('widget'), dict)}


def color(rgb, alpha=1):
    channels = [int(rgb[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    channels = [v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4 for v in channels]
    return dict(zip(('r', 'g', 'b', 'a'), channels + [alpha]))


WHITE = color('EEEAE0')
SOFT = color('B9B9B9')
CHARCOAL = color('16201D', .88)
EDGE = color('64706A', .9)


def slate(value):
    return {'specifiedColor': value, 'colorUseRule': 'UseColor_Specified'}


def canvas_slot(x, y, width, height, anchor=(0, 0), align=(0, 0), z=0):
    return {'layoutData': {'offsets': {'left': x, 'top': y, 'right': width, 'bottom': height},
            'anchors': {'minimum': {'x': anchor[0], 'y': anchor[1]},
                        'maximum': {'x': anchor[0], 'y': anchor[1]}},
            'alignment': {'x': align[0], 'y': align[1]}}, 'bAutoSize': False, 'zOrder': z}


def stretch_slot(inset=0, z=0):
    return {'layoutData': {'offsets': {'left': inset, 'top': inset, 'right': inset, 'bottom': inset},
            'anchors': {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 1, 'y': 1}},
            'alignment': {'x': 0, 'y': 0}}, 'bAutoSize': False, 'zOrder': z}


def add(path, name, widget_class, parent=None, props=None, slot=None, variable=False):
    kwargs = {'widgetBlueprint': ref(path), 'widgetClass': ref(widget_class), 'widgetDisplayName': name}
    if parent:
        kwargs['parentWidget'] = parent
    row = call(UMG, 'AddWidget', **kwargs)
    require(row and isinstance(row.get('widget'), dict), 'Cannot add ' + name)
    if row['widgetName'] != name:
        row = call(UMG, 'RenameWidget', widgetBlueprint=ref(path), widget=row['widget'], newDisplayName=name)
    properties(row['widget'], props or {'visibility': 'HitTestInvisible'})
    if isinstance(row.get('slot'), dict):
        properties(row['slot'], slot) if slot else properties(row['slot'])
    call(UMG, 'ToggleWidgetAsVariable', widgetBlueprint=ref(path), widget=row['widget'], bIsVariable=variable)
    return row['widget']


def text(path, name, parent, box, px, value='', tint=None, font=FONT, align='Left', variable=True):
    return add(path, name, '/Script/UMG.TextBlock', parent,
        {'text': value, 'font': {'fontObject': ref(font),
         'typefaceFontName': 'Default' if font == FONT else 'Regular', 'size': px * .75,
         'letterSpacing': 0, 'skewAmount': 0, 'outlineSettings': {'outlineSize': 0}},
         'colorAndOpacity': slate(tint or WHITE), 'shadowColorAndOpacity': color('000000', .72),
         'shadowOffset': {'x': 1, 'y': 1}, 'justification': align,
         'textOverflowPolicy': 'Ellipsis',
         'visibility': 'HitTestInvisible'}, canvas_slot(*box, z=4), variable)


def flat_image(path, name, parent, tint, slot, visible='HitTestInvisible'):
    return add(path, name, '/Script/UMG.Image', parent,
        {'brush': {'drawAs': 'Image', 'tintColor': slate(color('FFFFFF'))},
         'colorAndOpacity': tint, 'visibility': visible}, slot)


def begin_asset(name, path, parent):
    created = call(UMG, 'CreateWidgetBlueprint', folderPath=FOLDER, assetName=name, parentClass=ref(parent))
    current = tree(path)
    require(current['info']['parentClass'] == ref(parent), 'Wrong native parent on ' + path)
    require(not real_widgets(current), 'Existing widget tree must be inspected/resumed explicitly: ' + path)
    return created


def author_entry():
    created = begin_asset(ENTRY_NAME, ENTRY, ENTRY_PARENT)
    canvas = add(ENTRY, 'EntryCanvas', '/Script/UMG.CanvasPanel', props={'visibility': 'Visible'})
    # A 2px color-changing outer fill under an inset charcoal fill produces an
    # editable outline. Native HighlightBorder.SetBrushColor never tints the
    # entire card or makes its content unreadable.
    add(ENTRY, 'HighlightBorder', '/Script/UMG.Border', canvas,
        {'brushColor': EDGE, 'background': {'drawAs': 'Image', 'tintColor': slate(color('FFFFFF'))},
         'padding': {'left': 0, 'top': 0, 'right': 0, 'bottom': 0},
         'visibility': 'HitTestInvisible'}, stretch_slot(), True)
    flat_image(ENTRY, 'CardFill', canvas, CHARCOAL, stretch_slot(2, z=1))
    # Inner fill is intentionally near-opaque to keep the outline hue from
    # reading as a full-card selection tint. Independent opacity is editable.
    scale = add(ENTRY, 'IconScale', '/Script/UMG.ScaleBox', canvas,
        {'stretch': 'ScaleToFit', 'stretchDirection': 'Both', 'visibility': 'HitTestInvisible'},
        canvas_slot(15, 28, 138, 60, z=3))
    add(ENTRY, 'Icon', '/Script/UMG.Image', scale,
        {'brush': {'drawAs': 'Image', 'tintColor': slate(color('FFFFFF')),
                   'imageSize': {'x': 138, 'y': 60}},
         'colorAndOpacity': WHITE, 'visibility': 'Collapsed'},
        {'horizontalAlignment': 'HAlign_Center', 'verticalAlignment': 'VAlign_Center'}, True)
    text(ENTRY, 'CategoryText', canvas, (12, 8, 116, 22), 16, tint=SOFT)
    text(ENTRY, 'KeyText', canvas, (130, 8, 26, 22), 16, tint=SOFT, align='Right')
    text(ENTRY, 'NameText', canvas, (12, 86, 144, 21), 17)
    text(ENTRY, 'StateText', canvas, (12, 106, 88, 18), 13, tint=SOFT, font=BODY)
    text(ENTRY, 'AmmoText', canvas, (98, 104, 58, 23), 19, align='Right')
    return {'asset': created, 'tree': tree(ENTRY), 'compile_after_return': [ENTRY]}


def author_root():
    created = begin_asset(ROOT_NAME, ROOT, ROOT_PARENT)
    canvas = add(ROOT, 'QuickSelectRoot', '/Script/UMG.CanvasPanel', props={'visibility': 'Visible'})
    flat_image(ROOT, 'SceneDim', canvas, color('07100E', .24), stretch_slot(), 'Visible')
    safe = add(ROOT, 'QuickSelectSafeArea', '/Script/UMG.SafeZone', canvas,
        {'visibility': 'SelfHitTestInvisible', 'padLeft': True, 'padRight': True,
         'padTop': True, 'padBottom': True}, stretch_slot(z=1))
    layout = add(ROOT, 'SafeLayout', '/Script/UMG.CanvasPanel', safe,
        {'visibility': 'SelfHitTestInvisible'}, {'hAlign': 'HAlign_Fill', 'vAlign': 'VAlign_Fill'})
    # At 1920x1080 the host centers reproduce v03: Left=(930,538),
    # Center=(1118,538), Right=(1306,538), Up=(1118,390), Down=(1118,686).
    # Pointer navigation hit-tests each live card; it is not tied to viewport center.
    cross = add(ROOT, 'CrossLayout', '/Script/UMG.CanvasPanel', layout,
        {'visibility': 'SelfHitTestInvisible'},
        canvas_slot(0, 32, 544, 650, (.5822916667, .5), (.5, .5)))
    text(ROOT, 'HeaderText', cross, (0, 0, 544, 32), 23, 'QUICK SELECT', SOFT, align='Center')
    for name, (x, y) in HOSTS.items():
        add(ROOT, name, '/Script/UMG.SizeBox', cross,
            {'visibility': 'Collapsed', 'bOverride_WidthOverride': True, 'widthOverride': 168,
             'bOverride_HeightOverride': True, 'heightOverride': 128},
            canvas_slot(x, y, 168, 128, z=2), True)
    text(ROOT, 'StatusText', cross, (0, 532, 544, 52), 20, tint=WHITE, font=BODY, align='Center')
    text(ROOT, 'HintText', cross, (0, 601, 544, 45), 18, tint=SOFT, font=BODY, align='Center')
    return {'asset': created, 'tree': tree(ROOT), 'compile_after_return': [ROOT]}


def verify_variables(path, parent, expected):
    data = tree(path)
    require(data['info']['parentClass'] == ref(parent), 'Unexpected parent on ' + path)
    widgets = real_widgets(data)
    require(not set(expected) - set(widgets), 'Missing BindWidget elements on ' + path)
    for name, cls in expected.items():
        require(widgets[name]['widgetClassPath'] == ref(cls) and widgets[name]['bIsVariable'],
                'BindWidget type/variable flag differs: ' + name)
    return data, widgets


def verify():
    entry, entry_widgets = verify_variables(ENTRY, ENTRY_PARENT, ENTRY_VARIABLES)
    root, root_widgets = verify_variables(ROOT, ROOT_PARENT, ROOT_VARIABLES)
    for name, (x, y) in HOSTS.items():
        actual = properties(root_widgets[name]['slot'], names=['layoutData', 'bAutoSize', 'zOrder'])
        require(actual == canvas_slot(x, y, 168, 128, z=2), 'Cross host geometry differs: ' + name)
    for name in ('NameText', 'AmmoText', 'KeyText', 'StateText', 'CategoryText'):
        require(properties(entry_widgets[name]['widget'], names=['text'])['text'] == '',
                'An illustrative runtime label/count was authored: ' + name)
    return {'entry': entry, 'root': root, 'runtime_values': 'Empty until native views arrive',
            'scope': 'Separate selector widgets only; existing HUD/inventory unchanged'}


def run():
    if MODE == 'audit':
        return {'existing_hud': tree(HUD), 'planned_root': ROOT, 'planned_entry': ENTRY,
                'native_parents': [ROOT_PARENT, ENTRY_PARENT], 'host_positions': HOSTS,
                'next': 'Back up controller/shared IMC; build native classes; author entry/root'}
    if MODE == 'verify':
        return verify()
    require(BACKUP_READY, 'Run quick_select_assign.main("backup") before asset authoring')
    if MODE == 'entry':
        return author_entry()
    if MODE == 'root':
        return author_root()
    if MODE == 'author':
        return {'entry': author_entry(), 'root': author_root(),
                'compile_after_return': [ENTRY, ROOT], 'save': 'Dedicated tools after defaults assignment'}
    raise RuntimeError('Unknown MODE: ' + MODE)
