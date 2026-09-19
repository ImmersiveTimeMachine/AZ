# @Description: Retain the owned compass module across HUD reconstruction.
"""Code-only authoring recipe; importing performs no asset changes.

Run prepare() while Idle, then dedicated native bridge compile for bAttached.
Run wire(); compile BOTH bridge/module with dedicated native Blueprint tools
AFTER Python returns; then seed_defaults() and save explicitly.
Never runs PIE/tests, compiles, saves, or touches animation/source-pack assets.

Narrow changes:
- DetachModule keeps ModuleRef, retaining the same tree/maps for controller life.
- InitializeNavigation's existing true branch reattaches without resetting leafs.
- Module Destruct asks its bridge to DetachModule(self); the bridge checks identity.
- Bridge ReceiveEndPlay releases retained references/maps without any owner guard.
- Attach always raises bAttached, even for the same retained module; Detach lowers
  it and MarkReady requires it. A late Init cannot re-ready a detached bridge.
- Remove/RemoveAll still cancel queued state, then also reach a retained valid,
  locally owned module irrespective of bReady. The module's own navigation-ready
  guard prevents calls into uninitialized leafs. Adds remain queued when detached.
- Initial visibility is seeded on the CDO only, never reset at BeginPlay/reattach.

Parent host must reuse Bridge.ModuleRef, attach it to NavigationHost, then call
InitializeNavigation with this module's existing children and local bridge. Do
not create a second module/tree or overwrite its maps. One bridge per controller.

Parent also owns vendor-marker logical removal/late-fade identity fixes. This
script changes routing, not that leaf behavior. Removal must detach the registry
entry immediately so remove/re-add during a fade cannot erase the new marker.
"""
import importlib.util
import json
from pathlib import Path
import unreal

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location('az_compass_module_retention_support', HERE / 'compass_module_setup.py')
M = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(M)
B, BL, GE = M.B, M.BL, M.GE
PL = unreal.BlueprintGraphPinLibrary
KEY = 'AZ.Compass.RetentionStage'
VERSION = 'compass_retention_setup:v1'
OWNER_KEY = 'AZ.Compass.RetentionAuthor'
DEFAULT_KEY = 'AZ.Compass.RetentionDefaults'


def receipt(stage, **details):
    data = dict(stage=stage, bridge=B.TARGET, module=M.MODULE, **details)
    B.OUT.mkdir(parents=True, exist_ok=True)
    (B.OUT / ('retention-' + stage + '.json')).write_text(json.dumps(data, indent=2), encoding='utf-8')
    return data


class PatchGraph(M.ModuleGraph):
    """Use the verified existing native graph API without requiring an empty body."""
    def __init__(self, bp, graph_name, paths, entry=None):
        self.editor = GE.get_graph_editor_by_name(bp, graph_name)
        B.require(self.editor is not None, 'Missing graph: ' + graph_name)
        self.entry = entry if entry is not None else self.editor.find_graph_entry_pin()
        B.require(self.entry.is_valid(), 'Missing graph entry: ' + graph_name)
        self.entry_node = self.entry.get_owning_node()
        self.paths = paths


def input_is(node, name):
    return node.find_input_pin(name).is_valid()


def output_is(node, name):
    return node.find_output_pin(name).is_valid()


def linked(pin):
    return list(PL.list_connected_pins(pin))


def one(items, reason):
    B.require(len(items) == 1, reason + ': count=' + str(len(items)))
    return items[0]


def owned_assets():
    bridge, module = B.owned(), M.get_module()
    B.require(unreal.EditorAssetLibrary.get_metadata_tag(bridge, B.STAGE_KEY) == 'wired',
              'Finish original bridge wiring first.')
    B.require(unreal.EditorAssetLibrary.get_metadata_tag(module, M.AUTHOR_KEY) == M.AUTHOR
              and unreal.EditorAssetLibrary.get_metadata_tag(module, M.STAGE_KEY) == 'wired',
              'Finish owned module wiring first.')
    return bridge, module


def prepare():
    """Add the one attachment-state field; native compile bridge afterward."""
    B.idle()
    bridge, module = owned_assets()
    stages = [unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY) for bp in (bridge, module)]
    if stages in (['prepared', 'prepared'], ['wired', 'wired']):
        B.require(all(unreal.EditorAssetLibrary.get_metadata_tag(bp, OWNER_KEY) == VERSION
                      for bp in (bridge, module)), 'Retention owner mismatch.')
        return receipt('prepare-noop', current_stage=stages[0])
    B.require(not any(stages), 'Partial retention preparation; inspect before retrying: ' + str(stages))
    B.require('bAttached' not in set(BL.list_member_variable_names(bridge, False)),
              'Unowned bAttached field already exists; inspect rather than overwrite.')
    for bp in (bridge, module):
        unreal.EditorAssetLibrary.set_metadata_tag(bp, OWNER_KEY, VERSION)
        unreal.EditorAssetLibrary.set_metadata_tag(bp, KEY, 'preparing')
    B.require(BL.add_member_variable(bridge, 'bAttached', BL.get_basic_type_by_name('bool')),
              'Could not add attachment state.')
    BL.set_blueprint_variable_category(bridge, 'bAttached', 'Navigation|Bridge')
    for bp in (bridge, module):
        unreal.EditorAssetLibrary.set_metadata_tag(bp, KEY, 'prepared')
    return receipt('prepared', next_step='Dedicated native bridge compile, then wire().')


