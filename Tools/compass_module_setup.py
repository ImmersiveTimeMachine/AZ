# @Description: Author owner-scoped forwarding and readiness on the isolated compass module.
"""Authoring only. Importing performs no asset mutations; execute only while Idle.

Order:
1. Existing module/config + leaf NavigationContext fields must be compiled.
2. compass_bridge_setup.prepare(); dedicated native bridge compile.
3. This prepare(); dedicated native module compile.
4. This wire(); dedicated native module compile/save. Wire/compile bridge too.
5. Parent integrates the tree/HUD and the compass completion hook below.

Only WBP_AZ_CompassModule graphs/fields are authored. No source pack, leaf graph,
HUD, animation, C++ or map changes. Never compiles/saves assets or runs PIE/tests.

Host calls InitializeNavigation(CompassView, WorldMarkersView, LocalBridge) once,
with typed widget instances from this module's tree and its own controller bridge.
Compass completion hook: NavigationContext.NotifyCompassReady(self), after the
copied Init chain COMPLETES. Parent must gate copied compass Init on valid context
and a per-widget one-shot initialized flag; its inherited Delay(0) can otherwise
run Init twice. InitializeNavigation explicitly invokes Init once context is set,
so it also handles the case where automatic deferred Init happened too early.
No fixed wait substitutes for readiness. Source World.JustConstruct only assigns
WorldMarkerValues synchronously; its return is sufficient for world-config ready.

Host/module Destruct calls BridgeRef.DetachModule(self). Reinitialization of this
same module instance is intentionally a no-op; recreate its leafs for a new module
lifetime. Active registry persistence/republication remains the host's contract.

Module CONTRACT is the bridge's exact contract (Marker_Object, MarkerInfo).
Leaf names differ: Compass Add takes MarkerObject, MarkerInfo, PingIcon1/2;
Compass remove-all is RemoveAllMarkers; World remove-all is RemoveAllWorldMarkers.
Module commands are for the ready bridge; early external callers use that bridge's
pending maps. This file does not create a second marker registry or projection.
"""
import importlib.util
import json
import re
from pathlib import Path
import unreal

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location('az_compass_bridge_authoring', HERE / 'compass_bridge_setup.py')
B = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(B)  # definitions only; no prepare()/wire() call
BL = unreal.BlueprintEditorLibrary
GE = unreal.BlueprintGraphEditor
MODULE = B.MODULE
BRIDGE = B.TARGET
COMPASS = B.DEST + '/WBP_AZ_Compass'
WORLD = B.DEST + '/WBP_AZ_WorldMarkerContainer'
WORLD_VALUES = '/Game/ProHUDV2_Horror/Blueprints/Structs/S_WorldMarkerValues_H'
OUT = B.OUT
AUTHOR_KEY = 'AZ.Compass.ModuleAuthoring'
AUTHOR = 'compass_module_setup:v1'
STAGE_KEY = 'AZ.Compass.ModuleStage'
LIFECYCLE = {
    'InitializeNavigation': [('Compass', 'CompassWidget'), ('World', 'WorldWidget'), ('Bridge', 'Bridge')],
    'NotifyCompassReady': [('Compass', 'CompassWidget')],
    'TryNavigationReady': [],
}
FIELDS = {
    'CompassWidgetRef': 'CompassWidget', 'WorldWidgetRef': 'WorldWidget', 'BridgeRef': 'Bridge',
    'bNavigationInitialized': 'bool', 'bCompassReady': 'bool',
    'bWorldReady': 'bool', 'bNavigationReady': 'bool',
}
# (leaf class, function name, module-param -> leaf-param mapping)
FORWARD = {
    'AddOrUpdateCompassMarker': ('CompassWidget', 'AddOrUpdateCompassMarker',
                                 {'Marker_Object': 'MarkerObject', 'MarkerInfo': 'MarkerInfo'}),
    'RemoveCompassMarker': ('CompassWidget', 'RemoveCompassMarker', {'Marker_Object': 'MarkerObject'}),
    'RemoveAllCompassMarkers': ('CompassWidget', 'RemoveAllMarkers', {}),
    'AddOrUpdateWorldMarker': ('WorldWidget', 'AddOrUpdateWorldMarker',
                               {'Marker_Object': 'MarkerObject', 'MarkerInfo': 'MarkerInfo'}),
    'RemoveWorldMarker': ('WorldWidget', 'RemoveWorldMarker', {'Marker_Object': 'MarkerObject'}),
    'RemoveAllWorldMarkers': ('WorldWidget', 'RemoveAllWorldMarkers', {}),
    'ShowCompass': ('CompassWidget', 'ShowCompass', {}),
    'HideCompass': ('CompassWidget', 'HideCompass', {}),
}


