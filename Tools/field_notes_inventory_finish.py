# @Description: Finish Field Notes inventory headers, paper panels and compatible linear vitals.
"""Separate structural Module5 stage; importing performs no editor operations.

Prerequisite: first-pass field_notes_inventory_setup author/compile/save/verify.
capture() -> replace_bars(max_bars=1), repeat -> native compile Vitals ->
layout() -> native compile Vitals + Main -> explicit save -> verify().

No source/config/pack changes, compile/save calls, gameplay execution or PIE.
Only MAIN and VITALS are mutation targets. Existing widget names are preserved;
three compatible HQUI widgets are replaced, old empty layout containers retained,
and passive headers/panel backings are added. No fake vitals/stat values.
"""
from __future__ import annotations

import importlib.util
import copy
import json
import shutil
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'Saved/FieldNotesImplementation/InventoryFinish'
MAIN = '/Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBP_GameInventoryMenu'
VITALS = '/Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBPCharacterVitalsPanel'
LINEAR = '/Game/HQUI_ProgressBars/Widgets/ProgressBarLinear/ProgressBarLinear'
CIRCULAR = '/Game/HQUI_ProgressBars/Widgets/ProgressBarCircular/ProgressBarCircular'
INTERFACE = '/Game/HQUI_ProgressBars/Blueprints/Interfaces/BPi_ProgressBars'
KEY = 'AZ.FieldNotes.InventoryFinish'
VERSION = 'v1'
BARS = ('HealthProgressBar', 'InfectionProgressBar', 'MortalityProgressBar')


def support():
    spec = importlib.util.spec_from_file_location('az_field_notes_inventory_finish_base', ROOT / 'Tools/field_notes_inventory_setup.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def require(value, message):
    if not value:
        raise RuntimeError(message)


def tool(name, **kwargs):
    allowed = {'GetWidgets', 'ReplaceWidgetWithTemplate', 'AddWidget', 'MoveWidget', 'ToggleWidgetAsVariable'}
    require(name in allowed, 'Operation outside structural recipe')
    s = support().support()
    result = s.ue().ToolsetRegistry.execute_tool('UMGToolSet.UMGToolSet', name, json.dumps(kwargs))
    require(result.is_complete and not result.error, name + ': ' + str(result.error))
    return json.loads(result.value)['returnValue']


def rows(package):
    s = support().support()
    return {row['widgetName']: row for row in tool('GetWidgets', widgetBlueprint=s.ref(package))['widgets']}


def _real(row):
    return isinstance(row.get('widget'), dict)


def _snapshot(package):
    s = support().support()
    bp, cdo = s.defaults(package)
    tree = tool('GetWidgets', widgetBlueprint=s.ref(package))
    return {'asset': package, 'tree': tree, 'cdo': s.read_object(cdo),
            'templates': {row['widgetName']: {'row': row,
                          **{kind: s.read_object(s.resolve(row[kind])) for kind in ('widget', 'slot')
                             if isinstance(row.get(kind), dict)}} for row in tree['widgets']},
            'saved_package_sha256': s.digest(s.package_file(package))}


def _state():
    file = OUT / 'state.json'
    require(file.is_file(), 'Run capture() first')
    value = json.loads(file.read_text(encoding='utf-8'))
    require(value['version'] == VERSION, 'Unexpected structural recipe version')
    return value


def _write_state(state):
    return support().support().write(OUT / 'state.json', state)


def _patch(reference, changes):
    s = support().support()
    obj = s.resolve(reference)
    require(obj.get_path_name().split('.')[0] in (MAIN, VITALS), 'Not an owned inventory template: ' + obj.get_path_name())
    before = s.read_object(obj)
    s._validate_patch(before['schema'], changes)
    obj.modify()
    require(s.ue().ToolsetLibrary.set_object_properties(obj, json.dumps(changes, allow_nan=False)), 'Native property write failed')
    after = s.read_object(s.resolve(reference))
    require(s._contains(after['values'], changes), 'Structural style readback mismatch: ' + obj.get_path_name())
    return after


def _widget(package, name, changes):
    row = rows(package).get(name)
    require(row and _real(row), 'Expected real widget ' + name)
    return _patch(row['widget'], changes)


def _slot(package, name, changes):
    row = rows(package)[name]
    require(isinstance(row.get('slot'), dict), 'Missing slot for ' + name)
    return _patch(row['slot'], changes)


def _rect(x, y, width, height, z=0):
    return {'layoutData': {'offsets': {'left': x, 'top': y, 'right': width, 'bottom': height},
                          'anchors': {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 0, 'y': 0}},
                          'alignment': {'x': 0, 'y': 0}}, 'bAutoSize': False, 'zOrder': z}


