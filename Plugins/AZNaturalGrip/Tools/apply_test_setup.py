# @Description: Test bed for AZ Natural Grip Apply: copies of the M16 weapon BP, grip pose and mesh (own test socket) + test profiles
# exec(open(r"C:/UnrealEngine/Games/AZ/Plugins/AZNaturalGrip/Tools/apply_test_setup.py").read())
# The real M16 assets are protected: Apply is exercised on copies in /Game/AZ/Tests/NaturalGripApply (git-ignored).
# LeftHandGrip lives on the M16 SKELETON (shared by a mesh copy), so the copy gets its own MESH socket LeftHandGrip_ApplyTest
# at zero, and the BP copy's correction is reset to identity: Apply then has real changes to write and verify. No
# AnimBP is touched; the BP copy is a regular Blueprint.
import unreal

EAL = unreal.EditorAssetLibrary
DST = "/Game/AZ/Tests/NaturalGripApply"
SRC = {
    "mesh": "/Game/AZ/Assets/M16/SKL/M16_Skeleton",
    "pose": "/Game/AZ/Blueprints/Animation/WeaponGrip/AS_Grip_M16",
    "bp": "/Game/AZ/Blueprints/Weapon/AZ_BP_Rifle",
}
COPY = {k: DST + "/" + v.rsplit("/", 1)[1] + "_ApplyTest" for k, v in SRC.items()}
for k in SRC:
    if not EAL.does_asset_exist(COPY[k]):
        EAL.duplicate_asset(SRC[k], COPY[k])
mesh = unreal.load_asset(COPY["mesh"])
if mesh.find_socket("LeftHandGrip_ApplyTest") is None:
    s = unreal.new_object(unreal.SkeletalMeshSocket, outer=mesh)     # socket_name is read-only from Python: add, then rename
    s.set_socket_parent(mesh, "hand_r")
    mesh.add_socket(s, False)
    mesh.rename_socket(s.get_editor_property("socket_name"), "LeftHandGrip_ApplyTest")
s = mesh.find_socket("LeftHandGrip_ApplyTest")
s.set_editor_property("relative_location", unreal.Vector(0, 0, 0))
s.set_editor_property("relative_rotation", unreal.Rotator(0, 0, 0))
bp = unreal.load_asset(COPY["bp"])
cdo = unreal.get_default_object(bp.generated_class())
cdo.set_editor_property("right_hand_grip_correction", unreal.Transform())
cdo.set_editor_property("baked_left_hand_grasp", False)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
for k in COPY:
    EAL.save_asset(COPY[k], only_if_is_dirty=False)
print("socket owner:", s.get_outer().get_path_name())

# test profiles = the M16 ones pointed at the copies
for side in ("Left", "Right"):
    name = "NGP_M16_%s_ApplyTest" % side
    path = DST + "/" + name
    if EAL.does_asset_exist(path):
        prof = unreal.load_asset(path)
    else:
        prof = EAL.duplicate_asset("/AZNaturalGrip/Profiles/NGP_M16_%s" % side, path)
    prof.set_editor_property("weapon_mesh", mesh)
    prof.set_editor_property("weapon_blueprint", bp)
    prof.set_editor_property("grip_pose_override", unreal.load_asset(COPY["pose"]))
    prof.set_editor_property("left_hand_socket", "LeftHandGrip_ApplyTest")
    EAL.save_asset(path, only_if_is_dirty=False)
    print("profile", path)
import gc
gc.collect()
