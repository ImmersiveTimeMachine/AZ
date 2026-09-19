# @Description: Author the isolated local-player ProHUD compass bridge Blueprint.
"""Editor authoring only; importing this file performs no asset mutations.

Stages: prepare() -> dedicated native BP compile -> wire() -> native compile/save.
Module functions in CONTRACT must also exist and be compiled before wire().
Never invokes compile, save, PIE, tests, C++ compilation or active-HUD attachment.

Only TARGET may be edited. A completed stage is idempotent; an interrupted stage
fails closed instead of overwriting partially authored graphs. Inspect that asset
before retrying. This script is intentionally not a generic graph remapper.

Integration contract:
- Add ONE nonreplicated bridge to each LOCAL PlayerController, with no Tick.
- Create module with that controller as OwningPlayer. AttachModule(Module)
  publishes the reference before the copied vendor widgets' deferred Init.
- Module calls MarkReady(self) only after BOTH containers are initialized;
  Destruct/host teardown calls DetachModule(self). Identity guards ignore old
  delayed Ready/Destruct callbacks. The bridge never searches for a player/widget.
- Module implements CONTRACT with the exact vendor structs, no output params.
  Remove paths retain vendor remove/re-add ownership fixes in the module.
- Before Ready: one latest Add/Update per object per channel, max128. Removal
  cancels queued state. At capacity, updates still work; a new key is rejected
  and bPendingOverflow is latched for inspection. Visibility has one last value.
- Ready replays once and drains pending keys; invalid/destroyed targets drop.
  Active marker registries remain vendor/module-owned. Replacing a READY module
  does not reconstruct its former active markers: the host must retain that
  module or ask the actual marker producers to republish after recreation.
- This is authoring code, syntax-checked but not executed/compiled in Unreal.
  prepare()/wire() receipts explicitly do not claim runtime verification.
"""
import json
import re
from pathlib import Path
import unreal

DEST = '/Game/AZ/Blueprints/Menu/HUD/Navigation'
TARGET = DEST + '/BPC_AZ_CompassBridge'
MODULE = DEST + '/WBP_AZ_CompassModule'
STRUCTS = {
    'Compass': '/Game/ProHUDV2_Horror/Blueprints/Structs/S_CompassMarkerInfo_H',
    'World': '/Game/ProHUDV2_Horror/Blueprints/Structs/S_WorldMarkerInfo_H',
}
OWNER_KEY = 'AZ.Compass.BridgeAuthoring'
VERSION = 'compass_bridge_setup:v1'
STAGE_KEY = 'AZ.Compass.BridgeStage'
OUT = Path('C:/UnrealEngine/Games/AZ/Saved/CompassIntegration')
MAX_PENDING = 128
CONTRACT = {
    'AddOrUpdateCompassMarker': [('Marker_Object', 'Object'), ('MarkerInfo', 'Compass')],
    'RemoveCompassMarker': [('Marker_Object', 'Object')],
    'RemoveAllCompassMarkers': [],
    'AddOrUpdateWorldMarker': [('Marker_Object', 'Object'), ('MarkerInfo', 'World')],
    'RemoveWorldMarker': [('Marker_Object', 'Object')],
    'RemoveAllWorldMarkers': [],
    'ShowCompass': [],
    'HideCompass': [],
}
LIFECYCLE = {
    'AttachModule': [('Module', 'Module')],
    'MarkReady': [('Module', 'Module')],
    'DetachModule': [('Module', 'Module')],
}
LIB = unreal.BlueprintEditorLibrary


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def idle():
    result = unreal.ToolsetRegistry.execute_tool(
        'EditorToolset.EditorAppToolset', 'IsPIERunning', '{}')
    require(result.is_complete and not result.error, 'Cannot establish editor Idle: ' + str(result.error))
    require(not json.loads(result.value)['returnValue'], 'PIE active: no bridge authoring allowed.')


def receipt(stage, **details):
    result = dict(stage=stage, target=TARGET, module=MODULE, contract=CONTRACT,
                  lifecycle=LIFECYCLE, max_pending_per_channel=MAX_PENDING, **details)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / ('bridge-' + stage + '.json')).write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


def owned():
    bp = unreal.load_asset(TARGET)
    require(bp is not None, 'Run prepare() first.')
    require(unreal.EditorAssetLibrary.get_metadata_tag(bp, OWNER_KEY) == VERSION,
            'Refusing to edit an unowned bridge: ' + TARGET)
    return bp


