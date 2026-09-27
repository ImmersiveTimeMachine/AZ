"""Weapon grip - ONE-CLICK REFIT after moving a hand socket by eye (run inside Unreal, PIE stopped).

Reads the CURRENT hand placements:
  - right hand: RightHandWinchesterSocket on the hero body mesh (edit it on SKM_AZ_Master or the hero mesh);
  - left hand : LeftHandGrip on the weapon's skeletal mesh (SK_Winchester).
Keeps both placements exactly as they are and re-solves only the FINGERS of both hands against the weapon geometry
(Tools/az_grip_solve2.py with AZ_GRIP_KEEP_HANDS=1 + AZ_GRIP_LEFT_HAND), then writes the grip pose asset
(Tools/az_grip_apply.py MODE assets: AS_Grip_Winchester + mirrors LeftHandGrip onto SM_Winchester_Whole).
The previous grip pose is kept as AS_Grip_Winchester_prev.

Run (Output Log, switch "Cmd" to "Python"):
    exec(open(r"C:/UnrealEngine/Games/AZ/Tools/az_grip_refit.py").read())
The editor freezes for ~1-2 minutes while the fingers are solved. The report is printed to the Output Log
(filter "AZGRIP") and saved to Saved/az_grip_refit_report.txt.
"""
import os
import sys

import unreal

TOOLS = r"C:/UnrealEngine/Games/AZ/Tools"
SAVED = r"C:/UnrealEngine/Games/AZ/Saved"
WEAPON_SKM = "/Game/AZ/Assets/Weapons/Winchester_Rifle/Skeletal/SK_Winchester"
GRIP_POSE = "/Game/AZ/Blueprints/Animation/WeaponGrip/AS_Grip_Winchester"
EAL = unreal.EditorAssetLibrary


def say(msg):
    unreal.log("AZGRIP refit: " + msg)


def run():
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        say("STOP PIE first - assets cannot be saved while playing. Nothing done.")
        return
    # 0. save the user's socket edits so the solver and the game read the same values
    dirty = {d.get_name() for d in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    for p in (WEAPON_SKM, "/Game/AZ/Assets/Characters/Master/SKM_AZ_Master",
              "/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh"):
        if p in dirty:
            say("saved your edit on %s: %s" % (p.rsplit("/", 1)[1], EAL.save_asset(p, only_if_is_dirty=False)))
    # 0b. the right-hand socket is tuned on the preview mesh SKM_AZ_Master -> the hero mesh (game) + json follow it
    master = unreal.load_asset("/Game/AZ/Assets/Characters/Master/SKM_AZ_Master").find_socket("RightHandWinchesterSocket")
    hero_path = "/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh"
    hero_mesh = unreal.load_asset(hero_path)
    hero = hero_mesh.find_socket("RightHandWinchesterSocket")
    ml, mr = master.get_editor_property("relative_location"), master.get_editor_property("relative_rotation")
    hl, hr = hero.get_editor_property("relative_location"), hero.get_editor_property("relative_rotation")
    if (ml - hl).length() > 1e-3 or abs(mr.pitch - hr.pitch) + abs(mr.yaw - hr.yaw) + abs(mr.roll - hr.roll) > 1e-3:
        hero_mesh.modify()
        hero.modify()
        hero.set_editor_property("relative_location", ml)
        hero.set_editor_property("relative_rotation", mr)
        say("right hand socket copied SKM_AZ_Master -> hero: saved %s" % EAL.save_asset(hero_path, only_if_is_dirty=False))
        import json
        jp = os.path.join(TOOLS, "hero_sockets.json")
        data = json.load(open(jp, encoding="utf-8"))
        data["RightHandWinchesterSocket"]["loc"] = [ml.x, ml.y, ml.z]
        data["RightHandWinchesterSocket"]["rot_pitch_yaw_roll"] = [mr.pitch, mr.yaw, mr.roll]
        json.dump(data, open(jp, "w", encoding="utf-8"), indent=1, sort_keys=True)
    say("right hand (RightHandWinchesterSocket) loc %s rot %s" % (ml, mr))
    # 1. left hand target = LeftHandGrip on the weapon mesh
    s = unreal.load_asset(WEAPON_SKM).find_socket("LeftHandGrip")
    l, r = s.get_editor_property("relative_location"), s.get_editor_property("relative_rotation")
    q = unreal.Rotator(roll=r.roll, pitch=r.pitch, yaw=r.yaw).quaternion()
    left = "%f,%f,%f,%f,%f,%f,%f" % (l.x, l.y, l.z, q.x, q.y, q.z, q.w)
    say("left hand (LeftHandGrip) %s" % left)
    # 2. reference hold with the current right-hand socket
    exec(open(os.path.join(TOOLS, "az_grip_export.py")).read(), {"OUT_NAME": "az_grip_reference_refit.json"})
    # 3. fingers only, hands fixed
    os.environ["AZ_GRIP_KEEP_HANDS"] = "1"
    os.environ["AZ_GRIP_LEFT_HAND"] = left
    argv = sys.argv
    sys.argv = ["az_grip_solve2.py", "az_grip_reference_refit.json", "az_grip_solution_refit.json"]
    try:
        solver = os.path.join(TOOLS, "az_grip_solve2.py")
        exec(open(solver).read(), {"__name__": "__main__", "__file__": solver})
    finally:
        sys.argv = argv
        os.environ.pop("AZ_GRIP_KEEP_HANDS", None)
        os.environ.pop("AZ_GRIP_LEFT_HAND", None)
    import json
    report = json.load(open(os.path.join(SAVED, "az_grip_solution_refit.json")))["report"]
    for line in report:
        say(line)
    # 4. keep the previous pose, write the new one
    bak = GRIP_POSE + "_prev"
    if EAL.does_asset_exist(bak):
        EAL.delete_asset(bak)
    EAL.duplicate_asset(GRIP_POSE, bak)
    EAL.save_asset(bak, only_if_is_dirty=False)
    exec(open(os.path.join(TOOLS, "az_grip_apply.py")).read(),
         {"MODE": "assets", "SOLUTION_NAME": "az_grip_solution_refit.json"})
    with open(os.path.join(SAVED, "az_grip_refit_report.txt"), "w") as fh:
        fh.write("left %s\n%s\n" % (left, "\n".join(report)))
    say("DONE - fingers re-solved and saved to %s (previous kept as %s_prev). Clearance: + = gap, - = inside (cm)."
        % (GRIP_POSE.rsplit("/", 1)[1], GRIP_POSE.rsplit("/", 1)[1]))


run()
