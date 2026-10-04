# @Description: Put a weapon's LeftHandGrip socket on the solved support-hand hold (hand_l transform in the weapon mesh's component space)
# exec(open(r"C:/UnrealEngine/Games/AZ/Tools/wgs/set_weapon_left_grip.py").read(),
#      {"MESH": "/Game/AZ/Assets/M16/SKL/M16_Skeleton", "DATA": "C:/UnrealEngine/Games/AZ/Saved/wgs/nat_grip_m16_left.json"})
# DATA "hand_in_weapon" = [tx, ty, tz, qx, qy, qz, qw] of hand_l in the weapon mesh's component space (the solver's weapon
# space). The socket keeps its parent bone; its local transform = target relative to that bone's ref-pose transform.
# The AZ Weapon Grip node IKs hand_l onto this socket (full transform, rotation included). Back the mesh up first.
import json
import unreal
MESH = globals()["MESH"]
DATA = globals()["DATA"]
SOCKET = globals().get("SOCKET", "LeftHandGrip")
v = json.load(open(DATA))["hand_in_weapon"]
target = unreal.Transform(unreal.Vector(v[0], v[1], v[2]), unreal.Quat(v[3], v[4], v[5], v[6]).rotator(), unreal.Vector(1, 1, 1))
mesh = unreal.load_asset(MESH)
sock = mesh.find_socket(SOCKET)
bone = sock.get_editor_property("bone_name")
comp = unreal.new_object(unreal.SkeletalMeshComponent)
comp.set_skinned_asset_and_update(mesh, True)
def bone_cs(b):
    t = unreal.Transform(); chain = []
    while str(b) != "None":
        chain.append(b); b = comp.get_parent_bone(b)
    for c in reversed(chain):
        t = unreal.MathLibrary.compose_transforms(comp.get_ref_pose_transform(comp.get_bone_index(c)), t)
    return t
bcs = bone_cs(bone)
local = unreal.MathLibrary.make_relative_transform(target, bcs)
_l, _r = sock.get_editor_property("relative_location"), sock.get_editor_property("relative_rotation")
before = ((round(_l.x, 3), round(_l.y, 3), round(_l.z, 3)), (round(_r.pitch, 2), round(_r.yaw, 2), round(_r.roll, 2)))   # copy: the getter returns a live view
mesh.modify(); sock.modify()
sock.set_socket_local_transform(local)
ok = unreal.EditorAssetLibrary.save_asset(MESH, only_if_is_dirty=False)
s = unreal.load_asset(MESH).find_socket(SOCKET)
back = unreal.MathLibrary.compose_transforms(unreal.Transform(s.get_editor_property("relative_location"), s.get_editor_property("relative_rotation"), unreal.Vector(1, 1, 1)), bcs)
q, tq = back.rotation, target.rotation
print("[set_weapon_left_grip]", MESH.rsplit("/", 1)[1], SOCKET, "on", bone, "| before", before, "| saved", ok,
      "| check: pos err %.4f cm, quat dot %.6f" % ((back.translation - target.translation).length(), abs(q.x * tq.x + q.y * tq.y + q.z * tq.z + q.w * tq.w)))
import gc
gc.collect()