def types():
    module_bp = unreal.load_asset(MODULE)
    require(module_bp is not None and module_bp.generated_class() is not None,
            'Compile WBP_AZ_CompassModule first.')
    result = {
        'Object': LIB.get_object_reference_type(unreal.Object.static_class()),
        'Module': LIB.get_object_reference_type(module_bp.generated_class()),
        'bool': LIB.get_basic_type_by_name('bool'),
    }
    for channel, path in STRUCTS.items():
        struct = unreal.load_asset(path)
        require(struct is not None, 'Missing vendor struct: ' + path)
        result[channel] = LIB.get_struct_type(struct)
        # Important: the key is UObject, NOT the string key used by tool helpers.
        result[channel + 'Map'] = LIB.get_map_type(result['Object'], result[channel])
    return result


def prepare():
    """Create only typed fields/signatures; return before native compilation."""
    idle()
    pin_types = types()
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET):
        bp = owned()
        stage = unreal.EditorAssetLibrary.get_metadata_tag(bp, STAGE_KEY)
        if stage == 'preparing':
            expected = {'ModuleRef', 'bReady', 'PendingCompass', 'PendingWorld', 'bPendingVisibility',
                        'bCompassVisible', 'bPendingOverflow'}
            require(expected <= set(LIB.list_member_variable_names(bp, False)), 'Incomplete bridge fields')
            require(set(CONTRACT) | set(LIFECYCLE) <= set(str(n) for n in LIB.list_graph_names(bp)),
                    'Incomplete bridge function signatures')
            cdo = unreal.get_default_object(bp.generated_class())
            tick = cdo.get_editor_property('primary_component_tick')
            tick.set_editor_property('start_with_tick_enabled', False)
            cdo.set_editor_property('primary_component_tick', tick)
            unreal.EditorAssetLibrary.set_metadata_tag(bp, STAGE_KEY, 'prepared')
            return receipt('prepared', recovered='all signatures verified; start tick disabled',
                           next_step='Native compile bridge and module contract, then wire().')
        require(stage in ('prepared', 'wired'), 'Partial bridge stage: inspect before recovery: ' + stage)
        return receipt('prepare-noop', current_stage=stage)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.ActorComponent.static_class())
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        TARGET.rsplit('/', 1)[1], DEST, unreal.Blueprint.static_class(), factory)
    require(bp is not None, 'Could not create isolated bridge.')
    unreal.EditorAssetLibrary.set_metadata_tag(bp, OWNER_KEY, VERSION)
    unreal.EditorAssetLibrary.set_metadata_tag(bp, STAGE_KEY, 'preparing')
    for name, kind in [('ModuleRef', 'Module'), ('bReady', 'bool'),
                       ('PendingCompass', 'CompassMap'), ('PendingWorld', 'WorldMap'),
                       ('bPendingVisibility', 'bool'), ('bCompassVisible', 'bool'),
                       ('bPendingOverflow', 'bool')]:
        require(LIB.add_member_variable(bp, name, pin_types[kind]), 'Field creation failed: ' + name)
        LIB.set_blueprint_variable_category(bp, name, 'Navigation|Bridge')
    for name, params in {**CONTRACT, **LIFECYCLE}.items():
        editor = unreal.BlueprintGraphEditor.create_and_edit_function_graph(bp, name)
        require(editor is not None, 'Graph creation failed: ' + name)
        editor.set_function_is_public()
        for param_name, kind in params:
            require(editor.add_graph_input_parameter(param_name, pin_types[kind]).is_valid(),
                    'Parameter creation failed: ' + name + '.' + param_name)
        if name == 'MarkReady':
            # Cache Find's value BEFORE removing its key; pure outputs otherwise
            # re-evaluate against the modified map and return a default struct.
            for channel in STRUCTS:
                require(editor.add_local_variable('Replay' + channel + 'Info', pin_types[channel]),
                        'Replay value local could not be created.')
    # ActorComponent tick is unnecessary. This edits only the new staged CDO.
    cdo = unreal.get_default_object(bp.generated_class())
    tick = cdo.get_editor_property('primary_component_tick')
    # bCanEverTick is not a reflected editable property in this engine. No Tick
    # event is authored; retain the native component default and disable start.
    tick.set_editor_property('start_with_tick_enabled', False)
    cdo.set_editor_property('primary_component_tick', tick)
    unreal.EditorAssetLibrary.set_metadata_tag(bp, STAGE_KEY, 'prepared')
    return receipt('prepared', next_step='Native compile bridge; compile module CONTRACT; then wire().')


def function_path(owner_path, name):
    # Resolve the real UFunction path; never invent a graph action label.
    for delimiter in (':', '.'):
        obj = unreal.load_object(None, owner_path + delimiter + name)
        if obj is not None:
            return obj.get_path_name()
    raise RuntimeError('Missing compiled function: ' + owner_path + ':' + name)


