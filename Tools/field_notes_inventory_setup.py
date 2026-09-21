# @Description: Baseline-driven paper styling of the existing CHALK inventory widgets.
"""Import-inert Module 5 first pass; no editor calls on import or in --plan.

Offline: python Tools/field_notes_inventory_setup.py --plan
Editor: preflight(max_assets=1); author(max_assets=1); repeat bounded pairs;
then native compile + explicit save externally; verify().

This pass edits exact existing owned widget/CDO/slot properties only. No widget
creation, replacement, deletion, graph changes, source/config edits, compile,
save, input remapping, gameplay calls, PIE or tests. Shared overlay assets and
source packs are protected. It deliberately does NOT claim exact screen parity:
the compatible HQUI linear-vitals replacement and passive header follow-up are
listed by plan()['remaining_visual_work'] and are separate reviewed stages.
"""
from __future__ import annotations

import copy
import importlib.util
import json
import hashlib
import shutil
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'Saved/FieldNotesImplementation/Inventory'
BASELINE = ROOT / 'Saved/FieldNotesImplementation/Baseline/baseline'
PREFIX = '/Game/AZ/Blueprints/Menu/CommonInventory/'
MAIN = PREFIX + 'AZ_WBP_GameInventoryMenu'
SWITCHER = PREFIX + 'AZ_WBP_GameInventorySwitcher'
VITALS = PREFIX + 'AZ_WBPCharacterVitalsPanel'
SKILLS = PREFIX + 'AZ_WBP_CharacterSkillsPanel'
WIDGETS = PREFIX + 'CommonUI/UI/Widgets/'
GRID = WIDGETS + 'WBP_AZ_Inv_GridContainer_Horror'
POPUP = WIDGETS + 'WBP_AZ_CommonUI_ItemPopUp'
DESCRIPTION = WIDGETS + 'ItemDescription/WBP_AZ_CommonUI_ItemDescription'
KEY = 'AZ.FieldNotes.InventoryStage'
VERSION = 'field_notes_inventory_setup:v1'


def support():
    spec = importlib.util.spec_from_file_location('az_field_notes_inventory_styles', ROOT / 'Tools/field_notes_styles_setup.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def require(value, message):
    if not value:
        raise RuntimeError(message)


def canonical_hash(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=False, allow_nan=False).encode()).hexdigest()


def merge(before, patch):
    # Object references may be serialized as "None" before becoming refPath
    # objects. A structured replacement must not recurse into that scalar.
    result = copy.deepcopy(before) if isinstance(before, dict) else {}
    for key, value in patch.items():
        result[key] = merge(result.get(key, {}), value) if isinstance(value, dict) else copy.deepcopy(value)
    return result


def baseline():
    index = json.loads((BASELINE / 'manifest.json').read_text(encoding='utf-8'))
    require(index.get('complete') and not index.get('pending'), 'Finish the exact baseline before planning inventory writes')
    snapshots = {path: json.loads((BASELINE / info['receipt']).read_text(encoding='utf-8'))
                 for path, info in index['assets'].items()}
    require(all(value['asset'] == path for path, value in snapshots.items()), 'Baseline identity mismatch')
    return index, snapshots


def _references(value):
    if isinstance(value, dict):
        if isinstance(value.get('refPath'), str):
            yield value['refPath'].split('.')[0]
        for item in value.values():
            yield from _references(item)
    elif isinstance(value, list):
        for item in value:
            yield from _references(item)


def shared_reference_audit(index, snapshots):
    assets = set(snapshots)
    edges = {path: (set(_references(data)) & assets) - {path} for path, data in snapshots.items()}
    todo = [path for path in index['roots'] if '/HUD/' in path]
    overlay = set()
    while todo:
        path = todo.pop()
        if path in overlay:
            continue
        overlay.add(path)
        todo.extend(edges.get(path, set()) - overlay)
    inventory = {path for path in snapshots if path.startswith(PREFIX)}
    shared = sorted(inventory & overlay)
    require(not shared, 'Inventory widgets are shared with the overlay closure; isolate their instances first: ' + str(shared))
    return {'inventory_assets': sorted(inventory), 'protected_overlay_closure': sorted(overlay),
            'shared_owned_widgets': shared,
            'scope': 'Captured widget/CDO/class references, including parents. Source pack classes/styles are never mutated.'}


class Plan:
    def __init__(self, snapshots):
        self.s = support()
        self.snapshots = snapshots
        self.rows = {path: {item['row']['widgetName']: item for item in data['templates']}
                     for path, data in snapshots.items()}
        self.operations = {}
        self.skipped = []

    def target(self, asset, widget=None, part='widget'):
        require(asset.startswith(PREFIX), 'Outside inventory allowlist: ' + asset)
        require(asset in self.snapshots, 'Missing baseline: ' + asset)
        if widget is None:
            return self.snapshots[asset]['cdo']
        require(widget in self.rows[asset], 'Unknown widget: ' + asset + ':' + widget)
        obj = self.rows[asset][widget].get(part)
        require(isinstance(obj, dict) and obj.get('schema'), 'No concrete editable ' + part + ': ' + asset + ':' + widget)
        return obj

    def add(self, asset, patch, widget=None, part='widget'):
        obj = self.target(asset, widget, part)
        self.s._validate_patch(obj['schema'], patch)
        identity = (asset, obj['object'])
        if identity not in self.operations:
            self.operations[identity] = {'asset': asset, 'object': obj['object'], 'widget': widget,
                                          'part': 'cdo' if widget is None else part, 'patch': {},
                                          'before': obj['values'], 'schema': obj['schema']}
        self.operations[identity]['patch'] = merge(self.operations[identity]['patch'], patch)

    def optional(self, asset, patch, widget=None, part='widget'):
        """Only for optional native BindWidget placeholders, never misspelled targets."""
        if widget is not None and (widget not in self.rows[asset] or not self.rows[asset][widget].get(part, {}).get('schema')):
            self.skipped.append({'asset': asset, 'widget': widget, 'part': part,
                                 'reason': 'Baseline lists an unbound/abstract native optional field, not an editable tree object'})
            return
        self.add(asset, patch, widget, part)


