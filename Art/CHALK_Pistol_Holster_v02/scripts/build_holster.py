"""Native Blender rebuild of the approved option 01, using the real AZ pistol.

Run in a separate --background --factory-startup process. All output belongs to
this iteration; neither the old holster nor any live Blender scene is loaded.
"""
import bpy, bmesh, math, json, random, sys, hashlib
from pathlib import Path
from mathutils import Vector, Matrix

ROOT=Path('C:/UnrealEngine/Games/AZ/Art/CHALK_Pistol_Holster_v02')
sys.path.insert(0,str(ROOT/'scripts'))
import holster_details as D
from holster_materials import create_materials, rgba

random.seed(2409)
PI=math.pi
OUT=ROOT/'CHALK_Holster01_Rebuilt.blend'
REFERENCE=Path('C:/UnrealEngine/Games/AZ/UI Design/CHALK_Pistol_Holster_v01/concepts/01_Thigh_Canvas_Holster.png')
SOURCE_OLD=Path('C:/UnrealEngine/Games/AZ/Art/CHALK_Pistol_Holster_v01/CHALK_PistolHolster_01_CanvasThigh.blend')
old_hash=hashlib.sha256(SOURCE_OLD.read_bytes()).hexdigest()

print('HOLSTER: initialize isolated scene',flush=True)
for obj in list(bpy.data.objects): bpy.data.objects.remove(obj,do_unlink=True)
scene=bpy.context.scene
scene.name='Holster 01 / rebuilt canvas'
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
scene.render.engine='CYCLES';scene.cycles.samples=32
scene.cycles.use_denoising=True
scene.render.resolution_x=1200;scene.render.resolution_y=1600
scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX'
scene.view_settings.look='AgX - Medium High Contrast'
scene.view_settings.exposure=-2.0
scene.render.image_settings.file_format='PNG'
scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.14,.13,.12,1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value=.55

def collection(name):
    c=bpy.data.collections.new(name);scene.collection.children.link(c);return c
body_col=collection('01 / HOLSTER / shell and lining')
web_col=collection('02 / WEBBING / two thigh bands and suspension')
sew_col=collection('03 / SEAMS / piping and fine stitches')
hardware_col=collection('04 / HARDWARE / rings and single snap')
gun_col=collection('REFERENCE / actual AZ pistol / toggle')
studio_col=collection('STUDIO / cameras and lighting')
guide_col=collection('REFERENCE / thigh and approved image / hidden')
root=bpy.data.objects.new('HOLSTER_ROOT / thigh attachment',None)
body_col.objects.link(root);root.empty_display_type='PLAIN_AXES';root.empty_display_size=.035
root['design']='Approved option01 only. Rounded charcoal canvas, olive straps, one blue patch.'
mats=create_materials(anchor=root,webbing_uv_map='ClothMeters')
for role,weight in [('canvas',.025),('canvas_worn',.025),('webbing',.05),('blue_patch',.035)]:
    for node in mats[role].node_tree.nodes:
        if node.type=='BSDF_PRINCIPLED': node.inputs['Sheen Weight'].default_value=weight

def mesh_obj(name,verts,faces,col,materials,uvs=None,wear=None,sub=0):
    mesh=bpy.data.meshes.new(name+' geometry');mesh.from_pydata(verts,[],faces);mesh.update()
    for mat in materials:mesh.materials.append(mat)
    for face in mesh.polygons:face.use_smooth=True
    if uvs:
        uv=mesh.uv_layers.new(name='ClothMeters')
        for poly in mesh.polygons:
            for li in poly.loop_indices:uv.data[li].uv=uvs[mesh.loops[li].vertex_index]
    if wear:
        a=mesh.attributes.new('holster_edge_wear','FLOAT','POINT');a.data.foreach_set('value',wear)
    obj=bpy.data.objects.new(name,mesh);col.objects.link(obj);obj.parent=root
    if sub:
        mod=obj.modifiers.new('Soft cloth form / editable','SUBSURF');mod.levels=sub;mod.render_levels=sub
    return obj