def inp(node, name):
    pin = node.find_input_pin(name)
    require(pin.is_valid(), 'Missing input pin: ' + node.get_name() + '.' + name)
    return pin


def out(node, name='ReturnValue'):
    pin = node.find_output_pin(name)
    if not pin.is_valid():
        normalized = lambda s: re.sub(r'[^a-z0-9]', '', str(s).lower())
        matches = [p for p in LIB.list_output_pins(node) if normalized(p.get_pin_name()) == normalized(name)]
        if len(matches) == 1:
            pin = matches[0]
    require(pin.is_valid(), 'Missing output pin: ' + node.get_name() + '.' + name)
    return pin


def connect(source, target):
    require(source.is_valid() and target.is_valid() and source.try_create_connection(target),
            'Cannot wire ' + str(source) + ' -> ' + str(target))


def literal(pin, value):
    require(pin.set_pin_value(str(value)), 'Cannot set pin literal: ' + str(value))


class Graph:
    def __init__(self, bp, name, paths):
        self.editor = unreal.BlueprintGraphEditor.get_graph_editor_by_name(bp, name)
        require(self.editor is not None, 'Missing prepared graph: ' + name)
        self.entry = self.editor.find_graph_entry_pin()
        require(self.entry.is_valid() and not self.entry.list_connected_pins(),
                'Graph already wired; refusing overwrite: ' + name)
        self.entry_node = self.entry.get_owning_node()
        self.paths = paths

    def param(self, name):
        return out(self.entry_node, name)

    def call(this, path, **values):
        # 'self' is an actual Unreal target pin, so the Python receiver must
        # have another name to accept self=... in the graph pin mapping.
        node = this.editor.add_call_function_node(path)
        require(node is not None, 'Function-node creation failed: ' + path)
        for key, value in values.items():
            connect(value, inp(node, key))
        return node

    def native(this, owner, name, **values):
        return this.call(this.paths[(owner, name)], **values)

    def get(self, name, local=False):
        node = (self.editor.add_get_local_variable_node(name) if local
                else self.editor.add_get_member_variable_node(name))
        require(node is not None, 'Getter creation failed: ' + name)
        return out(node, name)

    def set(self, name, execute, value=None, default=None, local=False):
        node = (self.editor.add_set_local_variable_node(name) if local
                else self.editor.add_set_member_variable_node(name))
        require(node is not None, 'Setter creation failed: ' + name)
        if value is not None:
            connect(value, inp(node, name))
        elif default is not None:
            literal(inp(node, name), default)
        connect(execute, inp(node, 'execute'))
        return out(node, 'then')

    def branch(self, execute, condition):
        node = self.editor.add_branch_node()
        connect(execute, inp(node, 'execute'))
        connect(condition, inp(node, 'Condition'))
        return out(node, 'then'), out(node, 'else')

    def bool_op(self, operation, a, b):
        return out(self.native('KismetMathLibrary', operation, A=a, B=b))

    def valid(self, obj):
        return out(self.native('KismetSystemLibrary', 'IsValid', Object=obj))

    def local_guard(self):
        owner = self.native('ActorComponent', 'GetOwner')
        cast = self.editor.create_node_from_name('Utilities|Casting|CastToPlayerController',
                                                unreal.Vector2D(220, 0), [], None)
        require(cast is not None, 'PlayerController cast node unavailable.')
        connect(out(owner), inp(cast, 'Object'))
        connect(self.entry, inp(cast, 'execute'))
        pc = out(cast, 'AsPlayerController')
        local = out(self.native('Controller', 'IsLocalController', self=pc))
        return self.branch(out(cast, 'then'), local)[0], pc

    def target_guard(self, execute, target):
        actor = self.native('GameplayStatics', 'ObjectIsA', Object=target)
        scene = self.native('GameplayStatics', 'ObjectIsA', Object=target)
        literal(inp(actor, 'ObjectClass'), '/Script/Engine.Actor')
        literal(inp(scene, 'ObjectClass'), '/Script/Engine.SceneComponent')
        supported = self.bool_op('BooleanOR', out(actor), out(scene))
        return self.branch(execute, self.bool_op('BooleanAND', self.valid(target), supported))[0]

    def module_owner_guard(self, execute, module, pc):
        # Guard first; BooleanAND is not short-circuit evaluation in Blueprint.
        execute = self.branch(execute, self.valid(module))[0]
        owning = out(self.native('Widget', 'GetOwningPlayer', self=module))
        same = out(self.native('KismetMathLibrary', 'EqualEqual_ObjectObject', A=owning, B=pc))
        return self.branch(execute, same)[0]

    def ready_branch(self, execute, pc):
        condition = self.bool_op('BooleanAND', self.get('bReady'), self.valid(self.get('ModuleRef')))
        yes, no = self.branch(execute, condition)
        return self.module_owner_guard(yes, self.get('ModuleRef'), pc), no

    def map(self, channel, operation, **values):
        return self.native('BlueprintMapLibrary', 'Map_' + operation,
                           TargetMap=self.get('Pending' + channel), **values)

    def mutate_map(self, channel, operation, execute, **values):
        node = self.map(channel, operation, **values)
        connect(execute, inp(node, 'execute'))
        return out(node, 'then')

    def forward(self, name, execute, **values):
        node = self.call(self.paths[('Module', name)], self=self.get('ModuleRef'), **values)
        connect(execute, inp(node, 'execute'))
        return out(node, 'then')

    def expected_module_guard(self, execute, pc):
        same = out(self.native('KismetMathLibrary', 'EqualEqual_ObjectObject',
                               A=self.param('Module'), B=self.get('ModuleRef')))
        return self.module_owner_guard(self.branch(execute, same)[0], self.param('Module'), pc)


