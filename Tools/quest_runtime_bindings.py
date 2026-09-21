# @Description: Author the local-player QuestModule runtime adapter, without compiling or starting Play.
"""Import-inert, staged authoring of the OWNED CHALK QuestModule only.

Editor stages: tree(); prepare(); [native compile]; prepare_callbacks();
[native compile]; wire(); [native compile]; seed_marker_defaults(); [save].
The parent performs each native compilation in a separate tool invocation.

Optional Map-page load route: load_controls_tree(); prepare_load_controls();
[native compile]; prepare_load_callback(); [native compile];
wire_load_controls(); [native compile/save]. It adds only a separate control
group and preserves the existing native journal/map button handlers.

The presentation owner implements RefreshTrackedMissionVisuals() separately in
quest_mission_presentation.py. This file prepares its signature, NEVER its body.
GameHUD calls RuntimeInitialize(Bridge) on its owned QuestModule BEFORE hosting
it; initialize the existing compass host first. See contract() for exact details.
No C++, vendor assets, GameHUD layout, quest mutations, save requests, timers, Tick,
PIE, tests, compilation, or asset saving are performed by this recipe.

All editor stages fail closed while Play is active. Completed stages are
idempotent; partial stages require inspection, never a broad graph rebuild.
"""
from __future__ import annotations

import importlib.util
import json
import re
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'Saved/QuestMapImplementation/RuntimeBindings'
MODULE = '/Game/AZ/Blueprints/Menu/HUD/Navigation/Quests/WBP_AZ_QuestModule'
BRIDGE = '/Game/AZ/Blueprints/Menu/HUD/Navigation/BPC_AZ_CompassBridge'
NAV_CLASS = '/Script/AZ.AZ_QuestMapComponent'
SAVE_CLASS = '/Script/AZ.AZ_CampaignSaveCoordinator'
SOURCE_OWNER_KEY, SOURCE_OWNER = 'AZ.Quest.AuthoringOwner', 'quest_prohud_setup:v1'
KEY = 'AZ.Quest.Runtime.'
VERSION = 'v1'
STRUCTS = {
    'Compass': '/Game/ProHUDV2_Horror/Blueprints/Structs/S_CompassMarkerInfo_H',
    'World': '/Game/ProHUDV2_Horror/Blueprints/Structs/S_WorldMarkerInfo_H',
}
DELEGATES = (
    ('BoundQuestMap', NAV_CLASS, 'OnViewChanged', 'RuntimeViewChanged', ()),
    ('BoundQuestMap', NAV_CLASS, 'OnMarkerUpsert', 'RuntimeMarkerUpsert', ('MarkerObject', 'Label', 'bPersonal')),
    ('BoundQuestMap', NAV_CLASS, 'OnMarkerRemove', 'RuntimeMarkerRemove', ('MarkerObject',)),
    ('BoundCampaignSave', SAVE_CLASS, 'OnSaveCompleted', 'RuntimeSaveCompleted', ('bSuccess', 'Message')),
    ('BoundCampaignSave', SAVE_CLASS, 'OnLoadCompleted', 'RuntimeLoadCompleted', ('bSuccess', 'Message')),
)
FUNCTIONS = {
    'RuntimeInitialize': [('Bridge', 'Bridge')],
    'RuntimeShutdown': [],
    'RuntimeUnbindDelegates': [],
    'RuntimeRemovePublishedMarkers': [],
    'ShowCampaignResult': [('bSuccess', 'bool'), ('Message', 'string'), ('bLoad', 'bool')],
    'RefreshTrackedMissionVisuals': [],
}
MAP_PAGE = '/Game/AZ/Blueprints/Menu/Map/WBP_AZ_QuestMapPage'
LOAD_FUNCTIONS = {
    'InitializeCheckpointControls': [], 'ShutdownCheckpointControls': [],
    'RequestCheckpointLoadConfirmation': [], 'CancelCheckpointLoadConfirmation': [],
    'ConfirmCheckpointLoad': [], 'RefreshCheckpointAvailability': [],
    'ShowCheckpointLoadStatus': [('Message', 'string')],
}
LOAD_DELEGATES = (
    ('LoadRequestButton', '/Script/UMG.Button', 'OnClicked', 'RequestCheckpointLoadConfirmation', ()),
    ('LoadConfirmButton', '/Script/UMG.Button', 'OnClicked', 'ConfirmCheckpointLoad', ()),
    ('LoadCancelButton', '/Script/UMG.Button', 'OnClicked', 'CancelCheckpointLoadConfirmation', ()),
    ('BoundLoadCoordinator', SAVE_CLASS, 'OnLoadCompleted', 'CheckpointLoadCompleted', ('bSuccess', 'Message')),
    ('BoundLoadCoordinator', SAVE_CLASS, 'OnSaveCompleted', 'CheckpointSaveCompleted', ('bSuccess', 'Message')),
)


def require(value, message):
    if not value:
        raise RuntimeError(message)


def ue():
    import unreal
    return unreal


def support():
    """Import existing verified graph helpers only inside an explicit editor stage."""
    spec = importlib.util.spec_from_file_location('az_quest_runtime_bridge_support', ROOT / 'Tools/compass_bridge_setup.py')
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def norm(value):
    return re.sub(r'[^a-z0-9]', '', str(value).lower())


def receipt(stage, **values):
    result = dict(stage=stage, module=MODULE, runtime_verified=False, **values)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / (stage + '.json')).write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


def load(path):
    value = ue().load_asset(path)
    require(value is not None, 'Missing asset: ' + path)
    return value


def klass(path):
    value = ue().load_class(None, path)
    require(value is not None, 'Missing compiled class: ' + path)
    return value


def owned():
    value = load(MODULE)
    require(ue().EditorAssetLibrary.get_metadata_tag(value, SOURCE_OWNER_KEY) == SOURCE_OWNER,
            'Refusing an unowned QuestModule')
    require(not isinstance(value, ue().AnimBlueprint), 'Animation assets are out of scope')
    return value


def state(bp, name):
    return ue().EditorAssetLibrary.get_metadata_tag(bp, KEY + name)


def stamp(bp, name, value):
    ue().EditorAssetLibrary.set_metadata_tag(bp, KEY + name, value)


def stage_start(bp, name):
    old = state(bp, name)
    require(old in ('', VERSION), 'Interrupted ' + name + '; inspect before retrying: ' + old)
    if old == VERSION:
        return False
    stamp(bp, name, 'writing')
    return True


def tool(name, **args):
    group, method = name.rsplit('.', 1)
    result = ue().ToolsetRegistry.execute_tool(group, method, json.dumps(args))
    require(result.is_complete and not result.error, name + ': ' + str(result.error))
    return json.loads(result.value)['returnValue']


def idle():
    require(not tool('EditorToolset.EditorAppToolset.IsPIERunning'),
            'Play is active; let the user finish before authoring')


