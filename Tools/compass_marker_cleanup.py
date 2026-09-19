# @Description: Make owned navigation marker retirement immediate and late-fade safe.
"""Prepare -> native compile both marker widgets -> wire -> native compile/save.

Logical removal happens before the existing visual fade. A retiring widget never
removes a newer registration at its old key. Source-pack assets stay untouched.
"""
import gc
import importlib.util
from pathlib import Path
import unreal

spec = importlib.util.spec_from_file_location('az_compass_adapter', Path('C:/UnrealEngine/Games/AZ/Tools/compass_graph_adapter.py'))
A = importlib.util.module_from_spec(spec)
spec.loader.exec_module(A)
BL, GE, PL = A.BL, A.GE, A.PL
NAMES = ('WBP_AZ_CompassMarker', 'WBP_AZ_WorldMarker')


def prepare():
    A.idle()
    for name in NAMES:
        bp = A.load(A.DEST + '/' + name)
        if 'bRetiring' not in set(BL.list_member_variable_names(bp, False)):
            A.require(BL.add_member_variable(bp, 'bRetiring', BL.get_basic_type_by_name('bool')), 'Could not add retire guard')
    return {'next': 'Compile both owned marker Blueprints, then wire().'}


def only_link(pin):
    links = list(PL.list_connected_pins(pin))
    A.require(len(links) == 1, 'Expected one source connection: ' + str(PL.get_pin_name(pin)))
    return links[0]


def wire():
    A.idle()
    report = {}
    for name in NAMES:
        bp = A.load(A.DEST + '/' + name)
        if unreal.EditorAssetLibrary.get_metadata_tag(bp, 'AZ.Compass.SafeRemoval') == 'v1':
            report[name] = 'already wired'
            continue
        graph, editor = A.graph_nodes(bp, 'EventGraph')
        nodes = list(editor.list_all_nodes())
        events = [n for n in nodes if n.get_class().get_name() == 'K2Node_CustomEvent'
                  and A.normalized(A.title(n)) == 'removemarker']
        removes = [n for n in nodes if n.get_class().get_name() == 'K2Node_CallFunction'
                   and A.normalized(A.title(n)) in ('remove', 'mapremove')
                   and 'TargetMap' in [str(PL.get_pin_name(p)) for p in BL.list_input_pins(n)]]
        tick_label = 'eventtick' if name == 'WBP_AZ_CompassMarker' else 'ontickupdate'
        updates = [n for n in nodes if n.get_class().get_name() in ('K2Node_Event', 'K2Node_CustomEvent')
                   and A.normalized(A.title(n)) == tick_label]
        A.require(len(events) == len(removes) == len(updates) == 1, 'Source marker lifecycle is not uniquely identified')
        event, old_remove, tick = events[0], removes[0], updates[0]
        fade_start = only_link(A.pin(event, 'then', True))
        late_prev = only_link(A.pin(old_remove, 'execute'))
        late_next = only_link(A.pin(old_remove, 'then', True))
        map_value = only_link(A.pin(old_remove, 'TargetMap'))
        key_value = only_link(A.pin(old_remove, 'Key'))
        A.require(not PL.list_connected_pins(A.pin(old_remove, 'ReturnValue', True)), 'Removal result is used; inspect before changing lifetime')
        tick_next = only_link(A.pin(tick, 'then', True))
        retiring = editor.add_get_member_variable_node('bRetiring')
        parent = editor.add_get_member_variable_node('Parent')
        target = editor.add_get_member_variable_node('MarkerObject')
        remove_now = editor.add_call_function_node('/Script/Engine.BlueprintMapLibrary:Map_Remove')
        A.connect(map_value, A.pin(remove_now, 'TargetMap'))
        A.connect(key_value, A.pin(remove_now, 'Key'))
        set_retiring = editor.add_set_member_variable_node('bRetiring')
        PL.set_pin_value(A.pin(set_retiring, 'bRetiring'), 'true')
        guard = editor.add_branch_node()
        A.connect(A.pin(retiring, 'bRetiring', True), A.pin(guard, 'Condition'))
        PL.break_pin_links(A.pin(event, 'then', True))
        A.connect(A.pin(event, 'then', True), A.pin(guard, 'execute'))
        A.connect(A.pin(guard, 'else', True), A.pin(set_retiring, 'execute'))
        valid_parent = editor.add_call_function_node('/Script/Engine.KismetSystemLibrary:IsValid')
        parent_branch = editor.add_branch_node()
        A.connect(A.pin(parent, 'Parent', True), A.pin(valid_parent, 'Object'))
        A.connect(A.pin(valid_parent, 'ReturnValue', True), A.pin(parent_branch, 'Condition'))
        A.connect(A.pin(set_retiring, 'then', True), A.pin(parent_branch, 'execute'))
        A.connect(A.pin(parent_branch, 'then', True), A.pin(remove_now, 'execute'))
        orphan_remove = editor.add_call_function_node('/Script/UMG.Widget:RemoveFromParent')
        A.connect(A.pin(parent_branch, 'else', True), A.pin(orphan_remove, 'execute'))
        after_remove = A.pin(remove_now, 'then', True)
        if 'CanUpdate' in set(BL.list_member_variable_names(bp, False)):
            stop = editor.add_set_member_variable_node('CanUpdate')
            PL.set_pin_value(A.pin(stop, 'CanUpdate'), 'false')
            A.connect(after_remove, A.pin(stop, 'execute'))
            after_remove = A.pin(stop, 'then', True)
        A.connect(after_remove, fade_start)
        # The old delayed tail still unbinds/removes THIS widget. It no longer
        # touches the keyed registry, so re-add can safely create a new widget.
        PL.break_pin_links(A.pin(old_remove, 'execute'))
        PL.break_pin_links(A.pin(old_remove, 'then', True))
        A.connect(late_prev, late_next)
        editor.remove_nodes([old_remove])
        # Invalid targets retire instead of reaching the source's world-zero
        # location fallback. No extra world scan or animation tick is added.
        tick_guard, valid_target_branch = editor.add_branch_node(), editor.add_branch_node()
        valid_target = editor.add_call_function_node('/Script/Engine.KismetSystemLibrary:IsValid')
        request_remove = editor.add_call_function_node(bp.generated_class().get_path_name() + ':RemoveMarker')
        A.connect(A.pin(retiring, 'bRetiring', True), A.pin(tick_guard, 'Condition'))
        A.connect(A.pin(target, 'MarkerObject', True), A.pin(valid_target, 'Object'))
        A.connect(A.pin(valid_target, 'ReturnValue', True), A.pin(valid_target_branch, 'Condition'))
        PL.break_pin_links(A.pin(tick, 'then', True))
        A.connect(A.pin(tick, 'then', True), A.pin(tick_guard, 'execute'))
        A.connect(A.pin(tick_guard, 'else', True), A.pin(valid_target_branch, 'execute'))
        A.connect(A.pin(valid_target_branch, 'then', True), tick_next)
        A.connect(A.pin(valid_target_branch, 'else', True), A.pin(request_remove, 'execute'))
        unreal.EditorAssetLibrary.set_metadata_tag(bp, 'AZ.Compass.SafeRemoval', 'v1')
        report[name] = 'immediate logical detach; one visual fade; invalid-target guard'
    A.write('marker-removal-safe', report)
    gc.collect()
    return report
