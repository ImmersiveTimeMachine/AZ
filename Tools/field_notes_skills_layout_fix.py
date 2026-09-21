# @Description: Reflow the owned Field Notes skills panel and remove vendor visual treatments.
"""Import-inert, target-only follow-up to the completed inventory first pass.

Offline: python Tools/field_notes_skills_layout_fix.py --plan
Editor: capture(); author(); external native compile/save; verify().
No compile, save, PIE, tests, source edits, widget removal or vendor-asset writes.
All existing text, percentages, button objects and optional value placeholders
are retained. Four childless textured buttons gain passive ADD POINT labels.
"""
from __future__ import annotations

import copy
import gc
import importlib.util
import json
import shutil
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'Saved/FieldNotesImplementation/SkillsLayoutFix'
AUDIT = ROOT / 'Saved/FieldNotesImplementation/Audit/inventory-layout-after.json'
ASSET = '/Game/AZ/Blueprints/Menu/CommonInventory/AZ_WBP_CharacterSkillsPanel'
LINEAR = '/Game/HQUI_ProgressBars/Widgets/ProgressBarLinear/ProgressBarLinear'
STEMS = ('Strength', 'Agility', 'Resilience', 'Expertise')
VERSION = 'field_notes_skills_layout_fix:v1'
KEY = 'AZ.FieldNotes.SkillsLayoutFix'
RESET = {'translation': {'x': 0, 'y': 0}, 'scale': {'x': 1, 'y': 1},
         'shear': {'x': 0, 'y': 0}, 'angle': 0}
WHITE = {'r': 1, 'g': 1, 'b': 1, 'a': 1}


def support():
    spec = importlib.util.spec_from_file_location('az_skills_fix_inventory', ROOT / 'Tools/field_notes_inventory_setup.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def require(value, message):
    if not value:
        raise RuntimeError(message)


def _tool(name, **kwargs):
    require(name in ('GetWidgets', 'AddWidget', 'MoveWidget'), 'Operation outside skills scope')
    s = support().support()
    result = s.ue().ToolsetRegistry.execute_tool('UMGToolSet.UMGToolSet', name, json.dumps(kwargs))
    require(result.is_complete and not result.error, name + ': ' + str(result.error))
    return json.loads(result.value)['returnValue']


def _rows():
    s = support().support()
    return {row['widgetName']: row for row in _tool('GetWidgets', widgetBlueprint=s.ref(ASSET))['widgets']}


def _snapshot():
    s = support().support()
    bp, cdo = s.defaults(ASSET)
    tree = _tool('GetWidgets', widgetBlueprint=s.ref(ASSET))
    return {'asset': ASSET, 'tree': tree, 'cdo': s.read_object(cdo),
            'templates': {row['widgetName']: {'row': row, **{
                kind: s.read_object(s.resolve(row[kind])) for kind in ('widget', 'slot')
                if isinstance(row.get(kind), dict)}} for row in tree['widgets']},
            'saved_package_sha256': s.digest(s.package_file(ASSET))}


def _rect(x, y, width, height):
    return {'layoutData': {'offsets': {'left': x, 'top': y, 'right': width, 'bottom': height},
                          'anchors': {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 0, 'y': 0}},
                          'alignment': {'x': 0, 'y': 0}}, 'bAutoSize': False, 'zOrder': 0}


