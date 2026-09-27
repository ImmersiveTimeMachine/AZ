"""Place the selected interaction specimens beside the existing grenade row, without gameplay logic."""
from pathlib import Path
from datetime import datetime
import json
import shutil
import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
TAG = 'AZ_InteractionShowcase_v1'


def run():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    assert not editor.get_game_world(), 'Stop PIE before placement'
    world = editor.get_editor_world()
    assert world.get_path_name().startswith('/Game/AZ/Maps/L_001.'), 'Unexpected map'
    existing = actors.get_all_level_actors()
    owned = {a.get_actor_label(): a for a in existing if TAG in [str(t) for t in a.tags]}
    assert owned or world.get_outermost() not in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Map already dirty'
    def pose(actor):
        p=actor.get_actor_location();r=actor.get_actor_rotation();s=actor.get_actor_scale3d()
        return (p.x,p.y,p.z,r.pitch,r.yaw,r.roll,s.x,s.y,s.z)
    preserved = {a.get_name(): pose(a) for a in existing if a not in owned.values()}
    out = ROOT/'Saved/InteractionAudit'/('placement-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    out.mkdir(parents=True)
    shutil.copy2(ROOT/'Content/AZ/Maps/L_001.umap', out/'L_001-before.umap')
    (out/'existing-actors.json').write_text(json.dumps(preserved,indent=2),encoding='utf-8')
    rows = []

    def floor(x, y):
        hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x,y,1000), unreal.Vector(x,y,-1000),
            unreal.TraceTypeQuery.ECC_VISIBILITY, False, list(owned.values()), unreal.DrawDebugTrace.NONE)
        assert hit, 'No floor at '+str((x,y))
        location = hit.to_tuple()[5]
        assert -100 < location.z < 100, 'Obstacle or unsuitable height at '+str((x,y,location.z))
        return location.z

    # The user's corrected anchor is the first grenade at (-2857,3582,11).
    # Keep its central lane clear; examples sit in two parallel rows beside it.
    anchors = {'wood_door':(-2507,3582), 'gate':(-3307,3582), 'drawers':(-3307,3932),
               'locker':(-3307,4282), 'chest':(-3307,4632), 'shelf':(-2507,3932), 'desk':(-2507,4432)}
    floors = {n:floor(*xy) for n,xy in anchors.items()}
    assets = {
        'wood_door':'/Game/ClassicMansion/Meshes/SM_DoorA_L',
        'gate_frame':'/Game/Safe_House/meshes/SM_entrance_door_frame',
        'gate_left':'/Game/Safe_House/meshes/SM_entrance_door_left',
        'gate_right':'/Game/Safe_House/meshes/SM_entrance_door_right',
        'drawers':'/Game/ClassicMansion/Meshes/SM_DrawersA',
        'drawer_a':'/Game/ClassicMansion/Meshes/SM_DrawerA',
        'drawer_b':'/Game/ClassicMansion/Meshes/SM_DrawerB',
        'locker':'/Game/Safe_House/meshes/SM_locker_locker_main',
        'locker_door':'/Game/Safe_House/meshes/SM_locker_locker_door',
        'chest':'/Game/InfinityBladeFireLands/Environments/Forge/Env_Forge/StaticMesh/SM_Forge_Chest_Bottom',
        'chest_lid':'/Game/InfinityBladeFireLands/Environments/Forge/Env_Forge/StaticMesh/SM_Forge_Chest_Top',
        'shelf':'/Game/PostDistrict/Models/Structure/Furnitures/SM_Shelf_4X1',
        'desk':'/Game/ClassicMansion/Meshes/SM_DeskA',
        'key':'/Game/InventorySystemPro/ExampleContent/Common/Art/Key/SM_HotelKey',
        'book':'/Game/Safe_House/meshes/SM_book_01',
        'note':'/Game/Post_ap_city/Meshes/Post-apocalypse_vol2-square/props_shop/postal_1/SM_postal_letter_1',
        'picture':'/Game/Post_ap_city/Meshes/Post-apocalypse_vol2-square/props_shop/pharmacy_pops_1/SM_picture_1',
        'radio':'/Game/Safe_House/meshes/SM_rest_area_radio',
    }
    meshes = {n:unreal.load_asset(p) for n,p in assets.items()}
    assert all(isinstance(m,unreal.StaticMesh) for m in meshes.values()), 'Missing static mesh'

    def place(label, mesh, location, yaw=0, scale=1, roll=0, ground=None):
        name='AZ INTERACTION '+label
        actor=owned.get(name)
        if not actor:
            assert not any(a.get_actor_label()==name for a in existing), 'Unowned label conflict'
            actor=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*location))
            assert actor, 'Spawn failed: '+name
            actor.set_actor_label(name)
            actor.tags=[unreal.Name(TAG)]
            owned[name]=actor
        comp=actor.static_mesh_component
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        comp.set_static_mesh(meshes[mesh])
        comp.set_editor_property('can_ever_affect_navigation',False)
        comp.set_collision_profile_name('BlockAll')
        actor.set_actor_scale3d(unreal.Vector(scale,scale,scale))
        actor.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=roll),False)
        actor.set_actor_location(unreal.Vector(*location),False,True)
        if ground is not None:
            center,extent=actor.get_actor_bounds(False)
            pos=actor.get_actor_location()
            pos.z += ground-(center.z-extent.z)+.25
            actor.set_actor_location(pos,False,True)
        actor.set_folder_path('AZ_Interaction_Showcase')
        comp.set_mobility(unreal.ComponentMobility.STATIC)
        pos=actor.get_actor_location()
        rows.append({'label':name,'asset':assets[mesh],'location':[pos.x,pos.y,pos.z],'scale':scale,'yaw':yaw,'roll':roll})
        (out/'placed.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
        return actor

    def base(n):
        x,y=anchors[n]
        return (x,y,floors[n])

    place('01 Wood Door','wood_door',base('wood_door'),yaw=-90,ground=floors['wood_door'])
    gate=place('02 Gate Frame','gate_frame',base('gate'),yaw=180,scale=.6,ground=floors['gate'])
    p=gate.get_actor_location()
    for side in ['left','right']:
        place('02 Gate '+side.title(),'gate_'+side,(p.x,p.y,p.z),yaw=180,scale=.6)
    body=place('03 Drawers Body','drawers',base('drawers'),yaw=180,ground=floors['drawers'])
    p=body.get_actor_location()
    place('03 Drawer A','drawer_a',(p.x,p.y,p.z),yaw=180)
    place('03 Drawer B','drawer_b',(p.x,p.y,p.z),yaw=180)
    body=place('04 Locker Body','locker',base('locker'),yaw=180,ground=floors['locker'])
    p=body.get_actor_location()
    place('04 Locker Left Door','locker_door',(p.x-19,p.y,p.z-2.5),yaw=180)
    place('04 Locker Right Door','locker_door',(p.x-19,p.y,p.z-2.5),yaw=0)
    body=place('05 Chest Body','chest',base('chest'),yaw=180,ground=floors['chest'])
    p=body.get_actor_location()
    place('05 Chest Lid','chest_lid',(p.x,p.y,p.z+48),yaw=180)
    place('06 Shelf','shelf',base('shelf'),yaw=-90,scale=.8,ground=floors['shelf'])
    desk=place('07 Inspection Desk','desk',base('desk'),ground=floors['desk'])
    p=desk.get_actor_location();center,extent=desk.get_actor_bounds(False);top=center.z+extent.z
    place('08 Key','key',(p.x-65,p.y-15,top),scale=.075,ground=top)
    place('09 Book','book',(p.x-25,p.y-15,top),roll=90,ground=top)
    place('10 Note','note',(p.x+15,p.y-15,top),ground=top)
    place('11 Picture','picture',(p.x+55,p.y+10,top),scale=.2,ground=top)
    place('12 Radio','radio',(p.x-55,p.y+20,top),yaw=90,ground=top)
    assert all(pose(a)==preserved[a.get_name()] for a in existing if a.get_name() in preserved), 'An existing actor moved'
    assert unreal.EditorLoadingAndSavingUtils.save_packages([world.get_outermost()],False), 'Map save failed'
    (out/'complete.json').write_text(json.dumps({'actors':len(rows),'folder':'AZ_Interaction_Showcase',
        'anchor':[-2857,3582,11],'map_saved':True,'gameplay_connected':False},indent=2),encoding='utf-8')
    print('INTERACTION_SHOWCASE_SAVED actors='+str(len(rows))+' receipt='+str(out))


if __name__=='__main__':
    run()
