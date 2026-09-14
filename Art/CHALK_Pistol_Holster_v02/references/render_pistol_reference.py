import bpy,math
from pathlib import Path
from mathutils import Vector
out=Path(r'C:/UnrealEngine/Games/AZ/Art/CHALK_Pistol_Holster_v02/references')
scene=bpy.context.scene
scene.render.engine='CYCLES';scene.cycles.samples=12
scene.render.resolution_x=1100;scene.render.resolution_y=680;scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('PistolReferenceWorld');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.14,.18,.23,1)
scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.65
scene.view_settings.view_transform='AgX'
center=Vector((0,-.100911675,.04109438))
cam_data=bpy.data.cameras.new('ReferenceCamera');cam=bpy.data.objects.new('ReferenceCamera',cam_data);scene.collection.objects.link(cam);scene.camera=cam;cam_data.type='ORTHO';cam_data.ortho_scale=.36
for i,(loc,power,size) in enumerate([((.35,-.3,.5),35,.4),((-.35,.1,.4),25,.3),((.1,.35,.05),15,.3)]):
    d=bpy.data.lights.new('ReferenceLight'+str(i),'AREA');d.energy=power;d.shape='DISK';d.size=size;o=bpy.data.objects.new(d.name,d);scene.collection.objects.link(o);o.location=loc;o.rotation_euler=(center-o.location).to_track_quat('-Z','Y').to_euler()
for name,offset in [('side',(.8,0,0)),('reverse',(-.8,0,0)),('top',(.15,0,.8))]:
    cam.location=center+Vector(offset);cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler();scene.render.filepath=str(out/('pistol_'+name+'.png'));bpy.ops.render.render(write_still=True)
