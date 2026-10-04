# @Description: M16 hold analysis: weapon-in-hand via the middle_01_r socket, az_weapon_r local, left hand vs LeftHandGrip (read only)
import json
import unreal
APE = unreal.AnimPoseExtensions
W, LS = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL
HERO = "/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh"
MESH = "/Game/AZ/Assets/M16/SKL/M16_Skeleton"
OUT = "C:/UnrealEngine/Games/AZ/Saved/wgs/m16_hold.json"
def tr(t):
    q = t.rotation
    return [t.translation.x, t.translation.y, t.translation.z, q.x, q.y, q.z, q.w]
hero = unreal.load_asset(HERO); m16 = unreal.load_asset(MESH)
s = hero.find_socket("RightHandRifleSocketAim")
sock = unreal.Transform(s.get_editor_property("relative_location"), s.get_editor_property("relative_rotation"), unreal.Vector(1, 1, 1))
lg = {}
for n in ("LeftHandGrip", "LeftHandGripAim"):
    x = m16.find_socket(n)
    lg[n] = (str(x.get_editor_property("bone_name")), tr(unreal.Transform(x.get_editor_property("relative_location"), x.get_editor_property("relative_rotation"), unreal.Vector(1, 1, 1))))
# M16 bone of the sockets in the mesh ref pose (component space)
comp = unreal.new_object(unreal.SkeletalMeshComponent); comp.set_skinned_asset_and_update(m16, True)
def cs(bone):
    t = unreal.Transform(); b = unreal.Name(bone); ch = []
    while str(b) != "None":
        ch.append(b); b = comp.get_parent_bone(b)
    for b in reversed(ch):
        t = unreal.MathLibrary.compose_transforms(comp.get_ref_pose_transform(comp.get_bone_index(b)), t)
    return t
lg_cs = {n: tr(unreal.MathLibrary.compose_transforms(unreal.Transform(unreal.Vector(*v[1][:3]), unreal.Quat(*v[1][3:]).rotator(), unreal.Vector(1, 1, 1)), cs(v[0]))) for n, v in lg.items()}
ar = unreal.AssetRegistryHelpers.get_asset_registry()
seqs = sorted(str(a.package_name) for a in ar.get_assets_by_path("/Game/AZ/Assets/M16/Riffle_RTG_MH", recursive=True) if str(a.asset_class_path.asset_name) == "AnimSequence")
txt = open("C:/UnrealEngine/Games/AZ/Saved/wgs/cht_v2_dump.txt", encoding="utf-8").read()
used = [p for p in seqs if (":" + p.rsplit("/", 1)[1] + "\\\"") in txt or (":" + p.rsplit("/", 1)[1] + "\"") in txt]
opts = unreal.AnimPoseEvaluationOptions()
out = {"sock_middle01": tr(sock), "left_sockets_mesh_cs": lg_cs, "clips": {}}
for p in used:
    seq = unreal.load_asset(p); L = seq.get_play_length(); fr = []
    for k in range(6):
        t = L * k / 6.0
        pose = APE.get_anim_pose_at_time(seq, t, opts)
        hand = APE.get_bone_pose(pose, "hand_r", W)
        wpn = unreal.MathLibrary.compose_transforms(sock, APE.get_bone_pose(pose, "middle_01_r", W))
        fr.append({"t": t, "weapon_in_hand": tr(unreal.MathLibrary.make_relative_transform(wpn, hand)),
                   "azw_local": tr(APE.get_bone_pose(pose, "az_weapon_r", LS)),
                   "hand_l_in_weapon": tr(unreal.MathLibrary.make_relative_transform(APE.get_bone_pose(pose, "hand_l", W), wpn))})
    out["clips"][p.rsplit("/", 1)[1]] = {"len": L, "frames": fr}
json.dump(out, open(OUT, "w"))
print("used clips", len(used), "->", OUT, "left sockets", {k: v[:3] for k, v in lg_cs.items()})
import gc; gc.collect()