def tree():
    """Add only the shared root and optional result text; preserve presentation children."""
    idle()
    bp = owned()
    ref = {'refPath': MODULE + '.' + MODULE.rsplit('/', 1)[1]}
    prefix = 'UMGToolSet.UMGToolSet.'
    rows = tool(prefix + 'GetWidgets', widgetBlueprint=ref)
    rows = {row['widgetName']: row for row in rows['widgets'] if isinstance(row.get('widget'), dict)}
    require(not rows or 'QuestModuleRoot' in rows, 'Nonempty module tree has a different root; preserve and inspect')
    if 'QuestModuleRoot' not in rows:
        root = tool(prefix + 'AddWidget', widgetBlueprint=ref,
                    widgetClass={'refPath': '/Script/UMG.CanvasPanel'}, widgetDisplayName='QuestModuleRoot')
    else:
        root = rows['QuestModuleRoot']
        require(root['widgetClassPath']['refPath'] == '/Script/UMG.CanvasPanel', 'Unexpected module root type')
    if 'CampaignStatusText' not in rows:
        child = tool(prefix + 'AddWidget', widgetBlueprint=ref, parentWidget=root['widget'],
                     widgetClass={'refPath': '/Script/UMG.TextBlock'}, widgetDisplayName='CampaignStatusText')
        props = {'visibility': 'Collapsed'}
        require(tool('editor_toolset.toolsets.object.ObjectTools.set_properties', instance=child['widget'],
                     values=json.dumps(props)), 'Cannot collapse initial campaign status')
        # Root owns final approved font/color/layout. This small, passive default
        # occupies the top-right safe inset until that styling is applied.
        slot = {'layoutData': {'offsets': {'left': -32, 'top': 160, 'right': 0, 'bottom': 0},
                              'anchors': {'minimum': {'x': 1, 'y': 0}, 'maximum': {'x': 1, 'y': 0}},
                              'alignment': {'x': 1, 'y': 0}}, 'bAutoSize': True, 'zOrder': 5}
        require(tool('editor_toolset.toolsets.object.ObjectTools.set_properties', instance=child['slot'],
                     values=json.dumps(slot)), 'Cannot place campaign status')
    else:
        child = rows['CampaignStatusText']
        require(child['widgetClassPath']['refPath'] == '/Script/UMG.TextBlock', 'CampaignStatusText must be a TextBlock')
    tool(prefix + 'ToggleWidgetAsVariable', widgetBlueprint=ref, widget=child['widget'], bIsVariable=True)
    stamp(bp, 'Tree', VERSION)
    return receipt('tree', native_compile_required=True, preserved_other_widgets=True)


def types():
    lib = ue().BlueprintEditorLibrary
    bridge = load(BRIDGE).generated_class()
    require(bridge is not None, 'Native compile compass bridge first')
    values = {name: lib.get_basic_type_by_name(name) for name in ('bool', 'string', 'text')}
    values.update({name: lib.get_object_reference_type(value) for name, value in {
        'Bridge': bridge, 'Nav': klass(NAV_CLASS), 'Save': klass(SAVE_CLASS),
        'Object': ue().Object.static_class(),
    }.items()})
    values['Objects'] = lib.get_array_type(values['Object'])
    for name, path in STRUCTS.items():
        values[name] = lib.get_struct_type(load(path))
    return values


def prepare():
    """Fields/base functions only. External delegate signatures are copied after compile."""
    idle()
    bp, lib, ge = owned(), ue().BlueprintEditorLibrary, ue().BlueprintGraphEditor
    if state(bp, 'Prepared') == VERSION:
        return receipt('prepare-noop')
    fields = [('BoundQuestMap', 'Nav'), ('BoundCampaignSave', 'Save'), ('BoundCompassBridge', 'Bridge'),
              ('bRuntimeBound', 'bool'), ('PublishedMarkerKeys', 'Objects'),
              ('QuestCompassInfo', 'Compass'), ('PersonalCompassInfo', 'Compass'),
              ('QuestWorldInfo', 'World'), ('PersonalWorldInfo', 'World'),
              ('LastCampaignMessage', 'string'), ('bLastCampaignSuccess', 'bool'), ('bLastCampaignWasLoad', 'bool')]
    pin_types = types()
    old_fields = {str(name) for name in lib.list_member_variable_names(bp, False)}
    require(not (old_fields & {name for name, _ in fields}), 'Runtime field collision; inspect instead of overwriting')
    require(all(lib.find_graph(bp, name) is None for name in FUNCTIONS), 'Runtime function collision')
    require(stage_start(bp, 'Prepared'), 'Unexpected prepared state')
    for name, kind in fields:
        require(lib.add_member_variable(bp, name, pin_types[kind]), 'Cannot add field ' + name)
        lib.set_blueprint_variable_category(bp, name, 'Quest|Runtime')
    for name, parameters in FUNCTIONS.items():
        graph = ge.create_and_edit_function_graph(bp, name)
        require(graph is not None, 'Cannot create ' + name)
        graph.set_function_is_public()
        for parameter, kind in parameters:
            require(graph.add_graph_input_parameter(parameter, pin_types[kind]).is_valid(), 'Cannot add ' + parameter)
        if name == 'RuntimeRemovePublishedMarkers':
            require(graph.add_local_variable('MarkerSnapshot', pin_types['Objects']), 'Cannot add key snapshot')
    stamp(bp, 'Prepared', VERSION)
    return receipt('prepared', native_compile_required=True,
                   next='Native compile module, then prepare_callbacks; renderer owns RefreshTrackedMissionVisuals body')


def action(editor, title, expected_class, context=(), declaring=None):
    """Resolve actual palette action, constrain owner, verify the created node class."""
    names = list(dict.fromkeys(str(value) for value in editor.list_available_nodes(list(context))))
    candidates = [value for value in names if norm(value.rsplit('|', 1)[-1]) == norm(title)]
    require(len(candidates) == 1, 'Missing/ambiguous action ' + title + ': ' + str(candidates))
    node = editor.create_node_from_name(candidates[0], ue().Vector2D(0, 0), list(context), declaring)
    require(node is not None and node.get_class().get_name() == expected_class, 'Wrong node for ' + title)
    return node


