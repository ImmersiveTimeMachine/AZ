# @Description: Render native tracked-quest snapshots through owned ProHUD mission widgets.
"""File-authored recipe; importing performs no editor operations.

Root runs prepare_tree(), prepare(), native compile; harden_source_lifetime(),
then wire_function()/wire_refresh_phase() with native compile/save after each.
Bulk wire() is disabled after the crash. No PIE/tests, gameplay mutations, marker
changes or C++ edits. Runtime bindings own BoundQuestMap and invoke the public
RefreshTrackedMissionVisuals entry. Progress belongs exclusively to PlayerState.
"""
import importlib.util
import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path

_spec = importlib.util.spec_from_file_location('quest_presentation_context', Path(__file__).with_name('quest_prohud_setup.py'))
Q = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(Q)
MODULE = Q.MODULE
NOTIFICATION = Q.DEST + '/WBP_AZ_MissionNotification'
MISSION = Q.DEST + '/WBP_AZ_Mission'
TASK = Q.DEST + '/WBP_AZ_Task'
KEY = 'AZ.Quest.SnapshotPresentation'
TRACE_DIRECTORY = Q.ROOT / 'Saved/QuestMapImplementation/AuthoringTrace'
_TRACE = None
FUNCTIONS = {
    'RefreshTrackedMissionVisuals': [],
    'ClearTrackedMissionVisuals': [],
    'BuildQuestDisplayKey': [('Record', 'Record'), ('FocusedObjective', 'name')],
    'RenderQuestRecord': [('Record', 'Record'), ('Definition', 'Definition'), ('FocusedObjective', 'name')],
    'RenderQuestObjective': [('Objective', 'Objective'), ('ProgressRows', 'ProgressArray'),
                           ('FocusedObjective', 'name'), ('DefinitionIndex', 'int'), ('Mission', 'Mission')],
    'AppendMissionRow': [('Objective', 'Objective'), ('Progress', 'Progress'), ('FocusedObjective', 'name'),
                        ('DefinitionIndex', 'int'), ('Mission', 'Mission'), ('VisualState', 'VisualState')],
}


def receipt(name, data):
    return Q.write('presentation-' + name, data)


def _trace_event(phase, operation, arguments=(), error=None):
    if _TRACE is None:
        return
    _TRACE['sequence'] += 1
    frame = sys._getframe(1)
    while frame and (frame.f_code.co_name in ('_trace_call', 'invoke', '__getattr__') or frame.f_code.co_filename != __file__):
        frame = frame.f_back
    row = {'sequence': _TRACE['sequence'], 'utc': datetime.now(timezone.utc).isoformat(),
           'function': _TRACE['function'], 'phase': phase, 'operation': operation,
           'arguments': [value if isinstance(value, (str, int, float, bool, type(None))) else type(value).__name__ for value in arguments],
           'source_line': frame.f_lineno if frame else None}
    if error:
        row['error'] = error
    # Flush to disk before entering engine code so a process exit preserves the
    # last started operation. Never serialize live UObject/pin wrappers here.
    with _TRACE['path'].open('a', encoding='utf-8') as stream:
        stream.write(json.dumps(row) + '\n')
        stream.flush()
        os.fsync(stream.fileno())


def _trace_call(operation, callback, arguments=()):
    _trace_event('before', operation, arguments)
    try:
        result = callback()
    except Exception as exc:
        _trace_event('exception', operation, arguments, str(exc))
        raise
    _trace_event('after', operation, arguments)
    return result


class _EditorTrace:
    def __init__(self, editor):
        self.editor = editor

    def __getattr__(self, name):
        def invoke(*args, **kwargs):
            return _trace_call('BlueprintGraphEditor.' + name,
                               lambda: getattr(self.editor, name)(*args, **kwargs), args)
        return invoke


class _PinTrace:
    def __init__(self, library):
        self.library = library

    def __getattr__(self, name):
        actual = getattr(self.library, name)
        if name not in ('try_create_connection', 'set_pin_value', 'break_pin_links', 'break_single_pin_link'):
            return actual
        def invoke(*args, **kwargs):
            return _trace_call('BlueprintGraphPinLibrary.' + name, lambda: actual(*args, **kwargs), args)
        return invoke


class _GraphSupport:
    def __init__(self):
        original = Q.support()
        self.BL, self.GE, self.PL = original.BL, original.GE, _PinTrace(original.PL)

    def pin(self, node, name, output=False):
        pins = self.BL.list_output_pins(node) if output else self.BL.list_input_pins(node)
        for pin in pins:
            if str(self.PL.get_pin_name(pin)) == name:
                return pin
        raise RuntimeError('Missing pin ' + name + ' on ' + node.get_path_name())

    def connect(self, source, target):
        # Names are read before the native mutation; the trace captures the
        # connection endpoints without retaining raw native pin addresses.
        labels = (str(self.PL.get_pin_name(source)), str(self.PL.get_pin_name(target)))
        result = _trace_call('ConnectPins', lambda: self.PL.library.try_create_connection(source, target), labels)
        Q.require(result, 'Pin connection failed: ' + ' -> '.join(labels))


def require_compiled_view_field(bp):
    """WidgetTree variables are generated properties, not BP NewVariables entries."""
    rows = Q.widget_rows(MODULE)
    row = rows.get('MissionNotificationView')
    Q.require(row is not None and row['widgetClassPath']['refPath'].split('.')[0] == NOTIFICATION,
              'MissionNotificationView tree entry/type is missing')
    Q.require(row.get('bIsVariable') is True, 'MissionNotificationView must be a generated widget variable')
    cls = bp.generated_class()
    Q.require(cls is not None, 'Compile the module view tree first')
    # Success means the compiled property exists. Null is normal on a widget CDO.
    Q.ue().get_default_object(cls).get_editor_property('MissionNotificationView')
    return row


def reflect_types():
    U, A = Q.ue(), Q.support()
    result = {name: A.BL.get_basic_type_by_name(name) for name in ('name', 'string', 'int', 'bool')}
    for name, path in [('Record', '/Script/AZ.AZ_QuestProgressRecord'), ('Objective', '/Script/AZ.AZ_QuestObjectiveDefinition'),
                       ('Progress', '/Script/AZ.AZ_QuestObjectiveProgress')]:
        value = U.load_object(None, path)
        Q.require(value is not None, 'Native quest type is not loaded: ' + path)
        result[name] = A.BL.get_struct_type(value)
    result['RecordArray'] = A.BL.get_array_type(result['Record'])
    result['ProgressArray'] = A.BL.get_array_type(result['Progress'])
    result['Definition'] = A.BL.get_object_reference_type(U.AZ_QuestDefinition.static_class())
    result['Mission'] = A.BL.get_object_reference_type(Q.owned(MISSION).generated_class())
    result['Rows'] = A.BL.get_map_type(result['name'], A.BL.get_object_reference_type(Q.owned(TASK).generated_class()))
    _, editor = A.graph_nodes(Q.owned(TASK), 'EventGraph')
    events = [node for node in editor.list_all_nodes() if node.get_class().get_name() == 'K2Node_CustomEvent'
              and Q.normalized(A.title(node)) == 'updatetaskstate']
    Q.require(len(events) == 1, 'Cannot resolve source visual enum signature')
    result['VisualState'] = A.PL.get_pin_type(A.pin(events[0], 'TaskState', True))
    return result


