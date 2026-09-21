# @Description: Stage the approved CHALK Field Journal Map page as owned native UMG child Blueprints.
"""Import this module inside Unreal, then call stages explicitly.

    prepare()
    tree('children')
    # Root compiles Canvas + JournalEntry using the dedicated native tool.
    tree('page')
    style()  # or supply imported font asset paths/typefaces
    # Root compiles all three Blueprints outside Python.
    configureMapDefinition('/Game/AZ/Blueprints/Menu/Map/DA_AZ_Map_L001')
    # Root compiles the page, verifies it, and explicitly saves owned packages.
    attachMenuClass()  # ONLY the existing menu's MapPageClass default changes.
    # Root compiles/saves the menu externally.

No compilation, saving assets, PIE, gameplay execution, input remapping or vendor
mutation is performed here. Importing this module performs no editor queries or
mutations. prepare/tree/style are resumable only for assets tagged as ours.
Every property write first resolves names against ObjectTools.list_properties;
unknown/ambiguous fields stop the stage. Native code owns player context,
button delegates, journal data and canvas input; no player-zero graph is added.
"""
import copy
import json
import math
import re
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'Saved/MapImplementation'
FOLDER = '/Game/AZ/Blueprints/Menu/Map'
PAGE = FOLDER + '/WBP_AZ_QuestMapPage'
ENTRY = FOLDER + '/WBP_AZ_QuestJournalEntry'
CANVAS = FOLDER + '/WBP_AZ_MapCanvas'
MENU = '/Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBP_GameInventoryMenu'
PARENTS = {PAGE: '/Script/AZ.AZ_QuestMapPage', ENTRY: '/Script/AZ.AZ_QuestJournalEntry',
           CANVAS: '/Script/AZ.AZ_MapCanvasWidget'}
OWNER_KEY = 'AZ.QuestMap.AuthoringOwner'
OWNER = 'quest_map_page_setup:v1'
UMG = 'UMGToolSet.UMGToolSet.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
TITLE_FONT = '/Game/InventorySystemPro/ExampleContent/Common/Art/Fonts/Oswald-Light_Font'
BODY_FONT = '/Engine/EngineFonts/Roboto'
COLORS = {'chalk': 'EEEAE0', 'peach': 'FFBA8C', 'sage': 'B5C8B7', 'muted': '929D96',
          'background': '0F1816', 'panel': '17201E', 'button': '26332D', 'line': '3A4840'}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def ref(path):
    return {'refPath': path}


def object_path(path):
    return path if '.' in path.rsplit('/', 1)[-1] else path + '.' + path.rsplit('/', 1)[-1]


def call(name, **args):
    allowed = {UMG + key for key in ('CreateWidgetBlueprint', 'GetWidgets', 'AddWidget', 'ToggleWidgetAsVariable')}
    allowed |= {OBJ + key for key in ('list_properties', 'get_properties', 'set_properties')}
    allowed.add('EditorToolset.EditorAppToolset.IsPIERunning')
    require(name in allowed, 'Tool is outside this authoring recipe: ' + name)
    group, tool = name.rsplit('.', 1)
    result = unreal.ToolsetRegistry.execute_tool(group, tool, json.dumps(args))
    require(result.is_complete and not result.error, name + ': ' + str(result.error))
    return json.loads(result.value)['returnValue']


def idle():
    require(not call('EditorToolset.EditorAppToolset.IsPIERunning'), 'Stop authoring while PIE is running.')


def write_receipt(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / ('page-' + name + '.json')).write_text(json.dumps(value, indent=2), encoding='utf-8')
    return value


def norm(value):
    return re.sub(r'[^a-z0-9]', '', value.lower())


def actual_key(schema, requested):
    if requested in schema:
        return requested
    candidates = [key for key in schema if norm(key) == norm(requested)]
    # Getter-backed bools may expose the native b prefix or the property getter name.
    if not candidates and requested.startswith('b') and len(requested) > 1 and requested[1].isupper():
        candidates = [key for key in schema if norm(key) == norm(requested[1:])]
    require(len(candidates) == 1, 'Property not uniquely present in reflected schema: ' + requested)
    return candidates[0]


