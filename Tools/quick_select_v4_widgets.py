"""Shrink the existing cross and make mode 0 one of its eight visible cells.

ProgrammaticToolset.execute_tool_script supplies execute_tool and invokes run.
MODE='audit' (or 'read') is read-only. Authoring requires explicit root/mode
widget backups, no PIE, the actual Explore texture, and the root-owned native
body changes: slot 0 uses CenterEntryWidgetClass in a directional host, while
Entry.ApplyEntryView toggles the named ExploreActionIcon against Icon.

Set MODE='author', BACKUP_READY=True after the verified Explore texture has
been imported. Compile and save the two returned Widget Blueprints with
dedicated tools after this script returns, then run MODE='verify'. No graph,
reflection, input, item, map, source pack, physical entry, or inventory changes
are made here. Importing this file performs no tool calls.
"""
import copy
import json


MODE = 'audit'
BACKUP_READY = False
# Root-authored GIMP artwork; preflight requires the import to exist.
EXPLORE_TEXTURE = '/Game/AZ/Blueprints/Menu/HUD/Art/T_HUD_Explore.T_HUD_Explore'
ROOT = '/Game/AZ/Blueprints/Menu/HUD/QuickSelect/WBP_AZ_QuickSelect.WBP_AZ_QuickSelect'
MODE_WIDGET = '/Game/AZ/Blueprints/Menu/HUD/QuickSelect/WBP_AZ_QuickSelectFists.WBP_AZ_QuickSelectFists'
UMG = 'UMGToolSet.UMGToolSet.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
ASSET = 'editor_toolset.toolsets.asset.AssetTools.'
EDITOR = 'EditorToolset.EditorAppToolset.'
OUTPUT = 'C:/UnrealEngine/Games/AZ/Saved/QuickSelect'
SCALE = .70
PIVOT = {'x': .5, 'y': 240 / 664}
HOST_NAMES = ('LeftSlot', 'LeftSlotSecond', 'UpSlot', 'UpSlotSecond',
              'RightSlot', 'RightSlotSecond', 'DownSlot', 'DownSlotSecond')
SLOT_PROPS = ['layoutData', 'bAutoSize', 'zOrder']


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def ref(path):
    return {'refPath': path}


def call(prefix, name, **kwargs):
    result = execute_tool(prefix + name, json.dumps(kwargs))
    require(isinstance(result, dict), 'Tool failed: ' + name)
    return result.get('returnValue')


def merge(before, changes):
    result = copy.deepcopy(before)
    for key, value in changes.items():
        result[key] = merge(result[key], value) if isinstance(value, dict) and isinstance(result.get(key), dict) else copy.deepcopy(value)
    return result


def equal(actual, expected):
    if isinstance(expected, dict):
        return isinstance(actual, dict) and set(actual) == set(expected) and all(equal(actual[k], v) for k, v in expected.items())
    if isinstance(expected, (int, float)) and not isinstance(expected, bool):
        return isinstance(actual, (int, float)) and abs(actual - expected) < .0001
    return actual == expected


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
            'Property write failed on ' + str(instance))
    return json.loads(call(OBJ, 'get_properties', instance=instance, properties=selected))


def tree(path):
    return call(UMG, 'GetWidgets', widgetBlueprint=ref(path))


def widgets(data):
    return {row['widgetName']: row for row in data['widgets'] if isinstance(row.get('widget'), dict)}


def widget(rows, name, kind=None):
    require(name in rows, 'Missing widget: ' + name)
    row = rows[name]
    if kind:
        require(row['widgetClassPath'] == ref(kind), 'Unexpected widget class: ' + name)
    return row


def canvas_slot(x, y, width, height, z=0):
    return {'layoutData': {'offsets': {'left': x, 'top': y, 'right': width, 'bottom': height},
            'anchors': {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 0, 'y': 0}},
            'alignment': {'x': 0, 'y': 0}}, 'bAutoSize': False, 'zOrder': z}