def prepare_tree(left=48.0, top=170.0, width=340.0):
    """Use the shared canvas without changing campaign status or existing HUD widgets."""
    Q.idle()
    Q.owned(MODULE)
    rows = Q.widget_rows(MODULE)
    if 'QuestModuleRoot' not in rows:
        Q.require(not rows, 'Module has a different root; preserve it for review')
        Q.tool('UMGToolSet.UMGToolSet.AddWidget', widgetBlueprint={'refPath': Q.object_path(MODULE)},
               widgetClass={'refPath': '/Script/UMG.CanvasPanel'}, widgetDisplayName='QuestModuleRoot')
        rows = Q.widget_rows(MODULE)
    if 'MissionNotificationView' in rows:
        Q.require(rows['MissionNotificationView']['widgetClassPath']['refPath'].split('.')[0] == NOTIFICATION,
                  'Existing mission view has unexpected class')
        Q.require(rows['MissionNotificationView'].get('bIsVariable') is True, 'Existing mission view is not exposed as a widget variable')
        return {'state': 'mission view already exists; preserve placement'}
    row = Q.tool('UMGToolSet.UMGToolSet.AddWidget', widgetBlueprint={'refPath': Q.object_path(MODULE)},
                 widgetClass={'refPath': Q.owned(NOTIFICATION).generated_class().get_path_name()},
                 widgetDisplayName='MissionNotificationView', parentWidget=rows['QuestModuleRoot']['widget'])
    Q.require(row['widgetName'] == 'MissionNotificationView', 'Unexpected view name')
    Q.tool('UMGToolSet.UMGToolSet.ToggleWidgetAsVariable', widgetBlueprint={'refPath': Q.object_path(MODULE)},
           widget=row['widget'], bIsVariable=True)
    group = 'editor_toolset.toolsets.object.ObjectTools.'
    for obj, values in [(row['widget'], {'visibility': 'HitTestInvisible'}),
                        (row['slot'], {'layoutData': {'offsets': {'left': left, 'top': top, 'right': width, 'bottom': 1.0},
                                                      'anchors': {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 0, 'y': 0}},
                                                      'alignment': {'x': 0, 'y': 0}}, 'bAutoSize': True, 'zOrder': 1})]:
        schema = json.loads(Q.tool(group + 'list_properties', instance=obj))
        Q.require(set(values) <= set(schema), 'Unexpected UMG property schema')
        Q.require(Q.tool(group + 'set_properties', instance=obj, values=json.dumps(values)), 'Widget property assignment failed')
    return receipt('tree', {'widget': row, 'native_compile_required': True, 'placement': [left, top, width]})


def prepare():
    Q.idle()
    A, U = Q.support(), Q.ue()
    bp = Q.owned(MODULE)
    if U.EditorAssetLibrary.get_metadata_tag(bp, KEY + '.Signatures') == 'v1':
        return {'state': 'already prepared'}
    types = reflect_types()
    Q.require('BoundQuestMap' in {str(name) for name in A.BL.list_member_variable_names(bp, False)},
              'Runtime-binding agent must prepare BoundQuestMap first')
    current = {str(name) for name in A.BL.list_member_variable_names(bp, False)}
    for name, kind in [('LastQuestDisplayKey', 'string'), ('RenderedObjectiveRows', 'Rows')]:
        Q.require(name not in current, 'Unowned presentation member already exists: ' + name)
    for name in FUNCTIONS:
        existing = A.BL.find_graph(bp, name)
        if existing is not None:
            Q.require(name == 'RefreshTrackedMissionVisuals', 'Unowned presentation function: ' + name)
            editor = A.GE.get_graph_editor(existing)
            Q.require(not editor.find_graph_entry_pin().list_connected_pins(), 'Runtime callback already has a body')
    U.EditorAssetLibrary.set_metadata_tag(bp, KEY + '.Signatures', 'preparing')
    for name, kind in [('LastQuestDisplayKey', 'string'), ('RenderedObjectiveRows', 'Rows')]:
        Q.require(A.BL.add_member_variable(bp, name, types[kind]), 'Cannot add ' + name)
        A.BL.set_blueprint_variable_category(bp, name, 'Quest|Presentation Cache')
    for name, inputs in FUNCTIONS.items():
        editor = A.GE.get_graph_editor_by_name(bp, name)
        if editor is None:
            editor = A.GE.create_and_edit_function_graph(bp, name)
        editor.set_function_is_public() if name == 'RefreshTrackedMissionVisuals' else editor.set_function_is_private()
        for field, kind in inputs:
            Q.require(editor.add_graph_input_parameter(field, types[kind]).is_valid(), 'Cannot add input ' + field)
        if name == 'BuildQuestDisplayKey':
            Q.require(editor.add_graph_output_parameter('ReturnValue', types['string']) is not None, 'Cannot add display-key result')
        if name == 'RefreshTrackedMissionVisuals':
            for local, kind in [('RecordsSnapshot', 'RecordArray'), ('SelectedRecord', 'Record'), ('RecordFound', 'bool')]:
                Q.require(editor.add_local_variable(local, types[kind]), 'Cannot add ' + local)
        if name == 'BuildQuestDisplayKey':
            Q.require(editor.add_local_variable('DisplayKey', types['string']), 'Cannot add display-key accumulator')
        if name == 'RenderQuestRecord':
            Q.require(editor.add_local_variable('ProgressRowsSnapshot', types['ProgressArray']), 'Cannot add writable progress snapshot')
    U.EditorAssetLibrary.set_metadata_tag(bp, KEY + '.Signatures', 'v1')
    return receipt('signatures', {'functions': list(FUNCTIONS), 'native_compile_required': True})


def harden_source_lifetime():
    """Keep source appearance; presentation never owns quest success or row lifetime."""
    Q.idle()
    A, U = Q.support(), Q.ue()
    report = {}
    for package, first_action in ((NOTIFICATION, 'clearchildren'), (MISSION, 'setrenderopacity')):
        bp = Q.owned(package)
        key = KEY + '.Lifetime'
        stage = U.EditorAssetLibrary.get_metadata_tag(bp, key)
        if stage == 'v1':
            report[package] = 'already hardened'
            continue
        Q.require(not stage, 'Interrupted source lifetime adaptation')
        _, editor = A.graph_nodes(bp, 'EventGraph')
        nodes = list(editor.list_all_nodes())
        events = [node for node in nodes if node.get_class().get_name() == 'K2Node_Event'
                  and Q.normalized(A.title(node)) in ('construct', 'eventconstruct')]
        Q.require(len(events) == 1, 'Expected one source Construct')
        output = A.pin(events[0], 'then', True)
        following = list(output.list_connected_pins())
        Q.require(len(following) == 1 and Q.normalized(A.title(following[0].get_owning_node())) == first_action,
                  'Source destructive Construct changed; inspect it')
        tick = None
        if package == MISSION:
            ticks = [node for node in nodes if node.get_class().get_name() == 'K2Node_Event'
                     and Q.normalized(A.title(node)) == 'eventtick']
            Q.require(len(ticks) == 1, 'Mission retirement Tick is not unique')
            tick = A.pin(ticks[0], 'then', True)
            link = list(tick.list_connected_pins())
            Q.require(len(link) == 1 and Q.normalized(A.title(link[0].get_owning_node())) == 'delay', 'Unexpected mission Tick work')
            Q.require(float(A.PL.get_pin_value(A.pin(link[0].get_owning_node(), 'Duration'))) == 1.0,
                      'Mission Tick is not the audited retirement poll')
            # These two owned presentation helpers are called from QuestModule.
            # Do not depend on undocumented access flags of the vendor copy.
            for function in ('ClearTasks', 'AddTaskToVerticalBox'):
                _, function_editor = A.graph_nodes(bp, function)
                function_editor.set_function_is_public()
        U.EditorAssetLibrary.set_metadata_tag(bp, key, 'wiring')
        context = Q._quest_context(editor)
        valid = editor.add_call_function_node('/Script/Engine.KismetSystemLibrary:IsValid')
        A.connect(context, A.pin(valid, 'Object'))
        branch = editor.add_branch_node()
        A.connect(A.pin(valid, 'ReturnValue', True), A.pin(branch, 'Condition'))
        A.PL.break_pin_links(output)
        A.connect(output, A.pin(branch, 'execute'))
        A.connect(A.pin(branch, 'else', True), following[0])
        if tick is not None:
            A.PL.break_pin_links(tick)
        U.EditorAssetLibrary.set_metadata_tag(bp, key, 'v1')
        report[package] = 'context-managed Construct retained; gameplay-driven retirement only'
    return receipt('lifetime', {'widgets': report, 'native_compile_required': True})


