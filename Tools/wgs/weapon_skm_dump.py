# @Description: Dump a weapon skeletal mesh (bind pose) - vertices, triangles, bones, sockets - to JSON for the grip tools
# exec(open(r"C:/UnrealEngine/Games/AZ/Tools/wgs/weapon_skm_dump.py").read(), {"MESH": "/Game/AZ/Assets/M16/SKL/M16_Skeleton", "OUT": ".../m16_mesh.json"})
import json
import unreal
MESH = globals()["MESH"]
OUT = globals()["OUT"]
mesh = unreal.load_asset(MESH)
res = unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(mesh, unreal.DynamicMesh(), unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
dm = res[0]
verts = [[v.x, v.y, v.z] for v in unreal.GeometryScript_MeshQueries.get_all_vertex_positions(dm, False)[1].convert_vector_list_to_array()]
tri = unreal.GeometryScript_MeshQueries.get_all_triangle_indices(dm, False)
tl = [x for x in (tri if isinstance(tri, tuple) else (tri,)) if hasattr(x, "convert_triangle_list_to_array")]
tris = []
for t in tl[0].convert_triangle_list_to_array():
    tris += [t.x, t.y, t.z]                                   # IntVector
comp = unreal.new_object(unreal.SkeletalMeshComponent); comp.set_skinned_asset_and_update(mesh, True)
def cs(bone):
    t = unreal.Transform(); b = unreal.Name(bone); ch = []
    while str(b) != "None":
        ch.append(b); b = comp.get_parent_bone(b)
    for b in reversed(ch):
        t = unreal.MathLibrary.compose_transforms(comp.get_ref_pose_transform(comp.get_bone_index(b)), t)
    return t
def tr(t):
    q = t.rotation
    return [t.translation.x, t.translation.y, t.translation.z, q.x, q.y, q.z, q.w]
bones = {}
for i in range(comp.get_num_bones()):
    n = str(comp.get_bone_name(i)); bones[n] = {"parent": str(comp.get_parent_bone(unreal.Name(n))), "cs": tr(cs(n))}
sockets = {}
for i in range(mesh.num_sockets()):
    s = mesh.get_socket_by_index(i); b = str(s.get_editor_property("bone_name"))
    rel = unreal.Transform(s.get_editor_property("relative_location"), s.get_editor_property("relative_rotation"), unreal.Vector(1, 1, 1))
    sockets[str(s.get_editor_property("socket_name"))] = {"bone": b, "cs": tr(unreal.MathLibrary.compose_transforms(rel, cs(b)))}
json.dump({"mesh": MESH, "verts": verts, "tris": tris, "bones": bones, "sockets": sockets}, open(OUT, "w"))
print("verts", len(verts), "tris", len(tris) // 3, "bones", list(bones)[:30], "sockets", list(sockets))
import gc; gc.collect()
