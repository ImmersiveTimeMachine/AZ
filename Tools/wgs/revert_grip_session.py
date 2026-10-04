# @Description: Revert the Winchester grip to the state before the 2026-09-27 evening solver attempts
# exec(open(r"C:/UnrealEngine/Games/AZ/Tools/wgs/revert_grip_session.py").read())
# - RightHandWinchesterSocket <- Saved/wgs/right_socket_backup_20260927_190703.json (the user's tuned value)
# - AS_Grip_Winchester finger + metacarpal tracks <- AS_Grip_Winchester_preV3 (backup taken before the first write)
# - Grip_* markers <- the "old" values of Saved/wgs/markers_backup_20260927_184957.json (the first derive)
import json
import unreal

APE = unreal.AnimPoseExtensions
EAL = unreal.EditorAssetLibrary
S = "C:/UnrealEngine/Games/AZ/Saved/wgs/"
NAME = "RightHandWinchesterSocket"

# 1. socket
sb = json.load(open(S + "right_socket_backup_20260927_190703.json"))["values"]
for path, v in sb.items():
    m = unreal.load_asset(path)
    s = m.find_socket(NAME)
    m.modify(); s.modify()
    s.set_editor_property("relative_location", unreal.Vector(*v["loc"]))
    p, y, r = v["rot_pitch_yaw_roll"]
    s.set_editor_property("relative_rotation", unreal.Rotator(roll=r, pitch=p, yaw=y))
    print("socket restored", path.rsplit("/", 1)[1], EAL.save_asset(path, only_if_is_dirty=False))
jp = "C:/UnrealEngine/Games/AZ/Tools/hero_sockets.json"
data = json.load(open(jp, encoding="utf-8"))
hv = sb["/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh"]
data[NAME]["loc"], data[NAME]["rot_pitch_yaw_roll"] = hv["loc"], hv["rot_pitch_yaw_roll"]
json.dump(data, open(jp, "w", encoding="utf-8"), indent=1, sort_keys=True)

# 2. grip pose
POSE = "/Game/AZ/Blueprints/Animation/WeaponGrip/AS_Grip_Winchester"
src = APE.get_anim_pose_at_time(unreal.load_asset(POSE + "_preV3"), 0.0, unreal.AnimPoseEvaluationOptions())
seq = unreal.load_asset(POSE)
n = unreal.AnimationLibrary.get_num_keys(seq)
c = seq.controller
c.open_bracket("WGS revert grip pose", False)
count = 0
for side in ("l", "r"):
    for f in ("index", "middle", "ring", "pinky", "thumb"):
        names = ([] if f == "thumb" else ["%s_metacarpal_%s" % (f, side)]) + ["%s_0%d_%s" % (f, i, side) for i in (1, 2, 3)]
        for b in names:
            t = APE.get_bone_pose(src, b, unreal.AnimPoseSpaces.LOCAL)
            c.set_bone_track_keys(unreal.Name(b), [t.translation] * n, [t.rotation] * n, [unreal.Vector(1, 1, 1)] * n, False)
            count += 1
c.close_bracket(False)
print("grip pose tracks restored", count, EAL.save_asset(POSE, only_if_is_dirty=False))

# 3. markers
mb = json.load(open(S + "markers_backup_20260927_184957.json"))["markers"]
WEAPON = "/Game/AZ/Assets/Weapons/Winchester_Rifle/Skeletal/SK_Winchester"
sk = unreal.load_asset(WEAPON)
sk.modify()
for name, v in mb.items():
    s = sk.find_socket(name)
    s.modify()
    s.set_editor_property("relative_location", unreal.Vector(*v["old"]))
print("markers restored", len(mb), EAL.save_asset(WEAPON, only_if_is_dirty=False))
import gc; gc.collect()
