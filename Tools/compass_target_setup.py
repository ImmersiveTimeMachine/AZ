# @Description: Author an explicit per-player navigation target example actor.
"""Authoring definitions only; importing does not modify assets.

prepare() -> dedicated native Blueprint compile -> wire() -> native compile ->
defaults(show_on_compass=..., show_in_world=...) -> explicit save by parent.
No auto-registration, map placement, quest data, PIE/tests, or C++ changes.
See compass_target_README.md for runtime integration and the remaining style step.
"""
import importlib.util
import json
from pathlib import Path
import unreal

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location('az_compass_target_bridge_support', HERE / 'compass_bridge_setup.py')
B = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(B)
BL, GE = unreal.BlueprintEditorLibrary, unreal.BlueprintGraphEditor
TARGET = B.DEST + '/Examples/BP_AZ_NavigationTarget'
OWNER_KEY = 'AZ.Compass.TargetAuthoring'
OWNER = 'compass_target_setup:v1'
STAGE_KEY = 'AZ.Compass.TargetStage'
FUNCTIONS = {'RegisterForPlayer': True, 'UnregisterForPlayer': True,
             'UnregisterAllRemembered': False}


def receipt(stage, **details):
    result = dict(stage=stage, target=TARGET, bridge=B.TARGET, **details)
    B.OUT.mkdir(parents=True, exist_ok=True)
    (B.OUT / ('target-' + stage + '.json')).write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


def owned():
    bp = unreal.load_asset(TARGET)
    B.require(bp is not None, 'Run target prepare() first.')
    B.require(unreal.EditorAssetLibrary.get_metadata_tag(bp, OWNER_KEY) == OWNER,
              'Refusing to overwrite unowned target: ' + TARGET)
    return bp


def bridge_class():
    bp = unreal.load_asset(B.TARGET)
    B.require(bp is not None and bp.generated_class() is not None, 'Compile compass bridge first.')
    return bp.generated_class()


def default_scene_root(bp):
    """Use loaded editor subobject APIs, not a guessed CDO RootComponent value."""
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    B.require(subsystem is not None, 'SubobjectDataSubsystem unavailable.')
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    roots = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        if lib.is_scene_component(data) and lib.is_root_component(data):
            obj = lib.get_associated_object(data)
            roots.append((obj, lib.is_default_scene_root(data)))
    B.require(len(roots) == 1 and roots[0][0] is not None and roots[0][1],
              'Expected one native-authored DefaultSceneRoot. Inspect target components before wiring.')
    return roots[0][0].get_path_name()


def prepare():
    B.idle()
    cls = bridge_class()
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET):
        bp = owned()
        stage = unreal.EditorAssetLibrary.get_metadata_tag(bp, STAGE_KEY)
        B.require(stage in ('prepared', 'wired'), 'Partial target preparation: inspect before retrying.')
        return receipt('prepare-noop', current_stage=stage)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.Actor.static_class())
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        TARGET.rsplit('/', 1)[1], TARGET.rsplit('/', 1)[0], unreal.Blueprint.static_class(), factory)
    B.require(bp is not None, 'Could not create target Blueprint.')
    unreal.EditorAssetLibrary.set_metadata_tag(bp, OWNER_KEY, OWNER)
    unreal.EditorAssetLibrary.set_metadata_tag(bp, STAGE_KEY, 'preparing')
    bool_type = BL.get_basic_type_by_name('bool')
    bridge_type = BL.get_object_reference_type(cls)
    array_type = BL.get_array_type(bridge_type)
    for name, struct_path in [('CompassInfo', B.STRUCTS['Compass']), ('WorldInfo', B.STRUCTS['World'])]:
        struct = unreal.load_asset(struct_path)
        B.require(struct is not None, 'Missing exact vendor struct: ' + struct_path)
        B.require(BL.add_member_variable(bp, name, BL.get_struct_type(struct)), 'Cannot add ' + name)
    for name in ('bShowOnCompass', 'bShowInWorld'):
        B.require(BL.add_member_variable(bp, name, bool_type), 'Cannot add ' + name)
    for name in ('CompassInfo', 'WorldInfo', 'bShowOnCompass', 'bShowInWorld'):
        BL.set_blueprint_variable_instance_editable(bp, name, True)
        BL.set_blueprint_variable_category(bp, name, 'Navigation|Target')
    B.require(BL.add_member_variable(bp, 'RegisteredBridges', array_type), 'Cannot add remembered bridges.')
    BL.set_blueprint_variable_category(bp, 'RegisteredBridges', 'Navigation|Runtime')
    pc_type = BL.get_object_reference_type(unreal.PlayerController.static_class())
    for name, public in FUNCTIONS.items():
        editor = GE.create_and_edit_function_graph(bp, name)
        B.require(editor is not None, 'Cannot create target function: ' + name)
        editor.set_function_is_public() if public else editor.set_function_is_private()
        if public:
            B.require(editor.add_graph_input_parameter('Controller', pc_type).is_valid(), 'Cannot add controller input.')
        if name == 'RegisterForPlayer':
            B.require(editor.add_local_variable('ResolvedBridge', bridge_type), 'Cannot add bridge local.')
        else:
            B.require(editor.add_local_variable('BridgeSnapshot', array_type), 'Cannot add snapshot local.')
    unreal.EditorAssetLibrary.set_metadata_tag(bp, STAGE_KEY, 'prepared')
    return receipt('prepared', next_step='Native compile target; wire() validates DefaultSceneRoot before edits.')