def _class_ref(s, package):
    return {'refPath': s.object_path(package) + '_C'}


def _font(s, role='Body', pixels=None):
    value = copy.deepcopy(s.text_patch('Paper', role)['font'])
    if pixels is not None:
        value['size'] = pixels * 0.75
    return value


def _margin(left=0, top=0, right=0, bottom=0):
    return dict(left=left, top=top, right=right, bottom=bottom)


def _rect(left, top, right, bottom):
    return {'layoutData': {'offsets': _margin(left, top, right, bottom),
                          'anchors': {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 1, 'y': 1}},
                          'alignment': {'x': 0, 'y': 0}}, 'bAutoSize': False}


def _solid(s, color, edge=None, width=0):
    return {'drawAs': 'RoundedBox', 'resourceObject': 'None', 'resourceName': 'None',
            'imageType': 'NoImage', 'tiling': 'NoTile', 'mirroring': 'NoMirror',
            'tintColor': s.slate(s.linear('Paper', color)),
            'margin': _margin(),
            'outlineSettings': {'cornerRadii': {'x': 0, 'y': 0, 'z': 0, 'w': 0},
                                'roundingType': 'FixedRadius', 'width': width,
                                'color': s.slate(s.linear('Paper', edge or color)), 'bUseBrushTransparency': False}}


def _text(p, asset, name, role='Body', color=None, pixels=None):
    item = p.rows[asset][name]
    obj = item.get('widget', {})
    if not obj.get('schema'):
        p.skipped.append({'asset': asset, 'widget': name, 'reason': 'Unbound native text placeholder'})
        return
    schema = obj['schema']
    spec = p.s.tokens()['shared_text_roles'][role]
    patch = {'font': _font(p.s, role, pixels), 'colorAndOpacity': p.s.slate(p.s.linear('Paper', color or spec['color']))}
    if item['row']['widgetClassPath']['refPath'] == '/Script/CommonUI.CommonTextBlock':
        style_path = p.s.resource_paths()[0][('Paper', role)]
        # CommonTextBlock reapplies its style on reload. A manual tint beside
        # a neutral style is temporary; use an authored accent-style variant.
        if color and color != spec['color']:
            require(role == 'Caption' and color == 'story', 'Create a reviewed shared variant for this override')
            style_path = p.s.DEST + '/Text/TS_FN_Paper_Caption_Story'
        patch['style'] = _class_ref(p.s, style_path)
    if 'shadowColorAndOpacity' in schema:
        patch['shadowColorAndOpacity'] = {'r': 0, 'g': 0, 'b': 0, 'a': 0}
    if 'shadowOffset' in schema:
        patch['shadowOffset'] = {'x': 0, 'y': 0}
    p.add(asset, patch, name)


def _generic_styles(p, assets):
    s = p.s
    text_paths, buttons = s.resource_paths()
    button_ref = _class_ref(s, buttons['Paper'])
    for asset in assets:
        data = p.snapshots[asset]
        cdo, parent = data['cdo'], data['tree']['info']['parentClass']['refPath']
        # Instance class behavior remains intact; no source CommonButtonStyle is edited.
        if 'style' in cdo['schema'] and 'CommonButtonStyle' in cdo['schema']['style'].get('title', ''):
            p.add(asset, {'style': button_ref})
        # Native leaf TextStyle/LabelStyle/ValueStyle are EditInstanceOnly.
        # Their WidgetTree instances below are writable; their class CDOs are
        # intentionally excluded. Actual CommonTextBlock.style remains styled.
        for name, item in p.rows[asset].items():
            obj = item.get('widget', {})
            schema, values = obj.get('schema', {}), obj.get('values', {})
            cls = item['row']['widgetClassPath']['refPath']
            if cls in ('/Script/UMG.TextBlock', '/Script/CommonUI.CommonTextBlock'):
                if name in ('SectionLabelText', 'DetailsHeaderText'):
                    role = 'SectionTitle'
                elif name.endswith('NameText'):
                    role = 'Body'
                elif name.endswith('BonusText') or name == 'ActiveSelectionLabelText':
                    role = 'Muted'
                elif name in ('Text_StackCount', 'Text_SplitAmount'):
                    role = 'Caption'
                elif name == 'Text_ActionName':
                    role = 'Button'
                else:
                    role = 'Body'
                _text(p, asset, name, role)
            if 'style' in schema and 'CommonButtonStyle' in schema['style'].get('title', ''):
                p.add(asset, {'style': button_ref}, name)
            # Composite child overrides are important: changing the shared leaf
            # CDO alone would leave their explicitly assigned vendor styles.
            if 'textStyle' in schema:
                role = 'Body'
                if asset.endswith('/WBP_Inv_CommonUI_Item_Name_Composit'):
                    role = 'SectionTitle' if name == 'WBP_Inv_CommonUI_Leaf_Text' else 'Muted'
                elif any(token in str(values.get('textStyle', {})) for token in ('Small', 'Minus', 'Plus')):
                    role = 'Caption'
                p.add(asset, {'textStyle': _class_ref(s, text_paths[('Paper', role)])}, name)
            for key in ('labelStyle', 'valueStyle'):
                if key in schema:
                    p.add(asset, {key: _class_ref(s, text_paths[('Paper', 'Muted' if key == 'labelStyle' else 'Body')])}, name)


