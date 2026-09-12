"""Build only Quick Select's real inventory-composite presentation widgets.

Run via ProgrammaticToolset.execute_tool_script (injects execute_tool). Audit
is the default. After the caller's backup and native build/restart:
  1. MODE='leaves', BACKUP_READY=True; compile the two returned leaf BPs outside
     this script using the dedicated BlueprintTools compile operation.
  2. MODE='details'; compile the returned ItemDetails BP outside this script.
  3. MODE='verify'; only then attach ItemDetails to the physical selector entry
     through the separate quick_select_v2_widgets.py authoring step.

No compilation, package saves, tests, PIE, native code, old HUD/inventory/widget
assets, mappings, controller defaults, or item manifests are changed here.
Existing/nonempty new asset trees are never cleared or silently rewritten.
All drawing and text are designer-editable UMG. These are actual project
CompositeWidget/TextLeaf/ImageLeaf classes using exact FragmentTag matching.
No synthetic item objects or placeholder gameplay values are created.
"""
import copy
import json


MODE = 'audit'
ASSET = 'editor_toolset.toolsets.asset.AssetTools.'
BACKUP_READY = False
UMG = 'UMGToolSet.UMGToolSet.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
FOLDER = '/Game/AZ/Blueprints/Menu/HUD/QuickSelect'
NAME = FOLDER + '/WBP_AZ_QuickSelectNameLeaf.WBP_AZ_QuickSelectNameLeaf'
IMAGE = FOLDER + '/WBP_AZ_QuickSelectImageLeaf.WBP_AZ_QuickSelectImageLeaf'
DETAILS = FOLDER + '/WBP_AZ_QuickSelectItemDetails.WBP_AZ_QuickSelectItemDetails'
NAME_PARENT = '/Script/AZ.AZ_Inv_CommonUI_LeafWidget_Text'
IMAGE_PARENT = '/Script/AZ.AZ_Inv_CommonUI_LeafWidget_Image'
DETAILS_PARENT = '/Script/AZ.AZ_Inv_CommonUI_CompositeWidget'
NAME_TAG = 'Item.Fragment.Name.StaticText'
IMAGE_TAG = 'Item.Fragment.Icon'
FONT = '/Game/InventorySystemPro/ExampleContent/Common/Art/Fonts/Oswald-Light_Font.Oswald-Light_Font'
SOURCE_NAME = '/Game/AZ/Blueprints/Menu/CommonInventory/CommonUI/UI/Widgets/ItemDescription/EditInstanceOnly/WBP_Inv_CommonUI_Leaf_Text.WBP_Inv_CommonUI_Leaf_Text'
SOURCE_IMAGE = '/Game/AZ/Blueprints/Menu/CommonInventory/CommonUI/UI/Widgets/ItemDescription/WBP_Inv_CommonUI_Leaf_Icon.WBP_Inv_CommonUI_Leaf_Icon'


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def ref(path):
    return {'refPath': path}


def call(prefix, name, **kwargs):
    result = execute_tool(prefix + name, json.dumps(kwargs))
    require(isinstance(result, dict) and 'returnValue' in result,
            'Tool failed: ' + name + ' ' + str(result))
    return result['returnValue']


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
    require(not set(selected) - set(schema), 'Unknown properties: ' + str(set(selected) - set(schema)))
    before = json.loads(call(OBJ, 'get_properties', instance=instance, properties=selected))
    if changes is None:
        return before
    require(call(OBJ, 'set_properties', instance=instance, values=json.dumps(merge(before, changes))),
            'Property setter failed: ' + str(instance))
    return json.loads(call(OBJ, 'get_properties', instance=instance, properties=selected))


def tree(path):
    return call(UMG, 'GetWidgets', widgetBlueprint=ref(path))


def widgets(path):
    return {row['widgetName']: row for row in tree(path)['widgets'] if isinstance(row.get('widget'), dict)}