class TargetGraph(B.Graph):
    def controller_guard(self):
        pc = self.param('Controller')
        execute = self.branch(self.entry, self.valid(pc))[0]
        local = B.out(self.native('Controller', 'IsLocalController', self=pc))
        return self.branch(execute, local)[0], pc

    def self_value(self):
        if not hasattr(self, '_self_value'):
            node = self.editor.create_node_from_name('Variables|Getareferencetoself',
                                                    unreal.Vector2D(0, 400), [], None)
            B.require(node is not None and node.get_class().get_name() == 'K2Node_Self', 'Cannot create actor Self.')
            self._self_value = B.out(node, 'self')
        return self._self_value

    def exec_call(this, path, execute, **values):
        node = this.call(path, **values)
        B.connect(execute, B.inp(node, 'execute'))
        return B.out(node, 'then')

    def array_mutation(self, operation, execute, **values):
        node = self.native('KismetArrayLibrary', 'Array_' + operation,
                           TargetArray=self.get('RegisteredBridges'), **values)
        B.connect(execute, B.inp(node, 'execute'))
        return B.out(node, 'then')

    def cast_bridge(self, graph_name, execute, value):
        # Existing loaded project bridge supplies exact TargetClass; no new C++.
        before = set(self.editor.list_all_nodes())
        guid = unreal.AZ_BlueprintNodeUtils.add_cast_node(
            TARGET, graph_name, bridge_class(), False, 0, 0)
        B.require(bool(guid), 'Typed bridge cast could not be authored.')
        added = [node for node in self.editor.list_all_nodes() if node not in before]
        B.require(len(added) == 1, 'Unexpected cast-node creation result.')
        node = added[0]
        B.connect(execute, B.inp(node, 'execute'))
        B.connect(value, B.inp(node, 'Object'))
        # B.out normalizes AsType spacing against the generated display name.
        result = B.out(node, 'As' + bridge_class().get_name().removesuffix('_C'))
        return B.out(node, 'then'), result

    def remove_channels(self, execute, bridge):
        # Deliberately no IsValid(Self). Actor EndPlay can run while Self is
        # pending kill; map removal must still receive its exact identity.
        for name in ('RemoveCompassMarker', 'RemoveWorldMarker'):
            execute = self.exec_call(self.paths[('Bridge', name)], execute,
                                     self=bridge, Marker_Object=self.self_value())
        return execute


