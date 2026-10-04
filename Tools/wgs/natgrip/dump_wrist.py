# @Description: Dump right forearm/hand local + ref for Winchester clips (read only)
import json, unreal
APE = unreal.AnimPoseExtensions
LS, WS = unreal.AnimPoseSpaces.LOCAL, unreal.AnimPoseSpaces.WORLD
hero = unreal.load_asset("/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh")
comp = unreal.new_object(unreal.SkeletalMeshComponent); comp.set_skinned_asset_and_update(hero, True)
def tr(t):
    q = t.rotation; return [t.translation.x, t.translation.y, t.translation.z, q.x, q.y, q.z, q.w]
B = ["upperarm_r", "lowerarm_r", "hand_r", "lowerarm_twist_01_r", "lowerarm_twist_02_r", "lowerarm_correctiveRoot_r"]
out = {"ref": {}, "clips": {}}
for b in B:
    i = comp.get_bone_index(unreal.Name(b))
    if i >= 0:
        out["ref"][b] = tr(comp.get_ref_pose_transform(i)); out.setdefault("parent", {})[b] = str(comp.get_parent_bone(unreal.Name(b)))
opts = unreal.AnimPoseEvaluationOptions()
for p in ["/Game/AZ/Assets/Master/RifleMega/Rifle_AimOffsets/Rifle01/AZ_MST_Rifle01_St_Aim_CC", "/Game/AZ/Assets/Master/RifleMega/Rifle_Styly01_St/Rifle01_IdleSet/AZ_MST_Rifle01_St_Idle00"]:
    pose = APE.get_anim_pose_at_time(unreal.load_asset(p), 0.0, opts)
    out["clips"][p.split("/")[-1]] = {b: {"l": tr(APE.get_bone_pose(pose, b, LS)), "w": tr(APE.get_bone_pose(pose, b, WS))} for b in out["ref"]}
json.dump(out, open("C:/UnrealEngine/Games/AZ/Saved/wgs/win_wrist.json", "w"), indent=1)
print(list(out["ref"].keys()), out.get("parent"))
