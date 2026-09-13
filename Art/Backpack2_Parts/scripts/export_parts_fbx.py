"""Export seven assembled skinned copies in a separate Blender 5.2 process.

Run with Blender --background --factory-startup --python-exit-code 1 --python
this_file.py. Optional arguments after --: --source PATH --output-dir PATH.
The source .blend is loaded as library copies and is never opened for editing
or saved. The live Blender scene is never contacted. Existing FBXs/report are
not overwritten. Each FBX is parsed back to guard its geometry, normals, UVs,
weights, material names, full bone hierarchy, centimeter units and tangent data.

This imported asset has 100 Blender bones and the armature object represents
Unreal's real `root` bone. Export that object as Root named `root`; do not name
it Armature, which Unreal's Blender workaround strips from skeleton hierarchies.
Y forward / Z up matches Unreal's default FBX import basis (-Y FBX Front, +Z Up).
The resulting Unreal handedness mapping is Blender (X, -Y, Z) * 100 centimeters.
"""

import argparse
import hashlib
import json
from pathlib import Path
import sys

import bpy
from mathutils import Matrix
from io_scene_fbx import parse_fbx


ROOT = Path('C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts')
SOURCE_BLEND = ROOT / 'Backpack2_Separated.blend'
OUTPUT_DIR = ROOT / 'exports' / 'Assembly'
COLLECTION_NAME = 'BP2_Assembled'
RIG_NAME = 'BP2_Rig'
MATERIAL_NAMES = ('MI_SurvivalMan_Backpack_Inst', 'MI_SurvivalMan_Jacket_Inst')
PARTS = {
    'Backpack': (6623, 9526),
    'Knife': (214, 311),
    'Axe': (676, 1112),
    'Bottle': (508, 660),
    'Bottle_Holder': (124, 196),
    'Rope': (696, 1184),
    'Front_Pouch': (1025, 1623),
}
EXPORT_SETTINGS = {
    'use_selection': True, 'object_types': {'ARMATURE', 'MESH'},
    'global_scale': 1.0, 'apply_unit_scale': True,
    'apply_scale_options': 'FBX_SCALE_NONE',
    'axis_forward': 'Y', 'axis_up': 'Z',
    'use_space_transform': True, 'bake_space_transform': False,
    'use_mesh_modifiers': False, 'mesh_smooth_type': 'FACE',
    'use_mesh_edges': False, 'use_tspace': True, 'use_triangles': False,
    'primary_bone_axis': 'Y', 'secondary_bone_axis': 'X',
    'armature_nodetype': 'ROOT', 'use_armature_deform_only': False,
    'add_leaf_bones': False, 'bake_anim': False,
    'bake_anim_use_all_bones': False, 'bake_anim_use_nla_strips': False,
    'bake_anim_use_all_actions': False, 'bake_anim_force_startend_keying': False,
    'path_mode': 'STRIP', 'embed_textures': False, 'use_custom_props': False,
    'batch_mode': 'OFF',
}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def file_hash(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':'),
                                     allow_nan=False).encode()).hexdigest()


def mesh_snapshot(obj):
    mesh = obj.data
    return {
        'positions': [list(vertex.co) for vertex in mesh.vertices],
        'faces': [list(face.vertices) for face in mesh.polygons],
        'uv': {uv.name: [list(item.uv) for item in uv.data] for uv in mesh.uv_layers},
        'normals': [list(normal.vector) for normal in mesh.corner_normals],
        'weights': {group.name: [(vertex.index, item.weight)
                               for vertex in mesh.vertices for item in vertex.groups
                               if item.group == group.index and item.weight != 0.0]
                    for group in obj.vertex_groups},
        'material_indices': [face.material_index for face in mesh.polygons],
        'materials': [slot.material.name if slot.material else None for slot in obj.material_slots],
        'world_matrix': [list(row) for row in obj.matrix_world],
    }


def rig_snapshot(rig):
    return [{'name': bone.name, 'parent': bone.parent.name if bone.parent else 'root',
             'matrix_local': [list(row) for row in bone.matrix_local],
             'deform': bone.use_deform} for bone in rig.data.bones]


def element(parent, element_id, required=True):
    found = next((child for child in parent.elems if child.id == element_id), None)
    require(found is not None or not required, 'FBX element missing: ' + repr(element_id))
    return found


def fbx_array(parent, name, required=True):
    child = element(parent, name, required)
    return list(child.props[0]) if child is not None else []


def fbx_name(raw):
    return raw.split(b'\x00\x01', 1)[0].decode('utf-8')