NATIVE_FUNCTIONS = {
    'ActorComponent': ['GetOwner'], 'Controller': ['IsLocalController'],
    'Widget': ['GetOwningPlayer'], 'KismetSystemLibrary': ['IsValid'],
    'GameplayStatics': ['ObjectIsA'],
    'KismetMathLibrary': ['BooleanAND', 'BooleanOR', 'EqualEqual_ObjectObject', 'Less_IntInt'],
    'BlueprintMapLibrary': ['Map_Add', 'Map_Remove', 'Map_Clear', 'Map_Contains', 'Map_Length',
                            'Map_Keys', 'Map_Find'],
}


def preflight(bp):
    require(bp.generated_class() is not None, 'Compile bridge signatures before wire().')
    module_class = unreal.load_asset(MODULE).generated_class()
    paths = {}
    for owner, names in NATIVE_FUNCTIONS.items():
        package = 'UMG' if owner == 'Widget' else 'Engine'
        for name in names:
            paths[(owner, name)] = function_path('/Script/' + package + '.' + owner, name)
    for name in CONTRACT:
        paths[('Module', name)] = function_path(module_class.get_path_name(), name)
    for name in {**CONTRACT, **LIFECYCLE}:
        paths[('Bridge', name)] = function_path(bp.generated_class().get_path_name(), name)
    macros = unreal.load_asset('/Engine/EditorBlueprintResources/StandardMacros')
    require(macros is not None, 'StandardMacros missing.')
    graphs = [g for g in LIB.list_graphs(macros) if g.get_name() == 'ForEachLoop']
    require(len(graphs) == 1, 'StandardMacros.ForEachLoop is not uniquely resolved.')
    paths['foreach'] = graphs[0].get_path_name()
    return paths


def author_marker(bp, paths, channel, operation):
    plural = operation == 'RemoveAll'
    name = operation + channel + ('Markers' if plural else 'Marker')
    graph = Graph(bp, name, paths)
    execute, pc = graph.local_guard()
    target = None if plural else graph.param('Marker_Object')
    if operation == 'AddOrUpdate':
        execute = graph.target_guard(execute, target)
        yes, no = graph.ready_branch(execute, pc)
        # Remove pending state on ready calls too: a reentrant update/removal
        # during replay must invalidate that key's still-pending snapshot entry.
        yes = graph.mutate_map(channel, 'Remove', yes, Key=target)
        graph.forward(name, yes, Marker_Object=target, MarkerInfo=graph.param('MarkerInfo'))
        contains = out(graph.map(channel, 'Contains', Key=target))
        length = out(graph.map(channel, 'Length'))
        less = graph.native('KismetMathLibrary', 'Less_IntInt', A=length)
        literal(inp(less, 'B'), MAX_PENDING)
        room, overflow = graph.branch(no, graph.bool_op('BooleanOR', contains, out(less)))
        graph.mutate_map(channel, 'Add', room, Key=target, Value=graph.param('MarkerInfo'))
        graph.set('bPendingOverflow', overflow, default='true')
    else:
        # Removal also accepts a now-invalid object identity so queued strong
        # references are not kept merely because a target was destroyed.
        execute = graph.mutate_map(channel, 'Clear' if plural else 'Remove', execute,
                                   **({} if plural else {'Key': target}))
        yes, _ = graph.ready_branch(execute, pc)
        graph.forward(name, yes, **({} if plural else {'Marker_Object': target}))


