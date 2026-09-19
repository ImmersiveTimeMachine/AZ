# @Description: Extract ProHUD navigation helpers into explicitly configured CHALK copies.
"""Owned regular Blueprint authoring; compile separately through native tools.

No Play, tests, C++ builds, source-pack edits, or animation edits. Steps are
recorded under Saved/CompassIntegration and stop on unexpected signatures.
"""
import gc
import importlib.util
import json
import re
from pathlib import Path
import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'Saved/CompassIntegration'
DEST = '/Game/AZ/Blueprints/Menu/HUD/Navigation'
MODULE = DEST + '/WBP_AZ_CompassModule'
LIBS = {'settings': ('/Game/ProHUDV2_Horror/Blueprints/Libraries/BP_PHV2_Functions_H', DEST + '/BFL_AZ_NavigationSettings'),
        'widgets': ('/Game/ProHUDV2_Horror/Blueprints/Libraries/BP_PHV2_Widget_Functions_H', DEST + '/BFL_AZ_NavigationWidgets')}
LEAVES = [DEST + '/' + n for n in ['WBP_AZ_Compass', 'WBP_AZ_CompassMarker', 'WBP_AZ_WorldMarkerContainer', 'WBP_AZ_WorldMarker']]
BL = unreal.BlueprintEditorLibrary
GE = unreal.BlueprintGraphEditor
PL = unreal.BlueprintGraphPinLibrary


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def write(name, value):
    (OUT / (name + '.json')).write_text(json.dumps(value, indent=2), encoding='utf-8')


def load(path):
    ob = unreal.load_asset(path)
    require(ob is not None, 'Missing asset: ' + path)
    return ob


def idle():
    result = unreal.ToolsetRegistry.execute_tool('EditorToolset.EditorAppToolset', 'IsPIERunning', '{}')
    require(not result.error and not json.loads(result.value)['returnValue'], 'Editor must be idle')


def normalized(name):
    return re.sub(r'[^a-z0-9]', '', name.lower())


def helper_closure():
    data = json.loads((OUT / 'helper-closure-source.json').read_text(encoding='utf-8'))
    indices = {k: {normalized(n): n for n in data[k]['graphs']} for k in LIBS}
    owner_names = {p[0].rsplit('/', 1)[1] + '_C': k for k, p in LIBS.items()}
    def deps(lines, current=None):
        result = set()
        title = None
        owner_key = None
        def finish():
            if title:
                k = owner_key or current
                if k in indices and normalized(title) in indices[k]:
                    result.add((k, indices[k][normalized(title)]))
        for line in lines:
            header = re.match(r'^\s*\[[A-F0-9]+\] (\S+) \| (.*)$', line)
            if header:
                finish()
                title = header.group(2) if header.group(1) == 'CallFunction' else None
                owner_key = None
            owner = re.search(r'in\s+self:object = Default__(\S+)', line)
            if title and owner and owner.group(1) in owner_names:
                k = owner_names[owner.group(1)]
                owner_key = k
                key = normalized(title)
                require(key in indices[k], 'Unknown helper function title: ' + str((k, title)))
                result.add((k, indices[k][key]))
        finish()
        return result
    pending = set()
    for key in ('compass', 'compass_marker', 'world', 'world_marker'):
        for lines in data[key]['graphs'].values():
            pending.update(deps(lines))
    keep = set()
    while pending:
        key, name = pending.pop()
        if (key, name) in keep:
            continue
        keep.add((key, name))
        pending.update(deps(data[key]['graphs'][name], key) - keep)
    require(('settings', 'GetHUDManager_H') not in keep, 'Unexpected vendor AHUD dependency')
    result = {k: sorted(n for group, n in keep if group == k) for k in LIBS}
    write('helper-closure', result)
    return result


def pin(node, name, output=False):
    pins = BL.list_output_pins(node) if output else BL.list_input_pins(node)
    for p in pins:
        if str(PL.get_pin_name(p)) == name:
            return p
    raise RuntimeError('Missing pin ' + name + ' on ' + node.get_path_name())


def connect(a, b):
    require(PL.try_create_connection(a, b), 'Pin connection failed: ' + str(PL.get_pin_name(a)) + ' -> ' + str(PL.get_pin_name(b)))


def graph_nodes(bp, name):
    graph = BL.find_graph(bp, name)
    require(graph is not None, 'Missing graph: ' + name)
    return graph, GE.get_graph_editor(graph)


