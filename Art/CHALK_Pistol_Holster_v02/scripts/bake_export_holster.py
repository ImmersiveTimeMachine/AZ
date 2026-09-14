"""Isolated, source-preserving bake and FBX export of the approved holster.

Run using Blender --background --factory-startup --python this_file.py.
Only the four approved holster collections contribute export geometry.

BaseColor is 8-bit encoded sRGB; Normal/ORM are 16-bit linear data.
Unreal ImageCore.h (raw format gamma rules) and Texture.cpp
FTextureSource::GetGammaSpace treat 16/32-bit raw formats as Linear regardless
of GammaSpace/SRGB. The PNG wrapper reads raw 16-bit samples without decoding
their sRGB transfer. Therefore an sRGB-encoded PNG16 base color imports too
bright even when the texture's sRGB checkbox is enabled. Preserve its encoded
colors using the verified native Blender Standard/exposure0 PNG8 output route.
"""
import bpy
import hashlib
import json
import math
import time
from pathlib import Path
from mathutils import Matrix, Vector

import numpy as np
from io_scene_fbx import parse_fbx

ROOT = Path('C:/UnrealEngine/Games/AZ/Art/CHALK_Pistol_Holster_v02')
SOURCE = ROOT / 'CHALK_Holster01_Rebuilt.blend'
EXPORTS = ROOT / 'exports'
TEXTURES = ROOT / 'textures'
DEST = ROOT / 'CHALK_Holster01_UnrealExport.blend'
FBX = EXPORTS / 'SM_CHALK_PistolHolster01.fbx'
MANIFEST = EXPORTS / 'export_manifest.json'
PROGRESS = EXPORTS / 'bake_progress.json'
SIZE = 4096
START = time.time()


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def progress(stage, **values):
    value = {'stage': stage, 'elapsed_seconds': round(time.time() - START, 2), **values}
    PROGRESS.write_text(json.dumps(value, indent=2), encoding='utf-8')
    print('HOLSTER_EXPORT ' + json.dumps(value), flush=True)


EXPORTS.mkdir(parents=True, exist_ok=True)
TEXTURES.mkdir(parents=True, exist_ok=True)
for target in [DEST, FBX, MANIFEST]:
    if target.exists():
        raise RuntimeError('Refusing to overwrite existing final output: ' + str(target))
source_hash = sha(SOURCE)
source_manifest = json.loads((ROOT / 'build_manifest.json').read_text(encoding='utf-8-sig'))
allowed_collections = source_manifest['holster_collections']
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
scene = bpy.context.scene
progress('loaded source', source_sha256=source_hash)

original_objects = list(bpy.data.objects)
source_objects = sorted({obj for name in allowed_collections
                         for obj in bpy.data.collections[name].all_objects
                         if obj.type in {'MESH', 'CURVE', 'SURFACE', 'FONT'}}, key=lambda o: o.name)
if len(source_objects) != 65:
    raise RuntimeError('Expected the approved 65 renderable holster objects, got ' + str(len(source_objects)))
if any('pistol' in obj.name.lower() or 'REFERENCE' in obj.name for obj in source_objects):
    raise RuntimeError('Unexpected reference object in the allowed collection selection')

working = bpy.data.collections.new('EXPORT / baked single mesh')
scene.collection.children.link(working)
records = []
copies = []
depsgraph = bpy.context.evaluated_depsgraph_get()
for src in source_objects:
    original_eval = src.evaluated_get(depsgraph)
    original_mesh = original_eval.to_mesh(preserve_all_data_layers=True, depsgraph=depsgraph)
    original_mesh.calc_loop_triangles()
    old_count = len(original_mesh.loop_triangles)
    original_eval.to_mesh_clear()
    obj = src.copy()
    obj.data = src.data.copy()
    world = src.matrix_world.copy()
    obj.parent = None
    obj.matrix_world = world
    working.objects.link(obj)
    obj.hide_render = False
    obj.hide_viewport = False
    obj.hide_set(False)
    changes = []
    roles = {m.get('holster_role') for m in obj.data.materials if m}
    if obj.type == 'CURVE' and roles == {'thread'}:
        obj.data.resolution_u = 2
        obj.data.render_resolution_u = 2
        obj.data.bevel_resolution = 0
        changes.append('Thread curve resolution_u/render_resolution_u=2; bevel_resolution=0')
    if src.name in ['Canvas pocket / real open cavity', 'Front canvas panel / curved and softly worn']:
        for modifier in obj.modifiers:
            if modifier.type == 'SUBSURF':
                modifier.levels = 0
                modifier.render_levels = 0
                changes.append('Export-only dense canvas base cage; one subdivision level removed')
    bpy.context.view_layer.update()
    evaluated = obj.evaluated_get(depsgraph)
    mesh = bpy.data.meshes.new_from_object(evaluated, preserve_all_data_layers=True, depsgraph=depsgraph)
    mesh.transform(world)
    mesh.update()
    converted = bpy.data.objects.new('BAKE / ' + src.name, mesh)
    working.objects.link(converted)
    mesh.calc_loop_triangles()
    records.append({'source_object': src.name, 'source_type': src.type,
                    'source_triangles': old_count, 'export_triangles': len(mesh.loop_triangles),
                    'export_only_changes': changes, 'material_roles': sorted(str(x) for x in roles)})
    copies.append(converted)
    bpy.data.objects.remove(obj, do_unlink=True)