def _main_layout(p):
    s = p.s
    p.add(MAIN, {'background': _solid(s, 'background'), 'brushColor': {'r': 1, 'g': 1, 'b': 1, 'a': 1}}, 'BackgroundBorder')
    # Preserve the old Image objects, but replace only their decorative brushes.
    p.add(MAIN, {'brush': _solid(s, 'background'), 'colorAndOpacity': {'r': 1, 'g': 1, 'b': 1, 'a': 1}}, 'Image_409')
    p.add(MAIN, {'brush': _solid(s, 'edge'), 'colorAndOpacity': {'r': 1, 'g': 1, 'b': 1, 'a': 1}}, 'Image_11')
    p.add(MAIN, {'layoutData': {'offsets': _margin(56, 105, 56, 1),
                              'anchors': {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 1, 'y': 0}},
                              'alignment': {'x': 0, 'y': 0}}, 'bAutoSize': False}, 'Image_11', 'slot')
    p.add(MAIN, {'blurStrength': 0}, 'BackgroundBlur_104')
    p.add(MAIN, _rect(56, 150, 56, 120), 'HorizontalBox_300', 'slot')
    # Available1808 at1920:405+49 gap+841+49 gap+464. Actual data/grid stays11x7.
    for name, weight, gap in [('AZ_WBPCharacterVitalsPanel', 405, 49), ('InventorySwitcherPanel', 841, 49),
                              ('AZ_WBP_CharacterSkillsPanel', 464, 0)]:
        p.add(MAIN, {'size': {'value': weight, 'sizeRule': 'Fill'}, 'padding': _margin(0, 0, gap, 0)}, name, 'slot')
    vitals = {'healthFillColor': s.linear('Paper', 'text'), 'criticalHealthFillColor': s.linear('Paper', 'danger')}
    p.add(VITALS, vitals)
    p.add(MAIN, vitals, 'AZ_WBPCharacterVitalsPanel')


def _center_layout(p):
    s = p.s
    p.add(SWITCHER, {'padding': _margin(0, 0, 0, 24)}, 'CurrencyBarHBox', 'slot')
    p.add(SWITCHER, {'padding': _margin()}, 'SectionLabelText', 'slot')
    p.add(SWITCHER, {'padding': _margin(0, 0, 0, 6)}, 'CurrencyNameText', 'slot')
    p.add(SWITCHER, {'brush': _solid(s, 'panel'), 'colorAndOpacity': {'r': 1, 'g': 1, 'b': 1, 'a': 1}}, 'CurrencyBarBgImage')
    p.add(SWITCHER, {'colorAndOpacity': s.linear('Paper', 'muted')}, 'CurrencyIcon')
    p.add(SWITCHER, {'size': {'value': 1, 'sizeRule': 'Automatic'}, 'padding': _margin(0, 8, 0, 12)}, 'HorizontalBoxMenuTabs', 'slot')
    p.add(SWITCHER, {'padding': _margin(0, 0, 0, 10)}, 'CurrencyContentHBox_1', 'slot')
    p.add(SWITCHER, {'size': {'value': 1, 'sizeRule': 'Automatic'}, 'padding': _margin(0, 0, 0, 18)}, 'ContentSwitcherHBox', 'slot')
    p.add(SWITCHER, {'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Top'}, 'InventoryGridSwitcher', 'slot')
    p.add(SWITCHER, {'size': {'value': 1, 'sizeRule': 'Fill'}, 'padding': _margin(8, 0, 8, 0)}, 'ItemDescriptionHBox', 'slot')
    for name in ('TabSeparatorImage', 'TabSeparatorImage_0', 'TabSeparatorImage_1', 'TabSeparatorImage_2', 'TabSeparatorImage_3'):
        p.add(SWITCHER, {'colorAndOpacity': s.linear('Paper', 'edge')}, name)
    # The viewport is400high while the actual unscaled11x7 grid is550x350.
    # These are UI container heights, not row/column count or item footprints.
    sizing = {'contentHeight': 400, 'gridContainerHeight': 400,
              'barNormalColor': s.linear('Paper', 'edge', 0.65), 'barTransparentColor': s.linear('Paper', 'edge', 0),
              'barHighlightedColor': s.linear('Paper', 'text'), 'barDragColor': s.linear('Paper', 'text')}
    p.add(GRID, sizing)
    for name in ('Grid_Equippables', 'Grid_Consumables', 'Grid_Craftables'):
        p.add(SWITCHER, sizing, name)
    p.add(GRID, {'widthOverride': 550, 'bOverride_WidthOverride': True}, 'ContentSizeBox')
    p.add(GRID, {'horizontalAlignment': 'HAlign_Center', 'verticalAlignment': 'VAlign_Top'}, 'GridContainerBorder', 'slot')
    p.add(GRID, {'padding': _margin(0, 4, 0, 4)}, 'ItemsScrollBox', 'slot')
    p.add(DESCRIPTION, {'widthOverride': 810, 'bOverride_WidthOverride': False}, 'SizeBox')
    for name in ('TopSeparatorImage', 'TopSeparatorImage_1'):
        p.add(DESCRIPTION, {'brush': _solid(s, 'edge'), 'colorAndOpacity': {'r': 1, 'g': 1, 'b': 1, 'a': 1}}, name)