def prepare_helpers():
    idle()
    require(not (OUT / 'helper-signatures.json').exists(), 'Helper signatures already prepared; resume rather than duplicate')
    keep = helper_closure()
    module = load(MODULE)
    context_type = BL.get_object_reference_type(module.generated_class())
    result = {'helpers': {}, 'context_member': 'NavigationContext'}
    for key, (source, target) in LIBS.items():
        if unreal.EditorAssetLibrary.does_asset_exist(target):
            bp = load(target)
            require(unreal.EditorAssetLibrary.get_metadata_tag(bp, 'AZ.Compass.AuthoringOwner') == 'compass_integration_setup:v1', 'Unowned destination: ' + target)
        else:
            bp = unreal.EditorAssetLibrary.duplicate_asset(source, target)
        require(bp is not None, 'Could not copy ' + source)
        unreal.EditorAssetLibrary.set_metadata_tag(bp, 'AZ.Compass.AuthoringOwner', 'compass_integration_setup:v1')
        removed = []
        for name in list(BL.list_graph_names(bp)):
            name = str(name)
            if name not in keep[key]:
                BL.remove_function_graph(bp, name)
                removed.append(name)
        for name in keep[key]:
            graph, editor = graph_nodes(bp, name)
            entry = next(n for n in editor.list_all_nodes() if n.get_class().get_name() == 'K2Node_FunctionEntry')
            if 'NavigationContext' not in [str(PL.get_pin_name(p)) for p in BL.list_output_pins(entry)]:
                editor.add_graph_input_parameter('NavigationContext', context_type)
        result['helpers'][key] = {'path': target, 'retained': keep[key], 'removed': removed}
    for path in LEAVES:
        bp = load(path)
        names = set(BL.list_member_variable_names(bp, False))
        if 'NavigationContext' not in names:
            require(BL.add_member_variable(bp, 'NavigationContext', context_type), 'Could not add explicit context')
            BL.set_blueprint_variable_category(bp, 'NavigationContext', 'Navigation|Context')
            BL.set_blueprint_variable_expose_on_spawn(bp, 'NavigationContext', True)
            BL.set_blueprint_variable_instance_editable(bp, 'NavigationContext', True)
    if 'NorthYawDegrees' not in set(BL.list_member_variable_names(module, False)):
        require(BL.add_member_variable(module, 'NorthYawDegrees', BL.get_basic_type_by_name('float')), 'Could not add north setting')
        BL.set_blueprint_variable_category(module, 'NorthYawDegrees', 'Navigation|North')
    write('helper-signatures', result)
    gc.collect()
    return result


def title(node):
    return str(BL.get_node_title(node))


def transfer_node(old, new, editor, skip=(), rename=None):
    """Preserve every compatible data/exec connection and literal by pin name."""
    new_in = {str(PL.get_pin_name(p)): p for p in BL.list_input_pins(new)}
    new_out = {str(PL.get_pin_name(p)): p for p in BL.list_output_pins(new)}
    # Call-node recreation restores unsplit struct pins. Preserve source split
    # Vector2D connections through explicit Make/Break nodes, without changing math.
    for output, collection in ((False, BL.list_input_pins(old)), (True, BL.list_output_pins(old))):
        target = new_out if output else new_in
        made = set()
        for old_pin in collection:
            name = str(PL.get_pin_name(old_pin))
            if name in target or name in skip or not PL.list_connected_pins(old_pin):
                continue
            base, _, component = name.rpartition('_')
            if component in ('X', 'Y') and base in target and base not in made:
                if 'vector2d' in normalized(str(PL.get_pin_type_display_string(target[base]))):
                    fn = 'BreakVector2D' if output else 'MakeVector2D'
                    adapter = editor.add_call_function_node('/Script/Engine.KismetMathLibrary:' + fn)
                    if output:
                        connect(target[base], pin(adapter, 'InVec'))
                    else:
                        connect(pin(adapter, 'ReturnValue', True), target[base])
                    for axis in ('X', 'Y'):
                        target[base + '_' + axis] = pin(adapter, axis, output)
                    made.add(base)
    # Validate the whole signature BEFORE moving any existing link.
    for output, collection in ((False, BL.list_input_pins(old)), (True, BL.list_output_pins(old))):
        for p in collection:
            name = str(PL.get_pin_name(p))
            if name in skip or not PL.list_connected_pins(p):
                continue
            require((rename or {}).get(name, name) in (new_out if output else new_in),
                    'Connected pin lost on replacement: ' + name + ' / ' + old.get_path_name())
    for output, collection in ((False, BL.list_input_pins(old)), (True, BL.list_output_pins(old))):
        for old_pin in collection:
            name = str(PL.get_pin_name(old_pin))
            if name in skip:
                continue
            linked = list(PL.list_connected_pins(old_pin))
            candidates = new_out if output else new_in
            target_name = (rename or {}).get(name, name)
            require(target_name in candidates or not linked, 'Connected pin lost on replacement: ' + name + ' / ' + old.get_path_name())
            if target_name not in candidates:
                continue
            new_pin = candidates[target_name]
            if not output and not linked and not PL.list_connected_pins(new_pin):
                value = PL.get_pin_value(old_pin)
                if value:
                    PL.set_pin_value(new_pin, value)
            for other in linked:
                PL.break_single_pin_link(old_pin, other)
                connect(new_pin, other)
    editor.remove_nodes([old])


