# @Description: Sample a weapon's hold in its clips: weapon (attach socket) relative to hand_r, per clip and frame (read only)
import json
import unreal
APE = unreal.AnimPoseExtensions
W, LS = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL
HERO = "/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh"
SOCKETS = globals().get("SOCKETS", ["RightHandRifleSocketAim"])
FOLDER = globals().get("FOLDER", "/Game/AZ/Assets/M16/Riffle_RTG_MH")
OUT = globals().get("OUT", "C:/UnrealEngine/Games/AZ/Saved/wgs/hold_m16.json")
MAXCLIPS = globals().get("MAXCLIPS", 40)
def tr(t):
    q = t.rotation
    return [t.translation.x, t.translation.y, t.translation.z, q.x, q.y, q.z, q.w]
hero = unreal.load_asset(HERO)
socks = {}
for n in SOCKETS:
    s = hero.find_socket(n)
    socks[n] = (str(s.get_editor_property("bone_name")), unreal.Transform(s.get_editor_property("relative_location"), s.get_editor_property("relative_rotation"), unreal.Vector(1, 1, 1)))
ar = unreal.AssetRegistryHelpers.get_asset_registry()
paths = sorted(str(a.package_name) for a in ar.get_assets_by_path(FOLDER, recursive=True) if str(a.asset_class_path.asset_name) == "AnimSequence")[:MAXCLIPS]
FING = ["hand_r"] + ["%s_0%d_r" % (f, i) for f in ("thumb", "index", "middle", "ring", "pinky") for i in (1, 2, 3)] + ["%s_metacarpal_r" % f for f in ("index", "middle", "ring", "pinky")]
opts = unreal.AnimPoseEvaluationOptions()
out = {"sockets": {n: [b, tr(t)] for n, (b, t) in socks.items()}, "clips": {}}
for p in paths:
    seq = unreal.load_asset(p); L = seq.get_play_length()
    fr = []
    for k in range(5):
        t = L * k / 5.0
        pose = APE.get_anim_pose_at_time(seq, t, opts)
        hand = APE.get_bone_pose(pose, "hand_r", W)
        rec = {"t": t, "hand_ws": tr(hand)}
        for n, (b, st) in socks.items():
            wpn = unreal.MathLibrary.compose_transforms(st, APE.get_bone_pose(pose, b, W))
            rec["weapon_in_hand_" + n] = tr(unreal.MathLibrary.make_relative_transform(wpn, hand))
        rec["local"] = {b: tr(APE.get_bone_pose(pose, b, LS)) for b in FING[1:]}
        fr.append(rec)
    out["clips"][p.split("/")[-1]] = {"len": L, "frames": fr}
json.dump(out, open(OUT, "w"))
print("clips", len(paths), "->", OUT)
import gc; gc.collect()
