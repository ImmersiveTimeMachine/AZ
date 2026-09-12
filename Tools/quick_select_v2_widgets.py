"""Revise the existing selector cards and author a compact intrinsic Fists card.

Run through ProgrammaticToolset.execute_tool_script; it supplies execute_tool
and invokes run(). MODE defaults to read-only audit. Before MODE='author', back
up the existing root/entry packages and any existing Fists package, set
BACKUP_READY=True, rebuild native reflection, then author/compile the three
composite assets using quick_select_composite_assets.py. Run MODE='verify'
after dedicated Blueprint compilation and explicit package saves.

Only these UI assets are changed: WBP_AZ_QuickSelectEntry, WBP_AZ_QuickSelect,
and WBP_AZ_QuickSelectFists. No gameplay defaults, source-pack/inventory widgets,
images, compilation, saves, PIE, or tests are changed by this script. Importing
it defines helpers only; no tool call happens until run(). Repeat authoring
accepts the known v1/v2 geometry and preserves all four outer cross hosts.
"""
import copy
import json


MODE = 'audit'
BACKUP_READY = False
UMG = 'UMGToolSet.UMGToolSet.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
ASSET = 'editor_toolset.toolsets.asset.AssetTools.'
EDITOR = 'EditorToolset.EditorAppToolset.'
FOLDER = '/Game/AZ/Blueprints/Menu/HUD/QuickSelect'
ENTRY = FOLDER + '/WBP_AZ_QuickSelectEntry.WBP_AZ_QuickSelectEntry'
ROOT = FOLDER + '/WBP_AZ_QuickSelect.WBP_AZ_QuickSelect'
FISTS_NAME = 'WBP_AZ_QuickSelectFists'
FISTS = FOLDER + '/' + FISTS_NAME + '.' + FISTS_NAME
DETAILS_CLASS = FOLDER + '/WBP_AZ_QuickSelectItemDetails.WBP_AZ_QuickSelectItemDetails_C'
ENTRY_PARENT = '/Script/AZ.AZ_QuickSelectEntryWidget'
ROOT_PARENT = '/Script/AZ.AZ_QuickSelectWidget'
OWNER_KEY = 'AZ.QuickSelectV2Widgets.Owner'
OWNER = 'quick_select_v2_widgets:v1'
OUTPUT = 'C:/UnrealEngine/Games/AZ/Saved/QuickSelect'
FONT = '/Game/InventorySystemPro/ExampleContent/Common/Art/Fonts/Oswald-Light_Font.Oswald-Light_Font'
BODY = '/Engine/EngineFonts/Roboto.Roboto'
OUTER_HOSTS = ('LeftSlot', 'RightSlot', 'UpSlot', 'DownSlot')
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
    # Discover every object AND slot before reading/writing its exact properties.
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


def tree(path):
    return call(UMG, 'GetWidgets', widgetBlueprint=ref(path))


def widgets(value):
    return {row['widgetName']: row for row in value['widgets'] if isinstance(row.get('widget'), dict)}


def widget(data, name, expected_class=None):
    require(name in data, 'Missing expected widget: ' + name)
    row = data[name]
    if expected_class:
        expected_paths = {expected_class}
        if expected_class.endswith('_C'):
            expected_paths.add(expected_class[:-2])  # GetWidgets reports a BP asset class path.
        require(row['widgetClassPath'].get('refPath') in expected_paths, 'Wrong widget type for ' + name)
    return row


