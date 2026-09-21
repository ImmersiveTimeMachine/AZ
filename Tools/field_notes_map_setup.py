# @Description: Import approved Field Notes art and apply the isolated Map/journal paper pilot.
"""Root-executed stages only; import is inert, no compilation/saving/PIE control.

    preflight()                  # complete baseline + full live defaults/trees; backups
    import_art(max_assets=2)      # repeat until complete; exact eight new textures
    style_canvas()               # CDO + embedded Page.MapCanvas, never input values
    style_journal()              # CDO + original text/button templates
    selection_indicator()        # optional native root wrapper + 4px passive margin
    style_page()                 # original page/load controls, no callback/visibility edits
    bind_map_art()               # ONLY MapTexture on DA_AZ_Map_L001
    # Native compile Canvas, JournalEntry, Page outside Python, then explicitly save.
    verify()                     # whole-property and topology checks, no visual claim

The existing three-column/map fraction, native controls, binding names and data
remain. The optional journal wrapper is the only structural edit. The import
also stages masks/compass art for later HUD work; this script does not assign it
to the compass/HUD or alter any material. Map is still native painted geometry.
"""
import copy
import gc
import importlib.util
import json
import struct
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
_spec = importlib.util.spec_from_file_location('az_field_notes_style_support', ROOT / 'Tools/field_notes_styles_setup.py')
S = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(S)
OUT = S.OUT / 'MapPilot'
BASELINE = S.OUT / 'Baseline/baseline'
PAGE = '/Game/AZ/Blueprints/Menu/Map/WBP_AZ_QuestMapPage'
ENTRY = '/Game/AZ/Blueprints/Menu/Map/WBP_AZ_QuestJournalEntry'
CANVAS = '/Game/AZ/Blueprints/Menu/Map/WBP_AZ_MapCanvas'
DEFINITION = '/Game/AZ/Blueprints/Menu/Map/DA_AZ_Map_L001'
TARGETS = (CANVAS, ENTRY, PAGE, DEFINITION)
ART_FOLDER = S.DEST + '/Art'
PRODUCTION = ROOT / 'UI Design/CHALK_FieldNotes_Production_v01'
ART_NAMES = ('T_FN_Story', 'T_FN_Side', 'T_FN_Personal', 'T_FN_Active',
             'T_FN_Complete', 'T_FN_Failed', 'T_FN_CompassStrip', 'T_FN_Map_L001')
OWNER_KEY = 'AZ.FieldNotes.ArtOwner'
OWNER = 'field_notes_map_setup:v1'
SOURCE_HASH_KEY = 'AZ.FieldNotes.PNGSHA256'
UMG = 'UMGToolSet.UMGToolSet.'
NEW_ENTRY_WIDGETS = ('FNEntryOverlay', 'FNSelectionWidth', 'SelectionIndicator')


def call(function, **args):
    S.require(function in ('GetWidgets', 'WrapWidgets', 'RenameWidget', 'AddWidget', 'ToggleWidgetAsVariable'),
              'UMG operation outside the pilot')
    r = S.ue().ToolsetRegistry.execute_tool('UMGToolSet.UMGToolSet', function, json.dumps(args))
    S.require(r.is_complete and not r.error, function + ': ' + str(r.error))
    value = json.loads(r.value)
    # Native void ToggleWidgetAsVariable may omit returnValue.
    S.require('returnValue' in value or function == 'ToggleWidgetAsVariable', 'Missing tool return: ' + function)
    return value.get('returnValue')


def tree(package):
    value = call('GetWidgets', widgetBlueprint=S.ref(package))
    S.require(value.get('info', {}).get('widgetCount') == len(value.get('widgets', [])), 'Incomplete widget tree')
    return value


def rows(package):
    return {x['widgetName']: x for x in tree(package)['widgets'] if isinstance(x.get('widget'), dict)}


