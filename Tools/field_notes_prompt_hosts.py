# @Description: Add owned Field Notes action prompts and configure existing UI command hosts after the native build.
"""Import-inert, bounded Module7 host authoring. Root is the only editor writer.

plan() is file-only. After a full native build/restart:
  preflight()
  prepare_prompt(context); root runs returned native creation request OUTSIDE Python
  author_prompt(context); external native compile/save; repeat for other context
  host_tree(asset); external native compile/save; host_bindings(asset); external compile/save
  configure_host(QUICK); configure_host(MAP); external native compile/save
  append_shared_toggle(); external explicit save
  verify(asset) after each final external compile/save.

No compile/save/PIE/test calls are made here. Existing command assets are read-only.
Every stage checks the future native API BEFORE any mutation and backs up its
one exact asset. Interrupted stages stop; original widgets/nodes are never deleted.
"""
from __future__ import annotations

import copy
import gc
import importlib.util
import json
import shutil
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'Saved/FieldNotesImplementation/Module7/PromptHosts'
QDIR = '/Game/AZ/Blueprints/Menu/HUD/QuickSelect/'
QUICK, ENTRY, FISTS = (QDIR + n for n in ('WBP_AZ_QuickSelect', 'WBP_AZ_QuickSelectEntry', 'WBP_AZ_QuickSelectFists'))
MAP = '/Game/AZ/Blueprints/Menu/Map/WBP_AZ_QuestMapPage'
MENU = '/Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBP_GameInventoryMenu'
PC = '/Game/AZ/Blueprints/Player/BP_AZ_PlayerController'
INPUT_DATA = '/Game/AZ/Blueprints/Input/Common/BP_AZ_InventoryUIInputData'
SHARED = '/Game/AZ/Blueprints/Input/AlwaysAllowed/AZ_IMC_AlwaysAllowed'
TOGGLE = '/Game/AZ/Blueprints/Input/AlwaysAllowed/AZ_IA_QuickSelect'
COMMANDS = '/Game/AZ/Blueprints/Input/FieldNotes/'
PROMPTS = {c: '/Game/AZ/Blueprints/Menu/Common/WBP_FN_ActionPrompt_' + c for c in ('Paper', 'Overlay')}
HOSTS = (QUICK, ENTRY, FISTS, MAP, MENU)
OWNED = HOSTS + tuple(PROMPTS.values()) + (SHARED,)
OWNER_KEY, OWNER = 'AZ.FieldNotes.PromptHostOwner', 'field_notes_prompt_hosts:v3'
INVENTORY_FOOTER_PROMPTS = (('Back', 'Back'),)
MAP_SECTION_PROMPTS = (('TabLeft', 'Previous section'), ('TabRight', 'Next section'), ('Back', 'Inventory'))
QUICK_ACTIONS = ('FocusLeft', 'FocusRight', 'FocusUp', 'FocusDown', 'Activate', 'Assignment', 'PreviousCandidate', 'NextCandidate', 'Cancel')
MAP_ACTIONS = ('ZoomIn', 'ZoomOut', 'Recenter', 'PlaceWaypoint', 'ClearWaypoint', 'TrackSelection')
DESCRIPTIONS = {'FocusLeft': '', 'FocusRight': 'Focus', 'FocusUp': '', 'FocusDown': '',
                'Activate': 'Select', 'Assignment': 'Assign', 'PreviousCandidate': 'Previous',
                'NextCandidate': 'Next', 'Cancel': 'Cancel', 'ZoomIn': 'Zoom in', 'ZoomOut': 'Zoom out',
                'Recenter': 'Recenter', 'PlaceWaypoint': 'Place marker', 'ClearWaypoint': 'Clear marker',
                'TrackSelection': 'Track task'}


def module(filename):
    spec = importlib.util.spec_from_file_location('fn_prompt_' + filename.replace('.', '_'), ROOT / 'Tools' / filename)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def support():
    return module('field_notes_styles_setup.py')


def require(value, message):
    if not value:
        raise RuntimeError(message)


def prompt_creation_request(context):
    require(context in PROMPTS, 'Choose Paper or Overlay')
    asset = PROMPTS[context]
    return {'tool': 'UMGToolSet.UMGToolSet.CreateWidgetBlueprint',
            'args': {'folderPath': asset.rsplit('/', 1)[0], 'assetName': asset.rsplit('/', 1)[1],
                     'parentClass': {'refPath': '/Script/AZ.AZ_ActionPrompt'}}}


def plan():
    saved = json.loads((ROOT / 'Saved/FieldNotesImplementation/Module7/CommandAssets/saved.json').read_text())
    require(saved['saved'] and len(saved['assets']) == 18, 'Save/verify the existing command assets first')
    return {'recipe_version': OWNER, 'owned_targets': list(OWNED), 'new_prompt_templates': PROMPTS,
            'external_native_creation_requests': {context: prompt_creation_request(context) for context in PROMPTS},
            'creation_must_run_outside_python': True, 'command_assets_read_only': saved['assets'],
            'native_build_required': ['AZ_ActionPrompt.ConfigureAction', 'AZ_ActionPrompt.ConfigureBinding',
                                      'AZ_QuickSelectWidget.OnSelectorViewChanged', 'AZ_QuestMapPage.OnMapActionBindingsChanged',
                                      'AZ_Inv_CommonUI_GameInventoryMenu.NavigateInventoryTab',
                                      'AZ_Inv_CommonUI_GameInventoryMenu.IsInventoryPageActive'],
            'inventory_footer_actions': [name + 'Action' for name, _ in INVENTORY_FOOTER_PROMPTS],
            'inventory_tab_keycaps_owned_by': '/Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBP_GameInventorySwitcher',
            'map_section_actions_all_devices': {name + 'Action': label for name, label in MAP_SECTION_PROMPTS},
            'map_section_visibility': 'Lives only under MapPageRoot; existing outer page hierarchy hides it on Inventory.',
            'keyboard_mouse': 'Existing literal MMB/Wheel/Click instructions on KBM only; real mapped toggle action supplies Close glyph.',
            'no_dummy_mouse_actions': True, 'no_editor_calls': True, 'runtime_verified': False}


