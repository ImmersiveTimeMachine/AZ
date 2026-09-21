# @Description: Snapshot owned UI and prepare isolated Field Notes CommonUI style resources.
"""Explicit stages; importing this module has no editor side effects.

    snapshot_stage0(max_assets=1)  # repeat until complete; editor-template reads only
    snapshot_style_sources()     # native schema/defaults + source file hashes
    prepare_fonts()              # idle only; new Bold FontFace + Font, no overwrite
    prepare_styles()             # idle only; new owned copies, no existing UI changes
    # Compile returned style Blueprints through the native tool OUTSIDE Python.
    configure_text_styles()      # idle only, generated class required
    # Native compile text styles before configuring buttons that reference them.
    configure_button_styles()
    # Native compile all returned styles, explicitly save them + new fonts externally.
    verify_resources()           # read-back; does not claim visual/DPI acceptance

No compile, save, PIE control, widget-tree edits or assignment to existing widgets.
All writes are restricted to exact new resource paths under Style/FieldNotes.
Existing resource reruns require authoring ownership; pre-write defaults and any
saved package bytes are backed up. Module 4 applies styles separately after the
baseline, reflected native properties, and runtime geometry are reviewed.
"""
import copy
import gc
import hashlib
import json
import re
import shutil
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'Saved/FieldNotesImplementation'
TOKENS = OUT / 'style-tokens.json'
DEST = '/Game/AZ/Blueprints/Menu/Style/FieldNotes'
OWNER_KEY = 'AZ.FieldNotes.ResourceOwner'
OWNER = 'field_notes_styles_setup:v1'
HASH_KEY = 'AZ.FieldNotes.TokenHash'
SOURCE_KEY = 'AZ.FieldNotes.Source'
TEXT_SOURCE = '/Game/InventorySystemPro/ExampleContent/Horror/UI/Styles/TextStyleNormal_Horror'
BUTTON_SOURCE = '/Game/AZ/Blueprints/Menu/CommonInventory/CommonUI/Stytle/MenuCommonButtonStyle_AZ'
REGULAR = '/Game/AZ/Blueprints/Menu/Map/Art/Fonts/FF_CHALK_RobotoRegular_Font'
BOLD_FACE = DEST + '/Fonts/FF_CHALK_RobotoBold'
BOLD_FONT = BOLD_FACE + '_Font'
BOLD_FILE = ROOT / 'UI Design/CHALK_HUD_v01/sources/fonts/Roboto-Bold.ttf'
CONTEXTS = ('Paper', 'Overlay')
TEXT_ROLES = ('PageTitle', 'SectionTitle', 'Body', 'Caption', 'Muted', 'Button', 'ButtonDisabled')
KNOWN_ROOTS = (
    '/Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBP_GameInventoryMenu',
    '/Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBP_GameInventorySwitcher',
    '/Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBPCharacterVitalsPanel',
    '/Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBP_CharacterSkillsPanel',
    '/Game/AZ/Blueprints/Menu/Map/WBP_AZ_QuestMapPage',
    '/Game/AZ/Blueprints/Menu/Map/WBP_AZ_QuestJournalEntry',
    '/Game/AZ/Blueprints/Menu/Map/WBP_AZ_MapCanvas',
    '/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD',
    '/Game/AZ/Blueprints/Menu/HUD/Navigation/WBP_AZ_CompassModule',
    '/Game/AZ/Blueprints/Menu/HUD/Navigation/WBP_AZ_Compass',
    '/Game/AZ/Blueprints/Menu/HUD/Navigation/WBP_AZ_CompassMarker',
    '/Game/AZ/Blueprints/Menu/HUD/Navigation/WBP_AZ_WorldMarker',
    '/Game/AZ/Blueprints/Menu/HUD/Navigation/WBP_AZ_WorldMarkerContainer',
    '/Game/AZ/Blueprints/Menu/HUD/Navigation/Quests/WBP_AZ_QuestModule',
    '/Game/AZ/Blueprints/Menu/HUD/Navigation/Quests/WBP_AZ_MissionNotification',
    '/Game/AZ/Blueprints/Menu/HUD/Navigation/Quests/WBP_AZ_Mission',
    '/Game/AZ/Blueprints/Menu/HUD/Navigation/Quests/WBP_AZ_Task',
) + tuple('/Game/AZ/Blueprints/Menu/HUD/QuickSelect/' + name for name in (
    'WBP_AZ_QuickSelect', 'WBP_AZ_QuickSelectEntry', 'WBP_AZ_QuickSelectFists',
    'WBP_AZ_QuickSelectItemDetails', 'WBP_AZ_QuickSelectNameLeaf',
    'WBP_AZ_QuickSelectFocusDetails', 'WBP_AZ_QuickSelectFocusNameLeaf',
    'WBP_AZ_QuickSelectDescriptionLeaf', 'WBP_AZ_QuickSelectImageLeaf'))