def capture(package):
    S.require(package in TARGETS, 'Outside pilot targets')
    if package == DEFINITION:
        asset = S.ue().load_asset(package)
        S.require(isinstance(asset, S.ue().AZ_MapDefinition), 'Wrong map definition class')
        return {'asset': package, 'data': S.read_object(asset)}
    _, cdo = S.defaults(package)
    t = tree(package)
    result = {'asset': package, 'tree': t, 'cdo': S.read_object(cdo), 'templates': []}
    for row in t['widgets']:
        item = {'row': row}
        for key in ('widget', 'slot'):
            if isinstance(row.get(key), dict):
                item[key] = S.read_object(S.resolve(row[key]))
        result['templates'].append(item)
    return result


def baseline(package):
    path = OUT / 'Before' / (package.rsplit('/', 1)[-1] + '.json')
    S.require(path.is_file(), 'Run preflight() before writes: ' + str(path))
    return json.loads(path.read_text(encoding='utf-8'))


def _equal(a, b):
    return S._contains(a, b) and S._contains(b, a)


def _assert_patch_only(before, after, patch, label):
    """Every unlisted nested leaf must remain equal; no broad property exemptions."""
    S.require(isinstance(before, dict) and isinstance(after, dict), 'Expected object values: ' + label)
    S.require(set(before) == set(after), 'Property inventory changed: ' + label)
    for key in before:
        if key not in patch:
            S.require(_equal(before[key], after[key]), 'Unapproved property difference: ' + label + '.' + key)
        elif isinstance(patch[key], dict) and isinstance(before[key], dict) and isinstance(after[key], dict):
            _assert_patch_only(before[key], after[key], patch[key], label + '.' + key)
        else:
            S.require(_equal(after[key], before[key]) or _equal(after[key], patch[key]),
                      'Property is neither baseline nor approved target: ' + label + '.' + key)


def _object_record(snapshot, name, slot=False):
    if name == '@cdo':
        return snapshot['cdo']
    if name == '@data':
        return snapshot['data']
    items = [x for x in snapshot['templates'] if x['row']['widgetName'] == name]
    S.require(len(items) == 1 and ('slot' if slot else 'widget') in items[0], 'Missing actual template: ' + name)
    return items[0]['slot' if slot else 'widget']


def _font(reference_role):
    data = S.tokens()
    role = data['typography']['reference_sizes'][reference_role]
    font = data['fonts'][role['weight']]
    return {'fontObject': S.ref(font['asset']), 'typefaceFontName': font['typeface'],
            'size': role['slate_points'], 'fontMaterial': 'None', 'letterSpacing': 0,
            'skewAmount': 0, 'outlineSettings': {'outlineSize': 0, 'outlineMaterial': 'None'}}


def _color(role):
    return S.linear('Paper', role)


def _text(reference_role, color='text'):
    return {'font': _font(reference_role), 'colorAndOpacity': S.slate(_color(color)),
            'shadowColorAndOpacity': {'r': 0, 'g': 0, 'b': 0, 'a': 0}, 'shadowOffset': {'x': 0, 'y': 0}}


def button_style():
    shared = S.button_patch('Paper')
    return {'normal': copy.deepcopy(shared['normalBase']), 'hovered': copy.deepcopy(shared['normalHovered']),
            'pressed': copy.deepcopy(shared['normalPressed']), 'disabled': copy.deepcopy(shared['disabled']),
            'normalForeground': S.slate(_color('text')), 'hoveredForeground': S.slate(_color('text')),
            'pressedForeground': S.slate(_color('text')), 'disabledForeground': S.slate(_color('muted'))}


def canvas_patch():
    return {'chalkColor': _color('text'), 'trackedColor': _color('text'), 'personalColor': _color('personal'),
            'bUseCategoryMarkerStyle': True, 'storyColor': _color('story'), 'sideQuestColor': _color('side'),
            'selectionColor': _color('text'), 'trackingColor': _color('text'),
            'markerLineThickness': 1.8, 'trackedMarkerLineThickness': 2.5, 'selectionFramePadding': 4.0,
            'backgroundColor': _color('panel'), 'markerRadius': 17.0, 'labelFont': _font('hint_max')}


