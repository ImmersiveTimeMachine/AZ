# @Description: Import separated backpack meshes into their own AZ item asset folder
import unreal, json, gc, math, hashlib
from pathlib import Path

ROOT=Path('C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts')
BASE='/Game/AZ/Assets/Items/Backpack2'
SOURCE='/Game/SurvivalMan/Meshes/Parts/Without_a_jacket/SKM_SurvivalMan_backpack2'
OWNER='AZ_Backpack2_Separated_v1'
names=['Backpack','Knife','Axe','Bottle','Bottle_Holder','Rope','Front_Pouch']
selected=globals().get('bp2_import_jobs',[(group,name) for group in ['Assembly','Items'] for name in names])
source=unreal.load_asset(SOURCE)
assert source
materials=[slot.get_editor_property('material_interface') for slot in source.get_editor_property('materials')]
skelpath=BASE+'/Rig/SKEL_Backpack2'
source_skeleton=source.get_editor_property('skeleton')
protected=json.loads((ROOT/'textures/source_material_audit.json').read_text())['source_sha256_after']
skelfile=Path('C:/UnrealEngine/Games/AZ/Content/SurvivalMan/Meshes/SKEL_SurvivalMan.uasset')
protected[str(skelfile)]=hashlib.sha256(skelfile.read_bytes()).hexdigest()
def unchanged():
    for path,expected in protected.items():
        assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==expected,'Protected original changed: '+path
def vec(v): return [float(v.x),float(v.y),float(v.z)]
def bounds(b):
    center=vec(b.origin);extent=vec(b.box_extent)
    return {'min':[a-b for a,b in zip(center,extent)],'max':[a+b for a,b in zip(center,extent)]}
def ref_pose(mesh):
    mod=unreal.SkeletonModifier();assert mod.set_skeletal_mesh(mesh)
    result={}
    for name in mod.get_all_bone_names():
        transform=mod.get_bone_transform(name,False)
        q=transform.rotation
        result[str(name)]={'parent':str(mod.get_parent_name(name)),'location':vec(transform.translation),
                           'rotation':[q.x,q.y,q.z,q.w],'scale':vec(transform.scale3d)}
    return result