def prepare_callbacks():
    """Derive native delegate parameter flags from engine-authored temporary nodes.

    AddDispatcherEventNode cannot read an external component's dispatcher. The
    standard Custom Event action autowires to a delegate pin and invokes the
    engine's SetDelegateSignature (incl. const FString&); copy those actual pin
    types to functions, then remove exactly the three temporary schema nodes.
    """
    idle()
    bp, b = owned(), support()
    require(state(bp, 'Prepared') == VERSION, 'Prepare/native-compile fields first')
    require(bp.generated_class() is not None, 'Native compile module first')
    if state(bp, 'Callbacks') == VERSION:
        return receipt('callbacks-noop')
    require(all(ue().BlueprintEditorLibrary.find_graph(bp, row[3]) is None for row in DELEGATES), 'Callback collision')
    require(stage_start(bp, 'Callbacks'), 'Unexpected callback state')
    lib, ge, pl = ue().BlueprintEditorLibrary, ue().BlueprintGraphEditor, ue().BlueprintGraphPinLibrary
    events = ge.get_graph_editor_by_name(bp, 'EventGraph')
    require(events is not None, 'Missing module EventGraph')
    copied = {}
    for field, owner, dispatcher, handler, expected in DELEGATES:
        getter = events.add_get_member_variable_node(field)
        require(getter is not None, 'Native compile runtime fields first: ' + field)
        target = b.out(getter, field)
        binding = action(events, 'Bind Event to ' + dispatcher, 'K2Node_AddDelegate', [target], klass(owner))
        # Context autowiring connects the native target; enforce the exact pin.
        if not b.inp(binding, 'self').list_connected_pins():
            b.connect(target, b.inp(binding, 'self'))
        scratch = action(events, 'Add Custom Event...', 'K2Node_CustomEvent', [b.inp(binding, 'Delegate')])
        require(b.out(scratch, 'OutputDelegate').list_connected_pins(), 'Engine did not derive delegate signature')
        inputs = [(str(pl.get_pin_name(pin)), pl.get_pin_type(pin)) for pin in lib.list_output_pins(scratch)
                  if str(pl.get_pin_name(pin)) not in ('then', 'OutputDelegate')]
        require(tuple(name for name, _ in inputs) == expected, 'Native signature changed: ' + dispatcher + ': ' + str(inputs))
        graph = ge.create_and_edit_function_graph(bp, handler)
        graph.set_function_is_public()
        for name, pin_type in inputs:
            require(graph.add_graph_input_parameter(name, pin_type).is_valid(), 'Cannot copy delegate parameter ' + name)
        if handler == 'RuntimeMarkerUpsert':
            for name, kind in [('SelectedCompassInfo', 'Compass'), ('SelectedWorldInfo', 'World')]:
                require(graph.add_local_variable(name, types()[kind]), 'Cannot add selected marker style')
        copied[handler] = [name for name, _ in inputs]
        events.remove_nodes([scratch, binding, getter])
    stamp(bp, 'Callbacks', VERSION)
    return receipt('callbacks-prepared', copied_native_signatures=copied, native_compile_required=True)


class RuntimeGraph:
    """Composition around the established compass Graph helpers."""
    def __init__(self, bp, name):
        self.b = support()
        self.bp, self.name = bp, name
        self.g = self.b.Graph(bp, name, {})
        self.editor, self.entry = self.g.editor, self.g.entry

    def __getattr__(self, name):
        return getattr(self.g, name)

    def native(this, owner, function, **values):
        path = this.b.function_path(owner, function)
        return this.g.call(path, **values)

    def value(self, node, name='ReturnValue'):
        return self.b.out(node, name)

    def exec_call(this, path, execute, **values):
        node = this.g.call(path, **values)
        this.b.connect(execute, this.b.inp(node, 'execute'))
        return this.value(node, 'then')

    def call_local(self, name, execute, **values):
        return self.exec_call(self.b.function_path(self.bp.generated_class().get_path_name(), name), execute, **values)

    def valid(self, value):
        return self.value(self.native('/Script/Engine.KismetSystemLibrary', 'IsValid', Object=value))

    def same(self, one, two):
        return self.value(self.native('/Script/Engine.KismetMathLibrary', 'EqualEqual_ObjectObject', A=one, B=two))

    def self_value(self):
        if not hasattr(self, '_self_pin'):
            self._self_pin = self.value(action(self.editor, 'Get a reference to self', 'K2Node_Self'), 'self')
        return self._self_pin

    def local_owner(self, execute):
        pc = self.value(self.native('/Script/UMG.Widget', 'GetOwningPlayer'))
        execute = self.branch(execute, self.valid(pc))[0]
        local = self.value(self.native('/Script/Engine.Controller', 'IsLocalController', self=pc))
        return self.branch(execute, local)[0], pc

    def bridge_guard(self, execute):
        execute, pc = self.local_owner(execute)
        bridge = self.get('BoundCompassBridge')
        execute = self.branch(execute, self.valid(bridge))[0]
        owner = self.value(self.native('/Script/Engine.ActorComponent', 'GetOwner', self=bridge))
        return self.branch(execute, self.same(pc, owner))[0], bridge

    def join(self, branches):
        # An execution-only reroute is represented as a true branch. Never read
        # pure outputs from an operation after its owning collection was changed.
        node = self.editor.add_branch_node()
        self.b.literal(self.b.inp(node, 'Condition'), 'true')
        for pin in branches:
            self.b.connect(pin, self.b.inp(node, 'execute'))
        return self.value(node, 'then')

    def delegate(self, execute, row, bind):
        field, owner, dispatcher, handler, _ = row
        target = self.get(field)
        node = action(self.editor, ('Bind Event to ' if bind else 'Unbind Event from ') + dispatcher,
                      'K2Node_AddDelegate' if bind else 'K2Node_RemoveDelegate', [target], klass(owner))
        target_pin = self.b.inp(node, 'self')
        if not target_pin.list_connected_pins():
            self.b.connect(target, target_pin)
        create = action(self.editor, 'Create Event', 'K2Node_CreateDelegate')
        self.b.connect(self.self_value(), self.b.inp(create, 'self'))
        self.b.connect(self.value(create, 'OutputDelegate'), self.b.inp(node, 'Delegate'))
        lib = ue().BlueprintEditorLibrary
        compatible = {str(name) for name in lib.list_compatible_functions_for_delegate(create)}
        require(handler in compatible, 'Compile exact callback signature first: ' + handler + '; compatible=' + str(compatible))
        lib.set_create_delegate_function(create, handler)
        require(str(lib.get_create_delegate_function(create)) == handler, 'Delegate function readback failed')
        self.b.connect(execute, self.b.inp(node, 'execute'))
        return self.value(node, 'then')

    def array(self, operation, execute, **values):
        node = self.native('/Script/Engine.KismetArrayLibrary', 'Array_' + operation,
                           TargetArray=self.get('PublishedMarkerKeys'), **values)
        self.b.connect(execute, self.b.inp(node, 'execute'))
        return self.value(node, 'then')

    def bridge_call(self, name, execute, **values):
        return self.exec_call(self.b.function_path(load(BRIDGE).generated_class().get_path_name(), name), execute,
                              self=self.get('BoundCompassBridge'), **values)

    def remove_channels(self, execute, key):
        for channel in STRUCTS:
            execute = self.bridge_call('Remove' + channel + 'Marker', execute, Marker_Object=key)
        return execute