def lerp_profile(t):
    keys=[(0,.029,.007),(.012,.040,.017),(.040,.051,.026),(.12,.057,.031),(.50,.060,.034),(.90,.061,.033),(1,.063,.034)]
    for a,b in zip(keys,keys[1:]):
        if t<=b[0]:
            f=max(0,min(1,(t-a[0])/(b[0]-a[0])));f=f*f*(3-2*f)
            return (a[1]+f*(b[1]-a[1]),a[2]+f*(b[2]-a[2]))
    return keys[-1][1:]

def surface(theta,t,offset=0):
    rx,ry=lerp_profile(t)
    cs,sn=math.cos(theta),math.sin(theta)
    xunit=math.copysign(abs(cs)**.58,cs)
    yunit=math.copysign(abs(sn)**.72,sn)
    mouth=.111+.021*xunit+.006*yunit
    z=-.135+t*(mouth+.135)
    # Fine low-frequency cloth deformation, strongest around the mouth.
    wave=.00035*math.sin(11*theta+17*t)*math.sin(PI*t)
    wave+=.00065*math.sin(5*theta+.5)*max(0,(t-.66)/.34)**2
    # Small diagonal depressions at the loaded webbing levels.
    crease=.0007*math.sin(25*t+4*theta)*math.exp(-((t-.68)/.13)**2)
    rad=Vector((xunit,yunit,0)).normalized()
    return Vector((rx*xunit+.0012*math.sin(7*t)*math.sin(PI*t),ry*yunit,z))+rad*(wave+crease+offset)

print('HOLSTER: build hollow compound-curved canvas',flush=True)
N=96
ts=sorted(set([0,.008,.016,.025,.04,.065]+[i/40 for i in range(3,40)]+[.987,1]))
verts=[];uvs=[];wear=[]
for inner in (False,True):
    for t in ts:
        for j in range(N):
            th=2*PI*j/N
            p=surface(th,t,offset=-.0026 if inner else 0)
            if inner:p.z+=.003*(1-t)
            verts.append(tuple(p));uvs.append((j/N*.34,t*.26))
            wear.append(.55*max(0,(t-.85)/.15)+.18*max(0,(.10-t)/.10))
rows=len(ts);sidecount=rows*N
faces=[];interior_faces=[]
for inner in (0,1):
    shift=inner*sidecount
    for i in range(rows-1):
        for j in range(N):
            ids=(shift+i*N+j,shift+i*N+(j+1)%N,shift+(i+1)*N+(j+1)%N,shift+(i+1)*N+j)
            faces.append(ids if not inner else ids[::-1])
            if inner:interior_faces.append(len(faces)-1)
for j in range(N):
    a=(rows-1)*N+j;b=(rows-1)*N+(j+1)%N
    faces.append((a,b,b+sidecount,a+sidecount))
