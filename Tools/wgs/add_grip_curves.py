# @Description: Add constant AZ_Grip_R / AZ_Grip_L curves to a weapon's clips (motion untouched; idempotent; chunked)
# exec(open(r"C:/UnrealEngine/Games/AZ/Tools/wgs/add_grip_curves.py").read(),
#      {"FOLDER": "/Game/AZ/Assets/M16/Riffle_RTG_MH", "R": 1.0, "L": 0.0, "CHUNK": 0, "CHUNK_SIZE": 150})
# AZ_Grip_R / AZ_Grip_L = how much the right / left hand holds the weapon's grip (AZ Weapon Grip + Body Clearance).
# A clip WITHOUT the curve counts as 1 on its own, but blends toward 0 against clips that have it -> twitch; so every
# clip a weapon plays should carry both. L = 0 keeps a set's own left hand (no IK, no baked fingers).
import unreal

FOLDER = globals()["FOLDER"]
R = float(globals().get("R", 1.0))
L = float(globals().get("L", 1.0))
CHUNK = globals().get("CHUNK", None)
CHUNK_SIZE = int(globals().get("CHUNK_SIZE", 150))
AL = unreal.AnimationLibrary
EAL = unreal.EditorAssetLibrary
FLOAT = unreal.RawCurveTrackTypes.RCT_FLOAT
ar = unreal.AssetRegistryHelpers.get_asset_registry()
paths = sorted(str(a.package_name) for a in ar.get_assets_by_path(FOLDER, recursive=True)
               if str(a.asset_class_path.asset_name) == "AnimSequence")
total = len(paths)
if CHUNK is not None:
    paths = paths[CHUNK * CHUNK_SIZE:(CHUNK + 1) * CHUNK_SIZE]
written, had, failed = 0, 0, []
for p in paths:
    s = unreal.load_asset(p)
    length = s.get_play_length()
    changed = False
    for name, value in (("AZ_Grip_R", R), ("AZ_Grip_L", L)):
        if AL.does_curve_exist(s, name, FLOAT):
            continue
        AL.add_curve(s, name, FLOAT, False)
        AL.add_float_curve_keys(s, name, [0.0, max(length, 0.001)], [value, value])
        changed = True
    if changed:
        if EAL.save_asset(p, only_if_is_dirty=False):
            written += 1
        else:
            failed.append(p)
    else:
        had += 1
print("[add_grip_curves] %s chunk %s: %d clips (of %d), written %d, already had %d, failed %s"
      % (FOLDER, CHUNK, len(paths), total, written, had, failed[:5]))
import gc
gc.collect()