def ue():
    import unreal
    return unreal


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def stamp():
    return datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S_%fZ')


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False), encoding='utf-8')
    return value


def object_path(package):
    return package if '.' in package.rsplit('/', 1)[-1] else package + '.' + package.rsplit('/', 1)[-1]


def ref(package):
    return {'refPath': object_path(package)}


def package_path(value):
    if isinstance(value, dict):
        value = value.get('refPath', '')
    return value.split('.')[0] if isinstance(value, str) and value.startswith('/Game/') else None


def package_file(package):
    require(package.startswith('/Game/'), 'Expected a /Game package: ' + package)
    return ROOT / 'Content' / (package[len('/Game/'):] + '.uasset')


def tokens():
    data = json.loads(TOKENS.read_text(encoding='utf-8'))
    require(data['version'] == 1 and data['family'] == '02_FIELD_NOTES', 'Unexpected token contract')
    require(data['fonts']['Regular']['asset'] == REGULAR, 'Regular font contract drift')
    require(data['fonts']['Bold']['asset'] == BOLD_FONT, 'Bold font contract drift')
    return data


def native_tool(name, **args):
    allowed = {'UMGToolSet.UMGToolSet.GetWidgets', 'EditorToolset.EditorAppToolset.IsPIERunning'}
    require(name in allowed, 'Non-read tool outside this recipe: ' + name)
    group, function = name.rsplit('.', 1)
    result = ue().ToolsetRegistry.execute_tool(group, function, json.dumps(args))
    require(result.is_complete and not result.error, name + ': ' + str(result.error))
    payload = json.loads(result.value)
    require('returnValue' in payload, 'Missing native tool return: ' + name)
    return payload['returnValue']


def idle():
    require(not native_tool('EditorToolset.EditorAppToolset.IsPIERunning'),
            'This resource-write stage requires editor Idle. Snapshot reads may run separately.')


def resolve(reference):
    path = reference['refPath'] if isinstance(reference, dict) else reference
    obj = ue().find_object(None, path) or ue().load_object(None, path)
    require(obj is not None, 'Object unavailable: ' + path)
    return obj


def defaults(package):
    bp = ue().load_asset(package)
    require(bp is not None and isinstance(bp, ue().Blueprint), 'Expected Blueprint: ' + package)
    cls = bp.generated_class()
    require(cls is not None, 'Native compile the Blueprint first: ' + package)
    return bp, ue().get_default_object(cls)


def read_object(obj):
    # Direct native functions, not nested Python ObjectTools registry calls.
    raw_schema = ue().ToolsetLibrary.list_struct_properties(obj.get_class())
    # The base PanelSlot has no editable layout fields (Parent/Content are
    # relationship fields without editor-edit flags). The native schema helper returns an empty string,
    # not an empty JSON object, for this exact class. Its tree relation is still
    # captured by GetWidgets; do not generalize an empty result to other types.
    if not raw_schema and obj.get_class().get_path_name() == '/Script/UMG.PanelSlot':
        return {'object': obj.get_path_name(), 'schema': {}, 'values': {},
                'no_editable_properties': True}
    require(bool(raw_schema), 'Empty property schema: ' + obj.get_path_name())
    schema = json.loads(raw_schema)
    require(isinstance(schema, dict), 'Invalid native schema: ' + obj.get_path_name())
    values = json.loads(ue().ToolsetLibrary.get_object_properties(obj, list(schema)))
    require(isinstance(values, dict) and set(schema).issubset(values),
            'Incomplete property read: ' + obj.get_path_name())
    return {'object': obj.get_path_name(), 'schema': schema, 'values': values}


