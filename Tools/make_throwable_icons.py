"""Blender-only: owned prototype inventory icons and a simple ignition-tool mesh.
No project mesh/texture is edited. Outputs remain in Art/CHALK_Throwables_v01.
"""
import bpy
import math
import os
from mathutils import Vector

OUT = 'C:/UnrealEngine/Games/AZ/Art/CHALK_Throwables_v01'
os.makedirs(OUT, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.render.resolution_x = 256
scene.render.resolution_y = 256
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'
scene.render.film_transparent = True
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'MATERIAL'
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.display.shading.cavity_type = 'BOTH'
scene.display.shading.show_specular_highlight = True
scene.world.color = (0.7, 0.7, 0.7)
palette = {}
for name, color in [('glass',(0.16,0.27,0.20,1)),('metal',(0.40,0.43,0.40,1)),
                    ('dark',(0.06,0.08,0.07,1)),('cloth',(0.65,0.57,0.41,1)),
                    ('rock',(0.31,0.33,0.29,1)),('paper',(0.78,0.73,0.57,1)),
                    ('fuel',(0.43,0.25,0.10,1))]:
    m=bpy.data.materials.new(name); m.diffuse_color=color; palette[name]=m

parts=[]
def finish(obj, name, material):
    obj.name=name; obj.data.materials.append(palette[material]); parts.append(obj); return obj

def box(name, location, size, material, bevel=0.06):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location)
    obj=bpy.context.object; obj.dimensions=size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        mod=obj.modifiers.new('Soft edges','BEVEL'); mod.width=bevel; mod.segments=3
    return finish(obj,name,material)

def cylinder(name, location, radius, depth, material, top=None):
    bpy.ops.mesh.primitive_cone_add(vertices=32, radius1=radius, radius2=radius if top is None else top, depth=depth, location=location)
    obj=bpy.context.object
    bevel=obj.modifiers.new('Soft edges','BEVEL'); bevel.width=0.025; bevel.segments=2
    return finish(obj,name,material)

def bottle():
    cylinder('Bottle body',(0,0,0.75),0.37,1.5,'glass')
    cylinder('Bottle shoulder',(0,0,1.57),0.37,0.24,'glass',0.16)
    cylinder('Bottle neck',(0,0,1.94),0.16,0.5,'glass')
    cylinder('Bottle rim',(0,0,2.20),0.19,0.08,'glass')
    cylinder('Paper label',(0,0,0.91),0.375,0.58,'paper')

def build(kind):
    if kind in ['Bottle','Incendiary']:
        bottle()
        if kind=='Incendiary':
            wick=box('Cloth marker',(0.10,0,2.39),(0.13,0.12,0.40),'cloth',0.015)
            wick.rotation_euler[1]=-0.45
            box('Cloth band',(0,0,1.3),(0.79,0.08,0.18),'cloth',0.02)
    elif kind=='Stone':
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1)
        obj=bpy.context.object; obj.scale=(0.95,0.72,0.55); obj.location.z=0.55
        for i,v in enumerate(obj.data.vertices): v.co*=1.0+0.10*math.sin(i*3.7)
        finish(obj,'Stone','rock')
    elif kind=='Fuel':
        box('Resource container',(0,0,0.8),(1.25,0.72,1.55),'fuel')
        box('Top grip',(0,0,1.70),(0.82,0.18,0.15),'dark')
        for x in [-0.32,0.32]: box('Grip foot',(x,0,1.57),(0.16,0.18,0.22),'dark',0.025)
        cylinder('Cap',(-0.44,0.0,1.63),0.14,0.16,'metal')
        box('Container label',(0,-0.368,0.86),(0.72,0.015,0.58),'paper',0.015)
    elif kind=='Fabric':
        for i in range(3):
            o=box('Folded cloth',(0,0,0.18+i*0.19),(1.55-0.08*i,0.93,0.24),'cloth',0.10)
            o.rotation_euler[2]=0.025*i
        box('Cloth fold',(0.55,0,0.55),(0.20,0.96,0.15),'paper',0.06)
    elif kind=='Igniter':
        box('Igniter body',(0,0,0.74),(0.93,0.42,1.48),'metal',0.09)
        box('Igniter top',(0,0,1.67),(0.93,0.43,0.38),'dark',0.04)
        cylinder('Top detail',(0.22,0,1.91),0.11,0.09,'metal')

bpy.ops.object.camera_add(location=(4,-6,3.1))
camera=bpy.context.object; scene.camera=camera
camera.data.type='ORTHO'; camera.data.ortho_scale=3.4
for kind in ['Stone','Bottle','Incendiary','Fabric','Fuel','Igniter']:
    for obj in parts: bpy.data.objects.remove(obj,do_unlink=True)
    parts=[]; build(kind)
    target=Vector((0,0,1.02 if kind in ['Bottle','Incendiary','Igniter','Fuel'] else 0.42))
    camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=os.path.join(OUT,'T_Throwable_'+kind+'.png')
    bpy.ops.render.render(write_still=True)
    if kind in ['Igniter','Incendiary']:
        bpy.ops.object.select_all(action='DESELECT')
        for obj in parts: obj.select_set(True)
        bpy.context.view_layer.objects.active=parts[0]
        # Scale the decorative mesh to a 6 cm in-game prop; no functional internal construction.
        mesh_scale=0.03 if kind=='Igniter' else 0.115
        for obj in parts:
            obj.location*=mesh_scale; obj.scale*=mesh_scale
        bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
        mesh_name='SM_ThrowIgniter' if kind=='Igniter' else 'SM_ThrowIncendiary'
        bpy.ops.export_scene.fbx(filepath=os.path.join(OUT,mesh_name+'.fbx'),use_selection=True,
                                 apply_unit_scale=True,object_types={'MESH'},add_leaf_bones=False)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT,'Throwables_Icons.blend'))
print('THROWABLE_ART_READY icons=6 meshes=2')
