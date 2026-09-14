"""Embed the selected 01 concept as a hidden, editable Blender image reference."""
import bpy
import json
import math
from pathlib import Path

root = Path('C:/UnrealEngine/Games/AZ/Art/CHALK_Pistol_Holster_v01')
blend = root / 'CHALK_PistolHolster_01_CanvasThigh.blend'
reference_path = Path('C:/UnrealEngine/Games/AZ/UI Design/CHALK_Pistol_Holster_v01/concepts/01_Thigh_Canvas_Holster.png')
assert Path(bpy.data.filepath).resolve() == blend.resolve(), bpy.data.filepath
assert reference_path.is_file()

collection = bpy.data.collections.get('PH01_Reference / Packed concept image')
if collection is None:
    collection = bpy.data.collections.new('PH01_Reference / Packed concept image')
    bpy.context.scene.collection.children.link(collection)

image = bpy.data.images.get('REFERENCE_Option01_CanvasThighConcept')
if image is None:
    image = bpy.data.images.load(str(reference_path), check_existing=False)
    image.name = 'REFERENCE_Option01_CanvasThighConcept'
image.pack()

material = bpy.data.materials.get('PH01_M_Reference_Option01') or bpy.data.materials.new('PH01_M_Reference_Option01')
material.use_nodes = True
nodes, links = material.node_tree.nodes, material.node_tree.links
nodes.clear()
output = nodes.new('ShaderNodeOutputMaterial')
shader = nodes.new('ShaderNodeBsdfPrincipled')
texture = nodes.new('ShaderNodeTexImage')
texture.image = image
shader.inputs['Roughness'].default_value = 1.0
links.new(texture.outputs['Color'], shader.inputs['Base Color'])
links.new(shader.outputs['BSDF'], output.inputs['Surface'])

plane = bpy.data.objects.get('REFERENCE_Option01_CanvasThighConcept_Plane')
if plane is None:
    bpy.ops.mesh.primitive_plane_add(size=1, location=(.72, .15, -.10), rotation=(math.pi / 2, 0, math.pi / 2))
    plane = bpy.context.object
    plane.name = 'REFERENCE_Option01_CanvasThighConcept_Plane'
    for owner in list(plane.users_collection):
        owner.objects.unlink(plane)
    collection.objects.link(plane)
    plane.scale = (.28, .32, 1)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
plane.data.materials.clear()
plane.data.materials.append(material)
plane['purpose'] = 'Packed visual reference only; not part of PH01 model export.'
plane.hide_render = True
plane.hide_set(True)

manifest_path = root / 'CHALK_PistolHolster_01_CanvasThigh_manifest.json'
manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
manifest['packed_reference_image'] = image.name
manifest['hidden_reference_plane'] = plane.name
manifest_path.write_text(json.dumps(manifest, indent=2), encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(blend), check_existing=False)
print({'reference': image.name, 'packed': bool(image.packed_file), 'plane': plane.name})
