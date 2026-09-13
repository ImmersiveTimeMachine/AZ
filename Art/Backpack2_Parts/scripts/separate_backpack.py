"""Preserving, manifest-driven native Blender separation of backpack2.

Loading this file only defines the builder. Execute explicitly in Blender:

    exec(compile(Path(SCRIPT).read_text(), SCRIPT, 'exec'))
    report = build_separated(ROOT / 'inspection' / 'logical_parts.json')

The manifest is JSON (or a Python dictionary) with this schema:
    {"schema_version": 1,
     "parts": {"BP2_Backpack": [0, 1, ...], "BP2_Knife": [...], ...}}

Every source polygon index must occur exactly once. Names must begin BP2_.
This builder performs no inferred classification, mesh cleanup or remeshing.
It writes only Backpack2_Separated.blend, with the manifest and integrity
report embedded as Blender Text datablocks. Existing outputs are never replaced.
Source ids remain on parts as POINT/FACE/CORNER integer attributes so every
output element can be traced back to the archived source.

The working duplicate stores original evaluated corner normals in Blender 5.2's
native FLOAT_VECTOR custom_normal attribute before separation. This avoids the
lossy normal-fan re-encoding of imported INT16_2D packed normals. Original packed
data remains untouched on the archived/source mesh. Normals, UVs, topology,
weights and per-face material indices must remain exact.
"""

from collections import Counter
import hashlib
import json
import math
from pathlib import Path

import bpy
import bmesh


ROOT = Path('C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts')
SOURCE_NAME = 'SKM_SurvivalMan_backpack2'
ARCHIVE = ROOT / 'Backpack2_Source_Archive.blend'
OUTPUT = ROOT / 'Backpack2_Separated.blend'
ARCHIVE_SHA256 = 'ed9b577fb311ed4a53984f37c59d8928b4ce59b8817844c12d3377e8e0482fda'
SNAPSHOTS = {
    'source_geometry.json': '3f3778ae2852f9ebbae30657a2f974b90aa88bd91fb9a415e39faf861944d179',
    'source_topology.json': '3ba30b7d94abe57b84332aa0c9d0f6b8c87a90e265e881aeba674e9961686fa9',
    'source_weights.json': 'f107d43ad58bb91bcce45770754032396fc3ad86e466416cf8891befffe8a513',
}
ASSEMBLED_COLLECTION = 'BP2_Assembled'
SOURCE_COLLECTION = 'BP2_Source_Original'
RIG_NAME = 'BP2_Rig'
FACE_ID = 'bp2_source_face_id'
VERTEX_ID = 'bp2_source_vertex_id'
CORNER_ID = 'bp2_source_corner_id'
REPORT_TEXT = 'BP2_Separation_Report.json'
MAPPING_TEXT = 'BP2_Part_Mapping.json'
OWNER = 'AZ_Backpack2_Separation_v2'
NORMAL_TOLERANCE = 0.0
WORLD_TOLERANCE = 0.0000001


def require(condition, message):
    # Deliberately remains active under Python -O; these are asset save guards.
    if not condition:
        raise RuntimeError(message)