def _references(value):
    if isinstance(value, dict):
        if 'refPath' in value:
            yield value['refPath']
        for item in value.values():
            yield from _references(item)
    elif isinstance(value, list):
        for item in value:
            yield from _references(item)


def snapshot_stage0(max_assets=1, snapshot_id='baseline'):
    """Resumable read-only recursive owned-widget baseline; no package saves.

    GetWidgets captures hierarchy/slots. Native property reads capture actual
    template overrides and CDO class references. Owned WidgetBlueprint children
    and owned WidgetBlueprint parents are queued; external paths are recorded.
    """
    require(1 <= max_assets <= 4, 'Use 1-4 Blueprints per bounded call')
    require(re.fullmatch(r'[A-Za-z0-9_-]+', snapshot_id) is not None, 'Invalid snapshot id')
    folder = OUT / 'Baseline' / snapshot_id
    index_file = folder / 'manifest.json'
    if index_file.exists():
        index = json.loads(index_file.read_text(encoding='utf-8'))
        require(index['roots'] == list(KNOWN_ROOTS), 'Snapshot roots changed; use a new snapshot id')
    else:
        index = {'version': 1, 'started_utc': stamp(), 'roots': list(KNOWN_ROOTS),
                 'pending': list(KNOWN_ROOTS), 'assets': {}, 'external_dependencies': [],
                 'dirty_packages_at_start': [p.get_path_name() for p in ue().EditorLoadingAndSavingUtils.get_dirty_content_packages()],
                 'complete': False, 'mutation_performed': False}
        write(index_file, index)
    for _ in range(max_assets):
        if not index['pending']:
            break
        path = index['pending'][0]
        require(path.startswith('/Game/AZ/Blueprints/Menu/'), 'Unexpected owned widget path')
        bp, cdo = defaults(path)
        require(isinstance(bp, ue().WidgetBlueprint), 'Not a WidgetBlueprint: ' + path)
        tree = native_tool('UMGToolSet.UMGToolSet.GetWidgets', widgetBlueprint=ref(path))
        require(isinstance(tree, dict) and isinstance(tree.get('widgets'), list), 'Invalid GetWidgets result')
        require(tree.get('info', {}).get('widgetCount') == len(tree['widgets']), 'Incomplete tree: ' + path)
        snapshot = {'asset': path, 'captured_utc': stamp(), 'tree': tree,
                    'cdo': read_object(cdo), 'templates': [],
                    'saved_package_sha256': digest(package_file(path)) if package_file(path).exists() else None}
        for row in tree['widgets']:
            item = {'row': row}
            for key in ('widget', 'slot'):
                if isinstance(row.get(key), dict):
                    item[key] = read_object(resolve(row[key]))
            snapshot['templates'].append(item)
        child_paths = set()
        for referenced in _references(snapshot):
            candidate = package_path(referenced)
            if not candidate or candidate == path:
                continue
            if candidate.startswith('/Game/AZ/Blueprints/Menu/'):
                # Only load class/tree references for recursion; do not load every texture.
                if referenced.endswith('_C') or candidate.rsplit('/', 1)[-1].startswith(('WBP_', 'AZ_WBP')):
                    asset = ue().load_asset(candidate)
                    if isinstance(asset, ue().WidgetBlueprint):
                        child_paths.add(candidate)
            elif candidate not in index['external_dependencies']:
                index['external_dependencies'].append(candidate)
        relative = path[len('/Game/'):] + '.json'
        write(folder / relative, snapshot)
        index['assets'][path] = {'receipt': relative, 'widget_count': len(tree['widgets']),
                                 'saved_package_sha256': snapshot['saved_package_sha256']}
        index['pending'].pop(0)
        for child in sorted(child_paths):
            if child not in index['assets'] and child not in index['pending']:
                index['pending'].append(child)
        index['complete'] = not index['pending']
        index['external_dependencies'].sort()
        write(index_file, index)
    gc.collect()
    return {'manifest': str(index_file), 'captured': len(index['assets']),
            'pending': len(index['pending']), 'next': index['pending'][:4], 'complete': index['complete']}


