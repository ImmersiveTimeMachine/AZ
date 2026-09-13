# @Description: Verify final backpack materials, save imported assets and show the standalone axe
import unreal,json,gc,hashlib
from pathlib import Path
ROOT=Path('C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts')
reportpath=ROOT/'inspection/unreal_import_report.json'
report=json.loads(reportpath.read_text())
matpaths=['/Game/SurvivalMan/MAT/MaterialInstance/Clothes/MI_SurvivalMan_Backpack_Inst',
          '/Game/SurvivalMan/MAT/MaterialInstance/Clothes/MI_SurvivalMan_Jacket_Inst']
materials=[unreal.load_asset(p) for p in matpaths]
for path,entry in report['assets'].items():
    mesh=unreal.load_asset(path)
    prop='materials' if entry['group']=='Assembly' else 'static_materials'
    slots=mesh.get_editor_property(prop)
    for index,slot in enumerate(slots):
        material=materials[1] if 'Jacket' in str(slot.get_editor_property('material_slot_name')) else materials[0]
        if entry['group']=='Items':
            mesh.set_material(index,material)
        else:
            slot.set_editor_property('material_interface',material)
            slots[index]=slot
    if entry['group']=='Assembly': mesh.set_editor_property(prop,slots)
    actual=mesh.get_editor_property(prop)
    assert len(actual)==len(entry['materials'])
    for slot,expected in zip(actual,entry['materials']):
        assert slot.get_editor_property('material_interface').get_path_name()==expected['material'],path+' wrong material'
    assert unreal.EditorAssetLibrary.save_asset(path,only_if_is_dirty=False)
    entry['material_getters_verified']=True
    entry['saved']=True
    if entry['group']=='Items':
        entry['simple_collision_primitives']=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_simple_collision_count(mesh)
        data=mesh.get_editor_property('asset_import_data')
        entry['import_offset']={'translation':str(data.get_editor_property('import_translation')),'rotation':str(data.get_editor_property('import_rotation')),'scale':data.get_editor_property('import_uniform_scale')}
originals=json.loads((ROOT/'textures/source_material_audit.json').read_text())['source_sha256_after']
assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in originals.items())
report['source_originals_unchanged']=True
report['final_materials_verified']=True
reportpath.write_text(json.dumps(report,indent=2))
unreal.EditorAssetLibrary.sync_browser_to_objects(['/Game/AZ/Assets/Items/Backpack2/Meshes/Items/SM_Backpack2_Axe'])
print(json.dumps({'saved_meshes':len(report['assets']),'material_getters_verified':True,'originals_unchanged':True,'items_with_simple_collision':sum(e.get('simple_collision_primitives',0)>0 for e in report['assets'].values())}))
gc.collect()