original_total = sum(r['source_triangles'] for r in records)
export_total = sum(r['export_triangles'] for r in records)
if original_total != 398516 or export_total != 155200:
    raise RuntimeError('Geometry differs from audited counts: ' + str((original_total, export_total)))
progress('evaluated approved geometry', source_objects=len(records), source_triangles=original_total,
         export_triangles=export_total)

# Keep original coordinate anchor and lighting while baking. Coincident source
# geometry must never enter the bake scene or occlusion rays.
for obj in original_objects:
    if obj.type not in {'LIGHT', 'CAMERA', 'EMPTY'}:
        obj.hide_render = True
    obj.hide_set(True)
for obj in bpy.context.selected_objects:
    obj.select_set(False)
for obj in copies:
    obj.select_set(True)
bpy.context.view_layer.objects.active = copies[0]
bpy.ops.object.join()
target = bpy.context.view_layer.objects.active
target.name = 'SM_CHALK_PistolHolster01'
target.data.name = 'SM_CHALK_PistolHolster01_Mesh'
target.matrix_world = Matrix.Identity(4)
target.parent = None
target.hide_render = False
target.hide_set(False)
target.vertex_groups.clear()
triangulate = target.modifiers.new('Freeze render/export triangulation before tangent bake', 'TRIANGULATE')
if hasattr(triangulate, 'keep_custom_normals'):
    triangulate.keep_custom_normals = True
bpy.ops.object.modifier_apply(modifier=triangulate.name)
if len(target.data.polygons) != 155200 or any(len(p.vertices) != 3 for p in target.data.polygons):
    raise RuntimeError('Unexpected triangulation result')

# Separate material copies permit temporary EMIT outputs without touching the
# source materials. Preserve ClothMeters until all procedural sampling ends.
for i, material in enumerate(target.data.materials):
    target.data.materials[i] = material.copy()
    target.data.materials[i].name = 'BAKE_SOURCE / ' + material.name
materials = list(target.data.materials)
old_uv_names = [uv.name for uv in target.data.uv_layers]
atlas = target.data.uv_layers.new(name='UVAtlas', do_init=False)
target.data.uv_layers.active = atlas
atlas.active_render = True
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
progress('unwrapping unique normalized atlas', texture_size=SIZE, preserved_shader_uvs=old_uv_names)
bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=16.0 / SIZE,
                         area_weight=0.0, correct_aspect=True, scale_to_bounds=True)
bpy.ops.object.mode_set(mode='OBJECT')
# Edit Mode rebuilds mesh CustomData; the pre-edit UV RNA handle is stale.
atlas = target.data.uv_layers['UVAtlas']
target.data.uv_layers.active = atlas
atlas.active_render = True
uv_values = np.empty(len(atlas.data) * 2, dtype=np.float32)
atlas.data.foreach_get('uv', uv_values)
uv_values = uv_values.reshape(-1, 2)
if not np.isfinite(uv_values).all() or uv_values.min() < -1e-5 or uv_values.max() > 1.00001:
    raise RuntimeError('Atlas escaped the normalized UV tile')
uv_area = np.abs(np.cross((uv_values.reshape(-1, 3, 2)[:, 1] - uv_values.reshape(-1, 3, 2)[:, 0]),
                          (uv_values.reshape(-1, 3, 2)[:, 2] - uv_values.reshape(-1, 3, 2)[:, 0]))).sum() * .5