def centered_slot(with_padding=True):
    result = {'horizontalAlignment': 'HAlign_Center', 'verticalAlignment': 'VAlign_Center'}
    if with_padding:
        result['padding'] = {'left': 0, 'top': 0, 'right': 0, 'bottom': 0}
    return result


def color(rgb, alpha=1.0):
    values = [int(rgb[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    values = [v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4 for v in values]
    return dict(zip(('r', 'g', 'b', 'a'), values + [alpha]))


def slate(value):
    return {'specifiedColor': value, 'colorUseRule': 'UseColor_Specified'}


def geometry_snapshot(rows):
    # Compare every root CanvasPanelSlot; resizing uses RenderTransform only.
    snapshot = {}
    for name, row in rows.items():
        if isinstance(row.get('slot'), dict):
            schema = properties(row['slot'])
            if 'layoutData' in schema:
                snapshot[name] = properties(row['slot'], names=SLOT_PROPS)
    return snapshot


def audit():
    root_tree, mode_tree = tree(ROOT), tree(MODE_WIDGET)
    require(root_tree['info']['parentClass'] == ref('/Script/AZ.AZ_QuickSelectWidget'), 'Unexpected selector native parent')
    require(mode_tree['info']['parentClass'] == ref('/Script/AZ.AZ_QuickSelectEntryWidget'), 'Unexpected mode-card native parent')
    rows, mode_rows = widgets(root_tree), widgets(mode_tree)
    cross = widget(rows, 'CrossLayout', '/Script/UMG.CanvasPanel')
    for name in HOST_NAMES:
        widget(rows, name, '/Script/UMG.SizeBox')
        widget(rows, name + 'Scale', '/Script/UMG.ScaleBox')
    widget(rows, 'CenterSlot', '/Script/UMG.SizeBox')
    for name in ('ModeText', 'HeaderText'):
        widget(rows, name, '/Script/UMG.TextBlock')
    for name, kind in (('EntryCanvas', '/Script/UMG.CanvasPanel'), ('IconScale', '/Script/UMG.ScaleBox'),
                       ('Icon', '/Script/UMG.Image'), ('NameText', '/Script/UMG.TextBlock'),
                       ('KeyText', '/Script/UMG.TextBlock')):
        widget(mode_rows, name, kind)
    require(not any(name in mode_rows for name in ('AmmoText', 'CategoryText', 'ItemDetails')),
            'Mode card must remain icon/target-name/key only, without physical item composition or quantities')
    return {'root': root_tree, 'mode': mode_tree, 'root_canvas_geometry': geometry_snapshot(rows),
            'cross_transform': properties(cross['widget'], names=['renderTransform', 'renderTransformPivot']),
            'required_explore_texture': EXPLORE_TEXTURE,
            'rendered_bounds': [552 * SCALE, 664 * SCALE],
            'rendered_cell': [116 * SCALE, 96 * SCALE]}


def author_root(rows):
    properties(rows['CrossLayout']['widget'], {'renderTransform': {'scale': {'x': SCALE, 'y': SCALE}},
                                               'renderTransformPivot': PIVOT})
    for name in ('CenterSlot', 'ModeText', 'HeaderText'):
        properties(rows[name]['widget'], {'visibility': 'Collapsed'})


def mode_overlay():
    rows = widgets(tree(MODE_WIDGET))
    icon = rows['Icon']
    scale = rows['IconScale']
    if 'ModeIconOverlay' in rows:
        overlay = widget(rows, 'ModeIconOverlay', '/Script/UMG.Overlay')
        require(overlay['parent'] == scale['widget'] and icon['parent'] == overlay['widget'],
                'Mode icon overlay hierarchy has unreviewed changes')
        return overlay
    require(icon['parent'] == scale['widget'], 'Unexpected original mode icon parent')
    wrapped = call(UMG, 'WrapWidgets', widgetBlueprint=ref(MODE_WIDGET), widgets=[icon['widget']],
                   wrapperClass=ref('/Script/UMG.Overlay'))
    require(isinstance(wrapped, list) and len(wrapped) == 1, 'Could not wrap the mode icon')
    overlay = call(UMG, 'RenameWidget', widgetBlueprint=ref(MODE_WIDGET), widget=wrapped[0]['widget'],
                   newDisplayName='ModeIconOverlay')
    require(overlay and isinstance(overlay.get('widget'), dict), 'Could not name mode icon overlay')
    return overlay


def author_mode():
    overlay = mode_overlay()
    properties(overlay['widget'], {'visibility': 'SelfHitTestInvisible'})
    properties(overlay['slot'], centered_slot(False))  # ScaleBoxSlot has no editable padding.
    rows = widgets(tree(MODE_WIDGET))
    properties(rows['IconScale']['slot'], canvas_slot(58, 28, 52, 52, 3))
    properties(rows['IconScale']['widget'], {'stretch': 'ScaleToFit', 'stretchDirection': 'Both',
                                           'visibility': 'HitTestInvisible'})
    properties(rows['Icon']['widget'], {'brush': {'imageSize': {'x': 128, 'y': 128}}})
    properties(rows['Icon']['slot'], centered_slot())
    image_props = {'brush': {'drawAs': 'Image', 'resourceObject': ref(EXPLORE_TEXTURE),
                            'imageSize': {'x': 128, 'y': 128}, 'tintColor': slate(color('FFFFFF'))},
                   'colorAndOpacity': color('EEEAE0'), 'visibility': 'Collapsed'}
    if 'ExploreActionIcon' not in rows:
        image = call(UMG, 'AddWidget', widgetBlueprint=ref(MODE_WIDGET), widgetClass=ref('/Script/UMG.Image'),
                     widgetDisplayName='ExploreActionIcon', parentWidget=overlay['widget'])
        require(image and isinstance(image.get('widget'), dict), 'Could not add Explore action image')
        if image['widgetName'] != 'ExploreActionIcon':
            image = call(UMG, 'RenameWidget', widgetBlueprint=ref(MODE_WIDGET), widget=image['widget'],
                         newDisplayName='ExploreActionIcon')
    else:
        image = widget(rows, 'ExploreActionIcon', '/Script/UMG.Image')
        require(image['parent'] == overlay['widget'], 'Explore action image must share the mode overlay')
    properties(image['widget'], image_props)
    properties(image['slot'], centered_slot())
    # GetWidgetFromName is a native lookup; no new C++ reflection or Blueprint graph is required.
    call(UMG, 'ToggleWidgetAsVariable', widgetBlueprint=ref(MODE_WIDGET), widget=image['widget'], bIsVariable=False)
    properties(rows['NameText']['slot'], canvas_slot(12, 86, 144, 28, 5))
    properties(rows['NameText']['widget'], {'font': {'size': 22 * .75}, 'justification': 'Center', 'text': ''})
    properties(rows['KeyText']['slot'], canvas_slot(130, 8, 26, 24, 5))
    properties(rows['KeyText']['widget'], {'font': {'size': 22 * .75}, 'justification': 'Right', 'text': ''})
    # Existing stretch-to-parent background/outline automatically fill the new
    # 168x128 physical host; they do not need a new texture or another card class.


def verify():
    report = audit()
    rows, mode_rows = widgets(report['root']), widgets(report['mode'])
    require(equal(report['cross_transform']['renderTransform']['scale'], {'x': SCALE, 'y': SCALE})
            and equal(report['cross_transform']['renderTransformPivot'], PIVOT), 'Compact root scale/pivot differs')
    for name in ('CenterSlot', 'ModeText', 'HeaderText'):
        require(properties(rows[name]['widget'], names=['visibility'])['visibility'] == 'Collapsed', 'Unused center UI remains visible: ' + name)
    for name in HOST_NAMES:
        require(rows[name]['parent'] == rows[name + 'Scale']['widget'], 'Physical host scale hierarchy changed: ' + name)
        require(properties(rows[name]['widget'], names=['widthOverride', 'heightOverride']) == {'widthOverride': 168, 'heightOverride': 128},
                'Physical card design size changed: ' + name)
    overlay = widget(mode_rows, 'ModeIconOverlay', '/Script/UMG.Overlay')
    require(overlay['parent'] == mode_rows['IconScale']['widget'], 'Mode icon overlay parent differs')
    require(properties(overlay['slot'], names=['horizontalAlignment', 'verticalAlignment']) == centered_slot(False),
            'Mode overlay alignment differs')
    for name in ('Icon', 'ExploreActionIcon'):
        image = widget(mode_rows, name, '/Script/UMG.Image')
        require(image['parent'] == overlay['widget'], 'Mode image is not in the shared overlay')
        require(equal(properties(image['slot'], names=['horizontalAlignment', 'verticalAlignment', 'padding']), centered_slot()), 'Mode image alignment differs')
        brush = properties(image['widget'], names=['brush'])['brush']
        require(equal(brush['imageSize'], {'x': 128, 'y': 128}), 'Mode image natural size differs')
        if name == 'ExploreActionIcon':
            require(brush['resourceObject'] == ref(EXPLORE_TEXTURE), 'Mode action is using the wrong Explore artwork')
    require(equal(properties(mode_rows['IconScale']['slot'], names=SLOT_PROPS), canvas_slot(58, 28, 52, 52, 3)), 'Mode icon box differs')
    require(equal(properties(mode_rows['NameText']['slot'], names=SLOT_PROPS), canvas_slot(12, 86, 144, 28, 5)), 'Mode target label box differs')
    require(equal(properties(mode_rows['KeyText']['slot'], names=SLOT_PROPS), canvas_slot(130, 8, 26, 24, 5)), 'Mode key box differs')
    for name in ('NameText', 'KeyText'):
        require(properties(mode_rows[name]['widget'], names=['text'])['text'] == '', 'Mode preview contains a fixed runtime label')
    report['status'] = 'v4_widget_readback_passed'
    return report


def run():
    require(MODE in ('audit', 'read', 'author', 'verify'), 'Unknown MODE: ' + str(MODE))
    if MODE in ('audit', 'read'):
        return audit()
    require(isinstance(EXPLORE_TEXTURE, str) and EXPLORE_TEXTURE.startswith('/Game/'),
            'Supply the verified Explore texture object path')
    if MODE == 'verify':
        result = verify()
        call(ASSET, 'write_file', file_path=OUTPUT + '/v4-widgets-verify.json', content=json.dumps(result, indent=2))
        return result
    require(BACKUP_READY, 'Back up the root and mode widget packages first')
    require(not call(EDITOR, 'IsPIERunning'), 'Stop PIE before authoring the widgets')
    require(call(ASSET, 'exists', path=EXPLORE_TEXTURE), 'Explore artwork has not been imported')
    before = audit()
    call(ASSET, 'write_file', file_path=OUTPUT + '/v4-widgets-before.json', content=json.dumps(before, indent=2))
    author_root(widgets(before['root']))
    author_mode()
    after = verify()
    require(after['root_canvas_geometry'] == before['root_canvas_geometry'], 'V3 root slot geometry was changed instead of uniformly scaled')
    result = {'status': 'authored_requires_compile_and_save', 'readback': after,
              'compile_after_return': [MODE_WIDGET, ROOT], 'explicit_save_assets': [MODE_WIDGET, ROOT],
              'requires_native_body_hook': 'ExploreActionIcon visible only when slot 0 is currently in Fight; Icon visible otherwise',
              'requires_slot_data': 'Exactly 8 definitions: 0/1 Left, 2/3 Up, 4/5 Right, 6/7 Down; 0 mode is intrinsic',
              'rendered_bounds': [386.4, 464.8], 'rendered_cell': [81.2, 67.2],
              'scope': 'Root render scale/unused visibility and mode icon composition only; no Blueprint graph changes'}
    call(ASSET, 'write_file', file_path=OUTPUT + '/v4-widgets-author.json', content=json.dumps(result, indent=2))
    return result