def schema_value(schema, value):
    if isinstance(value, dict):
        children = schema.get('properties', {})
        require(children, 'No reflected child schema for structured value: ' + str(value))
        return {actual_key(children, key): schema_value(children[actual_key(children, key)], item)
                for key, item in value.items()}
    if 'enum' in schema and isinstance(value, str):
        members = schema['enum']
        if value not in members:
            matches = [item for item in members if norm(str(item)) == norm(value)]
            require(len(matches) == 1, 'Enum value unavailable: ' + value + '; available=' + str(members))
            return matches[0]
    return value


def merge(before, changes):
    result = copy.deepcopy(before)
    for key, value in changes.items():
        result[key] = merge(result[key], value) if isinstance(value, dict) and isinstance(result.get(key), dict) else copy.deepcopy(value)
    return result


def matches(actual, expected):
    if isinstance(expected, dict):
        return isinstance(actual, dict) and all(key in actual and matches(actual[key], value) for key, value in expected.items())
    if isinstance(expected, (int, float)) and not isinstance(expected, bool):
        return isinstance(actual, (int, float)) and abs(actual - expected) < 0.001
    return actual == expected


def properties(instance, changes=None, names=None):
    schema = json.loads(call(OBJ + 'list_properties', instance=instance))
    requested = list(changes) if changes is not None else (names if names is not None else list(schema))
    mapping = {key: actual_key(schema, key) for key in requested}
    before = json.loads(call(OBJ + 'get_properties', instance=instance, properties=list(mapping.values())))
    if changes is not None:
        resolved = {mapping[key]: schema_value(schema[mapping[key]], value) for key, value in changes.items()}
        require(call(OBJ + 'set_properties', instance=instance, values=json.dumps(merge(before, resolved))), 'Property write refused: ' + str(instance))
        after = json.loads(call(OBJ + 'get_properties', instance=instance, properties=list(mapping.values())))
        require(matches(after, resolved), 'Property read-back differs: ' + str(instance))
        return {key: after[actual] for key, actual in mapping.items()}
    return {key: before[actual] for key, actual in mapping.items()}


def get_tree(path):
    return call(UMG + 'GetWidgets', widgetBlueprint=ref(object_path(path)))


def rows(path):
    return {row['widgetName']: row for row in get_tree(path)['widgets'] if isinstance(row.get('widget'), dict)}


def owned(path):
    bp = unreal.load_asset(path)
    require(bp is not None, 'Missing prepared asset: ' + path)
    require(unreal.EditorAssetLibrary.get_metadata_tag(bp, OWNER_KEY) == OWNER, 'Refusing to alter unowned asset: ' + path)
    require(get_tree(path)['info']['parentClass'] == ref(PARENTS[path]), 'Native parent differs: ' + path)
    return bp


def class_default(path):
    bp = owned(path)
    cls = bp.generated_class()
    require(cls is not None, 'Compile the prepared Blueprint through native tools first: ' + path)
    return ref(unreal.get_default_object(cls).get_path_name())


def class_path(path):
    bp = owned(path)
    require(bp.generated_class() is not None, 'Native compile required for child Blueprint: ' + path)
    return bp.generated_class().get_path_name()


def prepare():
    idle()
    created = []
    for path, parent in PARENTS.items():
        require(unreal.load_class(None, parent) is not None, 'Required native class is not loaded: ' + parent)
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            call(UMG + 'CreateWidgetBlueprint', folderPath=FOLDER, assetName=path.rsplit('/', 1)[1], parentClass=ref(parent))
            bp = unreal.load_asset(path)
            require(bp is not None, 'Blueprint creation returned no asset: ' + path)
            unreal.EditorAssetLibrary.set_metadata_tag(bp, OWNER_KEY, OWNER)
            created.append(path)
        owned(path)
    return write_receipt('prepared', {'created': created, 'assets': PARENTS, 'next': "tree('children'); native compile children outside Python."})