faces.append(tuple(range(N-1,-1,-1)))
faces.append(tuple(sidecount+j for j in range(N)))
interior_faces.append(len(faces)-1)
shell=mesh_obj('Canvas pocket / real open cavity',verts,faces,body_col,[mats['canvas'],mats['lining']],uvs,wear,1)
for i in interior_faces:shell.data.polygons[i].material_index=1
bm=bmesh.new();bm.from_mesh(shell.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(shell.data);bm.free()

# A soft curved front skin follows the shell, including the rounded lower end.
U,V=40,56
panel_v=[];panel_f=[];panel_uv=[];panel_wear=[]
def panel(u,v,lift=.00065):
    th=-PI+.30+(PI-.60)*u
    t=.034+.935*v
    return surface(th,t,lift)
for j in range(V+1):
    for i in range(U+1):
        u=i/U;v=j/V;panel_v.append(tuple(panel(u,v)))
        panel_uv.append((u*.130,v*.246))
        boundary=min(u,1-u,v,1-v)
        panel_wear.append(math.exp(-boundary/.029)*(.5+.18*math.sin(u*61+v*53)))
for j in range(V):
    for i in range(U):
        k=j*(U+1)+i;panel_f.append((k,k+1,k+U+2,k+U+1))
front=mesh_obj('Front canvas panel / curved and softly worn',panel_v,panel_f,body_col,[mats['canvas']],panel_uv,panel_wear,1)
solid=front.modifiers.new('Thin sewn front layer','SOLIDIFY');solid.thickness=.0006

def edge_points(inset=0):
    pts=[]
    for j in range(51):pts.append(panel(inset,1-inset-(1-2*inset)*j/50,.0012))
    for i in range(1,61):pts.append(panel(inset+(1-2*inset)*i/60,inset,.0012))
    for j in range(1,51):pts.append(panel(1-inset,inset+(1-2*inset)*j/50,.0012))
    return pts
D.edge_piping(sew_col,'Rolled fabric edge / continuous U seam',mats['binding'],edge_points(.003),radius=.00145,smooth=False,parent=root)
for k,inset in enumerate((.029,.061)):
    D.stitch_path(sew_col,f'Front seam / fine saddle row {k+1}',mats['thread'],edge_points(inset),
       surface_normals=(0,-1,0),dash_length=.0020,gap=.00085,thread_radius=.00017,
       arch_height=.000065,samples_per_segment=1,smooth_path=False,parent=root)
mouth=[surface(2*PI*j/N,1,.0003) for j in range(N)]
D.edge_piping(sew_col,'Soft rolled open mouth rim',mats['binding'],mouth,radius=.0018,closed=True,smooth=False,parent=root)
D.stitch_path(sew_col,'Mouth seam / below rolled rim',mats['thread'],[surface(2*PI*j/N,.977,.00065) for j in range(N)],
    surface_normals=[(math.cos(2*PI*j/N),math.sin(2*PI*j/N),0) for j in range(N)],
    dash_length=.0018,gap=.0009,thread_radius=.00016,closed=True,samples_per_segment=1,smooth_path=False,parent=root)

def sewn_ribbon(name,centers,width_direction,width=.03,thickness=.0018,closed=False,sub=1,stitches=True):
    obj=D.ribbon(web_col,name,mats['webbing'],centers,width_direction,width,thickness,
                 subdivision=sub,samples_per_segment=2,closed=closed,parent=root)
    if stitches:
        left,right,normals=D.ribbon_edge_paths(centers,width_direction,width,inset=.0020,
                              surface_lift=thickness/2+.00009,samples_per_segment=2,closed=closed)
        for n,pts in enumerate((left,right)):
            D.stitch_path(sew_col,name+f' / edge stitching {n+1}',mats['thread'],pts,surface_normals=normals,
                dash_length=.0022,gap=.0010,thread_radius=.00014,arch_height=.00004,
                closed=closed,samples_per_segment=1,smooth_path=False,parent=root)
    return obj

print('HOLSTER: sew folded webbing and rounded hardware',flush=True)
# Pouch on the outer side of a right thigh; the loops curve behind the body.
cx,cy=-.044,.106
for band,z in [('Upper',.036),('Lower',-.078)]:
    centers=[]
    for j in range(49):
        th=-PI/2+2*PI*j/48
        centers.append((cx+.091*math.cos(th),cy+.081*math.sin(th),z+.003*math.cos(th)))
    sewn_ribbon(band+' thigh band / continuous soft loop',centers,(0,0,1),.031,closed=True)
    # The exposed strap terminal folds around the adjustment bar and doubles back.
    terminal=[(-.025,.027,z),(-.054,.009,z),(-.080,-.008,z),(-.103,-.010,z),
              (-.112,-.010,z),(-.115,-.014,z),(-.110,-.018,z),(-.090,-.019,z),(-.068,-.018,z)]
    sewn_ribbon(band+' band / folded adjustment terminal',terminal,(0,0,1),.030)
    D.rounded_rectangle_buckle(hardware_col,band+' blackened slider / rounded continuous frame',mats['metal_dark'],
         (-.074,-.024,z),outer_width=.021,outer_height=.039,corner_radius=.0048,wire_radius=.00165,
         center_bar=True,parent=root)
    D.box_x_stitch(sew_col,band+' band / box X reinforced end',mats['thread'],(-.101,-.0204,z),
         width=.026,height=.023,surface_normal=(0,-1,0),thread_radius=.00015,parent=root)
    # Compact fabric anchors reach the rear of the pocket; no four protruding blocks.
    anchor=[(.045,.035,z),(.064,.024,z),(.067,.012,z),(.061,.002,z),(.049,-.009,z)]
    sewn_ribbon(band+' band / sewn right attachment',anchor,(0,0,1),.029)

# Main vertical suspension, with a curved return around its upper loop.
vertical=[(.011,.033,.076),(.011,.042,.116),(.011,.040,.173),(.011,.030,.228),
          (.011,.024,.289),(.011,.019,.354),(.011,.018,.378),(.011,.014,.386),
          (.011,.005,.386),(.011,-.001,.380),(.011,-.002,.356),(.011,-.001,.322)]
sewn_ribbon('Belt suspension / top folded loop',vertical,(1,0,0),.034,stitches=True)
D.rounded_rectangle_buckle(hardware_col,'Belt adjuster / upper rounded slider',mats['metal_dark'],
     (.011,-.003,.313),outer_width=.039,outer_height=.025,corner_radius=.005,wire_radius=.0018,
     center_bar=True,parent=root)
sewn_ribbon('Belt suspension / doubled adjustment tail',[(.011,.017,.326),(.011,.009,.308),(.011,.004,.288),(.011,.004,.262)],(1,0,0),.033)
for z in (.351,.275,.211):
    D.box_x_stitch(sew_col,f'Belt suspension / reinforced box {z}',mats['thread'],(.011,-.002 if z>.33 else .026,z),
       width=.026,height=.028,surface_normal=(0,-1,0),thread_radius=.00015,parent=root)

# Flexible retention tab rises from the single visible snap, curves over the
# back of the pistol, and attaches behind the holster mouth.
retention=[(.012,-.036,.060),(.012,-.038,.079),(.012,-.038,.107),(.013,-.038,.138),
           (.014,-.034,.163),(.014,-.024,.178),(.014,-.005,.181),(.014,.019,.180),
           (.014,.037,.166),(.014,.041,.139),(.014,.038,.112)]
sewn_ribbon('Retention tab / curved snap fastening',retention,(1,0,0),.026,.0020)
D.box_x_stitch(sew_col,'Retention tab / lower box stitch',mats['thread'],(.012,-.0394,.072),
      width=.019,height=.015,surface_normal=(0,-1,0),thread_radius=.00014,parent=root)

def sphere_button(name,location,scale,mat):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32,ring_count=16,location=location)
    obj=bpy.context.object;obj.name=name
    for c in list(obj.users_collection):c.objects.unlink(obj)
    hardware_col.objects.link(obj);obj.parent=root
    obj.scale=scale;obj.data.materials.append(mat)
    for f in obj.data.polygons:f.use_smooth=True
    return obj