def _wire_initialize(bp):
    g = RuntimeGraph(bp, 'RuntimeInitialize')
    execute = g.call_local('RuntimeShutdown', g.entry)
    execute, pc = g.local_owner(execute)
    bridge = g.param('Bridge')
    execute = g.branch(execute, g.valid(bridge))[0]
    owner = g.value(g.native('/Script/Engine.ActorComponent', 'GetOwner', self=bridge))
    execute = g.branch(execute, g.same(owner, pc))[0]
    node = g.native(NAV_CLASS, 'GetOrCreateForController', Controller=pc)
    g.b.connect(execute, g.b.inp(node, 'execute'))
    execute = g.set('BoundQuestMap', g.value(node, 'then'), value=g.value(node))
    execute = g.branch(execute, g.valid(g.get('BoundQuestMap')))[0]
    execute = g.set('BoundCompassBridge', execute, value=bridge)
    for row in DELEGATES[:3]:
        execute = g.delegate(execute, row, True)
    # Campaign checkpointing is local-authority only. Its absence must not
    # disable the local client's otherwise valid quest/compass presentation.
    save = g.native(SAVE_CLASS, 'GetOrCreateForController', Controller=pc)
    g.b.connect(execute, g.b.inp(save, 'execute'))
    execute = g.set('BoundCampaignSave', g.value(save, 'then'), value=g.value(save))
    yes, no = g.branch(execute, g.valid(g.get('BoundCampaignSave')))
    for row in DELEGATES[3:]:
        yes = g.delegate(yes, row, True)
    execute = g.join([yes, no])
    execute = g.set('bRuntimeBound', execute, default='true')
    # Native RefreshBindings can broadcast immediately: all handlers are bound.
    execute = g.exec_call(g.b.function_path(NAV_CLASS, 'RefreshBindings'), execute, self=g.get('BoundQuestMap'))
    yes, no = g.branch(execute, g.valid(g.get('BoundCampaignSave')))
    yes = g.exec_call(g.b.function_path(SAVE_CLASS, 'RefreshBindings'), yes, self=g.get('BoundCampaignSave'))
    execute = g.join([yes, no])
    execute = g.call_local('RuntimeViewChanged', execute)
    g.exec_call(g.b.function_path(NAV_CLASS, 'ReplayNavigationMarkers'), execute, self=g.get('BoundQuestMap'))


def _wire_unbind(bp):
    g = RuntimeGraph(bp, 'RuntimeUnbindDelegates')
    execute = g.entry
    for field in ('BoundQuestMap', 'BoundCampaignSave'):
        yes, no = g.branch(execute, g.valid(g.get(field)))
        for row in (row for row in DELEGATES if row[0] == field):
            yes = g.delegate(yes, row, False)
        execute = g.join([yes, no])


def _wire_remove_published(bp):
    g = RuntimeGraph(bp, 'RuntimeRemovePublishedMarkers')
    execute = g.set('MarkerSnapshot', g.entry, value=g.get('PublishedMarkerKeys'), local=True)
    execute = g.array('Clear', execute)
    execute = g.branch(execute, g.valid(g.get('BoundCompassBridge')))[0]
    macros = load('/Engine/EditorBlueprintResources/StandardMacros')
    choices = [graph for graph in ue().BlueprintEditorLibrary.list_graphs(macros) if graph.get_name() == 'ForEachLoop']
    require(len(choices) == 1, 'Standard ForEachLoop is not unique')
    loop = g.editor.add_macro_node(choices[0].get_path_name())
    g.b.connect(execute, g.b.inp(loop, 'Exec'))
    g.b.connect(g.get('MarkerSnapshot', local=True), g.b.inp(loop, 'Array'))
    # Exact identities can be pending-kill. The bridge removes their map entries.
    g.remove_channels(g.value(loop, 'LoopBody'), g.value(loop, 'Array Element'))


def _wire_shutdown(bp):
    g = RuntimeGraph(bp, 'RuntimeShutdown')
    execute = g.set('bRuntimeBound', g.entry, default='false')
    execute = g.call_local('RuntimeUnbindDelegates', execute)
    execute = g.call_local('RuntimeRemovePublishedMarkers', execute)
    for field in ('BoundQuestMap', 'BoundCampaignSave', 'BoundCompassBridge'):
        execute = g.set(field, execute, default='None')
    # No PC/pawn-valid gate above: teardown must unbind even while owner dies.


def _world_info_with_label(g, style, label):
    struct_name = 'S_WorldMarkerInfo_H'
    old = action(g.editor, 'Break ' + struct_name, 'K2Node_BreakStruct')
    new = action(g.editor, 'Make ' + struct_name, 'K2Node_MakeStruct')
    g.b.connect(style, g.b.inp(old, struct_name))
    for pin in ue().BlueprintEditorLibrary.list_output_pins(old):
        name = str(pin.get_pin_name())
        g.b.connect(pin, g.b.inp(new, name))
    # User-defined struct internal names contain stable GUID suffixes. Match
    # the audited MarkerName field only, never MarkerNameFontInfo.
    members = [pin for pin in ue().BlueprintEditorLibrary.list_input_pins(new)
               if norm(str(pin.get_pin_name()).split('_', 1)[0]) == 'markername']
    require(len(members) == 1, 'World marker Label field changed; inspect exact vendor struct')
    ue().BlueprintGraphPinLibrary.break_pin_links(members[0])
    g.b.connect(label, members[0])
    return g.value(new, struct_name)


def _wire_upsert(bp):
    g = RuntimeGraph(bp, 'RuntimeMarkerUpsert')
    execute = g.branch(g.entry, g.get('bRuntimeBound'))[0]
    execute, _ = g.bridge_guard(execute)
    key = g.param('MarkerObject')
    execute = g.branch(execute, g.valid(key))[0]
    # Native QuestMap emits SceneComponent anchors. The bridge independently
    # enforces Actor/SceneComponent support and owning-local-controller context.
    execute = g.array('AddUnique', execute, NewItem=key)
    personal, quest = g.branch(execute, g.param('bPersonal'))
    done = []
    for branch, prefix in ((personal, 'Personal'), (quest, 'Quest')):
        branch = g.set('SelectedCompassInfo', branch, value=g.get(prefix + 'CompassInfo'), local=True)
        done.append(g.set('SelectedWorldInfo', branch, value=g.get(prefix + 'WorldInfo'), local=True))
    execute = g.join(done)
    execute = g.bridge_call('AddOrUpdateCompassMarker', execute, Marker_Object=key,
                            MarkerInfo=g.get('SelectedCompassInfo', local=True))
    world = _world_info_with_label(g, g.get('SelectedWorldInfo', local=True), g.param('Label'))
    g.bridge_call('AddOrUpdateWorldMarker', execute, Marker_Object=key, MarkerInfo=world)


def _wire_remove(bp):
    g = RuntimeGraph(bp, 'RuntimeMarkerRemove')
    execute = g.branch(g.entry, g.get('bRuntimeBound'))[0]
    # Remove remembered identity BEFORE external widget callbacks. Do not reject
    # invalid/destroyed keys; these are precisely why removal must be identity-based.
    execute = g.array('RemoveItem', execute, Item=g.param('MarkerObject'))
    execute = g.branch(execute, g.valid(g.get('BoundCompassBridge')))[0]
    g.remove_channels(execute, g.param('MarkerObject'))


def _wire_view(bp):
    g = RuntimeGraph(bp, 'RuntimeViewChanged')
    execute = g.branch(g.entry, g.get('bRuntimeBound'))[0]
    execute = g.branch(execute, g.valid(g.get('BoundQuestMap')))[0]
    g.call_local('RefreshTrackedMissionVisuals', execute)