def receipt(stage, **details):
    result = dict(stage=stage, module=MODULE, lifecycle=LIFECYCLE, bridge_contract=B.CONTRACT,
                  leaf_forwarding=FORWARD, **details)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / ('module-runtime-' + stage + '.json')).write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


def load(path):
    asset = unreal.load_asset(path)
    B.require(asset is not None, 'Missing asset: ' + path)
    return asset


def get_module():
    bp = load(MODULE)
    B.require(unreal.EditorAssetLibrary.get_metadata_tag(bp, 'AZ.Compass.AuthoringOwner')
              == 'compass_integration_setup:v1', 'Refusing unowned module asset.')
    return bp


def classes():
    result = {}
    for name, path in [('Module', MODULE), ('Bridge', BRIDGE),
                       ('CompassWidget', COMPASS), ('WorldWidget', WORLD)]:
        result[name] = load(path).generated_class()
        B.require(result[name] is not None, 'Compile asset first: ' + path)
    return result


def world_mapping():
    """Read exact GUID-bearing struct pins from the captured SOURCE graph."""
    lines = json.loads((OUT / 'config-extraction.json').read_text(encoding='utf-8'))[
        'module']['graphs']['ConstructWorldMarker']
    active, result = False, {}
    for line in lines:
        if re.match(r'^\[[0-9A-F]+\]', line):
            active = 'MakeStruct | Make S World Marker Values H' in line
        if active:
            match = re.match(r'^\s+in\s+(\S+):\w+.*? <-[0-9A-F]+\.([A-Za-z0-9_]+)$', line)
            if match:
                result[match.group(1)] = match.group(2)
    B.require(len(result) == 17, 'Expected exactly 17 source world-value mappings; inspect receipt.')
    return result


def prepare():
    """Add only module variables/signatures. Compile via native tool afterward."""
    B.idle()
    bp = get_module()
    old_stage = unreal.EditorAssetLibrary.get_metadata_tag(bp, STAGE_KEY)
    if old_stage:
        B.require(unreal.EditorAssetLibrary.get_metadata_tag(bp, AUTHOR_KEY) == AUTHOR,
                  'Another author owns module runtime stage.')
        B.require(old_stage in ('prepared', 'wired'), 'Partial module stage: inspect before recovery.')
        return receipt('prepare-noop', current_stage=old_stage)
    cl = classes()  # bridge.prepare()+compile must happen before this stage
    mapping = world_mapping()
    current_fields = set(BL.list_member_variable_names(bp, False))
    B.require(not (set(FIELDS) & current_fields), 'Runtime module fields already exist; inspect before overwrite.')
    B.require(set(mapping.values()) <= current_fields, 'Module is missing source world configuration fields.')
    B.require({'MarkerPingIcon1', 'MarkerPingIcon2'} <= current_fields, 'Compass ping settings missing.')
    functions = {**B.CONTRACT, **LIFECYCLE}
    existing_graphs = {str(name) for name in BL.list_graph_names(bp)}
    B.require(not (set(functions) & existing_graphs), 'Module function collision: do not clobber existing graphs.')
    pin_types = B.types()
    for name, cls in cl.items():
        pin_types[name] = BL.get_object_reference_type(cls)
    unreal.EditorAssetLibrary.set_metadata_tag(bp, AUTHOR_KEY, AUTHOR)
    unreal.EditorAssetLibrary.set_metadata_tag(bp, STAGE_KEY, 'preparing')
    for name, kind in FIELDS.items():
        B.require(BL.add_member_variable(bp, name, pin_types[kind]), 'Could not add field: ' + name)
        BL.set_blueprint_variable_category(bp, name, 'Navigation|Runtime')
    for name, params in functions.items():
        editor = GE.create_and_edit_function_graph(bp, name)
        B.require(editor is not None, 'Could not create function: ' + name)
        editor.set_function_is_public()
        for param, kind in params:
            B.require(editor.add_graph_input_parameter(param, pin_types[kind]).is_valid(),
                      'Could not create parameter: ' + name + '.' + param)
    unreal.EditorAssetLibrary.set_metadata_tag(bp, STAGE_KEY, 'prepared')
    return receipt('prepared', world_mapping=mapping, next_step='Dedicated native module compile, then wire().')