def color(hex_rgb, alpha=1.0):
    values = [int(hex_rgb[i:i+2], 16) / 255.0 for i in (0, 2, 4)]
    values = [value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4 for value in values]
    return dict(zip(('r', 'g', 'b', 'a'), values + [alpha]))


def slate(hex_rgb, alpha=1.0):
    return {'specifiedColor': color(hex_rgb, alpha), 'colorUseRule': 'UseColor_Specified'}


def margin(left=0, top=0, right=0, bottom=0):
    return dict(left=left, top=top, right=right, bottom=bottom)


def stretch(left=0, top=0, right=0, bottom=0, z=0):
    return {'layoutData': {'offsets': margin(left, top, right, bottom),
                          'anchors': {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 1, 'y': 1}},
                          'alignment': {'x': 0, 'y': 0}}, 'bAutoSize': False, 'zOrder': z}


def rect(left, top, width, height, anchor=(0, 0), align=(0, 0), z=1):
    return {'layoutData': {'offsets': margin(left, top, width, height),
                          'anchors': {'minimum': dict(zip(('x', 'y'), anchor)), 'maximum': dict(zip(('x', 'y'), anchor))},
                          'alignment': dict(zip(('x', 'y'), align))}, 'bAutoSize': False, 'zOrder': z}


def weighted(value, padding=None):
    return {'size': {'sizeRule': 'Fill', 'value': value}, 'padding': padding or margin(),
            'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Fill'}


def add(path, name, cls, parent=None, slot=None, values=None, variable=False):
    owned(path)
    existing = rows(path)
    if name in existing:
        row = existing[name]
        actual_class = row['widgetClassPath']['refPath']
        require(actual_class.removesuffix('_C') == cls.removesuffix('_C'), 'Existing widget has a different class: ' + name)
        require((row.get('parent') if isinstance(row.get('parent'), dict) else None) == parent, 'Existing widget parent differs: ' + name)
    else:
        args = {'widgetBlueprint': ref(object_path(path)), 'widgetClass': ref(cls), 'widgetDisplayName': name}
        if parent is not None:
            args['parentWidget'] = parent
        row = call(UMG + 'AddWidget', **args)
        require(row.get('widgetName') == name, 'Native BindWidget name was not preserved: ' + name)
    properties(row['widget'], values or {'visibility': 'SelfHitTestInvisible'})
    if slot is not None:
        require(isinstance(row.get('slot'), dict), 'Requested slot is missing: ' + name)
        properties(row['slot'], slot)
    call(UMG + 'ToggleWidgetAsVariable', widgetBlueprint=ref(object_path(path)), widget=row['widget'], bIsVariable=variable)
    return row['widget']


def text(path, name, parent, label='', slot=None, variable=False):
    return add(path, name, '/Script/UMG.TextBlock', parent, slot,
               {'text': label, 'visibility': 'HitTestInvisible', 'autoWrapText': True}, variable)


def button(path, name, parent, label, slot=None):
    widget = add(path, name, '/Script/UMG.Button', parent, slot, {'visibility': 'Visible'}, True)
    text(path, name + 'Label', widget, label)
    return widget