def sha256_file(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def digest(value):
    return hashlib.sha256(json.dumps(value, separators=(',', ':'),
                                     sort_keys=True, allow_nan=False).encode()).hexdigest()


def vector(value):
    return tuple(float(component) for component in value)


def matrix(value):
    return tuple(vector(row) for row in value)


def normals(mesh):
    if hasattr(mesh, 'calc_normals_split'):
        mesh.calc_normals_split()
    if hasattr(mesh, 'corner_normals'):
        return [vector(normal.vector) for normal in mesh.corner_normals]
    return [vector(loop.normal) for loop in mesh.loops]


def set_exact_corner_normals(mesh, expected):
    """Write Blender's native vector normal storage on a WORKING mesh only.

    normals_split_custom_set() uses packed INT16_2D fan-relative storage and can
    change imported vectors by >0.0004 when topology is separated. A CORNER
    FLOAT_VECTOR custom_normal is consumed directly by Blender 5.2 and survives
    native Edit Mode separation without quantization or changes to sharp edges.
    """
    require(bpy.app.version >= (5, 2, 0), 'Exact vector-normal workflow requires Blender 5.2 or newer')
    require(mesh != bpy.data.objects[SOURCE_NAME].data, 'Never replace normals on the original source mesh')
    require(len(expected) == len(mesh.loops), 'Normal vector count does not match working mesh corners')
    attribute = mesh.attributes.get('custom_normal')
    if attribute is not None and (attribute.data_type != 'FLOAT_VECTOR' or attribute.domain != 'CORNER'):
        mesh.attributes.remove(attribute)
        attribute = None
    if attribute is None:
        attribute = mesh.attributes.new(name='custom_normal', type='FLOAT_VECTOR', domain='CORNER')
    attribute.data.foreach_set('vector', [component for normal in expected for component in normal])
    mesh.update()
    require(normals(mesh) == expected, 'Blender did not preserve the exact float corner-normal vectors')


def mesh_state(obj):
    mesh = obj.data
    return {
        'positions': [vector(v.co) for v in mesh.vertices],
        'world_positions': [vector(obj.matrix_world @ v.co) for v in mesh.vertices],
        'edges': [tuple(edge.vertices) for edge in mesh.edges],
        'faces': [tuple(face.vertices) for face in mesh.polygons],
        'face_corners': [tuple(face.loop_indices) for face in mesh.polygons],
        'material_indices': [face.material_index for face in mesh.polygons],
        'smooth': [face.use_smooth for face in mesh.polygons],
        'uv': {layer.name: [vector(item.uv) for item in layer.data]
               for layer in mesh.uv_layers},
        'uv_active': mesh.uv_layers.active_index,
        'uv_render': [layer.active_render for layer in mesh.uv_layers],
        'normals': normals(mesh),
        'weights': [tuple((item.group, float(item.weight)) for item in vertex.groups)
                    for vertex in mesh.vertices],
        'groups': [group.name for group in obj.vertex_groups],
        'materials': [slot.material.name if slot.material else None
                      for slot in obj.material_slots],
        'material_links': [slot.link for slot in obj.material_slots],
        'matrix_world': matrix(obj.matrix_world),
    }


def validate_mapping(manifest, face_count):
    require(isinstance(manifest, dict) and manifest.get('schema_version') == 1,
            'Expected manifest schema_version: 1')
    parts = manifest.get('parts')
    require(isinstance(parts, dict) and bool(parts), 'Manifest parts must be a nonempty object')
    coverage = Counter()
    for name, face_ids in parts.items():
        require(isinstance(name, str) and name.startswith('BP2_') and len(name) <= 63,
                'Each part name must start BP2_ and be at most 63 characters')
        require(name != RIG_NAME, 'Part name conflicts with the working rig')
        require(isinstance(face_ids, list) and bool(face_ids), name + ' has no face list')
        require(all(type(index) is int and 0 <= index < face_count for index in face_ids),
                name + ' contains an invalid source polygon index')
        coverage.update(face_ids)
    require(set(coverage) == set(range(face_count)), 'Face mapping does not cover the complete source')
    require(all(count == 1 for count in coverage.values()), 'Face mapping assigns a source face more than once')
    return {name: set(face_ids) for name, face_ids in parts.items()}


def id_attribute(mesh, name, domain, count):
    require(mesh.attributes.get(name) is None, 'Reserved source id attribute already exists: ' + name)
    attribute = mesh.attributes.new(name=name, type='INT', domain=domain)
    attribute.data.foreach_set('value', list(range(count)))


def ids(mesh, name, domain):
    attribute = mesh.attributes.get(name)
    require(attribute is not None and attribute.domain == domain and attribute.data_type == 'INT',
            'Native separation lost a source id attribute: ' + name)
    return [item.value for item in attribute.data]


def activate_only(obj):
    for selected in list(bpy.context.selected_objects):
        selected.select_set(False)
    obj.hide_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def separate_selected_faces(working, selected_source_faces):
    activate_only(working)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='DESELECT')
    bm = bmesh.from_edit_mesh(working.data)
    layer = bm.faces.layers.int.get(FACE_ID)
    require(layer is not None, 'Source face ids are unavailable in Edit Mode')
    bm.select_mode = {'FACE'}
    for face in bm.faces:
        face.select_set(face[layer] in selected_source_faces)
    bm.select_flush_mode()
    bmesh.update_edit_mesh(working.data, loop_triangles=False, destructive=False)
    before = set(bpy.data.objects)
    result = bpy.ops.mesh.separate(type='SELECTED')
    require('FINISHED' in result, 'Native selected-face separation did not finish')
    bpy.ops.object.mode_set(mode='OBJECT')
    created = set(bpy.data.objects) - before
    require(len(created) == 1, 'Expected exactly one separated object')
    separated = created.pop()
    require(separated.type == 'MESH', 'Separated output is not a mesh')
    require(set(ids(separated.data, FACE_ID, 'FACE')) == selected_source_faces,
            'Native separation selected a different set of faces')
    return separated


