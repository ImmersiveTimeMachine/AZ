# @Description: Host exactly one local-controller quest adapter after the existing compass host.
"""Import-inert authoring recipe; no compile/save/Play or C++ changes.

tree(); prepare(); [native compile Bridge + GameHUD]; wire();
[native compile GameHUD, then explicit save Bridge/GameHUD].

Prerequisite: quest_runtime_bindings.wire plus quest_mission_presentation's
renderer and source-lifetime hardening must be compiled before active hosting.
The existing CompassBridge retains QuestPresentationRef across HUD recreation.
Old HUD cleanup is parent-identity guarded so it cannot tear down a widget
already moved to the replacement HUD. No first-player/global widget lookup.
"""
from __future__ import annotations

import importlib.util
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUT = ROOT / 'Saved/QuestMapImplementation/RuntimeBindings'
HUD = '/Game/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD'
MODULE = '/Game/AZ/Blueprints/Menu/HUD/Navigation/Quests/WBP_AZ_QuestModule'
BRIDGE = '/Game/AZ/Blueprints/Menu/HUD/Navigation/BPC_AZ_CompassBridge'
KEY = 'AZ.Quest.Host.'
VERSION = 'v1'


def ue():
    import unreal
    return unreal


def require(value, message):
    if not value:
        raise RuntimeError(message)


def support(filename, name):
    spec = importlib.util.spec_from_file_location(name, ROOT / 'Tools' / filename)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def runtime():
    return support('quest_runtime_bindings.py', 'az_quest_host_runtime')


def hud_support():
    return support('compass_hud_setup.py', 'az_quest_host_compass')


def state(bp, name):
    return ue().EditorAssetLibrary.get_metadata_tag(bp, KEY + name)


def stamp(bp, name, value):
    ue().EditorAssetLibrary.set_metadata_tag(bp, KEY + name, value)


def receipt(stage, **values):
    OUT.mkdir(parents=True, exist_ok=True)
    result = dict(stage=stage, hud=HUD, module=MODULE, bridge=BRIDGE,
                  runtime_verified=False, **values)
    (OUT / ('hud-' + stage + '.json')).write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result


def targets():
    r = runtime()
    bp, bridge, module = r.load(HUD), r.load(BRIDGE), r.owned()
    require(ue().EditorAssetLibrary.get_metadata_tag(bp, 'AZ.Compass.HostWired') == 'v1',
            'Existing compass HUD host must already be authored')
    require(ue().EditorAssetLibrary.get_metadata_tag(bridge, 'AZ.Compass.BridgeAuthoring') == 'compass_bridge_setup:v1',
            'Refusing unowned compass bridge')
    require(module.generated_class() is not None and bridge.generated_class() is not None, 'Native compile module/bridge first')
    return r, bp, bridge, module


