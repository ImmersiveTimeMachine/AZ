# @Description: Write selected fingers of the grasp solution into AS_Grip_Winchester (backup first)
# exec(open(r"C:/UnrealEngine/Games/AZ/Tools/wgs/apply_grasp_solution.py").read(), {"FINGERS": ["index_l", ...]})
# FINGERS: "<finger>_<side>" entries; each writes the rotations of <finger>_metacarpal/_01/_02/_03_<side> from
# Saved/wgs/grasp_solution.json into the 1-frame grip pose (translations stay the pose's own). Backup: the pose is
# duplicated once to AS_Grip_Winchester_preV3 before the first write.
import json
import unreal

POSE = "/Game/AZ/Blueprints/Animation/WeaponGrip/AS_Grip_Winchester"
BACKUP = POSE + "_preV3"
SOL = "C:/UnrealEngine/Games/AZ/Saved/wgs/grasp_solution.json"
APE = unreal.AnimPoseExtensions
AL = unreal.AnimationLibrary
sel = globals().get("FINGERS", [])

EAL = unreal.EditorAssetLibrary
if not EAL.does_asset_exist(BACKUP):
    EAL.duplicate_asset(POSE, BACKUP)
    print("backup", BACKUP, EAL.save_asset(BACKUP, only_if_is_dirty=False))
seq = unreal.load_asset(POSE)
sol = json.load(open(SOL))["bones"]
pose = APE.get_anim_pose_at_time(seq, 0.0, unreal.AnimPoseEvaluationOptions())
nkeys = AL.get_num_keys(seq)
c = seq.controller
c.open_bracket("WGS v3 grasp fingers", False)
written = []
for entry in sel:
    finger, side = entry.rsplit("_", 1)
    names = ([] if finger == "thumb" else ["%s_metacarpal_%s" % (finger, side)]) + ["%s_0%d_%s" % (finger, i, side) for i in (1, 2, 3)]
    for b in names:
        loc = APE.get_bone_pose(pose, b, unreal.AnimPoseSpaces.LOCAL)
        v = sol[b]
        q = unreal.Quat(v[3], v[4], v[5], v[6])
        ok = c.set_bone_track_keys(unreal.Name(b), [loc.translation] * nkeys, [q] * nkeys, [unreal.Vector(1, 1, 1)] * nkeys, False)
        written.append((b, bool(ok)))
c.close_bracket(False)
print("keys", nkeys, "written", written)
print("save", EAL.save_asset(POSE, only_if_is_dirty=False))
import gc; gc.collect()
