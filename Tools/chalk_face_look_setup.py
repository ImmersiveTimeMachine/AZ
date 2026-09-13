"""Guarded CHALK face looks. Default is a read-only audit.

Run main('build', pngs={...}) to author BOTH approved look families; then save
only save_after_return packages. Run main('select', look='03') to change only
the non-generated hero Face SCS template. Compile the REGULAR hero Blueprint
with the dedicated editor tool after Python returns, then save that BP and run
main('verify', look='03'). This file never compiles a BP, starts PIE, saves assets,
changes a source asset, changes global importer settings, or touches AnimBPs.

pngs format: {'02': {'LOD0': absolute_png, 'LOD2': absolute_png,
'LOD5to7': absolute_png}, '03': {...}}. Preserve the baked stubble in LOD2 and
baked eyebrows/stubble in LOD5to7 by painting each original neutral source.
main('preview', look='03') optionally creates a separate lookdev-only FaceMesh;
the hero keeps its original mesh. main('select', look='02') selects He Stayed.
"""
import datetime
import gc
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import unreal

ROOT = Path('C:/UnrealEngine/Games/AZ')
ART = ROOT / 'UI Design/CHALK_Protagonist_Face_v01'
REPORT = ART / 'implementation'
OWNED = '/Game/AZ/Blueprints/Character/Appearance/CHALKTeacher'
FACE = '/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Face'
MESH = FACE + '/SKM_MHC_Hero_FaceMesh'
HERO = '/Game/AZ/Blueprints/Character/Hero/MHC/AZ_BP_PawnMoverHero_MHC'
MASTER = '/Game/MetaHumans/Common/Lookdev_UHM/Skin/Materials/M_skin_unified_baked'
LOOKS = {'02': '02_HeStayed', '03': '03_AfterAHardNight'}
LODS = ('0', '1', '2', '3', '4', '5to7')
SLOTS = dict(zip(LODS, (0, 9, 10, 12, 13, 14)))
BASES = {'LOD0': FACE + '/Baked/T_Head_BC_VT',
         'LOD2': FACE + '/Baked/T_Head_LOD2_BC_VT',
         'LOD5to7': FACE + '/Baked/T_Head_LOD5to7_BC_VT'}
SIZES = {'LOD0': (2048, 2048), 'LOD2': (2048, 2048), 'LOD5to7': (1024, 1024)}
OWNER_KEY, OWNER = 'CHALK.FaceLook.Owner', 'chalk_face_look_setup:v1'
SOURCE_KEY, PNG_KEY = 'CHALK.FaceLook.Source', 'CHALK.FaceLook.PNG_SHA256'
TEXTURE_SETTINGS = (
    'srgb', 'compression_settings', 'virtual_texture_streaming', 'lod_group',
    'lod_bias', 'max_texture_size', 'mip_gen_settings', 'never_stream',
    'compression_no_alpha', 'compression_none', 'defer_compression',
    'flip_green_channel', 'preserve_border', 'filter', 'address_x', 'address_y',
    'num_cinematic_mip_levels', 'do_scale_mips_for_alpha_coverage',
    'alpha_coverage_thresholds', 'use_new_mip_filter', 'adjust_brightness',
    'adjust_brightness_curve', 'adjust_vibrance', 'adjust_saturation',
    'adjust_rg_bcurve', 'adjust_rgb_curve', 'adjust_hue', 'adjust_min_alpha', 'adjust_max_alpha',
    'power_of_two_mode', 'padding_color', 'chroma_key_texture',
    'chroma_key_threshold', 'chroma_key_color', 'lossy_compression_amount',
    'compress_final', 'downscale', 'downscale_options', 'source_color_settings')


def package(obj):
    return obj.get_path_name().split('.')[0] if obj else None


def need(path, cls=None):
    obj = unreal.load_asset(path)
    assert obj and (cls is None or isinstance(obj, cls)), 'Missing/wrong asset: ' + path
    return obj