def _add(package, name, cls, parent, changes=None, slot=None, variable=False):
    s = support().support()
    existing = rows(package).get(name)
    require(not existing or not _real(existing), 'Name already belongs to a real widget: ' + name)
    row = tool('AddWidget', widgetBlueprint=s.ref(package), widgetClass={'refPath': cls},
               widgetDisplayName=name, parentWidget=parent)
    require(row.get('widgetName') == name, 'Native binding name was changed: ' + name)
    _patch(row['widget'], changes or {'visibility': 'HitTestInvisible'})
    if slot is not None:
        _patch(row['slot'], slot)
    if variable:
        tool('ToggleWidgetAsVariable', widgetBlueprint=s.ref(package), widget=row['widget'], bIsVariable=True)
    return rows(package)[name]


def _move(package, name, parent, slot):
    s = support().support()
    row = rows(package)[name]
    result = tool('MoveWidget', widgetBlueprint=s.ref(package), widget=row['widget'], newParent=parent)
    current = rows(package)[name]
    require(current.get('parent') == parent, 'Move did not preserve expected parent: ' + name + '; ' + str(result))
    _patch(current['slot'], slot)
    return current


def _label(package, name, parent, text, rect, pixels, weight='Regular', color='text', variable=False, align='Left'):
    i = support(); s = i.support()
    role = 'SectionTitle' if weight == 'Bold' else 'Body'
    # New passive labels only. Existing numeric/fragment text is never overwritten.
    return _add(package, name, '/Script/UMG.TextBlock', parent,
                {'text': text, 'font': i._font(s, role, pixels), 'colorAndOpacity': s.slate(s.linear('Paper', color)),
                 'visibility': 'HitTestInvisible', 'justification': align, 'autoWrapText': False}, rect, variable)


