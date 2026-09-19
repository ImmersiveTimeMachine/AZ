# @Description: Use the owned marker maps as the authoritative navigation registry.
"""Run wire() while Idle, then native compile/save the two owned containers.

No PIE, tests, asset saving, compilation or vendor/animation edits occur here.
Visual children may outlive their logical registration during a fade; they must
not participate in lookup. Removal accepts the exact stored target key even if
its Actor is pending kill. Registration still uses the source target validation.
"""
import importlib.util
import json
from pathlib import Path
import unreal

_spec = importlib.util.spec_from_file_location('az_compass_registry_adapter', Path(__file__).with_name('compass_graph_adapter.py'))
A = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(A)
BL, GE, PL = A.BL, A.GE, A.PL
KEY = 'AZ.Compass.MapRegistry'
CONTAINERS = (
    ('WBP_AZ_Compass', 'WBP_AZ_CompassMarker', 'RemoveCompassMarker'),
    ('WBP_AZ_WorldMarkerContainer', 'WBP_AZ_WorldMarker', 'RemoveWorldMarker'),
)


def one(items, reason):
    A.require(len(items) == 1, reason + ': ' + str(len(items)))
    return items[0]


def connect(source, node, name):
    A.connect(source, A.pin(node, name))


def map_lookup(editor, key):
    registry = editor.add_get_member_variable_node('MarkerMap')
    find = editor.add_call_function_node('/Script/Engine.BlueprintMapLibrary:Map_Find')
    connect(A.pin(registry, 'MarkerMap', True), find, 'TargetMap')
    connect(key, find, 'Key')
    return registry, find


def valid_value(editor, find):
    valid = editor.add_call_function_node('/Script/Engine.KismetSystemLibrary:IsValid')
    connect(A.pin(find, 'Value', True), valid, 'Object')
    branch = editor.add_branch_node()
    connect(A.pin(valid, 'ReturnValue', True), branch, 'Condition')
    return branch


def retiring_value(editor, find, marker_class):
    get = editor.add_get_member_variable_node('bRetiring', marker_class)
    connect(A.pin(find, 'Value', True), get, 'self')
    branch = editor.add_branch_node()
    connect(A.pin(get, 'bRetiring', True), branch, 'Condition')
    return branch


def replace_in_use(bp, marker_class):
    _, editor = A.graph_nodes(bp, 'MarkerObjectInUse')
    nodes = list(editor.list_all_nodes())
    entry = one([n for n in nodes if n.get_class().get_name() == 'K2Node_FunctionEntry'], 'Function entry')
    result = one([n for n in nodes if n.get_class().get_name() == 'K2Node_FunctionResult'], 'Function result')
    # Vendor signatures differ: Compass takes Object, World takes MarkerObject.
    # Validate both signatures before touching any existing links/body nodes.
    key = one([p for p in BL.list_output_pins(entry)
               if str(PL.get_pin_name(p)) in ('Object', 'MarkerObject')], 'Marker lookup key input')
    A.pin(entry, 'then', True)
    A.pin(result, 'execute')
    A.pin(result, 'ReturnValue')
    # Keep the function/signature (including its existing pure flag) and replace
    # only its body. Separate execution branches prevent reading bRetiring on an
    # invalid widget; a Blueprint boolean AND does not short-circuit pure inputs.
    for node in (entry, result):
        for pin in BL.list_all_pins(node):
            PL.break_pin_links(pin)
    editor.remove_nodes([n for n in nodes if n not in (entry, result)])
    _, find = map_lookup(editor, key)
    found = editor.add_branch_node()
    connect(A.pin(entry, 'then', True), found, 'execute')
    connect(A.pin(find, 'ReturnValue', True), found, 'Condition')
    valid = valid_value(editor, find)
    connect(A.pin(found, 'then', True), valid, 'execute')
    retiring = retiring_value(editor, find, marker_class)
    connect(A.pin(valid, 'then', True), retiring, 'execute')
    A.require(PL.set_pin_value(A.pin(result, 'ReturnValue'), 'false'), 'False return default')
    for source in (A.pin(found, 'else', True), A.pin(valid, 'else', True), A.pin(retiring, 'then', True)):
        connect(source, result, 'execute')
    success = editor.add_return_node()
    A.require(PL.set_pin_value(A.pin(success, 'ReturnValue'), 'true'), 'True return default')
    connect(A.pin(retiring, 'else', True), success, 'execute')


