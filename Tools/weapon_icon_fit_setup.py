"""Preserve weapon/item icon aspect ratios in existing CommonUI layouts.

Run via ProgrammaticToolset.execute_tool_script, which injects execute_tool and
calls run(). Importing this file performs no work. MODE='audit' is read-only.
MODE='author' requires BACKUP_READY=True after disk backups and the three native
UI function-body changes have been built. No inventory/gameplay data changes.

Wrap only the existing images in ScaleToFit, retain every outer slot/property,
and leave text/badges and quick-select/description layouts alone. Compile/save
the returned Widget Blueprints through dedicated native tools after return.
Then MODE='verify' with BASELINE set to the audit/before receipt verifies all
original widget properties, hierarchy, order and non-image slot geometry.
"""
import copy
import json


MODE = 'audit'
BACKUP_READY = False
BASELINE = None
UMG = 'UMGToolSet.UMGToolSet.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'
EDITOR = 'EditorToolset.EditorAppToolset.'
TARGETS = (
    ('/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD.WBP_AZ_GameHUD', 'WeaponIcon', 'WeaponIconAspectFit'),
    ('/Game/AZ/Blueprints/Menu/CommonInventory/CommonUI/UI/Widgets/WBP_CommonUI_SlottedItem.WBP_CommonUI_SlottedItem',
     'Image_Icon', 'ItemIconAspectFit'),
    ('/Game/AZ/Blueprints/Menu/CommonInventory/CommonUI/UI/Widgets/WBP_AZ_CommonUI_HoverItem.WBP_AZ_CommonUI_HoverItem',
     'Image_Icon', 'ItemIconAspectFit'),
)
EMPTY_EQUIPPED = ('/Game/AZ/Blueprints/Menu/CommonInventory/CommonUI/UI/Widgets/'
                  'WBP_AZ_CommonUI_EquippedSlottedItem.WBP_AZ_CommonUI_EquippedSlottedItem')
WIDGET_FIELDS = ('brush', 'renderTransform', 'renderTransformPivot', 'visibility', 'colorAndOpacity',
                 'padding', 'stretch', 'stretchDirection', 'text', 'font', 'justification')


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def ref(path):
    return {'refPath': path}


def call(prefix, name, **arguments):
    response = execute_tool(prefix + name, json.dumps(arguments))
    require(isinstance(response, dict) and 'returnValue' in response, 'Tool failed: ' + name)
    return response['returnValue']


def equal(actual, expected):
    if isinstance(expected, dict):
        return isinstance(actual, dict) and set(actual) == set(expected) and all(equal(actual[k], v) for k, v in expected.items())
    if isinstance(expected, list):
        return isinstance(actual, list) and len(actual) == len(expected) and all(equal(a, b) for a, b in zip(actual, expected))
    if isinstance(expected, (int, float)) and not isinstance(expected, bool):
        return isinstance(actual, (int, float)) and abs(actual - expected) < 0.0001
    return actual == expected


def properties(instance, names=None, changes=None):
    # Property names are discovered on EACH concrete widget/slot; no guesses.
    schema = json.loads(call(OBJ, 'list_properties', instance=instance))
    selected = list(changes) if changes is not None else (list(schema) if names is None else [n for n in names if n in schema])
    require(not set(selected) - set(schema), 'Unsupported properties: ' + str(set(selected) - set(schema)))
    if not selected:
        return {}
    if changes is not None:
        require(call(OBJ, 'set_properties', instance=instance, values=json.dumps(changes)), 'Property write failed: ' + str(instance))
    result = json.loads(call(OBJ, 'get_properties', instance=instance, properties=selected))
    if changes is not None:
        require(equal(result, changes), 'Property readback failed: ' + str(instance))
    return result


def tree(path):
    return call(UMG, 'GetWidgets', widgetBlueprint=ref(path))


def real_widgets(data):
    return [w for w in data['widgets'] if isinstance(w.get('widget'), dict)]


def snapshot(path):
    data = tree(path)
    rows = real_widgets(data)
    captured = []
    for row in rows:
        item = {k: copy.deepcopy(row[k]) for k in ('widgetName', 'widgetClassPath', 'widget', 'parent', 'bIsVariable', 'bInherited')}
        item['properties'] = properties(row['widget'], names=WIDGET_FIELDS)
        item['slot_properties'] = properties(row['slot']) if isinstance(row.get('slot'), dict) else None
        captured.append(item)
    return {'path': path, 'info': data['info'], 'widgets': captured}