def gate():
    s = support(); u = s.ue(); s.idle()
    required = {'/Script/AZ.AZ_ActionPrompt': ('ConfigureAction', 'ConfigureBinding', 'ClearPrompt'),
                '/Script/AZ.AZ_QuickSelectWidget': ('GetSelectorActionBinding', 'OnSelectorViewChanged'),
                '/Script/AZ.AZ_QuestMapPage': ('GetMapActionBinding', 'OnMapActionBindingsChanged'),
                '/Script/AZ.AZ_Inv_CommonUI_GameInventoryMenu': ('NavigateInventoryTab', 'IsInventoryPageActive')}
    for owner, names in required.items():
        require(u.load_class(None, owner) is not None, 'Full native build/restart required: ' + owner)
        for name in names:
            require(u.load_object(None, owner + ':' + name) is not None or u.load_object(None, owner + '.' + name) is not None,
                    'Native function unavailable; stop before writes: ' + owner + ':' + name)
    plan()
    commands = module('field_notes_ui_commands.py')
    _, records = commands.records()
    metadata = commands.owned(COMMANDS + 'DA_FN_UICommandMetadata', u.CommonMappingContextMetadata)
    require(metadata and metadata.get_editor_property('enhanced_input_metadata').get_editor_property('is_generic_input_action'),
            'Saved generic CommonUI metadata differs')
    for group, record in records:
        action = commands.owned(record['proposed_asset'], u.InputAction)
        require(action and action.get_editor_property('value_type') == u.InputActionValueType.BOOLEAN, 'Command value type differs')
        require(action.get_editor_property('consume_input') == (group == 'quick'), 'Command consumption differs')
        require(not action.get_editor_property('consumes_action_and_axis_mappings')
                and not action.get_editor_property('triggers') and not action.get_editor_property('modifiers'), 'Command behavior differs')
        require(action.get_editor_property('player_mappable_key_settings').get_editor_property('metadata') == metadata,
                'Command generic metadata changed')
    return s


def tool(name, **args):
    require(name in ('GetWidgets', 'AddWidget', 'ToggleWidgetAsVariable'), 'Tool outside additive scope; native creation/compile cannot run from Python')
    s = support()
    result = s.ue().ToolsetRegistry.execute_tool('UMGToolSet.UMGToolSet', name, json.dumps(args))
    require(result.is_complete and not result.error, name + ': ' + str(result.error))
    return json.loads(result.value)['returnValue']


def rows(asset):
    return {r['widgetName']: r for r in tool('GetWidgets', widgetBlueprint=support().ref(asset))['widgets'] if isinstance(r.get('widget'), dict)}


def snapshot(asset):
    s = support(); bp = s.ue().load_asset(asset)
    require(bp is not None, 'Missing Blueprint snapshot target')
    tree = tool('GetWidgets', widgetBlueprint=s.ref(asset))
    objects = {'@CDO': s.read_object(s.ue().get_default_object(bp.generated_class()))} if bp.generated_class() else {}
    for row in tree['widgets']:
        for part in ('widget', 'slot'):
            if isinstance(row.get(part), dict):
                objects[row['widgetName'] + ':' + part] = s.read_object(s.resolve(row[part]))
    overlay = module('field_notes_overlay_setup.py')
    graphs = {str(graph.get_name()): overlay.graph_readback(bp, str(graph.get_name()))
              for graph in s.ue().BlueprintEditorLibrary.list_graphs(bp)}
    return {'asset': asset, 'tree': tree, 'objects': objects, 'graphs': graphs,
            'saved_sha256': s.digest(s.package_file(asset)) if s.package_file(asset).exists() else None}


def preserve_objects(before, after, patches=None):
    i = module('field_notes_inventory_setup.py'); patches = patches or {}
    for name, old in before['objects'].items():
        require(name in after['objects'], 'Preexisting template/CDO object removed: ' + name)
        desired = i.merge(old['values'], patches.get(name, {}))
        actual = after['objects'][name]['values']
        if name == '@CDO':
            added = set(actual) - set(desired)
            require(all(key.lower().startswith('fn_') and actual[key] == 'None' for key in added),
                    'Unexpected new CDO fields')
            actual = {key: value for key, value in actual.items() if key not in added}
        require(i._exact_value(actual, desired), 'Unexpected preexisting property change: ' + name)


