import bpy, json
from pathlib import Path
obj=bpy.data.objects['SKM_SurvivalMan_backpack2']
data={'groups':[g.name for g in obj.vertex_groups], 'weights':[[[g.group,g.weight] for g in v.groups] for v in obj.data.vertices]}
if obj.parent and obj.parent.type=='ARMATURE':
    data['bones']={b.name:{'head':list(obj.parent.matrix_world @ b.head_local),'tail':list(obj.parent.matrix_world @ b.tail_local)} for b in obj.parent.data.bones if b.name in data['groups']}
Path(r'C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts/inspection/source_weights.json').write_text(json.dumps(data))
print('Exported read-only vertex weights and bone rest coordinates: '+str(len(data['weights']))+' vertices')