def journal_patch():
    style = button_style()
    # The new FButtonStyle CDO starts with zero padding. Preserve the existing
    # EntryButton template's content padding instead of changing row geometry.
    initial = _object_record(baseline(ENTRY), 'EntryButton')['values']['widgetStyle']
    for key in ('normalPadding', 'pressedPadding'):
        style[key] = copy.deepcopy(initial[key])
    return {'titleFont': _font('selected_quest'), 'subtitleFont': _font('body_min'),
            'titleColor': _color('text'), 'selectedTitleColor': _color('text'), 'subtitleColor': _color('muted'),
            'bOverrideRowButtonStyle': True, 'rowButtonStyle': style,
            'normalBackgroundColor': _color('panel'), 'selectedBackgroundColor': _color('selected'),
            'hoveredBackgroundColor': _color('selected'), 'pressedBackgroundColor': _color('selected'),
            'selectionIndicatorColor': _color('text')}


def planned(package):
    """Exact per-object property allowlist; no generic recursive recolouring."""
    if package == CANVAS:
        return {('@cdo', False): canvas_patch()}
    if package == ENTRY:
        return {('@cdo', False): journal_patch(), ('TitleText', False): _text('selected_quest'),
                ('SubtitleText', False): _text('body_min', 'muted'),
                ('EntryButton', False): {'widgetStyle': button_style()}}
    if package == DEFINITION:
        return {('@data', False): {'mapTexture': S.ref(ART_FOLDER + '/T_FN_Map_L001')}}
    S.require(package == PAGE, 'Unknown plan target')
    plan = {('@cdo', False): {'sectionHeadingFont': _font('hint_max'), 'sectionHeadingColor': _color('muted')},
            ('MapCanvas', False): canvas_patch()}
    for name, role in {'PageBackground': 'background', 'JournalPanel': 'panel', 'MapPanel': 'panel'}.items():
        plan[(name, False)] = {'brushColor': _color(role)}
    typography = {'ChalkTitle': ('page_brand', 'text'), 'MapTabTitle': ('page_title', 'text'),
                  'FieldJournalLabel': ('hint_max', 'muted'), 'JournalHeading': ('journal_heading', 'text'),
                  'StatusText': ('hint_max', 'muted'), 'QuestTitle': ('selected_quest', 'text'),
                  'QuestDescription': ('body_min', 'text'), 'MapInputHint': ('hint_min', 'muted'),
                  'BackHint': ('hint_min', 'text'), 'CampaignLoadStatus': ('hint_min', 'muted')}
    for name, roles in typography.items():
        plan[(name, False)] = _text(*roles)
    for name in ('InventoryButton', 'TrackButton', 'RecenterButton', 'ClearWaypointButton',
                 'LoadRequestButton', 'LoadConfirmButton', 'LoadCancelButton'):
        plan[(name, False)] = {'widgetStyle': button_style()}
        # 18px maximum hint size keeps existing compact load and footer controls.
        plan[(name + 'Label', False)] = _text('hint_max')
    # Preserve responsive anchors, 25/75 weights, children and all input regions.
    # At 1080p these offsets place the body at y153 with a 787px height.
    plan[('Header', True)] = {'layoutData': {'offsets': {'left': 56}}}
    plan[('FieldJournalLabel', True)] = {'layoutData': {'offsets': {'left': 56, 'top': 110}}}
    plan[('BodyColumns', True)] = {'layoutData': {'offsets': {'left': 56, 'top': 153, 'right': 56, 'bottom': 140}}}
    plan[('FooterLayout', True)] = {'layoutData': {'offsets': {'left': 56}}}
    plan[('CheckpointControls', True)] = {'layoutData': {'offsets': {'left': -456}}}
    return plan