def mi_path(look, lod):
    return OWNED + '/' + LOOKS[look] + '/Materials/MI_CHALK_' + LOOKS[look] + '_LOD' + lod + '_VT'


def source_mi(lod):
    return FACE + '/Materials/MI_Face_Skin_Baked_LOD' + lod + '_VT'


def texture_path(look, source):
    if source in BASES.values():
        tail = source.rsplit('/', 1)[1].removeprefix('T_')
        return OWNED + '/' + LOOKS[look] + '/Textures/T_CHALK_' + LOOKS[look] + '_' + tail
    return OWNED + '/SharedOriginalCopies/' + source.rsplit('/', 1)[1]


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def protected_hashes():
    # All generated face asset files are protected, not just the six inputs.
    folder = ROOT / 'Content/AZ/Blueprints/Character/AZ_MHC_Hero/Face'
    return {str(p): sha(p) for p in sorted(folder.rglob('*'))
            if p.is_file() and p.suffix in ('.uasset', '.uexp', '.ubulk')}


def mesh_materials(mesh):
    return [package(s.get_editor_property('material_interface'))
            for s in mesh.get_editor_property('materials')]


def texture_settings(texture):
    values = {}
    for name in TEXTURE_SETTINGS:
        try:
            values[name] = texture.get_editor_property(name)
        except Exception:
            pass  # Optional fields differ by engine version; core fields required below.
    for name in ('srgb', 'compression_settings', 'virtual_texture_streaming', 'lod_group',
                 'lod_bias', 'max_texture_size', 'mip_gen_settings'):
        assert name in values, 'Required texture setting is inaccessible: ' + name
    return values


def stable_value(value):
    """Compare reflected values by content, never wrapper memory addresses."""
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, unreal.Object):
        return ('object', value.get_path_name())
    if isinstance(value, unreal.StructBase):
        # StructBase.__str__ includes its wrapper address; export_text contains
        # reflected field contents, including Vector4's X/Y/Z/W values.
        return ('struct', type(value).__name__, value.export_text())
    if isinstance(value, (list, tuple, unreal.Array)):
        return ('array', tuple(stable_value(item) for item in value))
    if isinstance(value, (set, frozenset, unreal.Set)):
        return ('set', tuple(sorted((stable_value(item) for item in value), key=repr)))
    if isinstance(value, (dict, unreal.Map)):
        pairs = ((stable_value(key), stable_value(item)) for key, item in value.items())
        return ('map', tuple(sorted(pairs, key=repr)))
    # Unreal enums, Names and Text expose stable value strings. Fail closed if
    # another wrapper type needs an explicit serializer instead of ignoring it.
    text = str(value)
    assert not re.search(r'\b0x[0-9a-fA-F]+\b', text), 'Unsupported reflected value serializer: ' + type(value).__name__
    return (type(value).__name__, text)


def material_values(material):
    lib = unreal.MaterialEditingLibrary
    result = {}
    for kind in ('scalar', 'vector', 'texture', 'static_switch'):
        result[kind] = {}
        for name in getattr(lib, 'get_' + kind + '_parameter_names')(material):
            val = getattr(lib, 'get_material_instance_' + kind + '_parameter_value')(material, name)
            if kind == 'texture':
                val = package(val)
            elif kind == 'vector':
                val = (val.r, val.g, val.b, val.a)
            result[kind][str(name)] = val
    return result


def face_template():
    bp = need(HERO, unreal.Blueprint)
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    matches = []
    for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        obj = lib.get_associated_object(data)
        if isinstance(obj, unreal.SkeletalMeshComponent) and str(lib.get_variable_name(data)) == 'Face':
            matches.append(obj)
    assert len(matches) == 1, 'Expected exactly one Face SCS template'
    face = matches[0]
    assert face.get_path_name().startswith(HERO + '.'), 'Face template is inherited from another BP'
    assert package(face.get_editor_property('skeletal_mesh_asset')) == MESH, 'Hero Face mesh changed; inspect first'
    return bp, face


