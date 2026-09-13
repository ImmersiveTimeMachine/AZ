"""Add packed base-color previews and a separate, labeled review scene."""
import bpy, json, math, hashlib
from pathlib import Path
from mathutils import Vector, Matrix

root=Path('C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts')
output=root/'Backpack2_Separated.blend'
assert Path(bpy.data.filepath).resolve()==output.resolve()
manifest=json.loads((root/'inspection/logical_parts.json').read_text())
assembled=bpy.context.scene
assert assembled.collection.children.get('BP2_Assembled')
assembled.name='BP2_Assembled'
source=bpy.data.objects['SKM_SurvivalMan_backpack2']
source_materials=[m.name for m in source.data.materials]

# Remove only temporary diagnostic scenes and their uniquely named objects.
for old_scene in list(bpy.data.scenes):
    if old_scene.name.startswith(('BP2_Inspection','BP2_Knife_Inspection')):
        for obj in list(old_scene.objects):
            bpy.data.objects.remove(obj,do_unlink=True)
        bpy.data.scenes.remove(old_scene)
for mesh in list(bpy.data.meshes):
    if mesh.name.startswith('DIAG_') and mesh.users==0:
        bpy.data.meshes.remove(mesh)
for mat in list(bpy.data.materials):
    if mat.name.startswith('BP2_Preview_') and mat.users==0:
        bpy.data.materials.remove(mat)

# The original imported materials remain untouched on the archived source.
preview_mats=[]
for index,key in enumerate(['Backpack','Jacket']):
    mat=source.data.materials[index].copy()
    mat.name='BP2_M_'+key+'_BaseColorPreview'
    mat.use_nodes=True
    nodes=mat.node_tree.nodes
    nodes.clear()
    bsdf=nodes.new('ShaderNodeBsdfPrincipled')
    bsdf.inputs['Roughness'].default_value=.78
    tex=nodes.new('ShaderNodeTexImage')
    tex.image=bpy.data.images.load(str(root/f'textures/T_SurvivalMan_{key}_BaseColor.png'),check_existing=True)
    tex.image.pack()
    mat.node_tree.links.new(tex.outputs['Color'],bsdf.inputs['Base Color'])
    out=nodes.new('ShaderNodeOutputMaterial')
    mat.node_tree.links.new(bsdf.outputs['BSDF'],out.inputs['Surface'])
    mat['source_material']=source_materials[index]
    mat['purpose']='Blender base-color preview; original Unreal shading unchanged.'
    preview_mats.append(mat)
parts=[bpy.data.objects[name] for name in manifest['parts']]
for obj in parts:
    for i,mat in enumerate(preview_mats): obj.data.materials[i]=mat
    if obj.name=='BP2_Knife':
        obj['source_limitation']='Blade tip was truncated/open in source. Needs finishing before full handheld use.'

# Preserve original camera, light and import empty in a hidden helper collection.
helpers=bpy.data.collections.new('BP2_Original_Scene_Helpers')
assembled.collection.children.link(helpers)
for obj in list(assembled.objects):
    if obj.name in {'Camera','Light','SKM_SurvivalMan_backpack2.001'}:
        helpers.objects.link(obj)
        for col in list(obj.users_collection):
            if col!=helpers: col.objects.unlink(obj)
helpers.hide_viewport=True
helpers.hide_render=True

scene=bpy.data.scenes.new('BP2_Parts_Overview')
scene['purpose']='Review only: copies share final meshes, arranged and scaled for readability. Use BP2_Assembled for original scale, alignment and rig.'
scene.render.engine='CYCLES'
scene.cycles.samples=32
scene.render.resolution_x=1800
scene.render.resolution_y=1200
scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('BP2_Review_World')
scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.07,.09,.11,1)
scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.7
scene.view_settings.view_transform='AgX'

camdata=bpy.data.cameras.new('BP2_Review_Camera')
camera=bpy.data.objects.new(camdata.name,camdata)
scene.collection.objects.link(camera)
camera.location=(0,12,0)
camera.rotation_euler=(-camera.location).to_track_quat('-Z','Y').to_euler()
camdata.type='ORTHO';camdata.ortho_scale=9.4
scene.camera=camera

textmat=bpy.data.materials.new('BP2_Review_Text')
textmat.use_nodes=True
nodes=textmat.node_tree.nodes;nodes.clear()
em=nodes.new('ShaderNodeEmission');em.inputs['Color'].default_value=(.83,.85,.81,1)
out=nodes.new('ShaderNodeOutputMaterial');textmat.node_tree.links.new(em.outputs[0],out.inputs[0])
def label(name,text,pos,size=.13,align='CENTER'):
    data=bpy.data.curves.new(name,'FONT');data.body=text;data.size=size;data.align_x=align
    data.materials.append(textmat)
    obj=bpy.data.objects.new(name,data);scene.collection.objects.link(obj)
    obj.location=pos;obj.rotation_euler=camera.rotation_euler
    return obj