def snapshot_style_sources():
    data = {'captured_utc': stamp(), 'token_sha256': digest(TOKENS), 'sources': {}}
    for package, expected in ((TEXT_SOURCE, ue().CommonTextStyle), (BUTTON_SOURCE, ue().CommonButtonStyle)):
        _, cdo = defaults(package)
        require(isinstance(cdo, expected), 'Unexpected source style class: ' + package)
        data['sources'][package] = read_object(cdo)
        data['sources'][package]['saved_package_sha256'] = digest(package_file(package))
    data['regular_font'] = verify_font(REGULAR)
    require(BOLD_FILE.is_file(), 'Bold font source missing')
    data['bold_ttf_sha256'] = digest(BOLD_FILE)
    gc.collect()
    return write(OUT / 'Resources/source-baseline.json', data)


def verify_font(package, expected_face=None):
    font = ue().load_asset(package)
    require(isinstance(font, ue().Font), 'Font asset missing: ' + package)
    composite = font.get_editor_property('composite_font')
    # DefaultTypeface is protected in this engine's Python wrapper. Its native
    # serialization is readable without changing it or assuming a face name.
    exported = composite.export_text()
    match = re.search(r'^\(DefaultTypeface=(.*?),FallbackTypeface=', exported)
    require(match is not None, 'Cannot inspect default typeface serialization: ' + package)
    names = [quoted or bare for quoted, bare in re.findall(r'\bName=(?:"([^"]+)"|([A-Za-z0-9_]+))', match.group(1))]
    require(names == ['Default'], 'Expected the verified Default typeface: ' + package + ' ' + str(names))
    if expected_face:
        require(object_path(expected_face) in exported, 'Font references wrong face: ' + package)
    return {'asset': package, 'typefaces': names, 'composite': exported}


def resource_paths():
    texts = {(context, role): DEST + '/Text/TS_FN_' + context + '_' + role
             for context in CONTEXTS for role in TEXT_ROLES}
    buttons = {context: DEST + '/Buttons/BS_FN_' + context for context in CONTEXTS}
    return texts, buttons


def require_owned(package):
    texts, buttons = resource_paths()
    require(package in set(texts.values()) | set(buttons.values()) | {BOLD_FACE, BOLD_FONT},
            'Outside exact resource allowlist: ' + package)
    asset = ue().load_asset(package)
    require(asset is not None and ue().EditorAssetLibrary.get_metadata_tag(asset, OWNER_KEY) == OWNER,
            'Asset is not owned by this recipe: ' + package)
    return asset


def source_baseline():
    path = OUT / 'Resources/source-baseline.json'
    require(path.is_file(), 'Run snapshot_style_sources() before creating resources')
    result = json.loads(path.read_text(encoding='utf-8'))
    for package, snapshot in result['sources'].items():
        _, cdo = defaults(package)
        require(read_object(cdo)['values'] == snapshot['values'], 'Source CDO changed; capture a new source baseline: ' + package)
        require(digest(package_file(package)) == snapshot['saved_package_sha256'], 'Source package changed: ' + package)
    return result


def prepare_fonts():
    idle()
    baseline = source_baseline()
    verify_font(REGULAR)
    require(digest(BOLD_FILE) == baseline['bold_ttf_sha256'], 'Bold source changed since snapshot')
    have_face = ue().EditorAssetLibrary.does_asset_exist(BOLD_FACE)
    have_font = ue().EditorAssetLibrary.does_asset_exist(BOLD_FONT)
    require(have_face == have_font, 'Partial Bold import exists; inspect it before retrying')
    if not have_face:
        factory = ue().FontFileImportFactory()
        factory.set_editor_property('batch_create_font_asset', ue().BatchCreateFontAsset.YES)
        task = ue().AssetImportTask()
        for key, value in {'filename': str(BOLD_FILE), 'destination_path': DEST + '/Fonts',
                           'destination_name': BOLD_FACE.rsplit('/', 1)[-1], 'automated': True,
                           'replace_existing': False, 'save': False, 'factory': factory}.items():
            task.set_editor_property(key, value)
        ue().AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        for path, expected in ((BOLD_FACE, ue().FontFace), (BOLD_FONT, ue().Font)):
            asset = ue().load_asset(path)
            require(isinstance(asset, expected), 'Font import did not create exact expected asset: ' + path)
            ue().EditorAssetLibrary.set_metadata_tag(asset, OWNER_KEY, OWNER)
            ue().EditorAssetLibrary.set_metadata_tag(asset, SOURCE_KEY, str(BOLD_FILE))
    else:
        require_owned(BOLD_FACE)
        require_owned(BOLD_FONT)
    result = {'regular': verify_font(REGULAR), 'bold': verify_font(BOLD_FONT, BOLD_FACE),
              'explicit_save_after_readback': [BOLD_FACE, BOLD_FONT], 'saved': False}
    gc.collect()
    return write(OUT / 'Resources/fonts-prepared.json', result)


