import json
from pathlib import Path
from collections import Counter,defaultdict
import numpy as np
p=Path(r'C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts/inspection')
g=json.loads((p/'source_geometry.json').read_text());top=json.loads((p/'source_topology.json').read_text());pos=np.array(g['positions'])
cs=[17,121,161,29,70];fs=sorted(f for c in cs for f in top['parts'][c]['faces']);vi=sorted({v for c in cs for v in top['parts'][c]['vertices']})
parent={v:v for v in vi}
def find(v):
    while parent[v]!=v:parent[v]=parent[parent[v]];v=parent[v]
    return v
for i,v in enumerate(vi):
 for ov in vi[:i]:
  if np.linalg.norm(pos[v]-pos[ov])<=1e-6:parent[find(v)]=find(ov)
ec=Counter()
for f in fs:
 poly=g['polygons'][f]
 for a,b in zip(poly,poly[1:]+poly[:1]):ec[tuple(sorted([find(a),find(b)]))]+=1
boundary=[e for e,n in ec.items() if n==1];adj=defaultdict(set)
for a,b in boundary:adj[a].add(b);adj[b].add(a)
todo=set(adj);records=[]
while todo:
 comp=set();stack=[min(todo)]
 while stack:
  v=stack.pop()
  if v in comp:continue
  comp.add(v);stack.extend(adj[v]-comp)
 todo-=comp;edges=[e for e in boundary if e[0] in comp]
 coords=pos[list(comp)]
 records.append({'canonical_source_vertices':sorted(comp),'edge_count':len(edges),'degree2_closed_loop':all(len(adj[v])==2 for v in comp),'world_bounds':[coords.min(0).tolist(),coords.max(0).tolist()],'perimeter_m':sum(float(np.linalg.norm(pos[a]-pos[b])) for a,b in edges)})
records.sort(key=lambda r:r['world_bounds'][0][2])
records[0]['location']='truncated open blade tip'
records[1]['location']='guard/handle interface opening covered by closed wooden handle'
result={'components':cs,'coincident_analysis_tolerance_m':1e-6,'note':'No geometry altered; virtual coordinate union for analysis only. Lower metal component is knife blade/guard, not a sheath, confirmed by native textured multi-angle render. Source blade tip is cut off and open.','boundary_edge_count':len(boundary),'nonmanifold_edges':sum(n>2 for n in ec.values()),'loops':records}
(p/'knife_boundary_audit.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
