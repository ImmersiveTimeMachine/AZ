# @Description: Dump right hand + finger bones of Rifle01 clips in Winchester weapon space (read only)
import json
import unreal
APE = unreal.AnimPoseExtensions
WS, LS = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL
HERO = "/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh"
OUT = "C:/UnrealEngine/Games/AZ/Saved/wgs/win_hand_live.json"
CLIPS = [
    "/Game/AZ/Assets/Master/RifleMega/Rifle_AimOffsets/Rifle01/AZ_MST_Rifle01_St_Aim_CC",
    "/Game/AZ/Assets/Master/RifleMega/Rifle_AimOffsets/Rifle01/AZ_MST_Rifle01_St_Aim_45U",
    "/Game/AZ/Assets/Master/RifleMega/Rifle_AimOffsets/Rifle01/AZ_MST_Rifle01_St_Aim_45D",
    "/Game/AZ/Assets/Master/RifleMega/Rifle_Styly01_St/Rifle01_IdleSet/AZ_MST_Rifle01_St_Idle00",
    "/Game/AZ/Assets/Master/RifleMega/Rifle_Styly01_St/Rifle01_IdleSet/AZ_MST_Rifle01_St_Idle02",
]
FINGERS = ("thumb", "index", "middle", "ring", "pinky")
BONES = ["hand_r"]
PARENT = {}
for f in FINGERS:
    prev = "hand_r"
    seq = ([] if f == "thumb" else ["%s_metacarpal_r" % f]) + ["%s_0%d_r" % (f, i) for i in (1, 2, 3)]
    for b in seq:
        BONES.append(b); PARENT[b] = prev; prev = b

def tr(t):
    q = t.rotation
    return [t.translation.x, t.translation.y, t.translation.z, q.x, q.y, q.z, q.w]

hero = unreal.load_asset(HERO)
s = hero.find_socket("RightHandWinchesterSocket")
sock = unreal.Transform(s.get_editor_property("relative_location"), s.get_editor_property("relative_rotation"), s.get_editor_property("relative_scale"))
skel = hero.get_editor_property("skeleton")
refpose = APE.get_reference_pose(skel)
out = {"bones": BONES, "parents": PARENT, "ref_local": {b: tr(APE.get_bone_pose(refpose, b, LS)) for b in BONES},
       "socket": tr(sock), "clips": {}}
opts = unreal.AnimPoseEvaluationOptions()
for path in CLIPS:
    seq = unreal.load_asset(path)
    if not seq:
        print("missing", path); continue
    L = seq.get_play_length()
    frames = []
    for k in range(6):
        t = L * k / 6.0
        pose = APE.get_anim_pose_at_time(seq, t, opts)
        wpn = unreal.MathLibrary.compose_transforms(sock, APE.get_bone_pose(pose, "az_weapon_r", WS))
        rec = {"t": t, "weapon_ws": tr(wpn)}
        rec["hand_in_weapon"] = tr(unreal.MathLibrary.make_relative_transform(APE.get_bone_pose(pose, "hand_r", WS), wpn))
        rec["azw_in_hand"] = tr(unreal.MathLibrary.make_relative_transform(APE.get_bone_pose(pose, "az_weapon_r", WS), APE.get_bone_pose(pose, "hand_r", WS)))
        rec["local"] = {b: tr(APE.get_bone_pose(pose, b, LS)) for b in BONES[1:]}
        frames.append(rec)
    out["clips"][path.split("/")[-1]] = {"len": L, "frames": frames}
json.dump(out, open(OUT, "w"), indent=1)
print("wrote", OUT, len(out["clips"]))
import gc; gc.collect()