def _assert_compatible(package, live):
    original = baseline(package)
    plan = planned(package)
    for selector, patch in plan.items():
        before = _object_record(original, *selector)
        S._validate_patch(before['schema'], patch)
    cdo_key = '@data' if package == DEFINITION else '@cdo'
    _assert_patch_only(_object_record(original, cdo_key)['values'], _object_record(live, cdo_key)['values'],
                       plan.get((cdo_key, False), {}), package + ':defaults')
    if package == DEFINITION:
        return
    old_rows = {x['row']['widgetName']: x for x in original['templates'] if 'widget' in x}
    now_rows = {x['row']['widgetName']: x for x in live['templates'] if 'widget' in x}
    added = set(now_rows) - set(old_rows)
    S.require(set(old_rows).issubset(now_rows), 'Existing widget removed: ' + package)
    S.require(not added or (package == ENTRY and added.issubset(NEW_ENTRY_WIDGETS)), 'Unexpected new widgets: ' + str(added))
    for name, old in old_rows.items():
        now = now_rows[name]
        S.require(old['row']['widget'] == now['row']['widget'], 'Template identity changed: ' + name)
        for key in ('widgetClassPath', 'widgetName', 'bIsVariable', 'namedSlotHost'):
            S.require(old['row'].get(key) == now['row'].get(key), 'Widget contract changed: ' + name + '.' + key)
        patch = copy.deepcopy(plan.get((name, False), {}))
        root_wrapped = package == ENTRY and name == 'EntryButton' and 'FNEntryOverlay' in now_rows
        if root_wrapped:
            patch['slot'] = now['widget']['values']['slot']
            S.require(now['row']['parent'] == now_rows['FNEntryOverlay']['row']['widget'], 'EntryButton left wrapper')
        else:
            S.require(old['row'].get('parent') == now['row'].get('parent'), 'Parent changed: ' + name)
            S.require(old['row'].get('slot') == now['row'].get('slot'), 'Slot identity changed: ' + name)
        _assert_patch_only(old['widget']['values'], now['widget']['values'], patch, name)
        if 'slot' in old:
            _assert_patch_only(old['slot']['values'], now['slot']['values'], plan.get((name, True), {}), name + ':slot')


def production():
    data = json.loads((PRODUCTION / 'production-manifest.json').read_text(encoding='utf-8'))
    products = {p['stem']: p for p in data['products'] if p['stem'] in ART_NAMES}
    S.require(set(products) == set(ART_NAMES), 'Incomplete approved production set')
    for name, p in products.items():
        path = Path(p['png'])
        S.require(path.resolve().parent == (PRODUCTION / 'unreal-art').resolve() and p['reopened'], 'Unverified art source')
        header = path.read_bytes()[:26]
        S.require(header[:8] == b'\x89PNG\r\n\x1a\n', 'Invalid PNG signature')
        S.require(list(struct.unpack('>II', header[16:24])) == p['size'], 'PNG dimensions differ: ' + name)
        S.require(header[24] == 8 and header[25] == 6, 'Expected 8-bit RGBA PNG: ' + name)
        p['sha256'] = S.digest(path)
    for path, sha in data['originals_unchanged'].items():
        S.require(S.digest(path) == sha, 'Retained art source changed: ' + path)
    return products


def preflight():
    """Read and back up exact targets. Existing staged work is checked, not reset."""
    manifest = json.loads((BASELINE / 'manifest.json').read_text(encoding='utf-8'))
    S.require(manifest['complete'], 'Whole owned-widget baseline must be complete')
    production()
    folder = OUT / 'Before'
    folder.mkdir(parents=True, exist_ok=True)
    for package in TARGETS:
        filename = folder / (package.rsplit('/', 1)[-1] + '.json')
        current = capture(package)
        if not filename.exists():
            if package != DEFINITION:
                recorded = json.loads((BASELINE / manifest['assets'][package]['receipt']).read_text(encoding='utf-8'))
                S.require(current['tree'] == recorded['tree'], 'Tree differs from complete baseline: ' + package)
                for key in ('cdo', 'templates'):
                    S.require(_equal(current[key], recorded[key]), 'Full property baseline differs: ' + package + ':' + key)
            else:
                v = current['data']['values']
                expected = {'mapId': 'L_001', 'layerId': 'Outdoor', 'worldOrigin': {'x': -1435, 'y': 3620, 'z': 0},
                            'worldSizeCm': {'x': 12400, 'y': 12400}, 'rotationDegrees': 0,
                            'bFlipU': False, 'bFlipV': False}
                S.require(S._contains(v, expected), 'Map calibration does not match retained art')
            S.write(filename, current)
            saved = S.package_file(package)
            S.require(saved.is_file(), 'No saved package to back up: ' + package)
            import shutil
            shutil.copy2(saved, folder / saved.name)
        _assert_compatible(package, current)
    gc.collect()
    return S.write(OUT / 'preflight.json', {'targets': list(TARGETS), 'backup': str(folder),
                                           'whole_baseline_verified': True, 'modified_assets': []})