def audit():
    bp, face = face_template()
    mats = {lod: material_values(need(source_mi(lod), unreal.MaterialInstanceConstant)) for lod in LODS}
    owned_sources = sorted({p for val in mats.values() for p in val['texture'].values()
                            if p and p.startswith(FACE + '/')})
    actual = mesh_materials(need(MESH, unreal.SkeletalMesh))
    for lod, slot in SLOTS.items():
        assert actual[slot] == source_mi(lod), 'Original face skin slot differs from audited source'
    return {'hero': HERO, 'template': face.get_path_name(), 'mesh': MESH,
            'original_mesh_materials': actual,
            'override_materials': [package(m) for m in face.get_editor_property('override_materials')],
            'skin_material_sources': mats, 'face_owned_texture_sources': owned_sources,
            'expected_look_materials': {look: {lod: mi_path(look, lod) for lod in LODS} for look in LOOKS}}


def owned_copy(source, target, changed):
    assert target.startswith(OWNED + '/') and not target.startswith(FACE + '/')
    src = need(source)
    if unreal.EditorAssetLibrary.does_asset_exist(target):
        obj = need(target)
        assert unreal.EditorAssetLibrary.get_metadata_tag(obj, OWNER_KEY) == OWNER, 'Unowned destination: ' + target
        assert unreal.EditorAssetLibrary.get_metadata_tag(obj, SOURCE_KEY) == source, 'Source mismatch: ' + target
        assert obj.get_class() == src.get_class(), 'Destination class mismatch'
        # A previous interrupted build may have left this owned asset unsaved.
        # Keep the exact owned package in the final save list on every resume.
        changed.add(target)
        return obj
    obj = unreal.EditorAssetLibrary.duplicate_asset(source, target)
    assert obj, 'Duplicate failed: ' + target
    unreal.EditorAssetLibrary.set_metadata_tag(obj, OWNER_KEY, OWNER)
    unreal.EditorAssetLibrary.set_metadata_tag(obj, SOURCE_KEY, source)
    changed.add(target)
    return obj


def png_inputs(pngs):
    if pngs is None:
        pngs = {look: {base: str(ART / 'sources' / ('face_' + look + '_basecolor_' + base + '.png'))
                      for base in BASES} for look in LOOKS}
    rows = {}
    for look in LOOKS:
        rows[look] = {}
        for base in BASES:
            path = Path(pngs[look][base]).resolve()
            assert path.is_file(), 'Missing authored LOD PNG: ' + str(path)
            header = path.read_bytes()[:26]
            assert header[:8] == b'\x89PNG\r\n\x1a\n', 'Not PNG: ' + str(path)
            assert struct.unpack('>II', header[16:24]) == SIZES[base], 'Incorrect LOD dimensions: ' + str(path)
            assert header[24] == 8 and header[25] in (2, 6), 'Expected 8-bit RGB/RGBA albedo PNG'
            rows[look][base] = {'path': str(path), 'sha256': sha(path), 'size': SIZES[base]}
    return rows


def import_owned_png(source, target, png, changed):
    obj = owned_copy(source, target, changed)
    # Every import is over an owned duplicate, never over a generated source.
    original_settings = texture_settings(need(source, unreal.Texture2D))
    if unreal.EditorAssetLibrary.get_metadata_tag(obj, PNG_KEY) != png['sha256']:
        task = unreal.AssetImportTask()
        task.filename = png['path']
        task.destination_path, task.destination_name = target.rsplit('/', 1)
        task.automated = True
        task.save = False
        task.replace_existing = True
        task.replace_existing_settings = False
        task.factory = unreal.TextureFactory()  # SpecifiedFactory != nullptr bypasses Interchange.
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        assert task.imported_object_paths, 'Native texture import returned no objects: ' + target
        assert all(p.split('.')[0] == target for p in task.imported_object_paths), 'Unexpected import destination'
        obj = need(target, unreal.Texture2D)
        unreal.EditorAssetLibrary.set_metadata_tag(obj, OWNER_KEY, OWNER)
        unreal.EditorAssetLibrary.set_metadata_tag(obj, SOURCE_KEY, source)
        unreal.EditorAssetLibrary.set_metadata_tag(obj, PNG_KEY, png['sha256'])
        changed.add(target)
    for name, value in original_settings.items():
        if stable_value(obj.get_editor_property(name)) != stable_value(value):
            obj.set_editor_property(name, value)
            changed.add(target)
    assert (obj.blueprint_get_size_x(), obj.blueprint_get_size_y()) == tuple(png['size']), 'Imported texture dimensions changed'
    for name, value in original_settings.items():
        expected = stable_value(value)
        actual = stable_value(obj.get_editor_property(name))
        assert actual == expected, 'Texture setting drift: ' + target + ' ' + name + ' expected=' + repr(expected) + ' actual=' + repr(actual)
    return obj


