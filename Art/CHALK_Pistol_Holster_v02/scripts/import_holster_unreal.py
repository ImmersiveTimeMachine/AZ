# @Description: Import the approved CHALK pistol holster mesh and baked material into its derived AZ asset folder
"""Run inside Unreal Python after reviewing exports/export_manifest.json.

Creates exactly one static mesh, three textures and one native material. The
default run refuses every existing target. A deliberate retry can set the global
``holster_replace_owned = True`` before execution; only this script's tagged
assets may then be replaced. No reference pistol, level, skeleton or original
material is imported, modified or saved. This script does not start PIE.
"""

import gc
import hashlib
import json
import math
import traceback
from pathlib import Path

import unreal


WORK_ROOT = Path('C:/UnrealEngine/Games/AZ/Art/CHALK_Pistol_Holster_v02')
PROJECT_ROOT = Path('C:/UnrealEngine/Games/AZ')
BASE = '/Game/AZ/Assets/Items/PistolHolster01'
MESH_NAME = 'SM_CHALK_PistolHolster01'
MATERIAL_NAME = 'M_CHALK_PistolHolster01'
OWNER_KEY = 'AZ.PistolHolster01.Owner'
OWNER = 'CHALK_PistolHolster01_v02'
MANIFEST_PATH = WORK_ROOT / 'exports/export_manifest.json'
REPORT_PATH = WORK_ROOT / 'exports/unreal_import_report.json'
ALLOW_REPLACE_OWNED = bool(globals().get('holster_replace_owned', False))
BOUNDS_TOLERANCE_CM = 0.05

TEXTURE_NAMES = {
    'base_color': 'T_CHALK_PistolHolster01_BaseColor',
    'normal': 'T_CHALK_PistolHolster01_Normal',
    'orm': 'T_CHALK_PistolHolster01_ORM',
}
MESH_PATH = BASE + '/Meshes/' + MESH_NAME
MATERIAL_PATH = BASE + '/Materials/' + MATERIAL_NAME
TEXTURE_PATHS = {key: BASE + '/Textures/' + name for key, name in TEXTURE_NAMES.items()}
EXPECTED_PATHS = set(TEXTURE_PATHS.values()) | {MESH_PATH, MATERIAL_PATH}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def package_path(asset_or_path):
    path = asset_or_path.get_path_name() if hasattr(asset_or_path, 'get_path_name') else str(asset_or_path)
    return path.split('.', 1)[0]


def package_file(path):
    require(path.startswith(BASE + '/'), 'Refusing a package outside the derived holster folder: ' + path)
    return PROJECT_ROOT / 'Content' / (path[len('/Game/'):] + '.uasset')


def vector(value):
    return [float(value.x), float(value.y), float(value.z)]


def bounds_dict(value):
    center, extent = vector(value.origin), vector(value.box_extent)
    return {
        'min': [center[i] - extent[i] for i in range(3)],
        'max': [center[i] + extent[i] for i in range(3)],
    }


def write_report(report):
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding='utf-8')


