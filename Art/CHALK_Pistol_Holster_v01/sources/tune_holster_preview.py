"""Correct review-only lighting in the task-owned Canvas Thigh holster blend."""
import bpy
import json
from pathlib import Path

root = Path('C:/UnrealEngine/Games/AZ/Art/CHALK_Pistol_Holster_v01')
blend = root / 'CHALK_PistolHolster_01_CanvasThigh.blend'
preview = root / 'CHALK_PistolHolster_01_CanvasThigh_Preview.png'
assert Path(bpy.data.filepath).resolve() == blend.resolve(), bpy.data.filepath

energies = {
    'REVIEW_Key / overcast': 24,
    'REVIEW_Fill / neutral': 8,
    'REVIEW_Rim / cool': 14,
}
for name, energy in energies.items():
    lamp = bpy.data.objects.get(name)
    assert lamp and lamp.type == 'LIGHT', name
    lamp.data.energy = energy

def hex_rgba(value):
    value = value.removeprefix('#')
    return tuple(int(value[i:i+2], 16) / 255 for i in (0, 2, 4)) + (1,)

def woven_material(name, dark, light, scale, roughness):
    mat = bpy.data.materials[name]
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    for node in list(nodes):
        nodes.remove(node)
    output = nodes.new('ShaderNodeOutputMaterial')
    shader = nodes.new('ShaderNodeBsdfPrincipled')
    noise = nodes.new('ShaderNodeTexNoise')
    noise.inputs['Scale'].default_value = scale
    noise.inputs['Detail'].default_value = 3.0
    noise.inputs['Roughness'].default_value = .7
    ramp = nodes.new('ShaderNodeValToRGB')
    ramp.color_ramp.elements[0].position = .31
    ramp.color_ramp.elements[0].color = hex_rgba(dark)
    ramp.color_ramp.elements[1].position = .74
    ramp.color_ramp.elements[1].color = hex_rgba(light)
    shader.inputs['Roughness'].default_value = roughness
    links.new(noise.outputs['Fac'], ramp.inputs['Fac'])
    links.new(ramp.outputs['Color'], shader.inputs['Base Color'])
    links.new(shader.outputs['BSDF'], output.inputs['Surface'])
    mat.diffuse_color = hex_rgba(light)

woven_material('PH01_M_Canvas_Charcoal', '#18201E', '#37423C', 105, .83)
woven_material('PH01_M_Canvas_EdgeBinding', '#0C110F', '#202825', 125, .86)
woven_material('PH01_M_Olive_Webbing', '#303824', '#5C6545', 145, .80)
woven_material('PH01_M_Blue_RepairPatch', '#1F3146', '#526F92', 80, .82)
woven_material('PH01_M_Review_Thigh', '#222826', '#3F4944', 28, .92)
woven_material('PH01_M_Review_Floor', '#090D0C', '#18201E', 16, .90)

root_empty = bpy.data.objects['PH01_Root_RightThigh']
asset_collections = [
    bpy.data.collections['PH01_Model / Canvas thigh holster'],
    bpy.data.collections['PH01_Details / Stitching and hardware'],
]
parented = []
for collection in asset_collections:
    for obj in collection.objects:
        if obj != root_empty:
            obj.parent = root_empty
            obj.matrix_parent_inverse = root_empty.matrix_world.inverted()
            parented.append(obj.name)

world = bpy.context.scene.world
world.use_nodes = True
background = world.node_tree.nodes.get('Background')
background.inputs['Color'].default_value = (0.013, 0.017, 0.016, 1)
background.inputs['Strength'].default_value = .18
bpy.context.scene.view_settings.view_transform = 'AgX'
bpy.context.scene.view_settings.look = 'AgX - Medium High Contrast'
bpy.context.scene.view_settings.exposure = -2.4
bpy.context.scene.render.filepath = str(preview)
bpy.ops.wm.save_as_mainfile(filepath=str(blend), check_existing=False)
bpy.ops.render.render(write_still=True)
assert preview.is_file()
manifest_path = root / 'CHALK_PistolHolster_01_CanvasThigh_manifest.json'
manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
manifest['all_asset_components_parented_to_root'] = True
manifest['parented_component_count'] = len(parented)
manifest['preview_lighting'] = {'energies': energies, 'exposure': bpy.context.scene.view_settings.exposure}
manifest_path.write_text(json.dumps(manifest, indent=2), encoding='utf-8')
print({'saved': str(blend), 'preview': str(preview), 'light_energies': energies, 'parented_components': len(parented)})