button=sphere_button('Single weathered steel snap / domed cap',(.012,-.041,.096),(.0075,.0028,.0075),mats['metal'])
sphere_button('Snap / dark recessed rim',(.012,-.0398,.096),(.0081,.0017,.0081),mats['metal_dark'])

print('HOLSTER: blue patch, real thread and fabric edge wear',flush=True)
# The asymmetric repair is sewn to the rounded front, not a floating cube.
def patchpoint(u,v,lift=.0018):
    # Lower right front of the pouch, about 28 x 39 mm.
    theta=-1.03+.47*u
    t=.25+.17*v
    p=surface(theta,t,lift)
    p.z+=.00055*math.sin(u*8+v*5)*math.sin(PI*u)
    return p
pv=[];pf=[];puv=[];pw=[];nx,ny=14,20
for j in range(ny+1):
    for i in range(nx+1):
        u=i/nx;v=j/ny;pv.append(tuple(patchpoint(u,v)));puv.append((u*.028,v*.039))
        pw.append(math.exp(-min(u,1-u,v,1-v)/.05)*.35)
for j in range(ny):
    for i in range(nx):
        k=j*(nx+1)+i;pf.append((k,k+1,k+nx+2,k+nx+1))
patch=mesh_obj('Blue field repair / frayed cloth patch',pv,pf,body_col,[mats['blue_patch']],puv,pw,1)
s=patch.modifiers.new('Patch fabric thickness','SOLIDIFY');s.thickness=.00055
patch_border=[patchpoint(.07,1-.06-i*.88/20,.0024) for i in range(21)]
patch_border += [patchpoint(.07+i*.86/20,.06,.0024) for i in range(1,21)]
patch_border += [patchpoint(.93,.06+i*.88/20,.0024) for i in range(1,21)]
patch_border += [patchpoint(.93-i*.86/20,.94,.0024) for i in range(1,21)]
D.stitch_path(sew_col,'Blue repair / hand stitched perimeter',mats['thread'],patch_border,
  surface_normals=(0,-1,0),dash_length=.0017,gap=.0010,thread_radius=.00016,
  closed=True,samples_per_segment=1,smooth_path=False,parent=root)