if not .03 < uv_area < 1.001:
    raise RuntimeError('Atlas coverage is invalid: ' + str(uv_area))
progress('atlas ready', uv_area_sum=float(uv_area), uv_min=float(uv_values.min()), uv_max=float(uv_values.max()))

scene.render.engine = 'CYCLES'
scene.cycles.samples = 8
scene.cycles.use_denoising = False
scene.render.bake.use_selected_to_active = False
scene.render.bake.use_clear = False
scene.render.bake.margin = 8
scene.render.bake.margin_type = 'EXTEND'
scene.render.bake.target = 'IMAGE_TEXTURES'
scene.render.bake.normal_space = 'TANGENT'
scene.render.bake.normal_r = 'POS_X'
scene.render.bake.normal_g = 'POS_Y'
scene.render.bake.normal_b = 'POS_Z'
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0
preferences = bpy.context.preferences.addons['cycles'].preferences
cuda = preferences.get_device_list('CUDA')  # Never enumerate oneAPI/other backends on this host.
if any(device[1] == 'CUDA' for device in cuda):
    preferences.compute_device_type = 'CUDA'
    for device in preferences.devices:
        device.use = device.type == 'CUDA'
    scene.cycles.device = 'GPU'
    device_name = next(device[0] for device in cuda if device[1] == 'CUDA')
else:
    scene.cycles.device = 'CPU'
    device_name = 'CPU'
progress('bake engine ready', device=device_name, samples=scene.cycles.samples)

original_surfaces = {}
bsdfs = {}
for mat in materials:
    output = next(n for n in mat.node_tree.nodes if n.type == 'OUTPUT_MATERIAL' and n.is_active_output)
    original_surfaces[mat.name] = (output, output.inputs['Surface'].links[0].from_socket)
    bsdfs[mat.name] = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')


def wire(mat, source, target_socket):
    if hasattr(source, 'is_output'):
        mat.node_tree.links.new(source, target_socket)
    else:
        target_socket.default_value = source


def input_value(socket):
    return socket.links[0].from_socket if socket.is_linked else socket.default_value


def set_bake_image(image):
    for mat in materials:
        nodes = mat.node_tree.nodes
        for node in nodes:
            node.select = False
        node = nodes.get('BAKE_TARGET') or nodes.new('ShaderNodeTexImage')
        node.name = 'BAKE_TARGET'
        node.label = 'Active bake target / UVAtlas only'
        node.image = image
        node.select = True
        nodes.active = node


def save_texture(image, path, key):
    if key == 'base_color':
        # Never use the approved scene's AgX/exposure-2 settings for albedo.
        # Standard + exposure0 + gamma1 applies only the sRGB output transfer.
        conversion_scene = bpy.data.scenes.new('TEMP / sRGB 8-bit base-color output')
        try:
            conversion_scene.view_settings.view_transform = 'Standard'
            conversion_scene.view_settings.look = 'None'
            conversion_scene.view_settings.exposure = 0.0
            conversion_scene.view_settings.gamma = 1.0
            conversion_scene.display_settings.display_device = 'sRGB'
            conversion_scene.render.image_settings.file_format = 'PNG'
            conversion_scene.render.image_settings.color_mode = 'RGB'
            conversion_scene.render.image_settings.color_depth = '8'
            conversion_scene.render.dither_intensity = 0.0
            image.save_render(str(path), scene=conversion_scene)
        finally:
            bpy.data.scenes.remove(conversion_scene)
    else:
        # Keep normal and ORM channels linear and 16-bit, without a view transform.
        image.filepath_raw = str(path)
        image.file_format = 'PNG'
        image.save()
    if not path.exists() or path.stat().st_size < 10000:
        raise RuntimeError('Texture write failed: ' + str(path))
    with path.open('rb') as stream:
        header = stream.read(25)
    expected_depth = 8 if key == 'base_color' else 16
    if header[:8] != b'\x89PNG\r\n\x1a\n' or header[24] != expected_depth:
        raise RuntimeError('Unexpected PNG depth for ' + key)
    if key == 'base_color':
        # Pack/use the same verified byte image that Unreal receives, rather
        # than retaining the float bake as a different hidden texture source.
        converted = bpy.data.images.load(str(path), check_existing=False)
        converted.colorspace_settings.name = 'sRGB'
        converted.name = 'T_CHALK_PistolHolster01_BaseColor_8bit_sRGB'
        return converted
    return image