def _panels_and_popup(p):
    s = p.s
    # Existing circular meters remain bound and receive only style changes.
    for name in ('HealthProgressBar', 'InfectionProgressBar', 'MortalityProgressBar'):
        p.add(VITALS, {'fillColorCurrent': s.linear('Paper', 'text')}, name)
    for name in ('HealthIconImage', 'InfectionIconImage', 'MortalityIconImage', 'HeartbeatImage'):
        p.add(VITALS, {'colorAndOpacity': s.linear('Paper', 'text'),
                       'brush': {'tintColor': s.slate({'r': 1, 'g': 1, 'b': 1, 'a': 1})}}, name)
    p.add(VITALS, {'colorAndOpacity': s.linear('Paper', 'edge', 0.35),
                   'brush': {'tintColor': s.slate({'r': 1, 'g': 1, 'b': 1, 'a': 1})}}, 'HeartbeatShadowImage')
    # Do not tint the actual portrait or alter its resource/aspect/binding.
    p.add(SKILLS, {'padding': _margin(0, 90, 8, 0)}, 'VerticalBox_77', 'slot')
    p.add(SKILLS, {'padding': _margin(0, 0, 10, 0)}, 'PrimarySkills', 'slot')
    p.add(SKILLS, {'horizontalAlignment': 'HAlign_Fill'}, 'SkillBarsAndUpgradeVBox', 'slot')
    p.add(SKILLS, {'padding': _margin(0, 28, 0, 0)}, 'SkillBarsVBox', 'slot')
    p.add(SKILLS, {'padding': _margin(0, 25, 10, 0)}, 'DetailsHBox', 'slot')
    for stem in ('Strength', 'Agility', 'Resilience', 'Expertise'):
        p.add(SKILLS, {'size': {'x': 385, 'y': 7}, 'fillColorCurrent': s.linear('Paper', 'text'),
                       'backgroundTint': s.linear('Paper', 'edge', 0.6)}, stem + 'Progress')
        p.add(SKILLS, {'heightOverride': 105}, stem + 'SizeBox')
        p.add(SKILLS, {'colorAndOpacity': s.linear('Paper', 'muted')}, stem + 'IconImage')
        p.add(SKILLS, {'renderOpacity': 0}, stem + 'ShineImage')
    for name in ('UpgradeNotificationBgImage', 'DetailsHeaderBgImage'):
        p.add(SKILLS, {'brush': _solid(s, 'panel'), 'colorAndOpacity': {'r': 1, 'g': 1, 'b': 1, 'a': 1}}, name)
    _text(p, SKILLS, 'UpgradeNotificationText', 'Caption', 'story')
    # Popup availability/labels remain native item-dependent. Only inherited
    # CommonButton style refs and the actual quantity slider/text are styled.
    p.add(POPUP, {'widthOverride': 320, 'heightOverride': 300}, 'SizeBox_Root')
    p.add(POPUP, {'sliderBarColor': s.linear('Paper', 'edge'), 'sliderHandleColor': s.linear('Paper', 'text')}, 'Slider_Split')
    for asset in (WIDGETS + 'WBP_CommonUI_SlottedItem', WIDGETS + 'WBP_AZ_CommonUI_HoverItem',
                  WIDGETS + 'ItemDescription/WBP_Inv_CommonUI_Leaf_Icon'):
        p.add(asset, {'colorAndOpacity': s.linear('Paper', 'text')}, 'Image_Icon')


def plan():
    """Pure file reads; validates every intended property against the captured schema."""
    index, snapshots = baseline()
    reference_audit = shared_reference_audit(index, snapshots)
    p = Plan(snapshots)
    grid = snapshots[GRID]['cdo']['values']
    require(grid['gridSize'] == {'x': 11, 'y': 7} and grid['tileSize'] == 50,
            'Reviewed grid geometry changed; derive a new visual fit instead of changing capacity')
    _generic_styles(p, reference_audit['inventory_assets'])
    _main_layout(p)
    _center_layout(p)
    _panels_and_popup(p)
    operations = list(p.operations.values())
    for op in operations:
        forbidden = {'gridSize', 'tileSize', 'itemCategory', 'fragmentTag', 'text', 'previewText',
                     'bShowUnsupportedVitals', 'bShowUnsupportedSkills', 'inputMapping', 'mapPageClass'}
        require(not forbidden.intersection(op['patch']), 'Gameplay/text/template data entered a style patch: ' + op['object'])
        p.s._validate_patch(op['schema'], op['patch'])
    # Topological child-before-owner order, not directory-depth order: Composits
    # live deeper than their shared leaf but must still be compiled after it.
    targets = {op['asset'] for op in operations}
    order, visiting = [], set()
    def visit(path):
        if path in order:
            return
        require(path not in visiting, 'Unexpected cyclic owned-widget dependency: ' + path)
        visiting.add(path)
        for child in sorted((set(_references(snapshots[path])) & targets) - {path}):
            visit(child)
        visiting.remove(path)
        order.append(path)
    for path in sorted(targets):
        visit(path)
    result = {'version': VERSION, 'family': '02_FIELD_NOTES', 'token_sha256': p.s.digest(p.s.TOKENS),
              'baseline_manifest_sha256': p.s.digest(BASELINE / 'manifest.json'), 'asset_order': order,
              'operations': operations, 'reference_audit': reference_audit, 'skipped_optional_fields': p.skipped,
              'preserved_grid': {'columns': 11, 'rows': 7, 'tile_size': 50, 'item_footprints': 'unchanged'},
              'remaining_visual_work': [
                  'Add passive CHALK/page-title/header/footer labels after this preserved-tree pilot.',
                  'Replace or configure circular vitals through verified HQUI-compatible PB_SetPercent contract; retain native ApplyHealthPresentation.',
                  'Confirm skill text/meter containment and popup resizing with the actual DPI; no mockup values are assigned.',
                  'Check paper tint on full-color consumable icons; resources and alpha/aspect remain unchanged.',
                  'Complete dynamic device prompt module separately; existing CommonActionWidget/input bindings are not touched.',
              ], 'module5_complete': False, 'runtime_verified': False}
    result['plan_sha256'] = canonical_hash({key: value for key, value in result.items() if key != 'plan_sha256'})
    return result


