import bpy
import json
from pathlib import Path
from collections import Counter

root = Path('C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts')
(root / 'inspection').mkdir(parents=True, exist_ok=True)
obj = bpy.data.objects.get('SKM_SurvivalMan_backpack2')
assert obj and obj.type == 'MESH', 'Expected imported backpack mesh is missing'
if obj.mode == 'EDIT':
    obj.update_from_editmode()
mesh = obj.data
parent = list(range(len(mesh.vertices)))
def find(i):
    while parent[i] != i:
        parent[i] = parent[parent[i]]
        i = parent[i]
    return i
def union(a,b):
    a,b = find(a),find(b)
    if a != b:
        parent[b] = a
for edge in mesh.edges:
    union(*edge.vertices)
groups = {}
for vertex in mesh.vertices:
    groups.setdefault(find(vertex.index), []).append(vertex.index)
parts=[]
for index, vertices in enumerate(sorted(groups.values(), key=lambda v:min(v))):
    included=set(vertices)
    polygons=[p for p in mesh.polygons if p.vertices[0] in included]
    points=[obj.matrix_world @ mesh.vertices[i].co for i in vertices]
    lo=[min(p[a] for p in points) for a in range(3)]
    hi=[max(p[a] for p in points) for a in range(3)]
    parts.append({'id':index,'vertices':vertices,'faces':[p.index for p in polygons],
                  'vertex_count':len(vertices),'face_count':len(polygons),
                  'bounds':[lo,hi], 'center':[(lo[a]+hi[a])/2 for a in range(3)],
                  'materials':dict(Counter(p.material_index for p in polygons))})
report={'filepath':bpy.data.filepath,'scene':bpy.context.scene.name,'object':obj.name,
        'mesh':mesh.name,'vertices':len(mesh.vertices),'edges':len(mesh.edges),
        'faces':len(mesh.polygons),'uv_layers':[u.name for u in mesh.uv_layers],
        'materials':[m.name if m else None for m in mesh.materials],
        'images':[{'name':i.name,'path':i.filepath,'packed':bool(i.packed_file)} for i in bpy.data.images],
        'parent':obj.parent.name if obj.parent else None,
        'modifiers':[{'name':m.name,'type':m.type,'object':m.object.name if m.type=='ARMATURE' and m.object else None} for m in obj.modifiers],
        'vertex_groups':[g.name for g in obj.vertex_groups],
        'matrix_world':[list(row) for row in obj.matrix_world],
        'parts':parts}
(root/'inspection/source_topology.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k not in ('parts','vertex_groups','images')}))
print(json.dumps({'parts':[{'id':p['id'],'v':p['vertex_count'],'f':p['face_count'],'center':p['center'],'bounds':p['bounds'],'materials':p['materials']} for p in parts]}))
