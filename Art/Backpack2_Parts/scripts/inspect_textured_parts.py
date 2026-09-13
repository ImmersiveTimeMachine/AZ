import bpy, json, math
from pathlib import Path
from mathutils import Vector, Matrix

root=Path('C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts')
src=bpy.data.objects['SKM_SurvivalMan_backpack2']
if src.mode == 'EDIT':
    src.update_from_editmode()
    bpy.ops.object.mode_set(mode='OBJECT')
top=json.loads((root/'inspection/source_topology.json').read_text())
groups=globals().get('diagnostic_groups') or json.loads((root/'inspection/semantic_component_candidates.json').read_text())['groups']
scene=bpy.data.scenes.new(globals().get('diagnostic_scene','BP2_Inspection'))
scene.render.engine='CYCLES'
scene.cycles.samples=16
scene.render.resolution_x=1650
scene.render.resolution_y=1450
scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('BP2_Inspection_World')
scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(0.13,0.16,0.19,1)
scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.65
scene.view_settings.view_transform='AgX'
materials=[]
for key in ['Backpack','Jacket']:
    mat=bpy.data.materials.new('BP2_Preview_'+key)
    mat.use_nodes=True
    bsdf=mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Roughness'].default_value=.78
    tex=mat.node_tree.nodes.new('ShaderNodeTexImage')
    tex.image=bpy.data.images.load(str(root/f'textures/T_SurvivalMan_{key}_BaseColor.png'),check_existing=True)
    tex.image.pack()
    mat.node_tree.links.new(tex.outputs['Color'],bsdf.inputs['Base Color'])
    materials.append(mat)
camera_data=bpy.data.cameras.new('BP2_Inspection_Camera')
camera=bpy.data.objects.new('BP2_Inspection_Camera',camera_data)
scene.collection.objects.link(camera)
camera.location=(0,12,0)
camera.rotation_euler=(Vector((0,0,0))-camera.location).to_track_quat('-Z','Y').to_euler()
camera_data.type='ORTHO'
camera_data.ortho_scale=8.4
scene.camera=camera
labelmat=bpy.data.materials.new('BP2_Preview_Labels')
labelmat.use_nodes=True
nodes=labelmat.node_tree.nodes
nodes.clear()
em=nodes.new('ShaderNodeEmission')
em.inputs['Color'].default_value=(.8,.85,.9,1)
out=nodes.new('ShaderNodeOutputMaterial')
labelmat.node_tree.links.new(em.outputs[0],out.inputs[0])
rot=Matrix.Rotation(math.radians(20),4,'Z')
for index,group in enumerate(groups):
    rot=Matrix.Rotation(math.radians(group.get('rotation_deg',20)),4,'Z')
    faces=sorted(f for c in group['component_ids'] for f in top['parts'][c]['faces'])
    vids=sorted(set(v for f in faces for v in src.data.polygons[f].vertices))
    remap={v:i for i,v in enumerate(vids)}
    points=[rot @ (src.matrix_world @ src.data.vertices[v].co) for v in vids]
    lo=Vector([min(p[a] for p in points) for a in range(3)])
    hi=Vector([max(p[a] for p in points) for a in range(3)])
    centre=(lo+hi)/2
    scale=1.75/max(hi.x-lo.x,hi.z-lo.z)
    col=index%3; row=index//3
    # Positive camera Y views negative X to the right on screen.
    target=Vector((2.65-col*2.65,0,2.4-row*2.4+.15))
    mesh=bpy.data.meshes.new('DIAG_'+group['name'])
    mesh.from_pydata([tuple((p-centre)*scale+target) for p in points],[],[[remap[v] for v in src.data.polygons[f].vertices] for f in faces])
    for mat in materials: mesh.materials.append(mat)
    uv=mesh.uv_layers.new(name='DiffuseUV')
    for poly,faceid in zip(mesh.polygons,faces):
        orig=src.data.polygons[faceid]
        poly.material_index=orig.material_index
        poly.use_smooth=orig.use_smooth
        for li,oldli in zip(poly.loop_indices,orig.loop_indices):
            uv.data[li].uv=src.data.uv_layers.active.data[oldli].uv
    obj=bpy.data.objects.new('DIAG_'+group['name'],mesh)
    scene.collection.objects.link(obj)
    textdata=bpy.data.curves.new('LABEL_'+group['name'],'FONT')
    textdata.body=f'{index+1:02d}  '+group['name'].replace('_',' ')
    textdata.align_x='CENTER'
    textdata.size=.12
    textdata.materials.append(labelmat)
    label=bpy.data.objects.new(textdata.name,textdata)
    scene.collection.objects.link(label)
    label.location=(target.x,.8,target.z-1.08)
    label.rotation_euler=camera.rotation_euler
for name,loc,power,size in [('Key',(-4,6,6),1800,5),('Fill',(5,4,1),1100,4)]:
    data=bpy.data.lights.new('BP2_DIAG_'+name,'AREA')
    data.energy=power;data.shape='DISK';data.size=size
    obj=bpy.data.objects.new(data.name,data)
    scene.collection.objects.link(obj)
    obj.location=loc
    obj.rotation_euler=(-obj.location).to_track_quat('-Z','Y').to_euler()
scene.render.image_settings.file_format='PNG'
scene.render.filepath=str(root/'inspection'/globals().get('diagnostic_image','textured_parts.png'))
bpy.ops.render.render(write_still=True,scene=scene.name)
print(json.dumps({'scene':scene.name,'render':scene.render.filepath,'parts':len(groups)}))