def load_and_check_manifest():
    require(MANIFEST_PATH.is_file(), 'Missing reviewed export manifest: ' + str(MANIFEST_PATH))
    manifest = json.loads(MANIFEST_PATH.read_text(encoding='utf-8-sig'))
    expected_fbx = (WORK_ROOT / 'exports' / (MESH_NAME + '.fbx')).resolve()
    require(Path(manifest['fbx_path']).resolve() == expected_fbx, 'Manifest FBX must be the single holster export.')
    source_files = {'fbx': expected_fbx}
    expected_hashes = {'fbx': str(manifest['fbx_sha256']).lower()}
    for key, name in TEXTURE_NAMES.items():
        expected = (WORK_ROOT / 'textures' / (name + '.png')).resolve()
        require(Path(manifest['texture_files'][key]).resolve() == expected, 'Unexpected texture source for ' + key)
        source_files[key] = expected
        expected_hashes[key] = str(manifest['texture_sha256'][key]).lower()
    for key, path in source_files.items():
        require(path.is_file(), 'Missing export: ' + str(path))
        require(sha256(path) == expected_hashes[key], 'Export hash does not match manifest: ' + str(path))
    color_header = source_files['base_color'].read_bytes()[:33]
    require(color_header[:8] == b'\x89PNG\r\n\x1a\n' and color_header[24] == 8,
            'BaseColor must be 8-bit sRGB PNG; Unreal treats 16-bit source images as linear.')
    count = manifest['expected_triangles']
    require(isinstance(count, int) and not isinstance(count, bool) and count > 0, 'Invalid expected triangle count.')
    expected_bounds = manifest['expected_bounds_ue_cm']
    for side in ('min', 'max'):
        require(len(expected_bounds[side]) == 3 and all(math.isfinite(float(v)) for v in expected_bounds[side]),
                'Invalid expected bounds: ' + side)
    require(all(expected_bounds['max'][i] > expected_bounds['min'][i] for i in range(3)),
            'The reviewed mesh must have real dimensions on all three axes.')
    return manifest, source_files, expected_hashes


def collision_guard():
    existing = {}
    for path in sorted(EXPECTED_PATHS):
        obj = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
        if obj is not None:
            require(unreal.EditorAssetLibrary.get_metadata_tag(obj, OWNER_KEY) == OWNER,
                    'Existing unrelated asset; no import performed: ' + path)
            require(ALLOW_REPLACE_OWNED,
                    'Target already exists. Review it before explicitly enabling holster_replace_owned: ' + path)
            expected_class = unreal.StaticMesh if path == MESH_PATH else unreal.Material if path == MATERIAL_PATH else unreal.Texture2D
            require(isinstance(obj, expected_class), 'Existing owned target has an unexpected class: ' + path)
        existing[path] = obj
    return existing


def mark_owned(asset, source_path, source_hash, manifest_hash):
    require(package_path(asset) in EXPECTED_PATHS, 'Refusing to tag an unexpected asset: ' + asset.get_path_name())
    unreal.EditorAssetLibrary.set_metadata_tag(asset, OWNER_KEY, OWNER)
    unreal.EditorAssetLibrary.set_metadata_tag(asset, 'AZ.PistolHolster01.Source', str(source_path))
    unreal.EditorAssetLibrary.set_metadata_tag(asset, 'AZ.PistolHolster01.SourceSHA256', source_hash)
    unreal.EditorAssetLibrary.set_metadata_tag(asset, 'AZ.PistolHolster01.ManifestSHA256', manifest_hash)