def preflight(bp):
    root = default_scene_root(bp)
    B.require(hasattr(unreal, 'AZ_BlueprintNodeUtils'), 'Loaded typed cast helper unavailable.')
    cls = bridge_class()
    paths = {}
    for owner, names in {
        'Actor': ['GetComponentByClass', 'AddComponentByClass'],
        'ActorComponent': ['GetOwner'], 'Controller': ['IsLocalController'],
        'KismetSystemLibrary': ['IsValid'],
        'KismetMathLibrary': ['MakeTransform', 'EqualEqual_ObjectObject'],
        'KismetArrayLibrary': ['Array_AddUnique', 'Array_RemoveItem', 'Array_Clear'],
    }.items():
        for name in names:
            paths[(owner, name)] = B.function_path('/Script/Engine.' + owner, name)
    for name in ('AddOrUpdateCompassMarker', 'AddOrUpdateWorldMarker', 'RemoveCompassMarker', 'RemoveWorldMarker'):
        paths[('Bridge', name)] = B.function_path(cls.get_path_name(), name)
    paths[('Target', 'UnregisterAllRemembered')] = B.function_path(bp.generated_class().get_path_name(),
                                                               'UnregisterAllRemembered')
    macros = unreal.load_asset('/Engine/EditorBlueprintResources/StandardMacros')
    B.require(macros is not None, 'StandardMacros unavailable.')
    graphs = [g for g in BL.list_graphs(macros) if g.get_name() == 'ForEachLoop']
    B.require(len(graphs) == 1, 'ForEachLoop graph is not uniquely resolved.')
    paths['foreach'] = graphs[0].get_path_name()
    return paths, root


def register(bp, paths):
    graph = TargetGraph(bp, 'RegisterForPlayer', paths)
    execute, pc = graph.controller_guard()
    get = graph.native('Actor', 'GetComponentByClass', self=pc)
    B.literal(B.inp(get, 'ComponentClass'), bridge_class().get_path_name())
    exists, missing = graph.branch(execute, graph.valid(B.out(get)))
    old_done, old_value = graph.cast_bridge('RegisterForPlayer', exists, B.out(get))
    old_done = graph.set('ResolvedBridge', old_done, value=old_value, local=True)
    create = graph.native('Actor', 'AddComponentByClass', self=pc)
    B.literal(B.inp(create, 'Class'), bridge_class().get_path_name())
    B.literal(B.inp(create, 'bManualAttachment'), 'false')
    identity = graph.native('KismetMathLibrary', 'MakeTransform')
    # A real MakeTransform supplies valid identity defaults, including scale1.
    # Never leave the relative-transform input as a zero/empty struct literal.
    B.connect(B.out(identity), B.inp(create, 'RelativeTransform'))
    deferred = create.find_input_pin('bDeferredFinish')
    if deferred.is_valid():
        B.literal(deferred, 'false')
    B.connect(missing, B.inp(create, 'execute'))
    new_done, new_value = graph.cast_bridge('RegisterForPlayer', B.out(create, 'then'), B.out(create))
    new_done = graph.set('ResolvedBridge', new_done, value=new_value, local=True)
    bridge = graph.get('ResolvedBridge', local=True)
    remember = graph.native('KismetArrayLibrary', 'Array_AddUnique',
                            TargetArray=graph.get('RegisteredBridges'), NewItem=bridge)
    B.connect(old_done, B.inp(remember, 'execute'))
    B.connect(new_done, B.inp(remember, 'execute'))
    execute = B.out(remember, 'then')
    # Re-registering also removes a channel that was switched off since the last
    # call; no stale marker survives a designer's channel selection change.
    for channel, flag, info in [('Compass', 'bShowOnCompass', 'CompassInfo'),
                               ('World', 'bShowInWorld', 'WorldInfo')]:
        yes, no = graph.branch(execute, graph.get(flag))
        add_done = graph.exec_call(paths[('Bridge', 'AddOrUpdate' + channel + 'Marker')], yes,
                                   self=bridge, Marker_Object=graph.self_value(), MarkerInfo=graph.get(info))
        remove_done = graph.exec_call(paths[('Bridge', 'Remove' + channel + 'Marker')], no,
                                      self=bridge, Marker_Object=graph.self_value())
        # Merge execution with a harmless validity branch; its true path is the
        # continuation. This also stops if external marker callbacks destroy the
        # bridge while forwarding the first channel.
        join = graph.editor.add_branch_node()
        B.connect(add_done, B.inp(join, 'execute'))
        B.connect(remove_done, B.inp(join, 'execute'))
        B.connect(graph.valid(bridge), B.inp(join, 'Condition'))
        execute = B.out(join, 'then')