def _write(package, selector, patch):
    S.require(package in TARGETS and selector in planned(package), 'Property target outside exact allowlist')
    original = _object_record(baseline(package), *selector)
    obj = S.resolve(original['object'])
    before = S.read_object(obj)
    S._validate_patch(before['schema'], patch)
    if S._contains(before['values'], patch):
        return
    breadcrumb = OUT / 'writes' / (S.stamp() + '.json')
    S.write(breadcrumb, {'asset': package, 'selector': selector, 'before': before, 'patch': patch, 'complete': False})
    S.require(S.ue().ToolsetLibrary.set_object_properties(obj, json.dumps(patch)), 'Partial property write; inspect ' + str(breadcrumb))
    after = S.read_object(obj)
    _assert_patch_only(before['values'], after['values'], patch, str(selector))
    S.require(S._contains(after['values'], patch), 'Readback mismatch: ' + str(selector))
    S.write(breadcrumb, {'asset': package, 'selector': selector, 'before': before, 'patch': patch, 'after': after, 'complete': True})


def _style(package, selectors=None):
    S.idle()
    S.require((OUT / 'preflight.json').is_file(), 'Run preflight() first')
    S.verify_font(S.REGULAR)
    S.verify_font(S.BOLD_FONT, S.BOLD_FACE)
    S.verify_resources()  # paper controls must be configured, not mere duplicates
    _assert_compatible(package, capture(package))
    changes = planned(package)
    selected = list(changes) if selectors is None else selectors
    for key in selected:
        _write(package, key, changes[key])
    live = capture(package)
    _assert_compatible(package, live)
    gc.collect()
    return S.write(OUT / (package.rsplit('/', 1)[-1] + '-styled.json'),
                   {'asset': package, 'changed_selectors': selected, 'readback': live,
                    'native_compile_after_return': [package], 'save_called': False})


def style_canvas():
    _style(CANVAS)
    return _style(PAGE, [('MapCanvas', False)])


def style_journal():
    return _style(ENTRY)


def style_page():
    return _style(PAGE)