def _wire_results(bp):
    for callback, is_load in (('RuntimeSaveCompleted', False), ('RuntimeLoadCompleted', True)):
        g = RuntimeGraph(bp, callback)
        execute = g.branch(g.entry, g.get('bRuntimeBound'))[0]
        node = g.call(g.b.function_path(bp.generated_class().get_path_name(), 'ShowCampaignResult'),
                      bSuccess=g.param('bSuccess'), Message=g.param('Message'))
        g.b.literal(g.b.inp(node, 'bLoad'), str(is_load).lower())
        g.b.connect(execute, g.b.inp(node, 'execute'))
    g = RuntimeGraph(bp, 'ShowCampaignResult')
    execute = g.set('LastCampaignMessage', g.entry, value=g.param('Message'))
    execute = g.set('bLastCampaignSuccess', execute, value=g.param('bSuccess'))
    execute = g.set('bLastCampaignWasLoad', execute, value=g.param('bLoad'))
    text = g.get('CampaignStatusText')
    execute = g.branch(execute, g.valid(text))[0]
    message = g.value(g.native('/Script/Engine.KismetTextLibrary', 'Conv_StringToText', InString=g.param('Message')))
    execute = g.exec_call(g.b.function_path('/Script/UMG.TextBlock', 'SetText'), execute, self=text, InText=message)
    node = g.call(g.b.function_path('/Script/UMG.Widget', 'SetVisibility'), self=text)
    g.b.literal(g.b.inp(node, 'InVisibility'), 'HitTestInvisible')
    g.b.connect(execute, g.b.inp(node, 'execute'))


def _wire_destruct(bp):
    b, lib, ge = support(), ue().BlueprintEditorLibrary, ue().BlueprintGraphEditor
    events = ge.get_graph_editor_by_name(bp, 'EventGraph')
    old = [node for node in events.list_all_nodes() if node.get_class().get_name() == 'K2Node_Event'
           and norm(lib.get_node_title(node)) in ('destruct', 'eventdestruct')]
    require(len(old) <= 1, 'Ambiguous existing Destruct override')
    event = old[0] if old else lib.add_event_override(bp, 'Destruct', ue().IntPoint(0, 0))
    require(event is not None, 'Cannot create widget Destruct override')
    begin = b.out(event, 'then')
    following = list(begin.list_connected_pins())
    require(len(following) <= 1, 'Unexpected Destruct execution fanout')
    node = events.add_call_function_node(b.function_path(bp.generated_class().get_path_name(), 'RuntimeShutdown'))
    require(node is not None, 'Compile RuntimeShutdown signature first')
    ue().BlueprintGraphPinLibrary.break_pin_links(begin)
    b.connect(begin, b.inp(node, 'execute'))
    for pin in following:
        b.connect(b.out(node, 'then'), pin)


def wire():
    idle()
    bp = owned()
    require(state(bp, 'Prepared') == state(bp, 'Callbacks') == VERSION, 'Prepare both stages and compile callbacks first')
    require(state(bp, 'Tree') == VERSION, 'Author/native-compile CampaignStatusText first')
    if state(bp, 'Wired') == VERSION:
        return receipt('wire-noop')
    # Compile/API preflight before the first body mutation.
    b = support()
    for name in list(FUNCTIONS) + [row[3] for row in DELEGATES]:
        b.function_path(bp.generated_class().get_path_name(), name)
    widget_tree = tool('UMGToolSet.UMGToolSet.GetWidgets',
                       widgetBlueprint={'refPath': MODULE + '.' + MODULE.rsplit('/', 1)[1]})
    status = [row for row in widget_tree['widgets'] if row.get('widgetName') == 'CampaignStatusText']
    require(len(status) == 1 and status[0].get('bIsVariable')
            and status[0]['widgetClassPath']['refPath'] == '/Script/UMG.TextBlock',
            'CampaignStatusText must be a TextBlock widget variable')
    # WidgetTree-generated properties are absent from NewVariables (and hence
    # ListMemberVariableNames). CDO lookup establishes compiled-field existence;
    # its value is legitimately None before a widget instance has a tree.
    ue().get_default_object(bp.generated_class()).get_editor_property('CampaignStatusText')
    for owner, name in ((NAV_CLASS, 'GetOrCreateForController'), (NAV_CLASS, 'ReplayNavigationMarkers'),
                        (SAVE_CLASS, 'GetOrCreateForController'), ('/Script/UMG.TextBlock', 'SetText'),
                        ('/Script/Engine.KismetTextLibrary', 'Conv_StringToText')):
        b.function_path(owner, name)
    require(stage_start(bp, 'Wired'), 'Unexpected wired state')
    for author in (_wire_initialize, _wire_unbind, _wire_remove_published, _wire_shutdown,
                   _wire_upsert, _wire_remove, _wire_view, _wire_results, _wire_destruct):
        author(bp)
    stamp(bp, 'Wired', VERSION)
    return receipt('wired', native_compile_required=True, delegates=len(DELEGATES),
                   tracked_mission_body_owned_by='Tools/quest_mission_presentation.py',
                   hud_host_attachment_done=False, marker_style_done=False)


def seed_marker_defaults():
    """Clone the existing authored CHALK target styles, not guessed vendor defaults.

    Quest/personal styles are independent fields for later approved art choices.
    Their exact identities remain independent even if they initially share art.
    """
    idle()
    bp = owned()
    require(state(bp, 'Wired') == VERSION, 'Wire/native-compile runtime adapter first')
    if state(bp, 'Style') == VERSION:
        return receipt('style-noop')
    example = load('/Game/AZ/Blueprints/Menu/HUD/Navigation/Examples/BP_AZ_NavigationTarget')
    source, dest = ue().get_default_object(example.generated_class()), ue().get_default_object(bp.generated_class())
    require(source is not None and dest is not None, 'Native compile target/module first')
    compass = source.get_editor_property('CompassInfo')
    world = source.get_editor_property('WorldInfo')
    require(stage_start(bp, 'Style'), 'Unexpected style state')
    for prefix in ('Quest', 'Personal'):
        dest.set_editor_property(prefix + 'CompassInfo', compass)
        dest.set_editor_property(prefix + 'WorldInfo', world)
    stamp(bp, 'Style', VERSION)
    return receipt('style-seeded', source=example.get_path_name(), save_required=True,
                   note='Parent may select distinct approved quest/personal icons; no vendor/source defaults changed')


