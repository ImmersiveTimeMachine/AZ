# @Description: Add or update a weapon hand socket on the hero body mesh, SKM_AZ_Master and Tools/hero_sockets.json
# exec(open(r"C:/UnrealEngine/Games/AZ/Tools/wgs/add_hand_socket.py").read(),
#      {"NAME": "RightHandM16Socket", "BONE": "az_weapon_r", "XF": [tx, ty, tz, qx, qy, qz, qw]})
# Rule (user): every weapon gets its OWN hand socket on the body (RightHand<Weapon>Socket). hero_sockets.json is what
# Tools/metahuman_fixup.py restores after a MetaHuman re-assembly, so it is kept in sync here.
import json
import unreal

NAME = globals()["NAME"]
BONE = globals().get("BONE", "az_weapon_r")
XF = globals()["XF"]
MESHES = globals().get("MESHES", ["/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh",
                                  "/Game/AZ/Assets/Characters/Master/SKM_AZ_Master"])
RECORD = "C:/UnrealEngine/Games/AZ/Tools/hero_sockets.json"
loc = unreal.Vector(XF[0], XF[1], XF[2])
rot = unreal.Quat(XF[3], XF[4], XF[5], XF[6]).rotator()
xf = unreal.Transform(loc, rot, unreal.Vector(1, 1, 1))
for path in MESHES:
    mesh = unreal.load_asset(path)
    mesh.modify()
    sock = mesh.find_socket(NAME)
    created = sock is None
    if created:                      # same recipe as Tools/metahuman_fixup.py (socket_name is read-only from Python)
        sock = unreal.new_object(unreal.SkeletalMeshSocket, outer=mesh)
        mesh.add_socket(sock, False)                     # mesh-only
        mesh.rename_socket(sock.get_editor_property("socket_name"), NAME)
    sock.modify()
    sock.set_socket_parent(mesh, BONE)
    sock.set_socket_local_transform(xf)
    ok = unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    s = unreal.load_asset(path).find_socket(NAME)
    print("[add_hand_socket]", path.rsplit("/", 1)[1], "created" if created else "updated", "saved", ok, "->",
          s and (str(s.get_editor_property("bone_name")), s.get_editor_property("relative_location")))
rec = json.load(open(RECORD))
rec[NAME] = {"bone": BONE, "loc": [loc.x, loc.y, loc.z], "rot_pitch_yaw_roll": [rot.pitch, rot.yaw, rot.roll], "scale": [1.0, 1.0, 1.0]}
json.dump(rec, open(RECORD, "w"), indent=1)
print("[add_hand_socket] recorded in", RECORD)
import gc
gc.collect()
