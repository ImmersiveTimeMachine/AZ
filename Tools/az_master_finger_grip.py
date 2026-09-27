"""Master RifleMega clips: open the finger curl so the fingers sit ON our Winchester instead of inside it.

The mocap fingers are curled for the pack's thinner forearm / stock wrist; on our Winchester (thicker) the middle and
distal phalanges sank 0.5-1.6 cm into the wood (user screenshots 2026-09-26). Per-finger openings (2-24 deg, about
the knuckle line, shared 0.35/1/1 over the _01/_02/_03 joints) were solved against the model's triangles on the aim
pose (scratchpad az_finger_final.py -> Saved/az_finger_fix.json: bone -> local delta quaternion).

Applied only while that hand holds the gun:
  right hand - the gun is still in the hand: the az_weapon_r local track is near identity (reloads move it away);
  left hand  - hand_l sits at its grip on the gun: its position in az_weapon_r space is within a few cm of the
               holding reference (Winchester reload frame 0; the pack IKs the left hand to the gun, so it is rigid).
local_new = local_old * nlerp(identity, delta, weight).  Clips of the Automatic / DoubleBarrel / ShotGun sets are
skipped (other guns). Specific to the Winchester - the runtime grip for other weapons is still to be built.

Run AFTER az_master_riflemega.py convert and az_master_left_grip.py, ONCE per clip (not idempotent: a second run
opens the fingers again - re-convert first). Clips done: see the report.
    MODE = "fix"; CHUNK = i (of CHUNK_SIZE) or NAMES = [pack sub/name, ...]
    exec(open(r"C:/UnrealEngine/Games/AZ/Tools/az_master_finger_grip.py").read())
Report: Saved/az_master_finger_grip_report.txt
"""
import json
import math
import os

import unreal

MODE = globals().get("MODE", "fix")
NAMES = globals().get("NAMES", None)
CHUNK = globals().get("CHUNK", None)
CHUNK_SIZE = globals().get("CHUNK_SIZE", 50)
APE = unreal.AnimPoseExtensions
EAL = unreal.EditorAssetLibrary
W, LOC = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL
DST_ROOT, DST_PREFIX = "/Game/AZ/Assets/Master/RifleMega/", "AZ_MST_"
REF_CLIP = DST_ROOT + "Rifle_ReloadingSet/Winchester/AZ_MST_Rifle01_St_Reload_Winch"
MASTER_MESH = "/Game/AZ/Assets/Characters/Master/SKM_AZ_Master"
SKIP_SETS = ("/Automatic/", "/DoubleBarrel/", "/ShotGun/")
SAVED = unreal.Paths.project_saved_dir()
FIX = os.path.join(SAVED, "az_finger_fix.json")
REPORT = os.path.join(SAVED, "az_master_finger_grip_report.txt")


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3.0 - 2.0 * t)


def nlerp_identity(q, w):
    x, y, z, s = q[0] * w, q[1] * w, q[2] * w, 1.0 - w + q[3] * w
    n = math.sqrt(x * x + y * y + z * z + s * s)
    return (x / n, y / n, z / n, s / n)


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def opts(mesh):
    o = unreal.AnimPoseEvaluationOptions()
    o.set_editor_property("optional_skeletal_mesh", mesh)
    o.set_editor_property("should_retarget", False)
    o.set_editor_property("extract_root_motion", False)
    return o


def left_in_gun(pose):
    g = APE.get_bone_pose(pose, "az_weapon_r", W)
    return g.inverse_transform_location(APE.get_bone_pose(pose, "hand_l", W).translation)


def all_clips():
    return sorted(p.split(".")[0] for p in EAL.list_assets(DST_ROOT, recursive=True, include_folder=False)
                  if p.rsplit("/", 1)[-1].startswith(DST_PREFIX) and not any(s in p for s in SKIP_SETS))


def fix_clip(path, deltas, ref_l, o):
    clip = unreal.load_asset(path)
    n = unreal.AnimationLibrary.get_num_keys(clip)
    bones = sorted(deltas)
    tracks = {b: ([], [], []) for b in bones}
    held_r = held_l = 0
    for f in range(n):
        p = APE.get_anim_pose_at_frame(clip, f, o)
        a = APE.get_bone_pose(p, "az_weapon_r", LOC)
        ang = math.degrees(2.0 * math.acos(min(1.0, abs(a.rotation.w))))
        w_r = (1.0 - smoothstep(1.0, 3.0, a.translation.length())) * (1.0 - smoothstep(3.0, 8.0, ang))
        w_l = 1.0 - smoothstep(2.0, 5.0, (left_in_gun(p) - ref_l).length())
        held_r += w_r > 0.99
        held_l += w_l > 0.99
        for b in bones:
            t = APE.get_bone_pose(p, b, LOC)
            w = w_l if b.endswith("_l") else w_r
            q = (t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w)
            if w > 1e-3:
                q = qmul(q, nlerp_identity(deltas[b], w))
            tracks[b][0].append(unreal.Vector(t.translation.x, t.translation.y, t.translation.z))
            tracks[b][1].append(unreal.Quat(*q))
            tracks[b][2].append(unreal.Vector(t.scale3d.x, t.scale3d.y, t.scale3d.z))
    if not (held_r or held_l):
        return "UNCHANGED %s (hands never on the gun)" % path.rsplit("/", 1)[-1]
    clip.modify()
    c = clip.controller
    c.open_bracket("AZ master finger grip", False)
    try:
        for b, (p_, r_, s_) in tracks.items():
            if not c.set_bone_track_keys(unreal.Name(b), p_, r_, s_, False):
                raise RuntimeError("set_bone_track_keys failed on %s" % b)
    finally:
        c.close_bracket(False)
    saved = EAL.save_asset(path, only_if_is_dirty=False)
    return "FIXED %s: %d frames, right hand on the gun %d, left %d; saved %s" % (path.rsplit("/", 1)[-1], n, held_r, held_l, saved)


if MODE == "fix":
    with open(FIX, encoding="utf-8") as fh:
        deltas = {k: tuple(v) for k, v in json.load(fh).items()}
    mesh = unreal.load_asset(MASTER_MESH)
    o = opts(mesh)
    ref_l = left_in_gun(APE.get_anim_pose_at_frame(unreal.load_asset(REF_CLIP), 0, o))
    if NAMES is not None:
        paths = [p if p.startswith("/Game/") else DST_ROOT + p.rsplit("/", 1)[0] + "/" + DST_PREFIX + p.rsplit("/", 1)[1] for p in NAMES]
    else:
        paths = all_clips()
        if CHUNK is not None:
            paths = paths[CHUNK * CHUNK_SIZE:(CHUNK + 1) * CHUNK_SIZE]
    lines = []
    for path in paths:
        try:
            lines.append(fix_clip(path, deltas, ref_l, o))
        except Exception as e:
            lines.append("FAIL %s: %s" % (path, e))
        with open(REPORT, "a", encoding="utf-8") as fh:
            fh.write(lines[-1] + "\n")
    unreal.log("AZFGRIP chunk %s: %d clips, %d fixed, %d unchanged, %d failed (of %d eligible)" % (
        CHUNK, len(lines), sum(l.startswith("FIXED") for l in lines), sum(l.startswith("UNCHANGED") for l in lines),
        sum(l.startswith("FAIL") for l in lines), len(all_clips())))