def audit():
    data = {path: snapshot(path) for path, _, _ in TARGETS}
    for path, image_name, wrapper_name in TARGETS:
        rows = {w['widgetName']: w for w in data[path]['widgets']}
        require(image_name in rows and rows[image_name]['widgetClassPath'] == ref('/Script/UMG.Image'), 'Required existing image is missing: ' + path)
        require(rows[image_name]['slot_properties'] is not None, 'Expected image to have an existing outer slot')
        if wrapper_name in rows:
            require(rows[wrapper_name]['widgetClassPath'] == ref('/Script/UMG.ScaleBox')
                    and rows[image_name]['parent'] == rows[wrapper_name]['widget'], 'Existing wrapper has different ownership')
        else:
            require(rows[image_name]['parent'] in [w['widget'] for w in data[path]['widgets']], 'Image parent is outside authored tree')
    equipped = tree(EMPTY_EQUIPPED)
    require(not real_widgets(equipped), 'Equipped widget now has a tree; include its actual Image_Icon in this audit before authoring')
    return {'status': 'audit', 'targets': data,
            'equipped': {'path': EMPTY_EQUIPPED, 'status': 'empty_widget_tree_preserved', 'info': equipped['info']}}


def author(before):
    for path, image_name, wrapper_name in TARGETS:
        original = {w['widgetName']: w for w in before['targets'][path]['widgets']}
        require(wrapper_name not in original, 'Already authored; run verify instead of wrapping twice: ' + path)
    for path, image_name, wrapper_name in TARGETS:
        original = {w['widgetName']: w for w in before['targets'][path]['widgets']}
        image = original[image_name]
        wrapped = call(UMG, 'WrapWidgets', widgetBlueprint=ref(path), widgets=[image['widget']], wrapperClass=ref('/Script/UMG.ScaleBox'))
        require(isinstance(wrapped, list) and len(wrapped) == 1 and isinstance(wrapped[0].get('widget'), dict), 'Image wrapping failed')
        wrapper = call(UMG, 'RenameWidget', widgetBlueprint=ref(path), widget=wrapped[0]['widget'], newDisplayName=wrapper_name)
        require(wrapper and wrapper['widgetName'] == wrapper_name, 'Wrapper naming failed')
        # The wrapper occupies the EXACT prior Canvas/Overlay slot, including z-order and padding.
        properties(wrapper['slot'], changes=image['slot_properties'])
        properties(wrapper['widget'], changes={'stretch': 'ScaleToFit', 'stretchDirection': 'Both',
                                               'visibility': 'SelfHitTestInvisible'})
        now = {w['widgetName']: w for w in real_widgets(tree(path))}
        image_slot = now[image_name]['slot']
        properties(image_slot, changes={'horizontalAlignment': 'HAlign_Center', 'verticalAlignment': 'VAlign_Center'})


def verify(before):
    require(before and before.get('targets'), 'Pass the original audit/before receipt as BASELINE')
    after = audit()
    for path, image_name, wrapper_name in TARGETS:
        original_rows = before['targets'][path]['widgets']
        current_rows = after['targets'][path]['widgets']
        current = {w['widgetName']: w for w in current_rows}
        original = {w['widgetName']: w for w in original_rows}
        require(wrapper_name not in original, 'BASELINE must be captured before authoring')
        require(set(current) == set(original) | {wrapper_name}, 'Unexpected widget additions/removals: ' + path)
        require([w['widgetName'] for w in current_rows if w['widgetName'] != wrapper_name]
                == [w['widgetName'] for w in original_rows], 'Original walk/sibling order changed')
        wrapper = current[wrapper_name]
        require(wrapper['parent'] == original[image_name]['parent'], 'Wrapper moved original image placement')
        require(equal(wrapper['slot_properties'], original[image_name]['slot_properties']), 'Original outer slot geometry changed')
        require(wrapper['properties'].get('stretch') == 'ScaleToFit'
                and wrapper['properties'].get('stretchDirection') == 'Both', 'ScaleBox aspect policy differs')
        for name, old in original.items():
            new = current[name]
            for field in ('widgetClassPath', 'bIsVariable', 'bInherited', 'properties'):
                require(equal(new[field], old[field]), 'Existing widget property changed: ' + path + '/' + name + '/' + field)
            if name == image_name:
                require(new['parent'] == wrapper['widget'], 'Image is not inside expected fit wrapper')
                require(new['slot_properties'].get('horizontalAlignment') == 'HAlign_Center'
                        and new['slot_properties'].get('verticalAlignment') == 'VAlign_Center', 'Image must be centered inside ScaleBox')
            else:
                require(new['parent'] == old['parent'] and equal(new['slot_properties'], old['slot_properties']),
                        'Unrelated widget hierarchy/slot changed: ' + name)
    return {'status': 'verified', 'targets': [p for p, _, _ in TARGETS], 'after': after}


def run():
    require(MODE in ('audit', 'author', 'verify'), 'Unknown mode: ' + MODE)
    if MODE == 'verify':
        return verify(BASELINE)
    before = audit()
    if MODE == 'audit':
        return before
    require(BACKUP_READY, 'Root must back up these exact Widget Blueprint packages before authoring')
    require(not call(EDITOR, 'IsPIERunning'), 'Stop PIE before editing Widget Blueprints')
    author(before)
    verified = verify(before)
    return {'status': 'authored_native_compile_save_required', 'before': before, 'readback': verified,
            'compile_after_return': [p for p, _, _ in TARGETS],
            'save_after_compile': [p.split('.')[0] for p, _, _ in TARGETS]}