def map_support():
    spec = importlib.util.spec_from_file_location('az_checkpoint_map_authoring_support', ROOT / 'Tools/quest_map_page_setup.py')
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def load_controls_tree():
    """Add a small separate header control group; do not rebuild existing Map page."""
    idle()
    p = map_support()
    bp = p.owned(MAP_PAGE)
    if state(bp, 'LoadTree') == VERSION:
        return receipt('load-tree-noop', page=MAP_PAGE)
    rows = p.rows(MAP_PAGE)
    original_count = len(rows)
    require('PageLayout' in rows and rows['PageLayout']['widgetClassPath']['refPath'] == '/Script/UMG.CanvasPanel',
            'Expected the existing Field Journal PageLayout CanvasPanel')
    names = {'CheckpointControls', 'LoadRequestButton', 'LoadConfirmBox', 'LoadConfirmButton',
             'LoadCancelButton', 'CampaignLoadStatus'}
    require(not (set(rows) & names), 'Existing load controls/partial authoring; inspect instead of replacing')
    require(stage_start(bp, 'LoadTree'), 'Unexpected load tree state')
    parent = rows['PageLayout']['widget']
    controls = p.add(MAP_PAGE, 'CheckpointControls', '/Script/UMG.VerticalBox', parent,
                     p.rect(-440, 22, 400, 108, anchor=(1, 0)), {'visibility': 'SelfHitTestInvisible'})
    p.button(MAP_PAGE, 'LoadRequestButton', controls, 'LOAD CHECKPOINT')
    confirmation = p.add(MAP_PAGE, 'LoadConfirmBox', '/Script/UMG.HorizontalBox', controls,
                         {'padding': p.margin(0, 4, 0, 0)}, {'visibility': 'Collapsed'}, True)
    p.button(MAP_PAGE, 'LoadConfirmButton', confirmation, 'CONFIRM LOAD', {'padding': p.margin(0, 0, 10, 0)})
    p.button(MAP_PAGE, 'LoadCancelButton', confirmation, 'CANCEL')
    p.text(MAP_PAGE, 'CampaignLoadStatus', controls, '', {'padding': p.margin(0, 5, 0, 0)}, True)
    # Use the established body font/style, preserving the existing map widgets.
    rows = p.rows(MAP_PAGE)
    for name in ('LoadRequestButtonLabel', 'LoadConfirmButtonLabel', 'LoadCancelButtonLabel', 'CampaignLoadStatus'):
        p.properties(rows[name]['widget'], {'font': p.font_value(p.BODY_FONT, 'Regular', 12),
                                            'colorAndOpacity': {'specifiedColor': p.color(p.COLORS['chalk'])}})
    stamp(bp, 'LoadTree', VERSION)
    return receipt('load-tree', page=MAP_PAGE, native_compile_required=True, preexisting_widget_count=original_count)


def prepare_load_controls():
    idle()
    p, lib, ge = map_support(), ue().BlueprintEditorLibrary, ue().BlueprintGraphEditor
    bp = p.owned(MAP_PAGE)
    if state(bp, 'LoadPrepared') == VERSION:
        return receipt('load-prepare-noop', page=MAP_PAGE)
    require(state(bp, 'LoadTree') == VERSION, 'Add load controls tree first')
    old = {str(name) for name in lib.list_member_variable_names(bp, False)}
    require(not (old & {'BoundLoadCoordinator', 'bLoadUiBound', 'bLoadConfirmArmed'}), 'Load field collision')
    require(all(lib.find_graph(bp, name) is None for name in LOAD_FUNCTIONS), 'Load function collision')
    require(stage_start(bp, 'LoadPrepared'), 'Unexpected load preparation state')
    pin_types = types()
    for name, kind in (('BoundLoadCoordinator', 'Save'), ('bLoadUiBound', 'bool'), ('bLoadConfirmArmed', 'bool')):
        require(lib.add_member_variable(bp, name, pin_types[kind]), 'Cannot add load field ' + name)
        lib.set_blueprint_variable_category(bp, name, 'Campaign|Load UI')
    for name, parameters in LOAD_FUNCTIONS.items():
        graph = ge.create_and_edit_function_graph(bp, name)
        graph.set_function_is_public()
        for parameter, kind in parameters:
            require(graph.add_graph_input_parameter(parameter, pin_types[kind]).is_valid(), 'Cannot add load parameter')
    stamp(bp, 'LoadPrepared', VERSION)
    return receipt('load-prepared', page=MAP_PAGE, native_compile_required=True,
                   next='Native compile MapPage, prepare_load_callback(), native compile, wire_load_controls()')


def prepare_load_callback():
    """Same exact native signature extraction as module callback preparation."""
    idle()
    bp, b, lib, ge, pl = (map_support().owned(MAP_PAGE), support(), ue().BlueprintEditorLibrary,
                           ue().BlueprintGraphEditor, ue().BlueprintGraphPinLibrary)
    require(state(bp, 'LoadPrepared') == VERSION, 'Prepare/native-compile load fields first')
    if state(bp, 'LoadCallback') == VERSION:
        return receipt('load-callback-noop', page=MAP_PAGE)
    require(all(lib.find_graph(bp, name) is None for name in ('CheckpointLoadCompleted', 'CheckpointSaveCompleted')),
            'Load/save callback collision')
    require(stage_start(bp, 'LoadCallback'), 'Unexpected load callback state')
    events = ge.get_graph_editor_by_name(bp, 'EventGraph')
    require(events is not None, 'Missing page EventGraph')
    getter = events.add_get_member_variable_node('BoundLoadCoordinator')
    require(getter is not None, 'Native compile load coordinator field first')
    target = b.out(getter, 'BoundLoadCoordinator')
    binding = action(events, 'Bind Event to OnLoadCompleted', 'K2Node_AddDelegate', [target], klass(SAVE_CLASS))
    if not b.inp(binding, 'self').list_connected_pins():
        b.connect(target, b.inp(binding, 'self'))
    event = action(events, 'Add Custom Event...', 'K2Node_CustomEvent', [b.inp(binding, 'Delegate')])
    parameters = [(str(pin.get_pin_name()), pl.get_pin_type(pin)) for pin in lib.list_output_pins(event)
                  if str(pin.get_pin_name()) not in ('then', 'OutputDelegate')]
    require([name for name, _ in parameters] == ['bSuccess', 'Message'], 'Unexpected native load callback signature')
    # Both native declarations use exactly FAZ_CampaignSaveResult.
    for handler in ('CheckpointLoadCompleted', 'CheckpointSaveCompleted'):
        graph = ge.create_and_edit_function_graph(bp, handler)
        graph.set_function_is_public()
        for name, pin_type in parameters:
            require(graph.add_graph_input_parameter(name, pin_type).is_valid(), 'Cannot copy native load signature')
    events.remove_nodes([event, binding, getter])
    stamp(bp, 'LoadCallback', VERSION)
    return receipt('load-callback-prepared', page=MAP_PAGE, native_compile_required=True)


def _load_status_literal(g, execute, message):
    node = g.call(g.b.function_path(g.bp.generated_class().get_path_name(), 'ShowCheckpointLoadStatus'))
    g.b.literal(g.b.inp(node, 'Message'), message)
    g.b.connect(execute, g.b.inp(node, 'execute'))
    return g.value(node, 'then')