def capture():
    """Read exact replacement compatibility and take a fresh post-style baseline."""
    i = support(); s = i.support(); s.idle()
    first = ROOT / 'Saved/FieldNotesImplementation/Inventory/verified.json'
    require(first.exists() and json.loads(first.read_text())['saved'], 'Finish/save/verify the first inventory style pass first')
    require(not (OUT / 'state.json').exists(), 'Structural capture already exists; inspect/resume its exact stage')
    dirty = {p.get_name() for p in s.ue().EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    require(not dirty.intersection((MAIN, VITALS)), 'Save known owned changes before structural backup')
    snapshots = {path: _snapshot(path) for path in (MAIN, VITALS)}
    iface = s.ue().load_asset(INTERFACE)
    linear, linear_cdo = s.defaults(LINEAR)
    circular, circular_cdo = s.defaults(CIRCULAR)
    require(iface is not None and iface.generated_class() is not None, 'HQUI interface is not compiled')
    compatibility = {}
    import re
    normalized = lambda value: re.sub(r'[^a-z0-9]', '', str(value).lower())
    required = {'pbsetpercent', 'pbsetprogressmethod', 'pbsetfillcolor'}
    for package, bp, cdo in ((LINEAR, linear, linear_cdo), (CIRCULAR, circular, circular_cdo)):
        require(s.ue().SystemLibrary.does_implement_interface(cdo, iface.generated_class()), 'Class does not implement the existing HQUI API: ' + package)
        functions = list(s.ue().BlueprintEditorLibrary.list_functions(bp)) + list(s.ue().BlueprintEditorLibrary.list_events(bp))
        names = [str(item.get_editor_property('name')) for item in functions]
        require(required <= {normalized(name) for name in names}, 'Required HQUI interface members missing: ' + package)
        compatibility[package] = {'functions': names, 'cdo': s.read_object(cdo)}
    # Exact new scalar/vector schema is captured rather than copying circular
    # Size(float) into linear Size(Vector2D). Native binding itself is UUserWidget.
    size = compatibility[LINEAR]['cdo']['schema']['size']
    require(set(size.get('properties', {})) >= {'x', 'y'}, 'Linear size is not Vector2D')
    for name in BARS:
        row = snapshots[VITALS]['templates'][name]['row']
        require(row['widgetClassPath']['refPath'].split('.')[0] == CIRCULAR, 'Unexpected bar class: ' + name)
    backup = OUT / ('BeforeStructure_' + s.stamp())
    backup.mkdir(parents=True)
    for package in (MAIN, VITALS):
        shutil.copy2(s.package_file(package), backup / s.package_file(package).name)
    s.write(OUT / 'baseline.json', {'snapshots': snapshots, 'compatibility': compatibility,
                                    'backup': str(backup), 'interface': INTERFACE, 'mutation_performed': False})
    _write_state({'version': VERSION, 'token_sha256': s.digest(s.TOKENS), 'bars': {}, 'layout': '',
                  'backup': str(backup), 'saved': False})
    return {'replacement_compatible': True, 'backup': str(backup), 'next': 'replace_bars(max_bars=1)', 'runtime_verified': False}


def replace_bars(max_bars=1):
    require(1 <= max_bars <= 3, 'Replace1–3bars per bounded call')
    i = support(); s = i.support(); s.idle()
    state = _state()
    require(not state['layout'], 'Bars must be replaced before layout stage')
    baseline = json.loads((OUT / 'baseline.json').read_text(encoding='utf-8'))
    require(state['token_sha256'] == s.digest(s.TOKENS), 'Tokens changed since structural capture')
    bp, _ = s.defaults(VITALS)
    pending = [name for name in BARS if state['bars'].get(name, {}).get('state') != 'authored']
    for name in pending[:max_bars]:
        require(name not in state['bars'], 'Interrupted replacement; inspect its report/backup before retrying')
        s.idle()
        before = baseline['snapshots'][VITALS]['templates'][name]
        current = rows(VITALS)[name]
        require(current['widgetClassPath']['refPath'].split('.')[0] == CIRCULAR, 'Unexpected current bar class')
        current_values = s.read_object(s.resolve(current['widget']))['values']
        for key in ('currentPercent', 'targetPercent', 'useTargetPercent', 'progressMethod', 'interpTimeCurrent'):
            require(current_values[key] == before['widget']['values'][key], 'Bar value changed since capture: ' + name + '.' + key)
        state['bars'][name] = {'state': 'replacing'}; _write_state(state)
        result = tool('ReplaceWidgetWithTemplate', widgetBlueprint=s.ref(VITALS), widgetToReplace=current['widget'],
                      templateClass={'refPath': s.defaults(LINEAR)[0].generated_class().get_path_name()})
        state['bars'][name]['report'] = result; _write_state(state)
        require(result.get('bSuccess') and not result.get('unmatchedReferencedProperties')
                and not result.get('unmatchedReferencedFunctions') and not result.get('missingReferencesWarning'),
                'Replacement reported incompatible references; STOP and inspect saved report/backup')
        after = rows(VITALS)[name]
        require(after['widgetClassPath']['refPath'].split('.')[0] == LINEAR, 'Bar replacement class readback mismatch')
        require(after['parent'] == current['parent'], 'Replacement unexpectedly moved the bar')
        old = before['widget']['values']
        patch = {key: old[key] for key in ('currentPercent', 'targetPercent', 'useTargetPercent', 'progressMethod',
                                         'interpTimeCurrent', 'interpTimeTarget', 'visibility') if key in old}
        patch.update({'size': {'x': 350, 'y': 7}, 'fillType': 'LeftToRight',
                      'fillColorCurrent': s.linear('Paper', 'text'), 'backgroundTint': s.linear('Paper', 'edge', 0.6),
                      'blendMask': 'None', 'fillColorMask': 'None', 'backgroundMask': 'None',
                      'fillColorUseGradient': False, 'backgroundThickness': 0, 'backgroundBlurStrength': 0,
                      'useShaderProgressBar': False,
                      'renderTransform': {'translation': {'x': 0, 'y': 0}, 'scale': {'x': 1, 'y': 1}, 'shear': {'x': 0, 'y': 0}, 'angle': 0}})
        _widget(VITALS, name, patch)
        state['bars'][name]['state'] = 'authored'; _write_state(state)
    return {'remaining': max(0, len(pending) - max_bars), 'native_compile_after_all': [VITALS],
            'preserved': 'Exact widget names, current/target values, native health binding and interface messages', 'runtime_verified': False}


def _paper_column_wrappers():
    i = support(); s = i.support()
    current = rows(MAIN); parent = current['HorizontalBox_300']['widget']
    for name, wrapper in (('AZ_WBPCharacterVitalsPanel', 'FN_VitalsPaperPanel'),
                          ('InventorySwitcherPanel', 'FN_BackpackPaperPanel'),
                          ('AZ_WBP_CharacterSkillsPanel', 'FN_SkillsPaperPanel')):
        original = rows(MAIN)[name]
        require(original['parent'] == parent, 'Expected the reviewed direct column child: ' + name)
        old_slot = s.read_object(s.resolve(original['slot']))['values']
        panel = _add(MAIN, wrapper, '/Script/UMG.Border', parent,
                     {'background': i._solid(s, 'panel'), 'brushColor': {'r': 1, 'g': 1, 'b': 1, 'a': 1},
                      'padding': i._margin(), 'visibility': 'SelfHitTestInvisible'}, old_slot)
        _move(MAIN, name, panel['widget'], {'padding': i._margin(), 'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Fill'})


def _headers():
    i = support(); s = i.support(); parent = rows(MAIN)['InventoryCanvas']['widget']
    _label(MAIN, 'FN_ChalkBrand', parent, 'CHALK', _rect(56, 32, 370, 60, 3), 45, 'Bold')
    _label(MAIN, 'FN_InventoryTitle', parent, 'Inventory', _rect(510, 46, 650, 45, 3), 29, 'Bold')
    footer = _add(MAIN, 'FN_InventoryFooterRule', '/Script/UMG.Image', parent,
                  {'brush': i._solid(s, 'edge'), 'visibility': 'HitTestInvisible'},
                  {'layoutData': {'offsets': i._margin(56, -66, 56, 1),
                                  'anchors': {'minimum': {'x': 0, 'y': 1}, 'maximum': {'x': 1, 'y': 1}},
                                  'alignment': {'x': 0, 'y': 0}}, 'bAutoSize': False, 'zOrder': 3})
    # No static E/Esc/RMB art is added: the CommonInput workstream owns prompts.


def _vitals_layout():
    i = support(); s = i.support(); current = rows(VITALS)
    root = current['VerticalBox']['widget']
    canvas = _add(VITALS, 'FN_VitalsCanvas', '/Script/UMG.CanvasPanel', root,
                  {'visibility': 'SelfHitTestInvisible'}, {'size': {'value': 1, 'sizeRule': 'Fill'},
                  'padding': i._margin(), 'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Fill'})
    _label(VITALS, 'FN_ConditionHeading', canvas['widget'], 'PERSONAL CONDITION', _rect(24, 24, 360, 32), 21, 'Bold', 'muted')
    portrait_fit = _add(VITALS, 'FN_PortraitAspectFit', '/Script/UMG.ScaleBox', canvas['widget'],
                        {'stretch': 'ScaleToFit', 'visibility': 'HitTestInvisible'}, _rect(39, 56, 316, 444))
    _move(VITALS, 'HeroPortraitImage', portrait_fit['widget'], {'horizontalAlignment': 'HAlign_Center', 'verticalAlignment': 'VAlign_Center'})
    _label(VITALS, 'FN_HealthLabel', canvas['widget'], 'HEALTH', _rect(28, 521, 170, 30), 20)
    _label(VITALS, 'HealthValueText', canvas['widget'], '', _rect(220, 521, 158, 30), 20, variable=True, align='Right')
    _move(VITALS, 'HealthProgressBar', canvas['widget'], _rect(28, 557, 350, 7))
    _move(VITALS, 'HealthIconImage', canvas['widget'], _rect(4, 521, 18, 22))
    reset = {'renderTransform': {'translation': {'x': 0, 'y': 0}, 'scale': {'x': 1, 'y': 1},
                                'shear': {'x': 0, 'y': 0}, 'angle': 0}}
    _widget(VITALS, 'HealthIconImage', reset)
    for stem, y in (('Infection', 601), ('Mortality', 676)):
        # These are the native optional containers, not new gameplay variables.
        # Their visibility follows existing bShowUnsupportedVitals policy.
        group = _add(VITALS, stem + 'Container', '/Script/UMG.CanvasPanel', canvas['widget'],
                     {'visibility': 'SelfHitTestInvisible'}, _rect(0, y, 405, 54), variable=True)
        _label(VITALS, 'FN_' + stem + 'Label', group['widget'], stem.upper(), _rect(28, 0, 350, 30), 20)
        _move(VITALS, stem + 'ProgressBar', group['widget'], _rect(28, 37, 350, 7))
        _move(VITALS, stem + 'IconImage', group['widget'], _rect(4, 0, 18, 22))
        _widget(VITALS, stem + 'IconImage', reset)
    # Heartbeat decorations deliberately remain outside unsupported-meter groups.
    for name in ('HeartbeatShadowImage', 'HeartbeatImage'):
        _move(VITALS, name, canvas['widget'], _rect(28, 744, 350, 58))
        _widget(VITALS, name, reset)
    # Only emptied layout scaffolding is collapsed. No widget/binding is deleted.
    for name in ('HorizontalBox_592', 'HorizontalBox_713'):
        _widget(VITALS, name, {'visibility': 'Collapsed'})


def layout():
    i = support(); s = i.support(); s.idle(); state = _state()
    if state['layout'] == 'authored':
        return {'state': 'already authored', 'native_compile_required': [VITALS, MAIN]}
    require(not state['layout'], 'Interrupted layout; inspect backup before retrying')
    require(all(state['bars'].get(name, {}).get('state') == 'authored' for name in BARS), 'Replace and native-compile all3bars first')
    before_names = {package: {name for name, row in rows(package).items() if _real(row)} for package in (MAIN, VITALS)}
    for package in (MAIN, VITALS):
        s.defaults(package)[0].modify()
    state['layout'] = 'writing'; _write_state(state)
    _paper_column_wrappers()
    _headers()
    _vitals_layout()
    for package in (MAIN, VITALS):
        after = rows(package)
        require(before_names[package] <= {name for name, row in after.items() if _real(row)}, 'A preexisting widget was removed')
        s.ue().EditorAssetLibrary.set_metadata_tag(s.defaults(package)[0], KEY, VERSION)
    state['layout'] = 'authored'; _write_state(state)
    return {'state': 'authored', 'native_compile_required': [VITALS, MAIN], 'explicit_save_required': [VITALS, MAIN],
            'runtime_verified': False, 'note': 'No fabricated infection/mortality values or keyboard glyph text were added.'}


def verify():
    i = support(); s = i.support(); state = _state()
    require(state['layout'] == 'authored', 'Finish layout then native-compile first')
    baseline = json.loads((OUT / 'baseline.json').read_text())
    current = {package: _snapshot(package) for package in (MAIN, VITALS)}
    for name in BARS:
        entry = current[VITALS]['templates'][name]
        require(entry['row']['widgetClassPath']['refPath'].split('.')[0] == LINEAR, 'Unexpected final bar class')
        old = baseline['snapshots'][VITALS]['templates'][name]['widget']['values']
        require(all(entry['widget']['values'][key] == old[key] for key in ('currentPercent', 'targetPercent', 'useTargetPercent', 'progressMethod')),
                'A captured bar value/method changed')
    old_portrait = baseline['snapshots'][VITALS]['templates']['HeroPortraitImage']['widget']['values']['brush']['resourceObject']
    require(current[VITALS]['templates']['HeroPortraitImage']['widget']['values']['brush']['resourceObject'] == old_portrait, 'Portrait resource changed')
    for package in (MAIN, VITALS):
        bp, _ = s.defaults(package)
        require(s.ue().EditorAssetLibrary.get_metadata_tag(bp, KEY) == VERSION, 'Missing final-stage marker')
        for graph in s.ue().BlueprintEditorLibrary.list_graphs(bp):
            editor = s.ue().BlueprintGraphEditor.get_graph_editor(graph)
            require(not editor.list_nodes_with_errors(), 'Blueprint graph has errors after replacement')
    dirty = {p.get_name() for p in s.ue().EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    saved = not dirty.intersection((MAIN, VITALS))
    return s.write(OUT / 'verified.json', {'snapshots': current, 'saved': saved, 'runtime_verified': False,
                                          'module5_complete': False,
                                          'remaining': ['User visual/DPI/input/health acceptance',
                                                        'CommonInput action prompts in their own module',
                                                        'Do not treat placeholder illness/skills values as gameplay bindings']})
