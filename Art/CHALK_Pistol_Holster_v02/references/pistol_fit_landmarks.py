import json
from pathlib import Path
from collections import defaultdict
import numpy as np
out=Path(r'C:/UnrealEngine/Games/AZ/Art/CHALK_Pistol_Holster_v02/references')
d=json.loads((out/'pistol_geometry.json').read_text())['objects'][0];p=np.array(d['positions_world_m']);parents=list(range(len(p)))
def find(v):
    while parents[v]!=v:parents[v]=parents[parents[v]];v=parents[v]
    return v
def union(a,b):
    a,b=find(a),find(b)
    if a!=b:parents[b]=a
for a,b in d['edges']:union(a,b)
lookup={}
for i,po in enumerate(p):
    key=tuple(np.round(po,6))
    if key in lookup:union(i,lookup[key])
    lookup[key]=i
comps=defaultdict(list)
for i in range(len(p)):comps[find(i)].append(i)
bymin={min(vs):vs for vs in comps.values()}
def region(roots):
    vs=sorted(v for root in roots for v in bymin[root]);po=p[vs]
    return {'source_vertex_ids':vs,'bounds_m':[po.min(0).tolist(),po.max(0).tolist()],'bounds_center_m':((po.min(0)+po.max(0))/2).tolist()}
slide=region([99]);barrel=region([380]);guard=region([0,273]);grip=region([56]);mag=region([578])
muzzle=[0,float(p[bymin[380],1].min()),float((p[bymin[380],2].min()+p[bymin[380],2].max())/2)]
tipverts=np.where(p[:,2]<p[:,2].min()+.0015)[0];tip=p[tipverts].mean(0).tolist()
base=p[p[:,2]<-.042];base_center=((base.min(0)+base.max(0))/2).tolist()
sections=[]
for y in np.arange(-.255,.046,.01):
    cuts=[]
    for tri in d['triangles']:
        xyz=p[tri]
        if y<xyz[:,1].min() or y>xyz[:,1].max():continue
        for a,b in zip(xyz,np.roll(xyz,-1,axis=0)):
            if (a[1]<=y<=b[1] or b[1]<=y<=a[1]) and abs(b[1]-a[1])>1e-12:
                v=a+(b-a)*((y-a[1])/(b[1]-a[1]));cuts.append(v)
    if cuts:
        po=np.array(cuts);sections.append({'source_y_m':round(float(y),6),'x_min_m':float(po[:,0].min()),'x_max_m':float(po[:,0].max()),'z_min_m':float(po[:,2].min()),'z_max_m':float(po[:,2].max())})
result={'coordinate_space':'Blender world metres, exactly as pistol_geometry.json','ue_mapping_cm':'UE=(Blender.x,-Blender.y,Blender.z)*100','barrel_direction_toward_muzzle':[0,-1,0],'lateral_axis':[1,0,0],'gun_up_axis':[0,0,1],'muzzle_axis_center_m':muzzle,'overall_bounds_m':[p.min(0).tolist(),p.max(0).tolist()],'overall_dimensions_m':np.ptp(p,axis=0).tolist(),'grip_tip_lowest_edge_center_m':tip,'grip_base_center_m':base_center,'grip_tip_source_vertex_ids':tipverts.tolist(),'regions':{'slide':slide,'barrel_muzzle':barrel,'trigger_guard_outer':guard,'grip_frame':grip,'magazine_and_internal':mag},'cross_sections_perpendicular_barrel':sections,'notes':['All region IDs refer to raw source vertices in pistol_geometry.json. Regions inferred from native textured side/top views plus connected topology.','The 31.20cm weapon is long; its housing should be sized from actual mesh rather than a generic small-pistol cavity.','A muzzle-down holster basis can use local X=sourceZ, local Y=sourceX, local Z=sourceY (cyclic permutation, det+1), then choose an insertion origin. This is a fitting basis suggestion, not a source transform.','Cross-sections are outer min/max envelopes of exact triangle-plane intersections; they include trigger/grip wherever those cross the plane, so choose the opening position and cloth cutout intentionally.','Preview material uses exported original base color; normal/mask textures are not required for fit. Source Unreal asset and live Blender were unchanged.']}
(out/'pistol_fit_landmarks.json').write_text(json.dumps(result,indent=2))
print(json.dumps({k:v for k,v in result.items() if k not in ['regions','cross_sections_perpendicular_barrel','notes']}))
print('REGION_BOUNDS',json.dumps({k:v['bounds_m'] for k,v in result['regions'].items()}))