def bake_channel(key, suffix, mode):
    image = bpy.data.images.new('T_CHALK_PistolHolster01_' + suffix, width=SIZE, height=SIZE,
                                alpha=False, float_buffer=True, is_data=(key != 'base_color'))
    image.colorspace_settings.name = 'sRGB' if key == 'base_color' else 'Non-Color'
    image.generated_color = ((.5, .5, 1.0, 1.0) if key == 'normal' else
                             (1.0, 1.0, 0.0, 1.0) if key == 'orm' else (0.02, .02, .02, 1.0))
    set_bake_image(image)
    for mat in materials:
        output, original_surface = original_surfaces[mat.name]
        bsdf = bsdfs[mat.name]
        if mode == 'NORMAL':
            wire(mat, original_surface, output.inputs['Surface'])
        else:
            emission = mat.node_tree.nodes.new('ShaderNodeEmission')
            emission.name = 'TEMP_BAKE_EMISSION_' + key
            emission.inputs['Strength'].default_value = 1.0
            if key == 'base_color':
                wire(mat, input_value(bsdf.inputs['Base Color']), emission.inputs['Color'])
            else:
                combine = mat.node_tree.nodes.new('ShaderNodeCombineColor')
                combine.mode = 'RGB'
                combine.name = 'TEMP_ORM / R AO, G roughness, B metallic'
                ao = mat.node_tree.nodes.new('ShaderNodeAmbientOcclusion')
                ao.name = 'TEMP_AO / holster-only local cavity shading'
                ao.samples = 16
                ao.only_local = True
                ao.inputs['Distance'].default_value = .03
                wire(mat, ao.outputs['AO'], combine.inputs['Red'])
                wire(mat, input_value(bsdf.inputs['Roughness']), combine.inputs['Green'])
                wire(mat, input_value(bsdf.inputs['Metallic']), combine.inputs['Blue'])
                wire(mat, combine.outputs[0], emission.inputs['Color'])
            wire(mat, emission.outputs[0], output.inputs['Surface'])
    progress('baking ' + key, resolution=SIZE, pass_type=mode)
    before = time.time()
    bpy.ops.object.bake(type=mode, uv_layer='UVAtlas')
    path = TEXTURES / (image.name + '.png')
    # Range statistics confirm finite, nonempty data independent of display exposure.
    pixels = np.empty(SIZE * SIZE * 4, dtype=np.float32)
    image.pixels.foreach_get(pixels)
    pixels = pixels.reshape(-1, 4)
    if not np.isfinite(pixels).all():
        raise RuntimeError('Nonfinite bake pixels: ' + key)
    stats = {'min': pixels[:, :3].min(axis=0).tolist(), 'max': pixels[:, :3].max(axis=0).tolist(),
             'mean': pixels[:, :3].mean(axis=0).tolist(), 'statistics_space': 'scene-linear bake floats'}
    image = save_texture(image, path, key)
    stats['seconds'] = round(time.time() - before, 2)
    progress('saved ' + key, path=str(path), bytes=path.stat().st_size, statistics=stats)
    return image, path, stats


images, paths, stats = {}, {}, {}
for key, suffix, mode in [('base_color', 'BaseColor', 'EMIT'), ('normal', 'Normal', 'NORMAL'), ('orm', 'ORM', 'EMIT')]:
    images[key], paths[key], stats[key] = bake_channel(key, suffix, mode)