class Graph:
    def __init__(self, name, resume=False):
        self.name = name
        self.A = _GraphSupport()
        self.bp = _trace_call('LoadOwnedModule', lambda: Q.owned(MODULE))
        raw_editor = _trace_call('GetGraphEditorByName', lambda: self.A.GE.get_graph_editor_by_name(self.bp, name), (name,))
        Q.require(raw_editor is not None, 'Missing prepared graph: ' + name)
        self.editor = _EditorTrace(raw_editor)
        self.entry_pin = self.editor.find_graph_entry_pin()
        Q.require(self.entry_pin.is_valid() and (resume or not self.entry_pin.list_connected_pins()), 'Graph body is not empty: ' + name)
        self.entry = self.entry_pin.get_owning_node()
        self._actions = None
        self.self_pin = None

    @property
    def actions(self):
        # Avoid a broad menu/template enumeration in guard-only phases.
        if self._actions is None:
            self._actions = [str(item) for item in self.editor.list_available_nodes([])]
        return self._actions

    def pin(self, node, name, output=False):
        return self.A.pin(node, name, output)

    def param(self, name):
        return self.pin(self.entry, name, True)

    def get(self, name, local=False, target=None, cls=None):
        node = self.editor.add_get_local_variable_node(name) if local else self.editor.add_get_member_variable_node(name, cls or '')
        Q.require(node is not None, 'Getter missing: ' + name)
        if target is not None:
            self.A.connect(target, self.pin(node, 'self'))
        return self.pin(node, name, True)

    def set(self, name, tail, value=None, literal=None, local=False, target=None, cls=None):
        node = self.editor.add_set_local_variable_node(name) if local else self.editor.add_set_member_variable_node(name, cls or '')
        Q.require(node is not None, 'Setter missing: ' + name)
        if target is not None:
            self.A.connect(target, self.pin(node, 'self'))
        if value is not None:
            self.A.connect(value, self.pin(node, name))
        elif literal is not None:
            Q.require(self.A.PL.set_pin_value(self.pin(node, name), str(literal)), 'Setter literal rejected: ' + name)
        self.A.connect(tail, self.pin(node, 'execute'))
        return self.pin(node, 'then', True)

    def call(this, path, **inputs):
        plain_math = {
            '/Script/Engine.KismetMathLibrary:EqualEqual_NameName',
            '/Script/Engine.KismetMathLibrary:EqualEqual_ObjectObject',
            '/Script/Engine.KismetMathLibrary:Greater_IntInt',
        }
        if path in plain_math:
            # The generic spawner may choose UK2Node_PromotableOperator and its
            # global operator/template machinery. A hang was isolated inside
            # that spawn call before comparison pins were connected. These
            # concrete signatures need no promotion; use the existing plain
            # call-node utility. Array/map/custom-thunk calls keep their normal
            # spawner path and are deliberately NOT routed through this helper.
            owner_path, function_name = path.rsplit(':', 1)
            target_class = _trace_call('LoadConcreteMathClass', lambda: Q.ue().load_class(None, owner_path), (owner_path,))
            Q.require(target_class is not None, 'Exact native math class is unavailable')
            helper = getattr(Q.ue(), 'AZ_BlueprintNodeUtils', None)
            Q.require(helper is not None and hasattr(helper, 'add_function_call_node'), 'Loaded plain call-node utility is unavailable')
            before = {item.get_path_name() for item in this.editor.list_all_nodes()}
            guid = _trace_call('AZ_BlueprintNodeUtils.AddFunctionCallNode',
                               lambda: helper.add_function_call_node(MODULE, this.name, target_class, function_name,
                                                                    240 * (len(before) % 8), 160 * (len(before) // 8)),
                               (this.name, owner_path, function_name))
            Q.require(bool(guid), 'Plain math node creation failed: ' + path)
            created = [item for item in this.editor.list_all_nodes() if item.get_path_name() not in before]
            Q.require(len(created) == 1 and created[0].get_class().get_name() == 'K2Node_CallFunction',
                      'Plain math bridge did not create exactly one ordinary call node')
            node = created[0]
            # Validate the concrete signature before touching the existing graph.
            for required_pin in ('A', 'B'):
                this.pin(node, required_pin)
            this.pin(node, 'ReturnValue', True)
        else:
            node = this.editor.add_call_function_node(path)
        Q.require(node is not None, 'Function unavailable: ' + path)
        for name, value in inputs.items():
            this.A.connect(value, this.pin(node, name))
        return node

    def execute(this, path, tail, **inputs):
        node = this.call(path, **inputs)
        this.A.connect(tail, this.pin(node, 'execute'))
        return node, this.pin(node, 'then', True)

    def branch(self, tail, condition):
        node = self.editor.add_branch_node()
        self.A.connect(tail, self.pin(node, 'execute'))
        self.A.connect(condition, self.pin(node, 'Condition'))
        return self.pin(node, 'then', True), self.pin(node, 'else', True)

    def valid(self, value):
        return self.pin(self.call('/Script/Engine.KismetSystemLibrary:IsValid', Object=value), 'ReturnValue', True)

    def action(self, caption, kind):
        matches = [item for item in self.actions if Q.normalized(item.rsplit('|', 1)[-1]) == Q.normalized(caption)]
        if kind == 'K2Node_Select':
            matches = [item for item in self.actions if item == 'Utilities|Select']
        Q.require(len(matches) == 1, 'Missing/ambiguous action: ' + caption + str(matches))
        node = self.editor.create_node_from_name(matches[0], Q.ue().Vector2D(0, 0), [], None)
        Q.require(node is not None and node.get_class().get_name() == kind, 'Wrong node type: ' + caption)
        return node

    def self_value(self):
        if self.self_pin is None:
            self.self_pin = self.pin(self.action('Get a reference to self', 'K2Node_Self'), 'self', True)
        return self.self_pin

    def break_struct(self, name, value):
        node = self.action('Break ' + name, 'K2Node_BreakStruct')
        self.A.connect(value, self.pin(node, name))
        return node

    def member(self, node, prefix, output=False):
        pins = self.A.BL.list_output_pins(node) if output else self.A.BL.list_input_pins(node)
        matches = [pin for pin in pins if str(self.A.PL.get_pin_name(pin)) == prefix or str(self.A.PL.get_pin_name(pin)).startswith(prefix + '_')]
        Q.require(len(matches) == 1, 'Missing/ambiguous field: ' + prefix)
        return matches[0]

    def foreach(self, tail, array, with_break=False):
        bp = _trace_call('LoadStandardMacros', lambda: Q.load('/Engine/EditorBlueprintResources/StandardMacros'))
        names = ('ForEachLoopWithBreak', 'ForEachLoopwithBreak') if with_break else ('ForEachLoop',)
        graphs = [graph for graph in self.A.BL.list_graphs(bp) if Q.normalized(graph.get_name()) in {Q.normalized(name) for name in names}]
        Q.require(len(graphs) == 1, 'Standard loop macro not unique')
        node = self.editor.add_macro_node(graphs[0].get_path_name())
        self.A.connect(array, self.pin(node, 'Array'))
        self.A.connect(tail, self.pin(node, 'Exec'))
        return node

    def switch(self, tail, enum_name, value):
        wanted = {Q.normalized('Switch on ' + enum_name), Q.normalized('Switch on ' + enum_name.removeprefix('E'))}
        matches = [item for item in self.actions if Q.normalized(item.rsplit('|', 1)[-1]) in wanted]
        Q.require(len(matches) == 1, 'Native enum action not unique: ' + enum_name + str(matches))
        node = self.editor.create_node_from_name(matches[0], Q.ue().Vector2D(0, 0), [], None)
        Q.require(node is not None and node.get_class().get_name() == 'K2Node_SwitchEnum', 'Wrong enum node')
        self.A.connect(value, self.pin(node, 'Selection'))
        self.A.connect(tail, self.pin(node, 'execute'))
        return node

    def case(self, node, value):
        pins = [pin for pin in self.A.BL.list_output_pins(node) if str(self.A.PL.get_pin_name(pin)).rsplit('::', 1)[-1] == value]
        Q.require(len(pins) == 1, 'Missing native enum case: ' + value)
        return pins[0]

    def select(self, condition, false_value, true_value):
        node = self.action('Select', 'K2Node_Select')
        self.A.connect(condition, self.pin(node, 'Index'))
        pins = [pin for pin in self.A.BL.list_input_pins(node) if str(self.A.PL.get_pin_name(pin)) != 'Index']
        Q.require(len(pins) == 2, 'Select option count changed')
        self.A.connect(false_value, pins[0])
        pins = [pin for pin in self.A.BL.list_input_pins(node) if str(self.A.PL.get_pin_name(pin)) != 'Index']
        self.A.connect(true_value, pins[1])
        return self.pin(node, 'ReturnValue', True)


def _class(package):
    return Q.owned(package).generated_class().get_path_name()


def _clear(graph, tail):
    return graph.execute('ClearTrackedMissionVisuals', tail)[1]


def wire_clear():
    g = Graph('ClearTrackedMissionVisuals')
    view = g.get('MissionNotificationView')
    yes, _ = g.branch(g.entry_pin, g.valid(view))
    mission = g.get('CurrentMission', target=view, cls=_class(NOTIFICATION))
    exists, absent = g.branch(yes, g.valid(mission))
    _, removed = g.execute('/Script/UMG.Widget:RemoveFromParent', exists, self=mission)
    clear_ref = g.editor.add_set_member_variable_node('CurrentMission', _class(NOTIFICATION))
    g.A.connect(view, g.pin(clear_ref, 'self'))
    g.A.connect(removed, g.pin(clear_ref, 'execute'))
    g.A.connect(absent, g.pin(clear_ref, 'execute'))
    # Default object pin is null; clear ownership before any future callbacks.
    panel = g.get('OV_Content', target=view, cls=_class(NOTIFICATION))
    _, tail = g.execute('/Script/UMG.PanelWidget:ClearChildren', g.pin(clear_ref, 'then', True), self=panel)
    _, tail = g.execute('/Script/Engine.BlueprintMapLibrary:Map_Clear', tail, TargetMap=g.get('RenderedObjectiveRows'))
    tail = g.set('LastQuestDisplayKey', tail, literal='')
    visibility, _ = g.execute('/Script/UMG.Widget:SetVisibility', tail, self=view)
    Q.require(g.A.PL.set_pin_value(g.pin(visibility, 'InVisibility'), 'Collapsed'), 'Cannot hide empty tracker')


def wire_key():
    g = Graph('BuildQuestDisplayKey')
    record = g.break_struct('AZ_QuestProgressRecord', g.param('Record'))
    quest = g.call('/Script/Engine.KismetStringLibrary:BuildString_Name', InName=g.member(record, 'QuestId', True))
    Q.require(g.A.PL.set_pin_value(g.pin(quest, 'Suffix'), '|'), 'Key suffix rejected')
    focus = g.call('/Script/Engine.KismetStringLibrary:BuildString_Name', AppendTo=g.pin(quest, 'ReturnValue', True), InName=g.param('FocusedObjective'))
    Q.require(g.A.PL.set_pin_value(g.pin(focus, 'Suffix'), '|'), 'Key focus suffix rejected')
    tail = g.set('DisplayKey', g.entry_pin, value=g.pin(focus, 'ReturnValue', True), local=True)
    loop = g.foreach(tail, g.member(record, 'Objectives', True))
    progress = g.break_struct('AZ_QuestObjectiveProgress', g.pin(loop, 'Array Element', True))
    switch = g.switch(g.pin(loop, 'LoopBody', True), 'EAZ_QuestObjectiveStatus', g.member(progress, 'Status', True))
    for status in ('Locked', 'Active', 'Completed', 'Failed', 'Cancelled'):
        name = g.call('/Script/Engine.KismetStringLibrary:BuildString_Name', AppendTo=g.get('DisplayKey', local=True),
                      InName=g.member(progress, 'ObjectiveId', True))
        Q.require(g.A.PL.set_pin_value(g.pin(name, 'Suffix'), ':'), 'Key separator rejected')
        count = g.call('/Script/Engine.KismetStringLibrary:BuildString_Int', AppendTo=g.pin(name, 'ReturnValue', True),
                       InInt=g.member(progress, 'CurrentCount', True))
        Q.require(g.A.PL.set_pin_value(g.pin(count, 'Suffix'), ':' + status + ';'), 'Key status rejected')
        g.set('DisplayKey', g.case(switch, status), value=g.pin(count, 'ReturnValue', True), local=True)
    results = [node for node in g.editor.list_all_nodes() if node.get_class().get_name() == 'K2Node_FunctionResult']
    Q.require(len(results) == 1, 'Display-key return is not unique')
    g.A.connect(g.pin(loop, 'Completed', True), g.pin(results[0], 'execute'))
    g.A.connect(g.get('DisplayKey', local=True), g.pin(results[0], 'ReturnValue'))


def _pin_locator(pin):
    A = Q.support()
    owner = pin.get_owning_node()
    name = str(A.PL.get_pin_name(pin))
    Q.require(name in {str(A.PL.get_pin_name(item)) for item in A.BL.list_output_pins(owner)},
              'Checkpoint expects an output pin')
    return {'node': owner.get_path_name(), 'pin': name, 'output': True}


def _restore_pin(g, locator):
    node = _trace_call('LoadCheckpointNode', lambda: Q.ue().load_object(None, locator['node']), (locator['node'],))
    Q.require(node is not None and node.get_outer() == g.editor.get_graph(), 'Checkpoint node missing or belongs to another graph')
    return g.pin(node, locator['pin'], locator['output'])


def _refresh_phase_one(g, state):
    view = g.get('MissionNotificationView')
    tail, _ = g.branch(g.entry_pin, g.valid(view))
    pc_node = g.call('/Script/UMG.Widget:GetOwningPlayer')
    pc = g.pin(pc_node, 'ReturnValue', True)
    tail, bad_pc = g.branch(tail, g.valid(pc))
    _clear(g, bad_pc)
    local = g.call('/Script/Engine.Controller:IsLocalController', self=pc)
    tail, nonlocal_pc = g.branch(tail, g.pin(local, 'ReturnValue', True))
    _clear(g, nonlocal_pc)
    navigation = g.get('BoundQuestMap')
    tail, no_navigation = g.branch(tail, g.valid(navigation))
    _clear(g, no_navigation)
    owner = g.call('/Script/Engine.ActorComponent:GetOwner', self=navigation)
    same = g.call('/Script/Engine.KismetMathLibrary:EqualEqual_ObjectObject', A=pc, B=g.pin(owner, 'ReturnValue', True))
    tail, wrong_owner = g.branch(tail, g.pin(same, 'ReturnValue', True))
    _clear(g, wrong_owner)
    progress_node = g.call('/Script/AZ.AZ_QuestMapComponent:GetQuestProgress', self=navigation)
    progress = g.pin(progress_node, 'ReturnValue', True)
    tail, no_progress = g.branch(tail, g.valid(progress))
    _clear(g, no_progress)
    tracked = g.call('/Script/AZ.AZ_QuestProgressComponent:GetTrackedQuestId', self=progress)
    tracked_id = g.pin(tracked, 'ReturnValue', True)
    empty = g.call('/Script/Engine.KismetMathLibrary:EqualEqual_NameName', A=tracked_id)
    no_tracking, tail = g.branch(tail, g.pin(empty, 'ReturnValue', True))
    _clear(g, no_tracking)
    return {'tail': _pin_locator(tail), 'progress': _pin_locator(progress), 'tracked_id': _pin_locator(tracked_id)}


def _refresh_phase_two(g, state):
    tail = _restore_pin(g, state['tail'])
    progress = _restore_pin(g, state['progress'])
    records = g.call('/Script/AZ.AZ_QuestProgressComponent:GetQuestRecords', self=progress)
    tail = g.set('RecordsSnapshot', tail, value=g.pin(records, 'ReturnValue', True), local=True)
    tail = g.set('RecordFound', tail, literal='false', local=True)
    return {**state, 'tail': _pin_locator(tail)}


def _refresh_phase_three(g, state):
    tail = _restore_pin(g, state['tail'])
    tracked_id = _restore_pin(g, state['tracked_id'])
    loop = g.foreach(tail, g.get('RecordsSnapshot', local=True), True)
    record = g.break_struct('AZ_QuestProgressRecord', g.pin(loop, 'Array Element', True))
    equal = g.call('/Script/Engine.KismetMathLibrary:EqualEqual_NameName', A=g.member(record, 'QuestId', True), B=tracked_id)
    found, _ = g.branch(g.pin(loop, 'LoopBody', True), g.pin(equal, 'ReturnValue', True))
    found = g.set('SelectedRecord', found, value=g.pin(loop, 'Array Element', True), local=True)
    found = g.set('RecordFound', found, literal='true', local=True)
    g.A.connect(found, g.pin(loop, 'Break'))
    found, missing = g.branch(g.pin(loop, 'Completed', True), g.get('RecordFound', local=True))
    _clear(g, missing)
    return {**state, 'tail': _pin_locator(found)}


def _refresh_phase_four(g, state):
    found = _restore_pin(g, state['tail'])
    progress = _restore_pin(g, state['progress'])
    tracked_id = _restore_pin(g, state['tracked_id'])
    chosen = g.break_struct('AZ_QuestProgressRecord', g.get('SelectedRecord', local=True))
    switch = g.switch(found, 'EAZ_QuestStatus', g.member(chosen, 'Status', True))
    for status in ('Available', 'Completed', 'Failed', 'Cancelled'):
        _clear(g, g.case(switch, status))
    definition_node = g.call('/Script/AZ.AZ_QuestProgressComponent:FindQuestDefinition', self=progress, QuestId=tracked_id)
    definition = g.pin(definition_node, 'ReturnValue', True)
    tail, missing_definition = g.branch(g.case(switch, 'Active'), g.valid(definition))
    _clear(g, missing_definition)
    focus = g.call('/Script/AZ.AZ_QuestProgressComponent:GetTrackedObjectiveId', self=progress)
    g.execute('RenderQuestRecord', tail, Record=g.get('SelectedRecord', local=True), Definition=definition,
              FocusedObjective=g.pin(focus, 'ReturnValue', True))
    return state


def wire_refresh():
    raise RuntimeError('Bulk refresh authoring is disabled after the crash. Use wire_refresh_phase(1..4), native compile/save between phases.')


def wire_record():
    g = Graph('RenderQuestRecord')
    key, tail = g.execute('BuildQuestDisplayKey', g.entry_pin, Record=g.param('Record'), FocusedObjective=g.param('FocusedObjective'))
    current_key = g.pin(key, 'ReturnValue', True)
    same = g.call('/Script/Engine.KismetStringLibrary:EqualEqual_StrStr', A=current_key, B=g.get('LastQuestDisplayKey'))
    _, tail = g.branch(tail, g.pin(same, 'ReturnValue', True))
    tail = _clear(g, tail)
    view = g.get('MissionNotificationView')
    tail = g.set('QuestContext', tail, value=g.self_value(), target=view, cls=_class(NOTIFICATION))
    owner = g.call('/Script/UMG.Widget:GetOwningPlayer')
    _, tail = g.execute('/Script/UMG.UserWidget:SetOwningPlayer', tail, self=view, LocalPlayerController=g.pin(owner, 'ReturnValue', True))
    _, tail = g.execute(_class(NOTIFICATION) + ':ApplyQuestContext', tail, self=view)
    noti = g.call('GetMissionNotiBaseInfo')
    tasks = g.call('GetMissionTasksBaseInfo')
    noti_value = g.pin(noti, 'S_MissionNotiBaseInfo', True)
    noti_fields = g.break_struct('S_MissionNotiBaseInfo_H', noti_value)
    title = g.get('Title', target=g.param('Definition'), cls='/Script/AZ.AZ_QuestDefinition')
    _, tail = g.execute(_class(NOTIFICATION) + ':CreateMission', tail, self=view, MissionName=title,
                        TextInfo=g.member(noti_fields, 'HeadlineTextInfo', True), NotificationBaseInfo=noti_value,
                        TasksBaseInfo=g.pin(tasks, 'S_TaskBaseInfo', True))
    # Empty vendor Tasks is intentional: owned retirement is disabled; rows are
    # built from stable objective IDs and are never addressed by vendor Map order.
    mission = g.get('CurrentMission', target=view, cls=_class(NOTIFICATION))
    tail, failed_create = g.branch(tail, g.valid(mission))
    _clear(g, failed_create)
    # Construct no longer owns clearing: remove the four designer preview rows
    # explicitly before appending the authoritative snapshot's display rows.
    _, tail = g.execute(_class(MISSION) + ':ClearTasks', tail, self=mission)
    record = g.break_struct('AZ_QuestProgressRecord', g.param('Record'))
    tail = g.set('ProgressRowsSnapshot', tail, value=g.member(record, 'Objectives', True), local=True)
    definition_rows = g.get('Objectives', target=g.param('Definition'), cls='/Script/AZ.AZ_QuestDefinition')
    loop = g.foreach(tail, definition_rows)
    g.execute('RenderQuestObjective', g.pin(loop, 'LoopBody', True), Objective=g.pin(loop, 'Array Element', True),
              ProgressRows=g.get('ProgressRowsSnapshot', local=True), FocusedObjective=g.param('FocusedObjective'),
              DefinitionIndex=g.pin(loop, 'Array Index', True), Mission=mission)
    tail = g.set('LastQuestDisplayKey', g.pin(loop, 'Completed', True), value=current_key)
    visibility, _ = g.execute('/Script/UMG.Widget:SetVisibility', tail, self=view)
    Q.require(g.A.PL.set_pin_value(g.pin(visibility, 'InVisibility'), 'HitTestInvisible'), 'Tracker visibility rejected')


def repair_progress_rows_reference():
    """Copy the read-only Record.Objectives into one writable local before the existing loop.

    No node deletion/rebuild and no function signature change. Handles the exact
    audited RenderQuestRecord→RenderQuestObjective connection only.
    """
    Q.idle()
    A, U = Q.support(), Q.ue()
    bp = Q.owned(MODULE)
    function = 'RenderQuestRecord'
    key = KEY + '.ProgressRowsReferenceRepair'
    Q.require(U.EditorAssetLibrary.get_metadata_tag(bp, KEY + '.Body.' + function) == 'v1', 'Expected the complete owned renderer body')
    _, editor = A.graph_nodes(bp, function)
    calls = [node for node in editor.list_all_nodes() if node.get_class().get_name() == 'K2Node_CallFunction'
             and Q.normalized(A.title(node)) == 'renderquestobjective']
    Q.require(len(calls) == 1, 'RenderQuestObjective call is not unique')
    call = calls[0]
    input_pin = A.pin(call, 'ProgressRows')
    links = list(input_pin.list_connected_pins())
    Q.require(len(links) == 1, 'Unexpected ProgressRows connection count')
    stage = U.EditorAssetLibrary.get_metadata_tag(bp, key)
    if stage == 'v1':
        Q.require(str(A.PL.get_pin_name(links[0])) == 'ProgressRowsSnapshot'
                  and links[0].get_owning_node().get_class().get_name() == 'K2Node_VariableGet', 'Repair metadata disagrees with graph')
        return {'state': 'writable snapshot already connected; unchanged'}
    Q.require(stage in ('', 'prepared'), 'Partial reference repair; inspect before retrying')
    source = links[0]
    source_node = source.get_owning_node()
    Q.require(source_node.get_class().get_name() == 'K2Node_BreakStruct' and str(A.PL.get_pin_name(source)) == 'Objectives',
              'ProgressRows is not the audited read-only native record field')
    record_input = A.pin(source_node, 'AZ_QuestProgressRecord')
    record_links = list(record_input.list_connected_pins())
    Q.require(len(record_links) == 1 and str(A.PL.get_pin_name(record_links[0])) == 'Record'
              and record_links[0].get_owning_node().get_class().get_name() == 'K2Node_FunctionEntry', 'Unexpected Record input')
    body_links = list(A.pin(call, 'execute').list_connected_pins())
    Q.require(len(body_links) == 1 and str(A.PL.get_pin_name(body_links[0])) == 'LoopBody', 'Row call is not in the expected loop')
    loop = body_links[0].get_owning_node()
    Q.require(loop.get_class().get_name() == 'K2Node_MacroInstance' and Q.normalized(A.title(loop)) == 'foreachloop',
              'Unexpected row loop')
    before_links = list(A.pin(loop, 'Exec').list_connected_pins())
    Q.require(len(before_links) == 1 and Q.normalized(A.title(before_links[0].get_owning_node())) == 'cleartasks',
              'Expected source ClearTasks immediately before row loop')
    source_locator, predecessor_locator = _pin_locator(source), _pin_locator(before_links[0])
    call_path, loop_path = call.get_path_name(), loop.get_path_name()
    local_name = 'ProgressRowsSnapshot'
    expected_type = reflect_types()['ProgressArray']
    current_type = editor.get_local_variable_type(local_name)
    if stage == '':
        Q.require(current_type is None, 'An unowned local uses the repair name')
        _start_trace('RenderQuestRecord-prepare-writable-snapshot')
        try:
            Q.require(_trace_call('AddLocalVariable.ProgressRowsSnapshot',
                                  lambda: editor.add_local_variable(local_name, expected_type)), 'Cannot add writable snapshot local')
            U.EditorAssetLibrary.set_metadata_tag(bp, key, 'prepared')
        except Exception:
            _end_trace(False)
            raise
        _end_trace(True)
    else:
        Q.require(current_type is not None, 'Prepared snapshot local is missing')
        Q.require(json.loads(A.BL.pin_type_to_json_schema(current_type, bp.generated_class())) ==
                  json.loads(A.BL.pin_type_to_json_schema(expected_type, bp.generated_class())), 'Snapshot local type differs')
    # Local declaration may regenerate the skeleton. Reacquire editor, nodes and
    # pins before linking; never retain FProperty/pin wrappers across that step.
    _start_trace('RenderQuestRecord-wire-writable-snapshot')
    try:
        g = Graph(function, resume=True)
        source = _restore_pin(g, source_locator)
        predecessor = _restore_pin(g, predecessor_locator)
        call = U.load_object(None, call_path)
        loop = U.load_object(None, loop_path)
        Q.require(call is not None and loop is not None, 'Existing renderer nodes are unavailable')
        U.EditorAssetLibrary.set_metadata_tag(bp, key, 'wiring')
        setter = g.editor.add_set_local_variable_node(local_name)
        getter = g.editor.add_get_local_variable_node(local_name)
        Q.require(setter is not None and getter is not None, 'Writable local nodes unavailable; inspect skeleton before retry')
        # Validate the full patch's pins before disturbing either original link.
        setter_value = g.pin(setter, local_name)
        getter_value = g.pin(getter, local_name, True)
        setter_exec, setter_then = g.pin(setter, 'execute'), g.pin(setter, 'then', True)
        target_array, loop_exec = g.pin(call, 'ProgressRows'), g.pin(loop, 'Exec')
        g.A.connect(source, setter_value)
        g.A.PL.break_single_pin_link(predecessor, loop_exec)
        g.A.connect(predecessor, setter_exec)
        g.A.connect(setter_then, loop_exec)
        g.A.PL.break_single_pin_link(source, target_array)
        g.A.connect(getter_value, target_array)
        U.EditorAssetLibrary.set_metadata_tag(bp, key, 'v1')
        result = {'function': function, 'local': local_name, 'original_read_only_source': source_locator,
                  'predecessor': predecessor_locator, 'call': call_path, 'loop': loop_path,
                  'new_nodes': [setter.get_path_name(), getter.get_path_name()], 'nodes_removed': 0,
                  'next': 'Return from Python; native compile module. Save only after this reference error is gone.'}
        receipt('progress-rows-reference-repair', result)
        del g
    except Exception:
        _end_trace(False)
        raise
    result['trace'] = _end_trace(True)
    return receipt('progress-rows-reference-repair', result)


def wire_objective():
    g = Graph('RenderQuestObjective')
    definition = g.break_struct('AZ_QuestObjectiveDefinition', g.param('Objective'))
    loop = g.foreach(g.entry_pin, g.param('ProgressRows'), True)
    progress = g.break_struct('AZ_QuestObjectiveProgress', g.pin(loop, 'Array Element', True))
    equal = g.call('/Script/Engine.KismetMathLibrary:EqualEqual_NameName',
                   A=g.member(definition, 'ObjectiveId', True), B=g.member(progress, 'ObjectiveId', True))
    found, _ = g.branch(g.pin(loop, 'LoopBody', True), g.pin(equal, 'ReturnValue', True))
    switch = g.switch(found, 'EAZ_QuestObjectiveStatus', g.member(progress, 'Status', True))
    for status, visual in (('Active', 'NewEnumerator0'), ('Completed', 'NewEnumerator2'), ('Failed', 'NewEnumerator3')):
        call, done = g.execute('AppendMissionRow', g.case(switch, status), Objective=g.param('Objective'),
                               Progress=g.pin(loop, 'Array Element', True), FocusedObjective=g.param('FocusedObjective'),
                               DefinitionIndex=g.param('DefinitionIndex'), Mission=g.param('Mission'))
        # Exact vendor names established by WB_Task.SwitchTaskState, not schema enum order.
        Q.require(g.A.PL.set_pin_value(g.pin(call, 'VisualState'), visual), 'Vendor state literal rejected')
        if status == 'Active':
            focused = g.call('/Script/Engine.KismetMathLibrary:EqualEqual_NameName',
                             A=g.member(definition, 'ObjectiveId', True), B=g.param('FocusedObjective'))
            select = g.action('Select', 'K2Node_Select')
            g.A.connect(g.pin(focused, 'ReturnValue', True), g.pin(select, 'Index'))
            # Specialize Select from the exact vendor enum pin, then assign its two cases.
            g.A.connect(g.pin(select, 'ReturnValue', True), g.pin(call, 'VisualState'))
            options = [pin for pin in g.A.BL.list_input_pins(select) if str(g.A.PL.get_pin_name(pin)) != 'Index']
            Q.require(len(options) == 2, 'Visual-state Select options changed')
            Q.require(g.A.PL.set_pin_value(options[0], 'NewEnumerator0') and g.A.PL.set_pin_value(options[1], 'NewEnumerator1'),
                      'Inactive/active vendor state assignment failed')
        g.A.connect(done, g.pin(loop, 'Break'))
    for status in ('Locked', 'Cancelled'):
        g.A.connect(g.case(switch, status), g.pin(loop, 'Break'))


def wire_row():
    g = Graph('AppendMissionRow')
    definition = g.break_struct('AZ_QuestObjectiveDefinition', g.param('Objective'))
    progress = g.break_struct('AZ_QuestObjectiveProgress', g.param('Progress'))
    description = g.member(definition, 'Description', True)
    formatting = g.action('Format Text', 'K2Node_FormatText')
    Q.require(g.A.PL.set_pin_value(g.pin(formatting, 'Format'), '{Description} ({Current}/{Required})'), 'Count format rejected')
    for name, value in [('Description', description), ('Current', g.member(progress, 'CurrentCount', True)),
                        ('Required', g.member(definition, 'RequiredCount', True))]:
        g.A.connect(value, g.pin(formatting, name))
    counted = g.call('/Script/Engine.KismetMathLibrary:Greater_IntInt', A=g.member(definition, 'RequiredCount', True))
    Q.require(g.A.PL.set_pin_value(g.pin(counted, 'B'), '1'), 'Count comparison literal rejected')
    label = g.select(g.pin(counted, 'ReturnValue', True), description, g.pin(formatting, 'Result', True))
    info = g.action('Make S_Task_H', 'K2Node_MakeStruct')
    g.A.connect(label, g.member(info, 'Description'))
    g.A.connect(description, g.member(info, 'ObjectName'))
    g.A.connect(g.member(definition, 'bOptional', True), g.member(info, 'IsOptionalTask'))
    focused = g.call('/Script/Engine.KismetMathLibrary:EqualEqual_NameName',
                     A=g.member(definition, 'ObjectiveId', True), B=g.param('FocusedObjective'))
    noti, tasks = g.call('GetMissionNotiBaseInfo'), g.call('GetMissionTasksBaseInfo')
    create, tail = g.execute(_class(MISSION) + ':AddTaskToVerticalBox', g.entry_pin, self=g.param('Mission'),
                            TaskInfo=g.pin(info, 'S_Task_H', True), NotiBaseInfo=g.pin(noti, 'S_MissionNotiBaseInfo', True),
                            TaskBaseInfo=g.pin(tasks, 'S_TaskBaseInfo', True), bUseHighlight=g.pin(focused, 'ReturnValue', True),
                            TaskItemIndex=g.param('DefinitionIndex'))
    Q.require(g.A.PL.set_pin_value(g.pin(create, 'bIsFirstTime'), 'false'), 'Row first-time flag rejected')
    row = g.pin(create, 'Task', True)
    tail, _ = g.branch(tail, g.valid(row))
    _, tail = g.execute(_class(TASK) + ':UpdateTaskState', tail, self=row, TaskState=g.param('VisualState'))
    g.execute('/Script/Engine.BlueprintMapLibrary:Map_Add', tail, TargetMap=g.get('RenderedObjectiveRows'),
              Key=g.member(definition, 'ObjectiveId', True), Value=row)


def _wire_preflight():
    Q.idle()
    A, U = Q.support(), Q.ue()
    bp = Q.owned(MODULE)
    Q.require(U.EditorAssetLibrary.get_metadata_tag(bp, KEY + '.Signatures') == 'v1', 'Prepare/native-compile presentation signatures first')
    Q.require(all(U.EditorAssetLibrary.get_metadata_tag(Q.owned(package), KEY + '.Lifetime') == 'v1'
                  for package in (MISSION, NOTIFICATION)), 'Harden source Construct/retirement before rendering')
    require_compiled_view_field(bp)
    # Force registration of struct actions before individual function action lists.
    for path in ('/Script/AZ.AZ_QuestProgressRecord', '/Script/AZ.AZ_QuestObjectiveDefinition', '/Script/AZ.AZ_QuestObjectiveProgress'):
        Q.require(U.load_object(None, path) is not None, 'Missing native struct action type')
    for name in ('S_Task_H', 'S_MissionNotiBaseInfo_H'):
        Q.load(Q.SOURCE + '/Blueprints/Structs/' + name)
    return bp


def _start_trace(function):
    global _TRACE
    Q.require(_TRACE is None, 'Authoring trace is already active')
    TRACE_DIRECTORY.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    _TRACE = {'function': function, 'sequence': 0, 'path': TRACE_DIRECTORY / (stamp + '-' + function + '.jsonl')}
    receipt('trace-latest', {'function': function, 'path': str(_TRACE['path']), 'pid': os.getpid(),
                            'state': 'started', 'native_compile_save_must_follow_after_python_returns': True})
    _trace_event('checkpoint', 'BeginAuthoring')


def _end_trace(success):
    global _TRACE
    if _TRACE is not None:
        _trace_event('checkpoint', 'AuthoringComplete' if success else 'AuthoringRaisedException')
        path = str(_TRACE['path'])
        _TRACE = None
        return path
    return None


def wire_function(name):
    """Author ONE body, then return for root-owned native compilation and save."""
    writers = {'ClearTrackedMissionVisuals': wire_clear, 'BuildQuestDisplayKey': wire_key,
               'RenderQuestRecord': wire_record, 'RenderQuestObjective': wire_objective, 'AppendMissionRow': wire_row}
    Q.require(name in writers, 'Use wire_refresh_phase(1..4) for refresh; unknown function cannot be authored')
    bp = _wire_preflight()
    key = KEY + '.Body.' + name
    stage = Q.ue().EditorAssetLibrary.get_metadata_tag(bp, key)
    if stage == 'v1':
        return {'state': 'already wired; inspect native asset, do not rely on stale disk receipts', 'function': name}
    Q.require(not stage, 'Partial function; inspect trace/asset before any retry: ' + name)
    _start_trace(name)
    try:
        _trace_call('SetAuthoringStage', lambda: Q.ue().EditorAssetLibrary.set_metadata_tag(bp, key, 'wiring'), (name,))
        writers[name]()
        _trace_call('SetAuthoringComplete', lambda: Q.ue().EditorAssetLibrary.set_metadata_tag(bp, key, 'v1'), (name,))
    except Exception:
        _end_trace(False)
        raise
    path = _end_trace(True)
    # Only Python wrapper collection here. Never native compile/save inside a
    # Python authoring frame. The caller performs both after this function exits.
    import gc
    gc.collect()
    return receipt('function-' + name, {'function': name, 'state': 'wired', 'trace': path,
                                       'next': 'Native compile module; inspect diagnostics; explicitly save only verified packages before next function.',
                                       'runtime_ready': False})


def wire_refresh_phase(phase):
    """Four durable checkpoints: guards; array locals; wildcard loop; result dispatch.

    Native compile/save is required between calls. Each resume reacquires pins
    from recorded node paths, so no pin/editor wrapper survives compilation.
    """
    Q.require(type(phase) is int and phase in (1, 2, 3, 4), 'Choose one refresh phase from 1 through 4')
    bp = _wire_preflight()
    U, A = Q.ue(), Q.support()
    phase_key = KEY + '.RefreshPhase'
    stage = U.EditorAssetLibrary.get_metadata_tag(bp, phase_key)
    body_stage = U.EditorAssetLibrary.get_metadata_tag(bp, KEY + '.Body.RefreshTrackedMissionVisuals')
    if body_stage == 'v1':
        return {'state': 'refresh already complete'}
    required = '' if phase == 1 else str(phase - 1)
    Q.require(stage == required, 'Refresh phase mismatch/partial operation: expected ' + required + ', got ' + stage)
    if phase == 1:
        Q.require(not body_stage, 'Refresh has an older partial body; inspect it before authoring')
        state = {}
    else:
        checkpoint = Q.OUT / ('presentation-refresh-phase-' + str(phase - 1) + '.json')
        Q.require(checkpoint.exists(), 'Missing prior refresh checkpoint receipt')
        recorded = json.loads(checkpoint.read_text(encoding='utf-8'))
        Q.require(recorded['module'] == MODULE and recorded['completed_phase'] == phase - 1, 'Checkpoint identity mismatch')
        _, editor = A.graph_nodes(bp, 'RefreshTrackedMissionVisuals')
        actual_paths = {node.get_path_name() for node in editor.list_all_nodes()}
        Q.require(actual_paths == set(recorded['node_paths']), 'Refresh graph changed since checkpoint; inspect instead of appending')
        state = recorded['pins']
    _start_trace('RefreshTrackedMissionVisuals-phase' + str(phase))
    try:
        _trace_call('SetRefreshPhaseInProgress', lambda: U.EditorAssetLibrary.set_metadata_tag(bp, phase_key, 'wiring-' + str(phase)))
        if phase == 1:
            U.EditorAssetLibrary.set_metadata_tag(bp, KEY + '.Body.RefreshTrackedMissionVisuals', 'phased-authoring')
        graph = Graph('RefreshTrackedMissionVisuals', resume=phase > 1)
        writers = (_refresh_phase_one, _refresh_phase_two, _refresh_phase_three, _refresh_phase_four)
        state = writers[phase - 1](graph, state)
        paths = [node.get_path_name() for node in graph.editor.list_all_nodes()]
        _trace_call('SetRefreshPhaseComplete', lambda: U.EditorAssetLibrary.set_metadata_tag(bp, phase_key, str(phase)))
        if phase == 4:
            U.EditorAssetLibrary.set_metadata_tag(bp, KEY + '.Body.RefreshTrackedMissionVisuals', 'v1')
        result = {'module': MODULE, 'completed_phase': phase, 'pins': state, 'node_paths': paths,
                  'partial_not_runtime_ready': phase != 4,
                  'next': 'Return from Python; native compile and explicitly save module before next phase.'}
        receipt('refresh-phase-' + str(phase), result)
        del graph
    except Exception:
        _end_trace(False)
        raise
    result['trace'] = _end_trace(True)
    import gc
    gc.collect()
    return receipt('refresh-phase-' + str(phase), result)


def inspect_presentation_state():
    """Read actual loaded graph state after restart; stale wired receipts are not evidence."""
    A, U = Q.support(), Q.ue()
    bp = Q.owned(MODULE)
    result = {'module': MODULE, 'pid': os.getpid(), 'disk_sha256': Q.digest_file(Q.package_file(MODULE)),
              'signature_stage': U.EditorAssetLibrary.get_metadata_tag(bp, KEY + '.Signatures'),
              'refresh_phase': U.EditorAssetLibrary.get_metadata_tag(bp, KEY + '.RefreshPhase'), 'functions': {}}
    for name in FUNCTIONS:
        graph = A.BL.find_graph(bp, name)
        if graph is None:
            result['functions'][name] = {'exists': False}
            continue
        editor = A.GE.get_graph_editor(graph)
        result['functions'][name] = {'exists': True, 'stage': U.EditorAssetLibrary.get_metadata_tag(bp, KEY + '.Body.' + name),
                                    'entry_linked': bool(editor.find_graph_entry_pin().list_connected_pins()),
                                    'nodes': [node.get_path_name() for node in editor.list_all_nodes()],
                                    'errors': [node.get_path_name() for node in editor.list_nodes_with_errors()]}
    return receipt('actual-state', result)


def wire():
    raise RuntimeError('Bulk authoring disabled after editor crash. Inspect loaded state; use wire_function(name) and wire_refresh_phase(1..4), native compile/save after each.')


def audit():
    """Read-only graph receipt after root's native compile; no gameplay tests."""
    A, U = Q.support(), Q.ue()
    module = Q.owned(MODULE)
    forbidden = {'removetask', 'switchtask', 'setactivetasks', 'findtaskbyint', 'getcurrenttasks', 'getcurrenttaskstates',
                 'acceptquest', 'failobjective', 'cancelquest', 'settrackedobjective', 'getplayercontroller', 'getallwidgetsofclass'}
    findings, graphs = [], {}
    for function in FUNCTIONS:
        Q.require(U.EditorAssetLibrary.get_metadata_tag(module, KEY + '.Body.' + function) == 'v1', 'Incomplete presentation function: ' + function)
        _, editor = A.graph_nodes(module, function)
        nodes = list(editor.list_all_nodes())
        Q.require(editor.find_graph_entry_pin().list_connected_pins(), 'Presentation entry has no work: ' + function)
        errors = [node.get_path_name() for node in editor.list_nodes_with_errors()]
        for node in nodes:
            if node.get_class().get_name() == 'K2Node_CallFunction' and Q.normalized(A.title(node)) in forbidden:
                findings.append({'function': function, 'node': node.get_path_name(), 'forbidden_call': A.title(node)})
        graphs[function] = {'nodes': len(nodes), 'errors': errors,
                            'body': list(U.AZ_BlueprintNodeUtils.list_function_nodes(MODULE, function))}
    for package in (MISSION, NOTIFICATION):
        Q.require(U.EditorAssetLibrary.get_metadata_tag(Q.owned(package), KEY + '.Lifetime') == 'v1', 'Missing source lifetime hardening')
    result = receipt('readback', {'graphs': graphs, 'findings': findings, 'runtime_tested': False,
                                  'contract': 'Read-only native progress snapshots; ObjectId→row map; source progress mutation APIs bypassed.'})
    Q.require(not findings and not any(value['errors'] for value in graphs.values()), 'Presentation graph findings; inspect readback')
    return result


if __name__ == '__main__':
    print(json.dumps({'module': MODULE, 'functions': list(FUNCTIONS),
                      'stages': ['inspect_presentation_state', 'wire_refresh_phase(1)', 'native compile/save',
                                 'wire_refresh_phase(2)', 'native compile/save', 'wire_refresh_phase(3)', 'native compile/save',
                                 'wire_refresh_phase(4)', 'native compile/save', 'wire_function(RenderQuestRecord)',
                                 'wire_function(RenderQuestObjective)', 'wire_function(AppendMissionRow)', 'audit'],
                      'editor_operations_executed': False}, indent=2))