def _recipe(snapshot):
    i = support(); s = i.support(); t = snapshot['templates']
    operations = []
    def patch(name, values, part='widget'):
        require(name in t and part in t[name], 'Missing exact template: ' + name + ':' + part)
        s._validate_patch(t[name][part]['schema'], values)
        operations.append({'kind': 'patch', 'name': name, 'part': part, 'patch': values})
    def move(name, parent, slot):
        operations.append({'kind': 'move', 'name': name, 'parent': parent, 'slot': slot})
    def add(name, cls, parent, values, slot):
        operations.append({'kind': 'add', 'name': name, 'class': cls, 'parent': parent,
                           'patch': values, 'slot': slot})
    def label(name, parent, text, pixels, slot, bold=False):
        values = {'text': text, 'font': i._font(s, 'SectionTitle' if bold else 'Body', pixels),
                  'colorAndOpacity': s.slate(s.linear('Paper', 'text')), 'visibility': 'HitTestInvisible',
                  'shadowOffset': {'x': 0, 'y': 0}, 'shadowColorAndOpacity': {'r': 0, 'g': 0, 'b': 0, 'a': 0},
                  'autoWrapText': False}
        s._validate_patch(t['StrengthNameText']['widget']['schema'], values)
        add(name, '/Script/UMG.TextBlock', parent, values, slot)
    def brush(color, width=0, edge='edge'):
        result = i._solid(s, color, edge, width)
        result['imageSize'] = {'x': 0, 'y': 0}
        return result
    def button_style(upgrade=False):
        result = {state: brush('panel' if state in ('normal', 'disabled') else 'background',
                               0 if upgrade else 1, 'text' if state == 'hovered' else 'edge')
                  for state in ('normal', 'hovered', 'pressed', 'disabled')}
        result.update({state + 'Foreground': s.slate(s.linear('Paper', 'muted' if state == 'disabled' else 'text'))
                       for state in ('normal', 'hovered', 'pressed', 'disabled')})
        result.update({'normalPadding': i._margin(), 'pressedPadding': i._margin()})
        return result
    fill = {'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Fill'}
    overlay_fill = dict(fill, padding=i._margin())

    # The existing root survives; the new canvas has the same 464x810 paper-column
    # coordinates as the selected design. Old scaffolding is retained but empty.
    add('FN_SkillsCanvas', '/Script/UMG.CanvasPanel', 'RootHBox', {'visibility': 'SelfHitTestInvisible'},
        dict(overlay_fill, size={'value': 1, 'sizeRule': 'Fill'}))
    label('FN_CapabilitiesHeading', 'FN_SkillsCanvas', 'CAPABILITIES', 28, _rect(27, 24, 403, 38), True)

    for index, stem in enumerate(STEMS):
        move(stem + 'Overlay', 'FN_SkillsCanvas', _rect(27, 112 + index * 115, 403, 62))
        # The native percentage/interpolation/range data is deliberately absent
        # from this patch. separation_Steps=20 caused the checkerboard appearance.
        visual = {'size': {'x': 403, 'y': 7}, 'fillType': 'LeftToRight',
                  'fillColorCurrent': s.linear('Paper', 'text'),
                  'backgroundTint': s.linear('Paper', 'edge', 0.6),
                  'blendMask': 'None', 'fillColorMask': 'None', 'backgroundMask': 'None',
                  'fillColorUseGradient': False, 'fillColorBrushTiling': 'NoTile',
                  'backgroundBrushTiling': 'NoTile', 'backgroundThickness': 0, 'backgroundBlurStrength': 0,
                  'useShaderProgressBar': False, 'separation_Steps': 1, 'separation_Steps_Spacing': 0,
                  'separation_AbsoluteFillMethod': False, 'bUseCustomMarquee': False, 'bUseDefaultMarquee': False,
                  'renderTransform': copy.deepcopy(RESET)}
        # Retain effect records and sound actions; disable only vendor visual FX.
        effects = copy.deepcopy(t[stem + 'Progress']['widget']['values']['effects'])
        for effect in effects:
            if effect['effect Type'] not in ('Sound Effect', 'Sound Effect Looped'):
                effect['is Enabled'] = False
            s._validate_patch(t[stem + 'Progress']['widget']['schema']['effects']['items']['properties'], effect)
        visual['effects'] = effects
        patch(stem + 'Progress', visual)
        patch(stem + 'Progress', {'padding': i._margin(0, 43, 0, 0),
                                 'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Top'}, 'slot')
        patch(stem + 'TextHBox', {'padding': i._margin(30, 0, 118, 0),
                                 'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Top'}, 'slot')
        patch(stem + 'NameText', {'font': i._font(s, 'Body', 22), 'renderTransform': copy.deepcopy(RESET)})
        move(stem + 'IconImage', stem + 'Overlay', {'padding': i._margin(0, 3, 0, 0),
                                                  'horizontalAlignment': 'HAlign_Left', 'verticalAlignment': 'VAlign_Top'})
        patch(stem + 'IconImage', {'brush': {'imageSize': {'x': 22, 'y': 22}, 'tintColor': s.slate(WHITE)},
                                   'colorAndOpacity': s.linear('Paper', 'muted'), 'renderTransform': copy.deepcopy(RESET)})
        patch(stem + 'ShineImage', {'renderOpacity': 0})
        patch(stem + 'ButtonSizeBox', {'widthOverride': 110, 'heightOverride': 30,
                                       'bOverride_WidthOverride': True, 'bOverride_HeightOverride': True,
                                       'renderTransform': copy.deepcopy(RESET)})
        patch(stem + 'ButtonSizeBox', {'padding': i._margin(), 'horizontalAlignment': 'HAlign_Right',
                                       'verticalAlignment': 'VAlign_Top'}, 'slot')
        patch(stem + 'ButtonScaleBox', {'stretch': 'Fill', 'stretchDirection': 'Both',
                                       'renderTransform': copy.deepcopy(RESET)})
        patch(stem + 'Add', {'widgetStyle': button_style(), 'backgroundColor': WHITE, 'colorAndOpacity': WHITE,
                            'renderTransform': copy.deepcopy(RESET)})
        patch(stem + 'Add', fill, 'slot')
        label('FN_' + stem + 'AddLabel', stem + 'Add', 'ADD POINT', 14,
              {'padding': i._margin(6, 2, 6, 2), 'horizontalAlignment': 'HAlign_Center', 'verticalAlignment': 'VAlign_Center'})

    move('UpgradeNotificationOverlay', 'FN_SkillsCanvas', _rect(27, 595, 403, 43))
    patch('UpgradeButton', {'widgetStyle': button_style(True), 'backgroundColor': WHITE,
                             'colorAndOpacity': WHITE, 'renderTransform': copy.deepcopy(RESET)})
    patch('UpgradeButton', overlay_fill, 'slot')
    patch('UpgradeNotificationBgImage', {'brush': brush('panel'), 'colorAndOpacity': WHITE, 'renderOpacity': 0})
    patch('UpgradeNotificationText', {'padding': i._margin(), 'horizontalAlignment': 'HAlign_Left',
                                      'verticalAlignment': 'VAlign_Center'}, 'slot')
    # No font/style/text patch: the persistent Caption_Story style must survive.

    move('DetailsHeaderBgSizeBox', 'FN_SkillsCanvas', _rect(27, 648, 403, 1))
    patch('DetailsHeaderBgSizeBox', {'heightOverride': 1, 'bOverride_HeightOverride': True,
                                    'bOverride_WidthOverride': False, 'renderTransform': copy.deepcopy(RESET)})
    patch('DetailsHeaderBgImage', {'brush': brush('edge'), 'colorAndOpacity': WHITE, 'renderOpacity': 1})
    move('DetailsVBox', 'FN_SkillsCanvas', _rect(27, 664, 403, 130))
    patch('DetailsHeaderHBox', {'padding': i._margin(0, 0, 0, 12), 'size': {'value': 1, 'sizeRule': 'Automatic'}}, 'slot')
    patch('DetailsHeaderOverlay', overlay_fill, 'slot')
    patch('DetailsHeaderText', {'renderTransform': copy.deepcopy(RESET)})
    for name in ('CarryWeightVBox', 'MeleeDmgVBox'):
        patch(name, {'padding': i._margin(), 'size': {'value': 1, 'sizeRule': 'Fill'}}, 'slot')
    for name in ('CarryWeightRowHBox', 'MeleeDmgRowHBox'):
        patch(name, {'padding': i._margin(0, 0, 0, 15)}, 'slot')
    for name in ('CarryWeightText', 'MeleeDmgText'):
        patch(name, {'font': i._font(s, 'Body', 20)})
    for name in ('CarryWeightBonusText', 'MeleeDmgBonusText'):
        patch(name, {'font': i._font(s, 'Muted', 16)})
    for name in ('VerticalBox_77', 'ContentVBox'):
        patch(name, {'visibility': 'Collapsed'})
    return operations


