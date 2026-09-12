# @Description: Extract the real Clip_Bone pistol magazine into an owned pickup mesh.
"""Guarded audit/author/verify. Importing this module changes no assets.

PistolB_Ammo is a cartridge casing, not a magazine. This helper copies Pistols_B
LOD0 into a transient DynamicMesh, retains only rigid Clip_Bone triangles,
preserves their UVs/material, and centers their original geometry without scale
or rotation. Only explicit author mode writes the new owned StaticMesh package.
It never edits pickup Blueprints/actors, source meshes, skeletons, animations,
PIE, tests or native code. Root owns updating pickup definitions afterward.
"""
import gc
import hashlib
import json
import math
import shutil
from datetime import datetime, timezone
from pathlib import Path

import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
OUTPUT = ROOT / 'Saved/PistolMagazineMesh'
SOURCE = '/Game/MilitaryWeapDark/Weapons/Pistols_B'
TARGET = '/Game/AZ/Blueprints/Items/Equippables/Weapons/Pistol/SM_Pistol_Magazine'
BONE = 'Clip_Bone'
OWNER_KEY = 'AZ.PistolMagazineMesh.Owner'
OWNER = 'pistol_magazine_mesh_setup:v1'
EAL = unreal.EditorAssetLibrary
Q = unreal.GeometryScript_MeshQueries


def require(value, message):
    if not value:
        raise RuntimeError(message)


def v(point):
    return [float(point.x), float(point.y), float(point.z)]


def path_file(path):
    require(path.startswith('/Game/'), 'Expected a project package path')
    return ROOT / 'Content' / (path.removeprefix('/Game/') + '.uasset')


def file_hash(path):
    file = path_file(path)
    require(file.is_file(), 'Package is missing on disk: ' + str(file))
    return hashlib.sha256(file.read_bytes()).hexdigest()


def source_record(source):
    bounds = source.get_bounds()
    return {'path': source.get_path_name(), 'sha256': file_hash(SOURCE),
            'skeleton': source.get_editor_property('skeleton').get_path_name(),
            'origin': v(bounds.origin), 'extent': v(bounds.box_extent),
            'materials': [slot.material_interface.get_path_name() if slot.material_interface else None
                          for slot in source.get_editor_property('materials')]}


def read_lod():
    lod = unreal.GeometryScriptMeshReadLOD()
    lod.set_editor_property('lod_type', unreal.GeometryScriptLODType.SOURCE_MODEL)
    lod.set_editor_property('lod_index', 0)
    return lod


def mesh_triangles(mesh):
    result = {}
    for index in range(Q.get_num_triangle_i_ds(mesh)):
        triangle, valid = Q.get_triangle_indices(mesh, index)
        if valid:
            result[index] = [int(triangle.x), int(triangle.y), int(triangle.z)]
    return result


def mesh_points(mesh, triangles):
    result = {}
    for index in sorted({vertex for ids in triangles.values() for vertex in ids}):
        point, valid = Q.get_vertex_position(mesh, index)
        require(valid, 'Triangle references an invalid vertex')
        result[index] = v(point)
    return result


def bounds_of(points):
    require(points, 'Empty extracted mesh')
    minimum = [min(point[axis] for point in points.values()) for axis in range(3)]
    maximum = [max(point[axis] for point in points.values()) for axis in range(3)]
    return {'min': minimum, 'max': maximum,
            'center': [(a + b) * 0.5 for a, b in zip(minimum, maximum)],
            'dimensions': [b - a for a, b in zip(minimum, maximum)]}


def geometry_rows(mesh):
    triangles = mesh_triangles(mesh)
    points = mesh_points(mesh, triangles)
    rows = []
    for triangle_id, indices in triangles.items():
        uv1, uv2, uv3, has_uv = Q.get_triangle_u_vs(mesh, 0, triangle_id)
        require(has_uv, 'Extracted triangle lost source UV channel 0')
        material_id, valid = unreal.GeometryScript_Materials.get_triangle_material_id(mesh, triangle_id)
        require(valid and material_id == 0, 'Expected the single source pistol material')
        # Canonical triangle records compare geometry/UVs independently of vertex
        # and triangle renumbering when the StaticMesh is serialized.
        vertices = [tuple(points[index] + [float(uv.x), float(uv.y)])
                    for index, uv in zip(indices, (uv1, uv2, uv3))]
        start = min(range(3), key=lambda i: vertices[i])
        rows.append(tuple(vertices[start:] + vertices[:start]))
    return sorted(rows), {'vertices': len(points), 'triangles': len(triangles), 'bounds': bounds_of(points)}