class ModuleGraph(B.Graph):
    def local_guard(self):
        pc = B.out(self.native('Widget', 'GetOwningPlayer'))
        execute = self.branch(self.entry, self.valid(pc))[0]
        local = B.out(self.native('Controller', 'IsLocalController', self=pc))
        return self.branch(execute, local)[0], pc

    def self_value(self):
        if not hasattr(self, '_self_value'):
            # Exact menu source: K2Node_Self GetMenuCategory / MenuTitle.
            node = self.editor.create_node_from_name('Variables|Getareferencetoself',
                                                    unreal.Vector2D(0, 400), [], None)
            B.require(node is not None and node.get_class().get_name() == 'K2Node_Self',
                      'Native self-reference action unavailable.')
            self._self_value = B.out(node, 'self')
        return self._self_value

    def set_leaf_context(self, execute, kind, leaf):
        node = self.editor.add_set_member_variable_node('NavigationContext', self.paths['classes'][kind])
        B.require(node is not None, 'Compile ' + kind + '.NavigationContext first.')
        B.connect(leaf, B.inp(node, 'self'))
        B.connect(self.self_value(), B.inp(node, 'NavigationContext'))
        B.connect(execute, B.inp(node, 'execute'))
        return B.out(node, 'then')

    def bridge_guard(self, execute, bridge, pc):
        execute = self.branch(execute, self.valid(bridge))[0]
        owner = B.out(self.native('ActorComponent', 'GetOwner', self=bridge))
        same = B.out(self.native('KismetMathLibrary', 'EqualEqual_ObjectObject', A=owner, B=pc))
        return self.branch(execute, same)[0]

    def exec_call(this, path, execute, **values):
        node = this.call(path, **values)
        B.connect(execute, B.inp(node, 'execute'))
        return B.out(node, 'then')


