import bpy,json,hashlib,sys
from pathlib import Path
from mathutils import Vector

out=Path(r'C:/UnrealEngine/Games/AZ/Art/CHALK_Pistol_Holster_v02/references')
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.unit_settings.system='METRIC'
bpy.context.scene.unit_settings.scale_length=1.0
bpy.ops.import_scene.fbx(filepath=str(out/'Pistol.fbx'),use_custom_normals=True)
objects=[]
for obj in bpy.context.scene.objects:
    if obj.type!='MESH':continue
    mesh=obj.data
    points=[obj.matrix_world@v.co for v in mesh.vertices]
    mesh.calc_loop_triangles()
    rec={'name':obj.name,'mesh_name':mesh.name,'matrix_world':[list(r) for r in obj.matrix_world],'positions_world_m':[list(p) for p in points],'positions_local':[list(v.co) for v in mesh.vertices], 'edges':[list(e.vertices) for e in mesh.edges], 'polygons':[list(f.vertices) for f in mesh.polygons], 'polygon_material_indices':[f.material_index for f in mesh.polygons], 'triangles':[list(t.vertices) for t in mesh.loop_triangles], 'triangle_polygon_indices':[t.polygon_index for t in mesh.loop_triangles], 'materials':[m.name if m else None for m in mesh.materials], 'uv_layers':{uv.name:[[list(uv.data[i].uv) for i in poly.loop_indices] for poly in mesh.polygons] for uv in mesh.uv_layers}, 'polygon_corner_normals':[[list(mesh.corner_normals[i].vector) for i in poly.loop_indices] for poly in mesh.polygons], 'bounds_world_m':[[min(p[a] for p in points) for a in range(3)],[max(p[a] for p in points) for a in range(3)]]}
    objects.append(rec)
    for mat in mesh.materials:
        if not mat:continue
        mat.use_nodes=True
        mat.node_tree.nodes.clear()
        bs=mat.node_tree.nodes.new('ShaderNodeBsdfPrincipled')
        output=mat.node_tree.nodes.new('ShaderNodeOutputMaterial')
        mat.node_tree.links.new(bs.outputs['BSDF'],output.inputs['Surface'])
        if bs:
            tex=mat.node_tree.nodes.new('ShaderNodeTexImage')
            tex.image=bpy.data.images.load(str(out/'Pistol_BaseColor.png'),check_existing=True)
            tex.image.pack()
            mat.node_tree.links.new(tex.outputs['Color'],bs.inputs['Base Color'])
            bs.inputs['Metallic'].default_value=.3
            bs.inputs['Roughness'].default_value=.55
data={'units':'metres','coordinate_space':'Blender world after native FBX import, before any transforms','blender_version':bpy.app.version_string,'fbx_sha256':hashlib.sha256((out/'Pistol.fbx').read_bytes()).hexdigest(),'objects':objects}
(out/'pistol_geometry.json').write_text(json.dumps(data))
bpy.ops.wm.save_as_mainfile(filepath=str(out/'Pistol_Reference.blend'))
print('PISTOL_GEOMETRY_SUMMARY '+json.dumps([{'name':o['name'],'vertices':len(o['positions_world_m']),'faces':len(o['polygons']),'triangles':len(o['triangles']),'bounds':o['bounds_world_m'],'materials':o['materials']} for o in objects]))