def plan():
    snapshot = json.loads(AUDIT.read_text(encoding='utf-8'))[ASSET]
    return {'version': VERSION, 'asset': ASSET, 'operations': _recipe(snapshot),
            'preserves': ['Every existing widget and existing text', 'Current/target/interpolation values',
                          'Native bShowUnsupportedSkills policy', 'All original button objects and input flags',
                          'Unbound optional skill-value placeholders', 'UpgradeNotificationText Caption_Story style'],
            'compile_and_save_external': True, 'runtime_verified': False}


def capture():
    i = support(); s = i.support(); s.idle()
    state_file = OUT / 'state.json'
    if state_file.is_file():
        state = json.loads(state_file.read_text())
        require(state['version'] == VERSION, 'Unexpected existing skills-stage state')
        return {'state': state['state'], 'backup': state['backup'], 'already_captured': True}
    snapshot = _snapshot(); t = snapshot['templates']
    require('FN_SkillsCanvas' not in t, 'Layout exists without its capture receipt; inspect before proceeding')
    require('TS_FN_Paper_Caption_Story' in str(t['UpgradeNotificationText']['widget']['values']['style']),
            'Persistent upgrade-accent style is missing')
    for stem in STEMS:
        require(t[stem + 'Progress']['row']['widgetClassPath']['refPath'].split('.')[0] == LINEAR, 'Unexpected bar class')
        parent = t[stem + 'Add']['row']['widget']
        require(not any(row['row']['parent'] == parent for row in t.values()), 'Add button has content; review its label')
    operations = _recipe(snapshot)
    backup = OUT / ('BeforeWrites_' + s.stamp())
    backup.mkdir(parents=True)
    shutil.copy2(s.package_file(ASSET), backup / s.package_file(ASSET).name)
    refs = sorted(set(i._references(snapshot)))
    s.write(backup / 'snapshot.json', snapshot)
    s.write(backup / 'references.json', {'references': refs, 'source_asset_sha256': snapshot['saved_package_sha256']})
    s.write(OUT / 'plan.json', {'version': VERSION, 'operations': operations})
    result = {'version': VERSION, 'state': 'captured', 'backup': str(backup), 'completed': [],
              'token_sha256': s.digest(s.TOKENS), 'runtime_verified': False}
    s.write(state_file, result)
    gc.collect()
    return {'state': 'captured', 'backup': str(backup), 'operations': len(operations), 'next': 'author()'}


