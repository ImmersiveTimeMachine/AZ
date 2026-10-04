# @Description: Derive the Winchester finger markers from the current grip pose (WGS task 1.3, marker part)
# Run in the editor: exec(open(r"C:/UnrealEngine/Games/AZ/Tools/wgs/derive_markers_from_grip_pose.py").read(), {"WRITE": True})
#
# The AZ Weapon Grip node pulls every fingertip pad toward its Grip_<L|R>_<Finger> socket. When the markers do not
# come from the grip pose the node is fed (they were solved for older hand sockets), the IK straightens the fingers
# toward points up to 8 cm away. Here the markers are rebuilt FROM the pose, with the hands where the node puts them:
#   right hand: carries the gun -> weapon = RightHandWinchesterSocket (hero body mesh) on az_weapon_r of the pose;
#   left hand : IK places hand_l exactly at LeftHandGrip -> pad = LeftHandGrip * (pad in hand_l space).
# Pad = end of _03 + 0.85 x (_03 - _02)  (the node's FingertipExtension). Old values -> Saved/wgs/markers_backup_*.json.
import unreal, json, time, os

APE = unreal.AnimPoseExtensions
WS = unreal.AnimPoseSpaces.WORLD
EXT = 0.85
HERO = "/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh"
WEAPON = "/Game/AZ/Assets/Weapons/Winchester_Rifle/Skeletal/SK_Winchester"
POSE = "/Game/AZ/Blueprints/Animation/WeaponGrip/AS_Grip_Winchester"
FINGERS = (("thumb", "Thumb"), ("index", "Index"), ("middle", "Middle"), ("ring", "Ring"), ("pinky", "Pinky"))
write = globals().get("WRITE", False)

def sock(mesh, name):
    s = mesh.find_socket(name)
    return s, unreal.Transform(s.get_editor_property("relative_location"), s.get_editor_property("relative_rotation"),
                               s.get_editor_property("relative_scale"))

hero, sk, gp = unreal.load_asset(HERO), unreal.load_asset(WEAPON), unreal.load_asset(POSE)
pose = APE.get_anim_pose_at_time(gp, 0.0, unreal.AnimPoseEvaluationOptions())
_, hand_sock = sock(hero, "RightHandWinchesterSocket")
weapon_w = unreal.MathLibrary.compose_transforms(hand_sock, APE.get_bone_pose(pose, "az_weapon_r", WS))
_, lhg = sock(sk, "LeftHandGrip")
hand_l_w = APE.get_bone_pose(pose, "hand_l", WS)

def pad_world(finger, side):
    a = APE.get_bone_pose(pose, "%s_02_%s" % (finger, side), WS).translation
    b = APE.get_bone_pose(pose, "%s_03_%s" % (finger, side), WS).translation
    return b + (b - a) * EXT

report = {"time": time.strftime("%Y-%m-%d %H:%M:%S"), "markers": {}}
for side, S in (("r", "R"), ("l", "L")):
    for f, F in FINGERS:
        p = pad_world(f, side)
        if side == "r":
            new = unreal.MathLibrary.inverse_transform_location(weapon_w, p)
        else:
            new = unreal.MathLibrary.transform_location(lhg, unreal.MathLibrary.inverse_transform_location(hand_l_w, p))
        s, old = sock(sk, "Grip_%s_%s" % (S, F))
        report["markers"]["Grip_%s_%s" % (S, F)] = {"old": [old.translation.x, old.translation.y, old.translation.z],
                                                   "new": [new.x, new.y, new.z],
                                                   "moved_cm": round((new - old.translation).length(), 2)}
        if write:
            s.modify()
            s.set_editor_property("relative_location", new)

out_dir = "C:/UnrealEngine/Games/AZ/Saved/wgs/"
os.makedirs(out_dir, exist_ok=True)
json.dump(report, open(out_dir + "markers_backup_%s.json" % time.strftime("%Y%m%d_%H%M%S"), "w"), indent=1)
if write:
    sk.modify()
    # save_asset(only_if_is_dirty=False): socket edits do not always dirty the package (fails during PIE)
    ok = unreal.EditorAssetLibrary.save_asset(WEAPON, only_if_is_dirty=False)
    sk2 = unreal.load_asset(WEAPON)
    worst = max(( sock(sk2, n)[1].translation - unreal.Vector(*v["new"])).length() for n, v in report["markers"].items())
    print("saved", ok, "readback worst cm %.4f" % worst)
print({k: v["moved_cm"] for k, v in report["markers"].items()})
import gc; gc.collect()