def comparison_branch(editor, field):
    """Find the one branch whose comparison reads this member directly."""
    found = []
    for node in editor.list_all_nodes():
        if node.get_class().get_name() != 'K2Node_IfThenElse':
            continue
        values = linked(B.inp(node, 'Condition'))
        if len(values) != 1:
            continue
        comparison = values[0].get_owning_node()
        if any(str(source.get_pin_name()) == field
               for pin in BL.list_input_pins(comparison) for source in linked(pin)):
            found.append(node)
    return one(found, 'Expected one comparison branch reading ' + field)


def preflight(bridge, module):
    # Reflected Blueprint property must exist on the compiled class, not only in
    # NewVariables. Do not let an uncompiled signature stage partially wire.
    try:
        unreal.get_default_object(bridge.generated_class()).get_editor_property('bAttached')
    except Exception as error:
        raise RuntimeError('Compile prepared bridge before wire(): bAttached unavailable.') from error
    paths = {}
    for owner, names in {
        'ActorComponent': ['GetOwner'], 'Controller': ['IsLocalController'],
        'Widget': ['GetOwningPlayer'], 'KismetSystemLibrary': ['IsValid'],
        'KismetMathLibrary': ['EqualEqual_ObjectObject'],
        'BlueprintMapLibrary': ['Map_Clear'],
    }.items():
        package = 'UMG' if owner == 'Widget' else 'Engine'
        for name in names:
            paths[(owner, name)] = B.function_path('/Script/' + package + '.' + owner, name)
    for name in ('AttachModule', 'MarkReady', 'DetachModule'):
        paths[('Bridge', name)] = B.function_path(bridge.generated_class().get_path_name(), name)
    paths[('Module', 'TryNavigationReady')] = B.function_path(module.generated_class().get_path_name(),
                                                            'TryNavigationReady')
    detach_editor = GE.get_graph_editor_by_name(bridge, 'DetachModule')
    B.require(detach_editor is not None, 'DetachModule graph missing.')
    clear = one([n for n in detach_editor.list_all_nodes()
                 if n.get_class().get_name() == 'K2Node_VariableSet' and input_is(n, 'ModuleRef')],
                'Expected the one original ModuleRef clear in DetachModule')
    B.require(not linked(B.inp(clear, 'ModuleRef')) and
              PL.get_pin_value(B.inp(clear, 'ModuleRef')).lower() in ('', 'none'),
              'ModuleRef setter is not the expected empty clear.')
    previous = one(linked(B.inp(clear, 'execute')), 'ModuleRef clear must have one predecessor')
    previous_node = previous.get_owning_node()
    B.require(previous_node.get_class().get_name() == 'K2Node_VariableSet'
              and input_is(previous_node, 'bReady')
              and not linked(B.inp(previous_node, 'bReady'))
              and PL.get_pin_value(B.inp(previous_node, 'bReady')).lower() in ('false', '0'),
              'Expected bReady=false immediately before ModuleRef clear.')
    B.require(not linked(B.out(clear, 'Output_Get')), 'Cleared ModuleRef value is consumed elsewhere.')
    init_editor = GE.get_graph_editor_by_name(module, 'InitializeNavigation')
    B.require(init_editor is not None, 'InitializeNavigation graph missing.')
    branches = []
    for node in init_editor.list_all_nodes():
        if node.get_class().get_name() == 'K2Node_IfThenElse':
            condition_links = linked(B.inp(node, 'Condition'))
            if len(condition_links) == 1 and str(condition_links[0].get_pin_name()) == 'bNavigationInitialized':
                branches.append(node)
    initialized_branch = one(branches, 'Expected one initialized-state branch')
    B.require(not linked(B.out(initialized_branch, 'then')), 'Initialized branch already authored; inspect it.')
    B.require(len(linked(B.out(initialized_branch, 'else'))) == 1, 'Fresh initialization path changed.')
    for bp, event_name in ((bridge, 'ReceiveEndPlay'), (module, 'Destruct')):
        event_graph = BL.find_event_graph(bp)
        B.require(event_graph is not None, 'Missing EventGraph on ' + bp.get_name())
        editor = GE.get_graph_editor(event_graph)
        event = editor.find_event_node(event_name)
        if event is not None:
            B.require(len(linked(B.out(event, 'then'))) <= 1, 'Unexpected lifecycle execution fanout.')
    attach_editor = GE.get_graph_editor_by_name(bridge, 'AttachModule')
    B.require(attach_editor is not None, 'AttachModule graph missing.')
    same_branch = comparison_branch(attach_editor, 'ModuleRef')
    one(linked(B.inp(same_branch, 'execute')), 'Attach same-instance branch must have one predecessor')
    ready_editor = GE.get_graph_editor_by_name(bridge, 'MarkReady')
    B.require(ready_editor is not None, 'MarkReady graph missing.')
    one(linked(ready_editor.find_graph_entry_pin()), 'MarkReady entry must have one existing successor')
    removals = []
    for name in ('RemoveCompassMarker', 'RemoveAllCompassMarkers',
                 'RemoveWorldMarker', 'RemoveAllWorldMarkers'):
        editor = GE.get_graph_editor_by_name(bridge, name)
        B.require(editor is not None, 'Removal graph missing: ' + name)
        branch = comparison_branch(editor, 'bReady')
        boolean_and = one(linked(B.inp(branch, 'Condition')), 'Expected one ready condition').get_owning_node()
        module_valid = one(linked(B.inp(boolean_and, 'B')), 'Expected module-valid input')
        valid_node = module_valid.get_owning_node()
        module_ref = one(linked(B.inp(valid_node, 'Object')), 'Expected IsValid(ModuleRef)')
        B.require(str(module_ref.get_pin_name()) == 'ModuleRef', 'Removal validity source changed.')
        removals.append((name, branch, module_valid))
    return paths, detach_editor, clear, initialized_branch, same_branch, removals