def _patch(reference, changes):
    s = support().support(); obj = s.resolve(reference)
    require(obj.get_path_name().split('.')[0] == ASSET, 'Property write outside skills asset')
    before = s.read_object(obj)
    s._validate_patch(before['schema'], changes)
    if not s._contains(before['values'], changes):
        obj.modify()
        require(s.ue().ToolsetLibrary.set_object_properties(obj, json.dumps(changes, allow_nan=False)), 'Property write failed')
    after = s.read_object(s.resolve(reference))
    require(s._contains(after['values'], changes), 'Property readback mismatch: ' + obj.get_path_name())


def _execute(op):
    s = support().support(); current = _rows(); name = op['name']
    if op['kind'] == 'patch':
        _patch(current[name][op['part']], op['patch'])
    elif op['kind'] == 'move':
        parent = current[op['parent']]['widget']
        if current[name]['parent'] != parent:
            _tool('MoveWidget', widgetBlueprint=s.ref(ASSET), widget=current[name]['widget'], newParent=parent)
        row = _rows()[name]
        require(row['parent'] == parent, 'Move readback mismatch: ' + name)
        _patch(row['slot'], op['slot'])
    elif op['kind'] == 'add':
        parent = current[op['parent']]['widget']
        if name not in current:
            _tool('AddWidget', widgetBlueprint=s.ref(ASSET), widgetClass={'refPath': op['class']},
                  widgetDisplayName=name, parentWidget=parent)
        row = _rows()[name]
        require(row['parent'] == parent and row['widgetClassPath']['refPath'] == op['class'], 'New widget identity mismatch')
        _patch(row['widget'], op['patch']); _patch(row['slot'], op['slot'])


