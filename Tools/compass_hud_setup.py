# @Description: Host the owned CHALK navigation module inside the existing passive HUD.
"""Staged authoring. No PIE/tests or Python Blueprint compilation.

tree() builds the isolated module tree and adds only NavigationHost to GameHUD.
prepare_graph() adds a function signature; compile through native tools before
wire_graph(). Existing HUD presentation/event connections are preserved.
"""
import copy
import gc
import importlib.util
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'Saved/CompassIntegration'
spec = importlib.util.spec_from_file_location('az_compass_adapter', ROOT / 'Tools/compass_graph_adapter.py')
A = importlib.util.module_from_spec(spec)
spec.loader.exec_module(A)
spec = importlib.util.spec_from_file_location('az_compass_bridge', ROOT / 'Tools/compass_bridge_setup.py')
B = importlib.util.module_from_spec(spec)
spec.loader.exec_module(B)
BL, GE, PL = A.BL, A.GE, A.PL
HUD = '/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD'
MODULE = A.MODULE
BRIDGE = A.DEST + '/BPC_AZ_CompassBridge'
UMG = 'UMGToolSet.UMGToolSet.'
OBJ = 'editor_toolset.toolsets.object.ObjectTools.'


def ref(path):
    return {'refPath': path}


def object_path(path):
    return path + '.' + path.rsplit('/', 1)[1]


def call(name, **args):
    toolset, tool = name.rsplit('.', 1)
    result = unreal.ToolsetRegistry.execute_tool(toolset, tool, json.dumps(args))
    A.require(result.is_complete and not result.error, name + ': ' + str(result.error))
    return json.loads(result.value)['returnValue']


def properties(obj, changes=None):
    schema = json.loads(call(OBJ + 'list_properties', instance=obj))
    names = list(changes) if changes is not None else list(schema)
    A.require(not (set(names) - set(schema)), 'Unknown widget properties: ' + str(set(names) - set(schema)))
    if changes is not None:
        A.require(call(OBJ + 'set_properties', instance=obj, values=json.dumps(changes)), 'Property write failed')
    return json.loads(call(OBJ + 'get_properties', instance=obj, properties=names))


def rows(path):
    data = call(UMG + 'GetWidgets', widgetBlueprint=ref(object_path(path)))
    return {w['widgetName']: w for w in data['widgets'] if isinstance(w.get('widget'), dict)}


def canvas_full(z=0):
    return {'layoutData': {'offsets': {'left': 0, 'top': 0, 'right': 0, 'bottom': 0},
            'anchors': {'minimum': {'x': 0, 'y': 0}, 'maximum': {'x': 1, 'y': 1}},
            'alignment': {'x': 0, 'y': 0}}, 'bAutoSize': False, 'zOrder': z}


def add(path, name, cls, parent=None, slot=None, variable=False):
    existing = rows(path)
    if name in existing:
        return existing[name]
    args = {'widgetBlueprint': ref(object_path(path)), 'widgetClass': ref(cls), 'widgetDisplayName': name}
    if parent is not None:
        args['parentWidget'] = parent
    row = call(UMG + 'AddWidget', **args)
    A.require(row.get('widgetName') == name, 'Unexpected created widget name')
    properties(row['widget'], {'visibility': 'HitTestInvisible'})
    if slot is not None:
        properties(row['slot'], slot)
    call(UMG + 'ToggleWidgetAsVariable', widgetBlueprint=ref(object_path(path)),
         widget=row['widget'], bIsVariable=variable)
    return row


def hud_snapshot():
    return {name: {'row': row, 'properties': properties(row['widget']),
                   'slot': properties(row['slot']) if isinstance(row.get('slot'), dict) else None}
            for name, row in rows(HUD).items()}