def import_one(source, destination, factory, existing, options=None):
    cached = existing[destination]
    if cached is not None and unreal.EditorAssetLibrary.get_metadata_tag(cached, 'AZ.PistolHolster01.SourceSHA256') == sha256(source):
        # An ownership-checked retry can resume after material verification
        # without rebuilding already imported, identical source files.
        return cached
    folder, name = destination.rsplit('/', 1)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(source))
    task.set_editor_property('destination_path', folder)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', existing[destination] is not None)
    task.set_editor_property('replace_existing_settings', True)
    task.set_editor_property('save', False)
    task.set_editor_property('factory', factory)
    if options is not None:
        task.set_editor_property('options', options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    objects = list(task.get_objects())
    actual_paths = {package_path(obj) for obj in objects}
    require(actual_paths == {destination},
            'Import did not produce exactly its one expected asset: ' + json.dumps(sorted(actual_paths)))
    asset = unreal.load_asset(destination)
    require(asset is not None, 'Imported asset could not be read: ' + destination)
    return asset


def texture_settings(key):
    return {
        'base_color': (unreal.TextureCompressionSettings.TC_DEFAULT, True, False,
                       unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
        'normal': (unreal.TextureCompressionSettings.TC_NORMALMAP, False, True,
                   unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),
        'orm': (unreal.TextureCompressionSettings.TC_MASKS, False, False,
                unreal.MaterialSamplerType.SAMPLERTYPE_MASKS),
    }[key]


def texture_audit(texture, key):
    compression, srgb, flip_green, sampler = texture_settings(key)
    require(isinstance(texture, unreal.Texture2D), 'Expected Texture2D: ' + texture.get_path_name())
    require(texture.get_editor_property('compression_settings') == compression, 'Wrong texture compression: ' + key)
    require(bool(texture.get_editor_property('srgb')) == srgb, 'Wrong texture color space: ' + key)
    require(bool(texture.get_editor_property('flip_green_channel')) == flip_green, 'Wrong normal Y convention: ' + key)
    require(not texture.get_editor_property('virtual_texture_streaming'), 'Expected a regular texture sampler: ' + key)
    return {'asset': texture.get_path_name(), 'compression': str(compression), 'srgb': srgb,
            'flip_green_channel': flip_green, 'virtual_texture_streaming': False, 'sampler_type': str(sampler)}


def make_material(textures, existing, manifest_hash):
    material = existing[MATERIAL_PATH]
    if material is None:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            MATERIAL_NAME, BASE + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    require(isinstance(material, unreal.Material), 'Expected a native Material at ' + MATERIAL_PATH)
    mark_owned(material, MANIFEST_PATH, manifest_hash, manifest_hash)
    # Only reachable for a new material or an explicitly approved, ownership-checked retry.
    # UE5.8 DeleteAllMaterialExpressions iterates the same array it shrinks;
    # use a snapshot so an owned retry cannot leave skipped orphan nodes.
    for expression in list(unreal.MaterialEditingLibrary.get_material_expressions(material)):
        unreal.MaterialEditingLibrary.delete_material_expression(material, expression)
    require(not unreal.MaterialEditingLibrary.get_material_expressions(material),
            'Owned material expression reset did not finish.')
    material.set_editor_property('two_sided', False)
    material.set_editor_property('tangent_space_normal', True)
    nodes = {}
    for row, key in enumerate(('base_color', 'normal', 'orm')):
        node = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionTextureSample, -500, row * 240 - 240)
        require(node is not None, 'Could not create material texture node: ' + key)
        node.set_editor_property('texture', textures[key])
        node.set_editor_property('sampler_type', texture_settings(key)[3])
        node.set_editor_property('const_coordinate', 0)
        nodes[key] = node
    for key, output, prop in material_connections():
        require(unreal.MaterialEditingLibrary.connect_material_property(nodes[key], output, prop),
                'Material connection failed: ' + key + '/' + output + ' -> ' + str(prop))
    errors = list(unreal.MaterialEditingLibrary.recompile_material(material))
    require(not errors, 'Native material compile failed: ' + '; '.join(str(e) for e in errors))
    return material


def material_connections():
    # Use explicit RGB; Unreal can report its default first output as either
    # RGB or an empty display name depending on expression initialization.
    return (
        ('base_color', 'RGB', unreal.MaterialProperty.MP_BASE_COLOR),
        ('normal', 'RGB', unreal.MaterialProperty.MP_NORMAL),
        ('orm', 'R', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION),
        ('orm', 'G', unreal.MaterialProperty.MP_ROUGHNESS),
        ('orm', 'B', unreal.MaterialProperty.MP_METALLIC),
    )


def material_audit(material, textures):
    expressions = list(unreal.MaterialEditingLibrary.get_material_expressions(material))
    require(len(expressions) == 3, 'Expected exactly the three native atlas texture nodes.')
    connections = []
    for key, output, prop in material_connections():
        node = unreal.MaterialEditingLibrary.get_material_property_input_node(material, prop)
        actual_output = unreal.MaterialEditingLibrary.get_material_property_input_node_output_name(material, prop)
        require(isinstance(node, unreal.MaterialExpressionTextureSample), 'Missing texture sample for ' + str(prop))
        require(node.get_editor_property('texture') == textures[key], 'Material uses an unexpected texture: ' + str(prop))
        require(node.get_editor_property('sampler_type') == texture_settings(key)[3], 'Wrong material sampler: ' + key)
        require(node.get_editor_property('const_coordinate') == 0, 'Material must sample the baked UV0 atlas.')
        require((str(actual_output) or 'RGB') == output, 'Wrong ORM/RGB channel on ' + str(prop) + ': ' + str(actual_output))
        connections.append({'property': str(prop), 'texture': textures[key].get_path_name(),
                            'channel': output or 'RGB', 'expression': node.get_path_name()})
    return {'asset': material.get_path_name(), 'native_expression_count': len(expressions),
            'connections': connections, 'two_sided': bool(material.get_editor_property('two_sided')),
            'tangent_space_normal': bool(material.get_editor_property('tangent_space_normal'))}


def mesh_options():
    ui = unreal.FbxImportUI()
    for name, value in {
        'automated_import_should_detect_type': False,
        'import_mesh': True, 'import_as_skeletal': False,
        'mesh_type_to_import': unreal.FBXImportType.FBXIT_STATIC_MESH,
        'import_animations': False, 'import_materials': False, 'import_textures': False,
        'create_physics_asset': False,
    }.items():
        ui.set_editor_property(name, value)
    data = ui.get_editor_property('static_mesh_import_data')
    for name, value in {
        'combine_meshes': True,
        'build_nanite': False,
        'auto_generate_collision': False,  # Wearable cloth accessory; no blocking hull is requested.
        'generate_lightmap_u_vs': False,
        'remove_degenerates': False,
        'import_mesh_lo_ds': False,
        'transform_vertex_to_absolute': True,
        'bake_pivot_in_vertex': False,
        'import_translation': unreal.Vector(0, 0, 0),
        'import_rotation': unreal.Rotator(0, 0, 0),
        'import_uniform_scale': 1.0,
        'convert_scene': True,
        'force_front_x_axis': False,
        'convert_scene_unit': False,  # Reviewed Y-forward/Z-up FBX already declares centimetres.
        'normal_import_method': unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS,
    }.items():
        data.set_editor_property(name, value)
    return ui


def mesh_audit(mesh, material, manifest):
    require(isinstance(mesh, unreal.StaticMesh), 'Expected one StaticMesh, not a skeletal or reference asset.')
    triangles = mesh.get_num_triangles(0)
    expected_bounds = manifest['expected_bounds_ue_cm']
    actual_bounds = bounds_dict(mesh.get_bounds())
    bounds_error = max(abs(actual_bounds[side][axis] - expected_bounds[side][axis])
                       for side in ('min', 'max') for axis in range(3))
    require(triangles == manifest['expected_triangles'], 'Imported triangle count differs from the reviewed export.')
    require(bounds_error <= BOUNDS_TOLERANCE_CM, 'Imported units, axes or origin differ from the export: ' + str(bounds_error) + ' cm')
    slots = list(mesh.get_editor_property('static_materials'))
    require(len(slots) == 1, 'Expected one baked atlas material slot.')
    require(mesh.get_num_tex_coords(0) == 1, 'Expected only the baked UV0 atlas.')
    require(mesh.get_num_lods() == 1, 'Expected exactly the reviewed LOD0.')
    require(mesh.get_material(0) == material, 'Mesh material getter does not match the created native material.')
    require(not mesh.get_editor_property('nanite_settings').enabled, 'Nanite unexpectedly enabled.')
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    build_settings = subsystem.get_lod_build_settings(mesh, 0)
    require(build_settings.get_editor_property('use_full_precision_u_vs'),
            'Fine atlas islands require full-precision UVs.')
    require(build_settings.get_editor_property('use_high_precision_tangent_basis'),
            'Fine normal-map detail requires high-precision tangents.')
    simple_count = subsystem.get_simple_collision_count(mesh)
    convex_count = subsystem.get_convex_collision_count(mesh)
    require(simple_count == 0 and convex_count == 0, 'Unexpected collision on the wearable holster.')
    imported = mesh.get_editor_property('asset_import_data')
    require(isinstance(imported, unreal.FbxStaticMeshImportData), 'Expected native legacy FBX import data.')
    require(imported.get_editor_property('normal_import_method') == unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS,
            'Imported normal/tangent settings changed.')
    return {
        'asset': mesh.get_path_name(), 'class': mesh.get_class().get_name(),
        'vertices': mesh.get_num_vertices(0), 'triangles': triangles,
        'expected_triangles': manifest['expected_triangles'], 'uv_channels': mesh.get_num_tex_coords(0),
        'material_slots': [{'slot': str(slots[0].material_slot_name), 'material': mesh.get_material(0).get_path_name()}],
        'bounds_cm': actual_bounds, 'expected_bounds_cm': expected_bounds,
        'max_bounds_error_cm': bounds_error, 'bounds_tolerance_cm': BOUNDS_TOLERANCE_CM,
        'export_matches': True, 'nanite': False,
        'full_precision_uvs': True, 'high_precision_tangents': True,
        'simple_collision_count': simple_count, 'convex_collision_count': convex_count,
        'import_factory': 'FbxFactory', 'normal_import_method': str(imported.get_editor_property('normal_import_method')),
        'convert_scene': bool(imported.get_editor_property('convert_scene')),
        'force_front_x_axis': bool(imported.get_editor_property('force_front_x_axis')),
        'convert_scene_unit': bool(imported.get_editor_property('convert_scene_unit')),
        'import_uniform_scale': float(imported.get_editor_property('import_uniform_scale')),
    }


def main():
    report = {'status': 'preflight', 'owner': OWNER, 'destination': BASE,
              'expected_assets': sorted(EXPECTED_PATHS), 'saved_assets': [],
              'pie_started': False, 'level_modified': False, 'thumbnail_used_as_proof': False}
    try:
        manifest, source_files, expected_hashes = load_and_check_manifest()
        manifest_hash = sha256(MANIFEST_PATH)
        existing = collision_guard()  # Check every target before creating or importing anything.
        baseline = {package_path(p) for p in unreal.EditorAssetLibrary.list_assets(BASE, recursive=True, include_folder=False)}
        report.update({'manifest': str(MANIFEST_PATH), 'manifest_sha256': manifest_hash,
                       'source_files': {k: str(v) for k, v in source_files.items()},
                       'source_sha256': expected_hashes, 'status': 'importing'})
        write_report(report)
        for folder in ('Textures', 'Materials', 'Meshes'):
            destination_folder = BASE + '/' + folder
            if not unreal.EditorAssetLibrary.does_directory_exist(destination_folder):
                require(unreal.EditorAssetLibrary.make_directory(destination_folder), 'Could not create derived asset folder: ' + folder)

        textures = {}
        for key in ('base_color', 'normal', 'orm'):
            texture = import_one(source_files[key], TEXTURE_PATHS[key], unreal.TextureFactory(), existing)
            require(isinstance(texture, unreal.Texture2D), 'Texture factory produced an unexpected class: ' + key)
            mark_owned(texture, source_files[key], expected_hashes[key], manifest_hash)
            compression, srgb, flip_green, _ = texture_settings(key)
            for name, value in {'compression_settings': compression, 'srgb': srgb,
                                'flip_green_channel': flip_green, 'virtual_texture_streaming': False}.items():
                texture.set_editor_property(name, value)
            textures[key] = texture
        report['textures'] = {key: texture_audit(texture, key) for key, texture in textures.items()}

        material = make_material(textures, existing, manifest_hash)
        report['material'] = material_audit(material, textures)
        mesh = import_one(source_files['fbx'], MESH_PATH, unreal.FbxFactory(), existing, mesh_options())
        require(isinstance(mesh, unreal.StaticMesh), 'FBX did not produce the requested static holster.')
        mark_owned(mesh, source_files['fbx'], expected_hashes['fbx'], manifest_hash)
        unreal.EditorAssetLibrary.set_metadata_tag(mesh, 'AZ.PistolHolster01.Purpose', 'Approved option 01 wearable holster; attachment origin baked into mesh.')
        require(len(mesh.get_editor_property('static_materials')) == 1, 'Export is not a single-material atlas.')
        mesh.set_material(0, material)
        mesh_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        build_settings = mesh_editor.get_lod_build_settings(mesh, 0)
        if not build_settings.get_editor_property('use_full_precision_u_vs') or not build_settings.get_editor_property('use_high_precision_tangent_basis'):
            build_settings.set_editor_property('use_full_precision_u_vs', True)
            build_settings.set_editor_property('use_high_precision_tangent_basis', True)
            mesh_editor.set_lod_build_settings(mesh, 0, build_settings)
        report['mesh'] = mesh_audit(mesh, material, manifest)
        current = {package_path(p) for p in unreal.EditorAssetLibrary.list_assets(BASE, recursive=True, include_folder=False)}
        require(not (current - baseline - EXPECTED_PATHS), 'Unexpected assets created in the derived folder: ' + str(sorted(current - baseline - EXPECTED_PATHS)))
        for key, path in source_files.items():
            require(sha256(path) == expected_hashes[key], 'Source export changed during import: ' + str(path))
        require(sha256(MANIFEST_PATH) == manifest_hash, 'Export manifest changed during import.')
        report['status'] = 'verified_before_save'
        write_report(report)

        # Save only these five derived packages; no save-all, levels, originals or reference pistol.
        for path in [TEXTURE_PATHS[k] for k in ('base_color', 'normal', 'orm')] + [MATERIAL_PATH, MESH_PATH]:
            obj = unreal.load_asset(path)
            require(unreal.EditorAssetLibrary.get_metadata_tag(obj, OWNER_KEY) == OWNER, 'Owner changed before save: ' + path)
            require(unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False), 'Could not save derived asset: ' + path)
            file = package_file(path)
            require(file.is_file() and file.stat().st_size > 0, 'Saved package file missing: ' + str(file))
            report['saved_assets'].append({'asset': path, 'file': str(file), 'sha256': sha256(file)})
            write_report(report)

        # Fresh native getters after saving verify real slots, graph references and texture settings.
        fresh_textures = {key: unreal.load_asset(path) for key, path in TEXTURE_PATHS.items()}
        fresh_material = unreal.load_asset(MATERIAL_PATH)
        fresh_mesh = unreal.load_asset(MESH_PATH)
        report['textures_after_save'] = {key: texture_audit(texture, key) for key, texture in fresh_textures.items()}
        report['material_after_save'] = material_audit(fresh_material, fresh_textures)
        report['mesh_after_save'] = mesh_audit(fresh_mesh, fresh_material, manifest)
        report['export_matches'] = True
        report['status'] = 'success'
        write_report(report)
        print('HOLSTER_IMPORT_COMPLETE ' + json.dumps({'asset': MESH_PATH, 'triangles': report['mesh_after_save']['triangles'],
                                                      'export_matches': True, 'saved_assets': len(report['saved_assets']),
                                                      'report': str(REPORT_PATH)}))
        return report
    except Exception as exc:
        report['status'] = 'failed'
        report['error'] = str(exc)
        report['traceback'] = traceback.format_exc()
        write_report(report)
        print('HOLSTER_IMPORT_FAILED ' + str(REPORT_PATH) + ': ' + str(exc))
        raise
    finally:
        gc.collect()


if __name__ == '__main__':
    main()