def begin(asset, stage, new=False):
    s = gate(); require(asset in OWNED, 'Not an exact owned target')
    marker = OUT / (asset.rsplit('/', 1)[-1] + '-' + stage + '.json')
    if marker.exists():
        old = json.loads(marker.read_text())
        require(old.get('recipe_version') == OWNER, 'Older prompt-host recipe receipt; inspect its layout before migrating')
        require(old['state'] == 'complete', 'Interrupted stage; inspect its backup before recovery')
        return s, marker, None
    dirty = {p.get_name() for p in s.ue().EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    require(new or asset not in dirty, 'Externally compile/save the prior owned stage before a fresh backup: ' + asset)
    folder = OUT / 'Before' / (asset.rsplit('/', 1)[-1] + '-' + stage + '-' + s.stamp())
    folder.mkdir(parents=True)
    if s.ue().EditorAssetLibrary.does_asset_exist(asset):
        before = snapshot(asset)
        s.write(folder / 'before.json', before)
        if s.package_file(asset).is_file():
            shutil.copy2(s.package_file(asset), folder / s.package_file(asset).name)
    s.write(marker, {'asset': asset, 'stage': stage, 'state': 'writing', 'backup': str(folder), 'recipe_version': OWNER})
    return s, marker, folder


def finish(s, marker, folder, **details):
    state = json.loads(marker.read_text())
    state.update(state='complete', native_compile_save_required=True, runtime_verified=False, **details)
    s.write(folder / 'after.json', snapshot(state['asset']))
    return s.write(marker, state)


def patch(reference, changes):
    s = support(); obj = s.resolve(reference)
    require(obj.get_path_name().split('.')[0] in OWNED, 'Object outside exact owned targets')
    old = s.read_object(obj); s._validate_patch(old['schema'], changes)
    obj.modify()
    require(s.ue().ToolsetLibrary.set_object_properties(obj, json.dumps(changes, allow_nan=False)), 'Native property write failed')
    after = s.read_object(obj)
    i = module('field_notes_inventory_setup.py')
    require(i._exact_value(after['values'], i.merge(old['values'], changes)), 'Full property readback mismatch')


def margin(l=0, t=0, r=0, b=0):
    return dict(left=l, top=t, right=r, bottom=b)


def rect(x, y, w, h, z=20, anchor_y=0):
    return {'layoutData': {'offsets': margin(x, y, w, h),
                          'anchors': {'minimum': {'x': 0, 'y': anchor_y}, 'maximum': {'x': 0, 'y': anchor_y}},
                          'alignment': {'x': 0, 'y': 0}}, 'bAutoSize': False, 'zOrder': z}


def boxslot(right=8):
    return {'size': {'value': 1, 'sizeRule': 'Automatic'}, 'padding': margin(0, 0, right, 0),
            'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Center'}


def text_values(context, text='', visibility='HitTestInvisible'):
    s = support(); style = s.text_patch(context, 'Caption')
    return {'text': text, 'font': style['font'], 'colorAndOpacity': s.slate(style['color']),
            'visibility': visibility, 'autoWrapText': False}


def add(asset, name, cls, parent=None, values=None, slot=None, variable=True):
    s = support(); require(name not in rows(asset), 'Existing widget name collision: ' + name)
    args = {'widgetBlueprint': s.ref(asset), 'widgetClass': {'refPath': cls}, 'widgetDisplayName': name}
    if parent is not None:
        args['parentWidget'] = parent
    row = tool('AddWidget', **args)
    require(row['widgetName'] == name, 'Native tool changed widget identity')
    patch(row['widget'], values or {'visibility': 'SelfHitTestInvisible'})
    if slot is not None:
        patch(row['slot'], slot)
    if variable:
        tool('ToggleWidgetAsVariable', widgetBlueprint=s.ref(asset), widget=row['widget'], bIsVariable=True)
    return row['widget']


def prompt(asset, name, context, parent, slot):
    s = support(); bp, _ = s.defaults(PROMPTS[context])
    require(s.ue().EditorAssetLibrary.get_metadata_tag(bp, OWNER_KEY) == OWNER, 'Prompt template is not owned')
    return add(asset, name, bp.generated_class().get_path_name(), parent, slot=slot)


def preflight():
    s = gate()
    data = {'commands': {}, 'hosts': {}, 'native_guard_passed': True}
    receipt = json.loads((ROOT / 'Saved/FieldNotesImplementation/Module7/CommandAssets/verified.json').read_text())
    require(receipt['generic_metadata_verified'] and receipt['map_non_consuming_verified'], 'Command contract not verified')
    for path in receipt['assets']:
        asset = s.ue().load_asset(path); require(asset is not None, 'Missing saved command asset: ' + path)
        data['commands'][path] = {'values': s.read_object(asset), 'sha256': s.digest(s.package_file(path))}
    for path in HOSTS:
        data['hosts'][path] = snapshot(path)
    s.write(OUT / 'preflight.json', data); gc.collect()
    return {'receipt': str(OUT / 'preflight.json'), 'hosts': len(data['hosts']), 'commands': len(data['commands']),
            'native_guard_passed': True, 'mutated_assets': []}


def prepare_prompt(context):
    """Reserve an absent target and return a native creation request; no UObject write."""
    require(context in PROMPTS, 'Choose Paper or Overlay')
    asset = PROMPTS[context]; s = gate()
    settings_class = s.ue().load_class(None, '/Script/UMGEditor.UMGEditorProjectSettings')
    require(settings_class is not None, 'Installed UMG editor project settings class is unavailable')
    require(s.ue().get_default_object(settings_class).get_editor_property('default_root_widget') is None,
            'DefaultRootWidget is overridden in the running editor; do not create an unexpected authored root')
    require(not s.ue().EditorAssetLibrary.does_asset_exist(asset) and not s.package_file(asset).exists(),
            'Prompt target already exists; never claim a preexisting asset')
    receipt = OUT / (asset.rsplit('/', 1)[-1] + '-NativeCreation.json')
    if receipt.exists():
        state = json.loads(receipt.read_text())
        require(state['state'] == 'prepared' and state['recipe_sha256'] == s.digest(Path(__file__)),
                'Existing creation receipt is not reusable')
        return {'receipt': str(receipt), 'native_create_request': state['request'], 'run_outside_python': True}
    state = {'asset': asset, 'context': context, 'parent': '/Script/AZ.AZ_ActionPrompt',
             'recipe_version': OWNER, 'recipe_sha256': s.digest(Path(__file__)), 'state': 'prepared',
             'target_existed': False, 'default_root_widget': None,
             'prepared_utc': s.stamp(), 'request': prompt_creation_request(context)}
    s.write(receipt, state); gc.collect()
    return {'receipt': str(receipt), 'native_create_request': state['request'], 'run_outside_python': True,
            'next': 'After root native creation completes, call author_prompt(context); this stage creates no UObject'}


def author_prompt(context):
    """Claim only the fresh externally created empty Blueprint, then author its tree."""
    require(context in PROMPTS, 'Choose Paper or Overlay')
    asset = PROMPTS[context]; s = gate()
    creation_file = OUT / (asset.rsplit('/', 1)[-1] + '-NativeCreation.json')
    require(creation_file.is_file(), 'Call prepare_prompt then perform the returned creation outside Python')
    creation = json.loads(creation_file.read_text())
    require(creation['state'] == 'prepared' and not creation['target_existed']
            and creation['recipe_version'] == OWNER and creation['recipe_sha256'] == s.digest(Path(__file__))
            and creation['asset'] == asset and creation['request'] == prompt_creation_request(context),
            'Fresh creation receipt mismatch or already claimed')
    bp = s.ue().load_asset(asset); require(bp is not None, 'Root must execute the native creation request first')
    tree = tool('GetWidgets', widgetBlueprint=s.ref(asset))
    require(tree['info']['parentClass'] == {'refPath': creation['parent']}, 'Fresh prompt has a different native parent')
    require(not rows(asset), 'Fresh prompt has preexisting real widgets; stop before claiming it')
    require(not s.ue().EditorAssetLibrary.get_metadata_tag(bp, OWNER_KEY), 'Prompt already has an owner')
    for graph in s.ue().BlueprintEditorLibrary.list_graphs(bp):
        editor = s.ue().BlueprintGraphEditor.get_graph_editor(graph)
        for node in editor.list_all_nodes():
            require(node.get_class().get_name() == 'K2Node_Event'
                    and not any(pin.list_connected_pins() for pin in s.ue().BlueprintEditorLibrary.list_output_pins(node)),
                    'Fresh prompt contains authored graph logic')
    # A freshly created unsaved package is allowed only through this absent-target
    # receipt, exact native parent, empty real tree and unconnected-event checks.
    s, marker, folder = begin(asset, 'Tree', new=True)
    require(folder is not None, 'Unexpected previously completed fresh-prompt stage')
    creation['state'] = 'claimed'; creation['backup'] = str(folder); s.write(creation_file, creation)
    bp.modify()
    s.ue().EditorAssetLibrary.set_metadata_tag(bp, OWNER_KEY, OWNER)
    root = add(asset, 'PromptRow', '/Script/UMG.HorizontalBox')
    # A real dark neutral keycap backs existing white native controller glyphs on paper.
    keycap = add(asset, 'Keycap', '/Script/UMG.Border', root,
                 {'background': {'drawAs': 'Box', 'resourceObject': 'None', 'margin': margin(),
                                 'tintColor': s.slate(s.linear('Overlay', 'panel'))},
                  'padding': margin(3, 2, 3, 2), 'visibility': 'HitTestInvisible'}, boxslot(6))
    bounds = add(asset, 'KeyBounds', '/Script/UMG.SizeBox', keycap,
                 {'widthOverride': 24, 'heightOverride': 24, 'bOverride_WidthOverride': True,
                  'bOverride_HeightOverride': True, 'visibility': 'HitTestInvisible'},
                 {'padding': margin(3, 2, 3, 2), 'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Fill'})
    fit = add(asset, 'KeyFit', '/Script/UMG.ScaleBox', bounds,
              {'stretch': 'ScaleToFit', 'visibility': 'HitTestInvisible'},
              {'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Fill', 'padding': margin()})
    stack = add(asset, 'KeyStack', '/Script/UMG.Overlay', fit,
                slot={'horizontalAlignment': 'HAlign_Center', 'verticalAlignment': 'VAlign_Center'})
    overlay_slot = {'padding': margin(), 'horizontalAlignment': 'HAlign_Center', 'verticalAlignment': 'VAlign_Center'}
    add(asset, 'ActionIcon', '/Script/CommonUI.CommonActionWidget', stack,
        {'visibility': 'HitTestInvisible', 'progressMaterialBrush': {'drawAs': 'NoDrawType', 'resourceObject': 'None'},
         'iconRimBrush': {'drawAs': 'NoDrawType', 'resourceObject': 'None'}}, overlay_slot)
    add(asset, 'KeyFallbackText', '/Script/CommonUI.CommonTextBlock', stack,
        text_values('Overlay', visibility='Collapsed'), overlay_slot)
    add(asset, 'DescriptionText', '/Script/CommonUI.CommonTextBlock', root,
        text_values(context), boxslot(0))
    result = finish(s, marker, folder, context=context, next='Native compile/save this new prompt before hosting it')
    creation['state'] = 'authored'; s.write(creation_file, creation)
    gc.collect(); return result


def host_tree(asset):
    require(asset in HOSTS, 'Not an owned host')
    s, marker, folder = begin(asset, 'Tree')
    if folder is None:
        return {'already_complete': True}
    current = rows(asset); old_names = set(current)
    if asset == QUICK:
        parent = current['CrossLayout']['widget']
        main = add(asset, 'FN_QuickPadMain', '/Script/UMG.HorizontalBox', parent, slot=rect(0, 566, 568, 32))
        focus = add(asset, 'FN_QuickFocusGroup', '/Script/UMG.HorizontalBox', main, slot=boxslot(10))
        for name in ('FocusLeft', 'FocusUp', 'FocusDown', 'FocusRight'):
            prompt(asset, 'FN_' + name + 'Prompt', 'Overlay', focus, boxslot(4))
        activate = add(asset, 'FN_QuickActivateGroup', '/Script/UMG.HorizontalBox', main, slot=boxslot(8))
        prompt(asset, 'FN_ActivatePrompt', 'Overlay', activate, boxslot(0))
        for name in ('Assignment', 'Cancel'):
            prompt(asset, 'FN_' + name + 'Prompt', 'Overlay', main, boxslot(10))
        prompt(asset, 'FN_PadTogglePrompt', 'Overlay', main, boxslot(0))
        edit = add(asset, 'FN_QuickPadEdit', '/Script/UMG.HorizontalBox', parent, slot=rect(0, 604, 568, 32))
        for name in ('PreviousCandidate', 'NextCandidate'):
            prompt(asset, 'FN_' + name + 'Prompt', 'Overlay', edit, boxslot(16))
        kbm = add(asset, 'FN_QuickKBM', '/Script/UMG.HorizontalBox', parent, slot=rect(0, 566, 568, 32))
        add(asset, 'FN_QuickKBMHint', '/Script/CommonUI.CommonTextBlock', kbm,
            text_values('Overlay'), boxslot(14))
        prompt(asset, 'FN_KBMTogglePrompt', 'Overlay', kbm, boxslot(0))
        patch(current['HintText']['widget'], {'visibility': 'Collapsed'})
    elif asset in (ENTRY, FISTS):
        child = prompt(asset, 'FN_SlotKeyPrompt', 'Overlay', current['EntryCanvas']['widget'], rect(74, 2, 28, 24, 6))
        patch(child, {'renderTransform': {'translation': {'x': 0, 'y': 0}, 'scale': {'x': 0.72, 'y': 0.72},
                                         'shear': {'x': 0, 'y': 0}, 'angle': 0},
                      'renderTransformPivot': {'x': 0, 'y': 0}})
    elif asset == MAP:
        bar = add(asset, 'FN_MapCommandBar', '/Script/UMG.HorizontalBox', current['MapPageRoot']['widget'],
                  slot=rect(56, -52, 1108, 32, 25, 1))
        for name in MAP_ACTIONS:
            prompt(asset, 'FN_Map' + name + 'Prompt', 'Paper', bar, boxslot(20))
        # This group remains visible for both KBM and gamepad while Map is shown.
        # Its own page ancestry provides the page-change visibility; no new event
        # subscription or duplicated action registration is needed on the menu.
        section_slot = rect(-706, -52, 650, 32, 25, 1)
        for anchor in section_slot['layoutData']['anchors'].values():
            anchor['x'] = 1
        sections = add(asset, 'FN_MapSectionNavigation', '/Script/UMG.HorizontalBox',
                       current['MapPageRoot']['widget'], slot=section_slot)
        for name, _ in MAP_SECTION_PROMPTS:
            prompt(asset, 'FN_Map' + name + 'Prompt', 'Paper', sections, boxslot(20 if name != 'Back' else 0))
    else:
        bar = add(asset, 'FN_InventoryPromptBar', '/Script/UMG.HorizontalBox', current['InventoryCanvas']['widget'],
                  slot=rect(56, -50, 1808, 30, 25, 1))
        # Existing-action previous/next keycaps are native-owned by the tab row.
        for name, _ in INVENTORY_FOOTER_PROMPTS:
            prompt(asset, 'FN_' + name + 'Prompt', 'Paper', bar, boxslot(28))
    require(old_names <= set(rows(asset)), 'Preexisting widget removed')
    before = json.loads((folder / 'before.json').read_text())
    preserve_objects(before, snapshot(asset), {'HintText:widget': {'visibility': 'Collapsed'}} if asset == QUICK else {})
    result = finish(s, marker, folder, added=sorted(set(rows(asset)) - old_names), next='Native compile/save host, then host_bindings(asset)')
    gc.collect(); return result


class Graph:
    def __init__(self, asset):
        self.asset = asset; self.s = support()
        self.bp = self.s.defaults(asset)[0]
        self.o = module('field_notes_overlay_setup.py')
        self.g = self.o.AppendGraph(self.bp, 'EventGraph'); self.b = self.g.b

    def call(self, execute, owner, name, values=None, literals=None):
        node = self.g.call(owner, name, **(values or {}))
        for key, value in (literals or {}).items():
            self.b.literal(self.b.inp(node, key), value)
        pin = node.find_input_pin('execute')
        if pin.is_valid():
            self.b.connect(execute, pin)
            execute = self.b.out(node, 'then')
        return execute, node

    def get(self, name, target=None, owner=None):
        return self.g.get(name, target=target, owner=owner)

    def cast(self, value, cls):
        before = {n.get_path_name() for n in self.g.editor.list_all_nodes()}
        self.s.ue().AZ_BlueprintNodeUtils.add_cast_node(self.asset, 'EventGraph', self.s.ue().load_class(None, cls), True, 0, 0)
        nodes = [n for n in self.g.editor.list_all_nodes() if n.get_path_name() not in before]
        require(len(nodes) == 1 and nodes[0].get_class().get_name() == 'K2Node_DynamicCast', 'Pure cast creation failed')
        node = nodes[0]; self.b.connect(value, self.b.inp(node, 'Object'))
        return self.b.out(node, 'As' + cls.rsplit('.', 1)[-1])

    def branch(self, execute, value):
        return self.g.branch(execute, value)[1:]

    def visibility(self, execute, widget, value):
        return self.call(execute, '/Script/UMG.Widget', 'SetVisibility', {'self': self.get(widget)}, {'InVisibility': value})[0]

    def splice(self, event_name):
        event = self.g.editor.find_event_node(event_name)
        if event is None:
            event = self.s.ue().BlueprintEditorLibrary.add_event_override(self.bp, event_name, self.s.ue().IntPoint(0, 0))
        require(event is not None, 'Missing native event: ' + event_name)
        begin = self.b.out(event, 'then'); following = list(begin.list_connected_pins())
        require(len(following) <= 1, 'Unexpected event fanout')
        old = {n.get_path_name() for n in self.g.editor.list_all_nodes()}
        self.s.ue().AZ_BlueprintNodeUtils.add_sequence_node(self.asset, 'EventGraph', 2, 0, 0)
        made = [n for n in self.g.editor.list_all_nodes() if n.get_path_name() not in old]
        require(len(made) == 1 and made[0].get_class().get_name() == 'K2Node_ExecutionSequence', 'Sequence creation failed')
        sequence = made[0]
        self.s.ue().BlueprintGraphPinLibrary.break_pin_links(begin)
        self.b.connect(begin, self.b.inp(sequence, 'execute'))
        for pin in following:
            self.b.connect(self.b.out(sequence, 'then_0'), pin)
        allowed = {(event.get_path_name(), 'then', True), (event.get_path_name(), 'View', True)}
        allowed.update((p.get_owning_node().get_path_name(), str(p.get_pin_name()), False) for p in following)
        return event, self.b.out(sequence, 'then_1'), allowed

    def owner(self, execute):
        execute, node = self.call(execute, '/Script/UMG.Widget', 'GetOwningPlayer')
        pc = self.cast(self.b.out(node), '/Script/AZ.AZ_PlayerController')
        condition = self.g.call('/Script/Engine.KismetSystemLibrary', 'IsValid', Object=pc)
        yes, no = self.branch(execute, self.b.out(condition))
        return yes, pc, no

    def device(self, execute, pc):
        execute, subsystem = self.call(execute, '/Script/Engine.SubsystemBlueprintLibrary', 'GetLocalPlayerSubSystemFromPlayerController',
                                      {'PlayerController': pc}, {'Class': '/Script/CommonInput.CommonInputSubsystem'})
        target = self.cast(self.b.out(subsystem), '/Script/CommonInput.CommonInputSubsystem')
        execute, node = self.call(execute, '/Script/CommonInput.CommonInputSubsystem', 'IsInputMethodActive',
                                 {'self': target}, {'InputMethod': 'Gamepad'})
        return self.branch(execute, self.b.out(node))

    def configure(self, execute, widget, description, action=None, binding=None):
        values = {'self': self.get(widget)}
        values['Binding' if binding is not None else 'Action'] = binding if binding is not None else action
        return self.call(execute, '/Script/AZ.AZ_ActionPrompt', 'ConfigureBinding' if binding is not None else 'ConfigureAction',
                         values, {'Description': description, 'Context': 'Overlay' if self.asset.startswith(QDIR) else 'Paper'})[0]


def _quick_graph(g, event, execute):
    b = g.b
    execute, pc, _ = g.owner(execute)
    toggle = g.get('QuickSelectToggleAction', pc, '/Script/AZ.AZ_PlayerController')
    for name in QUICK_ACTIONS:
        execute, node = g.call(execute, '/Script/AZ.AZ_QuickSelectWidget', 'GetSelectorActionBinding', {'Action': g.get(name + 'Action')})
        execute = g.configure(execute, 'FN_' + name + 'Prompt', DESCRIPTIONS[name], binding=b.out(node))
    for name in ('FN_PadTogglePrompt', 'FN_KBMTogglePrompt'):
        execute = g.configure(execute, name, 'Close', action=toggle)
    fields = g.g.q.action(g.g.editor, 'Break AZ_QuickSelectView', 'K2Node_BreakStruct')
    b.connect(b.out(event, 'View'), b.inp(fields, 'AZ_QuickSelectView'))
    compare = g.g.call('/Script/Engine.KismetMathLibrary', 'EqualEqual_ByteByte', A=b.out(fields, 'State'))
    b.literal(b.inp(compare, 'B'), '2')  # source enum EditingAssignment
    pad, kbm = g.device(execute, pc)
    pad = g.visibility(pad, 'FN_QuickPadMain', 'SelfHitTestInvisible')
    pad = g.visibility(pad, 'FN_QuickKBM', 'Collapsed')
    edit, browse = g.branch(pad, b.out(compare))
    for name in ('FN_QuickFocusGroup', 'FN_QuickActivateGroup'):
        edit = g.visibility(edit, name, 'Collapsed'); browse = g.visibility(browse, name, 'SelfHitTestInvisible')
    g.visibility(edit, 'FN_QuickPadEdit', 'SelfHitTestInvisible')
    g.visibility(browse, 'FN_QuickPadEdit', 'Collapsed')
    kbm = g.visibility(kbm, 'FN_QuickPadMain', 'Collapsed')
    kbm = g.visibility(kbm, 'FN_QuickPadEdit', 'Collapsed')
    kbm = g.visibility(kbm, 'FN_QuickKBM', 'SelfHitTestInvisible')
    edit, browse = g.branch(kbm, b.out(compare))
    for arm, text in ((edit, 'Wheel Browse   MMB Assign   Esc Cancel'), (browse, 'LMB / RMB Select   MMB Assign')):
        g.call(arm, '/Script/UMG.TextBlock', 'SetText', {'self': g.get('FN_QuickKBMHint')}, {'InText': text})


def _entry_graph(g, event, execute):
    b = g.b
    execute = g.visibility(execute, 'KeyText', 'Collapsed')
    execute, pc, invalid = g.owner(execute)
    g.call(invalid, '/Script/AZ.AZ_ActionPrompt', 'ClearPrompt', {'self': g.get('FN_SlotKeyPrompt')})
    fields = g.g.q.action(g.g.editor, 'Break AZ_QuickSelectEntryView', 'K2Node_BreakStruct')
    b.connect(b.out(event, 'View'), b.inp(fields, 'AZ_QuickSelectEntryView'))
    index = b.out(fields, 'SlotIndex'); array = g.get('WeaponSlotActions', pc, '/Script/AZ.AZ_PlayerController')
    valid = g.g.q.action(g.g.editor, 'Is Valid Index', 'K2Node_CallArrayFunction', [array])
    b.connect(array, b.inp(valid, 'TargetArray')); b.connect(index, b.inp(valid, 'IndexToTest'))
    yes, no = g.branch(execute, b.out(valid))
    g.call(no, '/Script/AZ.AZ_ActionPrompt', 'ClearPrompt', {'self': g.get('FN_SlotKeyPrompt')})
    get = g.g.q.action(g.g.editor, 'Get (a copy)', 'K2Node_GetArrayItem', [array])
    b.connect(array, b.inp(get, 'Array')); b.connect(index, b.inp(get, 'Dimension 1'))
    outputs = [p for p in g.s.ue().BlueprintEditorLibrary.list_output_pins(get) if str(p.get_pin_name()) != 'then']
    require(len(outputs) == 1, 'Unexpected array-get output schema')
    g.configure(yes, 'FN_SlotKeyPrompt', '', action=outputs[0])


def _map_graph(g, execute):
    execute, pc, _ = g.owner(execute)
    for name in MAP_ACTIONS:
        execute, node = g.call(execute, '/Script/AZ.AZ_QuestMapPage', 'GetMapActionBinding', {'Action': g.get('Map' + name + 'Action')})
        execute = g.configure(execute, 'FN_Map' + name + 'Prompt', DESCRIPTIONS[name], binding=g.b.out(node))
    # Read actual defaults from the existing callback owner. Input remaps/device
    # changes stay adaptive because these are the same real action objects.
    parent = support().read_object(support().defaults(MENU)[1])['values']
    for name, description in MAP_SECTION_PROMPTS:
        ref = parent[name[0].lower() + name[1:] + 'Action']
        require(isinstance(ref, dict) and ref.get('refPath'), 'Parent section action unavailable: ' + name)
        execute, _ = g.call(execute, '/Script/AZ.AZ_ActionPrompt', 'ConfigureAction',
                            {'self': g.get('FN_Map' + name + 'Prompt')},
                            {'Action': ref['refPath'], 'Description': description, 'Context': 'Paper'})
    execute = g.visibility(execute, 'FN_MapSectionNavigation', 'SelfHitTestInvisible')
    # Replace the static ESC string with the real mapped Back prompt on all devices.
    execute = g.visibility(execute, 'BackHint', 'Collapsed')
    pad, kbm = g.device(execute, pc)
    for name, value in (('FN_MapCommandBar', 'SelfHitTestInvisible'), ('FooterLayout', 'Collapsed')):
        pad = g.visibility(pad, name, value)
    for name, value in (('FN_MapCommandBar', 'Collapsed'), ('FooterLayout', 'SelfHitTestInvisible')):
        kbm = g.visibility(kbm, name, value)


def host_bindings(asset):
    require(asset in HOSTS, 'Not an exact owned host')
    s, marker, folder = begin(asset, 'Bindings')
    if folder is None:
        return {'already_complete': True}
    event_name = 'OnSelectorViewChanged' if asset == QUICK else 'OnEntryViewChanged' if asset in (ENTRY, FISTS) else 'OnMapActionBindingsChanged' if asset == MAP else 'Construct'
    before = json.loads((folder / 'before.json').read_text()); g = Graph(asset)
    event, execute, allowed = g.splice(event_name)
    if asset == QUICK:
        _quick_graph(g, event, execute)
    elif asset in (ENTRY, FISTS):
        _entry_graph(g, event, execute)
    elif asset == MAP:
        _map_graph(g, execute)
    else:
        for name, description in INVENTORY_FOOTER_PROMPTS:
            execute = g.configure(execute, 'FN_' + name + 'Prompt', description, action=g.get(name + 'Action'))
    after = snapshot(asset)
    g.o.preserve_existing_graph(before['graphs'].get('EventGraph', []), after['graphs']['EventGraph'], allowed)
    for name, graph in before['graphs'].items():
        if name != 'EventGraph':
            require(after['graphs'][name] == graph, 'Unrelated graph changed')
    preserve_objects(before, after)
    result = finish(s, marker, folder, event=event_name, old_event_body_runs_first=True, new_command_bindings=0)
    gc.collect(); return result


def configure_host(asset):
    require(asset in (QUICK, MAP), 'Only Quick/Map need command host defaults')
    s, marker, folder = begin(asset, 'CommandDefaults')
    if folder is None:
        return {'already_complete': True}
    main = s.read_object(s.defaults(MENU)[1])['values']
    pc = s.read_object(s.defaults(PC)[1])['values']
    data = s.read_object(s.defaults(INPUT_DATA)[1])['values']
    current = s.read_object(s.defaults(asset)[1])
    prefix = 'Quick' if asset == QUICK else 'Map'
    patch_values = {(name[0].lower() + name[1:] + 'Action') if asset == QUICK else ('map' + name + 'Action'):
                    s.ref(COMMANDS + 'IA_FN_' + prefix + '_' + name)
                    for name in (QUICK_ACTIONS if asset == QUICK else MAP_ACTIONS)}
    patch_values['inputMapping'] = s.ref(COMMANDS + ('IMC_FN_QuickSelect' if asset == QUICK else 'IMC_FN_Map'))
    patch_values['inputMappingPriority'] = max(2, int(main['inputMappingPriority'])) + 10
    require(current['values']['inputMapping'] in ('None', patch_values['inputMapping']), 'An existing different context must not be replaced')
    if asset == MAP:
        refs = [main[n] for n in ('backAction', 'tabLeftAction', 'tabRightAction')] + [pc['openInventoryAction']]
        refs += [data['enhancedInputClickAction'], data['enhancedInputBackAction']]
        require(all(isinstance(ref, dict) and ref.get('refPath') for ref in refs), 'Parent action identity unavailable')
        patch_values['reservedParentActions'] = list({ref['refPath']: ref for ref in refs}.values())
    patch(current['object'], patch_values)
    preserve_objects(json.loads((folder / 'before.json').read_text()), snapshot(asset), {'@CDO': patch_values})
    result = finish(s, marker, folder, assigned_defaults=patch_values, command_assets_recreated=False)
    gc.collect(); return result


def append_shared_toggle():
    """Only append LeftThumbstick to the existing action; preserve every old row/profile."""
    s = gate(); asset = s.ue().load_asset(SHARED)
    progress_path = OUT / 'shared-toggle-progress.json'
    if progress_path.exists():
        previous = json.loads(progress_path.read_text())
        require(previous['state'] == 'complete', 'Interrupted shared toggle append; inspect backup before recovery')
        require(module('field_notes_inventory_setup.py')._exact_value(s.read_object(asset)['values'], previous['after_values']),
                'Shared context changed since the exact append receipt')
        return {'already_mapped': True, 'changed': False}
    dirty = {p.get_name() for p in s.ue().EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    require(SHARED not in dirty, 'Save known shared context changes before one-key backup')
    old = s.read_object(asset); values = old['values']
    require(not values.get('mappingProfileOverrides'), 'Shared mapping profile overrides require separate exact conflict review')
    mappings = values['defaultKeyMappings']['mappings']; key = 'Gamepad_LeftThumbstick'
    conflicts = [row for row in mappings if row['key'] == key]
    if conflicts:
        require(len(conflicts) == 1 and conflicts[0]['action'] == s.ref(TOGGLE), 'Proposed toggle key is already assigned elsewhere')
        return {'already_mapped': True, 'changed': False}
    # Validate declared active-gameplay/shared mappings, not only the suppressed effective list.
    for path in (SHARED, '/Game/AZ/Blueprints/Input/InputActions/RT/AZ_IMC_RT_PawnInputs'):
        vals = s.read_object(s.ue().load_asset(path))['values']
        require(not vals.get('mappingProfileOverrides'), 'Profile-specific toggle conflict review required')
        require(not any(row['key'] == key for row in vals['defaultKeyMappings']['mappings']), 'Toggle key conflict in ' + path)
    original = [row for row in mappings if row['action'] == s.ref(TOGGLE) and row['key'] == 'Tab']
    require(len(original) == 1, 'Expected exactly one preserved Tab toggle mapping')
    # InputMappingContext is not a Blueprint, so take a dedicated exact backup.
    folder = OUT / 'Before' / ('SharedToggle-' + s.stamp()); folder.mkdir(parents=True)
    shutil.copy2(s.package_file(SHARED), folder / s.package_file(SHARED).name); s.write(folder / 'before.json', old)
    s.write(progress_path, {'state': 'writing', 'backup': str(folder)})
    copied = copy.deepcopy(original[0]); copied['key'] = key
    patch(asset.get_path_name(), {'defaultKeyMappings': {'mappings': mappings + [copied]}})
    after = s.read_object(asset)
    require(after['values']['defaultKeyMappings']['mappings'][:-1] == mappings, 'Preexisting shared mapping changed')
    s.write(progress_path, {'state': 'complete', 'backup': str(folder), 'after_values': after['values']})
    gc.collect()
    return s.write(OUT / 'shared-toggle-authored.json', {'asset': SHARED, 'backup': str(folder), 'added_key': key,
                                                        'native_save_required': True, 'runtime_verified': False})


def verify(asset):
    s = gate(); require(asset in OWNED and asset != SHARED, 'Choose a prompt/host Blueprint')
    current = snapshot(asset)
    errors = []
    bp, _ = s.defaults(asset)
    for graph in s.ue().BlueprintEditorLibrary.list_graphs(bp):
        errors += [n.get_path_name() for n in s.ue().BlueprintGraphEditor.get_graph_editor(graph).list_nodes_with_errors()]
    dirty = {p.get_name() for p in s.ue().EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    require(not errors and asset not in dirty, 'External native compile/save not complete')
    receipt = OUT / (asset.rsplit('/', 1)[-1] + '-verified.json')
    s.write(receipt, {'asset': asset, 'snapshot': current, 'saved': True, 'graph_errors': errors, 'runtime_verified': False})
    gc.collect()
    return {'asset': asset, 'receipt': str(receipt), 'saved': True, 'graph_errors': errors, 'runtime_verified': False}


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument('--plan', action='store_true', required=True)
    parser.parse_args(); print(json.dumps(plan(), indent=2))