def preflight(bp):
    cl = classes()
    mapping = world_mapping()
    paths = {'classes': {name: cls.get_path_name() for name, cls in cl.items()}, 'world_mapping': mapping}
    for owner, names in {
        'Widget': ['GetOwningPlayer'], 'UserWidget': ['SetOwningPlayer'],
        'Controller': ['IsLocalController'], 'ActorComponent': ['GetOwner'],
        'KismetSystemLibrary': ['IsValid'],
        'KismetMathLibrary': ['BooleanAND', 'BooleanOR', 'EqualEqual_ObjectObject'],
        'GameplayStatics': ['ObjectIsA'],
    }.items():
        package = 'UMG' if owner in ('Widget', 'UserWidget') else 'Engine'
        for name in names:
            paths[(owner, name)] = B.function_path('/Script/' + package + '.' + owner, name)
    for name in {**B.CONTRACT, **LIFECYCLE}:
        paths[('Module', name)] = B.function_path(cl['Module'].get_path_name(), name)
    for name in ('AttachModule', 'MarkReady', 'DetachModule'):
        paths[('Bridge', name)] = B.function_path(cl['Bridge'].get_path_name(), name)
    for kind, name, _ in FORWARD.values():
        paths[(kind, name)] = B.function_path(cl[kind].get_path_name(), name)
    paths[('WorldWidget', 'JustConstruct')] = B.function_path(cl['WorldWidget'].get_path_name(), 'JustConstruct')
    # Init is the inherited/overridden vendor event. Find it on the copied class;
    # falling back to the vendor base is safe only for resolving its signature.
    # Runtime target remains the owned compass instance and dispatch is virtual.
    try:
        paths[('CompassWidget', 'Init')] = B.function_path(cl['CompassWidget'].get_path_name(), 'Init')
    except RuntimeError:
        base = load('/Game/ProHUDV2_Horror/Widgets/Base/WB_Construct_H').generated_class()
        paths[('CompassWidget', 'Init')] = B.function_path(base.get_path_name(), 'Init')
    for kind, path in [('CompassWidget', COMPASS), ('WorldWidget', WORLD)]:
        B.require('NavigationContext' in set(BL.list_member_variable_names(load(path), False)),
                  kind + ' context field missing.')
    load(WORLD_VALUES)  # ensure its MakeStruct action is registered
    editor = GE.get_graph_editor_by_name(bp, 'InitializeNavigation')
    actions = {str(s) for s in editor.list_available_nodes([])}
    normalized = lambda s: re.sub(r'[^a-z0-9]', '', s.lower())
    candidates = sorted(s for s in actions if normalized(s.rsplit('|', 1)[-1]) == 'makesworldmarkervaluesh')
    B.require(len(candidates) == 1, 'World MakeStruct action is ambiguous/unavailable: ' + str(candidates))
    paths['make_world_values'] = candidates[0]
    B.require('Variables|Getareferencetoself' in actions, 'Self-reference action unavailable.')
    return paths


def author_forward(bp, paths, name):
    graph = ModuleGraph(bp, name, paths)
    execute, pc = graph.local_guard()
    execute = graph.branch(execute, graph.get('bNavigationReady'))[0]
    kind, function, params = FORWARD[name]
    leaf = graph.get('CompassWidgetRef' if kind == 'CompassWidget' else 'WorldWidgetRef')
    execute = graph.module_owner_guard(execute, leaf, pc)
    if name.startswith('AddOrUpdate'):
        execute = graph.target_guard(execute, graph.param('Marker_Object'))
    arguments = {leaf_param: graph.param(module_param) for module_param, leaf_param in params.items()}
    if name == 'AddOrUpdateCompassMarker':
        arguments.update(PingIcon1=graph.get('MarkerPingIcon1'), PingIcon2=graph.get('MarkerPingIcon2'))
    graph.exec_call(paths[(kind, function)], execute, self=leaf, **arguments)


def author_initialize(bp, paths):
    graph = ModuleGraph(bp, 'InitializeNavigation', paths)
    execute, pc = graph.local_guard()
    _, execute = graph.branch(execute, graph.get('bNavigationInitialized'))
    compass, world, bridge = (graph.param(n) for n in ('Compass', 'World', 'Bridge'))
    execute = graph.branch(execute, graph.valid(compass))[0]
    execute = graph.branch(execute, graph.valid(world))[0]
    execute = graph.bridge_guard(execute, bridge, pc)
    # Typed references are supplied by the owning module's tree, never searched.
    for field, value in [('CompassWidgetRef', compass), ('WorldWidgetRef', world), ('BridgeRef', bridge)]:
        execute = graph.set(field, execute, value=value)
    for field in ('bCompassReady', 'bWorldReady', 'bNavigationReady'):
        execute = graph.set(field, execute, default='false')
    execute = graph.set('bNavigationInitialized', execute, default='true')
    for kind, leaf in [('CompassWidget', compass), ('WorldWidget', world)]:
        execute = graph.exec_call(paths[('UserWidget', 'SetOwningPlayer')], execute,
                                  self=leaf, LocalPlayerController=pc)
        execute = graph.set_leaf_context(execute, kind, leaf)
    execute = graph.exec_call(paths[('Bridge', 'AttachModule')], execute,
                              self=bridge, Module=graph.self_value())
    make = graph.editor.create_node_from_name(paths['make_world_values'], unreal.Vector2D(1500, 450), [], None)
    B.require(make is not None and make.get_class().get_name() == 'K2Node_MakeStruct',
              'Unexpected MakeStruct action result.')
    for struct_pin, module_field in paths['world_mapping'].items():
        B.connect(graph.get(module_field), B.inp(make, struct_pin))
    execute = graph.exec_call(paths[('WorldWidget', 'JustConstruct')], execute,
                              self=world, Values=B.out(make, 'S_WorldMarkerValues_H'))
    execute = graph.set('bWorldReady', execute, default='true')
    execute = graph.exec_call(paths[('CompassWidget', 'Init')], execute, self=compass)
    # May not yet be ready if the leaf completes asynchronously. Its explicit
    # completion callback also invokes TryNavigationReady; no timer is created.
    graph.exec_call(paths[('Module', 'TryNavigationReady')], execute)