def write_plan():
    result = plan()
    support().write(OUT / 'inventory-plan.json', result)
    return {key: result[key] for key in ('plan_sha256', 'asset_order', 'preserved_grid', 'reference_audit',
                                       'skipped_optional_fields', 'remaining_visual_work', 'module5_complete')}


def _progress(planned):
    file = OUT / 'author-progress.json'
    if not file.exists():
        return {'version': VERSION, 'plan_sha256': planned['plan_sha256'], 'assets': {}, 'saved': False}
    value = json.loads(file.read_text(encoding='utf-8'))
    require(value['version'] == VERSION and value['plan_sha256'] == planned['plan_sha256'], 'Plan changed since authoring; review before restarting')
    return value


def _tree_identity(tree):
    return [(row['widgetName'], row['widgetClassPath'], row.get('parent'), row.get('slot'), row.get('bIsVariable'))
            for row in tree['widgets']]


def _exact_value(actual, expected):
    if isinstance(expected, dict):
        return isinstance(actual, dict) and set(actual) == set(expected) and all(_exact_value(actual[key], value) for key, value in expected.items())
    if isinstance(expected, list):
        return isinstance(actual, list) and len(actual) == len(expected) and all(_exact_value(a, b) for a, b in zip(actual, expected))
    if isinstance(expected, (int, float)) and not isinstance(expected, bool):
        return isinstance(actual, (int, float)) and abs(actual - expected) < 0.00001
    return actual == expected


def _approved_values(actual, original, desired, allow_original):
    """Permit only baseline or the exact planned value, down to changed leaves."""
    if _exact_value(actual, desired) or (allow_original and _exact_value(actual, original)):
        return True
    if isinstance(original, dict) and isinstance(desired, dict) and isinstance(actual, dict):
        return set(actual) == set(original) == set(desired) and all(
            _approved_values(actual[key], original[key], desired[key], allow_original) for key in original)
    return False


def _accepted_saved_hash(asset, snapshots):
    original = snapshots[asset]['saved_package_sha256']
    file = OUT / 'accepted-resaves.json'
    if not file.exists():
        return original
    data = json.loads(file.read_text(encoding='utf-8'))
    record = data.get('assets', {}).get(asset)
    if not record:
        return original
    require(record['original_baseline_sha256'] == original
            and record['baseline_manifest_sha256'] == support().digest(BASELINE / 'manifest.json'),
            'Accepted resave belongs to another baseline: ' + asset)
    return record['accepted_saved_sha256']


def _require_live_baseline(asset, planned, snapshots, completed=False, check_saved_hash=True):
    s = support()
    bp, _ = s.defaults(asset)
    current_tree = s.native_tool('UMGToolSet.UMGToolSet.GetWidgets', widgetBlueprint=s.ref(asset))
    require(_tree_identity(current_tree) == _tree_identity(snapshots[asset]['tree']), 'Widget identity/tree changed: ' + asset)
    if not completed and check_saved_hash:
        require(s.digest(s.package_file(asset)) == _accepted_saved_hash(asset, snapshots), 'Saved asset changed since baseline/verified resave: ' + asset)
    # Read ALL concrete objects, not only planned write targets. This proves
    # that a pending parent dirtied by child compilation has no unapproved
    # template/CDO/slot changes before accepting its dirty state.
    objects = {snapshots[asset]['cdo']['object']: snapshots[asset]['cdo']}
    for item in snapshots[asset]['templates']:
        for kind in ('widget', 'slot'):
            if isinstance(item.get(kind), dict) and item[kind].get('object'):
                objects[item[kind]['object']] = item[kind]
    operations = {value['object']: value for value in planned['operations'] if value['asset'] == asset}
    before = []
    for path, baseline_object in objects.items():
        live = s.read_object(s.resolve(path))
        patch = operations.get(path, {}).get('patch', {})
        s._validate_patch(live['schema'], patch)
        original = baseline_object['values']
        desired = merge(original, patch)
        # These fields are derived from the assigned CommonTextStyle during
        # UpdateFromStyle (CommonTextBlock.cpp), including reload/parent compile.
        # Compare with the selected style CDO rather than stale template fields.
        # This is restricted to an explicitly planned CommonTextBlock style.
        if patch.get('style') and 'Class@/Script/CommonUI.CommonTextStyle' == live['schema'].get('style', {}).get('title') and 'strikeBrush' in desired:
            style_class = s.resolve(patch['style'])
            style_values = s.read_object(s.ue().get_default_object(style_class))['values']
            for key in ('font', 'strikeBrush', 'margin', 'lineHeightPercentage', 'applyLineHeightToBottomLine'):
                desired[key] = copy.deepcopy(style_values[key])
            desired['colorAndOpacity'] = s.slate(style_values['color'])
            desired['shadowOffset'] = copy.deepcopy(style_values['shadowOffset']) if style_values['bUsesDropShadow'] else {'x': 0, 'y': 0}
            desired['shadowColorAndOpacity'] = copy.deepcopy(style_values['shadowColor']) if style_values['bUsesDropShadow'] else {'r': 0, 'g': 0, 'b': 0, 'a': 0}
        require(set(live['values']) == set(original), 'Live property set changed: ' + path)
        for key, baseline_value in original.items():
            require(_approved_values(live['values'][key], baseline_value, desired[key], not completed),
                    'Unapproved live difference at ' + path + '.' + key)
        before.append(live)
    return bp, before