def author_visibility(bp, paths, visible):
    name = 'ShowCompass' if visible else 'HideCompass'
    graph = Graph(bp, name, paths)
    execute, pc = graph.local_guard()
    execute = graph.set('bCompassVisible', execute, default='true' if visible else 'false')
    execute = graph.set('bPendingVisibility', execute, default='true')
    yes, _ = graph.ready_branch(execute, pc)
    yes = graph.set('bPendingVisibility', yes, default='false')
    graph.forward(name, yes)


def author_attach(bp, paths):
    graph = Graph(bp, 'AttachModule', paths)
    execute, pc = graph.local_guard()
    module = graph.param('Module')
    execute = graph.module_owner_guard(execute, module, pc)
    same = out(graph.native('KismetMathLibrary', 'EqualEqual_ObjectObject', A=module, B=graph.get('ModuleRef')))
    _, changed = graph.branch(execute, same)
    changed = graph.set('bReady', changed, default='false')
    graph.set('ModuleRef', changed, value=module)


def author_detach(bp, paths):
    graph = Graph(bp, 'DetachModule', paths)
    execute, _ = graph.local_guard()
    same = out(graph.native('KismetMathLibrary', 'EqualEqual_ObjectObject',
                            A=graph.param('Module'), B=graph.get('ModuleRef')))
    execute = graph.branch(execute, same)[0]
    execute = graph.set('bReady', execute, default='false')
    graph.set('ModuleRef', execute, default='None')


def author_ready(bp, paths):
    graph = Graph(bp, 'MarkReady', paths)
    execute, pc = graph.local_guard()
    execute = graph.expected_module_guard(execute, pc)
    _, execute = graph.branch(execute, graph.get('bReady'))  # repeated Ready is a no-op
    execute = graph.set('bReady', execute, default='true')
    for channel in STRUCTS:
        keys = graph.map(channel, 'Keys')
        connect(execute, inp(keys, 'execute'))
        loop = graph.editor.add_macro_node(paths['foreach'])
        require(loop is not None, 'Cannot create standard ForEachLoop macro.')
        connect(out(keys, 'Keys'), inp(loop, 'Array'))
        connect(out(keys, 'then'), inp(loop, 'Exec'))
        key = out(loop, 'Array Element')
        body = graph.expected_module_guard(out(loop, 'LoopBody'), pc)
        find = graph.map(channel, 'Find', Key=key)
        body = graph.branch(body, out(find))[0]
        body = graph.set('Replay' + channel + 'Info', body, value=out(find, 'Value'), local=True)
        body = graph.mutate_map(channel, 'Remove', body, Key=key)
        # Public Add validates targets again after Delay(0)/destruction and
        # removes queued state before a synchronous module callback can recur.
        forward = graph.call(paths[('Bridge', 'AddOrUpdate' + channel + 'Marker')],
                             Marker_Object=key, MarkerInfo=graph.get('Replay' + channel + 'Info', local=True))
        connect(body, inp(forward, 'execute'))
        execute = out(loop, 'Completed')
    execute = graph.expected_module_guard(execute, pc)
    execute = graph.branch(execute, graph.get('bPendingVisibility'))[0]
    execute = graph.set('bPendingVisibility', execute, default='false')
    show, hide = graph.branch(execute, graph.get('bCompassVisible'))
    graph.forward('ShowCompass', show)
    graph.forward('HideCompass', hide)


def wire():
    """Wire prepared/compiled signatures. Does not compile or save."""
    idle()
    bp = owned()
    stage = unreal.EditorAssetLibrary.get_metadata_tag(bp, STAGE_KEY)
    if stage == 'wired':
        return receipt('wire-noop', current_stage=stage)
    require(stage == 'prepared', 'Partial authoring stage; inspect before retrying: ' + stage)
    paths = preflight(bp)  # missing module/native API fails BEFORE graph edits
    unreal.EditorAssetLibrary.set_metadata_tag(bp, STAGE_KEY, 'wiring')
    for channel in STRUCTS:
        for operation in ('AddOrUpdate', 'Remove', 'RemoveAll'):
            author_marker(bp, paths, channel, operation)
    author_visibility(bp, paths, True)
    author_visibility(bp, paths, False)
    author_attach(bp, paths)
    author_detach(bp, paths)
    author_ready(bp, paths)
    unreal.EditorAssetLibrary.set_metadata_tag(bp, STAGE_KEY, 'wired')
    return receipt('wired', next_step='Dedicated native compile; inspect all warnings/errors; save TARGET only.',
                   runtime_verified=False, active_hud_attached=False)