def import_art(max_assets=2):
    """Legacy TextureFactory import; final pixel dimensions are verified later.

    Asset settings may enqueue asynchronous texture rebuilds even after import
    returns. Run verify() in a separate editor call after yielding; never treat
    the temporary platform-data size reported here as source pixel dimensions.
    """
    S.idle()
    S.require(1 <= max_assets <= 8, 'Use 1-8 imports per call')
    products = production()
    for name in ART_NAMES:
        path = ART_FOLDER + '/' + name
        if S.ue().EditorAssetLibrary.does_asset_exist(path):
            texture = S.ue().load_asset(path)
            S.require(S.ue().EditorAssetLibrary.get_metadata_tag(texture, OWNER_KEY) == OWNER, 'Unowned texture collision: ' + path)
            S.require(S.ue().EditorAssetLibrary.get_metadata_tag(texture, SOURCE_HASH_KEY) == products[name]['sha256'],
                      'Existing texture source hash differs; no implicit reimport')
    imported = []
    for name in ART_NAMES:
        path = ART_FOLDER + '/' + name
        if S.ue().EditorAssetLibrary.does_asset_exist(path):
            continue
        if len(imported) >= max_assets:
            break
        receipt_path = OUT / 'ArtImports' / (name + '.json')
        receipt = {'asset': path, 'source': products[name]['png'], 'source_sha256': products[name]['sha256'],
                   'expected_size': products[name]['size'], 'created_before_call': False,
                   'started_utc': S.stamp(), 'stage': 'before_import', 'factory': 'TextureFactory',
                   'settings_verified': False, 'dimensions_verified': False, 'save_called': False}
        S.write(receipt_path, receipt)
        task = S.ue().AssetImportTask()
        for key, value in {'filename': products[name]['png'], 'destination_path': ART_FOLDER,
                           'destination_name': name, 'automated': True, 'replace_existing': False, 'save': False,
                           'factory': S.ue().TextureFactory()}.items():
            task.set_editor_property(key, value)
        S.ue().AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        imported_paths = [str(p) for p in task.get_editor_property('imported_object_paths')]
        S.require(imported_paths and all(p.split('.')[0] == path for p in imported_paths),
                  'Unexpected texture import result; inspect ' + str(receipt_path))
        texture = S.ue().load_asset(path)
        S.require(isinstance(texture, S.ue().Texture2D), 'Texture import failed: ' + path)
        import_data = texture.get_editor_property('asset_import_data')
        source_files = [str(p) for p in import_data.extract_filenames()] if import_data else []
        S.require(len(source_files) == 1 and Path(source_files[0]).resolve() == Path(products[name]['png']).resolve(),
                  'Imported source identity differs; inspect ' + str(receipt_path))
        # Ownership is recorded after source/identity validation, before any
        # settings-triggered async rebuild. No adoption path exists for unowned assets.
        S.ue().EditorAssetLibrary.set_metadata_tag(texture, OWNER_KEY, OWNER)
        S.ue().EditorAssetLibrary.set_metadata_tag(texture, SOURCE_HASH_KEY, products[name]['sha256'])
        receipt.update(stage='created_identity_verified', imported_object_paths=imported_paths,
                       import_source_filenames=source_files)
        S.write(receipt_path, receipt)
        expected_settings = {'compression_settings': S.ue().TextureCompressionSettings.TC_EDITOR_ICON,
                           'lod_group': S.ue().TextureGroup.TEXTUREGROUP_UI, 'srgb': True,
                           'mip_gen_settings': S.ue().TextureMipGenSettings.TMGS_NO_MIPMAPS,
                           'address_x': S.ue().TextureAddress.TA_WRAP if name == 'T_FN_CompassStrip' else S.ue().TextureAddress.TA_CLAMP,
                           'address_y': S.ue().TextureAddress.TA_CLAMP}
        for key, value in expected_settings.items():
            texture.set_editor_property(key, value)
        S.require(all(texture.get_editor_property(k) == v for k, v in expected_settings.items()),
                  'Texture setting readback differs: ' + path)
        receipt.update(stage='settings_verified_pending_dimensions', settings_verified=True,
                       settings={k: str(v) for k, v in expected_settings.items()},
                       next='Yield this editor call, then run verify(); dimensions remain mandatory.')
        S.write(receipt_path, receipt)
        imported.append(path)
    remaining = [n for n in ART_NAMES if not S.ue().EditorAssetLibrary.does_asset_exist(ART_FOLDER + '/' + n)]
    gc.collect()
    return S.write(OUT / 'art-import.json', {'imported_this_call': imported, 'pending': remaining,
                                           'complete': not remaining, 'dimensions_verified': False,
                                           'next': 'Run verify() in a separate editor call after yielding.', 'save_called': False})


def bind_map_art():
    path = ART_FOLDER + '/T_FN_Map_L001'
    texture = S.ue().load_asset(path)
    S.require(isinstance(texture, S.ue().Texture2D) and
              S.ue().EditorAssetLibrary.get_metadata_tag(texture, OWNER_KEY) == OWNER, 'Import owned map art first')
    result = _style(DEFINITION)
    current = capture(DEFINITION)['data']['values']
    original = baseline(DEFINITION)['data']['values']
    S.require({k: v for k, v in current.items() if k != 'mapTexture'} ==
              {k: v for k, v in original.items() if k != 'mapTexture'}, 'Map identity/calibration changed')
    result['native_compile_after_return'] = []
    return S.write(OUT / 'map-art-bound.json', result)