def _load_visibility(g, execute, visible):
    node = g.call(g.b.function_path('/Script/UMG.Widget', 'SetVisibility'), self=g.get('LoadConfirmBox'))
    g.b.literal(g.b.inp(node, 'InVisibility'), 'SelfHitTestInvisible' if visible else 'Collapsed')
    g.b.connect(execute, g.b.inp(node, 'execute'))
    return g.value(node, 'then')


def _load_guard(g, execute):
    execute = g.branch(execute, g.get('bLoadUiBound'))[0]
    active = g.value(g.native('/Script/CommonUI.CommonActivatableWidget', 'IsActivated'))
    execute = g.branch(execute, active)[0]
    execute = g.branch(execute, g.valid(g.get('BoundLoadCoordinator')))[0]
    # Native LoadCampaign still performs authoritative world/player/transaction
    # validation. This is only a local UI guard and never bypasses native checks.
    busy = g.value(g.native(SAVE_CLASS, 'IsBusy', self=g.get('BoundLoadCoordinator')))
    return g.branch(execute, busy)[1]


def _load_initialize(bp):
    g = RuntimeGraph(bp, 'InitializeCheckpointControls')
    execute = g.call_local('ShutdownCheckpointControls', g.entry)
    execute, pc = g.local_owner(execute)
    node = g.native(SAVE_CLASS, 'GetOrCreateForController', Controller=pc)
    g.b.connect(execute, g.b.inp(node, 'execute'))
    execute = g.set('BoundLoadCoordinator', g.value(node, 'then'), value=g.value(node))
    execute, unavailable = g.branch(execute, g.valid(g.get('BoundLoadCoordinator')))
    unavailable = _load_status_literal(g, unavailable, 'Checkpoint loading is available in a local campaign.')
    disable = g.call(g.b.function_path('/Script/UMG.Widget', 'SetIsEnabled'), self=g.get('LoadRequestButton'))
    g.b.literal(g.b.inp(disable, 'bInIsEnabled'), 'false')
    g.b.connect(unavailable, g.b.inp(disable, 'execute'))
    for row in LOAD_DELEGATES:
        execute = g.branch(execute, g.valid(g.get(row[0])))[0]
        execute = g.delegate(execute, row, True)
    execute = g.set('bLoadUiBound', execute, default='true')
    execute = g.call_local('RefreshCheckpointAvailability', execute)
    busy = g.value(g.native(SAVE_CLASS, 'IsBusy', self=g.get('BoundLoadCoordinator')))
    yes, no = g.branch(execute, busy)
    _load_status_literal(g, yes, 'Loading checkpoint...')
    last = g.editor.add_get_member_variable_node('LastMessage', SAVE_CLASS)
    g.b.connect(g.get('BoundLoadCoordinator'), g.b.inp(last, 'self'))
    g.call_local('ShowCheckpointLoadStatus', no, Message=g.value(last, 'LastMessage'))


def _load_shutdown(bp):
    g = RuntimeGraph(bp, 'ShutdownCheckpointControls')
    execute = g.set('bLoadUiBound', g.entry, default='false')
    execute = g.set('bLoadConfirmArmed', execute, default='false')
    for row in LOAD_DELEGATES:
        yes, no = g.branch(execute, g.valid(g.get(row[0])))
        execute = g.join([g.delegate(yes, row, False), no])
    yes, no = g.branch(execute, g.valid(g.get('LoadConfirmBox')))
    execute = g.join([_load_visibility(g, yes, False), no])
    g.set('BoundLoadCoordinator', execute, default='None')
    # Closing a map never cancels or restarts an accepted native transaction.


def _load_availability(bp):
    g = RuntimeGraph(bp, 'RefreshCheckpointAvailability')
    execute = g.branch(g.entry, g.valid(g.get('BoundLoadCoordinator')))[0]
    has_save = g.value(g.native(SAVE_CLASS, 'HasCampaignSave', self=g.get('BoundLoadCoordinator')))
    busy = g.value(g.native(SAVE_CLASS, 'IsBusy', self=g.get('BoundLoadCoordinator')))
    not_busy = g.value(g.native('/Script/Engine.KismetMathLibrary', 'Not_PreBool', A=busy))
    ready = g.value(g.native('/Script/Engine.KismetMathLibrary', 'BooleanAND', A=has_save, B=not_busy))
    not_armed = g.value(g.native('/Script/Engine.KismetMathLibrary', 'Not_PreBool', A=g.get('bLoadConfirmArmed')))
    enabled = g.value(g.native('/Script/Engine.KismetMathLibrary', 'BooleanAND', A=ready, B=not_armed))
    execute = g.exec_call(g.b.function_path('/Script/UMG.Widget', 'SetIsEnabled'), execute,
                          self=g.get('LoadRequestButton'), bInIsEnabled=enabled)
    g.exec_call(g.b.function_path('/Script/UMG.Widget', 'SetIsEnabled'), execute,
                self=g.get('LoadConfirmButton'), bInIsEnabled=ready)


def _load_request(bp):
    g = RuntimeGraph(bp, 'RequestCheckpointLoadConfirmation')
    execute = _load_guard(g, g.entry)
    has_save = g.value(g.native(SAVE_CLASS, 'HasCampaignSave', self=g.get('BoundLoadCoordinator')))
    yes, no = g.branch(execute, has_save)
    _load_status_literal(g, no, 'No checkpoint is available.')
    yes = g.set('bLoadConfirmArmed', yes, default='true')
    yes = _load_visibility(g, yes, True)
    yes = _load_status_literal(g, yes, 'Load the last checkpoint? Current progress will be replaced.')
    g.call_local('RefreshCheckpointAvailability', yes)


def _load_cancel(bp):
    g = RuntimeGraph(bp, 'CancelCheckpointLoadConfirmation')
    execute = g.set('bLoadConfirmArmed', g.entry, default='false')
    execute = _load_visibility(g, execute, False)
    execute = _load_status_literal(g, execute, '')
    g.call_local('RefreshCheckpointAvailability', execute)


def _load_confirm(bp):
    g = RuntimeGraph(bp, 'ConfirmCheckpointLoad')
    execute = _load_guard(g, g.entry)
    execute = g.branch(execute, g.get('bLoadConfirmArmed'))[0]
    # Reset confirmation BEFORE calling into the transaction. Double clicks and
    # reentrant completion events cannot initiate a second load.
    execute = g.set('bLoadConfirmArmed', execute, default='false')
    execute = _load_visibility(g, execute, False)
    execute = _load_status_literal(g, execute, 'Loading checkpoint...')
    node = g.native(SAVE_CLASS, 'LoadCampaign', self=g.get('BoundLoadCoordinator'))
    g.b.connect(execute, g.b.inp(node, 'execute'))
    accepted, rejected = g.branch(g.value(node, 'then'), g.value(node))
    # true is ACCEPTED, never "loaded". The result delegate supplies completion.
    rejected = g.call_local('ShowCheckpointLoadStatus', rejected, Message=g.value(node, 'OutError'))
    g.call_local('RefreshCheckpointAvailability', g.join([accepted, rejected]))


