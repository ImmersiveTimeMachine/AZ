# @Description: Write predicted fingertip markers (Saved/wgs/predicted_tips.json) onto SK_Winchester, backup first
# exec(open(r"C:/UnrealEngine/Games/AZ/Tools/wgs/write_markers.py").read())
import json, time
import unreal

WEAPON = "/Game/AZ/Assets/Weapons/Winchester_Rifle/Skeletal/SK_Winchester"
tips = json.load(open("C:/UnrealEngine/Games/AZ/Saved/wgs/predicted_tips.json"))["tips"]
sk = unreal.load_asset(WEAPON)
backup, moved = {}, {}
sk.modify()
for name, p in tips.items():
    s = sk.find_socket(name)
    old = s.get_editor_property("relative_location")
    backup[name] = [old.x, old.y, old.z]
    new = unreal.Vector(p[0], p[1], p[2])
    moved[name] = round((new - old).length(), 2)
    s.modify()
    s.set_editor_property("relative_location", new)
json.dump(backup, open("C:/UnrealEngine/Games/AZ/Saved/wgs/markers_backup_%s.json" % time.strftime("%Y%m%d_%H%M%S"), "w"), indent=1)
print("moved cm", moved)
print("saved", unreal.EditorAssetLibrary.save_asset(WEAPON, only_if_is_dirty=False))
import gc; gc.collect()
