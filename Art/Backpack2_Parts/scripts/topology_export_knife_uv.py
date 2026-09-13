import bpy,json
from pathlib import Path
root=Path(r'C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts')
obj=bpy.data.objects['SKM_SurvivalMan_backpack2']
top=json.loads((root/'inspection/source_topology.json').read_text())
data={}
for name,cs in [('handle',[17,121,161]),('blade_guard',[29,70])]:
    fs=sorted(f for c in cs for f in top['parts'][c]['faces'])
    data[name]={'components':cs,'faces':[{'face':f,'vertices':list(obj.data.polygons[f].vertices),'uv':[list(obj.data.uv_layers.active.data[li].uv) for li in obj.data.polygons[f].loop_indices]} for f in fs]}
(root/'inspection/knife_source_uv.json').write_text(json.dumps(data,indent=2))
print('Read-only knife UV export complete')