def acknowledge_verified_resaves():
    """Acknowledge only the three root-verified Play-triggered editor resaves.

    This is a read-only UObject audit plus file backup/receipts. It never saves,
    resets or modifies any live property/graph/dirty flag. Full current tree and
    EVERY concrete CDO/widget/slot property must match baseline-or-exact-plan.
    The refused leaf additionally must be entirely baseline-exact.
    """
    s = support(); s.idle()
    old = json.loads((OUT / 'inventory-plan.json').read_text(encoding='utf-8'))
    progress = json.loads((OUT / 'author-progress.json').read_text(encoding='utf-8'))
    require(old['plan_sha256'] == progress['plan_sha256'], 'Use the current attempted plan for resave proof')
    _, snapshots = baseline()
    require(old['baseline_manifest_sha256'] == s.digest(BASELINE / 'manifest.json'), 'Baseline changed since attempted authoring')
    failed = WIDGETS + 'ItemDescription/EditInstanceOnly/WBP_Inv_CommonUI_Leaf_Text'
    known = (MAIN, SWITCHER, failed)
    existing_file = OUT / 'accepted-resaves.json'
    accepted = json.loads(existing_file.read_text(encoding='utf-8')) if existing_file.exists() else {'assets': {}, 'history': []}
    verified = []
    for asset in known:
        require(progress['assets'].get(asset, {}).get('state') != 'authored', 'Resave recovery is only for the known pending packages')
        _, current = _require_live_baseline(asset, old, snapshots, check_saved_hash=False)
        if asset == failed:
            original = {snapshots[asset]['cdo']['object']: snapshots[asset]['cdo']['values']}
            for item in snapshots[asset]['templates']:
                for kind in ('widget', 'slot'):
                    if isinstance(item.get(kind), dict) and item[kind].get('object'):
                        original[item[kind]['object']] = item[kind]['values']
            require(all(_exact_value(obj['values'], original[obj['object']]) for obj in current),
                    'Refused leaf is not wholly baseline-exact; cannot acknowledge/recover this attempt')
        disk_hash = s.digest(s.package_file(asset))
        verified.append((asset, current, disk_hash))
    # Every package is proven BEFORE accepting any changed hash.
    backup = OUT / 'VerifiedResaves' / s.stamp()
    s.write(backup / 'previous-accepted-resaves.json', accepted)
    records = []
    for asset, current, disk_hash in verified:
        relative = Path(asset[len('/Game/'):])
        destination = backup / relative
        destination.mkdir(parents=True)
        shutil.copy2(s.package_file(asset), destination / s.package_file(asset).name)
        s.write(destination / 'verified-live-values.json', current)
        record = {'asset': asset, 'original_baseline_sha256': snapshots[asset]['saved_package_sha256'],
                  'previous_accepted_sha256': accepted['assets'].get(asset, {}).get('accepted_saved_sha256', snapshots[asset]['saved_package_sha256']),
                  'accepted_saved_sha256': disk_hash, 'baseline_manifest_sha256': s.digest(BASELINE / 'manifest.json'),
                  'proof_plan_sha256': old['plan_sha256'], 'objects_verified': len(current), 'backup': str(destination),
                  'verification': 'Full tree and all concrete properties baseline-or-exact-planned; failed leaf baseline-exact',
                  'root_evidence': 'Saved/Logs/AZ.log: user Play/Stop editor resaves Main+Switcher01:57:04 and refusedLeaf01:58:53',
                  'uobject_or_graph_writes': False, 'dirty_flags_cleared': False}
        records.append(record)
        accepted['assets'][asset] = record
    accepted['history'].append({'receipt': str(backup), 'records': records})
    s.write(backup / 'acknowledged.json', records)
    s.write(existing_file, accepted)
    return {'acknowledged': records, 'next': 'migrate_instance_only_cdo_plan()', 'live_mutations': False}


def _dirty_provenance(asset, before, snapshots, planned, progress, dirty):
    original = {snapshots[asset]['cdo']['object']: snapshots[asset]['cdo']['values']}
    for item in snapshots[asset]['templates']:
        for kind in ('widget', 'slot'):
            if isinstance(item.get(kind), dict) and item[kind].get('object'):
                original[item[kind]['object']] = item[kind]['values']
    differences = [{'object': obj['object'], 'property': key,
                    'before': original[obj['object']][key], 'observed': value}
                   for obj in before for key, value in obj['values'].items()
                   if not _exact_value(value, original[obj['object']][key])]
    recovered = [record for record in progress.get('recoveries', []) if record.get('asset') == asset]
    origin = ('Recovered refused first property write; full unchanged-value proof recorded by plan migration' if recovered
              else 'Consistent with inherited defaults after prior child compile; supplied root continuation confirms that origin')
    return {'asset': asset, 'was_dirty': asset in dirty, 'saved_hash_matches_baseline_or_verified_resave': True,
            'expected_saved_sha256': _accepted_saved_hash(asset, snapshots),
            'validation': 'All concrete CDO/widget/slot properties and full tree match baseline-or-exact-plan; no unapproved differences accepted',
            'differences': differences, 'objects_verified': len(before),
            'prior_authored_dependency_candidates': [path for path, value in progress['assets'].items() if value.get('state') == 'authored'],
            'dirty_origin': origin if asset in dirty else 'clean',
            'dirty_flag_cleared': False, 'plan_sha256': planned['plan_sha256']}