def _new_props(row, changes, slot=False):
    obj = S.resolve(row['slot' if slot else 'widget'])
    before = S.read_object(obj)
    S._validate_patch(before['schema'], changes)
    S.require(S.ue().ToolsetLibrary.set_object_properties(obj, json.dumps(changes)), 'New selection widget write failed')
    after = S.read_object(obj)['values']
    S.require(S._contains(after, changes), 'Selection widget readback mismatch')


def selection_indicator():
    S.idle()
    S.require((OUT / 'preflight.json').is_file(), 'Run preflight() before wrapping')
    _assert_compatible(ENTRY, capture(ENTRY))
    r = rows(ENTRY)
    for name in ('EntryButton', 'EntryLayout', 'TitleText', 'SubtitleText'):
        S.require(name in r, 'Required journal binding missing: ' + name)
    if 'FNEntryOverlay' not in r:
        S.require(not isinstance(r['EntryButton'].get('parent'), dict), 'EntryButton is no longer the root')
        S.write(OUT / 'selection-progress.json', {'stage': 'before_wrap', 'tree': tree(ENTRY)})
        gc.collect()  # no Python UObject wrappers are held across the native structural operation
        made = call('WrapWidgets', widgetBlueprint=S.ref(ENTRY), widgets=[r['EntryButton']['widget']],
                    wrapperClass={'refPath': '/Script/UMG.Overlay'})
        S.require(isinstance(made, list) and len(made) == 1, 'Unexpected wrapper result; inspect before retry')
        wrapper = call('RenameWidget', widgetBlueprint=S.ref(ENTRY), widget=made[0]['widget'], newDisplayName='FNEntryOverlay')
        S.require(wrapper['widgetName'] == 'FNEntryOverlay', 'Wrapper rename failed')
        S.write(OUT / 'selection-progress.json', {'stage': 'wrapped', 'tree': tree(ENTRY)})
        r = rows(ENTRY)
    S.require(r['FNEntryOverlay']['widgetClassPath'] == {'refPath': '/Script/UMG.Overlay'}, 'Wrong wrapper class')
    _new_props(r['FNEntryOverlay'], {'visibility': 'SelfHitTestInvisible'})
    _new_props(r['EntryButton'], {'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Fill',
                                  'padding': {'left': 0, 'top': 0, 'right': 0, 'bottom': 0}}, slot=True)
    for name, cls, parent in (('FNSelectionWidth', 'SizeBox', 'FNEntryOverlay'),
                              ('SelectionIndicator', 'Border', 'FNSelectionWidth')):
        if name not in r:
            made = call('AddWidget', widgetBlueprint=S.ref(ENTRY), widgetClass={'refPath': '/Script/UMG.' + cls},
                        widgetDisplayName=name, parentWidget=r[parent]['widget'])
            S.require(made['widgetName'] == name, 'Selection name collision; inspect before retry')
            r = rows(ENTRY)
        S.require(r[name]['widgetClassPath'] == {'refPath': '/Script/UMG.' + cls} and
                  r[name]['parent'] == r[parent]['widget'], 'Selection node class/parent mismatch')
        S.write(OUT / 'selection-progress.json', {'stage': name, 'tree': tree(ENTRY)})
    _new_props(r['FNSelectionWidth'], {'widthOverride': 4, 'bOverride_WidthOverride': True,
                                      'visibility': 'HitTestInvisible'})
    _new_props(r['FNSelectionWidth'], {'horizontalAlignment': 'HAlign_Left', 'verticalAlignment': 'VAlign_Fill',
                                      'padding': {'left': 0, 'top': 0, 'right': 0, 'bottom': 0}}, slot=True)
    _new_props(r['SelectionIndicator'], {'brushColor': _color('text'), 'visibility': 'Hidden',
                                        'background': {'drawAs': 'Image', 'resourceObject': 'None', 'resourceName': 'None',
                                                       'tintColor': S.slate({'r': 1, 'g': 1, 'b': 1, 'a': 1}),
                                                       'imageSize': {'x': 4, 'y': 1}},
                                        'padding': {'left': 0, 'top': 0, 'right': 0, 'bottom': 0}})
    _new_props(r['SelectionIndicator'], {'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Fill'}, slot=True)
    call('ToggleWidgetAsVariable', widgetBlueprint=S.ref(ENTRY), widget=r['SelectionIndicator']['widget'], bIsVariable=True)
    live = capture(ENTRY)
    _assert_compatible(ENTRY, live)
    S.require(rows(ENTRY)['SelectionIndicator']['bIsVariable'], 'Selection BindWidget variable unavailable')
    gc.collect()
    return S.write(OUT / 'selection-ready.json', {'tree': live, 'native_compile_after_return': [ENTRY], 'save_called': False})