def tree():
    A.idle()
    root = add(MODULE, 'NavigationRoot', '/Script/UMG.CanvasPanel')
    add(MODULE, 'WorldMarkersView', object_path(A.DEST + '/WBP_AZ_WorldMarkerContainer') + '_C',
        root['widget'], canvas_full(0), True)
    safe = add(MODULE, 'CompassSafeArea', '/Script/UMG.SafeZone', root['widget'], canvas_full(1))
    canvas = add(MODULE, 'CompassLayout', '/Script/UMG.CanvasPanel', safe['widget'])
    # Preserve source dimensions for the first functional parity check. The
    # approved narrower presentation is a separate verified layout/style step.
    compass_slot = {'layoutData': {'offsets': {'left': 0, 'top': 26, 'right': 896, 'bottom': 170},
                    'anchors': {'minimum': {'x': .5, 'y': 0}, 'maximum': {'x': .5, 'y': 0}},
                    'alignment': {'x': .5, 'y': 0}}, 'bAutoSize': False, 'zOrder': 1}
    add(MODULE, 'CompassView', object_path(A.DEST + '/WBP_AZ_Compass') + '_C',
        canvas['widget'], compass_slot, True)
    current = rows(HUD)
    if 'NavigationHost' not in current:
        dirty = {p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
        A.require(HUD not in dirty, 'Existing GameHUD has unsaved changes; preserve them before host insertion')
        before = hud_snapshot()
        A.write('hud-before-host', before)
        backup = OUT / ('HUDHostBackup_' + datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S'))
        backup.mkdir()
        shutil.copy2(ROOT / 'Content/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD.uasset', backup / 'WBP_AZ_GameHUD.uasset.bak')
        add(HUD, 'NavigationHost', '/Script/UMG.SizeBox', current['HUDRoot']['widget'], canvas_full(0), True)
        after = hud_snapshot()
        A.require(set(after) - set(before) == {'NavigationHost'}, 'Unexpected HUD tree additions')
        changed = [name for name in before if after[name] != before[name]]
        A.require(not changed, 'Existing HUD widgets changed: ' + str(changed))
        A.write('hud-host-added', {'preserved_widgets': len(before), 'added': ['NavigationHost'], 'backup': str(backup)})
    gc.collect()
    return {'module_widgets': list(rows(MODULE)), 'hud_widgets': len(rows(HUD)), 'next': 'Native compile module and HUD; prepare_graph().'}


def prepare_graph():
    A.idle()
    bp = A.load(HUD)
    editor = GE.get_graph_editor_by_name(bp, 'InitializeNavigationHost')
    if editor is None:
        editor = GE.create_and_edit_function_graph(bp, 'InitializeNavigationHost')
        editor.set_function_is_private()
        for name, path in [('ResolvedBridge', BRIDGE), ('ResolvedModule', MODULE)]:
            A.require(editor.add_local_variable(name, BL.get_object_reference_type(A.load(path).generated_class())),
                      'Could not create local navigation ref')
    return {'next': 'Native compile HUD, then wire_graph().'}


def cast(editor, blueprint, graph_name, cls):
    before = set(editor.list_all_nodes())
    guid = unreal.AZ_BlueprintNodeUtils.add_cast_node(blueprint, graph_name, cls, False, 0, 0)
    A.require(bool(guid), 'Cast creation failed')
    added = [n for n in editor.list_all_nodes() if n not in before]
    A.require(len(added) == 1, 'Unexpected cast creation result')
    node = added[0]
    name = 'As' + cls.get_name().removesuffix('_C')
    return node, B.out(node, name)


def wire_graph():
    A.idle()
    bp = A.load(HUD)
    if unreal.EditorAssetLibrary.get_metadata_tag(bp, 'AZ.Compass.HostWired') == 'v1':
        return {'state': 'already wired'}
    paths = {}
    for owner, names in {'KismetSystemLibrary': ['IsValid'], 'Controller': ['IsLocalController'],
                         'Actor': ['GetComponentByClass', 'AddComponentByClass']}.items():
        for name in names:
            paths[(owner, name)] = B.function_path('/Script/Engine.' + owner, name)
    paths[('Widget', 'GetOwningPlayer')] = B.function_path('/Script/UMG.Widget', 'GetOwningPlayer')
    graph = B.Graph(bp, 'InitializeNavigationHost', paths)
    pc = B.out(graph.native('Widget', 'GetOwningPlayer'))
    execute = graph.branch(graph.entry, graph.valid(pc))[0]
    execute = graph.branch(execute, B.out(graph.native('Controller', 'IsLocalController', self=pc)))[0]
    bridge_class = A.load(BRIDGE).generated_class()
    get = graph.native('Actor', 'GetComponentByClass', self=pc)
    B.literal(B.inp(get, 'ComponentClass'), bridge_class.get_path_name())
    exists, missing = graph.branch(execute, graph.valid(B.out(get)))
    old_cast, old_value = cast(graph.editor, HUD, 'InitializeNavigationHost', bridge_class)
    B.connect(exists, B.inp(old_cast, 'execute')); B.connect(B.out(get), B.inp(old_cast, 'Object'))
    old_done = graph.set('ResolvedBridge', B.out(old_cast, 'then'), value=old_value, local=True)
    create_bridge = graph.native('Actor', 'AddComponentByClass', self=pc)
    B.literal(B.inp(create_bridge, 'Class'), bridge_class.get_path_name())
    identity = graph.call('/Script/Engine.KismetMathLibrary:MakeTransform')
    B.connect(B.out(identity), B.inp(create_bridge, 'RelativeTransform'))
    # InternalUseParam can hide this pin from the public pin API. The native
    # call's default is false; no deferred exposed-property assignment is used.
    deferred = create_bridge.find_input_pin('bDeferredFinish')
    if deferred.is_valid():
        B.literal(deferred, 'false')
    B.connect(missing, B.inp(create_bridge, 'execute'))
    new_cast, new_value = cast(graph.editor, HUD, 'InitializeNavigationHost', bridge_class)
    B.connect(B.out(create_bridge, 'then'), B.inp(new_cast, 'execute'))
    B.connect(B.out(create_bridge), B.inp(new_cast, 'Object'))
    new_done = graph.set('ResolvedBridge', B.out(new_cast, 'then'), value=new_value, local=True)
    bridge = graph.get('ResolvedBridge', local=True)
    retained = graph.editor.add_get_member_variable_node('ModuleRef', bridge_class.get_path_name())
    B.connect(bridge, B.inp(retained, 'self'))
    has_module = graph.editor.add_branch_node()
    B.connect(old_done, B.inp(has_module, 'execute')); B.connect(new_done, B.inp(has_module, 'execute'))
    B.connect(graph.valid(B.out(retained, 'ModuleRef')), B.inp(has_module, 'Condition'))
    reuse = graph.set('ResolvedModule', B.out(has_module, 'then'), value=B.out(retained, 'ModuleRef'), local=True)
    actions = list(graph.editor.list_available_nodes([]))
    candidates = [a for a in actions if A.normalized(a.rsplit('|', 1)[-1]) == 'createwidget']
    A.require(len(candidates) == 1, 'Ambiguous CreateWidget action')
    create_module = graph.editor.create_node_from_name(candidates[0], unreal.Vector2D(1000, 0), [], None)
    B.literal(B.inp(create_module, 'Class'), A.load(MODULE).generated_class().get_path_name())
    B.connect(pc, B.inp(create_module, 'OwningPlayer'))
    B.connect(B.out(has_module, 'else'), B.inp(create_module, 'execute'))
    created = graph.set('ResolvedModule', B.out(create_module, 'then'), value=B.out(create_module), local=True)
    module = graph.get('ResolvedModule', local=True)
    remove = graph.call('/Script/UMG.Widget:RemoveFromParent', self=module)
    B.connect(reuse, B.inp(remove, 'execute')); B.connect(created, B.inp(remove, 'execute'))
    content = graph.call('/Script/UMG.ContentWidget:SetContent', self=graph.get('NavigationHost'), Content=module)
    B.connect(B.out(remove, 'then'), B.inp(content, 'execute'))
    child_refs = {}
    for name in ('CompassView', 'WorldMarkersView'):
        node = graph.editor.add_get_member_variable_node(name, A.load(MODULE).generated_class().get_path_name())
        B.connect(module, B.inp(node, 'self')); child_refs[name] = B.out(node, name)
    init = graph.call(A.load(MODULE).generated_class().get_path_name() + ':InitializeNavigation',
                      self=module, Compass=child_refs['CompassView'], World=child_refs['WorldMarkersView'], Bridge=bridge)
    B.connect(B.out(content, 'then'), B.inp(init, 'execute'))
    event_graph, editor = A.graph_nodes(bp, 'EventGraph')
    events = [n for n in editor.list_all_nodes() if n.get_class().get_name() == 'K2Node_Event'
              and A.normalized(A.title(n)) in ('construct', 'eventconstruct')]
    A.require(len(events) == 1, 'Existing HUD Construct event not unique')
    begin = B.out(events[0], 'then')
    A.require(not begin.list_connected_pins(), 'Existing HUD Construct has work; preserve its chain before attaching')
    call_init = editor.add_call_function_node('InitializeNavigationHost')
    B.connect(begin, B.inp(call_init, 'execute'))
    unreal.EditorAssetLibrary.set_metadata_tag(bp, 'AZ.Compass.HostWired', 'v1')
    A.write('hud-host-wired', {'runtime_verified': False, 'preserved_existing_construct_work': True})
    gc.collect()
    return {'state': 'wired; native compile/save required', 'runtime_verified': False}