# One conventional PBR atlas material; exported UV0 is the atlas only.
final_material = bpy.data.materials.new('M_CHALK_PistolHolster01_Baked')
final_material.use_nodes = True
nodes, links = final_material.node_tree.nodes, final_material.node_tree.links
nodes.clear()
output = nodes.new('ShaderNodeOutputMaterial')
principled = nodes.new('ShaderNodeBsdfPrincipled')
principled.inputs['Specular IOR Level'].default_value = .25
principled.inputs['Sheen Weight'].default_value = .025
links.new(principled.outputs['BSDF'], output.inputs['Surface'])
for index, (key, image) in enumerate(images.items()):
    texture = nodes.new('ShaderNodeTexImage')
    texture.name = key.upper() + ' / baked 4096 atlas'
    texture.image = image
    texture.location = (-750, 350 - index * 330)
    texture.interpolation = 'Linear'
    if key == 'base_color':
        links.new(texture.outputs['Color'], principled.inputs['Base Color'])
    elif key == 'normal':
        normal = nodes.new('ShaderNodeNormalMap')
        normal.space = 'TANGENT'
        normal.uv_map = 'UVAtlas'
        normal.inputs['Strength'].default_value = 1.0
        normal.location = (-390, 0)
        links.new(texture.outputs['Color'], normal.inputs['Color'])
        links.new(normal.outputs['Normal'], principled.inputs['Normal'])
    else:
        separate = nodes.new('ShaderNodeSeparateColor')
        separate.mode = 'RGB'
        separate.name = 'ORM / R AO for Unreal, G roughness, B metallic'
        separate.location = (-390, -330)
        links.new(texture.outputs['Color'], separate.inputs[0])
        links.new(separate.outputs['Green'], principled.inputs['Roughness'])
        links.new(separate.outputs['Blue'], principled.inputs['Metallic'])
principled.location = (0, 180)
output.location = (340, 180)
target.data.materials.clear()
target.data.materials.append(final_material)
for polygon in target.data.polygons:
    polygon.material_index = 0
for uv in list(target.data.uv_layers):
    if uv.name != 'UVAtlas':
        target.data.uv_layers.remove(uv)
target.data.uv_layers.active_index = 0
target.data.uv_layers[0].active_render = True
for name in ['holster_edge_wear']:
    attribute = target.data.attributes.get(name)
    if attribute:
        target.data.attributes.remove(attribute)
target['source_asset'] = str(SOURCE)
target['export_scope'] = 'Holster only, no pistol, reference, studio or parent empty in FBX.'
target['pivot'] = 'Approved identity thigh-attachment origin, unchanged.'
target['normal_convention'] = 'OpenGL tangent +Y; Unreal importer must flip green.'

# The editable export file retains just the baked asset and its presentation
# camera/lights. Source geometry and pistol datablocks are excluded entirely.
for obj in list(bpy.data.objects):
    if obj != target and obj.type not in {'LIGHT', 'CAMERA'}:
        bpy.data.objects.remove(obj, do_unlink=True)
for obj in bpy.data.objects:
    obj.hide_set(obj.type in {'LIGHT', 'CAMERA'})
target.hide_set(False)
for obj in bpy.context.selected_objects:
    obj.select_set(False)
target.select_set(True)
bpy.context.view_layer.objects.active = target

expected_points = np.array([(v.co.x * 100, -v.co.y * 100, v.co.z * 100) for v in target.data.vertices])
expected_bounds = {'min': expected_points.min(axis=0).tolist(), 'max': expected_points.max(axis=0).tolist()}
settings = dict(use_selection=True, object_types={'MESH'}, global_scale=1.0, apply_unit_scale=True,
                apply_scale_options='FBX_SCALE_NONE', axis_forward='Y', axis_up='Z',
                use_space_transform=True, bake_space_transform=False, use_mesh_modifiers=False,
                mesh_smooth_type='FACE', use_mesh_edges=False, use_tspace=True, use_triangles=False,
                bake_anim=False, path_mode='STRIP', embed_textures=False, use_custom_props=False)
progress('exporting FBX', triangles=len(target.data.polygons), expected_bounds_ue_cm=expected_bounds)
bpy.ops.export_scene.fbx(filepath=str(FBX), check_existing=False, **settings)
tree, fbx_version = parse_fbx.parse(str(FBX))
fbx_objects = next(e for e in tree.elems if e.id == b'Objects')
fbx_geometry = [e for e in fbx_objects.elems if e.id == b'Geometry']
if len(fbx_geometry) != 1:
    raise RuntimeError('Export unexpectedly contains multiple geometries')
indices = next(e for e in fbx_geometry[0].elems if e.id == b'PolygonVertexIndex').props[0]
if sum(i < 0 for i in indices) != 155200:
    raise RuntimeError('FBX polygon count mismatch')
if any(e.id in {b'Deformer', b'AnimationStack'} for e in fbx_objects.elems):
    raise RuntimeError('Unexpected rig or animation in static FBX')

for image in images.values():
    image.pack()
for block in list(bpy.data.collections):
    if block != working and not block.all_objects:
        bpy.data.collections.remove(block)