def tree(stage='children'):
    idle()
    require(stage in ('children', 'page'), "Use tree('children'), externally compile, then tree('page').")
    if stage == 'children':
        # A visible transparent surface gives the native painted canvas a proper hit-test rectangle.
        add(CANVAS, 'CanvasInputSurface', '/Script/UMG.Border', values={'visibility': 'Visible',
            'brushColor': color('000000', 0.0), 'padding': margin()})
        root = add(ENTRY, 'EntryButton', '/Script/UMG.Button', values={'visibility': 'Visible'}, variable=True)
        box = add(ENTRY, 'EntryLayout', '/Script/UMG.VerticalBox', root,
                  {'padding': margin(12, 10, 12, 10), 'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Center'})
        text(ENTRY, 'TitleText', box, variable=True)
        text(ENTRY, 'SubtitleText', box, slot={'padding': margin(0, 4, 0, 0)}, variable=True)
        return write_receipt('children-tree', {'assets': [CANVAS, ENTRY], 'trees': {path: get_tree(path) for path in (CANVAS, ENTRY)},
            'compile_after_return': [CANVAS, ENTRY], 'next': "Native compile children, then tree('page')."})

    class_path(CANVAS)
    root = add(PAGE, 'MapPageRoot', '/Script/UMG.CanvasPanel')
    add(PAGE, 'PageBackground', '/Script/UMG.Border', root, stretch(),
        {'visibility': 'HitTestInvisible', 'brushColor': color(COLORS['background']), 'padding': margin()})
    safe = add(PAGE, 'PageSafeArea', '/Script/UMG.SafeZone', root, stretch(z=1))
    layout = add(PAGE, 'PageLayout', '/Script/UMG.CanvasPanel', safe)
    # Stretch the content within the existing DPI/safe-zone policy; never change project DPI settings.
    header = add(PAGE, 'Header', '/Script/UMG.HorizontalBox', layout, rect(40, 20, 740, 62))
    text(PAGE, 'ChalkTitle', header, 'CHALK', slot={'padding': margin(0, 0, 80, 0), 'verticalAlignment': 'VAlign_Center'})
    button(PAGE, 'InventoryButton', header, 'INVENTORY', {'padding': margin(0, 0, 18, 0), 'verticalAlignment': 'VAlign_Center'})
    text(PAGE, 'MapTabTitle', header, 'MAP', slot={'padding': margin(14, 0, 0, 0), 'verticalAlignment': 'VAlign_Center'})
    text(PAGE, 'FieldJournalLabel', layout, 'FIELD JOURNAL', rect(40, 90, 420, 36))
    body = add(PAGE, 'BodyColumns', '/Script/UMG.HorizontalBox', layout, stretch(40, 140, 40, 80, 1))
    journal = add(PAGE, 'JournalPanel', '/Script/UMG.Border', body, weighted(0.25, margin(0, 0, 18, 0)),
                  {'visibility': 'SelfHitTestInvisible', 'brushColor': color(COLORS['panel']), 'padding': margin(16, 16, 16, 16)})
    journal_layout = add(PAGE, 'JournalLayout', '/Script/UMG.VerticalBox', journal)
    text(PAGE, 'JournalHeading', journal_layout, 'JOURNAL', {'padding': margin(0, 0, 0, 10)})
    text(PAGE, 'StatusText', journal_layout, 'No known tasks', {'padding': margin(0, 0, 0, 8)}, True)
    add(PAGE, 'QuestList', '/Script/UMG.ScrollBox', journal_layout, weighted(0.55),
        {'visibility': 'Visible', 'orientation': 'Orient_Vertical', 'clipping': 'ClipToBoundsAlways'}, True)
    details = add(PAGE, 'DetailsScroll', '/Script/UMG.ScrollBox', journal_layout, weighted(0.45, margin(0, 12, 0, 10)),
                  {'visibility': 'Visible', 'orientation': 'Orient_Vertical', 'clipping': 'ClipToBoundsAlways'})
    text(PAGE, 'QuestTitle', details, 'Choose a task', {'padding': margin(0, 0, 0, 8)}, True)
    text(PAGE, 'QuestDescription', details, '', variable=True)
    button(PAGE, 'TrackButton', journal_layout, 'TRACK SELECTED OBJECTIVE', {'padding': margin(0, 6, 0, 0)})
    map_panel = add(PAGE, 'MapPanel', '/Script/UMG.Border', body, weighted(0.75),
                    {'visibility': 'SelfHitTestInvisible', 'brushColor': color(COLORS['panel']), 'padding': margin()})
    add(PAGE, 'MapCanvas', class_path(CANVAS), map_panel,
        {'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Fill'}, {'visibility': 'Visible'}, True)
    footer = add(PAGE, 'FooterLayout', '/Script/UMG.HorizontalBox', layout, rect(40, -62, 1000, 46, anchor=(0, 1)))
    button(PAGE, 'RecenterButton', footer, 'RECENTER', {'padding': margin(0, 0, 10, 0)})
    button(PAGE, 'ClearWaypointButton', footer, 'CLEAR PERSONAL MARKER', {'padding': margin(0, 0, 18, 0)})
    text(PAGE, 'MapInputHint', footer, 'SCROLL  Zoom   DRAG  Pan   LMB  Select   RMB  Personal marker',
         {'verticalAlignment': 'VAlign_Center'})
    text(PAGE, 'BackHint', layout, 'ESC  Inventory', rect(-190, -57, 150, 34, anchor=(1, 1)))
    return write_receipt('page-tree', {'tree': get_tree(PAGE), 'compile_after_return': [PAGE], 'next': 'style(); external native compile.'})