def _preserved(before, after):
    t = before['templates']; now = after['templates']
    require(set(t) <= set(now), 'A preexisting widget or native optional field was removed')
    require(before['cdo']['values']['bShowUnsupportedSkills'] == after['cdo']['values']['bShowUnsupportedSkills'],
            'Native unsupported-skills policy changed')
    for name, entry in t.items():
        if 'widget' not in entry:
            require('widget' not in now[name], 'Optional placeholder was populated: ' + name)
            continue
        values = entry['widget']['values']; current = now[name]['widget']['values']
        require(entry['row']['widgetClassPath'] == now[name]['row']['widgetClassPath'], 'Existing widget class changed')
        if 'text' in values:
            require(current['text'] == values['text'], 'Existing text changed: ' + name)
        if name.endswith('Progress'):
            for key in ('currentPercent', 'targetPercent', 'useTargetPercent', 'progressMethod',
                        'interpTimeCurrent', 'interpTimeTarget', 'queueDelay', 'onPercentChanged'):
                require(current[key] == values[key], 'Skill value/action changed: ' + name + '.' + key)
        if name.endswith('IconImage'):
            require(current['brush']['resourceObject'] == values['brush']['resourceObject'], 'Icon resource changed')
        if name.endswith('Add') or name == 'UpgradeButton':
            for key in ('bIsEnabled', 'isFocusable', 'clickMethod', 'touchMethod', 'pressMethod', 'navigation'):
                if key in values:
                    require(current[key] == values[key], 'Button input behavior changed: ' + name + '.' + key)
    require(now['UpgradeNotificationText']['widget']['values']['style'] == t['UpgradeNotificationText']['widget']['values']['style'],
            'Persistent Story text style changed')


def author(max_operations=120):
    """Resumable bounded authoring; every step is independently idempotent/read back."""
    s = support().support(); s.idle()
    require(1 <= max_operations <= 150, 'Use a bounded count of 1..150 operations')
    state = json.loads((OUT / 'state.json').read_text())
    require(state['version'] == VERSION and state['token_sha256'] == s.digest(s.TOKENS), 'Capture/tokens changed')
    plan_data = json.loads((OUT / 'plan.json').read_text())
    operations = plan_data['operations']
    before = json.loads((Path(state['backup']) / 'snapshot.json').read_text())
    _preserved(before, _snapshot())
    pending = [index for index in range(len(operations)) if index not in state['completed']]
    if not pending:
        return {'state': state['state'], 'remaining': 0, 'already_authored': True, 'runtime_verified': False}
    s.defaults(ASSET)[0].modify()
    for index in pending[:max_operations]:
        state['state'] = 'authoring'; state['next_operation'] = index
        s.write(OUT / 'state.json', state)
        _execute(operations[index])
        state['completed'].append(index)
        s.write(OUT / 'state.json', state)
    remaining = len(operations) - len(state['completed'])
    if not remaining:
        _preserved(before, _snapshot())
        state['state'] = 'authored'
        s.ue().EditorAssetLibrary.set_metadata_tag(s.defaults(ASSET)[0], KEY, VERSION)
        s.write(OUT / 'state.json', state)
    gc.collect()
    return {'state': state['state'], 'remaining': remaining,
            'native_compile_required': [ASSET], 'explicit_save_required': [ASSET], 'runtime_verified': False}