def build(data, pngs, changed):
    inputs = png_inputs(pngs)  # Preflight all six files before any mutation.
    for source in data['face_owned_texture_sources']:
        if source not in BASES.values():
            owned_copy(source, texture_path('03', source), changed)
    for look in LOOKS:
        for base, source in BASES.items():
            import_owned_png(source, texture_path(look, source), inputs[look][base], changed)
        for index, lod in enumerate(LODS):
            obj = owned_copy(source_mi(lod), mi_path(look, lod), changed)
            parent = need(MASTER if index == 0 else mi_path(look, LODS[index - 1]))
            if package(obj.get_editor_property('parent')) != package(parent):
                unreal.MaterialEditingLibrary.set_material_instance_parent(obj, parent)
                changed.add(package(obj))
            values = data['skin_material_sources'][lod]
            for param, source in values['texture'].items():
                if source and source.startswith(FACE + '/'):
                    target = texture_path(look, source)
                    current = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(obj, param)
                    if package(current) != target:
                        # UE5.8 MaterialEditingLibrary.cpp performs the assignment but
                        # leaves bResult=false. Verify the effective value, not that return.
                        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(obj, param, need(target))
                        assigned = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(obj, param)
                        assert package(assigned) == target, 'Texture param readback failed: ' + package(obj) + '/' + param
                        changed.add(package(obj))
            unreal.MaterialEditingLibrary.update_material_instance(obj)
    return inputs


def verify_looks(data):
    rows = {}
    for look in LOOKS:
        rows[look] = {}
        for index, lod in enumerate(LODS):
            obj = need(mi_path(look, lod), unreal.MaterialInstanceConstant)
            assert unreal.EditorAssetLibrary.get_metadata_tag(obj, OWNER_KEY) == OWNER
            parent = MASTER if index == 0 else mi_path(look, LODS[index - 1])
            assert package(obj.get_editor_property('parent')) == parent, 'Look parent chain mismatch'
            actual = material_values(obj)
            expected = data['skin_material_sources'][lod]
            for kind in ('scalar', 'vector', 'static_switch'):
                assert actual[kind] == expected[kind], 'Unexpected material setting drift: ' + look + '/' + lod + '/' + kind
            for param, source in expected['texture'].items():
                target = texture_path(look, source) if source and source.startswith(FACE + '/') else source
                assert actual['texture'][param] == target, 'Unexpected texture binding: ' + look + '/' + lod + '/' + param
            rows[look][lod] = {'path': package(obj), 'parent': parent, 'animated_maps': actual['static_switch'].get('Use Animated Maps'), 'texture_parameters': actual['texture']}
    return rows


def backup_hero():
    relative = HERO.removeprefix('/Game/')
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    folder = REPORT / 'backups' / stamp
    files = []
    for ext in ('.uasset', '.uexp', '.ubulk'):
        source = ROOT / 'Content' / (relative + ext)
        if source.exists():
            target = folder / (source.name)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
            files.append({'source': str(source), 'backup': str(target), 'sha256': sha(source)})
    return files