def font_value(asset_path, typeface, size):
    require(unreal.load_asset(asset_path) is not None, 'Font asset is unavailable; pass the imported path explicitly: ' + asset_path)
    return {'fontObject': ref(object_path(asset_path)), 'typefaceFontName': typeface, 'size': size,
            'letterSpacing': 0, 'skewAmount': 0, 'outlineSettings': {'outlineSize': 0}}


def style(title_font=TITLE_FONT, body_font=BODY_FONT, title_typeface='Default', body_typeface='Regular'):
    idle()
    title = lambda size: font_value(title_font, title_typeface, size)
    body = lambda size: font_value(body_font, body_typeface, size)
    properties(class_default(ENTRY), {'TitleFont': title(18), 'SubtitleFont': body(12)})
    properties(class_default(CANVAS), {'LabelFont': title(14), 'ChalkColor': color(COLORS['chalk']),
        'TrackedColor': color(COLORS['peach']), 'PersonalColor': color(COLORS['sage']),
        'BackgroundColor': color(COLORS['panel']), 'MarkerRadius': 7, 'MaximumZoom': 8, 'DragThreshold': 6})
    page_rows = rows(PAGE)
    styles = {'ChalkTitle': (title(34), 'chalk'), 'MapTabTitle': (title(22), 'peach'),
              'FieldJournalLabel': (title(18), 'peach'), 'JournalHeading': (title(24), 'chalk'),
              'StatusText': (body(13), 'muted'), 'QuestTitle': (title(20), 'chalk'),
              'QuestDescription': (body(14), 'chalk'), 'MapInputHint': (body(12), 'chalk'), 'BackHint': (title(15), 'chalk')}
    for name, (font, tint) in styles.items():
        require(name in page_rows, 'Run page tree stage before styling: ' + name)
        properties(page_rows[name]['widget'], {'font': font, 'colorAndOpacity': slate(COLORS[tint]),
                                             'autoWrapText': name in ('QuestDescription', 'QuestTitle', 'StatusText')})
    for name in ('InventoryButton', 'TrackButton', 'RecenterButton', 'ClearWaypointButton'):
        properties(page_rows[name]['widget'], {'widgetStyle': {
            'normal': {'tintColor': slate(COLORS['button'])}, 'hovered': {'tintColor': slate('3E4A40')},
            'pressed': {'tintColor': slate('4C5649')}, 'normalPadding': margin(12, 8, 12, 8),
            'pressedPadding': margin(12, 9, 12, 7)}})
        properties(page_rows[name + 'Label']['widget'], {'font': title(16), 'colorAndOpacity': slate(COLORS['chalk']),
            'justification': 'Center', 'autoWrapText': False})
    properties(page_rows['MapCanvas']['widget'], {'LabelFont': title(14), 'ChalkColor': color(COLORS['chalk']),
        'TrackedColor': color(COLORS['peach']), 'PersonalColor': color(COLORS['sage']), 'BackgroundColor': color(COLORS['panel'])})
    return write_receipt('styled', {'title_font': title_font, 'body_font': body_font, 'compile_after_return': [CANVAS, ENTRY, PAGE],
                                  'layout': '01 Field Journal; 25% journal / 75% map; existing DPI policy preserved'})


