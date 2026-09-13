import json, math
from pathlib import Path
from collections import defaultdict, Counter
import numpy as np
p=Path(r'C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts/inspection')
t=json.loads((p/'source_topology.json').read_text()); g=json.loads((p/'source_geometry.json').read_text());w=json.loads((p/'source_weights.json').read_text())
pos=np.array(g['positions']); parts=t['parts']; vc={v:a['id'] for a in parts for v in a['vertices']}
parent=list(range(len(parts)))
def find(a):
    while parent[a]!=a:
        parent[a]=parent[parent[a]];a=parent[a]
    return a
def union(a,b):
    a,b=find(a),find(b)
    if a!=b:parent[max(a,b)]=min(a,b)
buckets=defaultdict(list);seams=Counter()
tol=1e-6
for v,po in enumerate(pos):
    key=tuple(int(math.floor(x/tol)) for x in po)
    for dx in (-1,0,1):
      for dy in (-1,0,1):
       for dz in (-1,0,1):
        for ov in buckets[(key[0]+dx,key[1]+dy,key[2]+dz)]:
         if vc[v]!=vc[ov] and np.linalg.norm(po-pos[ov])<=tol:
          union(vc[v],vc[ov]);seams[tuple(sorted([vc[v],vc[ov]]))]+=1
    buckets[key].append(v)
sets=defaultdict(list)
for a in parts:sets[find(a['id'])].append(a['id'])
records=[]
for cs in sets.values():
    vs=[v for c in cs for v in parts[c]['vertices']]; fs=[f for c in cs for f in parts[c]['faces']]
    xyz=pos[vs];cnt=Counter()
    for v in vs:
        for group,weight in w['weights'][v]:cnt[w['groups'][group]]+=weight
    rec={'id':min(cs),'components':cs,'vertex_count':len(vs),'face_count':len(fs),'bounds':[xyz.min(axis=0).tolist(),xyz.max(axis=0).tolist()], 'weights':[(k,round(v/len(vs),3)) for k,v in cnt.most_common()]}
    records.append(rec)
    print(f"S{rec['id']:3} C={','.join(map(str,cs))} V={len(vs):4} F={len(fs):4} bounds={np.round(np.array(rec['bounds']),3).tolist()} weights={rec['weights']}")
(p/'coincident_component_sets.json').write_text(json.dumps({'tolerance_world_m':tol,'sets':records,'seams':[[a,b,n] for (a,b),n in seams.items()]},indent=2))
print('COINCIDENT SET COUNT',len(records))
print('BONES',json.dumps(w.get('bones')))