def _load_completed(bp):
    g = RuntimeGraph(bp, 'CheckpointLoadCompleted')
    execute = g.branch(g.entry, g.get('bLoadUiBound'))[0]
    execute = g.set('bLoadConfirmArmed', execute, default='false')
    execute = _load_visibility(g, execute, False)
    execute = g.call_local('ShowCheckpointLoadStatus', execute, Message=g.param('Message'))
    g.call_local('RefreshCheckpointAvailability', execute)
    g = RuntimeGraph(bp, 'CheckpointSaveCompleted')
    execute = g.branch(g.entry, g.get('bLoadUiBound'))[0]
    # An important-quest autosave can create the first checkpoint while Map is
    # already open. Refresh availability without overwriting load confirmation.
    g.call_local('RefreshCheckpointAvailability', execute)
    g = RuntimeGraph(bp, 'ShowCheckpointLoadStatus')
    text = g.get('CampaignLoadStatus')
    execute = g.branch(g.entry, g.valid(text))[0]
    value = g.value(g.native('/Script/Engine.KismetTextLibrary', 'Conv_StringToText', InString=g.param('Message')))
    g.exec_call(g.b.function_path('/Script/UMG.TextBlock', 'SetText'), execute, self=text, InText=value)


def _splice_override(bp, event_name, function_name):
    """Prefix only this exact event, preserving its preexisting execution chain."""
    b, lib, ge = support(), ue().BlueprintEditorLibrary, ue().BlueprintGraphEditor
    editor = ge.get_graph_editor_by_name(bp, 'EventGraph')
    # FindEventNode resolves actual UFunction identity (unlike the displayed
    # CommonUI title, which deliberately omits BP_).
    event = editor.find_event_node(event_name)
    if event is None:
        event = lib.add_event_override(bp, event_name, ue().IntPoint(0, 0))
    require(event is not None, 'Missing override ' + event_name)
    begin = b.out(event, 'then')
    following = list(begin.list_connected_pins())
    require(len(following) <= 1, 'Unexpected event fanout ' + event_name)
    node = editor.add_call_function_node(b.function_path(bp.generated_class().get_path_name(), function_name))
    require(node is not None, 'Native compile callback first: ' + function_name)
    ue().BlueprintGraphPinLibrary.break_pin_links(begin)
    b.connect(begin, b.inp(node, 'execute'))
    for pin in following:
        b.connect(b.out(node, 'then'), pin)


def wire_load_controls():
    idle()
    bp = map_support().owned(MAP_PAGE)
    require(state(bp, 'LoadPrepared') == state(bp, 'LoadCallback') == VERSION, 'Prepare/native-compile load UI stages first')
    if state(bp, 'LoadWired') == VERSION:
        return receipt('load-wire-noop', page=MAP_PAGE)
    b = support()
    for name in list(LOAD_FUNCTIONS) + ['CheckpointLoadCompleted', 'CheckpointSaveCompleted']:
        b.function_path(bp.generated_class().get_path_name(), name)
    for owner, name in ((SAVE_CLASS, 'LoadCampaign'), (SAVE_CLASS, 'HasCampaignSave'), (SAVE_CLASS, 'IsBusy'),
                        ('/Script/CommonUI.CommonActivatableWidget', 'IsActivated'),
                        ('/Script/UMG.Widget', 'SetIsEnabled')):
        b.function_path(owner, name)
    require(stage_start(bp, 'LoadWired'), 'Unexpected load wiring state')
    for author in (_load_initialize, _load_shutdown, _load_availability, _load_request,
                   _load_cancel, _load_confirm, _load_completed):
        author(bp)
    _splice_override(bp, 'BP_OnActivated', 'InitializeCheckpointControls')
    _splice_override(bp, 'BP_OnDeactivated', 'ShutdownCheckpointControls')
    _splice_override(bp, 'Destruct', 'ShutdownCheckpointControls')
    stamp(bp, 'LoadWired', VERSION)
    return receipt('load-wired', page=MAP_PAGE, native_compile_required=True,
                   accepted_is_not_completed=True, confirmation='Separate Confirm/Cancel controls',
                   no_save_anywhere=True, no_pause_override=True)


def contract():
    """File-only contract. This function imports no Unreal modules and mutates nothing."""
    return {
        'module': MODULE, 'bridge': BRIDGE,
        'stages': ['tree + prepare', 'native compile', 'prepare_callbacks', 'native compile',
                   'wire', 'native compile', 'seed_marker_defaults', 'explicit save'],
        'native_delegates': [{'member': row[0], 'owner': row[1], 'dispatcher': row[2],
                             'handler': row[3], 'parameters': row[4]} for row in DELEGATES],
        'presentation': {'function': 'RefreshTrackedMissionVisuals()', 'component_member': 'BoundQuestMap',
                         'native_progress_getter': 'BoundQuestMap.GetQuestProgress()',
                         'owner': 'Tools/quest_mission_presentation.py', 'shared_root': 'QuestModuleRoot'},
        'hud_remaining': [
            'Execute Tools/quest_hud_host_setup.py staged tree/prepare/native compile/wire/native compile/save after renderer lifetime hardening.',
            'Add empty passive QuestHost SizeBox to existing GameHUD, preserving all existing children/layout.',
            'Run existing InitializeNavigationHost first; resolve its existing bridge from this HUD GetOwningPlayer.',
            'Create/reuse exactly one QuestModule per local controller via bridge.QuestPresentationRef; OwningPlayer is that explicit local PC.',
            'On reuse detach previous parent before RuntimeInitialize. Call RuntimeInitialize(Bridge) before QuestHost.SetContent(Module).',
            'Do not call RuntimeInitialize from Tick or a global widget scan. Module Destruct unbinds exact handlers.',
            'Rendering owner sets child OwningPlayer + QuestContext=self + ApplyQuestContext before initialization/hosting.',
        ],
        'feedback': [
            'CampaignStatusText shows exact OnSaveCompleted/OnLoadCompleted Message; no save operation is triggered here.',
            'LoadCampaign true means accepted only; completion delegate confirms success.',
            'Optional Map-page stages author a user-initiated load route; immediate rejection goes to CampaignLoadStatus.',
            'Manual checkpoint saving stays the authored E interaction; never add global Save Anywhere.',
            'No automatic status expiration or completion sounds authored. Root controls approved display styling.',
        ],
        'map_load_stages': ['load_controls_tree + prepare_load_controls', 'native compile MapPage',
                            'prepare_load_callback', 'native compile MapPage', 'wire_load_controls',
                            'native compile/save MapPage'],
        'map_load_policy': 'Separate confirmation button, armed flag consumed before request; never force unpause or cancel an accepted load on map closure.',
        'ownership': 'Only exact native anchor keys are forwarded/removed. Quest selection, personal waypoint, controller components and unrelated markers survive teardown.',
        'pending_validation': 'Authoring APIs statically inspected; execute only while editor Idle, native compile and read back, then user gameplay acceptance.',
    }


if __name__ == '__main__':
    print(json.dumps(contract(), indent=2))
