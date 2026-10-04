# @Description: Export hero hand skin vertices (SIDE r|l) (mesh bind pose) in hand_r space + mesh ref locals (read only)
import json
import unreal
HERO = "/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh"
SIDE = globals().get("SIDE", "r")
OUT = "C:/UnrealEngine/Games/AZ/Saved/wgs/hand_%s_verts.json" % SIDE
hero = unreal.load_asset(HERO)
comp = unreal.new_object(unreal.SkeletalMeshComponent)
comp.set_skinned_asset_and_update(hero, True)
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
FINGERS = ("thumb", "index", "middle", "ring", "pinky")
bones = []
for f in FINGERS:
    bones += ([] if f == "thumb" else ["%s_metacarpal_%s" % (f, SIDE)]) + ["%s_0%d_%s" % (f, i, SIDE) for i in (1, 2, 3)]
ref_local = {b: tr(comp.get_ref_pose_transform(comp.get_bone_index(unreal.Name(b)))) for b in bones}
hand = cs("hand_" + SIDE); inv = hand.inverse()
# bone positions in hand space (mesh bind pose) for nearest-bone skinning
bone_cs = {b: tr(unreal.MathLibrary.make_relative_transform(cs(b), hand)) for b in bones}
res = unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(hero, unreal.DynamicMesh(), unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
vl = list(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(res[0], False)[1].convert_vector_list_to_array())
out = []
for v in vl:
    p = unreal.MathLibrary.transform_location(inv, v)
    px = p.x if SIDE == "r" else -p.x          # left-side bones point along +X (mirrored)
    if -22.0 < px < 5.0 and abs(p.y) < 11 and abs(p.z) < 12:
        out.append([round(p.x, 3), round(p.y, 3), round(p.z, 3)])
json.dump({"ref_local_mesh": ref_local, "bone_hand_space": bone_cs, "verts_hand_space": out}, open(OUT, "w"))
print("kept", len(out))
import gc; gc.collect()