def fbx_properties(parent):
    properties = element(parent, b'Properties70', required=False)
    return {p.props[0]: list(p.props[4:]) for p in properties.elems if p.id == b'P'} if properties else {}


def decode_indexed_vectors(layer, values_name, index_name, dimensions):
    raw = fbx_array(layer, values_name)
    vectors = [raw[index:index + dimensions] for index in range(0, len(raw), dimensions)]
    mode = element(layer, b'ReferenceInformationType').props[0]
    if mode == b'Direct':
        return vectors
    require(mode == b'IndexToDirect', 'Unexpected FBX indexed-vector mode')
    return [vectors[index] for index in fbx_array(layer, index_name)]


def audit_fbx(path, expected, expected_bones):
    tree, version = parse_fbx.parse(str(path))
    objects = element(tree, b'Objects')
    models = {item.props[0]: item for item in objects.elems if item.id == b'Model'}
    geometries = [item for item in objects.elems if item.id == b'Geometry' and item.props[2] == b'Mesh']
    require(len(geometries) == 1, 'FBX must contain exactly one mesh geometry')
    geometry = geometries[0]
    actual_positions = fbx_array(geometry, b'Vertices')
    require(actual_positions == [coordinate for vertex in expected['positions'] for coordinate in vertex],
            'FBX changed raw mesh vertex coordinates')
    actual_faces, current_face = [], []
    for index in fbx_array(geometry, b'PolygonVertexIndex'):
        current_face.append(-index - 1 if index < 0 else index)
        if index < 0:
            actual_faces.append(current_face)
            current_face = []
    require(actual_faces == expected['faces'], 'FBX face topology or winding changed')
    normal_layer = element(geometry, b'LayerElementNormal')
    require(element(normal_layer, b'MappingInformationType').props[0] == b'ByPolygonVertex', 'FBX normals are not per corner')
    actual_normals = decode_indexed_vectors(normal_layer, b'Normals', b'NormalsIndex', 3)
    require(actual_normals == expected['normals'], 'FBX corner normals are not bit-exact')
    uv_layers = [item for item in geometry.elems if item.id == b'LayerElementUV']
    require(len(uv_layers) == len(expected['uv']), 'FBX UV layer count changed')
    for layer in uv_layers:
        name = element(layer, b'Name').props[0].decode('utf-8')
        require(name in expected['uv'], 'Unexpected UV layer in FBX: ' + name)
        actual_uv = decode_indexed_vectors(layer, b'UV', b'UVIndex', 2)
        require(actual_uv == expected['uv'][name], 'FBX UV coordinates changed')
    material_layer = element(geometry, b'LayerElementMaterial')
    indices = fbx_array(material_layer, b'Materials')
    if element(material_layer, b'MappingInformationType').props[0] == b'AllSame':
        indices = indices * len(actual_faces)
    require(indices == expected['material_indices'], 'FBX per-face material indices changed')
    material_names = [fbx_name(item.props[1]) for item in objects.elems if item.id == b'Material']
    require(set(material_names) == set(MATERIAL_NAMES), 'FBX original material names changed')
    bone_models = {fbx_name(item.props[1]): item for item in models.values()
                   if item.props[2] in {b'Root', b'LimbNode'}}
    require(set(bone_models) == {'root'} | {item['name'] for item in expected_bones},
            'FBX does not contain exactly the original 101 skeleton nodes')
    require(bone_models['root'].props[2] == b'Root', 'FBX armature object is not the real root node')
    connections = element(tree, b'Connections')
    model_parents = {item.props[1]: item.props[2] for item in connections.elems
                     if item.id == b'C' and item.props[0] == b'OO'
                     and item.props[1] in models and (item.props[2] in models or item.props[2] == 0)}
    require(model_parents.get(bone_models['root'].props[0]) == 0, 'FBX root has an unexpected parent')
    for bone in expected_bones:
        require(model_parents.get(bone_models[bone['name']].props[0]) == bone_models[bone['parent']].props[0],
                'FBX bone parent changed: ' + bone['name'])
    skins = [item for item in objects.elems if item.id == b'Deformer' and item.props[2] == b'Skin']
    clusters = [item for item in objects.elems if item.id == b'Deformer' and item.props[2] == b'Cluster']
    require(len(skins) == 1 and len(clusters) == len(expected_bones), 'FBX skin/full-bone bind clusters are incomplete')
    for cluster in clusters:
        bone_name = fbx_name(cluster.props[1])
        actual_weights = list(zip(fbx_array(cluster, b'Indexes', required=False),
                                  fbx_array(cluster, b'Weights', required=False)))
        require(actual_weights == expected['weights'].get(bone_name, []), 'FBX skin weights changed: ' + bone_name)
    require(not any(item.id.startswith(b'Animation') for item in objects.elems), 'Unexpected animation data in FBX')
    tangent_layer = element(geometry, b'LayerElementTangent')
    binormal_layer = element(geometry, b'LayerElementBinormal')
    corner_count = sum(len(face) for face in actual_faces)
    require(len(fbx_array(tangent_layer, b'Tangents')) == corner_count * 3, 'FBX tangents are incomplete')
    require(len(fbx_array(binormal_layer, b'Binormals')) == corner_count * 3, 'FBX binormals are incomplete')
    settings = fbx_properties(element(tree, b'GlobalSettings'))
    require(settings[b'UnitScaleFactor'] == [1.0], 'FBX physical units must be centimeters')
    expected_axes = {b'UpAxis': 2, b'UpAxisSign': 1, b'FrontAxis': 1,
                     b'FrontAxisSign': -1, b'CoordAxis': 0, b'CoordAxisSign': 1}
    require(all(settings[name] == [value] for name, value in expected_axes.items()), 'FBX coordinate basis changed')
    root_properties = fbx_properties(bone_models['root'])
    root_scale = root_properties.get(b'Lcl Scaling', [1.0, 1.0, 1.0])
    require(max(abs(value - 1.0) for value in root_scale) < 0.00001, 'FBX root is not unit scale in centimeters')
    return {'file': str(path), 'sha256': file_hash(path), 'fbx_version': version,
            'vertices': len(actual_positions) // 3, 'faces': len(actual_faces), 'corners': corner_count,
            'bones_including_root': len(bone_models), 'skin_clusters': len(clusters),
            'root_scale': root_scale, 'unit_scale_factor': 1.0,
            'materials': material_names, 'normals_uv_weights_positions_topology_exact': True,
            'tangents_and_binormals_present': True, 'animations': 0}


