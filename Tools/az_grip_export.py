"""Weapon grip pilot - export the reference hold for the grip solver (read-only).

Reference = the MetaHuman RifleMega clip (original mocap fingers, before any finger edits) at a holding frame, with
the weapon placed by its hand socket on az_weapon_r (at the hold az_weapon_r == hand_r). Everything the solver needs
is written in WEAPON-MODEL space: every bone's world transform, every bone's local transform (to build the grip pose
asset), the weapon's triangles.

    CLIP / FRAME / WEAPON_MESH / SOCKET overridable;  exec(open(r"C:/UnrealEngine/Games/AZ/Tools/az_grip_export.py").read())
Output: Saved/az_grip_reference.json
"""
import json
import os

import unreal

APE = unreal.AnimPoseExtensions
W, LOC = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL
CLIP = globals().get("CLIP", "/Game/AZ/Assets/RifleMega/Rifle_ReloadingSet/Winchester/AZ_RTG_MH_Rifle01_St_Reload_Winch")
FRAME = globals().get("FRAME", 0)
WEAPON_MESH = globals().get("WEAPON_MESH", "/Game/AZ/Assets/Weapons/Winchester_Rifle/SM_Winchester_Whole")
SOCKET = globals().get("SOCKET", "RightHandWinchesterSocket")
HERO = "/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh"
OUT = os.path.join(unreal.Paths.project_saved_dir(), globals().get("OUT_NAME", "az_grip_reference.json"))


def tr(t):
    q = t.rotation
    return [t.translation.x, t.translation.y, t.translation.z, q.x, q.y, q.z, q.w]


hero = unreal.load_asset(HERO)
sm = unreal.load_asset(WEAPON_MESH)
verts, tris = [], []
for s in range(sm.get_num_sections(0)):
    r = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(sm, 0, s)
    base = len(verts)
    verts += [[v.x, v.y, v.z] for v in r[0]]
    tris += [base + i for i in r[1]]
so = hero.find_socket(SOCKET)
sock = unreal.Transform(so.relative_location, so.relative_rotation, so.relative_scale)
o = unreal.AnimPoseEvaluationOptions()
o.set_editor_property("optional_skeletal_mesh", hero)
o.set_editor_property("should_retarget", False)
clip = unreal.load_asset(CLIP)
p = APE.get_anim_pose_at_frame(clip, FRAME, o)
names = [str(n) for n in APE.get_bone_names(p)]
weapon_cs = unreal.MathLibrary.compose_transforms(sock, APE.get_bone_pose(p, "az_weapon_r", W))
inv = weapon_cs.inverse()
out = {"clip": CLIP, "frame": FRAME, "weapon_mesh": WEAPON_MESH, "socket": SOCKET, "socket_rel": tr(sock),
       "verts": verts, "tris": tris, "bones": names,
       "model": {b: tr(unreal.MathLibrary.compose_transforms(APE.get_bone_pose(p, b, W), inv)) for b in names},
       "local": {b: tr(APE.get_bone_pose(p, b, LOC)) for b in names},
       # the skeleton's reference pose = the OPEN hand the grasp solver closes from
       "ref_local": (lambda rp: {b: tr(APE.get_bone_pose(rp, b, LOC)) for b in names})(APE.get_reference_pose(hero.skeleton)),
       "parent": {b: str(APE.get_bone_parent_name(p, b)) if hasattr(APE, "get_bone_parent_name") else "" for b in names}}
with open(OUT, "w") as fh:
    json.dump(out, fh)
unreal.log("AZGRIPREF wrote %s: %d bones, %d verts, %d tris" % (OUT, len(names), len(verts), len(tris) // 3))