label('BP2_Review_Title','BACKPACK  /  SEPARATED PARTS',(4.3,.9,2.77),.24,'LEFT')
label('BP2_Review_Subtitle','7 independent objects  |  Original UVs and skin weights retained',(4.3,.9,2.48),.12,'LEFT')
layout=[
    ('BP2_Backpack','01  BACKPACK + HARNESS',(3.0,0,.05),3.85,20),
    ('BP2_Knife','02  KNIFE',(.65,0,1.28),1.62,-45),
    ('BP2_Axe','03  AXE',(-1.25,0,1.28),1.62,-50),
    ('BP2_Bottle','04  BOTTLE',(-3.25,0,1.28),1.62,20),
    ('BP2_Bottle_Holder','05  BOTTLE HOLDER',(.65,0,-1.10),1.62,130),
    ('BP2_Rope','06  ROPE',(-1.25,0,-1.10),1.62,20),
    ('BP2_Front_Pouch','07  FRONT POUCH',(-3.25,0,-1.10),1.62,20),
]
for name,title,target,maxdim,degrees in layout:
    src=bpy.data.objects[name]
    obj=src.copy();obj.name='REVIEW_'+name;scene.collection.objects.link(obj)
    obj.parent=None
    for modifier in list(obj.modifiers): obj.modifiers.remove(modifier)
    rot=Matrix.Rotation(math.radians(degrees),4,'Z')
    points=[rot @ (src.matrix_world @ v.co) for v in src.data.vertices]
    lo=Vector([min(p[a] for p in points) for a in range(3)])
    hi=Vector([max(p[a] for p in points) for a in range(3)])
    centre=(lo+hi)/2
    scale=maxdim/max(hi.x-lo.x,hi.z-lo.z)
    obj.matrix_world=Matrix.Translation(Vector(target)) @ Matrix.Diagonal((scale,scale,scale,1)) @ Matrix.Translation(-centre) @ rot @ src.matrix_world
    obj['review_only']=True
    label('LABEL_'+name,title,(target[0],.9,target[2]-maxdim/2-.24))
    if name=='BP2_Knife':
        label('NOTE_Knife','Source blade tip is unfinished',(target[0],.9,target[2]-maxdim/2-.43),.082)
label('BP2_Review_Footer','Review layout: parts scaled for visibility. BP2_Assembled retains the original alignment and rig.',(4.3,.9,-2.70),.115,'LEFT')
for name,loc,energy,size in [('Key',(-4,6,6),1800,5),('Fill',(5,4,1),1200,4)]:
    data=bpy.data.lights.new('BP2_Review_'+name,'AREA');data.energy=energy;data.shape='DISK';data.size=size
    obj=bpy.data.objects.new(data.name,data);scene.collection.objects.link(obj)
    obj.location=loc;obj.rotation_euler=(-obj.location).to_track_quat('-Z','Y').to_euler()
scene.render.image_settings.file_format='PNG'
scene.render.filepath=str(root/'Backpack2_Parts_Overview.png')
bpy.ops.render.render(write_still=True,scene=scene.name)

readme='''BACKPACK 2 - SEPARATED SOURCE ASSET

BP2_Assembled: seven editable parts, original scale/alignment, shared copied rig.
BP2_Source_Original: intact imported mesh and original rig, hidden.
BP2_Parts_Overview: presentation copies; scale and position changed for readability.
Edit/export the BP2_ objects in the Assembled scene, not REVIEW_ objects.

Parts: Backpack (straps/harness/fasteners), Knife, Axe, Bottle,
Bottle Holder, Rope, Front Pouch (flap/strap/button).

The original knife blade has a truncated open tip. No replacement geometry was
invented; finish it before using as a fully exposed handheld weapon.
Two original base-color textures are packed in this file for Blender preview.
Normal/ORM shader setup has not been recreated; original Unreal assets unchanged.

Source polygon, vertex and corner IDs remain as bp2_source_* mesh attributes.
Detailed mapping and separation integrity report are embedded in Text data.
Original Hero_head.blend, source FBX and original Unreal asset are not overwritten.
'''
block=bpy.data.texts.new('BP2_README.txt');block.write(readme)
(root/'README.txt').write_text(readme,encoding='utf-8')

# Leave the editable aligned objects visible in a textured three-quarter view.
bpy.context.window.scene=assembled
for obj in bpy.context.selected_objects: obj.select_set(False)
for obj in parts: obj.select_set(True)
bpy.context.view_layer.objects.active=bpy.data.objects['BP2_Backpack']
bpy.data.objects['BP2_Rig'].hide_set(True)
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            space=area.spaces.active
            space.shading.type='MATERIAL'
            space.shading.use_scene_world=False
            space.shading.use_scene_lights=False
            space.overlay.show_overlays=False
            space.region_3d.view_location=Vector((0,.16,1.11))
            space.region_3d.view_rotation=Vector((-.9,1.7,.55)).to_track_quat('Z','Y')
            space.region_3d.view_distance=1.65
            space.region_3d.view_perspective='PERSP'
assert [m.name for m in source.data.materials]==source_materials
receipt=json.loads((root/'inspection/source_archive.json').read_text())
for pathkey,hashkey in [('active_file','active_file_sha256'),('archive','archive_sha256'),('unreal_source','unreal_sha256')]:
    assert hashlib.sha256(Path(receipt[pathkey]).read_bytes()).hexdigest()==receipt[hashkey],pathkey+' changed'
report={'parts':[{'object':obj.name,'vertices':len(obj.data.vertices),'faces':len(obj.data.polygons)} for obj in parts],
        'packed_base_color_images':[mat.node_tree.nodes.get('Image Texture').image.name for mat in preview_mats],
        'material_preview':'Copied materials using original base colors; source materials retained unchanged.',
        'original_files_unchanged':True,'active_scene':assembled.name,'review_scene':scene.name,'preview_image':scene.render.filepath}
(root/'inspection/final_presentation_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(output),check_existing=False)
print(json.dumps(report))