def context_pin(editor):
    entry = next(n for n in editor.list_all_nodes() if n.get_class().get_name() == 'K2Node_FunctionEntry')
    return pin(entry, 'NavigationContext', True)


def adapt_helpers():
    idle()
    require(not (OUT / 'helper-adapted.json').exists(), 'Helpers already adapted; inspect rather than rewrite')
    keep = json.loads((OUT / 'helper-closure.json').read_text(encoding='utf-8'))
    index = {k: {normalized(n): n for n in names} for k, names in keep.items()}
    module_class = load(MODULE).generated_class().get_path_name()
    counts = {'container_calls_removed': 0, 'setting_reads': 0, 'helper_calls': 0}
    for key, (_, path) in LIBS.items():
        bp = load(path)
        for name in keep[key]:
            if name == 'GetHUD_Container_H':
                continue
            graph, editor = graph_nodes(bp, name)
            ctx = context_pin(editor)
            if key == 'settings' and name == 'GetCardinalDirection':
                # Source fallback -90 is now an explicit per-module north angle.
                # Preserve rounded-degree and downstream orientation semantics.
                entry = next(n for n in editor.list_all_nodes() if n.get_class().get_name() == 'K2Node_FunctionEntry')
                returns = [n for n in editor.list_all_nodes() if n.get_class().get_name() == 'K2Node_FunctionResult']
                require(returns, 'Missing north return')
                ret = returns[0]
                PL.break_pin_links(pin(entry, 'then', True))
                PL.break_pin_links(pin(ret, 'execute'))
                PL.break_pin_links(pin(ret, 'ReturnValue'))
                editor.remove_nodes([n for n in editor.list_all_nodes() if n not in [entry, ret]])
                north = editor.add_get_member_variable_node('NorthYawDegrees', module_class)
                rounding = editor.add_call_function_node('/Script/Engine.KismetMathLibrary:Round')
                valid = editor.add_call_function_node('/Script/Engine.KismetSystemLibrary:IsValid')
                branch = editor.add_branch_node()
                connect(ctx, pin(north, 'self'))
                connect(ctx, pin(valid, 'Object'))
                connect(pin(entry, 'then', True), pin(branch, 'execute'))
                connect(pin(valid, 'ReturnValue', True), pin(branch, 'Condition'))
                connect(pin(branch, 'then', True), pin(ret, 'execute'))
                connect(pin(north, 'NorthYawDegrees', True), pin(rounding, 'A'))
                connect(pin(rounding, 'ReturnValue', True), pin(ret, 'ReturnValue'))
                fallback = editor.add_return_node()
                connect(pin(branch, 'else', True), pin(fallback, 'execute'))
                PL.set_pin_value(pin(fallback, 'ReturnValue'), '-90')
                continue
            # Snapshot first; new getter nodes must not be rewritten again.
            for node in list(editor.list_all_nodes()):
                cls = node.get_class().get_name()
                label = normalized(title(node))
                if cls == 'K2Node_CallFunction' and label == normalized('GetHUD_Container_H'):
                    valid = editor.add_call_function_node('/Script/Engine.KismetSystemLibrary:IsValid')
                    connect(ctx, pin(valid, 'Object'))
                    transfer_node(node, valid, editor, ('self', '__WorldContext', 'NavigationContext', 'HUD_Container'))
                    counts['container_calls_removed'] += 1
                elif key == 'settings' and cls == 'K2Node_VariableGet':
                    # Source library getters read only the source container fields;
                    # these exact fields were copied, with their actual types.
                    outputs = [p for p in BL.list_output_pins(node) if str(PL.get_pin_name(p)) not in ('then', 'else')]
                    require(len(outputs) == 1, 'Unexpected setting getter signature')
                    field = str(PL.get_pin_name(outputs[0]))
                    new = editor.add_get_member_variable_node(field, module_class)
                    connect(ctx, pin(new, 'self'))
                    transfer_node(node, new, editor, ('self',))
                    counts['setting_reads'] += 1
            # Every retained library call receives the same explicit context.
            # Native UFunction paths eliminate ambiguous action-menu titles.
            for node in list(editor.list_all_nodes()):
                if node.get_class().get_name() != 'K2Node_CallFunction':
                    continue
                self_pins = [p for p in BL.list_input_pins(node) if str(PL.get_pin_name(p)) == 'self']
                if self_pins:
                    # E.g. UOverlay::AddChildToOverlay shares the wrapper name.
                    # An instance target is not a static helper-library call.
                    target_type = normalized(str(PL.get_pin_type_display_string(self_pins[0])))
                    library_names = [normalized(p.rsplit('/', 1)[1]) for pair in LIBS.values() for p in pair]
                    if not any(n in target_type for n in library_names):
                        continue
                label = normalized(title(node))
                matches = [(k, ix[label]) for k, ix in index.items() if label in ix and ix[label] != 'GetHUD_Container_H']
                if not matches:
                    continue
                require(len(matches) == 1, 'Ambiguous helper title: ' + title(node))
                target_key, function = matches[0]
                target_bp = load(LIBS[target_key][1])
                # Failed/staged compile can retain the previous generated function
                # signature. The editor skeleton is authoritative while authoring.
                target_path = LIBS[target_key][1]
                skeleton_path = target_path + '.SKEL_' + target_path.rsplit('/', 1)[1] + '_C'
                new = editor.add_call_function_node(function if target_key == key else skeleton_path + ':' + function)
                connect(ctx, pin(new, 'NavigationContext'))
                transfer_node(node, new, editor, ('self', 'NavigationContext'))
                counts['helper_calls'] += 1
        if key == 'settings':
            BL.remove_function_graph(bp, 'GetHUD_Container_H')
    write('helper-adapted', counts)
    gc.collect()
    return counts