def tree():
    r, bp, bridge, module = targets()
    r.idle()
    h = hud_support()
    current = h.rows(HUD)
    if state(bp, 'Tree') == VERSION:
        require('QuestHost' in current and current['QuestHost']['widgetClassPath']['refPath'] == '/Script/UMG.SizeBox',
                'Stamped QuestHost is missing or changed')
        return receipt('tree-noop')
    require(not state(bp, 'Tree') and 'QuestHost' not in current, 'Existing/partial QuestHost requires inspection')
    require('HUDRoot' in current and current['HUDRoot']['widgetClassPath']['refPath'] == '/Script/UMG.CanvasPanel',
            'Unexpected HUD root; do not rebuild the tree')
    dirty = {package.get_name() for package in ue().EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    require(HUD not in dirty, 'Save the known owned HUD changes before taking its pre-host backup')
    before = h.hud_snapshot()
    backup = OUT / ('HUDQuestBackup_' + datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f'))
    backup.mkdir(parents=True)
    shutil.copy2(ROOT / 'Content/AZ/Blueprints/Menu/HUD/WBP_AZ_GameHUD.uasset', backup / 'WBP_AZ_GameHUD.uasset.bak')
    stamp(bp, 'Tree', 'writing')
    h.add(HUD, 'QuestHost', '/Script/UMG.SizeBox', current['HUDRoot']['widget'], h.canvas_full(1), True)
    after = h.hud_snapshot()
    require(set(after) - set(before) == {'QuestHost'}, 'Unexpected HUD widget additions')
    changed = [name for name in before if after[name] != before[name]]
    require(not changed, 'Preexisting HUD widgets changed: ' + str(changed))
    stamp(bp, 'Tree', VERSION)
    return receipt('tree', preserved_widgets=len(before), added=['QuestHost'], backup=str(backup), native_compile_required=True)


def prepare():
    r, bp, bridge, module = targets()
    r.idle()
    lib, ge = ue().BlueprintEditorLibrary, ue().BlueprintGraphEditor
    if state(bp, 'Prepared') == VERSION:
        require(state(bridge, 'RetainedReference') == VERSION, 'Bridge retention field not stamped')
        return receipt('prepare-noop')
    require(state(bp, 'Tree') == VERSION, 'Add the empty QuestHost first')
    require(not state(bp, 'Prepared') and not state(bridge, 'RetainedReference'), 'Partial preparation; inspect before retry')
    require('QuestPresentationRef' not in {str(name) for name in lib.list_member_variable_names(bridge, False)},
            'Bridge field collision')
    require('QuestModuleRef' not in {str(name) for name in lib.list_member_variable_names(bp, False)}, 'HUD field collision')
    require(all(lib.find_graph(bp, name) is None for name in ('InitializeQuestHost', 'ShutdownQuestHost')), 'HUD function collision')
    module_type = lib.get_object_reference_type(module.generated_class())
    bridge_type = lib.get_object_reference_type(bridge.generated_class())
    stamp(bp, 'Prepared', 'writing')
    stamp(bridge, 'RetainedReference', 'writing')
    require(lib.add_member_variable(bridge, 'QuestPresentationRef', module_type), 'Cannot create controller-retained quest reference')
    lib.set_blueprint_variable_category(bridge, 'QuestPresentationRef', 'Navigation|Presentation')
    require(lib.add_member_variable(bp, 'QuestModuleRef', module_type), 'Cannot add HUD quest reference')
    for name in ('InitializeQuestHost', 'ShutdownQuestHost'):
        graph = ge.create_and_edit_function_graph(bp, name)
        graph.set_function_is_private()
        if name == 'InitializeQuestHost':
            require(graph.add_local_variable('ResolvedQuestBridge', bridge_type), 'Cannot add bridge local')
            require(graph.add_local_variable('ResolvedQuestModule', module_type), 'Cannot add module local')
    stamp(bridge, 'RetainedReference', VERSION)
    stamp(bp, 'Prepared', VERSION)
    return receipt('prepared', native_compile_required=[BRIDGE, HUD],
                   retention='One QuestPresentationRef on each existing local-controller bridge')


def _cast(g, cls):
    before = set(g.editor.list_all_nodes())
    value = ue().AZ_BlueprintNodeUtils.add_cast_node(HUD, g.name, cls, False, 0, 0)
    require(bool(value), 'Cannot author typed bridge cast')
    added = [node for node in g.editor.list_all_nodes() if node not in before]
    require(len(added) == 1, 'Unexpected cast node result')
    return added[0], g.value(added[0], 'As' + cls.get_name().removesuffix('_C'))


def _initialize(r, bp, bridge, module):
    g = r.RuntimeGraph(bp, 'InitializeQuestHost')
    execute = g.call_local('ShutdownQuestHost', g.entry)
    execute, pc = g.local_owner(execute)
    execute = g.branch(execute, g.valid(g.get('QuestHost')))[0]
    find = g.native('/Script/Engine.Actor', 'GetComponentByClass', self=pc)
    g.b.literal(g.b.inp(find, 'ComponentClass'), bridge.generated_class().get_path_name())
    node, cast_value = _cast(g, bridge.generated_class())
    g.b.connect(g.value(find), g.b.inp(node, 'Object'))
    g.b.connect(execute, g.b.inp(node, 'execute'))
    execute = g.set('ResolvedQuestBridge', g.value(node, 'then'), value=cast_value, local=True)
    bridge_value = g.get('ResolvedQuestBridge', local=True)
    retained = g.editor.add_get_member_variable_node('QuestPresentationRef', bridge.generated_class().get_path_name())
    g.b.connect(bridge_value, g.b.inp(retained, 'self'))
    old = g.value(retained, 'QuestPresentationRef')
    existing, missing = g.branch(execute, g.valid(old))
    # A retained widget belongs to the same local player for the controller's
    # lifetime. Never steal/shut down a different player's widget on corruption.
    owner = g.value(g.native('/Script/UMG.Widget', 'GetOwningPlayer', self=old))
    existing = g.branch(existing, g.same(owner, pc))[0]
    existing = g.set('ResolvedQuestModule', existing, value=old, local=True)
    create = r.action(g.editor, 'Create Widget', 'K2Node_CreateWidget')
    g.b.literal(g.b.inp(create, 'Class'), module.generated_class().get_path_name())
    g.b.connect(pc, g.b.inp(create, 'OwningPlayer'))
    g.b.connect(missing, g.b.inp(create, 'execute'))
    new = g.set('ResolvedQuestModule', g.value(create, 'then'), value=g.value(create), local=True)
    execute = g.join([existing, new])
    widget = g.get('ResolvedQuestModule', local=True)
    execute = g.branch(execute, g.valid(widget))[0]
    # Unbind BEFORE reparent. The previous host can still exist during teardown.
    execute = g.exec_call(g.b.function_path(module.generated_class().get_path_name(), 'RuntimeShutdown'), execute, self=widget)
    execute = g.exec_call(g.b.function_path('/Script/UMG.Widget', 'RemoveFromParent'), execute, self=widget)
    retain = g.editor.add_set_member_variable_node('QuestPresentationRef', bridge.generated_class().get_path_name())
    g.b.connect(bridge_value, g.b.inp(retain, 'self'))
    g.b.connect(widget, g.b.inp(retain, 'QuestPresentationRef'))
    g.b.connect(execute, g.b.inp(retain, 'execute'))
    execute = g.set('QuestModuleRef', g.value(retain, 'then'), value=widget)
    execute = g.exec_call(g.b.function_path(module.generated_class().get_path_name(), 'RuntimeInitialize'), execute,
                          self=widget, Bridge=bridge_value)
    g.exec_call(g.b.function_path('/Script/UMG.ContentWidget', 'SetContent'), execute,
                self=g.get('QuestHost'), Content=widget)


def _shutdown(r, bp, module):
    g = r.RuntimeGraph(bp, 'ShutdownQuestHost')
    widget = g.get('QuestModuleRef')
    execute = g.branch(g.entry, g.valid(widget))[0]
    execute = g.branch(execute, g.valid(g.get('QuestHost')))[0]
    parent = g.value(g.native('/Script/UMG.Widget', 'GetParent', self=widget))
    ours, moved = g.branch(execute, g.same(parent, g.get('QuestHost')))
    ours = g.exec_call(g.b.function_path(module.generated_class().get_path_name(), 'RuntimeShutdown'), ours, self=widget)
    g.set('QuestModuleRef', g.join([ours, moved]), default='None')
    # Bridge retains the module until controller teardown. HUD shutdown never
    # clears progress, waypoint, the compass host, or another host's presentation.


def _hook_after_compass(r, bp):
    b, lib, ge = r.support(), ue().BlueprintEditorLibrary, ue().BlueprintGraphEditor
    editor = ge.get_graph_editor_by_name(bp, 'EventGraph')
    nodes = [node for node in editor.list_all_nodes() if node.get_class().get_name() == 'K2Node_CallFunction'
             and r.norm(lib.get_node_title(node)) == 'initializenavigationhost']
    require(len(nodes) == 1, 'Expected one existing InitializeNavigationHost call; preserve and inspect otherwise')
    call = nodes[0]
    require(b.inp(call, 'execute').list_connected_pins(), 'Existing navigation initialization is disconnected')
    following = list(b.out(call, 'then').list_connected_pins())
    require(len(following) <= 1, 'Unexpected navigation continuation fanout')
    start = editor.add_call_function_node(b.function_path(bp.generated_class().get_path_name(), 'InitializeQuestHost'))
    require(start is not None, 'Compile host signature before wiring')
    ue().BlueprintGraphPinLibrary.break_pin_links(b.out(call, 'then'))
    b.connect(b.out(call, 'then'), b.inp(start, 'execute'))
    for pin in following:
        b.connect(b.out(start, 'then'), pin)
    # Preserve any other existing Destruct work, prefixing our guarded teardown.
    r._splice_override(bp, 'Destruct', 'ShutdownQuestHost')


def wire():
    r, bp, bridge, module = targets()
    r.idle()
    if state(bp, 'Wired') == VERSION:
        return receipt('wire-noop')
    require(state(bp, 'Prepared') == state(bridge, 'RetainedReference') == VERSION, 'Prepare/native-compile host + bridge first')
    require(not state(bp, 'Wired'), 'Partial host wiring requires inspection')
    require(r.state(module, 'Wired') == VERSION, 'Complete and compile the runtime adapter first')
    require(ue().EditorAssetLibrary.get_metadata_tag(module, 'AZ.Quest.SnapshotPresentation.Body.RefreshTrackedMissionVisuals') == 'v1',
            'Complete/native-compile tracked mission rendering before active HUD attachment')
    for name in ('WBP_AZ_MissionNotification', 'WBP_AZ_Mission'):
        leaf = r.load(MODULE.rsplit('/', 1)[0] + '/' + name)
        require(ue().EditorAssetLibrary.get_metadata_tag(leaf, 'AZ.Quest.SnapshotPresentation.Lifetime') == 'v1',
                'Apply/native-compile source Construct lifetime hardening first: ' + name)
    # Actual fields can have null CDO values. Test property existence, not value.
    ue().get_default_object(bp.generated_class()).get_editor_property('QuestHost')
    ue().get_default_object(bridge.generated_class()).get_editor_property('QuestPresentationRef')
    for path, name in ((bp.generated_class().get_path_name(), 'InitializeQuestHost'),
                       (bp.generated_class().get_path_name(), 'ShutdownQuestHost'),
                       (module.generated_class().get_path_name(), 'RuntimeInitialize'),
                       (module.generated_class().get_path_name(), 'RuntimeShutdown'),
                       ('/Script/UMG.Widget', 'GetParent')):
        r.support().function_path(path, name)
    stamp(bp, 'Wired', 'writing')
    _initialize(r, bp, bridge, module)
    _shutdown(r, bp, module)
    _hook_after_compass(r, bp)
    stamp(bp, 'Wired', VERSION)
    return receipt('wired', native_compile_required=[HUD], save_required=[BRIDGE, HUD],
                   order='Existing InitializeNavigationHost → InitializeQuestHost → original continuation',
                   owner='GetOwningPlayer local PC; existing CompassBridge retains one QuestModule',
                   old_host_teardown='Only acts while Module.GetParent == this QuestHost')


if __name__ == '__main__':
    print(json.dumps({'stages': ['tree', 'prepare', 'native compile Bridge + GameHUD', 'wire',
                                 'native compile GameHUD + save Bridge/GameHUD'],
                      'gameplay_verified': False, 'active_editor_calls': False}, indent=2))