def extract():
    source = unreal.load_asset(SOURCE)
    require(isinstance(source, unreal.SkeletalMesh), 'Missing source Pistols_B skeletal mesh')
    before = source_record(source)
    mesh, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(
        source, unreal.DynamicMesh(), unreal.GeometryScriptCopyMeshFromAssetOptions(), read_lod())
    require(outcome == unreal.GeometryScriptOutcomePins.SUCCESS, 'Cannot copy source skeletal LOD0')
    mesh, valid, bone_index = unreal.GeometryScript_BoneWeights.get_bone_index(mesh, BONE)
    require(valid, 'Source mesh has no Clip_Bone binding')
    triangles = mesh_triangles(mesh)
    points = mesh_points(mesh, triangles)
    selected = set()
    histogram = {}
    for index in points:
        _, weights, valid = unreal.GeometryScript_BoneWeights.get_vertex_bone_weights(mesh, index)
        require(valid and weights, 'Referenced source vertex has no skin weights')
        largest = max(weights, key=lambda weight: weight.weight)
        histogram[int(largest.bone_index)] = histogram.get(int(largest.bone_index), 0) + 1
        influence = sum(weight.weight for weight in weights if weight.bone_index == bone_index)
        if influence >= 0.999:
            selected.add(index)
    retained, deleted, mixed = [], [], []
    for triangle_id, indices in triangles.items():
        count = sum(index in selected for index in indices)
        if count == 3:
            retained.append(triangle_id)
        else:
            deleted.append(triangle_id)
            if count:
                mixed.append(triangle_id)
    require(not mixed, 'Clip_Bone crosses weighted mesh boundaries; inspect before extracting')
    require(len(retained) > 100 and len(retained) < len(triangles) // 2,
            'Unexpected clip selection; do not substitute a cartridge or entire gun')
    original_bounds = bounds_of({index: points[index] for index in selected})
    dimensions = original_bounds['dimensions']
    require(8 < max(dimensions) < 25 and 1.5 < min(dimensions) < 5,
            'Selected geometry is not the reviewed full-size magazine')
    before_rows = geometry_rows_for_selection(mesh, retained)
    delete_list = unreal.GeometryScript_List.convert_array_to_index_list(deleted, unreal.GeometryScriptIndexType.TRIANGLE)
    mesh, removed = unreal.GeometryScript_MeshEdits.delete_triangles_from_mesh(mesh, delete_list)
    require(removed == len(deleted), 'Could not remove every non-magazine triangle')
    # The skeletal source contains unused zero-weight vertices. Remove those
    # too so they cannot pollute asset bounds or become imported loose points.
    unreal.GeometryScript_MeshRepair.remove_unused_vertices(mesh)
    unreal.GeometryScript_MeshRepair.compact_mesh(mesh)
    require(geometry_rows(mesh)[0] == before_rows, 'Triangle removal changed retained geometry or UVs')
    translation = [-value for value in original_bounds['center']]
    unreal.GeometryScript_MeshTransforms.translate_mesh(mesh, unreal.Vector(*translation))
    expected_rows, summary = geometry_rows(mesh)
    require(summary['triangles'] == len(retained), 'Extraction changed magazine triangle count')
    require(all(abs(value) < 0.00001 for value in summary['bounds']['center']), 'Magazine pivot was not centered')
    require(source_record(source) == before, 'Source pistol changed during transient extraction')
    report = {'source': before, 'target': TARGET, 'bone': BONE, 'bone_index': bone_index,
              'weight_threshold': 0.999, 'dominant_bone_vertex_counts': histogram,
              'source_referenced_vertices': len(points), 'source_triangles': len(triangles),
              'selected_vertices': len(selected), 'selected_triangles': len(retained), 'mixed_triangles': len(mixed),
              'source_clip_bounds': original_bounds, 'applied_translation_cm': translation,
              'scale': [1, 1, 1], 'geometry': summary}
    return source, mesh, expected_rows, report


def geometry_rows_for_selection(mesh, selection):
    rows = []
    for triangle_id in selection:
        ids, valid = Q.get_triangle_indices(mesh, triangle_id)
        require(valid, 'Invalid retained triangle')
        uv1, uv2, uv3, valid = Q.get_triangle_u_vs(mesh, 0, triangle_id)
        require(valid, 'Source magazine triangle has no UVs')
        vertices = []
        for index, uv in zip((ids.x, ids.y, ids.z), (uv1, uv2, uv3)):
            point, valid = Q.get_vertex_position(mesh, index)
            require(valid, 'Invalid retained vertex')
            vertices.append(tuple(v(point) + [float(uv.x), float(uv.y)]))
        start = min(range(3), key=lambda i: vertices[i])
        rows.append(tuple(vertices[start:] + vertices[:start]))
    return sorted(rows)


def owned_or_missing():
    if not EAL.does_asset_exist(TARGET):
        return None
    asset = unreal.load_asset(TARGET)
    require(isinstance(asset, unreal.StaticMesh) and EAL.get_metadata_tag(asset, OWNER_KEY) == OWNER,
            'Foreign magazine mesh target; inspect before replacing')
    return asset


