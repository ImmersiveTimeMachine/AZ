"""Connect the accepted door and key specimens after a full build. Never starts PIE.

Run inside Unreal Editor. Keeps original specimens hidden for rollback, saves only
the owned key Blueprint and a clean showcase map, and records a disk backup first.
"""
from pathlib import Path
from datetime import datetime
import ast
import json
import shutil
import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
KEY_BP = '/Game/AZ/Blueprints/Items/Interactions/BP_Pickup_ShowcaseDoorKey'
TAG = 'Item.Type.Craftable.Key.ShowcaseDoor'


def run(allow_own_dirty=False):
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    assert not editor.get_game_world(), 'Stop PIE before authoring'
    world = editor.get_editor_world()
    assert world.get_path_name().startswith('/Game/AZ/Maps/L_001.'), 'Unexpected map'
    assert hasattr(unreal, 'AZ_InteractiveDoor'), 'New native door class is not loaded; full build/restart required'
    assert allow_own_dirty or world.get_outermost() not in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Map has unrelated unsaved changes'
    placed = actors.get_all_level_actors()
    by_label = {a.get_actor_label(): a for a in placed}
    old_door = by_label['AZ INTERACTION 01 Wood Door']
    old_key = by_label['AZ INTERACTION 08 Key']
    backup = ROOT / 'Saved/InteractionAudit' / ('door-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    backup.mkdir(parents=True)
    shutil.copy2(ROOT / 'Content/AZ/Maps/L_001.umap', backup / 'L_001-before.umap')

    # Reuse verified authoring functions, never execute the throwable author's main().
    source = ROOT / 'Tools/author_throwable_completion.py'
    syntax = ast.parse(source.read_text(encoding='utf-8-sig'))
    helpers = ast.Module(body=[n for n in syntax.body if isinstance(n, ast.FunctionDef)
                              and n.name in ('require', 'save', 'duplicate', 'component_objects', 'build_manifest')], type_ignores=[])
    import os
    ns = {'unreal': unreal, 'os': os}
    exec(compile(helpers, str(source), 'exec'), ns)
    bp = ns['duplicate']('/Game/AZ/Blueprints/Items/Throwables/BP_Pickup_Igniter', KEY_BP)
    manifest = ns['build_manifest']('Showcase Door Key', TAG, 'Consumable', 1, KEY_BP,
        '/Game/InventorySystemPro/ExampleContent/Common/Art/Key/T_HotelKeyIcon')
    assert unreal.AZ_Inv_AuthoringUtils.set_pickup_manifest_text(bp, manifest), 'Key manifest import failed'
    cdo = unreal.get_default_object(bp.generated_class())
    mesh = unreal.load_asset('/Game/InventorySystemPro/ExampleContent/Common/Art/Key/SM_HotelKey')
    assert mesh
    cdo.get_editor_property('mesh_component').set_static_mesh(mesh)
    cdo.get_editor_property('mesh_component').set_editor_property('relative_scale3d', unreal.Vector(.075, .075, .075))
    cdo.get_editor_property('skeletal_mesh_component').set_skeletal_mesh_asset(None)
    template = None
    for obj in ns['component_objects'](bp):
        if isinstance(obj, unreal.AZ_Inv_CommonUI_ItemComponent):
            obj.set_editor_property('PickupCaption', 'Showcase Door Key')
            template = obj
    assert template is not None
    ns['save'](bp)

    door = by_label.get('AZ INTERACTIVE Door')
    if not door:
        door = actors.spawn_actor_from_class(unreal.AZ_InteractiveDoor, old_door.get_actor_location(), old_door.get_actor_rotation())
        assert door
        door.set_actor_label('AZ INTERACTIVE Door')
    door.set_actor_transform(old_door.get_actor_transform(), False, True)
    door_id = unreal.Guid()
    door_id.import_text('(A=1093309249,B=345459826,C=366035968,D=1943117825)')
    door.set_editor_property('DoorId', door_id)
    door.set_editor_property('bInitiallyLocked', True)
    door.set_editor_property('bInitiallyOpen', False)
    key_tag = unreal.GameplayTag()
    key_tag.import_text('(TagName="' + TAG + '")')
    door.set_editor_property('RequiredKeyType', key_tag)
    door.set_editor_property('OpenSound', ns['require']('/Game/ClassicMansion/Audio/Cues/SC_DoorOpen'))
    door.set_editor_property('CloseSound', ns['require']('/Game/ClassicMansion/Audio/Cues/SC_DoorClose'))
    door.get_editor_property('DoorMesh').set_static_mesh(old_door.static_mesh_component.static_mesh)
    door.refresh_door_preview()

    key = by_label.get('AZ INTERACTIVE Door Key')
    if not key:
        key = actors.spawn_actor_from_class(bp.generated_class(), old_key.get_actor_location(), old_key.get_actor_rotation())
        assert key
        key.set_actor_label('AZ INTERACTIVE Door Key')
    # Mesh is scaled on its component; do not also copy the display actor's .075 scale.
    key.set_actor_location(old_key.get_actor_location(), False, True)
    key.set_actor_rotation(old_key.get_actor_rotation(), True)
    key.set_actor_scale3d(unreal.Vector(1, 1, 1))
    key.get_editor_property('mesh_component').set_static_mesh(mesh)
    key.get_editor_property('mesh_component').set_editor_property('relative_scale3d', unreal.Vector(.075, .075, .075))
    item = key.get_component_by_class(unreal.AZ_Inv_CommonUI_ItemComponent)
    assert item
    item.set_editor_property('PickupItemManifest', template.get_editor_property('PickupItemManifest'))
    item.set_editor_property('PickupCaption', 'Showcase Door Key')
    pickup_id = unreal.Guid()
    pickup_id.import_text('(A=1093309249,B=345459826,C=366035968,D=1943117826)')
    assert item.get_owner() == key and item.get_outer() == key
    item.modify(); key.modify()
    item.get_editor_property('CampaignPickupId').import_text(pickup_id.export_text())
    assert item.get_editor_property('CampaignPickupId').to_string() == pickup_id.to_string()
    for obj in (door, key):
        obj.set_folder_path('AZ_Interaction_Showcase')
        obj.set_editor_property('tags', ['AZ_InteractiveDoor_v1'])
    for obj in (old_door, old_key):
        obj.set_actor_hidden_in_game(True)
        obj.set_is_temporarily_hidden_in_editor(True)
        obj.set_actor_enable_collision(False)
        obj.static_mesh_component.set_visibility(False, True)
    assert unreal.EditorLoadingAndSavingUtils.save_packages([world.get_outermost()], False), 'Map save failed'
    receipt = {'door': door.get_path_name(), 'key': key.get_path_name(), 'key_blueprint': KEY_BP,
               'status': 'Authored and saved; user PIE verification still required'}
    (backup / 'complete.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
    print(json.dumps(receipt))


run()
