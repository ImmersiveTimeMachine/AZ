# @Description: Export both hands of the grip pose in Winchester weapon space, as the grip node places them
# Run in the editor: exec(open(r"C:/UnrealEngine/Games/AZ/Tools/wgs/export_hands.py").read())
# Output: Saved/wgs/hands_export.json
#   weapon space = the SK_Winchester root, placed like the game does: RightHandWinchesterSocket (hero body mesh) on
#   az_weapon_r of the grip pose. The right hand stays where the pose has it (it carries the gun); the left hand is
#   moved rigidly onto LeftHandGrip (the node's two-bone IK target) together with its fingers.
#   For every hand / finger bone: component (weapon-space) transform in the GRIP pose, in the REFERENCE pose (same
#   hand transform, reference finger locals) and in the pack idle (mocap, same hand transform), plus parent names and
#   the local transforms, so the solver can do its own FK.
import json, os
import unreal

APE = unreal.AnimPoseExtensions
WS, LS = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL
HERO = "/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh"
WEAPON = "/Game/AZ/Assets/Weapons/Winchester_Rifle/Skeletal/SK_Winchester"
POSE = "/Game/AZ/Blueprints/Animation/WeaponGrip/AS_Grip_Winchester"
MOCAP = "/Game/AZ/Assets/Master/RifleMega/Rifle_Styly02_St/Rifle02_IdleSet/AZ_MST_Rifle02_St_Idle00"
OUT = "C:/UnrealEngine/Games/AZ/Saved/wgs/hands_export.json"
FINGERS = ("thumb", "index", "middle", "ring", "pinky")


def tr(t):
    q = t.rotation
    return [t.translation.x, t.translation.y, t.translation.z, q.x, q.y, q.z, q.w]


def sock(mesh, name):
    s = mesh.find_socket(name)
    return unreal.Transform(s.get_editor_property("relative_location"), s.get_editor_property("relative_rotation"),
                            s.get_editor_property("relative_scale"))


def chain(side):
    bones = ["hand_" + side]
    parents = {}
    for f in FINGERS:
        prev = "hand_" + side
        seq = ([] if f == "thumb" else ["%s_metacarpal_%s" % (f, side)]) + ["%s_0%d_%s" % (f, i, side) for i in (1, 2, 3)]
        for b in seq:
            bones.append(b)
            parents[b] = prev
            prev = b
    return bones, parents


hero, sk = unreal.load_asset(HERO), unreal.load_asset(WEAPON)
opts = unreal.AnimPoseEvaluationOptions()
grip = APE.get_anim_pose_at_time(unreal.load_asset(POSE), 0.0, opts)
mocap = APE.get_anim_pose_at_time(unreal.load_asset(MOCAP), 0.0, opts)
ref = APE.get_anim_pose_at_time(unreal.load_asset(POSE), 0.0, opts)
APE.set_bone_pose  # (exists; the reference is taken from the ref-pose query below)
weapon_w = unreal.MathLibrary.compose_transforms(sock(hero, "RightHandWinchesterSocket"), APE.get_bone_pose(grip, "az_weapon_r", WS))
lhg = sock(sk, "LeftHandGrip")
out = {"weapon": WEAPON, "pose": POSE, "mocap": MOCAP, "left_hand_grip": tr(lhg), "hands": {}}
for side in ("l", "r"):
    bones, parents = chain(side)
    missing = [b for b in bones if not APE.get_bone_names(grip) or b not in [str(n) for n in APE.get_bone_names(grip)]]
    rec = {"bones": bones, "parents": parents, "missing": missing}
    for label, pose in (("grip", grip), ("mocap", mocap)):
        hand_w = APE.get_bone_pose(pose, "hand_" + side, WS)
        hand_ws = unreal.MathLibrary.make_relative_transform(hand_w, weapon_w)   # hand in weapon space
        if side == "l":
            hand_target = lhg                                                   # the node puts hand_l here
        else:
            hand_target = unreal.MathLibrary.make_relative_transform(APE.get_bone_pose(grip, "hand_r", WS), weapon_w)
        rec[label] = {
            "hand_ws": tr(hand_target),
            "local": {b: tr(APE.get_bone_pose(pose, b, LS)) for b in bones},
            "ref_local": {b: tr(APE.get_ref_bone_pose(pose, b, LS)) for b in bones},
        }
    out["hands"][side] = rec
os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(out, open(OUT, "w"), indent=1)
print("hands export:", {s: (len(out["hands"][s]["bones"]), out["hands"][s]["missing"]) for s in out["hands"]})
import gc; gc.collect()
