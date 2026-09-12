"""Author the bottom Quick Select focus-name/description inventory composite.

ProgrammaticToolset script; execute_tool is injected. MODE='audit' is read-only.
After caller backup and full native build/restart: MODE='leaves', external
dedicated compilation, MODE='details', external compilation, MODE='verify'.
The separate root authoring script then adds WBP_AZ_QuickSelectFocusDetails_C
as its FocusDetails child at (0,492,552,100).

Only three NEW assets below QuickSelect are created. Existing card/inventory
leaves, fonts, HUD, bindings, input, item data, and maps remain untouched. No
PIE, tests, compilation, saving or gameplay actions run here. Runtime fills
these real native inventory text leaves from actual item fragments and sets
their exact FragmentTags; no EditInstanceOnly defaults/tag writes are used.
"""
import copy
import json

MODE = 'audit'
BACKUP_READY = False
UMG = 'UMGToolSet.UMGToolSet.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
FOLDER = '/Game/AZ/Blueprints/Menu/HUD/QuickSelect'
NAME = FOLDER + '/WBP_AZ_QuickSelectFocusNameLeaf.WBP_AZ_QuickSelectFocusNameLeaf'
DESCRIPTION = FOLDER + '/WBP_AZ_QuickSelectDescriptionLeaf.WBP_AZ_QuickSelectDescriptionLeaf'
DETAILS = FOLDER + '/WBP_AZ_QuickSelectFocusDetails.WBP_AZ_QuickSelectFocusDetails'
TEXT_PARENT = '/Script/AZ.AZ_Inv_CommonUI_LeafWidget_Text'
COMPOSITE_PARENT = '/Script/AZ.AZ_Inv_CommonUI_CompositeWidget'
FONT = '/Game/InventorySystemPro/ExampleContent/Common/Art/Fonts/Oswald-Light_Font.Oswald-Light_Font'
BODY = '/Engine/EngineFonts/Roboto.Roboto'
SOURCE_NAME = FOLDER + '/WBP_AZ_QuickSelectNameLeaf.WBP_AZ_QuickSelectNameLeaf'


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def ref(path):
    return {'refPath': path}


def call(prefix, name, **kwargs):
    result = execute_tool(prefix + name, json.dumps(kwargs))
    require(isinstance(result, dict) and 'returnValue' in result, 'Tool failed: ' + name + ' ' + str(result))
    return result['returnValue']


def merge(before, changes):
    result = copy.deepcopy(before)
    for name, value in changes.items():
        result[name] = merge(result[name], value) if isinstance(value, dict) and isinstance(result.get(name), dict) else copy.deepcopy(value)
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
            'Property write failed: ' + str(instance))
    return json.loads(call(OBJ, 'get_properties', instance=instance, properties=selected))


def tree(path):
    return call(UMG, 'GetWidgets', widgetBlueprint=ref(path))


def widgets(path):
    return {r['widgetName']: r for r in tree(path)['widgets'] if isinstance(r.get('widget'), dict)}