def retain_on_detach(bridge, paths, editor, clear):
    previous = one(linked(B.inp(clear, 'execute')), 'Detach predecessor changed')
    following = linked(B.out(clear, 'then'))
    B.require(len(following) <= 1, 'Unexpected ModuleRef-clear execution fanout.')
    PL.break_pin_links(B.inp(clear, 'execute'))
    PL.break_pin_links(B.out(clear, 'then'))
    graph = PatchGraph(bridge, 'DetachModule', paths)
    detached = graph.set('bAttached', previous, default='false')
    for pin in following:
        B.connect(detached, pin)
    editor.remove_nodes([clear])  # only this one proven empty-reference setter


def attach_and_ready_guards(bridge, paths, same_branch):
    # This point is AFTER module validity/owner checks, but BEFORE the same-module
    # no-op. Reattaching the same retained instance must set the flag again.
    graph = PatchGraph(bridge, 'AttachModule', paths)
    previous = one(linked(B.inp(same_branch, 'execute')), 'Attach predecessor changed')
    PL.break_pin_links(B.inp(same_branch, 'execute'))
    attached = graph.set('bAttached', previous, default='true')
    B.connect(attached, B.inp(same_branch, 'execute'))
    graph = PatchGraph(bridge, 'MarkReady', paths)
    following = one(linked(graph.entry), 'MarkReady entry changed')
    PL.break_pin_links(graph.entry)
    accepted, _ = graph.branch(graph.entry, graph.get('bAttached'))
    B.connect(accepted, following)


def route_detached_removals(removals):
    for _, branch, module_valid in removals:
        # Only replace bReady && IsValid(ModuleRef) with IsValid(ModuleRef).
        # Existing queued-map cancellation, local-controller guard, module-owner
        # guard and module's bNavigationReady guard all remain in place.
        PL.break_pin_links(B.inp(branch, 'Condition'))
        B.connect(module_valid, B.inp(branch, 'Condition'))


def reattach_initialized(module, paths, branch):
    graph = PatchGraph(module, 'InitializeNavigation', paths)
    execute = B.out(branch, 'then')
    pc = B.out(graph.native('Widget', 'GetOwningPlayer'))
    execute = graph.branch(execute, graph.valid(pc))[0]
    local = B.out(graph.native('Controller', 'IsLocalController', self=pc))
    execute = graph.branch(execute, local)[0]
    bridge = graph.param('Bridge')
    execute = graph.bridge_guard(execute, bridge, pc)
    # Refuse cross-owner retained child trees; do not reset configuration/maps.
    for name in ('CompassWidgetRef', 'WorldWidgetRef'):
        execute = graph.module_owner_guard(execute, graph.get(name), pc)
    execute = graph.set('BridgeRef', execute, value=bridge)
    execute = graph.exec_call(paths[('Bridge', 'AttachModule')], execute,
                              self=bridge, Module=graph.self_value())
    ready, waiting = graph.branch(execute, graph.get('bNavigationReady'))
    graph.exec_call(paths[('Bridge', 'MarkReady')], ready, self=bridge, Module=graph.self_value())
    # If both children became ready between removal and reattachment, let the
    # existing identity-protected readiness function finish its normal work.
    graph.exec_call(paths[('Module', 'TryNavigationReady')], waiting)


