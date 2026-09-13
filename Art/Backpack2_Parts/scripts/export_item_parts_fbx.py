"""Create standalone item meshes with baked pivots in a separate Blender process."""
import bpy,json,hashlib
from pathlib import Path
from mathutils import Vector,Matrix
from io_scene_fbx import parse_fbx

ROOT=Path('C:/UnrealEngine/Games/AZ/Art/Backpack2_Parts')
SOURCE=ROOT/'Backpack2_Separated.blend'
DEST=ROOT/'Backpack2_GameReady.blend'
OUT=ROOT/'exports/Items'
assert not DEST.exists(),'Do not replace an existing game-ready file'
source_hash=hashlib.sha256(SOURCE.read_bytes()).hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
manifest=json.loads((ROOT/'inspection/item_pivots.json').read_text())
OUT.mkdir(parents=True,exist_ok=True)
scene=bpy.data.scenes.new('BP2_GameItems')
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1.0
bpy.context.window.scene=scene
collection=bpy.data.collections.new('BP2_Items_LocalPivots')
scene.collection.children.link(collection)
report={'source_blend':str(SOURCE),'source_blend_sha256':source_hash,'blender_file':str(DEST),
        'axes':'FBX Y forward/Z up; Unreal coords=(X,-Y,Z)*100 cm','parts':{}}
def elem(tree,name): return next(e for e in tree.elems if e.id==name)
created=[]
for name,pivot in manifest['parts'].items():
    src=bpy.data.objects['BP2_'+name]
    R=Matrix(pivot['rotation_rows']);origin=Vector(pivot['origin_world_m'])
    assert abs(R.determinant()-1)<1e-5
    obj=src.copy();obj.data=src.data.copy();obj.parent=None
    obj.name='SM_Backpack2_'+name;obj.data.name=obj.name+'_Mesh'
    obj.matrix_world=Matrix.Identity(4)
    collection.objects.link(obj)
    for modifier in list(obj.modifiers): obj.modifiers.remove(modifier)
    obj.vertex_groups.clear()
    original_normals=[Vector(n.vector) for n in src.data.corner_normals]
    normal_matrix=src.matrix_world.to_3x3().inverted().transposed()
    normals=[(R @ normal_matrix @ n).normalized() for n in original_normals]
    for vertex,old_vertex in zip(obj.data.vertices,src.data.vertices):
        vertex.co=R @ (src.matrix_world @ old_vertex.co-origin)
    attr=obj.data.attributes.get('custom_normal')
    assert attr and attr.domain=='CORNER' and attr.data_type=='FLOAT_VECTOR'
    attr.data.foreach_set('vector',[x for n in normals for x in n])
    obj.data.update()
    obj['pivot_description']=pivot['pivot_description']
    obj['source_world_origin_m']=list(origin)
    obj['source_to_item_rotation_rows']=json.dumps(pivot['rotation_rows'])
    obj['purpose']='Standalone game item. Identity transform, pivot baked into vertex coordinates.'
    if name=='Knife': obj['source_limitation']='Original blade tip is truncated and open.'
    expected_points=[[v.co.x*100,-v.co.y*100,v.co.z*100] for v in obj.data.vertices]
    expected={'min':[min(p[a] for p in expected_points) for a in range(3)],'max':[max(p[a] for p in expected_points) for a in range(3)]}
    filename=OUT/(obj.name+'.fbx')
    preview_materials=list(obj.data.materials)
    originals=[bpy.data.materials['MI_SurvivalMan_Backpack_Inst'],bpy.data.materials['MI_SurvivalMan_Jacket_Inst']]
    for i,mat in enumerate(originals): obj.data.materials[i]=mat
    for selected in bpy.context.selected_objects: selected.select_set(False)
    obj.hide_set(False);obj.select_set(True);bpy.context.view_layer.objects.active=obj
    if not filename.exists():
        result=bpy.ops.export_scene.fbx(filepath=str(filename),check_existing=False,use_selection=True,
            object_types={'MESH'},global_scale=1.0,apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',
            axis_forward='Y',axis_up='Z',use_space_transform=True,bake_space_transform=False,
            use_mesh_modifiers=False,mesh_smooth_type='FACE',use_mesh_edges=False,use_tspace=True,
            use_triangles=False,bake_anim=False,path_mode='STRIP',embed_textures=False,use_custom_props=False)
        assert 'FINISHED' in result
    for i,mat in enumerate(preview_materials): obj.data.materials[i]=mat
    tree,version=parse_fbx.parse(str(filename))
    objects=elem(tree,b'Objects')
    geometries=[e for e in objects.elems if e.id==b'Geometry']
    assert len(geometries)==1
    geometry=geometries[0]
    vertices=list(elem(geometry,b'Vertices').props[0])
    polys=list(elem(geometry,b'PolygonVertexIndex').props[0])
    assert vertices==[float(x) for v in obj.data.vertices for x in v.co]
    assert sum(v<0 for v in polys)==len(obj.data.polygons)
    assert not any(e.id in {b'Deformer',b'AnimationStack'} for e in objects.elems)
    assert all(tuple(a.uv)==tuple(b.uv) for a,b in zip(obj.data.uv_layers.active.data,src.data.uv_layers.active.data))
    report['parts'][name]={'file':str(filename),'object':obj.name,'vertices':len(obj.data.vertices),
       'triangles':len(obj.data.polygons),'expected_unreal_bounds_cm':expected,
       'pivot_description':pivot['pivot_description'],'origin_world_m':list(origin),'rotation_rows':pivot['rotation_rows'],
       'transform_baked_into_geometry':True,'fbx_geometry_verified':True,
       'fbx_sha256':hashlib.sha256(filename.read_bytes()).hexdigest()}
    created.append(obj)

for obj in created:
    obj.select_set(False)
    obj.hide_set(obj.name!='SM_Backpack2_Axe')
axe=bpy.data.objects['SM_Backpack2_Axe'];axe.select_set(True);bpy.context.view_layer.objects.active=axe
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            space=area.spaces.active
            space.shading.type='MATERIAL'
            space.overlay.show_overlays=True
            space.overlay.show_floor=True
            space.region_3d.view_location=Vector((0,0,.10))
            space.region_3d.view_distance=.8
            space.region_3d.view_rotation=Vector((.1,-1,.15)).to_track_quat('Z','Y')
            if not bpy.app.background: space.region_3d.update()
text=bpy.data.texts.new('BP2_Item_Pivots.json');text.write(json.dumps(manifest,indent=2))
notes=bpy.data.texts.new('BP2_GameItems_README.txt')
notes.write('Standalone meshes are in scene BP2_GameItems. Only the axe is visible initially; enable another item in the Outliner to edit it. All seven have identity transforms and baked local origins. BP2_Assembled retains the skinned worn assembly. FBX exports/Items is the standalone set; exports/Assembly is the worn set. Knife blade tip remains unfinished source geometry.')
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest()==source_hash
(ROOT/'exports/items_export_manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(DEST),check_existing=False)
print('ITEM_EXPORT_COMPLETE '+json.dumps({name:part['triangles'] for name,part in report['parts'].items()}))