def prepare_styles():
    idle()
    source_baseline()
    texts, buttons = resource_paths()
    created, reused = [], []
    # Check every collision before the first duplication; never leave half the
    # resource family created because a later destination belongs to someone else.
    for target in list(texts.values()) + list(buttons.values()):
        if ue().EditorAssetLibrary.does_asset_exist(target):
            require_owned(target)
    for source, targets in ((TEXT_SOURCE, texts.values()), (BUTTON_SOURCE, buttons.values())):
        for target in targets:
            if ue().EditorAssetLibrary.does_asset_exist(target):
                require_owned(target)
                reused.append(target)
            else:
                asset = ue().EditorAssetLibrary.duplicate_asset(source, target)
                require(isinstance(asset, ue().Blueprint), 'Could not duplicate style: ' + target)
                ue().EditorAssetLibrary.set_metadata_tag(asset, OWNER_KEY, OWNER)
                ue().EditorAssetLibrary.set_metadata_tag(asset, SOURCE_KEY, source)
                created.append(target)
    result = {'created': created, 'reused': reused,
              'native_compile_before_next_stage': list(texts.values()) + list(buttons.values()),
              'existing_widget_assignments': [], 'saved': False}
    gc.collect()
    return write(OUT / 'Resources/styles-prepared.json', result)


def _validate_patch(schema, values, label='root'):
    for key, value in values.items():
        require(key in schema, 'Unknown property ' + label + '.' + key)
        field = schema[key]
        if isinstance(value, dict):
            require('properties' in field, 'No child schema for ' + key)
            _validate_patch(field['properties'], value, label + '.' + key)
        elif 'enum' in field:
            require(value in field['enum'], 'Invalid enum ' + label + '.' + key)
        elif field.get('type') == 'integer':
            require(isinstance(value, int) and not isinstance(value, bool), 'Expected integer ' + key)


def _contains(actual, expected):
    if isinstance(expected, dict):
        return isinstance(actual, dict) and all(k in actual and _contains(actual[k], v) for k, v in expected.items())
    if isinstance(expected, (int, float)) and not isinstance(expected, bool):
        return isinstance(actual, (int, float)) and abs(actual - expected) < 0.00001
    return actual == expected


def _patch(package, patch):
    asset = require_owned(package)
    _, cdo = defaults(package)
    before = read_object(cdo)
    _validate_patch(before['schema'], patch)
    if _contains(before['values'], patch):
        return {'asset': package, 'changed': False, 'values': {k: before['values'][k] for k in patch}}
    backup_dir = OUT / 'Resources/BeforeWrites' / stamp()
    write(backup_dir / (package.rsplit('/', 1)[-1] + '.json'), before)
    saved = package_file(package)
    if saved.is_file():
        shutil.copy2(saved, backup_dir / saved.name)
    # The native setter supports partial struct patches. Validate ALL requested
    # keys before the call; failure still leaves a recovery receipt, never retries.
    require(ue().ToolsetLibrary.set_object_properties(cdo, json.dumps(patch)),
            'Native property write failed; inspect backup before retry: ' + str(backup_dir))
    after = read_object(cdo)
    require(_contains(after['values'], patch), 'Style readback mismatch: ' + package)
    ue().EditorAssetLibrary.set_metadata_tag(asset, HASH_KEY, digest(TOKENS))
    return {'asset': package, 'changed': True, 'backup': str(backup_dir),
            'values': {k: after['values'][k] for k in patch}}


def linear(context, role, alpha=1.0):
    value = copy.deepcopy(tokens()['contexts'][context][role]['linear_rgba'])
    value['a'] = alpha
    return value


def slate(color):
    return {'specifiedColor': color, 'colorUseRule': 'UseColor_Specified'}