def migrate_instance_only_cdo_plan():
    """Narrow recovery for the confirmed, refused first leaf-CDO property write.

    Run this BEFORE --plan/write_plan overwrites the prior plan receipt. Does
    not write any UObject/package or clear dirty flags. Removes only the three
    known EditInstanceOnly CDO fields from pending operations, preserves every
    completed asset, and requires the failed leaf to be wholly baseline-exact.
    """
    s = support(); s.idle()
    old_file = OUT / 'inventory-plan.json'
    old = json.loads(old_file.read_text(encoding='utf-8'))
    progress = json.loads((OUT / 'author-progress.json').read_text(encoding='utf-8'))
    prior_preflight = json.loads((OUT / 'preflight.json').read_text(encoding='utf-8'))
    new = plan()
    require(old['plan_sha256'] == progress['plan_sha256'] == prior_preflight['plan_sha256'],
            'Old plan/progress/preflight receipts do not describe the same attempted plan')
    require(old['token_sha256'] == new['token_sha256'] and old['baseline_manifest_sha256'] == new['baseline_manifest_sha256'],
            'Tokens/baseline changed; this recovery cannot authorize unrelated plan changes')
    failed = WIDGETS + 'ItemDescription/EditInstanceOnly/WBP_Inv_CommonUI_Leaf_Text'
    writing = [asset for asset, record in progress['assets'].items() if record.get('state') != 'authored']
    require(writing == [failed] and progress['assets'][failed]['state'] == 'writing',
            'Not the exact confirmed refused leaf-CDO first write')
    allowed = {'textStyle', 'labelStyle', 'valueStyle'}
    identity = lambda op: (op['asset'], op['object'])
    old_ops, new_ops = ({identity(op): op for op in doc['operations']} for doc in (old, new))
    require(set(new_ops) <= set(old_ops), 'Recovery introduced new write targets')
    removed = []
    for key, old_op in old_ops.items():
        new_op = new_ops.get(key)
        old_patch, new_patch = old_op['patch'], new_op['patch'] if new_op else {}
        deleted = set(old_patch) - set(new_patch)
        require(not (set(new_patch) - set(old_patch)), 'Recovery introduced new properties')
        require(all(_exact_value(value, old_patch[name]) for name, value in new_patch.items()), 'Recovery altered a retained property value')
        if deleted:
            require(old_op['part'] == 'cdo' and deleted <= allowed, 'Recovery removes something other than known instance-only CDO fields')
            require(progress['assets'].get(old_op['asset'], {}).get('state') != 'authored', 'Recovery would change an already authored asset')
            obj = s.read_object(s.resolve(old_op['object']))
            require(all(_exact_value(obj['values'][name], old_op['before'][name]) for name in deleted),
                    'A removed CDO property was already modified; recovery is not applicable')
            removed.append({'asset': old_op['asset'], 'object': old_op['object'], 'fields': sorted(deleted),
                            'unchanged_values': {name: obj['values'][name] for name in deleted}})
        if new_op:
            require(_exact_value(new_op['before'], old_op['before']) and new_op['part'] == old_op['part'],
                    'Recovery changed baseline identity/data')
    require(removed, 'No instance-only CDO removals found; inspect instead of resetting progress')
    _, snapshots = baseline()
    _, current = _require_live_baseline(failed, new, snapshots)
    originals = {snapshots[failed]['cdo']['object']: snapshots[failed]['cdo']['values']}
    for item in snapshots[failed]['templates']:
        for kind in ('widget', 'slot'):
            if isinstance(item.get(kind), dict) and item[kind].get('object'):
                originals[item[kind]['object']] = item[kind]['values']
    require(all(_exact_value(obj['values'], originals[obj['object']]) for obj in current),
            'Failed leaf has a property change; first-write/no-mutation recovery is not applicable')
    backup = OUT / 'PlanMigrations' / s.stamp()
    s.write(backup / 'old-plan.json', old)
    s.write(backup / 'old-progress.json', progress)
    s.write(backup / 'old-preflight.json', prior_preflight)
    s.write(backup / 'failed-leaf-current.json', current)
    recovered_entry = progress['assets'].pop(failed)
    recovery = {'asset': failed, 'reason': 'EditInstanceOnly CDO assignment refused before any field write',
                'original_attempt': recovered_entry, 'full_live_baseline_exact': True,
                'old_plan_sha256': old['plan_sha256'], 'new_plan_sha256': new['plan_sha256'], 'receipt': str(backup)}
    progress.setdefault('recoveries', []).append(recovery)
    progress['plan_sha256'] = new['plan_sha256']
    completed = [asset for asset, record in progress['assets'].items() if record.get('state') == 'authored']
    next_preflight = {'plan_sha256': new['plan_sha256'], 'verified': completed,
                     'remaining_preflight': [asset for asset in new['asset_order'] if asset not in completed],
                     'protected_overlay_closure': new['reference_audit']['protected_overlay_closure'],
                     'mutations': False, 'migration': str(backup)}
    receipt = {'removed_only_unapplied_cdo_fields': removed, 'completed_assets_preserved': completed,
               'recovered_failed_asset': failed, 'uobject_writes': False, 'dirty_flags_cleared': False,
               'old_plan_sha256': old['plan_sha256'], 'new_plan_sha256': new['plan_sha256']}
    s.write(backup / 'migration.json', receipt)
    s.write(old_file, new)
    s.write(OUT / 'author-progress.json', progress)
    s.write(OUT / 'preflight.json', next_preflight)
    return receipt