def select(look, changed):
    assert look in LOOKS
    bp, face = face_template()
    overrides = list(face.get_editor_property('override_materials'))
    before = [package(m) for m in overrides]
    # A user may preview one of our LOD instances directly in another skin slot.
    # It is still an owned look; normalize it to the matching per-LOD instance.
    owned_look_materials = {mi_path(l, level) for l in LOOKS for level in LODS}
    for lod, slot in SLOTS.items():
        current = package(overrides[slot]) if slot < len(overrides) else None
        assert current in (None, source_mi(lod)) or current in owned_look_materials, 'Unrecognized skin override; inspect first'
    backups = backup_hero()
    materials = {lod: need(mi_path(look, lod), unreal.MaterialInstanceConstant) for lod in SLOTS}
    bp.modify()
    face.modify()
    for lod, slot in SLOTS.items():
        # Native SetMaterial updates OverrideMaterials without property-change
        # notification that could reconstruct the SCS template under Python.
        face.set_material(slot, materials[lod])
    bp, face = face_template()  # Read back through a freshly resolved template.
    unreal.EditorAssetLibrary.set_metadata_tag(bp, 'CHALK.FaceLook.Active', LOOKS[look])
    changed.add(HERO)
    after = [package(m) for m in face.get_editor_property('override_materials')]
    for slot in range(len(after)):
        if slot not in SLOTS.values():
            assert after[slot] == (before[slot] if slot < len(before) else None), 'Non-skin override changed'
    return {'before': before, 'after': after, 'backup_files': backups,
            'compile_regular_blueprint_after_python_returns': HERO}


def preview(look, changed):
    assert look in LOOKS
    target = OWNED + '/Lookdev/SKM_CHALK_' + LOOKS[look] + '_FacePreview'
    mesh = owned_copy(MESH, target, changed)
    materials = list(mesh.get_editor_property('materials'))
    for lod, slot in SLOTS.items():
        materials[slot].set_editor_property('material_interface', need(mi_path(look, lod)))
    mesh.modify()
    mesh.set_editor_property('materials', materials)
    changed.add(target)
    return {'lookdev_mesh': target, 'hero_mesh_remains': MESH}


def main(mode='audit', look='03', pngs=None):
    assert mode in ('audit', 'build', 'select', 'preview', 'verify')
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE before face setup'
    REPORT.mkdir(parents=True, exist_ok=True)
    before = protected_hashes()
    changed = set()
    data = audit()
    result = {'mode': mode, 'look': look, 'audit': data, 'status': 'running'}
    try:
        if mode == 'build':
            result['authored_pngs'] = build(data, pngs, changed)
        if mode in ('build', 'select', 'preview', 'verify'):
            result['look_readback'] = verify_looks(data)
        if mode == 'select':
            result['selection'] = select(look, changed)
        elif mode == 'preview':
            result['preview'] = preview(look, changed)
        if mode in ('select', 'verify'):
            bp, face = face_template()
            for lod, slot in SLOTS.items():
                assert package(face.get_material(slot)) == mi_path(look, lod), 'Active Face override mismatch'
            result['active_look_verified_on_scs_template'] = LOOKS[look]
        assert mesh_materials(need(MESH)) == data['original_mesh_materials'], 'Original mesh materials changed'
        result['status'] = 'completed'
        return result
    except Exception as exc:
        result['status'] = 'failed'
        result['error'] = type(exc).__name__ + ': ' + str(exc)
        raise
    finally:
        after = protected_hashes()
        result['original_face_asset_files_unchanged'] = before == after
        result['protected_asset_sha256'] = {'before': before, 'after': after}
        result['save_after_return'] = sorted(changed)
        if before != after:
            result['status'] = 'original_asset_drift'
        (REPORT / ('setup-' + mode + '-' + look + '.json')).write_text(json.dumps(result, indent=2, default=str), encoding='utf-8')
        gc.collect()
        assert before == after, 'Original generated face files changed; inspect preservation report'


if __name__ == '__main__':
    print(json.dumps(main(), indent=2, default=str))