def color(rgb, alpha=1):
    values = [int(rgb[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    values = [v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4 for v in values]
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


def begin(path, parent):
    call(UMG, 'CreateWidgetBlueprint', folderPath=FOLDER, assetName=path.rsplit('.', 1)[1], parentClass=ref(parent))
    require(tree(path)['info']['parentClass'] == ref(parent), 'Wrong native parent: ' + path)
    require(not widgets(path), 'Existing nonempty asset must be inspected, never overwritten: ' + path)


def author_leaf(path, font, size, tint, wrap):
    begin(path, TEXT_PARENT)
    add(path, 'Text_LeafText', '/Script/CommonUI.CommonTextBlock', props={
        'text': '', 'font': {'fontObject': ref(font), 'typefaceFontName': 'Default' if font == FONT else 'Regular',
            'size': size, 'letterSpacing': 0, 'skewAmount': 0, 'outlineSettings': {'outlineSize': 0}},
        'colorAndOpacity': slate(color(tint)), 'shadowColorAndOpacity': color('000000', .72),
        'shadowOffset': {'x': 1, 'y': 1}, 'justification': 'Center',
        'autoWrapText': wrap, 'textOverflowPolicy': 'Ellipsis', 'visibility': 'HitTestInvisible'}, variable=True)
    return {'asset': path, 'tree': tree(path), 'compile_after_return': [path]}


def verify_leaves():
    result = {}
    for path in (NAME, DESCRIPTION):
        require(tree(path)['info']['parentClass'] == ref(TEXT_PARENT), 'Expected real inventory text leaf')
        rows = widgets(path)
        require(set(rows) == {'Text_LeafText'} and
                rows['Text_LeafText']['widgetClassPath'] == ref('/Script/CommonUI.CommonTextBlock'),
                'Required CommonTextBlock binding differs')
        values = properties(rows['Text_LeafText']['widget'], names=['text', 'justification', 'autoWrapText'])
        require(values['text'] == '' and values['justification'] == 'Center'
                and values['autoWrapText'] == (path == DESCRIPTION), 'Focused text defaults differ')
        result[path] = tree(path)
    return result


def author_details():
    verify_leaves()
    # The CDO lookup proves both generated leaf classes are present; the caller
    # compiles them before this step. Read only, no EditInstanceOnly tag writes.
    for path in (NAME, DESCRIPTION):
        package, name = path.rsplit('.', 1)
        require(properties(ref(package + '.Default__' + name + '_C'), names=['previewText'])['previewText'] == '',
                'PreviewText must stay empty')
    begin(DETAILS, COMPOSITE_PARENT)
    canvas = add(DETAILS, 'FocusDetailsCanvas', '/Script/UMG.CanvasPanel', props={'visibility': 'HitTestInvisible'})
    add(DETAILS, 'ItemName', NAME + '_C', canvas,
        props={'visibility': 'HitTestInvisible', 'bIsFocusable': False},
        slot=canvas_slot(0, 0, 552, 42), variable=True)
    add(DETAILS, 'ItemDescription', DESCRIPTION + '_C', canvas,
        props={'visibility': 'HitTestInvisible', 'bIsFocusable': False, 'clipping': 'ClipToBounds'},
        slot=canvas_slot(0, 44, 552, 54), variable=True)
    return {'asset': DETAILS, 'tree': tree(DETAILS), 'compile_after_return': [DETAILS],
            'runtime_roles': {'ItemName': 'actual StaticText/Name/Ammo.Primary.Name fragment',
                              'ItemDescription': 'actual Description/Text fragment, or collapsed'}}


def verify():
    leaves = verify_leaves()
    require(tree(DETAILS)['info']['parentClass'] == ref(COMPOSITE_PARENT), 'Wrong focus composite parent')
    rows = widgets(DETAILS)
    require(set(rows) == {'FocusDetailsCanvas', 'ItemName', 'ItemDescription'}, 'Focus composite tree differs')
    for name, cls, box in (('ItemName', NAME, (0, 0, 552, 42)),
                           ('ItemDescription', DESCRIPTION, (0, 44, 552, 54))):
        require(rows[name]['widgetClassPath']['refPath'] in (cls, cls + '_C'), 'Wrong focus leaf class')
        require(properties(rows[name]['slot'], names=['layoutData', 'bAutoSize', 'zOrder']) == canvas_slot(*box),
                'Focus details placement differs: ' + name)
    return {'leaves': leaves, 'details': tree(DETAILS), 'compiled_class': DETAILS + '_C',
            'scope': 'New focus presentation assets only; no authored sample names/descriptions'}


def run():
    if MODE == 'audit':
        return {'existing_card_name_leaf': tree(SOURCE_NAME), 'planned_assets': [NAME, DESCRIPTION, DETAILS],
                'required_native_parents': [TEXT_PARENT, COMPOSITE_PARENT], 'no_mutations': True}
    if MODE == 'verify':
        return verify()
    if MODE == 'verify_leaves':
        return verify_leaves()
    require(BACKUP_READY, 'Caller must back up Quick Select before authoring')
    if MODE == 'name_leaf':
        return author_leaf(NAME, FONT, 26, 'EEEAE0', False)  # 34px reference, integer Slate points.
    if MODE == 'description_leaf':
        return author_leaf(DESCRIPTION, BODY, 15, 'B9B9B9', True)  # 20px reference.
    if MODE == 'leaves':
        return {'name': author_leaf(NAME, FONT, 26, 'EEEAE0', False),
                'description': author_leaf(DESCRIPTION, BODY, 15, 'B9B9B9', True),
                'compile_after_return': [NAME, DESCRIPTION], 'next': 'Compile leaves externally, then MODE=details'}
    if MODE == 'details':
        return author_details()
    raise RuntimeError('Unknown MODE: ' + MODE)
