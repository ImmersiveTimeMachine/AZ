# @Description: Dump the left hand for the natural-grasp solver: LeftHandGrip socket (hand_l in weapon space), mesh ref locals, clip finger locals (read only)
import json
import unreal
APE = unreal.AnimPoseExtensions
LS = unreal.AnimPoseSpaces.LOCAL
HERO = "/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh"
WEAPON = "/Game/AZ/Assets/Weapons/Winchester_Rifle/Skeletal/SK_Winchester"
CLIP = "/Game/AZ/Assets/Master/RifleMega/Rifle_AimOffsets/Rifle01/AZ_MST_Rifle01_St_Aim_CC"
OUT = "C:/UnrealEngine/Games/AZ/Saved/wgs/win_hand_live_l.json"
FINGERS = ("thumb", "index", "middle", "ring", "pinky")
BONES = ["hand_l"]; PARENT = {}
for f in FINGERS:
    prev = "hand_l"
    for b in ([] if f == "thumb" else ["%s_metacarpal_l" % f]) + ["%s_0%d_l" % (f, i) for i in (1, 2, 3)]:
        BONES.append(b); PARENT[b] = prev; prev = b
def tr(t):
    q = t.rotation
    return [t.translation.x, t.translation.y, t.translation.z, q.x, q.y, q.z, q.w]
hero = unreal.load_asset(HERO)
comp = unreal.new_object(unreal.SkeletalMeshComponent); comp.set_skinned_asset_and_update(hero, True)
ref_local = {b: tr(comp.get_ref_pose_transform(comp.get_bone_index(unreal.Name(b)))) for b in BONES}
s = unreal.load_asset(WEAPON).find_socket("LeftHandGrip")
grip = unreal.Transform(s.get_editor_property("relative_location"), s.get_editor_property("relative_rotation"), s.get_editor_property("relative_scale"))
pose = APE.get_anim_pose_at_time(unreal.load_asset(CLIP), 0.0, unreal.AnimPoseEvaluationOptions())
local = {b: tr(APE.get_bone_pose(pose, b, LS)) for b in BONES[1:]}
json.dump({"bones": BONES, "parents": PARENT, "ref_local": ref_local, "hand_in_weapon": tr(grip), "clip_local": local}, open(OUT, "w"), indent=1)
print("wrote", OUT)
import gc; gc.collect()
