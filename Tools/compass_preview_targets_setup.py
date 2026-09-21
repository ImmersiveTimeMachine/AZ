# @Description: Author opt-in, removable compass demonstration targets for Artur's manual Play check.
"""Never starts PIE or executes a gameplay test.

The demo subclass alone auto-registers with existing local PlayerControllers at
BeginPlay. The production target remains explicit. PIE creates its initial local
player before World.BeginPlay (Engine/Private/GameInstance.cpp); the existing
bridge queues registration if the HUD is not ready. No timer or Tick scan.
This demonstration does not handle players joining after BeginPlay.
"""
import importlib.util
import json
import re
import shutil
from pathlib import Path
import unreal

_spec = importlib.util.spec_from_file_location('az_compass_preview_support', Path(__file__).with_name('compass_target_setup.py'))
T = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(T)
B, BL, GE = T.B, T.BL, T.GE
DEST = B.DEST + '/Examples/BP_AZ_NavigationPreviewTarget'
KEY = 'AZ.Compass.PreviewStage'


def prepare():
    B.idle()
    if unreal.EditorAssetLibrary.does_asset_exist(DEST):
        bp = unreal.load_asset(DEST)
        B.require(unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY) in ('prepared', 'wired'), 'Unowned or partial demo target')
        return 'existing owned demo target'
    parent = T.owned()
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', parent.generated_class())
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        DEST.rsplit('/', 1)[1], DEST.rsplit('/', 1)[0], unreal.Blueprint.static_class(), factory)
    B.require(bp is not None, 'Cannot create demo subclass')
    unreal.EditorAssetLibrary.set_metadata_tag(bp, KEY, 'prepared')
    return 'demo subclass prepared; native compile before wire'


def wire():
    B.idle()
    bp = unreal.load_asset(DEST)
    B.require(bp is not None, 'Prepare first')
    stage = unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY)
    if stage == 'wired':
        return 'already wired'
    B.require(stage == 'prepared', 'Unexpected authoring stage')
    editor = GE.get_graph_editor_by_name(bp, 'EventGraph')
    events = [n for n in editor.list_all_nodes() if n.get_class().get_name() == 'K2Node_Event'
              and 'beginplay' in str(BL.get_node_title(n)).lower().replace(' ', '')]
    # Child Blueprint creation already supplies BeginPlay -> Parent:BeginPlay.
    # add_event_override does not deduplicate that event; append to the existing
    # parent call rather than installing a second handler for the same event.
    B.require(len(events) <= 1, 'Duplicate BeginPlay events; inspect before wiring')
    event = events[0] if events else BL.add_event_override(bp, 'ReceiveBeginPlay', unreal.IntPoint(0, 0))
    B.require(event is not None, 'Missing BeginPlay')
    tail = B.out(event, 'then')
    previous = list(tail.list_connected_pins())
    if previous:
        B.require(len(previous) == 1 and previous[0].get_owning_node().get_class().get_name() == 'K2Node_CallParentFunction', 'Unexpected BeginPlay work')
        tail = B.out(previous[0].get_owning_node(), 'then')
    B.require(not tail.list_connected_pins(), 'BeginPlay already wired')
    find = editor.add_call_function_node('/Script/Engine.GameplayStatics:GetAllActorsOfClass')
    B.literal(B.inp(find, 'ActorClass'), unreal.PlayerController.static_class().get_path_name())
    macros = unreal.load_asset('/Engine/EditorBlueprintResources/StandardMacros')
    graphs = [g for g in BL.list_graphs(macros) if g.get_name() == 'ForEachLoop']
    B.require(len(graphs) == 1, 'Cannot resolve source ForEachLoop')
    loop = editor.add_macro_node(graphs[0].get_path_name())
    before = set(editor.list_all_nodes())
    guid = unreal.AZ_BlueprintNodeUtils.add_cast_node(DEST, 'EventGraph', unreal.PlayerController.static_class(), False, 900, 0)
    B.require(bool(guid), 'Cannot create typed controller cast')
    created = [n for n in editor.list_all_nodes() if n not in before]
    B.require(len(created) == 1, 'Unexpected cast result')
    cast = created[0]
    register = editor.add_call_function_node(T.owned().generated_class().get_path_name() + ':RegisterForPlayer')
    # RegisterForPlayer owns the valid/local-controller guard and remembers
    # bridges for inherited EndPlay cleanup. No lookup of PlayerController zero.
    B.connect(tail, B.inp(find, 'execute'))
    B.connect(B.out(find, 'then'), B.inp(loop, 'Exec'))
    B.connect(B.out(find, 'OutActors'), B.inp(loop, 'Array'))
    B.connect(B.out(loop, 'LoopBody'), B.inp(cast, 'execute'))
    B.connect(B.out(loop, 'Array Element'), B.inp(cast, 'Object'))
    B.connect(B.out(cast, 'then'), B.inp(register, 'execute'))
    B.connect(B.out(cast, 'AsPlayerController'), B.inp(register, 'Controller'))
    unreal.EditorAssetLibrary.set_metadata_tag(bp, KEY, 'wired')
    return 'BeginPlay registers demonstration target for existing local controllers; parent EndPlay cleanup inherited'