def configureMapDefinition(data_asset_path):
    idle()
    definition = unreal.load_asset(data_asset_path)
    require(definition is not None and isinstance(definition, unreal.AZ_MapDefinition), 'Expected a loaded AZ_MapDefinition: ' + data_asset_path)
    data = properties(ref(definition.get_path_name()), names=['MapId', 'LayerId', 'MapTexture', 'WorldOrigin', 'WorldSizeCm', 'RotationDegrees'])
    require(str(data['MapId']) == 'L_001' and str(data['LayerId']) == 'Outdoor', 'This first-page recipe expects L_001 / Outdoor.')
    require(isinstance(data['MapTexture'], dict), 'Assign the real calibrated map texture first.')
    size = data['WorldSizeCm']
    require(all(math.isfinite(float(size[key])) and float(size[key]) > 0 for key in ('x', 'y')), 'Invalid map world spans.')
    require(all(math.isfinite(float(data['WorldOrigin'][key])) for key in ('x', 'y', 'z'))
            and math.isfinite(float(data['RotationDegrees'])), 'Nonfinite map origin or rotation.')
    properties(class_default(PAGE), {'DefaultMapDefinition': ref(definition.get_path_name()), 'JournalEntryClass': ref(class_path(ENTRY))})
    return write_receipt('configured', {'map': definition.get_path_name(), 'data': data, 'compile_after_return': [PAGE],
                                      'next': 'Native compile/verify page; attachMenuClass() is the final isolated menu-default assignment.'})


def attachMenuClass():
    idle()
    expected = {'MapCanvas': class_path(CANVAS), 'QuestList': '/Script/UMG.ScrollBox'}
    page_rows = rows(PAGE)
    for name, cls in expected.items():
        require(name in page_rows and page_rows[name]['widgetClassPath']['refPath'].removesuffix('_C') == cls.removesuffix('_C'),
                'Required native binding is missing/wrong: ' + name)
    configured = properties(class_default(PAGE), names=['DefaultMapDefinition', 'JournalEntryClass'])
    require(isinstance(configured['DefaultMapDefinition'], dict) and isinstance(configured['JournalEntryClass'], dict),
            'Configure and externally compile the page before attaching it.')
    menu = unreal.load_asset(MENU)
    require(menu is not None and menu.generated_class() is not None, 'Existing inventory menu is unavailable.')
    before_tree = get_tree(MENU)
    cdo = ref(unreal.get_default_object(menu.generated_class()).get_path_name())
    before = properties(cdo, names=['MapPageClass'])
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    package_file = ROOT / 'Content' / (MENU[6:] + '.uasset')
    backup = OUT / ('MenuDefaultBackup_' + stamp)
    backup.mkdir(parents=True, exist_ok=True)
    if package_file.is_file():
        shutil.copy2(package_file, backup / package_file.name)
    (backup / 'live-before.json').write_text(json.dumps({'tree': before_tree, 'defaults': before}, indent=2), encoding='utf-8')
    after = properties(cdo, {'MapPageClass': ref(class_path(PAGE))})
    require(get_tree(MENU) == before_tree, 'Existing inventory widget tree changed unexpectedly.')
    return write_receipt('attached', {'asset': MENU, 'before': before, 'after': after, 'backup': str(backup),
        'inventory_tree_unchanged': True, 'compile_after_return': [MENU], 'save_called': False, 'runtime_verified': False})