def verify():
    """Readback only. Run after the owner's native compile and explicit save."""
    s = support().support()
    state = json.loads((OUT / 'state.json').read_text())
    require(state['state'] == 'authored', 'Complete authoring first')
    before = json.loads((Path(state['backup']) / 'snapshot.json').read_text())
    after = _snapshot(); _preserved(before, after)
    rows = after['templates']
    for op in json.loads((OUT / 'plan.json').read_text())['operations']:
        entry = rows[op['name']]
        if op['kind'] in ('patch', 'add'):
            require(s._contains(entry[op.get('part', 'widget')]['values'], op['patch']), 'Final patch differs: ' + op['name'])
        if op['kind'] in ('move', 'add'):
            require(entry['row']['parent'] == rows[op['parent']]['row']['widget'], 'Final parent differs')
            require(s._contains(entry['slot']['values'], op['slot']), 'Final layout differs: ' + op['name'])
    dirty = {package.get_name() for package in s.ue().EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    return s.write(OUT / 'verified.json', {'asset': ASSET, 'snapshot': after, 'saved': ASSET not in dirty,
                                          'runtime_verified': False,
                                          'remaining_render_checks': ['Straight solid bars at preserved 25% placeholder values',
                                              'Small icons and legible textured-art-free ADD POINT controls',
                                              'Unclipped DETAILS and original bonus text within the paper panel',
                                              'Caption_Story upgrade accent persists after compile/reopen',
                                              'User acceptance of actual supported input/visibility behavior']})


def fix_designer_scaffolding():
    """Independent bounded delta; zero-size retained scaffolding in the designer.

    Designer visibility does not honor runtime Collapsed. Each retained root
    branch therefore receives an owned zero-size/clipped wrapper. This never
    reruns the 80-operation layout or touches any original leaf or action.
    """
    i = support(); s = i.support(); s.idle()
    require('FN_SkillsCanvas' in _rows(), 'Finish the skills canvas layout first')
    receipt = OUT / 'designer-scaffolding.json'
    if not receipt.exists():
        before = _snapshot()
        backup = OUT / ('BeforeDesignerScaffolding_' + s.stamp())
        backup.mkdir(parents=True)
        shutil.copy2(s.package_file(ASSET), backup / s.package_file(ASSET).name)
        s.write(backup / 'snapshot.json', before)
        s.write(receipt, {'version': VERSION, 'backup': str(backup), 'state': 'captured'})
    state = json.loads(receipt.read_text())
    before = json.loads((Path(state['backup']) / 'snapshot.json').read_text())
    operations = []
    for name, wrapper in (('VerticalBox_77', 'FN_LegacySkillsIconsZeroSize'),
                          ('ContentVBox', 'FN_LegacySkillsContentZeroSize')):
        operations.extend([
            {'kind': 'add', 'name': wrapper, 'class': '/Script/UMG.SizeBox', 'parent': 'RootHBox',
             'patch': {'widthOverride': 0, 'heightOverride': 0, 'bOverride_WidthOverride': True,
                       'bOverride_HeightOverride': True, 'clipping': 'ClipToBoundsAlways',
                       'visibility': 'SelfHitTestInvisible'},
             'slot': {'size': {'value': 0, 'sizeRule': 'Automatic'}, 'padding': i._margin(),
                      'horizontalAlignment': 'HAlign_Fill', 'verticalAlignment': 'VAlign_Top'}},
            {'kind': 'move', 'name': name, 'parent': wrapper,
             'slot': {'padding': i._margin(), 'horizontalAlignment': 'HAlign_Fill',
                      'verticalAlignment': 'VAlign_Fill'}}])
    for operation in operations:
        _execute(operation)
    after = _snapshot()
    _preserved(before, after)
    for operation in operations:
        entry = after['templates'][operation['name']]
        require(entry['row']['parent'] == after['templates'][operation['parent']]['row']['widget'],
                'Designer wrapper parent mismatch')
        require(s._contains(entry['slot']['values'], operation['slot']), 'Designer wrapper slot mismatch')
        if operation['kind'] == 'add':
            require(s._contains(entry['widget']['values'], operation['patch']), 'Designer wrapper size mismatch')
    state.update({'state': 'authored', 'operations': operations, 'after': after, 'runtime_verified': False})
    s.write(receipt, state)
    gc.collect()
    return {'state': 'authored', 'operations': len(operations), 'backup': state['backup'],
            'native_compile_required': [ASSET], 'explicit_save_required': [ASSET],
            'render_check': 'FN_SkillsCanvas fills the 464px paper column; old scaffolding consumes 0px'}


if __name__ == '__main__':
    import sys
    require(sys.argv[1:] == ['--plan'], 'Offline execution only supports --plan')
    result = plan()
    print(json.dumps({'asset': result['asset'], 'operations': len(result['operations']),
                      'preserves': result['preserves'], 'editor_mutation_performed': False}, indent=2))
