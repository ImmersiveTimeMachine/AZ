# @Description: Apply the fitted right-hand placement as RightHandWinchesterSocket (backup first)
# exec(open(r"C:/UnrealEngine/Games/AZ/Tools/wgs/apply_placement.py").read(), {"FIT": "placement_fit_norot.json", "WRITE": True})
# The fit gives hand_r in weapon space (hand_ws). The game keeps the hand where the clip has it and hangs the gun on
# az_weapon_r: weapon' = inverse(hand_ws) * hand_world, socket' = weapon' relative to az_weapon_r (grip pose, where
# az_weapon_r == hand_r at every hold). Written to SKM_AZ_Master (preview), the hero body mesh (game) and
# Tools/hero_sockets.json; the previous value goes to Saved/wgs/right_socket_backup_<time>.json.
import json, os, time
import unreal

APE = unreal.AnimPoseExtensions
WS = unreal.AnimPoseSpaces.WORLD
EAL = unreal.EditorAssetLibrary
FIT = "C:/UnrealEngine/Games/AZ/Saved/wgs/" + globals().get("FIT", "placement_fit_norot.json")
MESHES = ("/Game/AZ/Assets/Characters/Master/SKM_AZ_Master", "/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh")
JSON = "C:/UnrealEngine/Games/AZ/Tools/hero_sockets.json"
NAME = "RightHandWinchesterSocket"
write = globals().get("WRITE", False)

fit = json.load(open(FIT))
h = fit["best"]["hand_ws"]
hand_ws = unreal.Transform(unreal.Vector(h[0], h[1], h[2]), unreal.Quat(h[3], h[4], h[5], h[6]).rotator(), unreal.Vector(1, 1, 1))
pose = APE.get_anim_pose_at_time(unreal.load_asset("/Game/AZ/Blueprints/Animation/WeaponGrip/AS_Grip_Winchester"), 0.0,
                                 unreal.AnimPoseEvaluationOptions())
hand_w = APE.get_bone_pose(pose, "hand_r", WS)
az_w = APE.get_bone_pose(pose, "az_weapon_r", WS)
weapon_w = unreal.MathLibrary.compose_transforms(unreal.MathLibrary.invert_transform(hand_ws), hand_w)
sock_new = unreal.MathLibrary.make_relative_transform(weapon_w, az_w)
loc, rot = sock_new.translation, sock_new.rotation.rotator()

backup = {}
for p in MESHES:
    s = unreal.load_asset(p).find_socket(NAME)
    l, r = s.get_editor_property("relative_location"), s.get_editor_property("relative_rotation")
    backup[p] = {"loc": [l.x, l.y, l.z], "rot_pitch_yaw_roll": [r.pitch, r.yaw, r.roll]}
old = backup[MESHES[1]]
print("old", old)
print("new loc", [round(loc.x, 3), round(loc.y, 3), round(loc.z, 3)], "rot", [round(rot.pitch, 3), round(rot.yaw, 3), round(rot.roll, 3)],
      "moved cm %.2f" % (loc - unreal.Vector(*old["loc"])).length())
if write:
    os.makedirs("C:/UnrealEngine/Games/AZ/Saved/wgs", exist_ok=True)
    json.dump({"socket": NAME, "values": backup, "fit": FIT}, open("C:/UnrealEngine/Games/AZ/Saved/wgs/right_socket_backup_%s.json" % time.strftime("%Y%m%d_%H%M%S"), "w"), indent=1)
    for p in MESHES:
        m = unreal.load_asset(p)
        s = m.find_socket(NAME)
        m.modify(); s.modify()
        s.set_editor_property("relative_location", loc)
        s.set_editor_property("relative_rotation", rot)
        print("saved", p.rsplit("/", 1)[1], EAL.save_asset(p, only_if_is_dirty=False))
    data = json.load(open(JSON, encoding="utf-8"))
    data[NAME]["loc"] = [loc.x, loc.y, loc.z]
    data[NAME]["rot_pitch_yaw_roll"] = [rot.pitch, rot.yaw, rot.roll]
    json.dump(data, open(JSON, "w", encoding="utf-8"), indent=1, sort_keys=True)
    print("hero_sockets.json updated")
import gc; gc.collect()