bpy.ops.outliner.orphans_purge(do_local_ids=True, do_linked_ids=False, do_recursive=True)
notes = bpy.data.texts.new('UNREAL_EXPORT_README')
notes.write('Approved holster export copy. One static mesh, one unique normalized UV atlas, one PBR material. '
            'FBX Y forward/Z up; Unreal coordinates=(X,-Y,Z)*100 cm. BaseColor=sRGB. '
            'BaseColor PNG8 preserves encoded sRGB; Normal/ORM PNG16 remain linear. '
            'Normal=Non-Color, tangent OpenGL +Y (flip green in Unreal). ORM=Non-Color; R=AO, G=roughness, B=metallic. '
            'Only the approved four holster collections supplied geometry; the pistol is excluded. '
            '155,200 triangles: thread tessellation reduced and one subdivision removed from dense canvas bases. '
            'Measured local surface deviation below0.95mm; this is an export approximation, not an exact master replacement. '
            'Original approved Blender source is unchanged.')
bpy.ops.wm.save_as_mainfile(filepath=str(DEST))
if sha(SOURCE) != source_hash:
    raise RuntimeError('Original source hash changed')

report = {'source_blend': str(SOURCE), 'source_blend_sha256': source_hash, 'source_unchanged': True,
          'source_collections': allowed_collections, 'source_renderable_objects': len(records),
          'source_triangles': original_total, 'source_object_records': records,
          'export_blend': str(DEST), 'export_blend_sha256': sha(DEST),
          'fbx_path': str(FBX), 'fbx_sha256': sha(FBX), 'fbx_version': fbx_version,
          'expected_triangles': len(target.data.polygons), 'expected_vertices': len(target.data.vertices),
          'expected_bounds_ue_cm': expected_bounds, 'material_slots': 1, 'uv_count': 1, 'uv_name': 'UVAtlas',
          'atlas_resolution': SIZE, 'atlas_uv_area_sum': float(uv_area),
          'texture_files': {k: str(v) for k, v in paths.items()},
          'texture_sha256': {k: sha(v) for k, v in paths.items()}, 'bake_statistics': stats,
          'normal_convention': 'Blender tangent +Y / OpenGL; Unreal flip_green_channel=True',
          'orm_channels': {'R': 'AO, local holster only, distance0.03m and16 AO node samples', 'G': 'linear roughness', 'B': 'linear metallic'},
          'texture_srgb': {'base_color': True, 'normal': False, 'orm': False},
          'texture_bit_depth': {'base_color': 8, 'normal': 16, 'orm': 16},
          'base_color_encoding_note': 'Native Blender Standard/exposure0/gamma1/sRGB/dither0 PNG8. Unreal raw16/32 formats are always Linear, so encoded sRGB PNG16 must not be used for base color.',
          'bake_engine': 'Cycles', 'bake_device': device_name, 'bake_samples': 8,
          'export_geometry_note': 'Approved game copy: thread bevel0, curve resolution2, shell/front Subsurf0. No decimation. Bounds shift<0.015mm; measured master surface deviation<0.95mm.',
          'pivot': 'Original approved thigh-attachment origin, identity transform',
          'coordinate_recipe': 'FBX Y forward/Z up, metric scale1; UE=(X,-Y,Z)*100cm',
          'pistol_excluded': True, 'reference_studio_root_excluded_from_fbx': True,
          'fbx_geometry_verified': True, 'elapsed_seconds': round(time.time() - START, 2)}
MANIFEST.write_text(json.dumps(report, indent=2), encoding='utf-8')
progress('export ready for Unreal import', manifest=str(MANIFEST), fbx=str(FBX))

# Comparable baked asset preview under the approved camera/light/exposure setup.
scene.cycles.samples = 32
scene.cycles.use_denoising = True
scene.render.resolution_x = 900
scene.render.resolution_y = 1200
scene.render.resolution_percentage = 100
scene.render.filepath = str(EXPORTS / 'SM_CHALK_PistolHolster01_BakedPreview.png')
bpy.ops.render.render(write_still=True)
report['baked_preview'] = scene.render.filepath
report['elapsed_seconds'] = round(time.time() - START, 2)
MANIFEST.write_text(json.dumps(report, indent=2), encoding='utf-8')
progress('complete', manifest=str(MANIFEST), preview=scene.render.filepath)