def color(rgb, alpha=1.0):
    channels = [int(rgb[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    channels = [v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4 for v in channels]
    return dict(zip(('r', 'g', 'b', 'a'), channels + [alpha]))


def slate(value):
    return {'specifiedColor': value, 'colorUseRule': 'UseColor_Specified'}


def canvas_slot(x, y, width, height, z=0):
    return {'layoutData': {'offsets': {'left': x, 'top': y, 'right': width, 'bottom': height},
            'anchors': {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 0, 'y': 0}},
            'alignment': {'x': 0, 'y': 0}}, 'bAutoSize': False, 'zOrder': z}


def stretch_slot(inset=0, z=0):
    result = canvas_slot(inset, inset, inset, inset, z)
    result['layoutData']['anchors']['maximum'] = {'x': 1, 'y': 1}
    return result


def add(path, name, widget_class, parent=None, props=None, slot=None, variable=False):
    args = {'widgetBlueprint': ref(path), 'widgetClass': ref(widget_class), 'widgetDisplayName': name}
    if parent:
        args['parentWidget'] = parent
    row = call(UMG, 'AddWidget', **args)
    require(row and isinstance(row.get('widget'), dict), 'Could not add ' + name)
    if row['widgetName'] != name:
        row = call(UMG, 'RenameWidget', widgetBlueprint=ref(path), widget=row['widget'], newDisplayName=name)
    properties(row['widget'], props or {'visibility': 'HitTestInvisible'})
    if isinstance(row.get('slot'), dict):
        properties(row['slot'], slot) if slot is not None else properties(row['slot'])
    call(UMG, 'ToggleWidgetAsVariable', widgetBlueprint=ref(path), widget=row['widget'], bIsVariable=variable)
    return row['widget']


def text_props(px, value='', align='Left', tint=None):
    return {'text': value, 'font': {'fontObject': ref(FONT), 'typefaceFontName': 'Default',
            'size': px * .75, 'letterSpacing': 0, 'skewAmount': 0, 'outlineSettings': {'outlineSize': 0}},
            'colorAndOpacity': slate(tint or color('EEEAE0')), 'shadowColorAndOpacity': color('000000', .72),
            'shadowOffset': {'x': 1, 'y': 1}, 'justification': align,
            'textOverflowPolicy': 'Ellipsis', 'visibility': 'HitTestInvisible'}


def add_text(path, name, parent, box, px, value='', align='Left', tint=None):
    return add(path, name, '/Script/UMG.TextBlock', parent, text_props(px, value, align, tint),
               canvas_slot(*box, z=5), True)


def outer_snapshot(root_widgets):
    return {name: {'slot': properties(widget(root_widgets, name, '/Script/UMG.SizeBox')['slot'], names=SLOT_PROPS),
                   'size': properties(root_widgets[name]['widget'], names=SIZE_PROPS)} for name in OUTER_HOSTS}


def audit():
    entry_tree, root_tree = tree(ENTRY), tree(ROOT)
    require(entry_tree['info']['parentClass'] == ref(ENTRY_PARENT), 'Unexpected physical-entry native parent')
    require(root_tree['info']['parentClass'] == ref(ROOT_PARENT), 'Unexpected selector native parent')
    entry_widgets, root_widgets = widgets(entry_tree), widgets(root_tree)
    for name in ('NameText', 'CategoryText', 'AmmoText', 'KeyText', 'StateText'):
        widget(entry_widgets, name, '/Script/UMG.TextBlock')
    widget(entry_widgets, 'EntryCanvas', '/Script/UMG.CanvasPanel')
    center = widget(root_widgets, 'CenterSlot', '/Script/UMG.SizeBox')
    fists_exists = call(ASSET, 'exists', path=FISTS)
    return {'entry': entry_tree, 'root': root_tree, 'outer_hosts': outer_snapshot(root_widgets),
            'center': {'slot': properties(center['slot'], names=SLOT_PROPS),
                       'size': properties(center['widget'], names=SIZE_PROPS)},
            'fists': tree(FISTS) if fists_exists else None,
            'fists_owner': call(ASSET, 'get_metadata_tags', asset_path=FISTS).get(OWNER_KEY) if fists_exists else None,
            'required_compiled_composite_class': DETAILS_CLASS}


def preflight(report):
    # Accept only the known original host or this revision, preserving its center.
    offsets = report['center']['slot']['layoutData']['offsets']
    require(offsets in ({'left': 188, 'top': 227, 'right': 168, 'bottom': 128},
                        {'left': 226, 'top': 243, 'right': 92, 'bottom': 96}),
            'CenterSlot has unreviewed geometry; inspect before resizing')
    require(report['center']['slot']['layoutData']['anchors'] == {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 0, 'y': 0}}
            and report['center']['slot']['layoutData']['alignment'] == {'x': 0, 'y': 0},
            'CenterSlot anchors/alignment changed')
    entry_widgets = widgets(report['entry'])
    name_layout = properties(entry_widgets['NameText']['slot'], names=SLOT_PROPS)
    require(name_layout in (canvas_slot(12, 86, 144, 21, 4), canvas_slot(12, 8, 116, 22, 4)),
            'NameText has unreviewed geometry; inspect before moving')
    if report['fists'] is not None:
        require(report['fists_owner'] == OWNER, 'An unowned Fists widget already exists; do not overwrite it')
        require(report['fists']['info']['parentClass'] == ref(ENTRY_PARENT), 'Unexpected Fists native parent')
    # A class reference must resolve before any existing widget is modified.
    properties(ref(DETAILS_CLASS))


def author_physical():
    data = widgets(tree(ENTRY))
    canvas = data['EntryCanvas']['widget']
    properties(data['NameText']['slot'], canvas_slot(12, 8, 116, 22, 4))
    properties(data['CategoryText']['widget'], {'text': '', 'visibility': 'Collapsed'})
    if 'EmptyMarkText' not in data:
        add_text(ENTRY, 'EmptyMarkText', canvas, (15, 40, 138, 40), 30, '\u2014', 'Center', color('B9B9B9'))
    else:
        widget(data, 'EmptyMarkText', '/Script/UMG.TextBlock')
    if 'ItemDetails' not in data:
        add(ENTRY, 'ItemDetails', DETAILS_CLASS, canvas,
            {'visibility': 'HitTestInvisible'}, stretch_slot(z=4), True)
    else:
        widget(data, 'ItemDetails', DETAILS_CLASS)


def author_fists():
    if not call(ASSET, 'exists', path=FISTS):
        require(call(UMG, 'CreateWidgetBlueprint', folderPath=FOLDER, assetName=FISTS_NAME,
                     parentClass=ref(ENTRY_PARENT)), 'Could not create compact Fists widget')
        call(ASSET, 'update_metadata_tags', asset_path=FISTS, set_tags={OWNER_KEY: OWNER})
    existing = widgets(tree(FISTS))
    if existing:
        # A finished rerun is a no-op. A partial/custom tree needs review, not a destructive reset.
        verify_fists()
        return
    canvas = add(FISTS, 'EntryCanvas', '/Script/UMG.CanvasPanel', props={'visibility': 'Visible'})
    add(FISTS, 'HighlightBorder', '/Script/UMG.Border', canvas,
        {'brushColor': color('64706A', .9), 'background': {'drawAs': 'Image', 'tintColor': slate(color('FFFFFF'))},
         'padding': {'left': 0, 'top': 0, 'right': 0, 'bottom': 0}, 'visibility': 'HitTestInvisible'},
        stretch_slot(), True)
    add(FISTS, 'CardFill', '/Script/UMG.Image', canvas,
        {'brush': {'drawAs': 'Image', 'tintColor': slate(color('FFFFFF'))},
         'colorAndOpacity': color('16201D', .88), 'visibility': 'HitTestInvisible'}, stretch_slot(2, 1))
    scale = add(FISTS, 'IconScale', '/Script/UMG.ScaleBox', canvas,
        {'stretch': 'ScaleToFit', 'stretchDirection': 'Both', 'visibility': 'HitTestInvisible'},
        canvas_slot(20, 21, 52, 52, 3))
    add(FISTS, 'Icon', '/Script/UMG.Image', scale,
        {'brush': {'drawAs': 'Image', 'tintColor': slate(color('FFFFFF')), 'imageSize': {'x': 52, 'y': 52}},
         'colorAndOpacity': color('EEEAE0'), 'visibility': 'Collapsed'},
        {'horizontalAlignment': 'HAlign_Center', 'verticalAlignment': 'VAlign_Center'}, True)
    # Empty authored text is populated from the intrinsic slot's native view,
    # including the remapped key. No item quantity or fabricated count is present.
    add_text(FISTS, 'KeyText', canvas, (60, 5, 24, 18), 15, '', 'Right', color('B9B9B9'))
    add_text(FISTS, 'NameText', canvas, (6, 74, 80, 20), 16, '', 'Center')


def resize_center():
    center = widgets(tree(ROOT))['CenterSlot']
    properties(center['widget'], {'widthOverride': 92, 'heightOverride': 96,
                                 'bOverride_WidthOverride': True, 'bOverride_HeightOverride': True})
    properties(center['slot'], {'layoutData': {'offsets': {'left': 226, 'top': 243, 'right': 92, 'bottom': 96}}})


def verify_fists():
    data = tree(FISTS)
    require(data['info']['parentClass'] == ref(ENTRY_PARENT), 'Fists widget parent differs')
    rows = widgets(data)
    expected = {'EntryCanvas': '/Script/UMG.CanvasPanel', 'HighlightBorder': '/Script/UMG.Border',
                'CardFill': '/Script/UMG.Image', 'IconScale': '/Script/UMG.ScaleBox',
                'Icon': '/Script/UMG.Image', 'KeyText': '/Script/UMG.TextBlock', 'NameText': '/Script/UMG.TextBlock'}
    require(set(rows) == set(expected), 'Fists tree differs from the compact icon/key/name composition')
    for name, kind in expected.items():
        row = widget(rows, name, kind)
        if name in ('HighlightBorder', 'Icon', 'KeyText', 'NameText'):
            require(row['bIsVariable'], 'Fists BindWidget is not exposed: ' + name)
    require(properties(rows['IconScale']['slot'], names=SLOT_PROPS) == canvas_slot(20, 21, 52, 52, 3),
            'Fists icon region differs')
    require(properties(rows['NameText']['slot'], names=SLOT_PROPS) == canvas_slot(6, 74, 80, 20, 5),
            'Fists name position differs')
    for name in ('NameText', 'KeyText'):
        require(properties(rows[name]['widget'], names=['text'])['text'] == '', 'Fists preview contains runtime data')
    return data


def verify():
    report = audit()
    rows = widgets(report['entry'])
    for name, kind in (('EmptyMarkText', '/Script/UMG.TextBlock'), ('ItemDetails', DETAILS_CLASS)):
        row = widget(rows, name, kind)
        require(row['bIsVariable'], 'New BindWidget is not exposed: ' + name)
    require(properties(rows['EmptyMarkText']['slot'], names=SLOT_PROPS) == canvas_slot(15, 40, 138, 40, 5),
            'Empty-slot dash is not centered in the card')
    require(properties(rows['EmptyMarkText']['widget'], names=['text'])['text'] == '\u2014', 'Empty-slot mark differs')
    require(properties(rows['ItemDetails']['slot'], names=SLOT_PROPS) == stretch_slot(z=4), 'ItemDetails does not fill its authored card')
    require(properties(rows['NameText']['slot'], names=SLOT_PROPS) == canvas_slot(12, 8, 116, 22, 4), 'Fallback item name is not in the top row')
    require(properties(rows['CategoryText']['widget'], names=['text', 'visibility']) == {'text': '', 'visibility': 'Collapsed'},
            'A fixed category heading remains visible')
    require(report['center']['slot']['layoutData']['offsets'] == {'left': 226, 'top': 243, 'right': 92, 'bottom': 96},
            'Compact Fists host moved away from the original cross center')
    require(report['center']['size'] == {'widthOverride': 92, 'heightOverride': 96,
            'bOverride_WidthOverride': True, 'bOverride_HeightOverride': True}, 'Compact Fists host size differs')
    verify_fists()
    return report


def run():
    require(MODE in ('audit', 'author', 'verify'), 'Unknown MODE: ' + str(MODE))
    if MODE == 'audit':
        return audit()
    if MODE == 'verify':
        result = verify()
        call(ASSET, 'write_file', file_path=OUTPUT + '/v2-widgets-verify.json', content=json.dumps(result, indent=2))
        return result
    require(BACKUP_READY, 'Capture explicit entry/root/Fists backups before authoring')
    require(not call(EDITOR, 'IsPIERunning'), 'Stop PIE before widget authoring')
    before = audit()
    preflight(before)
    call(ASSET, 'write_file', file_path=OUTPUT + '/v2-widgets-before.json', content=json.dumps(before, indent=2))
    author_fists()
    author_physical()
    resize_center()
    after = verify()
    require(after['outer_hosts'] == before['outer_hosts'], 'An outer cross host changed')
    result = {'status': 'authored_requires_compile_assignment_and_save', 'readback': after,
              'compile_after_return': [FISTS, ENTRY, ROOT], 'explicit_save_assets': [FISTS, ENTRY, ROOT],
              'native_defaults_after_compile': {'root_center_entry_widget_class': FISTS + '_C',
                    'entry_unavailable_opacity': .75,
                    'fists_style': 'Copy physical-entry Idle/Hovered/Editing/Equipped colors; no quantity widget'},
              'scope': 'Existing physical entry + center host; new compact Fists only; outer four hosts preserved'}
    call(ASSET, 'write_file', file_path=OUTPUT + '/v2-widgets-author.json', content=json.dumps(result, indent=2))
    return result
