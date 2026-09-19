# @Description: Wire explicit navigation initialization and child ownership on owned widgets.
"""Staged regular-Blueprint edits only; compile through native tools afterwards.

Never starts PIE, runs tests, compiles from Python, or modifies vendor assets.
"""
import gc
import importlib.util
import json
from pathlib import Path
import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
spec = importlib.util.spec_from_file_location('az_compass_graph_adapter', ROOT / 'Tools/compass_graph_adapter.py')
A = importlib.util.module_from_spec(spec)
spec.loader.exec_module(A)
BL, GE, PL = A.BL, A.GE, A.PL
DEST = A.DEST


def prepare():
    A.idle()
    bp = A.load(DEST + '/WBP_AZ_Compass')
    if 'InitializedNavigationContext' not in set(BL.list_member_variable_names(bp, False)):
        A.require(BL.add_member_variable(bp, 'InitializedNavigationContext',
                  BL.get_object_reference_type(A.load(A.MODULE).generated_class())), 'Cannot add initialization identity')
    return {'next': 'Compile WBP_AZ_Compass, then wire().'}


def splice_after(source, first, last):
    following = list(PL.list_connected_pins(source))
    A.require(len(following) <= 1, 'Unexpected execution fanout')
    PL.break_pin_links(source)
    A.connect(source, first)
    for other in following:
        A.connect(last, other)


def wire_init():
    bp = A.load(DEST + '/WBP_AZ_Compass')
    key = 'AZ.Compass.InitHook'
    if unreal.EditorAssetLibrary.get_metadata_tag(bp, key) == 'v1':
        return 'already wired'
    graph, editor = A.graph_nodes(bp, 'EventGraph')
    nodes = list(editor.list_all_nodes())
    events = [n for n in nodes if n.get_class().get_name() == 'K2Node_Event'
              and A.normalized(A.title(n)) in ('init', 'eventinit')]
    terminals = [n for n in nodes if n.get_class().get_name() == 'K2Node_VariableSet'
                 and any(str(PL.get_pin_name(p)) == 'Orientation' for p in BL.list_input_pins(n))]
    A.require(len(events) == len(terminals) == 1, 'Source Init/Orientation endpoint is not unique')
    begin, end = events[0], terminals[0]
    previous = list(PL.list_connected_pins(A.pin(begin, 'then', True)))
    A.require(len(previous) == 1 and not PL.list_connected_pins(A.pin(end, 'then', True)),
              'Unexpected initialization wiring; inspect before adding callback')
    ctx = editor.add_get_member_variable_node('NavigationContext')
    initialized = editor.add_get_member_variable_node('InitializedNavigationContext')
    valid = editor.add_call_function_node('/Script/Engine.KismetSystemLibrary:IsValid')
    same = editor.add_call_function_node('/Script/Engine.KismetMathLibrary:EqualEqual_ObjectObject')
    valid_branch, same_branch = editor.add_branch_node(), editor.add_branch_node()
    setter = editor.add_set_member_variable_node('InitializedNavigationContext')
    notify = editor.add_call_function_node(A.load(A.MODULE).generated_class().get_path_name() + ':NotifyCompassReady')
    self_node = editor.create_node_from_name('Variables|Getareferencetoself', unreal.Vector2D(0, 0), [], None)
    A.connect(A.pin(ctx, 'NavigationContext', True), A.pin(valid, 'Object'))
    A.connect(A.pin(ctx, 'NavigationContext', True), A.pin(same, 'A'))
    A.connect(A.pin(initialized, 'InitializedNavigationContext', True), A.pin(same, 'B'))
    A.connect(A.pin(valid, 'ReturnValue', True), A.pin(valid_branch, 'Condition'))
    A.connect(A.pin(same, 'ReturnValue', True), A.pin(same_branch, 'Condition'))
    PL.break_pin_links(A.pin(begin, 'then', True))
    A.connect(A.pin(begin, 'then', True), A.pin(valid_branch, 'execute'))
    A.connect(A.pin(valid_branch, 'then', True), A.pin(same_branch, 'execute'))
    A.connect(A.pin(same_branch, 'else', True), previous[0])
    A.connect(A.pin(end, 'then', True), A.pin(setter, 'execute'))
    A.connect(A.pin(ctx, 'NavigationContext', True), A.pin(setter, 'InitializedNavigationContext'))
    A.connect(A.pin(setter, 'then', True), A.pin(notify, 'execute'))
    # A deferred source Init after explicit Init reports the same completion,
    # without restarting presentation or resetting material state.
    A.connect(A.pin(same_branch, 'then', True), A.pin(notify, 'execute'))
    A.connect(A.pin(ctx, 'NavigationContext', True), A.pin(notify, 'self'))
    A.connect(A.pin(self_node, 'self', True), A.pin(notify, 'Compass'))
    unreal.EditorAssetLibrary.set_metadata_tag(bp, key, 'v1')
    return 'context identity guard and actual completion notification wired'


def wire_spawns():
    result = {}
    for parent, child in [('WBP_AZ_Compass', 'WBP_AZ_CompassMarker'),
                          ('WBP_AZ_WorldMarkerContainer', 'WBP_AZ_WorldMarker')]:
        bp = A.load(DEST + '/' + parent)
        if unreal.EditorAssetLibrary.get_metadata_tag(bp, 'AZ.Compass.SpawnContext') == 'v1':
            result[parent] = 'already wired'
            continue
        found = []
        for graph in A.graphs_under(bp):
            editor = GE.get_graph_editor(graph)
            for node in editor.list_all_nodes():
                if node.get_class().get_name() == 'K2Node_CreateWidget':
                    found.append((editor, node))
        A.require(len(found) == 1, 'Expected exactly one source marker factory per container')
        editor, create = found[0]
        value = str(PL.get_pin_value(A.pin(create, 'Class')))
        A.require(child in value, 'Remap child class before injecting context: ' + value)
        owner = editor.add_call_function_node('/Script/UMG.Widget:GetOwningPlayer')
        ctx = editor.add_get_member_variable_node('NavigationContext')
        set_ctx = editor.add_set_member_variable_node('NavigationContext', A.load(DEST + '/' + child).generated_class().get_path_name())
        A.connect(A.pin(owner, 'ReturnValue', True), A.pin(create, 'OwningPlayer'))
        A.connect(A.pin(create, 'ReturnValue', True), A.pin(set_ctx, 'self'))
        A.connect(A.pin(ctx, 'NavigationContext', True), A.pin(set_ctx, 'NavigationContext'))
        # Before AddChildToOverlay triggers Construct, the new marker has both
        # its player context and its style/configuration context.
        splice_after(A.pin(create, 'then', True), A.pin(set_ctx, 'execute'), A.pin(set_ctx, 'then', True))
        unreal.EditorAssetLibrary.set_metadata_tag(bp, 'AZ.Compass.SpawnContext', 'v1')
        result[parent] = 'owning player + settings before child Construct'
    return result


def wire():
    A.idle()
    result = {'init': wire_init(), 'spawns': wire_spawns()}
    A.write('lifecycle-hooks', result)
    gc.collect()
    return result