def verify():
    result = {'targets': {}, 'textures': [], 'save_called': False, 'runtime_visual_review': 'Pending user review'}
    for package in TARGETS:
        current = capture(package)
        _assert_compatible(package, current)
        for selector, patch in planned(package).items():
            S.require(S._contains(_object_record(current, *selector)['values'], patch), 'Pilot style missing: ' + str(selector))
        result['targets'][package] = current
    if (OUT / 'selection-ready.json').exists():
        current = result['targets'][ENTRY]
        r = {x['row']['widgetName']: x for x in current['templates'] if 'widget' in x}
        S.require(set(NEW_ENTRY_WIDGETS).issubset(r), 'Selection stage was recorded but widgets are missing')
        S.require(not isinstance(r['FNEntryOverlay']['row'].get('parent'), dict), 'Journal wrapper is not the root')
        S.require(r['FNSelectionWidth']['row']['parent'] == r['FNEntryOverlay']['row']['widget'] and
                  r['SelectionIndicator']['row']['parent'] == r['FNSelectionWidth']['row']['widget'], 'Selection hierarchy differs')
        S.require(S._contains(r['FNSelectionWidth']['widget']['values'],
                            {'widthOverride': 4, 'bOverride_WidthOverride': True, 'visibility': 'HitTestInvisible'}),
                  'Selection margin no longer has its explicit 4px width')
        S.require(S._contains(r['FNSelectionWidth']['slot']['values'],
                            {'horizontalAlignment': 'HAlign_Left', 'verticalAlignment': 'VAlign_Fill'}),
                  'Selection margin is not left/full-height')
        S.require(r['SelectionIndicator']['row']['bIsVariable'] and
                  r['SelectionIndicator']['widget']['values']['visibility'] == 'Hidden', 'Selection binding/default changed')
    products = production()
    for name in ART_NAMES:
        path = ART_FOLDER + '/' + name
        texture = S.ue().load_asset(path)
        S.require(isinstance(texture, S.ue().Texture2D), 'Missing production texture: ' + path)
        expected = {'compression_settings': S.ue().TextureCompressionSettings.TC_EDITOR_ICON,
                    'lod_group': S.ue().TextureGroup.TEXTUREGROUP_UI, 'srgb': True,
                    'mip_gen_settings': S.ue().TextureMipGenSettings.TMGS_NO_MIPMAPS,
                    'address_x': S.ue().TextureAddress.TA_WRAP if name == 'T_FN_CompassStrip' else S.ue().TextureAddress.TA_CLAMP,
                    'address_y': S.ue().TextureAddress.TA_CLAMP}
        S.require(all(texture.get_editor_property(k) == v for k, v in expected.items()), 'Texture settings differ: ' + name)
        S.require(S.ue().EditorAssetLibrary.get_metadata_tag(texture, SOURCE_HASH_KEY) == products[name]['sha256'], 'Texture source changed')
        S.require([texture.blueprint_get_size_x(), texture.blueprint_get_size_y()] == products[name]['size'], 'Texture dimensions differ')
        result['textures'].append(path)
    result['explicit_save_allowlist'] = list(TARGETS) + result['textures']
    gc.collect()
    return S.write(OUT / 'verified.json', result)
