import json,math,sys
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
p=Path(r'C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts/inspection');g=json.loads((p/'source_geometry.json').read_text());s=json.loads((p/'coincident_component_sets.json').read_text());t=json.loads((p/'source_topology.json').read_text())
pos=g['positions'];parts=t['parts'];sets={a['id']:a['components'] for a in s['sets']}
items=[('S9',[9]),('AXE S11 S133 S167',[11,133,167]),('S17 handle',[17]),('S29',[29]),('S28 S98',[28,98]),('S30 S100',[30,100]),('S15 bag',[15]),('S33 34 35 60 142 harness',[33,34,35,60,142]),('S0 S47 dangling straps',[0,47]),('S1 S63 bottom harness',[1,63]),('S16 S67 buckles',[16,67]),('S21',[21])]
second=len(sys.argv)>1
if second:items=[('S17+S29 knife assembly',[17,29]),('S30 pouch shell',[30]),('C100',[100]),('S9 rope',[9]),('S3 S13',[3,13]),('S23 S31',[23,31]),('S24',[24]),('S37 S46 S85',[37,46,85]),('S18 S57',[18,57]),('S123 S102',[123,102]),('S32 clasp',[32]),('S21 + bottle',[21,28,98])]
font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',18)
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def sub(a,b):return [x-y for x,y in zip(a,b)]
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def normal(v):
 l=math.sqrt(dot(v,v));return [x/l for x in v] if l else [0,0,1]
u=normal([.86,.51,0]);v=normal([-.09,.15,.985]);depth=cross(u,v)
out=Image.new('RGB',(1600,1500),'white');dr=ImageDraw.Draw(out)
for i,(name,ss) in enumerate(items):
 x0=(i%4)*400;y0=(i//4)*500;cs=[c for a in ss for c in sets[a]];fs=[f for c in cs for f in parts[c]['faces']];vs={v for f in fs for v in g['polygons'][f]}
 xy={a:[dot(pos[a],u),dot(pos[a],v)] for a in vs};xmin=min(x[0] for x in xy.values());xmax=max(x[0] for x in xy.values());ymin=min(x[1] for x in xy.values());ymax=max(x[1] for x in xy.values());sc=min(360/(xmax-xmin),400/(ymax-ymin))
 def coord(a):return (x0+200+(xy[a][0]-(xmin+xmax)/2)*sc,y0+260-(xy[a][1]-(ymin+ymax)/2)*sc)
 for f in sorted(fs,key=lambda f:sum(dot(pos[a],depth) for a in g['polygons'][f])/len(g['polygons'][f])):
  poly=g['polygons'][f];n=normal(cross(sub(pos[poly[1]],pos[poly[0]]),sub(pos[poly[2]],pos[poly[0]])));shade=105+int(115*abs(dot(n,normal([.3,.5,.8]))));dr.polygon([coord(a) for a in poly],fill=(shade,min(255,shade+10),min(255,shade+20)),outline=(70,80,90))
 dr.text((x0+15,y0+12),name,font=font,fill='black');dr.text((x0+15,y0+470),'C='+','.join(map(str,cs)),font=font,fill='black');dr.rectangle((x0,y0,x0+399,y0+499),outline=(180,180,180))
name='topology_detail_panels.png' if second else 'topology_candidate_panels.png'
out.save(p/name)
print(str(p/name))