def export_parts(source_path=SOURCE_BLEND, output_dir=OUTPUT_DIR):
    require(bpy.app.background and not bpy.data.filepath,
            'Run in a fresh --background --factory-startup Blender process; never in the live source scene')
    require(bpy.app.version >= (5, 2, 0), 'This exact FLOAT_VECTOR normal export requires Blender 5.2+')
    source_path, output_dir = Path(source_path).resolve(), Path(output_dir).resolve()
    require(source_path.is_file(), 'Separated source .blend is missing')
    source_sha256 = file_hash(source_path)
    filenames = {name: output_dir / ('SKM_Backpack2_' + name + '.fbx') for name in PARTS}
    report_path = output_dir / 'export_manifest.json'
    require(not report_path.exists() and not any(path.exists() for path in filenames.values()),
            'Assembly export output already exists; refusing to overwrite it')
    require(bpy.data.collections.get(COLLECTION_NAME) is None and bpy.data.objects.get('root') is None,
            'Export process contains conflicting source/rig objects')
    with bpy.data.libraries.load(str(source_path), link=False) as (available, copies):
        require(COLLECTION_NAME in available.collections, 'Assembled source collection is missing')
        require(set(MATERIAL_NAMES).issubset(available.materials), 'Original material datablocks are missing')
        copies.collections = [COLLECTION_NAME]
        copies.materials = list(MATERIAL_NAMES)
    collection = copies.collections[0]
    bpy.context.scene.collection.children.link(collection)
    rig = next((item for item in collection.objects if item.name == RIG_NAME), None)
    require(rig is not None and rig.type == 'ARMATURE' and len(rig.data.bones) == 100,
            'Expected full 100-bone source armature is missing')
    require(rig.data.bones.get('root') is None, 'Source root representation changed; review export hierarchy')
    require(len(rig.constraints) == 0 and all(len(bone.constraints) == 0 for bone in rig.pose.bones),
            'Unexpected rig constraints require explicit rest-pose evaluation')
    # Parent Empty carries source .01 scale. Link dependencies before reading the
    # world matrix, then detach ONLY the imported rig copy with that matrix kept.
    parent = rig.parent
    while parent:
        if parent.name not in bpy.context.scene.objects:
            bpy.context.scene.collection.objects.link(parent)
        parent = parent.parent
    bpy.context.view_layer.update()
    rig_world = rig.matrix_world.copy()
    expected_bones = rig_snapshot(rig)
    rig_hash = digest(expected_bones)
    rig.parent = None
    rig.matrix_world = rig_world
    rig.name = 'root'
    rig.hide_viewport = False
    rig.hide_set(False)
    rig.animation_data_clear()
    rig.data.pose_position = 'REST'
    for bone in rig.pose.bones:
        bone.matrix_basis = Matrix.Identity(4)
    bpy.context.scene.unit_settings.system = 'METRIC'
    bpy.context.scene.unit_settings.scale_length = 1.0
    bpy.context.view_layer.update()
    sources = {}
    for part, counts in PARTS.items():
        obj = next((item for item in collection.objects if item.name == 'BP2_' + part), None)
        require(obj is not None and obj.type == 'MESH', 'Assembled part missing: ' + part)
        require((len(obj.data.vertices), len(obj.data.polygons)) == counts, 'Part geometry counts changed: ' + part)
        require(obj.parent == rig and len(obj.modifiers) == 1 and obj.modifiers[0].type == 'ARMATURE'
                and obj.modifiers[0].object == rig and obj.modifiers[0].use_vertex_groups,
                'Part no longer has the expected skin binding: ' + part)
        require(all(len(face.vertices) == 3 for face in obj.data.polygons), 'Unexpected non-triangle geometry')
        require(obj.data.attributes.get('custom_normal') is not None
                and obj.data.attributes['custom_normal'].data_type == 'FLOAT_VECTOR',
                'Exact source corner-normal storage is missing')
        require(len(obj.material_slots) == len(MATERIAL_NAMES), 'Unexpected source material slot layout')
        for index, material in enumerate(copies.materials):
            obj.material_slots[index].link = 'DATA'
            obj.data.materials[index] = material
        obj.name = 'SKM_Backpack2_' + part
        obj.hide_viewport = False
        obj.hide_set(False)
        obj.animation_data_clear()
        snapshot = mesh_snapshot(obj)
        used_groups = {name for name, weights in snapshot['weights'].items() if weights}
        require(used_groups.issubset(rig.data.bones.keys()), 'Mesh has weights outside the full original rig')
        sources[part] = (obj, snapshot)
    reference_path = ROOT / 'inspection' / 'unreal_source_skeleton.json'
    if reference_path.exists():
        reference_bones = json.loads(reference_path.read_text(encoding='utf-8-sig'))['bones']
        require({item['name'] for item in reference_bones} == {'root'} | {item['name'] for item in expected_bones},
                'Export rig names do not match the original Unreal skeletal mesh')
        hierarchy = {item['name']: item['parent'] for item in reference_bones}
        require(all(hierarchy[item['name']] == item['parent'] for item in expected_bones),
                'Export rig hierarchy does not match the original Unreal skeletal mesh')
    output_dir.mkdir(parents=True, exist_ok=True)
    exported = []
    for part, (obj, snapshot) in sources.items():
        for selected in list(bpy.context.selected_objects):
            selected.select_set(False)
        rig.select_set(True)
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        require(not filenames[part].exists(), 'Export destination appeared during export')
        result = bpy.ops.export_scene.fbx(filepath=str(filenames[part]), check_existing=False, **EXPORT_SETTINGS)
        require('FINISHED' in result and filenames[part].is_file(), 'Native FBX export failed: ' + part)
        require(digest(mesh_snapshot(obj)) == digest(snapshot), 'Native FBX export changed source-copy mesh data')
        exported.append(audit_fbx(filenames[part], snapshot, expected_bones))
    require(digest(rig_snapshot(rig)) == rig_hash, 'Export changed the copied rest skeleton')
    require(file_hash(source_path) == source_sha256, 'Separated source .blend changed during export')
    report = {'source_blend': str(source_path), 'source_sha256_before': source_sha256,
              'source_sha256_after': file_hash(source_path), 'source_unchanged': True,
              'blender_version': bpy.app.version_string, 'export_collection': COLLECTION_NAME,
              'export_settings': {key: sorted(value) if isinstance(value, set) else value
                                  for key, value in EXPORT_SETTINGS.items()},
              'unreal_coordinate_mapping': '(Blender X, -Y, Z) * 100 cm',
              'unreal_recommended_import': {'convert_scene': True, 'force_front_x_axis': False,
                                            'convert_scene_unit': False, 'import_uniform_scale': 1.0,
                                            'import_normals_and_tangents': True,
                                            'import_animations': False, 'update_skeleton_reference_pose': False},
              'files': exported}
    with report_path.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2, allow_nan=False)
    print(json.dumps({'export_manifest': str(report_path), 'source_unchanged': True,
                      'files': exported}, allow_nan=False))
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', default=str(SOURCE_BLEND))
    parser.add_argument('--output-dir', default=str(OUTPUT_DIR))
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    export_parts(args.source, args.output_dir)
