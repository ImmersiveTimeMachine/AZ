# @Description: M16 support hand in its clips: hand_l (+ arm, finger locals) in weapon space with the weapon on RightHandM16Socket (read only)
# exec(open(r"C:/UnrealEngine/Games/AZ/Tools/wgs/natgrip/dump_left_m16.py").read(), {"CHUNK": 0, "CHUNK_SIZE": 200})
# The M16 clips are MetaHuman-native (no az_weapon_r track): az_weapon_r sits on hand_r, so the weapon = S * hand_r with
# S = the RightHandM16Socket transform (Saved/wgs/m16_hold_pick.json "weapon_in_hand").
import json
import unreal
APE = unreal.AnimPoseExtensions
W, LS = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL
FOLDER = "/Game/AZ/Assets/M16/Riffle_RTG_MH"
CHUNK = int(globals().get("CHUNK", 0))
CHUNK_SIZE = int(globals().get("CHUNK_SIZE", 200))
NFR = int(globals().get("NFR", 8))
OUT = "C:/UnrealEngine/Games/AZ/Saved/wgs/m16_left_%d.json" % CHUNK
pick = json.load(open("C:/UnrealEngine/Games/AZ/Saved/wgs/m16_hold_pick.json"))
v = pick["weapon_in_hand"]
S = unreal.Transform(unreal.Vector(v[0], v[1], v[2]), unreal.Quat(v[3], v[4], v[5], v[6]).rotator(), unreal.Vector(1, 1, 1))
FING = ["%s_0%d_l" % (f, i) for f in ("thumb", "index", "middle", "ring", "pinky") for i in (1, 2, 3)] + ["%s_metacarpal_l" % f for f in ("index", "middle", "ring", "pinky")]
def tr(t):
    q = t.rotation
    return [t.translation.x, t.translation.y, t.translation.z, q.x, q.y, q.z, q.w]
ar = unreal.AssetRegistryHelpers.get_asset_registry()
paths = sorted(str(a.package_name) for a in ar.get_assets_by_path(FOLDER, recursive=True) if str(a.asset_class_path.asset_name) == "AnimSequence")
total = len(paths)
paths = paths[CHUNK * CHUNK_SIZE:(CHUNK + 1) * CHUNK_SIZE]
opts = unreal.AnimPoseEvaluationOptions()
out = {}
for p in paths:
    seq = unreal.load_asset(p); L = seq.get_play_length(); fr = []
    for k in range(NFR):
        t = L * k / NFR
        pose = APE.get_anim_pose_at_time(seq, t, opts)
        wpn = unreal.MathLibrary.compose_transforms(S, APE.get_bone_pose(pose, "hand_r", W))
        rel = lambda b: tr(unreal.MathLibrary.make_relative_transform(APE.get_bone_pose(pose, b, W), wpn))
        fr.append({"t": t, "hand_l": rel("hand_l"), "lowerarm_l": rel("lowerarm_l"), "upperarm_l": rel("upperarm_l"),
                   "local": {b: tr(APE.get_bone_pose(pose, b, LS)) for b in FING}})
    out[p.rsplit("/", 1)[1]] = {"len": L, "frames": fr}
json.dump(out, open(OUT, "w"))
print("[dump_left_m16] chunk", CHUNK, "clips", len(paths), "of", total, "->", OUT)
import gc; gc.collect()