def author_notify(bp, paths):
    graph = ModuleGraph(bp, 'NotifyCompassReady', paths)
    execute, pc = graph.local_guard()
    execute = graph.branch(execute, graph.get('bNavigationInitialized'))[0]
    compass = graph.param('Compass')
    same = B.out(graph.native('KismetMathLibrary', 'EqualEqual_ObjectObject',
                              A=compass, B=graph.get('CompassWidgetRef')))
    execute = graph.branch(execute, same)[0]
    execute = graph.module_owner_guard(execute, compass, pc)
    execute = graph.set('bCompassReady', execute, default='true')
    graph.exec_call(paths[('Module', 'TryNavigationReady')], execute)


def author_try_ready(bp, paths):
    graph = ModuleGraph(bp, 'TryNavigationReady', paths)
    execute, pc = graph.local_guard()
    execute = graph.branch(execute, graph.get('bNavigationInitialized'))[0]
    _, execute = graph.branch(execute, graph.get('bNavigationReady'))
    both = graph.bool_op('BooleanAND', graph.get('bCompassReady'), graph.get('bWorldReady'))
    execute = graph.branch(execute, both)[0]
    for field in ('CompassWidgetRef', 'WorldWidgetRef'):
        execute = graph.module_owner_guard(execute, graph.get(field), pc)
    bridge = graph.get('BridgeRef')
    execute = graph.bridge_guard(execute, bridge, pc)
    attached = graph.editor.add_get_member_variable_node('ModuleRef', paths['classes']['Bridge'])
    B.require(attached is not None, 'Bridge ModuleRef field is not compiled.')
    B.connect(bridge, B.inp(attached, 'self'))
    same = B.out(graph.native('KismetMathLibrary', 'EqualEqual_ObjectObject',
                              A=B.out(attached, 'ModuleRef'), B=graph.self_value()))
    execute = graph.branch(execute, same)[0]
    # Set BEFORE bridge replay; replay calls module forwarding synchronously.
    execute = graph.set('bNavigationReady', execute, default='true')
    graph.exec_call(paths[('Bridge', 'MarkReady')], execute, self=bridge, Module=graph.self_value())


def wire():
    """Author module bodies, returning BEFORE native compile/save."""
    B.idle()
    bp = get_module()
    B.require(unreal.EditorAssetLibrary.get_metadata_tag(bp, AUTHOR_KEY) == AUTHOR,
              'Run this module prepare() first.')
    stage = unreal.EditorAssetLibrary.get_metadata_tag(bp, STAGE_KEY)
    if stage == 'wired':
        return receipt('wire-noop', current_stage=stage)
    B.require(stage == 'prepared', 'Partial module authoring: inspect before retrying: ' + stage)
    paths = preflight(bp)
    unreal.EditorAssetLibrary.set_metadata_tag(bp, STAGE_KEY, 'wiring')
    for name in B.CONTRACT:
        author_forward(bp, paths, name)
    author_initialize(bp, paths)
    author_notify(bp, paths)
    author_try_ready(bp, paths)
    unreal.EditorAssetLibrary.set_metadata_tag(bp, STAGE_KEY, 'wired')
    return receipt('wired', world_mapping=paths['world_mapping'],
                   next_step='Native compile/save module; parent installs compass Init hook and HUD/tree lifecycle.',
                   compass_completion_hook_installed=False, active_hud_attached=False, runtime_verified=False)