frays=[]
for i in range(18):
    u=random.choice([.01,.99]);v=random.uniform(.04,.95)
    p=patchpoint(u,v,.0021);q=p+Vector((random.choice([-1,1])*.0015,-.00035,random.uniform(-.0014,.0014)))
    frays.append((p,q))
for i,(p,q) in enumerate(frays):
    D.edge_piping(sew_col,f'Patch edge / loose yarn {i:02}',mats['blue_patch'],[p,q],radius=.000075,bevel_resolution=1,parent=root)

# Actual game pistol, kept separate so the empty holster is also inspectable.
print('HOLSTER: place actual AZ pistol reference at original scale',flush=True)
data=json.loads((ROOT/'references/pistol_geometry.json').read_text())['objects'][0]
R=Matrix(((0,0,1),(1,0,0),(0,1,0)))
T=Vector((-.075,0,.150))
gunverts=[tuple(R@Vector(p)+T) for p in data['positions_world_m']]
gunmesh=bpy.data.meshes.new('Actual AZ pistol reference geometry')
gunmesh.from_pydata(gunverts,[],data['polygons']);gunmesh.update()
gun=bpy.data.objects.new('REFERENCE / actual AZ pistol / toggle visibility',gunmesh);gun_col.objects.link(gun)
gun['source_asset']='/Game/AZ/Blueprints/Items/Equippables/Weapons/Pistol/SM_Pistol_Pickup'
gun['source_scale_preserved']=True
uvkeys=list(data.get('uv_layers',{}))
print('Pistol UV schema '+str(uvkeys),flush=True)
# UV arrays from the native snapshot are stored per polygon in the reference JSON.
if uvkeys:
    source_uv=data['uv_layers'][uvkeys[0]]
    uv=gunmesh.uv_layers.new(name='UVMap')
    if len(source_uv)==len(gunmesh.polygons):
        for poly,coords in zip(gunmesh.polygons,source_uv):
            for li,co in zip(poly.loop_indices,coords):uv.data[li].uv=co
    else:
        for li,co in enumerate(source_uv):uv.data[li].uv=co
gunmat=bpy.data.materials.new('REFERENCE / original pistol base color');gunmat.use_nodes=True
bsdf=gunmat.node_tree.nodes.get('Principled BSDF');bsdf.inputs['Roughness'].default_value=.51
tex=gunmat.node_tree.nodes.new('ShaderNodeTexImage')
tex.image=bpy.data.images.load(str(ROOT/'references/Pistol_BaseColor.png'));tex.image.pack()
gunmat.node_tree.links.new(tex.outputs['Color'],bsdf.inputs['Base Color']);gunmesh.materials.append(gunmat)
for p in gunmesh.polygons:p.use_smooth=True
gunmesh.normals_split_custom_set([tuple(R@Vector(n)) for poly in data['polygon_corner_normals'] for n in poly])

# Standalone presentation with the reference lighting fixtures hidden in viewport.
print('HOLSTER: studio and editable reference setup',flush=True)
def area(name,loc,power,size,color):
    d=bpy.data.lights.new(name,'AREA');d.energy=power;d.shape='DISK';d.size=size;d.color=color
    o=bpy.data.objects.new(name,d);studio_col.objects.link(o);o.location=loc
    o.rotation_euler=(Vector((0,0,.10))-o.location).to_track_quat('-Z','Y').to_euler()
area('Key / large soft source',(-.38,-.48,.70),52,.75,(1,.94,.85))
area('Fill / cool broad source',(.48,-.24,.34),30,.55,(.86,.91,1))
area('Rim / upper fabric edge',(-.2,.46,.55),65,.50,(1,.95,.87))
cd=bpy.data.cameras.new('Camera / option01 three-quarter');cam=bpy.data.objects.new(cd.name,cd)
studio_col.objects.link(cam);cam.location=(.43,-.98,.40)
target=Vector((-.020,.025,.123));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
cd.type='ORTHO';cd.ortho_scale=.64;scene.camera=cam
scene.render.film_transparent=False