def unregister(bp, paths, all_players=False):
    name = 'UnregisterAllRemembered' if all_players else 'UnregisterForPlayer'
    graph = TargetGraph(bp, name, paths)
    if all_players:
        execute, pc = graph.entry, None
    else:
        execute, pc = graph.controller_guard()
    execute = graph.set('BridgeSnapshot', execute, value=graph.get('RegisteredBridges'), local=True)
    if all_players:
        # Drop remembered ownership BEFORE callbacks, and iterate the snapshot.
        # Reentrant removal cannot corrupt the list currently being traversed.
        execute = graph.array_mutation('Clear', execute)
    loop = graph.editor.add_macro_node(paths['foreach'])
    B.require(loop is not None, 'Cannot create snapshot loop.')
    B.connect(graph.get('BridgeSnapshot', local=True), B.inp(loop, 'Array'))
    B.connect(execute, B.inp(loop, 'Exec'))
    bridge = B.out(loop, 'Array Element')
    body = graph.branch(B.out(loop, 'LoopBody'), graph.valid(bridge))[0]
    if not all_players:
        owner = B.out(graph.native('ActorComponent', 'GetOwner', self=bridge))
        same = B.out(graph.native('KismetMathLibrary', 'EqualEqual_ObjectObject', A=owner, B=pc))
        body = graph.branch(body, same)[0]
        body = graph.array_mutation('RemoveItem', body, Item=bridge)
    graph.remove_channels(body, bridge)


def end_play(bp, paths):
    event = BL.add_event_override(bp, 'ReceiveEndPlay', unreal.IntPoint(0, 0))
    B.require(event is not None, 'Could not create Actor ReceiveEndPlay.')
    entry = B.out(event, 'then')
    B.require(not entry.list_connected_pins(), 'Target EndPlay already has work; inspect instead of overwriting.')
    editor = GE.get_graph_editor(event.get_outer())
    call = editor.add_call_function_node(paths[('Target', 'UnregisterAllRemembered')])
    B.require(call is not None, 'Could not create EndPlay cleanup call.')
    B.connect(entry, B.inp(call, 'execute'))


def wire():
    B.idle()
    bp = owned()
    stage = unreal.EditorAssetLibrary.get_metadata_tag(bp, STAGE_KEY)
    if stage == 'wired':
        return receipt('wire-noop')
    B.require(stage == 'prepared', 'Partial target stage: inspect before retrying: ' + stage)
    paths, root = preflight(bp)
    unreal.EditorAssetLibrary.set_metadata_tag(bp, STAGE_KEY, 'wiring')
    register(bp, paths)
    unregister(bp, paths)
    unregister(bp, paths, True)
    end_play(bp, paths)
    unreal.EditorAssetLibrary.set_metadata_tag(bp, STAGE_KEY, 'wired')
    return receipt('wired', default_scene_root=root, runtime_verified=False,
                   next_step='Native compile; defaults(show_on_compass=..., show_in_world=...); parent authors exact Info/icons.')


def defaults(*, show_on_compass, show_in_world):
    """After compilation: select channels explicitly; do not guess Info members."""
    B.idle()
    bp = owned()
    B.require(unreal.EditorAssetLibrary.get_metadata_tag(bp, STAGE_KEY) == 'wired', 'Wire/compile target first.')
    B.require(type(show_on_compass) is bool and type(show_in_world) is bool, 'Pass explicit boolean channel defaults.')
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('bShowOnCompass', show_on_compass)
    cdo.set_editor_property('bShowInWorld', show_in_world)
    tick = cdo.get_editor_property('primary_actor_tick')
    # bCanEverTick is not reflected in this engine; no Tick event is authored.
    tick.set_editor_property('start_with_tick_enabled', False)
    cdo.set_editor_property('primary_actor_tick', tick)
    B.require(cdo.get_editor_property('bShowOnCompass') == show_on_compass
              and cdo.get_editor_property('bShowInWorld') == show_in_world, 'Channel readback mismatch.')
    return receipt('defaults', show_on_compass=show_on_compass, show_in_world=show_in_world,
                   info_defaults_authored=False, next_step='Parent sets exact Info/icon defaults, saves explicit asset; no level placement.')
