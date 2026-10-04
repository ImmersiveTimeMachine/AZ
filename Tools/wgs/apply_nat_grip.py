# @Description: Write the solved natural right-hand grasp into the weapon's grip pose / weapon BP (Unreal Python)
# exec(open(r"C:/UnrealEngine/Games/AZ/Tools/wgs/apply_nat_grip.py").read(), {"MODE": "pose"})
# Input: Saved/wgs/nat_grip.json (Tools/wgs/natgrip/final.py):
#   "locals"     : right-hand finger bone local rotations of the solved grasp (metacarpals, phalanges, thumb)
#   "correction" : hand_r re-grip in its own space [tx, ty, tz, qx, qy, qz, qw]
# MODE "pose"   : the grip pose GRIP_POSE gets those rotations on the right-hand finger bones (translations stay the
#                 pose's own; the left hand is untouched). File backup is taken outside (AZ_Backups).
# MODE "weapon" : (needs the C++ with AAZ_Weapon::RightHandGripCorrection / bBakedRightHandGrasp) the weapon BP's
#                 defaults get the correction and bBakedRightHandGrasp = True; the BP is compiled and saved (a regular
#                 Blueprint, not an AnimBP).
import json

import unreal

MODE = globals().get("MODE", "pose")
DATA = globals().get("DATA", "C:/UnrealEngine/Games/AZ/Saved/wgs/nat_grip.json")
GRIP_POSE = globals().get("GRIP_POSE", "/Game/AZ/Blueprints/Animation/WeaponGrip/AS_Grip_Winchester")
WEAPON_BP = globals().get("WEAPON_BP", "/Game/AZ/Blueprints/Weapon/AZ_BP_Winchester")
APE = unreal.AnimPoseExtensions
AL = unreal.AnimationLibrary
EAL = unreal.EditorAssetLibrary
data = json.load(open(DATA))

if MODE == "pose":
    seq = unreal.load_asset(GRIP_POSE)
    pose = APE.get_anim_pose_at_time(seq, 0.0, unreal.AnimPoseEvaluationOptions())
    nkeys = max(1, AL.get_num_keys(seq))
    c = seq.controller
    c.open_bracket("Natural right-hand grasp", False)
    written = []
    try:
        for b, q in sorted(data["locals"].items()):
            loc = APE.get_bone_pose(pose, b, unreal.AnimPoseSpaces.LOCAL)
            ok = c.set_bone_track_keys(unreal.Name(b), [loc.translation] * nkeys, [unreal.Quat(q[0], q[1], q[2], q[3])] * nkeys,
                                       [unreal.Vector(1, 1, 1)] * nkeys, False)
            written.append((b, bool(ok)))
    finally:
        c.close_bracket(False)
    # verify: read the pose back
    pose = APE.get_anim_pose_at_time(seq, 0.0, unreal.AnimPoseEvaluationOptions())
    worst = 0.0
    for b, q in data["locals"].items():
        r = APE.get_bone_pose(pose, b, unreal.AnimPoseSpaces.LOCAL).rotation
        d = abs(r.x * q[0] + r.y * q[1] + r.z * q[2] + r.w * q[3])
        worst = max(worst, 1.0 - min(1.0, d))
    print("[apply_nat_grip] pose keys", nkeys, "bones", len(written), "failed", [b for b, ok in written if not ok],
          "max quat mismatch %.2e" % worst, "saved", EAL.save_asset(GRIP_POSE, only_if_is_dirty=False))
elif MODE == "weapon":
    bp = unreal.load_asset(WEAPON_BP)
    cdo = unreal.get_default_object(bp.generated_class())
    v = data["correction"]
    xf = unreal.Transform(unreal.Vector(v[0], v[1], v[2]), unreal.Quat(v[3], v[4], v[5], v[6]).rotator(), unreal.Vector(1, 1, 1))
    cdo.set_editor_property("right_hand_grip_correction", xf)
    cdo.set_editor_property("baked_right_hand_grasp", True)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    print("[apply_nat_grip] weapon", cdo.get_editor_property("right_hand_grip_correction"),
          cdo.get_editor_property("baked_right_hand_grasp"), "saved", EAL.save_asset(WEAPON_BP, only_if_is_dirty=False))
import gc
gc.collect()
