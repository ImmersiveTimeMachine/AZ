"""Author the accepted container specimens after the native motion extension is loaded.
No PIE, no source-asset edits. Original display actors are retained hidden.
"""
from pathlib import Path
from datetime import datetime
import shutil
import json
import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')


def run(allow_own_dirty=False):
    ed = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = ed.get_editor_world()
    assert not ed.get_game_world(), 'PIE must be stopped'
    assert world.get_path_name().startswith('/Game/AZ/Maps/L_001.'), 'Wrong map'
    assert allow_own_dirty or world.get_outermost() not in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Unsaved map edits'
    cdo = unreal.get_default_object(unreal.AZ_InteractiveDoor)
    cdo.get_editor_property('ContentPickupIds')  # full build/restart must have loaded the new fields
    by_label = {a.get_actor_label(): a for a in subsystem.get_all_level_actors()}
    out = ROOT/'Saved/InteractionAudit'/('containers-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    out.mkdir(parents=True)
    shutil.copy2(ROOT/'Content/AZ/Maps/L_001.umap', out/'L_001-before.umap')

    def guid(number):
        result = unreal.Guid()
        assert result.import_text('(A=1093309249,B=345459826,C=366035968,D='+str(number)+')')
        return result

    def hide(actor):
        actor.set_actor_hidden_in_game(True)
        actor.set_actor_enable_collision(False)
        actor.set_is_temporarily_hidden_in_editor(True)
        actor.static_mesh_component.set_visibility(False, True)

    def panel(name, original_name, body_name, number, axis, angle, offset, hinge):
        old = by_label['AZ INTERACTION '+original_name]
        obj = by_label.get('AZ INTERACTIVE '+name)
        if not obj:
            obj = subsystem.spawn_actor_from_class(unreal.AZ_InteractiveDoor, old.get_actor_location(), old.get_actor_rotation())
            assert obj
            obj.set_actor_label('AZ INTERACTIVE '+name)
        obj.set_actor_transform(old.get_actor_transform(), False, True)
        obj.set_editor_property('DoorId', guid(number))
        obj.set_editor_property('DisplayName', name)
        obj.set_editor_property('bInitiallyLocked', False)
        obj.set_editor_property('bInitiallyOpen', False)
        obj.set_editor_property('bPassageDoor', False)
        obj.set_editor_property('OpenAngle', angle)
        obj.set_editor_property('RotationAxis', unreal.Vector(*axis))
        obj.set_editor_property('OpenTranslation', unreal.Vector(*offset))
        obj.set_editor_property('ClosedPivotLocation', unreal.Vector(*hinge))
        obj.set_editor_property('InteractionRadius', 160.0)
        obj.set_editor_property('NoiseLoudness', .35)
        obj.set_editor_property('NoiseRange', 500.0)
        moving = obj.get_editor_property('DoorMesh')
        moving.set_static_mesh(old.static_mesh_component.static_mesh)
        moving.set_relative_location(unreal.Vector(*[-v for v in hinge]), False, True)
        if body_name:
            body = by_label['AZ INTERACTION '+body_name]
            frame = obj.get_editor_property('FrameMesh')
            frame.set_static_mesh(body.static_mesh_component.static_mesh)
            relative = unreal.MathLibrary.make_relative_transform(body.get_actor_transform(), obj.get_actor_transform())
            frame.set_relative_transform(relative, False, True)
            hide(body)
        obj.set_folder_path('AZ_Interaction_Showcase')
        obj.set_editor_property('tags', ['AZ_InteractiveContainers_v1'])
        obj.refresh_door_preview()
        hide(old)
        return obj

    drawer_a = panel('Drawer A', '03 Drawer A', '03 Drawers Body', 1943117840, (0,0,1), 0, (35,0,0), (0,0,0))
    drawer_b = panel('Drawer B', '03 Drawer B', None, 1943117841, (0,0,1), 0, (35,0,0), (0,0,0))
    locker_l = panel('Locker Left', '04 Locker Left Door', '04 Locker Body', 1943117842, (0,0,1), -100, (0,0,0), (0,-54,0))
    locker_r = panel('Locker Right', '04 Locker Right Door', None, 1943117843, (0,0,1), 100, (0,0,0), (0,-54,0))
    chest = panel('Chest', '05 Chest Lid', '05 Chest Body', 1943117844, (0,1,0), -100, (0,0,0), (-37,0,0))
    drawer_a.set_editor_property('AssemblyActors', [drawer_b])
    drawer_b.set_editor_property('AssemblyActors', [drawer_a])
    locker_l.set_editor_property('AssemblyActors', [locker_r])
    locker_r.set_editor_property('AssemblyActors', [locker_l])
    records = []

    def loot(container, name, pickup_name, location, number):
        bp = unreal.load_asset('/Game/AZ/Blueprints/Items/Throwables/BP_Pickup_'+pickup_name)
        assert bp
        actor = by_label.get('AZ CONTAINER LOOT '+name)
        if not actor:
            actor = subsystem.spawn_actor_from_class(bp.generated_class(), unreal.Vector(*location))
            assert actor
            actor.set_actor_label('AZ CONTAINER LOOT '+name)
        actor.set_actor_location(unreal.Vector(*location), False, True)
        item = actor.get_component_by_class(unreal.AZ_Inv_CommonUI_ItemComponent)
        assert item and item.get_owner() == actor and item.get_outer() == actor
        item.modify(); actor.modify()
        identity = guid(number)
        item.get_editor_property('CampaignPickupId').import_text(identity.export_text())
        assert item.get_editor_property('CampaignPickupId').to_string() == identity.to_string()
        container.set_editor_property('ContentPickupIds', [identity])
        actor.set_folder_path('AZ_Interaction_Showcase/Contents')
        actor.set_editor_property('tags', ['AZ_InteractiveContainerLoot_v1'])
        actor.set_actor_hidden_in_game(True)
        actor.set_actor_enable_collision(False)
        records.append({'container':container.get_path_name(),'pickup':actor.get_path_name(),'id':identity.to_string()})

    # Pickups are revealed in the open compartment; no random refill on opening.
    loot(drawer_a, 'Drawer A Fabric', 'Fabric', (-3347,3900,87), 1943117860)
    loot(drawer_b, 'Drawer B Stone', 'Stone', (-3347,3964,87), 1943117861)
    loot(locker_l, 'Locker Left Fuel', 'Fuel', (-3320,4309,95), 1943117862)
    loot(locker_r, 'Locker Right Bottle', 'Bottle', (-3320,4255,95), 1943117863)
    loot(chest, 'Chest Igniter', 'Igniter', (-3307,4632,49), 1943117864)
    assert unreal.EditorLoadingAndSavingUtils.save_packages([world.get_outermost()], False), 'Map save failed'
    (out/'complete.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
    print('CONTAINERS_SAVED panels=5 pickups=5 user_playtest_required=True')


run()