def enable_channels():
    B.idle()
    bp = unreal.load_asset(DEST)
    B.require(unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY) == 'wired', 'Wire before defaults')
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('bShowOnCompass', True)
    cdo.set_editor_property('bShowInWorld', True)
    tick = cdo.get_editor_property('primary_actor_tick')
    tick.set_editor_property('start_with_tick_enabled', False)
    cdo.set_editor_property('primary_actor_tick', tick)
    return 'Both channels enabled; start ticking disabled'


def place():
    """Place only the three user-authorized demonstration actors; save separately."""
    B.idle()
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    B.require(world.get_path_name() == '/Game/AZ/Maps/L_001.L_001', 'Expected authorized test level L_001')
    bp = unreal.load_asset(DEST)
    B.require(unreal.EditorAssetLibrary.get_metadata_tag(bp, KEY) == 'wired', 'Finish and compile demo Blueprint first')
    graph = list(unreal.AZ_BlueprintNodeUtils.list_function_nodes(DEST, 'EventGraph'))
    B.require(sum('Event | Event BeginPlay' in line for line in graph) == 1, 'Expected single compiled BeginPlay chain')
    backup = B.OUT / 'L_001.BeforeCompassPreview.umap.bak'
    if not backup.exists():
        B.require(not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Save pre-existing level changes before first placement backup')
        shutil.copy2(Path('C:/UnrealEngine/Games/AZ/Content/AZ/Maps/L_001.umap'), backup)
    cls = bp.generated_class()
    existing = {a.get_actor_label(): a for a in subsystem.get_all_level_actors() if a.get_class() == cls}
    # Relative to the verified PlayerStart (-158,-8), facing +X. Invisible
    # anchors have no collision. Display each marker 120cm above its top surface.
    points = [('A', 1642, -8), ('B', -158, 1992), ('C', -1758, -8)]
    rows = []
    def call(tool, args):
        r = unreal.ToolsetRegistry.execute_tool('editor_toolset.toolsets.object.ObjectTools', tool, json.dumps(args))
        B.require(r.is_complete and not r.error, 'Object property access failed: ' + str(r.error))
        return json.loads(r.value)['returnValue']
    for letter, x, y in points:
        label = 'CompassPreview_' + letter
        actor = existing.get(label)
        if actor is None:
            hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 1800), unreal.Vector(x, y, -3000),
                unreal.TraceTypeQuery.ECC_VISIBILITY, False, [], unreal.DrawDebugTrace.NONE, True)
            # FHitResult members are not exposed as Python properties in this
            # build. Its native export provides the verified impact position.
            hit_text = hit.export_text()
            B.require('bBlockingHit=True' in hit_text, 'No support surface for preview ' + letter)
            match = re.search(r'ImpactPoint=\(X=([^,]+),Y=([^,]+),Z=([^\)]+)\)', hit_text)
            B.require(match is not None, 'Cannot read preview support height')
            location = unreal.Vector(x, y, float(match.group(3)) + 120)
            actor = subsystem.spawn_actor_from_class(cls, location, unreal.Rotator(), transient=False)
            B.require(actor is not None, 'Cannot place preview ' + letter)
            actor.set_actor_label(label)
            actor.set_folder_path('Navigation/CompassPreview')
            actor.set_editor_property('tags', [unreal.Name('AZ.Compass.Preview')])
        actor.set_editor_property('bShowOnCompass', True)
        actor.set_editor_property('bShowInWorld', True)
        ref = {'refPath': actor.get_path_name()}
        values = json.loads(call('get_properties', {'instance': ref, 'properties': ['worldInfo']}))
        values['worldInfo']['marker Name'] = 'TEST ' + letter
        B.require(call('set_properties', {'instance': ref, 'values': json.dumps(values)}), 'Cannot label world marker')
        check = json.loads(call('get_properties', {'instance': ref, 'properties': ['worldInfo']}))
        B.require(check['worldInfo']['marker Name'] == 'TEST ' + letter, 'Marker label readback mismatch')
        loc = actor.get_actor_location()
        rows.append({'label': label, 'path': actor.get_path_name(), 'position': [loc.x, loc.y, loc.z],
                     'world_label': 'TEST ' + letter, 'compass': True, 'world': True})
    data = {'level': world.get_path_name(), 'blueprint': DEST, 'backup': str(backup),
            'actors': rows, 'graph': graph, 'user_play_verified': False}
    (B.OUT / 'preview-targets-placed.json').write_text(json.dumps(data, indent=2), encoding='utf-8')
    return data