def remap_rig_references(obj, old_rig, new_rig):
    if obj.parent == old_rig:
        world = obj.matrix_world.copy()
        obj.parent = new_rig
        obj.matrix_world = world
    for modifier in obj.modifiers:
        if modifier.type == 'ARMATURE' and modifier.object == old_rig:
            modifier.object = new_rig
    constraints = list(obj.constraints)
    if obj.type == 'ARMATURE':
        constraints += [constraint for bone in obj.pose.bones for constraint in bone.constraints]
    for constraint in constraints:
        if hasattr(constraint, 'target') and constraint.target == old_rig:
            constraint.target = new_rig


def verify_part(obj, expected_faces, source, world_matrix):
    mesh = obj.data
    face_ids = ids(mesh, FACE_ID, 'FACE')
    vertex_ids = ids(mesh, VERTEX_ID, 'POINT')
    corner_ids = ids(mesh, CORNER_ID, 'CORNER')
    require(len(face_ids) == len(set(face_ids)) and set(face_ids) == expected_faces,
            obj.name + ': face coverage changed')
    require(len(vertex_ids) == len(set(vertex_ids)), obj.name + ': native separation duplicated an internal vertex')
    require(len(corner_ids) == len(set(corner_ids)), obj.name + ': duplicate source corners')
    require([group.name for group in obj.vertex_groups] == source['groups'], obj.name + ': vertex groups changed')
    require([slot.material.name if slot.material else None for slot in obj.material_slots] == source['materials'],
            obj.name + ': material slots changed')
    require([slot.link for slot in obj.material_slots] == source['material_links'], obj.name + ': material linking changed')
    require([layer.name for layer in mesh.uv_layers] == list(source['uv']), obj.name + ': UV layers changed')
    require(mesh.uv_layers.active_index == source['uv_active'], obj.name + ': active UV layer changed')
    require([layer.active_render for layer in mesh.uv_layers] == source['uv_render'], obj.name + ': render UV layer changed')
    require(max(abs(obj.matrix_world[row][column] - world_matrix[row][column])
                for row in range(4) for column in range(4)) <= WORLD_TOLERANCE,
            obj.name + ': assembled matrix changed')
    max_world_error = 0.0
    for vertex, original_index in zip(mesh.vertices, vertex_ids):
        require(0 <= original_index < len(source['positions']), obj.name + ': invalid vertex provenance')
        require(vector(vertex.co) == source['positions'][original_index], obj.name + ': vertex coordinate changed')
        weights = tuple((item.group, float(item.weight)) for item in vertex.groups)
        require(weights == source['weights'][original_index], obj.name + ': vertex weights changed')
        error = max(abs(a - b) for a, b in zip(obj.matrix_world @ vertex.co,
                                              source['world_positions'][original_index]))
        max_world_error = max(max_world_error, error)
    require(max_world_error <= WORLD_TOLERANCE, obj.name + ': assembled world geometry moved')
    for polygon, original_index in zip(mesh.polygons, face_ids):
        require(tuple(vertex_ids[index] for index in polygon.vertices) == source['faces'][original_index],
                obj.name + ': face topology/winding changed')
        require(tuple(corner_ids[index] for index in polygon.loop_indices) == source['face_corners'][original_index],
                obj.name + ': face-corner order changed')
        require(polygon.material_index == source['material_indices'][original_index], obj.name + ': material assignment changed')
        require(polygon.use_smooth == source['smooth'][original_index], obj.name + ': smooth shading flags changed')
    for uv_layer in mesh.uv_layers:
        for index, uv in enumerate(uv_layer.data):
            require(vector(uv.uv) == source['uv'][uv_layer.name][corner_ids[index]],
                    obj.name + ': UV coordinate changed')
    expected_normals = [source['normals'][index] for index in corner_ids]
    actual_normals = normals(mesh)
    require(len(actual_normals) == len(expected_normals), obj.name + ': corner normals missing')
    before_error = max((abs(a - b) for expected, actual in zip(expected_normals, actual_normals)
                        for a, b in zip(expected, actual)), default=0.0)
    restored = before_error != 0.0
    if restored:
        set_exact_corner_normals(mesh, expected_normals)
        actual_normals = normals(mesh)
    normal_error = max((abs(a - b) for expected, actual in zip(expected_normals, actual_normals)
                        for a, b in zip(expected, actual)), default=0.0)
    require(normal_error <= NORMAL_TOLERANCE,
            obj.name + ': corner normals are not exact; max component error=' + str(normal_error))
    return {
        'object': obj.name, 'vertices': len(mesh.vertices), 'faces': len(mesh.polygons),
        'corners': len(mesh.loops), 'source_face_ids_sha256': digest(sorted(face_ids)),
        'source_vertex_ids_sha256': digest(sorted(vertex_ids)),
        'material_faces': dict(Counter(p.material_index for p in mesh.polygons)),
        'uv_sums': {layer.name: [math.fsum(item.uv[axis] for item in layer.data)
                                for axis in range(2)] for layer in mesh.uv_layers},
        'vertex_weight_sum': math.fsum(item.weight for vertex in mesh.vertices for item in vertex.groups),
        'corner_weight_sum': math.fsum(item.weight for loop in mesh.loops for item in mesh.vertices[loop.vertex_index].groups),
        'normal_sums': [math.fsum(normal[axis] for normal in actual_normals) for axis in range(3)],
        'source_normal_sums': [math.fsum(normal[axis] for normal in expected_normals) for axis in range(3)],
        'normal_restore_required': restored, 'native_normal_max_component_error': before_error,
        'normal_max_component_error': normal_error, 'normals_bit_exact': normal_error == 0.0,
        'normal_storage': mesh.attributes['custom_normal'].data_type,
        'world_max_component_error': max_world_error,
        'uv_weights_materials_topology_exact': True,
    }