def preflight(max_assets=1):
    require(1 <= max_assets <= 3, 'Use1–3assets per preflight call')
    s = support()
    s.idle()
    planned = plan()
    _, snapshots = baseline()
    progress = _progress(planned)
    dirty = {p.get_name() for p in s.ue().EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    prior_file = OUT / 'preflight.json'
    prior = json.loads(prior_file.read_text(encoding='utf-8')) if prior_file.exists() else {}
    require(not prior or prior['plan_sha256'] == planned['plan_sha256'], 'Preflight plan changed; inspect receipt before restarting')
    verified = prior.get('verified', [])
    pending = [asset for asset in planned['asset_order'] if progress['assets'].get(asset, {}).get('state') != 'authored'
               and asset not in verified]
    # Resource APIs verify actual native styles/Default font faces, not existence alone.
    s.verify_resources()
    provenance = prior.get('inherited_dirty_provenance', {})
    for asset in pending[:max_assets]:
        require(asset not in progress['assets'], 'Interrupted asset authoring requires inspection: ' + asset)
        _, before = _require_live_baseline(asset, planned, snapshots)
        if asset in dirty:
            provenance[asset] = _dirty_provenance(asset, before, snapshots, planned, progress, dirty)
        verified.append(asset)
    return s.write(OUT / 'preflight.json', {'plan_sha256': planned['plan_sha256'], 'verified': verified,
                                           'remaining_preflight': pending[max_assets:],
                                           'inherited_dirty_provenance': provenance,
                                           'unverified_dirty_pending': sorted((set(pending[max_assets:]) & dirty) - set(verified)),
                                           'protected_overlay_closure': planned['reference_audit']['protected_overlay_closure'],
                                           'mutations': False})


def author(max_assets=1):
    """Bounded, per-asset staged writes. Compile/save externally after this returns."""
    require(1 <= max_assets <= 3, 'Use1–3assets per call')
    s = support()
    s.idle()
    planned = plan()
    _, snapshots = baseline()
    progress = _progress(planned)
    require((OUT / 'preflight.json').exists(), 'Run preflight() first')
    checked = json.loads((OUT / 'preflight.json').read_text(encoding='utf-8'))
    require(checked['plan_sha256'] == planned['plan_sha256'], 'Preflight is for another plan')
    pending = [asset for asset in planned['asset_order'] if progress['assets'].get(asset, {}).get('state') != 'authored']
    done = []
    for asset in pending[:max_assets]:
        s.idle()
        require(asset in checked.get('verified', []), 'Run the next bounded preflight before authoring: ' + asset)
        require(asset not in progress['assets'], 'Interrupted asset stage: inspect backup, do not retry blindly')
        dirty = {p.get_name() for p in s.ue().EditorLoadingAndSavingUtils.get_dirty_content_packages()}
        bp, before = _require_live_baseline(asset, planned, snapshots)
        provenance = _dirty_provenance(asset, before, snapshots, planned, progress, dirty)
        protected = {path: s.digest(s.package_file(path)) for path in planned['reference_audit']['protected_overlay_closure']
                     if s.package_file(path).is_file()}
        # Keep the package path in the receipt; repeating its full hierarchy
        # here exceeds Windows MAX_PATH for the deepest composite widgets.
        backup = OUT / 'BeforeWrites' / s.stamp() / hashlib.sha256(asset.encode()).hexdigest()[:12]
        backup.mkdir(parents=True)
        shutil.copy2(s.package_file(asset), backup / (asset.rsplit('/', 1)[-1] + '.uasset'))
        s.write(backup / 'live-before.json', {'asset': asset, 'objects': before, 'snapshot': snapshots[asset],
                                             'saved_package_sha256': s.digest(s.package_file(asset)), 'plan_sha256': planned['plan_sha256'],
                                             'dirty_provenance': provenance})
        progress['assets'][asset] = {'state': 'writing', 'backup': str(backup), 'dirty_provenance': provenance}
        s.write(OUT / 'author-progress.json', progress)
        bp.modify()
        for op in (value for value in planned['operations'] if value['asset'] == asset):
            obj = s.resolve(op['object'])
            obj.modify()
            require(s.ue().ToolsetLibrary.set_object_properties(obj, json.dumps(op['patch'], allow_nan=False)),
                    'Property write failed; inspect ' + str(backup))
            after = s.read_object(s.resolve(op['object']))
            require(s._contains(after['values'], op['patch']), 'Property readback mismatch: ' + op['object'])
        s.ue().EditorAssetLibrary.set_metadata_tag(bp, KEY, VERSION)
        s.ue().EditorAssetLibrary.set_metadata_tag(bp, 'AZ.FieldNotes.InventoryTokenHash', planned['token_sha256'])
        _require_live_baseline(asset, planned, snapshots, completed=True)
        require(all(s.digest(s.package_file(path)) == value for path, value in protected.items()),
                'An overlay package changed during inventory authoring; inspect before continuing')
        progress['assets'][asset]['state'] = 'authored'
        s.write(OUT / 'author-progress.json', progress)
        done.append(asset)
    return {'authored_now': done, 'remaining': len(pending) - len(done),
            'native_compile_then_explicit_save': done, 'runtime_verified': False, 'module5_complete': False}


def verify():
    """Readback after native compile/save; compare bindings/tree/protected data too."""
    s = support()
    planned = plan()
    _, snapshots = baseline()
    progress = _progress(planned)
    require(all(progress['assets'].get(asset, {}).get('state') == 'authored' for asset in planned['asset_order']), 'Author every planned asset first')
    results = []
    for asset in planned['asset_order']:
        bp, _ = _require_live_baseline(asset, planned, snapshots, completed=True)
        require(s.ue().EditorAssetLibrary.get_metadata_tag(bp, KEY) == VERSION, 'Missing authoring ownership receipt')
        results.append({'asset': asset, 'saved_package_sha256': s.digest(s.package_file(asset))})
    # Other agents may legitimately style overlays. Do not insist that their
    # hashes remain the old global baseline; our own before/after guard protects
    # them within each inventory authoring call, and no operation addresses them.
    dirty = {p.get_name() for p in s.ue().EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    pending_save = sorted(set(planned['asset_order']) & dirty)
    return s.write(OUT / 'verified.json', {'assets': results, 'pending_save': pending_save,
                                          'saved': not pending_save, 'unchanged_tree_and_unpatched_properties': True,
                                          'actual_grid_preserved': planned['preserved_grid'],
                                          'remaining_visual_work': planned['remaining_visual_work'],
                                          'runtime_verified': False, 'module5_complete': False})


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plan', action='store_true', help='Validate saved schemas and write a file-only plan')
    args = parser.parse_args()
    require(args.plan, 'Only --plan is exposed outside the editor')
    print(json.dumps(write_plan(), indent=2))
