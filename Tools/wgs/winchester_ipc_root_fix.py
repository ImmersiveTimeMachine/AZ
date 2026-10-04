# @Description: Copy the root track of each RifleMega root-motion twin into its IPC loop and set loop and root flags
# Run in the editor:  exec(open(r"C:/UnrealEngine/Games/AZ/Tools/wgs/winchester_ipc_root_fix.py").read(), {"ONLY": [...], "SAVE": True})
#
# Why (2026-09-27, memory R17): the pack's _IPC loops have a root track that is exactly zero and loop / root motion
# flags off. In a trajectory schema (PSS_v2_SurvivalMan_Loco) a flat root claims "standing still" for every candidate,
# so motion matching returns garbage costs; loop=False freezes the loop after one cycle. The non-IPC twin of each
# clip is the same capture with the root moving (verified: every bone relative to root identical, same key count),
# so the IPC clip gets the twin's root keys + loop / enable_root_motion / force_root_lock = True - the M16 IPC setup.
import unreal, json, os, time

AL = unreal.AnimationLibrary
APE = unreal.AnimPoseExtensions
M = "/Game/AZ/Assets/Master/RifleMega/"
DIRS = ["F", "45R", "90R", "135R", "B", "135L", "90L", "45L"]
LOOPS = ([M + "Rifle_Styly02_St/Rifle02_LocomotionSet/AZ_MST_Rifle02_St_%s_%s" % (g, d) for g in ("Walk", "Run") for d in DIRS]
         + [M + "Rifle_Styly01_St/Rifle01_LocomotionSet/AZ_MST_Rifle01_St_%s_%s" % (g, d) for g in ("Walk", "Run") for d in DIRS]
         + [M + "Rifle_Cr/Rifle_Cr_LocomotionSet/AZ_MST_Rifle_Cr_Walk_%s" % d for d in DIRS]
         + [M + "Rifle_Styly01_St/Rifle01_LocomotionSet/AZ_MST_Rifle01_St_Sprint"])
IDLES = [M + "Rifle_Styly02_St/Rifle02_IdleSet/AZ_MST_Rifle02_St_Idle00",
         M + "Rifle_Styly01_St/Rifle01_IdleSet/AZ_MST_Rifle01_St_Idle00",
         M + "Rifle_Cr/Rifle_Cr_IdleSet/AZ_MST_Rifle_Cr_Idle00"]
only = globals().get("ONLY")
save = globals().get("SAVE", False)
OUT = "C:/UnrealEngine/Games/AZ/Saved/wgs/r1/ipc_root_fix.json"

def disk(p):
    return "C:/UnrealEngine/Games/AZ/Content/" + p[len("/Game/"):] + ".uasset"

opts = unreal.AnimPoseEvaluationOptions()
report = []
for twin_path in LOOPS:
    if only and not any(twin_path.endswith(o) for o in only):
        continue
    ipc_path = twin_path + "_IPC"
    twin, ipc = unreal.load_asset(twin_path), unreal.load_asset(ipc_path)
    rec = {"clip": ipc_path.split("/")[-1]}
    n = AL.get_num_keys(twin)
    if AL.get_num_keys(ipc) != n or abs(twin.get_play_length() - ipc.get_play_length()) > 1e-4:
        rec["error"] = "key count / length differ %d/%d" % (n, AL.get_num_keys(ipc)); report.append(rec); continue
    pos, rot, scl, flat = [], [], [], 0.0
    for k in range(n):
        tr = APE.get_bone_pose(APE.get_anim_pose_at_frame(twin, k, opts), "root", unreal.AnimPoseSpaces.LOCAL)
        ti = APE.get_bone_pose(APE.get_anim_pose_at_frame(ipc, k, opts), "root", unreal.AnimPoseSpaces.LOCAL)
        flat = max(flat, ti.translation.length())
        pos.append(tr.translation); rot.append(tr.rotation); scl.append(tr.scale3d)
    rec["ipc_root_max_before_cm"] = round(flat, 3)
    rec["root_delta_cm"] = [round(v, 1) for v in (pos[-1].x - pos[0].x, pos[-1].y - pos[0].y, pos[-1].z - pos[0].z)]
    rec["speed_cm_s"] = round((pos[-1] - pos[0]).length() / twin.get_play_length(), 1)
    # FORCE: two crouch IPC clips (Cr_Walk_135R / 45L) carry a 3 cm root wobble instead of an exact zero.
    if flat > 0.01 and not globals().get("FORCE", False):
        # already rebuilt (idempotent re-run): verify instead of writing
        rec["already"] = True
    else:
        c = ipc.controller
        c.open_bracket("AZ Winchester IPC root from twin", False)
        ok = c.set_bone_track_keys(unreal.Name("root"), pos, rot, scl, False)
        c.close_bracket(False)
        rec["track_written"] = bool(ok)
    ipc.set_editor_property("loop", True)
    ipc.set_editor_property("enable_root_motion", True)
    ipc.set_editor_property("force_root_lock", True)
    # verify from a fresh pose read
    te = APE.get_bone_pose(APE.get_anim_pose_at_frame(ipc, n - 1, opts), "root", unreal.AnimPoseSpaces.LOCAL)
    rec["verify_end_err_cm"] = round((te.translation - pos[-1]).length(), 4)
    if save:
        before = os.path.getmtime(disk(ipc_path))
        unreal.EditorAssetLibrary.save_loaded_asset(ipc, False)
        rec["saved"] = os.path.getmtime(disk(ipc_path)) > before
    report.append(rec)

if not only:
    for p in IDLES:
        s = unreal.load_asset(p)
        rec = {"clip": p.split("/")[-1], "loop_before": s.get_editor_property("loop")}
        s.set_editor_property("loop", True)
        if save:
            before = os.path.getmtime(disk(p))
            unreal.EditorAssetLibrary.save_loaded_asset(s, False)
            rec["saved"] = os.path.getmtime(disk(p)) > before
        report.append(rec)

os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(report, open(OUT, "w"), indent=1)
print(json.dumps(report)[:3000])
import gc; gc.collect()