def graphs_under(bp):
    prefix = bp.get_path_name() + ':'
    return sorted([g for g in unreal.ObjectIterator(unreal.EdGraph)
                   if g.get_path_name().startswith(prefix)], key=lambda g: g.get_path_name())


def adapt_leaf_context():
    idle()
    keep = json.loads((OUT / 'helper-closure.json').read_text(encoding='utf-8'))
    index = {normalized(n): (key, n) for key, names in keep.items() for n in names if n != 'GetHUD_Container_H'}
    result = {}
    for path in LEAVES:
        stage = OUT / (path.rsplit('/', 1)[1] + '-context.json')
        if stage.exists():
            result[path] = 'already authored'
            continue
        bp = load(path)
        local_names = {normalized(str(n)) for n in BL.list_graph_names(bp)}
        count = {'helper_calls': 0, 'camera_sources': 0, 'controller_sources': 0}
        for graph in graphs_under(bp):
            editor = GE.get_graph_editor(graph)
            context = None
            for node in list(editor.list_all_nodes()):
                if node.get_class().get_name() != 'K2Node_CallFunction':
                    continue
                label = normalized(title(node))
                if label == 'getplayercameramanager':
                    owner = editor.add_call_function_node('/Script/UMG.Widget:GetOwningPlayer')
                    manager = editor.add_get_member_variable_node('PlayerCameraManager', '/Script/Engine.PlayerController')
                    connect(pin(owner, 'ReturnValue', True), pin(manager, 'self'))
                    transfer_node(node, manager, editor, ('self', 'PlayerIndex', 'WorldContextObject', '__WorldContext'),
                                  {'ReturnValue': 'PlayerCameraManager'})
                    count['camera_sources'] += 1
                    continue
                if label == 'getplayercontroller':
                    owner = editor.add_call_function_node('/Script/UMG.Widget:GetOwningPlayer')
                    transfer_node(node, owner, editor, ('self', 'PlayerIndex', 'WorldContextObject', '__WorldContext'))
                    count['controller_sources'] += 1
                    continue
                if label not in index or label in local_names:
                    continue
                self_pins = [p for p in BL.list_input_pins(node) if str(PL.get_pin_name(p)) == 'self']
                if self_pins:
                    target_type = normalized(str(PL.get_pin_type_display_string(self_pins[0])))
                    library_names = [normalized(p.rsplit('/', 1)[1]) for pair in LIBS.values() for p in pair]
                    if not any(n in target_type for n in library_names):
                        continue
                context_pins = [p for p in BL.list_input_pins(node) if str(PL.get_pin_name(p)) == 'NavigationContext']
                if context_pins and PL.list_connected_pins(context_pins[0]):
                    continue
                if context is None:
                    context = editor.add_get_member_variable_node('NavigationContext')
                key, function = index[label]
                cls = load(LIBS[key][1]).generated_class()
                new = editor.add_call_function_node(cls.get_path_name() + ':' + function)
                connect(pin(context, 'NavigationContext', True), pin(new, 'NavigationContext'))
                transfer_node(node, new, editor, ('self', 'NavigationContext'))
                count['helper_calls'] += 1
        result[path] = count
        write(path.rsplit('/', 1)[1] + '-context', count)
    write('leaf-context-adapted', result)
    gc.collect()
    return result