def build_separated(mapping_path=None):
    """Build once from the archived source's current unchanged live object.

    mapping_path may also be the manifest dictionary. Failure never saves the
    scene. Working objects are removed on failure and source collection links,
    visibility and initial Edit Mode are restored. The immutable archive remains
    the recovery checkpoint for Blender-level/operator failures.
    """
    require(ARCHIVE.is_file() and sha256_file(ARCHIVE) == ARCHIVE_SHA256,
            'Intact source archive must be verified before changing Edit Mode')
    require(not OUTPUT.exists(), 'Separated output already exists; refusing to overwrite it')
    require(Path(bpy.data.filepath).resolve() != OUTPUT.resolve(), 'Separated file is already active')
    snapshots = {}
    for name, expected_hash in SNAPSHOTS.items():
        path = ROOT / 'inspection' / name
        require(path.is_file() and sha256_file(path) == expected_hash, 'Source inspection snapshot changed: ' + name)
        snapshots[name] = json.loads(path.read_text(encoding='utf-8-sig'))
    geometry = snapshots['source_geometry.json']
    topology = snapshots['source_topology.json']
    weights = snapshots['source_weights.json']
    manifest_input = mapping_path if mapping_path is not None else ROOT / 'inspection' / 'logical_parts.json'
    manifest = manifest_input if isinstance(manifest_input, dict) else json.loads(Path(manifest_input).read_text(encoding='utf-8-sig'))
    parts = validate_mapping(manifest, 14612)
    for name in (ASSEMBLED_COLLECTION, SOURCE_COLLECTION):
        existing = bpy.data.collections.get(name)
        require(existing is None, 'Output collection already exists; no replacement is permitted: ' + name)
    for name in (*parts, RIG_NAME):
        require(bpy.data.objects.get(name) is None, 'Output object name already exists: ' + name)
    for name in (REPORT_TEXT, MAPPING_TEXT):
        require(bpy.data.texts.get(name) is None, 'Output provenance Text already exists: ' + name)
    obj = bpy.data.objects.get(SOURCE_NAME)
    require(obj is not None and obj.type == 'MESH', 'Expected original source mesh is missing')
    require(obj.name in bpy.context.view_layer.objects, 'Source is not in the active view layer')
    require(obj.mode in {'EDIT', 'OBJECT'}, 'Source must be in Object or Edit Mode')
    require(bpy.context.mode == 'OBJECT' or
            (obj.mode == 'EDIT' and all(item == obj for item in bpy.context.objects_in_mode)),
            'Another object is currently in Edit/Pose Mode')
    require(obj.data.shape_keys is None, 'Shape keys require a separate preservation workflow')
    rig = obj.parent
    require(rig is not None and rig.type == 'ARMATURE', 'Expected source parent armature is missing')
    require(all(modifier.object == rig for modifier in obj.modifiers if modifier.type == 'ARMATURE'),
            'Source uses an additional armature that is not accounted for')
    initial_mode = obj.mode
    initial_active = bpy.context.view_layer.objects.active
    initial_selection = list(bpy.context.selected_objects)
    initial_select_mode = tuple(bpy.context.tool_settings.mesh_select_mode)
    originals = {item: {'collections': list(item.users_collection),
                        'hide': item.hide_get(), 'render': item.hide_render,
                        'viewport': item.hide_viewport} for item in (obj, rig)}
    original_objects = set(bpy.data.objects)
    original_meshes = set(bpy.data.meshes)
    original_armatures = set(bpy.data.armatures)
    created_collections = []
    created_texts = []
    try:
        if obj.mode == 'EDIT':
            obj.update_from_editmode()
            bpy.ops.object.mode_set(mode='OBJECT')
        bpy.context.view_layer.update()
        mesh = obj.data
        require((len(mesh.vertices), len(mesh.polygons)) == (9866, 14612), 'Original source counts changed')
        source = mesh_state(obj)
        source_hash = digest(source)
        require([list(item) for item in source['positions']] == geometry['local_positions'], 'Source positions differ from the archive snapshot')
        require([list(item) for item in source['edges']] == geometry['edges'], 'Source edges differ from the archive snapshot')
        require([list(item) for item in source['faces']] == geometry['polygons'], 'Source topology differs from the archive snapshot')
        require(source['material_indices'] == geometry['materials'], 'Source material assignments changed')
        require([list(row) for row in source['matrix_world']] == topology['matrix_world'], 'Source world transform changed')
        require(source['groups'] == weights['groups'], 'Source vertex group names changed')
        require([[list(item) for item in row] for row in source['weights']] == weights['weights'], 'Source vertex weights changed')
        require(list(source['uv']) == ['DiffuseUV'] and len(source['materials']) == 2, 'Unexpected source UV/material layout')
        require(set(vertex for face in source['faces'] for vertex in face) == set(range(len(mesh.vertices))),
                'Source has loose vertices outside face mapping; explicit handling is required')
        face_edges = {tuple(sorted(edge)) for face in source['faces']
                      for edge in zip(face, face[1:] + face[:1])}
        require(all(tuple(sorted(edge)) in face_edges for edge in source['edges']),
                'Source has loose edges outside face mapping; explicit handling is required')
        assembled = bpy.data.collections.new(ASSEMBLED_COLLECTION)
        created_collections.append(assembled)
        assembled['owner'] = OWNER
        bpy.context.scene.collection.children.link(assembled)
        working_rig = rig.copy()
        working_rig.data = rig.data.copy()
        working_rig.name = RIG_NAME
        working_rig.data.name = RIG_NAME + '_Armature'
        assembled.objects.link(working_rig)
        working_rig.hide_viewport = False
        working_rig.hide_render = False
        working_rig.hide_set(False)
        remap_rig_references(working_rig, rig, working_rig)
        working = obj.copy()
        working.data = mesh.copy()
        working.name = 'BP2_Working_Remaining'
        assembled.objects.link(working)
        working.hide_viewport = False
        working.hide_render = False
        working.hide_set(False)
        remap_rig_references(working, rig, working_rig)
        id_attribute(working.data, VERTEX_ID, 'POINT', len(mesh.vertices))
        id_attribute(working.data, FACE_ID, 'FACE', len(mesh.polygons))
        id_attribute(working.data, CORNER_ID, 'CORNER', len(mesh.loops))
        set_exact_corner_normals(working.data, source['normals'])
        bpy.context.tool_settings.mesh_select_mode = (False, False, True)
        built_parts = {}
        ordered = list(parts.items())
        for index, (name, face_ids) in enumerate(ordered):
            part = separate_selected_faces(working, face_ids) if index < len(ordered) - 1 else working
            part.name = name
            part.data.name = name + '_Mesh'
            part['bp2_source_object'] = SOURCE_NAME
            part['bp2_source_archive_sha256'] = ARCHIVE_SHA256
            part['bp2_logical_part'] = name
            built_parts[name] = part
        bpy.context.view_layer.update()
        part_reports = [verify_part(part, parts[name], source, obj.matrix_world)
                        for name, part in built_parts.items()]
        coverage = Counter(index for part in built_parts.values() for index in ids(part.data, FACE_ID, 'FACE'))
        require(coverage == Counter(range(14612)), 'Combined output face coverage is not exactly once')
        require(digest(mesh_state(obj)) == source_hash, 'Original source mesh or transform changed during separation')
        require(sha256_file(ARCHIVE) == ARCHIVE_SHA256, 'Source archive changed during separation')
        source_collection = bpy.data.collections.new(SOURCE_COLLECTION)
        created_collections.append(source_collection)
        source_collection['owner'] = OWNER
        source_collection['original_collection_links'] = json.dumps({item.name: [collection.name for collection in saved['collections']]
                                                                    for item, saved in originals.items()})
        source_collection['original_visibility'] = json.dumps({item.name: {key: saved[key] for key in ('hide', 'render', 'viewport')}
                                                              for item, saved in originals.items()})
        bpy.context.scene.collection.children.link(source_collection)
        for original in originals:
            source_collection.objects.link(original)
            for collection in list(original.users_collection):
                if collection != source_collection:
                    collection.objects.unlink(original)
        source_collection.hide_viewport = True
        source_collection.hide_render = True
        report = {
            'schema_version': 1, 'builder': OWNER, 'blender_version': bpy.app.version_string,
            'source_object': SOURCE_NAME, 'source_archive': str(ARCHIVE), 'archive_sha256': ARCHIVE_SHA256,
            'source_state_sha256_before': source_hash, 'source_state_sha256_after': digest(mesh_state(obj)),
            'mapping_sha256': digest(manifest), 'output': str(OUTPUT), 'source_vertices': len(mesh.vertices),
            'source_faces': len(mesh.polygons), 'source_corners': len(mesh.loops),
            'source_uv_sums': {name: [math.fsum(uv[axis] for uv in values) for axis in range(2)]
                               for name, values in source['uv'].items()},
            'source_vertex_weight_sum': math.fsum(value for row in source['weights'] for _, value in row),
            'source_corner_weight_sum': math.fsum(value for loop in mesh.loops for _, value in source['weights'][loop.vertex_index]),
            'source_normal_sums': [math.fsum(normal[axis] for normal in source['normals']) for axis in range(3)],
            'all_faces_present_exactly_once': True, 'source_geometry_unchanged': True,
            'normal_component_tolerance': NORMAL_TOLERANCE, 'world_component_tolerance': WORLD_TOLERANCE,
            'normal_storage_strategy': 'Original corner vectors stored as native FLOAT_VECTOR custom_normal on working parts',
            'assembled_collection': ASSEMBLED_COLLECTION, 'hidden_source_collection': SOURCE_COLLECTION,
            'parts': part_reports,
        }
        require(report['source_state_sha256_before'] == report['source_state_sha256_after'], 'Source state changed while organizing collections')
        for name, payload in ((REPORT_TEXT, report), (MAPPING_TEXT, manifest)):
            text_block = bpy.data.texts.new(name)
            created_texts.append(text_block)
            text_block.write(json.dumps(payload, indent=2, allow_nan=False))
        activate_only(next(iter(built_parts.values())))
        for part in built_parts.values():
            part.select_set(True)
        # Hidden in this view only; modifiers can still evaluate the working rig.
        working_rig.hide_set(True)
        bpy.context.tool_settings.mesh_select_mode = initial_select_mode
        bpy.context.view_layer.update()
        require(not OUTPUT.exists(), 'Separated output appeared during the build; refusing to overwrite it')
        result = bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT), check_existing=False)
        require('FINISHED' in result and OUTPUT.is_file(), 'Saving the separated file failed')
        print(json.dumps(report, allow_nan=False))
        return report
    except Exception:
        if bpy.context.object and bpy.context.object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')
        for original, saved in originals.items():
            for collection in saved['collections']:
                if original.name not in collection.objects:
                    collection.objects.link(original)
            for collection in list(original.users_collection):
                if collection not in saved['collections']:
                    collection.objects.unlink(original)
            original.hide_viewport = saved['viewport']
            original.hide_render = saved['render']
            original.hide_set(saved['hide'])
        for created in set(bpy.data.objects) - original_objects:
            bpy.data.objects.remove(created, do_unlink=True)
        for created in set(bpy.data.meshes) - original_meshes:
            if created.users == 0:
                bpy.data.meshes.remove(created)
        for created in set(bpy.data.armatures) - original_armatures:
            if created.users == 0:
                bpy.data.armatures.remove(created)
        for created in created_collections:
            bpy.data.collections.remove(created)
        for created in created_texts:
            bpy.data.texts.remove(created)
        bpy.context.tool_settings.mesh_select_mode = initial_select_mode
        for selected in list(bpy.context.selected_objects):
            selected.select_set(False)
        for selected in initial_selection:
            selected.select_set(True)
        bpy.context.view_layer.objects.active = initial_active
        if initial_mode == 'EDIT':
            bpy.context.view_layer.objects.active = obj
            obj.select_set(True)
            bpy.ops.object.mode_set(mode='EDIT')
        raise
