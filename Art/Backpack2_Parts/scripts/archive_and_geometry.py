import bpy, json, hashlib
from pathlib import Path

out=Path('C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts')
obj=bpy.data.objects['SKM_SurvivalMan_backpack2']
if obj.mode=='EDIT': obj.update_from_editmode()
source=Path(bpy.data.filepath) if bpy.data.filepath else None
ue=Path('C:/UnrealEngine/Games/AZ/Content/SurvivalMan/Meshes/Parts/Without_a_jacket/SKM_SurvivalMan_backpack2.uasset')
archive=out/'Backpack2_Source_Archive.blend'
assert not archive.exists(), 'Source archive already exists; do not overwrite it'
record={'active_file':str(source) if source else None,'active_file_sha256':hashlib.sha256(source.read_bytes()).hexdigest() if source and source.is_file() else None,
        'unreal_source':str(ue),'unreal_sha256':hashlib.sha256(ue.read_bytes()).hexdigest(),
        'archive':str(archive),'source_object':obj.name,'source_mode':obj.mode}
bpy.ops.wm.save_as_mainfile(filepath=str(archive),copy=True)
record['archive_sha256']=hashlib.sha256(archive.read_bytes()).hexdigest()
(out/'inspection/source_archive.json').write_text(json.dumps(record,indent=2),encoding='utf-8')
mesh=obj.data
geometry={'positions':[list(obj.matrix_world @ v.co) for v in mesh.vertices],
          'local_positions':[list(v.co) for v in mesh.vertices],
          'edges':[list(e.vertices) for e in mesh.edges],
          'polygons':[list(p.vertices) for p in mesh.polygons],
          'materials':[p.material_index for p in mesh.polygons]}
(out/'inspection/source_geometry.json').write_text(json.dumps(geometry),encoding='utf-8')
print(json.dumps({'archive':str(archive),'vertices':len(mesh.vertices),'faces':len(mesh.polygons)}))