reference=ref_pose(source)
top=json.loads((ROOT/'inspection/source_topology.json').read_text())
geo=json.loads((ROOT/'inspection/source_geometry.json').read_text())
mapping=json.loads((ROOT/'inspection/logical_parts.json').read_text())
reportpath=ROOT/'inspection/unreal_import_report.json'
report=json.loads(reportpath.read_text()) if reportpath.exists() else {'destination':BASE,'assets':{},'source_originals_unchanged':True}
for group,name in selected:
    prefix='SKM' if group=='Assembly' else 'SM'
    assetname=f'{prefix}_Backpack2_{name}'
    folder=BASE+'/Meshes/'+group
    assetpath=folder+'/'+assetname
    filename=ROOT/'exports'/group/(assetname+'.fbx')
    assert filename.is_file(),str(filename)
    exists=unreal.EditorAssetLibrary.does_asset_exist(assetpath)
    if exists:
        existing=unreal.load_asset(assetpath)
        assert unreal.EditorAssetLibrary.get_metadata_tag(existing,'AZ.Backpack2.Owner')==OWNER,'Existing unrelated asset: '+assetpath
        assert globals().get('bp2_replace_owned',False),'Already imported: '+assetpath
    options=unreal.FbxImportUI()
    options.set_editor_property('automated_import_should_detect_type',False)
    options.set_editor_property('import_mesh',True)
    options.set_editor_property('import_animations',False)
    options.set_editor_property('import_materials',False)
    options.set_editor_property('import_textures',False)
    options.set_editor_property('import_as_skeletal',group=='Assembly')
    options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_SKELETAL_MESH if group=='Assembly' else unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property('create_physics_asset',False)
    if group=='Assembly':
        if not unreal.EditorAssetLibrary.does_asset_exist(skelpath):
            skeleton=unreal.EditorAssetLibrary.duplicate_asset(source_skeleton.get_path_name(),skelpath)
            assert skeleton
            unreal.EditorAssetLibrary.set_metadata_tag(skeleton,'AZ.Backpack2.Owner',OWNER)
        skeleton=unreal.load_asset(skelpath)
        assert unreal.EditorAssetLibrary.get_metadata_tag(skeleton,'AZ.Backpack2.Owner')==OWNER
        options.set_editor_property('skeleton',skeleton)
        data=options.get_editor_property('skeletal_mesh_import_data')
        data.set_editor_property('update_skeleton_reference_pose',False)
        data.set_editor_property('use_t0_as_ref_pose',False)
        data.set_editor_property('import_mesh_lo_ds',False)
        data.set_editor_property('preserve_smoothing_groups',True)
    else:
        data=options.get_editor_property('static_mesh_import_data')
        data.set_editor_property('combine_meshes',True)
        data.set_editor_property('auto_generate_collision',True)
        data.set_editor_property('generate_lightmap_u_vs',False)
        data.set_editor_property('transform_vertex_to_absolute',True)
        data.set_editor_property('bake_pivot_in_vertex',False)
    data.set_editor_property('import_translation',unreal.Vector(0,0,0))
    data.set_editor_property('import_rotation',unreal.Rotator(0,0,0))
    data.set_editor_property('import_uniform_scale',1.0)
    data.set_editor_property('convert_scene',True)
    data.set_editor_property('convert_scene_unit',False)
    data.set_editor_property('force_front_x_axis',False)
    data.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    task=unreal.AssetImportTask()
    task.set_editor_property('filename',str(filename))
    task.set_editor_property('destination_path',folder)
    task.set_editor_property('destination_name',assetname)
    task.set_editor_property('automated',True)
    task.set_editor_property('replace_existing',exists)
    task.set_editor_property('replace_existing_settings',True)
    task.set_editor_property('save',False)
    task.set_editor_property('factory',unreal.FbxFactory())
    task.set_editor_property('options',options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh=unreal.load_asset(assetpath)
    assert mesh,'Import did not produce requested mesh: '+str(task.get_editor_property('imported_object_paths'))
    unreal.EditorAssetLibrary.set_metadata_tag(mesh,'AZ.Backpack2.Owner',OWNER)
    unreal.EditorAssetLibrary.set_metadata_tag(mesh,'AZ.Backpack2.Source',SOURCE)
    slots=mesh.get_editor_property('materials' if group=='Assembly' else 'static_materials')
    slotnames=[]
    for slot_index,slot in enumerate(slots):
        slotname=str(slot.get_editor_property('material_slot_name'))
        material=materials[1] if 'Jacket' in slotname else materials[0]
        slot.set_editor_property('material_interface',material)
        slots[slot_index]=slot
        slotnames.append({'slot':slotname,'material':material.get_path_name()})
    mesh.set_editor_property('materials' if group=='Assembly' else 'static_materials',slots)
    for slot,expected_slot in zip(mesh.get_editor_property('materials' if group=='Assembly' else 'static_materials'),slotnames):
        assert slot.get_editor_property('material_interface').get_path_name()==expected_slot['material'],'Material assignment did not persist'
    faces=mapping['parts']['BP2_'+name]
    indices=sorted(set(v for face in faces for v in geo['polygons'][face]))
    if group=='Assembly':
        points=[[geo['positions'][v][0]*100,-geo['positions'][v][1]*100,geo['positions'][v][2]*100] for v in indices]
        expected={'min':[min(p[a] for p in points) for a in range(3)],'max':[max(p[a] for p in points) for a in range(3)]}
        actual=bounds(mesh.get_imported_bounds())
        subsystem=unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
        verts=subsystem.get_num_verts(mesh,0)
        dm=unreal.DynamicMesh()
        result=unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(mesh,dm,unreal.GeometryScriptCopyMeshFromAssetOptions(),unreal.GeometryScriptMeshReadLOD(lod_type=unreal.GeometryScriptLODType.RENDER_DATA,lod_index=0))
        triangles=dm.get_triangle_count()
        bones=ref_pose(mesh)
        assert set(bones)==set(reference),'Bone names changed'
        assert all(v['parent']==reference[k]['parent'] for k,v in bones.items()),'Bone parents changed'
        maxloc=max(abs(v-reference[k]['location'][a]) for k,b in bones.items() for a,v in enumerate(b['location']))
        maxscale=max(abs(v-reference[k]['scale'][a]) for k,b in bones.items() for a,v in enumerate(b['scale']))
        maxangle=max(math.degrees(2*math.acos(min(1.0,abs(sum(a*b for a,b in zip(bones[k]['rotation'],reference[k]['rotation'])))))) for k in bones)
        posecheck={'bone_count':len(bones),'max_local_translation_error_cm':maxloc,'max_local_scale_error':maxscale,'max_local_rotation_error_deg':maxangle}
        unreal.EditorAssetLibrary.set_metadata_tag(mesh,'AZ.Backpack2.Purpose','Aligned skinned assembly part; original character-space origin.')
    else:
        static=json.loads((ROOT/'exports/items_export_manifest.json').read_text())['parts'][name]
        expected=static['expected_unreal_bounds_cm']
        actual=bounds(mesh.get_bounds())
        verts=mesh.get_num_vertices(0);triangles=mesh.get_num_triangles(0)
        posecheck=None
        unreal.EditorAssetLibrary.set_metadata_tag(mesh,'AZ.Backpack2.Purpose','Standalone item with baked local pivot.')
        unreal.EditorAssetLibrary.set_metadata_tag(mesh,'AZ.Backpack2.Pivot',static['pivot_description'])
    err=max(abs(actual[side][axis]-expected[side][axis]) for side in ['min','max'] for axis in range(3))
    entry={'asset':assetpath,'group':group,'filename':str(filename),'vertices':verts,'triangles':triangles,'expected_triangles':len(faces),'bounds_cm':actual,'expected_bounds_cm':expected,'max_bounds_error_cm':err,'ref_pose':posecheck,'materials':slotnames,'saved':False}
    report['assets'][assetpath]=entry
    reportpath.write_text(json.dumps(report,indent=2))
    assert triangles==len(faces),'Triangle count changed: '+str(entry)
    assert err<.05,'Import axes/scale or geometry shifted: '+str(entry)
    if posecheck:
        assert maxloc<.05 and maxscale<.0001 and maxangle<.25,'Reference pose shifted: '+str(posecheck)
    unchanged()
    assert unreal.EditorAssetLibrary.save_asset(assetpath,only_if_is_dirty=False),'Mesh save failed'
    if group=='Assembly':
        assert unreal.EditorAssetLibrary.save_asset(skelpath,only_if_is_dirty=False),'Local skeleton save failed'
    entry['saved']=True
    reportpath.write_text(json.dumps(report,indent=2))
    print(json.dumps(entry))
unchanged()
print('BACKPACK_IMPORT_COMPLETE '+str(len(selected)))
gc.collect()