def replace_remove(bp, marker_class, event_name):
    _, editor = A.graph_nodes(bp, 'EventGraph')
    nodes = list(editor.list_all_nodes())
    event = one([n for n in nodes if n.get_class().get_name() == 'K2Node_CustomEvent'
                 and A.normalized(A.title(n)) == A.normalized(event_name)], 'Remove event')
    # Preflight the isolated source chain before removing just these three nodes.
    old_in_use = one(list(PL.list_connected_pins(A.pin(event, 'then', True))), 'Old removal successor').get_owning_node()
    A.require(old_in_use.get_class().get_name() == 'K2Node_MacroInstance'
              and A.normalized(A.title(old_in_use)) == 'ismarkerobjectinuse', 'Unexpected remove chain')
    execs = [p for p in BL.list_output_pins(old_in_use) if A.normalized(str(PL.get_pin_name(p))) == 'isinuse']
    old_find = one(list(PL.list_connected_pins(one(execs, 'In-use exec output'))), 'Old Find successor').get_owning_node()
    A.require(old_find.get_class().get_name() == 'K2Node_MacroInstance'
              and A.normalized(A.title(old_find)) == 'findmarker', 'Unexpected Find macro')
    calls = [p.get_owning_node() for pin in BL.list_output_pins(old_find)
             for p in PL.list_connected_pins(pin)]
    old_call = one(list(set(calls)), 'Old RemoveMarker call')
    A.require(old_call.get_class().get_name() == 'K2Node_CallFunction'
              and A.normalized(A.title(old_call)) == 'removemarker', 'Unexpected terminal removal')
    A.require(not PL.list_connected_pins(A.pin(old_call, 'then', True)), 'Removal has downstream work')
    registry, find = map_lookup(editor, A.pin(event, 'MarkerObject', True))
    found = editor.add_branch_node()
    valid = valid_value(editor, find)
    retiring = retiring_value(editor, find, marker_class)
    call = editor.add_call_function_node(marker_class + ':RemoveMarker')
    prune = editor.add_call_function_node('/Script/Engine.BlueprintMapLibrary:Map_Remove')
    connect(A.pin(registry, 'MarkerMap', True), prune, 'TargetMap')
    connect(A.pin(event, 'MarkerObject', True), prune, 'Key')
    connect(A.pin(find, 'ReturnValue', True), found, 'Condition')
    connect(A.pin(found, 'then', True), valid, 'execute')
    connect(A.pin(valid, 'then', True), retiring, 'execute')
    connect(A.pin(valid, 'else', True), prune, 'execute')
    connect(A.pin(retiring, 'then', True), prune, 'execute')
    connect(A.pin(retiring, 'else', True), call, 'execute')
    connect(A.pin(find, 'Value', True), call, 'self')
    # RemoveMarker itself immediately detaches its key and starts the fade. Do
    # not remove again afterward: callbacks may already have registered a new
    # widget at this key. Absent keys are an idempotent no-op.
    PL.break_pin_links(A.pin(event, 'then', True))
    connect(A.pin(event, 'then', True), found, 'execute')
    editor.remove_nodes([old_in_use, old_find, old_call])


def wire_remove_all():
    """Retain the source key snapshot, route every key through exact-key removal."""
    A.idle()
    bp = A.load(A.DEST + '/WBP_AZ_Compass')
    key = KEY + '.RemoveAll'
    if unreal.EditorAssetLibrary.get_metadata_tag(bp, key) == 'v1':
        return 'already wired'
    A.require(unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY) == 'v1', 'Single-key patch required')
    _, editor = A.graph_nodes(bp, 'EventGraph')
    nodes = list(editor.list_all_nodes())
    event = one([n for n in nodes if n.get_class().get_name() == 'K2Node_CustomEvent'
                 and A.title(n) == 'RemoveAllMarkers'], 'RemoveAll event')
    keys = one(list(PL.list_connected_pins(A.pin(event, 'then', True))), 'Keys snapshot').get_owning_node()
    loop = one(list(PL.list_connected_pins(A.pin(keys, 'then', True))), 'Keys loop').get_owning_node()
    old_in_use = one(list(PL.list_connected_pins(A.pin(loop, 'LoopBody', True))), 'Old RemoveAll lookup').get_owning_node()
    A.require(A.normalized(A.title(old_in_use)) == 'ismarkerobjectinuse', 'Unexpected RemoveAll lookup')
    old_find = one(list(PL.list_connected_pins(A.pin(old_in_use, 'IsInUse', True))), 'Old RemoveAll Find').get_owning_node()
    A.require(A.normalized(A.title(old_find)) == 'findmarker', 'Unexpected RemoveAll Find')
    old_call = one(list(PL.list_connected_pins(A.pin(old_find, 'Found', True))), 'Old RemoveAll call').get_owning_node()
    A.require(A.title(old_call) == 'RemoveMarker' and not PL.list_connected_pins(A.pin(old_call, 'then', True)), 'Unexpected RemoveAll terminal')
    key_value = A.pin(loop, 'Array Element', True)
    call = editor.add_call_function_node(bp.generated_class().get_path_name() + ':RemoveCompassMarker')
    connect(key_value, call, 'MarkerObject')
    PL.break_pin_links(A.pin(loop, 'LoopBody', True))
    connect(A.pin(loop, 'LoopBody', True), call, 'execute')
    editor.remove_nodes([old_in_use, old_find, old_call])
    unreal.EditorAssetLibrary.set_metadata_tag(bp, key, 'v1')
    return 'source key snapshot invokes exact-key removal, including invalid identities'


def wire():
    A.idle()
    report = {}
    prepared = []
    for name, marker, event in CONTAINERS:
        bp, child = A.load(A.DEST + '/' + name), A.load(A.DEST + '/' + marker)
        stage = unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY)
        if stage == 'v1':
            report[name] = 'already wired'
            continue
        A.require(not stage, 'Interrupted registry authoring: inspect ' + name + ' before retrying')
        A.require(unreal.EditorAssetLibrary.get_metadata_tag(bp, 'AZ.Compass.AuthoringOwner') == 'compass_integration_setup:v1', 'Unowned container')
        A.require(unreal.EditorAssetLibrary.get_metadata_tag(child, 'AZ.Compass.SafeRemoval') == 'v1', 'Immediate marker retirement prerequisite missing')
        A.require('bRetiring' in set(BL.list_member_variable_names(child, False)), 'Retiring field missing')
        prepared.append((name, bp, child.generated_class().get_path_name(), event))
    for name, bp, marker_class, event in prepared:
        unreal.EditorAssetLibrary.set_metadata_tag(bp, KEY, 'wiring')
        replace_remove(bp, marker_class, event)
        replace_in_use(bp, marker_class)
        unreal.EditorAssetLibrary.set_metadata_tag(bp, KEY, 'v1')
        report[name] = 'map-authoritative live lookup; exact-key removal including invalid targets; visual fade retained'
    report['CompassRemoveAll'] = wire_remove_all()
    A.write('registry-map-wired', report)
    return report