def event_shell(bp, name, paths):
    event = BL.add_event_override(bp, name, unreal.IntPoint(-800, 1200))
    B.require(event is not None, 'Cannot author inherited lifecycle event: ' + name)
    entry = B.out(event, 'then')
    following = linked(entry)
    B.require(len(following) <= 1, 'Lifecycle execution fanout changed.')
    PL.break_pin_links(entry)
    graph = PatchGraph(bp, event.get_outer().get_name(), paths, entry)
    return graph, following


def module_destruct(module, paths):
    graph, following = event_shell(module, 'Destruct', paths)
    bridge = graph.get('BridgeRef')
    yes, no = graph.branch(graph.entry, graph.valid(bridge))
    finished = graph.exec_call(paths[('Bridge', 'DetachModule')], yes,
                               self=bridge, Module=graph.self_value())
    for pin in following:
        B.connect(finished, pin)
        B.connect(no, pin)


def bridge_end_play(bridge, paths):
    graph, following = event_shell(bridge, 'ReceiveEndPlay', paths)
    # No GetOwner/IsLocalController/IsValid gate: teardown must work after owner
    # validity changes. These are only the component's own transient references.
    execute = graph.set('bReady', graph.entry, default='false')
    execute = graph.set('bAttached', execute, default='false')
    for channel in ('Compass', 'World'):
        execute = graph.mutate_map(channel, 'Clear', execute)
    execute = graph.set('ModuleRef', execute, default='None')
    execute = graph.set('bPendingVisibility', execute, default='false')
    for pin in following:
        B.connect(execute, pin)


def wire():
    """Patch only the four inspected lifecycle edges; never compile/save here."""
    B.idle()
    bridge, module = owned_assets()
    stages = [unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY) for bp in (bridge, module)]
    if stages == ['wired', 'wired']:
        B.require(all(unreal.EditorAssetLibrary.get_metadata_tag(bp, OWNER_KEY) == VERSION
                      for bp in (bridge, module)), 'Retention stage owner mismatch.')
        return receipt('wire-noop')
    B.require(stages == ['prepared', 'prepared'],
              'Run prepare()+native bridge compile first; partial stages require inspection: ' + str(stages))
    B.require(all(unreal.EditorAssetLibrary.get_metadata_tag(bp, OWNER_KEY) == VERSION
                  for bp in (bridge, module)), 'Retention stage owner mismatch.')
    paths, detach_editor, clear, branch, attach_branch, removals = preflight(bridge, module)
    for bp in (bridge, module):
        unreal.EditorAssetLibrary.set_metadata_tag(bp, OWNER_KEY, VERSION)
        unreal.EditorAssetLibrary.set_metadata_tag(bp, KEY, 'wiring')
    retain_on_detach(bridge, paths, detach_editor, clear)
    attach_and_ready_guards(bridge, paths, attach_branch)
    route_detached_removals(removals)
    reattach_initialized(module, paths, branch)
    module_destruct(module, paths)
    bridge_end_play(bridge, paths)
    for bp in (bridge, module):
        unreal.EditorAssetLibrary.set_metadata_tag(bp, KEY, 'wired')
    return receipt('wired', next_step='Native compile bridge/module, then seed_defaults(), then explicit save.',
                   runtime_verified=False, detached_removal_handling='immediate retained-module forwarding',
                   removal_graphs=[name for name, _, _ in removals])


def seed_defaults():
    """Run AFTER the dedicated native compile; changes only new-instance defaults."""
    B.idle()
    bridge, module = owned_assets()
    B.require(all(unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY) == 'wired'
                  and unreal.EditorAssetLibrary.get_metadata_tag(bp, OWNER_KEY) == VERSION
                  for bp in (bridge, module)), 'Complete retention wiring/compilation first.')
    if unreal.EditorAssetLibrary.get_metadata_tag(bridge, DEFAULT_KEY) == VERSION:
        return receipt('defaults-noop')
    cdo = unreal.get_default_object(bridge.generated_class())
    cdo.set_editor_property('bCompassVisible', True)
    cdo.set_editor_property('bPendingVisibility', True)
    cdo.set_editor_property('bAttached', False)
    B.require(cdo.get_editor_property('bCompassVisible') and cdo.get_editor_property('bPendingVisibility'),
              'Visibility default readback failed.')
    unreal.EditorAssetLibrary.set_metadata_tag(bridge, DEFAULT_KEY, VERSION)
    return receipt('defaults-seeded', initial_compass_visible=True, initial_visibility_pending=True,
                   runtime_instances_modified=False,
                   next_step='Explicitly save bridge/module; user performs lifecycle gameplay checks.')