def text_patch(context, role):
    data = tokens()
    spec = data['shared_text_roles'][role]
    font = data['fonts'][spec['weight']]
    return {'font': {'fontObject': ref(font['asset']), 'fontMaterial': 'None',
                     'typefaceFontName': font['typeface'], 'size': spec['slate_points'],
                     'letterSpacing': 0, 'skewAmount': 0, 'bForceMonospaced': False,
                     'outlineSettings': {'outlineSize': 0, 'outlineMaterial': 'None'}},
            'color': linear(context, spec['color']), 'bUsesDropShadow': False,
            'shadowOffset': {'x': 0, 'y': 0}, 'shadowColor': {'r': 0, 'g': 0, 'b': 0, 'a': 0},
            'margin': {'left': 0, 'top': 0, 'right': 0, 'bottom': 0},
            'lineHeightPercentage': 1.0}


def configure_text_styles():
    idle()
    verify_font(REGULAR)
    verify_font(BOLD_FONT, BOLD_FACE)
    texts, _ = resource_paths()
    results = [_patch(path, text_patch(context, role)) for (context, role), path in texts.items()]
    gc.collect()
    return write(OUT / 'Resources/text-styles-configured.json',
                 {'styles': results, 'native_compile_after_return': list(texts.values()), 'saved': False})


def button_patch(context):
    texts, _ = resource_paths()
    def brush(fill, edge, width):
        return {'drawAs': 'RoundedBox', 'resourceObject': 'None', 'resourceName': 'None',
                'tiling': 'NoTile', 'mirroring': 'NoMirror', 'imageType': 'NoImage',
                'tintColor': slate(linear(context, fill)),
                'margin': {'left': 0, 'top': 0, 'right': 0, 'bottom': 0},
                'outlineSettings': {'cornerRadii': {'x': 0, 'y': 0, 'z': 0, 'w': 0},
                                    'roundingType': 'FixedRadius', 'width': width,
                                    'color': slate(linear(context, edge)), 'bUseBrushTransparency': False}}
    normal = brush('panel', 'edge', 1.0)
    focused = brush('selected', 'text', 2.0)
    selected = brush('selected', 'edge', 2.0)
    text = ref(object_path(texts[(context, 'Button')]) + '_C')
    disabled = ref(object_path(texts[(context, 'ButtonDisabled')]) + '_C')
    return {'bSingleMaterial': False, 'normalBase': normal, 'normalHovered': focused,
            'normalPressed': focused, 'selectedBase': selected, 'selectedHovered': focused,
            'selectedPressed': focused, 'disabled': brush('panel', 'edge', 1.0),
            'normalTextStyle': text, 'normalHoveredTextStyle': text,
            'selectedTextStyle': text, 'selectedHoveredTextStyle': text, 'disabledTextStyle': disabled}


def configure_button_styles():
    idle()
    texts, buttons = resource_paths()
    for (context, role), path in texts.items():
        if role in ('Button', 'ButtonDisabled'):
            require_owned(path)
            _, cdo = defaults(path)
            require(_contains(read_object(cdo)['values'], text_patch(context, role)), 'Configure/compile text styles first')
    results = [_patch(path, button_patch(context)) for context, path in buttons.items()]
    gc.collect()
    return write(OUT / 'Resources/button-styles-configured.json',
                 {'styles': results, 'native_compile_after_return': list(buttons.values()),
                  'note': 'Selection margin/equipped notch remain separate widget features; no tree is changed.', 'saved': False})


def verify_resources():
    texts, buttons = resource_paths()
    result = {'fonts': [verify_font(REGULAR), verify_font(BOLD_FONT, BOLD_FACE)],
              'styles': [], 'token_sha256': digest(TOKENS),
              'visual_dpi_verification': 'Pending Module 4 rendered pilot; no global DPI change',
              'existing_widget_assignments': []}
    for path, expected in ([(p, text_patch(*key)) for key, p in texts.items()] +
                           [(p, button_patch(key)) for key, p in buttons.items()]):
        require_owned(path)
        _, cdo = defaults(path)
        values = read_object(cdo)['values']
        require(_contains(values, expected), 'Verification failed: ' + path)
        result['styles'].append({'asset': path, 'values': values})
    source_baseline()  # catches accidental source CDO/package mutations
    result['explicit_save_allowlist'] = [BOLD_FACE, BOLD_FONT] + list(texts.values()) + list(buttons.values())
    result['dirty_packages_at_readback'] = [p.get_path_name() for p in ue().EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    gc.collect()
    return write(OUT / 'Resources/resources-readback.json', result)