# Real-scale thigh clearance guide, invisible in both viewport and rendering.
bpy.ops.mesh.primitive_uv_sphere_add(segments=48,ring_count=32,location=(cx,cy,.008))
guide=bpy.context.object;guide.name='REFERENCE / thigh envelope / hidden'
for c in list(guide.users_collection):c.objects.unlink(guide)
guide_col.objects.link(guide);guide.scale=(.083,.072,.23);guide.hide_render=True;guide.hide_set(True)
image=bpy.data.images.load(str(REFERENCE));image.pack();image.use_fake_user=True
image.name='APPROVED / option01 / packed source image'
ref=bpy.data.objects.new('REFERENCE / approved option01 image',None);guide_col.objects.link(ref)
ref.empty_display_type='IMAGE';ref.data=image;ref.empty_display_size=.7;ref.location=(.55,.3,.10)
ref.hide_render=True;ref.hide_set(True)

for ob in list(bpy.context.selected_objects):ob.select_set(False)
shell.select_set(True);bpy.context.view_layer.objects.active=shell
for screen in bpy.data.screens:
    for ar in screen.areas:
        if ar.type=='VIEW_3D':
            sp=ar.spaces.active;sp.shading.type='MATERIAL';sp.shading.use_scene_world=False
            sp.shading.use_scene_lights=False;sp.overlay.show_overlays=False
            sp.lens=50;sp.clip_start=.001;sp.region_3d.view_location=target
            sp.region_3d.view_rotation=cam.rotation_euler.to_quaternion()
            sp.region_3d.view_distance=.85;sp.region_3d.view_perspective='ORTHO'
for ob in studio_col.objects:ob.hide_set(True)

readme=bpy.data.texts.new('START HERE / holster rebuild')
readme.write('Option01 rebuilt from the approved canvas-thigh concept. The main pocket is hollow with a real inner lining. Two folded webbing thigh loops, a folded belt suspension, a curved retention tab with one domed snap, fine saddle stitches and blue sewn repair are modeled. Collections01-04 are the holster. The actual AZ pistol is a separate reference collection; hide it to see the empty cavity. Studio/reference helpers are hidden in the working viewport. Procedural Blender materials are editable; Unreal baking/rigging/import is a later step. Original v01 and character/backpack files unchanged.')
bm=bmesh.new();bm.from_mesh(shell.data)
shell_audit={'boundary_edges':sum(e.is_boundary for e in bm.edges),'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges),'wall_volume_m3':abs(bm.calc_volume(signed=True))}
bm.free()
assert shell_audit['nonmanifold_edges']==0 and shell_audit['wall_volume_m3']>0
model_objects=[o for c in [body_col,web_col,sew_col,hardware_col] for o in c.objects if o!=root]
assert all(o.parent==root for o in model_objects)
assert all(all(math.isfinite(x) for x in v.co) for o in model_objects if o.type=='MESH' for v in o.data.vertices)
assert all(i.packed_file for i in bpy.data.images if i.type=='IMAGE')
manifest={'file':str(OUT),'approved_concept':str(REFERENCE),'old_file_unchanged':hashlib.sha256(SOURCE_OLD.read_bytes()).hexdigest()==old_hash,
    'holster_collections':[c.name for c in [body_col,web_col,sew_col,hardware_col]],
    'holster_objects':sum(len(c.objects) for c in [body_col,web_col,sew_col,hardware_col]),
    'native_geometry':True,'hollow_pocket':True,'thigh_straps':2,'single_retention_snap':True,
    'actual_pistol_reference':gun['source_asset'],'pistol_scale_preserved':True,'root':root.name,
    'material_status':'Native procedural cloth, webbing and scratched metal. No texture bake/Unreal import yet.',
    'shell_audit':shell_audit,'all_holster_parts_follow_root':True,'packed_reference_images':True}
(ROOT/'build_manifest.json').write_text(json.dumps(manifest,indent=2))
assert manifest['old_file_unchanged']
scene.render.filepath=str(ROOT/'Holster01_Rebuilt_Preview.png')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT),check_existing=False)
print('HOLSTER: render with pistol',flush=True)
bpy.ops.render.render(write_still=True)
# The second real render makes the empty cavity and inner lining reviewable.
gun.hide_render=True
scene.render.filepath=str(ROOT/'Holster01_Rebuilt_Empty.png')
bpy.ops.render.render(write_still=True)
gun.hide_render=False
scene.render.filepath=str(ROOT/'Holster01_Rebuilt_Preview.png')
print('HOLSTER: complete '+str(OUT),flush=True)