def verify_asset(asset, source, expected_rows, report):
    actual, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
        asset, unreal.DynamicMesh(), unreal.GeometryScriptCopyMeshFromAssetOptions(), read_lod())
    require(outcome == unreal.GeometryScriptOutcomePins.SUCCESS, 'Cannot read saved magazine mesh LOD0')
    actual_rows, summary = geometry_rows(actual)
    require(len(actual_rows) == len(expected_rows), 'Saved magazine triangle count differs')
    max_error = max(abs(a - b) for arow, brow in zip(actual_rows, expected_rows)
                    for av, bv in zip(arow, brow) for a, b in zip(av, bv))
    require(max_error < 0.0001, 'Saved mesh geometry or UVs differ from the extracted magazine')
    require(asset.get_material(0) == source.get_editor_property('materials')[0].material_interface,
            'Source pistol material was not preserved')
    count = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_simple_collision_count(asset)
    require(count == 1, 'Magazine should have one simple box collision')
    require(source_record(source) == report['source'], 'Source pistol changed during magazine authoring')
    return {'geometry': summary, 'maximum_geometry_uv_readback_error': max_error,
            'material': asset.get_material(0).get_path_name(), 'simple_collision_shapes': count}


def write_obj(mesh, path):
    triangles = mesh_triangles(mesh)
    points = mesh_points(mesh, triangles)
    remap = {index: i + 1 for i, index in enumerate(sorted(points))}
    lines = ['# Actual Clip_Bone geometry, centered; centimetres; audit viewing only.']
    lines.extend('v ' + ' '.join(str(value) for value in points[index]) for index in sorted(points))
    lines.extend('f ' + ' '.join(str(remap[index]) for index in triangle) for triangle in triangles.values())
    path.write_text('\n'.join(lines) + '\n', encoding='utf-8')


def main(mode='audit'):
    require(mode in ('audit', 'author', 'verify'), 'Mode must be audit, author or verify')
    try:
        asset = owned_or_missing()
        source, mesh, expected, report = extract()
        report['mode'] = mode
        OUTPUT.mkdir(parents=True, exist_ok=True)
        write_obj(mesh, OUTPUT / 'Pistol_Magazine_Extracted.obj')
        if mode == 'author':
            require(not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE before creating the magazine asset')
            dirty = {package.get_path_name() for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
            require(TARGET not in dirty, 'Magazine mesh has unsaved changes; preserve before authoring')
            backup = ROOT / 'Saved/Backups/PistolMagazineMesh' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
            backup.mkdir(parents=True)
            if path_file(TARGET).is_file():
                shutil.copy2(path_file(TARGET), backup / path_file(TARGET).name)
            (backup / 'before.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
            if asset is None:
                options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
                options.set_editor_property('enable_nanite', False)
                asset, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh, TARGET, options)
                require(outcome == unreal.GeometryScriptOutcomePins.SUCCESS and isinstance(asset, unreal.StaticMesh),
                        'Could not create the extracted magazine StaticMesh')
                EAL.set_metadata_tag(asset, OWNER_KEY, OWNER)
            else:
                options = unreal.GeometryScriptCopyMeshToAssetOptions()
                _, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(mesh, asset, options, unreal.GeometryScriptMeshWriteLOD())
                require(outcome == unreal.GeometryScriptOutcomePins.SUCCESS, 'Cannot refresh the owned magazine mesh')
            asset.modify()
            asset.set_material(0, source.get_editor_property('materials')[0].material_interface)
            editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
            require(editor.remove_collisions(asset), 'Cannot reset owned magazine collision')
            require(editor.add_simple_collisions(asset, unreal.ScriptCollisionShapeType.BOX) >= 0, 'Cannot add magazine box collision')
            EAL.set_metadata_tag(asset, 'AZ.PistolMagazineMesh.Source', SOURCE + ':' + BONE)
            EAL.set_metadata_tag(asset, 'AZ.PistolMagazineMesh.SourceSHA256', report['source']['sha256'])
            report['readback'] = verify_asset(asset, source, expected, report)
            require(EAL.save_loaded_asset(asset, only_if_is_dirty=False), 'Could not save the magazine mesh')
            report.update(status='authored_saved', backup=str(backup), sha256=file_hash(TARGET))
        elif mode == 'verify':
            require(asset is not None, 'Magazine asset has not been authored')
            require(EAL.get_metadata_tag(asset, 'AZ.PistolMagazineMesh.SourceSHA256') == report['source']['sha256'], 'Source pistol changed after extraction')
            report['readback'] = verify_asset(asset, source, expected, report)
            dirty = {package.get_path_name() for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
            require(TARGET not in dirty, 'Magazine mesh still needs saving')
            report.update(status='verified_saved', sha256=file_hash(TARGET))
        else:
            report['status'] = 'proposal_only_no_asset_writes'
        receipt = OUTPUT / (mode + '-readback.json')
        receipt.write_text(json.dumps(report, indent=2), encoding='utf-8')
        print('PISTOL_MAGAZINE_MESH ' + json.dumps({'mode': mode, 'status': report['status'], 'receipt': str(receipt),
                                                  'vertices': report['selected_vertices'], 'triangles': report['selected_triangles'],
                                                  'dimensions_cm': report['geometry']['bounds']['dimensions']}))
        return report
    finally:
        gc.collect()


if __name__ == '__main__':
    main('audit')