def color(rgb, alpha=1):
    values = [int(rgb[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    values = [x / 12.92 if x <= .04045 else ((x + .055) / 1.055) ** 2.4 for x in values]
    return dict(zip(('r', 'g', 'b', 'a'), values + [alpha]))


def slate(tint):
    return {'specifiedColor': tint, 'colorUseRule': 'UseColor_Specified'}


def canvas_slot(x, y, w, h):
    return {'layoutData': {'offsets': {'left': x, 'top': y, 'right': w, 'bottom': h},
            'anchors': {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 0, 'y': 0}},
            'alignment': {'x': 0, 'y': 0}}, 'bAutoSize': False, 'zOrder': 0}


def add(path, name, cls, parent=None, props=None, slot=None, variable=False):
    args = {'widgetBlueprint': ref(path), 'widgetClass': ref(cls), 'widgetDisplayName': name}
    if parent:
        args['parentWidget'] = parent
    row = call(UMG, 'AddWidget', **args)
    require(row and isinstance(row.get('widget'), dict), 'Cannot add ' + name)
    if row['widgetName'] != name:
        row = call(UMG, 'RenameWidget', widgetBlueprint=ref(path), widget=row['widget'], newDisplayName=name)
    properties(row['widget'], props or {'visibility': 'HitTestInvisible'})
    if isinstance(row.get('slot'), dict):
        properties(row['slot'], slot) if slot else properties(row['slot'])
    call(UMG, 'ToggleWidgetAsVariable', widgetBlueprint=ref(path), widget=row['widget'], bIsVariable=variable)
    return row['widget']


def begin(path, native_parent):
    name = path.rsplit('.', 1)[1]
    result = call(UMG, 'CreateWidgetBlueprint', folderPath=FOLDER, assetName=name, parentClass=ref(native_parent))
    require(tree(path)['info']['parentClass'] == ref(native_parent), 'Wrong native parent: ' + path)
    require(not widgets(path), 'Asset already has a widget tree; inspect/resume explicitly: ' + path)
    return result


def author_name():
    begin(NAME, NAME_PARENT)
    add(NAME, 'Text_LeafText', '/Script/CommonUI.CommonTextBlock', props={
        'text': '', 'font': {'fontObject': ref(FONT), 'typefaceFontName': 'Default',
            'size': 13, 'letterSpacing': 0, 'skewAmount': 0, 'outlineSettings': {'outlineSize': 0}},
        'colorAndOpacity': slate(color('EEEAE0')),
        'shadowColorAndOpacity': color('000000', .72), 'shadowOffset': {'x': 1, 'y': 1},
        'justification': 'Left', 'textOverflowPolicy': 'Ellipsis',
        'autoWrapText': False, 'visibility': 'HitTestInvisible'}, variable=True)
    return {'asset': NAME, 'tree': tree(NAME), 'compile_after_return': [NAME]}


def author_image():
    begin(IMAGE, IMAGE_PARENT)
    size = add(IMAGE, 'SizeBox_Icon', '/Script/UMG.SizeBox', props={
        'bOverride_WidthOverride': True, 'widthOverride': 100,
        'bOverride_HeightOverride': True, 'heightOverride': 100,
        'visibility': 'HitTestInvisible'}, variable=True)
    add(IMAGE, 'Image_Icon', '/Script/UMG.Image', size, props={
        'brush': {'drawAs': 'Image', 'imageSize': {'x': 100, 'y': 100},
                  'tintColor': slate(color('FFFFFF'))},
        'colorAndOpacity': color('EEEAE0'), 'visibility': 'HitTestInvisible'},
        slot={'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Fill'}, variable=True)
    return {'asset': IMAGE, 'tree': tree(IMAGE), 'compile_after_return': [IMAGE]}


def verify_leaf(path, parent, expected):
    current = tree(path)
    require(current['info']['parentClass'] == ref(parent), 'Wrong native leaf class: ' + path)
    rows = widgets(path)
    require(set(rows) == set(expected), 'Unexpected/missing native leaf bindings: ' + path)
    for name, cls in expected.items():
        require(rows[name]['widgetClassPath'] == ref(cls), 'Wrong binding type: ' + name)
        require(rows[name]['bIsVariable'] or rows[name]['bInherited'], 'Required binding is not exposed: ' + name)
    return current


def verify_leaves():
    name = verify_leaf(NAME, NAME_PARENT, {'Text_LeafText': '/Script/CommonUI.CommonTextBlock'})
    image = verify_leaf(IMAGE, IMAGE_PARENT, {'SizeBox_Icon': '/Script/UMG.SizeBox', 'Image_Icon': '/Script/UMG.Image'})
    text = properties(widgets(NAME)['Text_LeafText']['widget'], names=['text'])
    require(text['text'] == '', 'The quick-select name leaf has an authored sample label')
    return {'name': name, 'image': image}


def author_details():
    verify_leaves()
    # Inspect the compiled generated-class CDOs before creating a parent that
    # instantiates these classes. Failure here means compile the leaves first.
    for path in (NAME, IMAGE):
        package, name = path.rsplit('.', 1)
        properties(ref(package + '.Default__' + name + '_C'), names=['fragmentTag'])
    if not call(ASSET, 'exists', path=DETAILS):
        begin(DETAILS, DETAILS_PARENT)
    require(tree(DETAILS)['info']['parentClass'] == ref(DETAILS_PARENT), 'Unexpected details parent')
    existing = widgets(DETAILS)
    require(set(existing).issubset({'ItemDetailsCanvas', 'ItemName', 'IconScale', 'ItemImage'}), 'Unexpected details widgets')
    canvas = existing['ItemDetailsCanvas']['widget'] if 'ItemDetailsCanvas' in existing else add(
        DETAILS, 'ItemDetailsCanvas', '/Script/UMG.CanvasPanel', props={'visibility': 'HitTestInvisible'})
    # EditInstanceOnly fragment tags cannot be edited on WidgetTree templates.
    # Entry's native setter assigns these exact name/icon roles before assimilation.
    if 'ItemName' not in existing:
        add(DETAILS, 'ItemName', NAME + '_C', canvas,
            props={'visibility': 'HitTestInvisible', 'bIsFocusable': False},
            slot=canvas_slot(12, 8, 116, 22), variable=True)
    else:
        properties(existing['ItemName']['widget'], {'visibility': 'HitTestInvisible', 'bIsFocusable': False})
        properties(existing['ItemName']['slot'], canvas_slot(12, 8, 116, 22))
        call(UMG, 'ToggleWidgetAsVariable', widgetBlueprint=ref(DETAILS), widget=existing['ItemName']['widget'], bIsVariable=True)
    scale = existing['IconScale']['widget'] if 'IconScale' in existing else add(
        DETAILS, 'IconScale', '/Script/UMG.ScaleBox', canvas,
        props={'stretch': 'ScaleToFit', 'stretchDirection': 'Both', 'visibility': 'HitTestInvisible'},
        slot=canvas_slot(15, 28, 138, 60))
    if 'ItemImage' not in existing:
        add(DETAILS, 'ItemImage', IMAGE + '_C', scale,
            props={'visibility': 'HitTestInvisible', 'bIsFocusable': False},
            slot={'horizontalAlignment': 'HAlign_Center', 'verticalAlignment': 'VAlign_Center'}, variable=True)
    return {'asset': DETAILS, 'tree': tree(DETAILS), 'compile_after_return': [DETAILS],
            'runtime_contract': 'ItemDetails->Collapse(); assign actual name tag; Manifest.AssimilateInventoryFragments(ItemDetails)'}


def verify():
    leaves = verify_leaves()
    current = tree(DETAILS)
    require(current['info']['parentClass'] == ref(DETAILS_PARENT), 'ItemDetails is not the real inventory composite')
    rows = widgets(DETAILS)
    require(set(rows) == {'ItemDetailsCanvas', 'ItemName', 'IconScale', 'ItemImage'}, 'Unexpected ItemDetails tree')
    require(rows['ItemName']['widgetClassPath']['refPath'] in (NAME, NAME + '_C')
            and rows['ItemImage']['widgetClassPath']['refPath'] in (IMAGE, IMAGE + '_C'), 'Wrong composite leaf classes')
    # The native entry assigns exact roles before calling inventory assimilation.
    require(properties(rows['ItemName']['widget'], names=['previewText'])['previewText'] == '', 'Sample name must stay empty')
    for name, box in (('ItemName', (12, 8, 116, 22)), ('IconScale', (15, 28, 138, 60))):
        require(properties(rows[name]['slot'], names=['layoutData', 'bAutoSize', 'zOrder']) == canvas_slot(*box),
                'Compact item details geometry differs: ' + name)
    return {'leaves': leaves, 'details': current, 'compiled_class': DETAILS + '_C',
            'item_name_instance': rows['ItemName']['widget'], 'item_image_instance': rows['ItemImage']['widget'],
            'scope': 'Three new QuickSelect presentation BPs only; inventory assets unchanged'}


def run():
    if MODE == 'audit':
        return {'source_name_leaf': tree(SOURCE_NAME), 'source_image_leaf': tree(SOURCE_IMAGE),
                'planned_assets': [NAME, IMAGE, DETAILS], 'native_parents': [NAME_PARENT, IMAGE_PARENT, DETAILS_PARENT],
                'required_tags': [NAME_TAG, IMAGE_TAG], 'no_mutations': True}
    if MODE == 'verify':
        return verify()
    if MODE == 'verify_leaves':
        return verify_leaves()
    require(BACKUP_READY, 'Caller must complete its Quick Select backup before authoring')
    if MODE == 'name_leaf':
        return author_name()
    if MODE == 'image_leaf':
        return author_image()
    if MODE == 'leaves':
        return {'name': author_name(), 'image': author_image(), 'compile_after_return': [NAME, IMAGE],
                'next': 'Compile both leaves through dedicated tools, then MODE=details'}
    if MODE == 'details':
        return author_details()
    raise RuntimeError('Unknown mode: ' + MODE)
