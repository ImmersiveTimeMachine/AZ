"""Master RifleMega clips: keep the left hand on the weapon while the pack's left hand holds it.

The MetaHuman post-process (Tools/riflemega_retarget.py, step c) put the left hand on the weapon relative to the right
hand, but faded that IK out while the hands were 50..75 cm apart ("hand off the weapon"). In reloads it is the RIGHT
hand that leaves (shells, lever) while the LEFT still holds the gun: those frames fell back to the FK arm, ~5 cm off
the handguard, and the hand slid against the moving gun (user report 2026-09-26, crouch reload ~frame 38).

Here the weight is whether the PACK's left palm is on the PACK's gun (10..14 cm from its nearest vertex; gun geometry
from Saved/az_gun_scan.json, written by Tools/az_gun_contact_scan_export.py). Where that weight is higher than the old
one, the left arm is re-solved onto the same grip target as before (the pack's left hand relative to the pack's right
hand, carried onto the hero's right hand - the gun track uses the same mapping, so hand and gun move together).
Only frames that gain weight change; the hand keeps its world rotation (it already follows the grip). The two-bone
solver is the one from riflemega_retarget.py (minimal swing + in-plane bend).

Run AFTER az_master_riflemega.py MODE=convert (convert copies the MetaHuman clips and would undo this).
    MODE = "fix";  NAMES = ["Rifle_Cr/..../Rifle_Cr_Reload_Winch", ...]   (pack sub/name; default: Saved/az_grip_affected.json)
    exec(open(r"C:/UnrealEngine/Games/AZ/Tools/az_master_left_grip.py").read())
Report: Saved/az_master_left_grip_report.txt
"""
import json
import math
import os
from collections import defaultdict

import unreal

MODE = globals().get("MODE", "fix")
NAMES = globals().get("NAMES", None)
SAVE = globals().get("SAVE", True)
APE = unreal.AnimPoseExtensions
EAL = unreal.EditorAssetLibrary
W, LOC = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL
PACK_ROOT = "/Game/RifleMega_MocapAnimPack/AnimationsFBX/"
DST_ROOT, DST_PREFIX = "/Game/AZ/Assets/Master/RifleMega/", "AZ_MST_"
MASTER_MESH = "/Game/AZ/Assets/Characters/Master/SKM_AZ_Master"
_M = "/Game/RifleMega_MocapAnimPack/Demo/Models/"
MESH_BY_SKEL = {
    "Rifle_Mannequin_A_Skeleton": _M + "Character/Mesh/Rifle_Mannequin_A",
    "Rifle_Auto_Mannequin_A_Skeleton": _M + "forShootingReloading/Character_Automatic/Mesh/Rifle_Auto_Mannequin_A",
    "Rifle_DB_Mannequin_A_Skeleton": _M + "forShootingReloading/Character_DoubleBarrel/Mesh/Rifle_DB_Mannequin_A",
    "Rifle_SG_Mannequin_A_Skeleton": _M + "forShootingReloading/Character_ShotGun/Mesh/Rifle_SG_Mannequin_A",
    "Rifle_Winch_Mannequin_A_Skeleton": _M + "forShootingReloading/Character_Winchester/Mesh/Rifle_Winch_Mannequin_A"}
SAVED = unreal.Paths.project_saved_dir()
GUN_SCAN = os.path.join(SAVED, "az_gun_scan.json")
AFFECTED = os.path.join(SAVED, "az_grip_affected.json")
REPORT = os.path.join(SAVED, "az_master_left_grip_report.txt")
GRIP_FULL, GRIP_NONE = 50.0, 75.0      # the old weight (cm between the hands), as in riflemega_retarget.py
HOLD_IN, HOLD_OUT = 10.0, 14.0         # pack left palm -> nearest pack gun vertex: held / released
REACH = 0.998
ARM = ("clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "hand_r")


# ---------------------------------------------------------------- math (same conventions as riflemega_retarget.py)
def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def length(a): return math.sqrt(dot(a, a))
def lerp(a, b, t): return add(a, mul(sub(b, a), t))
def unit(a):
    n = length(a)
    return (a[0] / n, a[1] / n, a[2] / n) if n > 1e-9 else (0.0, 0.0, 0.0)


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def qinv(a): return (-a[0], -a[1], -a[2], a[3])


def qrot(a, p):
    r = qmul(qmul(a, (p[0], p[1], p[2], 0.0)), qinv(a))
    return (r[0], r[1], r[2])


def qnorm(a):
    n = math.sqrt(sum(c * c for c in a))
    return tuple(c / n for c in a)


def between(a, b):
    d = max(-1.0, min(1.0, dot(a, b)))
    axis = cross(a, b)
    n = length(axis)
    if n < 1e-9:
        return (0.0, 0.0, 0.0, 1.0)
    half = math.acos(d) * 0.5
    s = math.sin(half) / n
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(half))


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3.0 - 2.0 * t)


def two_bone(a, b, c, target, fallback_pole):
    """Verbatim from riflemega_retarget.py: minimal swing + in-plane bend (twist-free)."""
    upper, lower = length(sub(b, a)), length(sub(c, b))
    to_t = sub(target, a)
    d = max(abs(upper - lower) + 0.01, min((upper + lower) * REACH, length(to_t)))
    direction = unit(to_t)
    swing = between(unit(sub(c, a)), direction)
    b1 = qrot(swing, sub(b, a))
    perp = sub(b1, mul(direction, dot(b1, direction)))
    fb = sub(fallback_pole, mul(direction, dot(fallback_pole, direction)))
    w = smoothstep(0.5, 3.0, length(perp))
    pole = unit(add(mul(unit(perp), w), mul(unit(fb), 1.0 - w))) if length(perp) > 1e-6 else unit(fb)
    cos_a = max(-1.0, min(1.0, (upper * upper + d * d - lower * lower) / (2.0 * upper * d)))
    sin_a = math.sqrt(max(0.0, 1.0 - cos_a * cos_a))
    b_new = mul(add(mul(direction, cos_a), mul(pole, sin_a)), upper)
    r_upper = qmul(between(unit(b1), unit(b_new)), swing)
    c_new = mul(direction, d)
    r_lower = between(unit(qrot(r_upper, sub(c, b))), unit(sub(c_new, b_new)))
    return r_upper, r_lower, length(to_t) - d


class Cloud(object):
    """Nearest-vertex distance on a gun point cloud (grid buckets)."""

    def __init__(self, pts, cell=4.0):
        self.cell, self.g = cell, defaultdict(list)
        for p in pts:
            self.g[tuple(int(math.floor(c / cell)) for c in p)].append(tuple(p))

    def dist(self, p, maxr=8):
        k = tuple(int(math.floor(c / self.cell)) for c in p)
        best = 1e18
        for r in range(0, maxr + 1):
            for dx in range(-r, r + 1):
                for dy in range(-r, r + 1):
                    for dz in range(-r, r + 1):
                        if max(abs(dx), abs(dy), abs(dz)) != r:
                            continue
                        for q in self.g.get((k[0] + dx, k[1] + dy, k[2] + dz), ()):
                            best = min(best, (q[0] - p[0]) ** 2 + (q[1] - p[1]) ** 2 + (q[2] - p[2]) ** 2)
            if best < (r * self.cell) ** 2:
                break
        return math.sqrt(best) if best < 1e17 else 99.0


def xf(t):
    return ((t.translation.x, t.translation.y, t.translation.z), (t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w))


def opts(mesh):
    o = unreal.AnimPoseEvaluationOptions()
    o.set_editor_property("optional_skeletal_mesh", mesh)
    o.set_editor_property("should_retarget", False)
    o.set_editor_property("extract_root_motion", False)
    return o


# ---------------------------------------------------------------- fix one clip
def fix_clip(key, clouds, master_mesh, meshes):
    sub_dir, base = key.rsplit("/", 1)
    dst_path = DST_ROOT + sub_dir + "/" + DST_PREFIX + base
    pack, mst = unreal.load_asset(PACK_ROOT + key), unreal.load_asset(dst_path)
    if not (pack and mst):
        return "SKIP %s: pack %s master %s" % (key, bool(pack), bool(mst))
    skel = pack.get_editor_property("skeleton")
    sk = skel.get_name()
    if sk not in meshes:
        meshes[sk] = unreal.load_asset(MESH_BY_SKEL[sk])
    gun_bone = [n for n in (str(x) for x in APE.get_bone_names(APE.get_reference_pose(skel))) if n.endswith("Mesh")][0]
    cloud = clouds[skel.get_path_name()]
    po, mo = opts(meshes[sk]), opts(master_mesh)
    n = unreal.AnimationLibrary.get_num_keys(mst)
    tracks = {b: ([], [], []) for b in ("upperarm_l", "lowerarm_l", "hand_l")}
    changed, before, after, frames = 0, 0.0, 0.0, []
    for f in range(n):
        ps, pt = APE.get_anim_pose_at_frame(pack, f, po), APE.get_anim_pose_at_frame(mst, f, mo)
        hl_s, hr_s = xf(APE.get_bone_pose(ps, "hand_l", W)), xf(APE.get_bone_pose(ps, "hand_r", W))
        m1 = xf(APE.get_bone_pose(ps, "middle_01_l", W))[0]
        g_p, g_r = xf(APE.get_bone_pose(ps, gun_bone, W))
        wd = {b: xf(APE.get_bone_pose(pt, b, W)) for b in ARM}
        loc = {b: APE.get_bone_pose(pt, b, LOC) for b in tracks}
        palm = mul(add(hl_s[0], m1), 0.5)
        d_gun = cloud.dist(qrot(qinv(g_r), sub(palm, g_p)))
        w_old = 1.0 - smoothstep(GRIP_FULL, GRIP_NONE, length(sub(hl_s[0], hr_s[0])))
        w_hold = 1.0 - smoothstep(HOLD_IN, HOLD_OUT, d_gun)
        rot = {b: (loc[b].rotation.x, loc[b].rotation.y, loc[b].rotation.z, loc[b].rotation.w) for b in tracks}
        if w_hold > w_old + 1e-3:
            hr_t = wd["hand_r"]
            rel_rot = qmul(hr_t[1], qinv(hr_s[1]))
            grip = add(hr_t[0], qrot(rel_rot, sub(hl_s[0], hr_s[0])))
            t = (w_hold - w_old) / max(1e-6, 1.0 - w_old)
            goal = lerp(wd["hand_l"][0], grip, t)
            miss0 = length(sub(wd["hand_l"][0], goal))
            if miss0 > 0.01:
                ru, rl, _ = two_bone(wd["upperarm_l"][0], wd["lowerarm_l"][0], wd["hand_l"][0], goal, (0.0, -1.0, -1.0))
                up_w = qmul(ru, wd["upperarm_l"][1])
                lo_w = qmul(rl, qmul(ru, wd["lowerarm_l"][1]))
                rot["upperarm_l"] = qnorm(qmul(qinv(wd["clavicle_l"][1]), up_w))
                rot["lowerarm_l"] = qnorm(qmul(qinv(up_w), lo_w))
                rot["hand_l"] = qnorm(qmul(qinv(lo_w), wd["hand_l"][1]))     # hand keeps its world rotation
                changed += 1
                frames.append(f)
                before = max(before, length(sub(wd["hand_l"][0], grip)) * t)
        for b in tracks:
            tl = loc[b].translation
            sc = loc[b].scale3d
            tracks[b][0].append(unreal.Vector(tl.x, tl.y, tl.z))
            tracks[b][1].append(unreal.Quat(*rot[b]))
            tracks[b][2].append(unreal.Vector(sc.x, sc.y, sc.z))
    if not changed:
        return "UNCHANGED %s" % key
    mst.modify()
    c = mst.controller
    c.open_bracket("AZ master left grip", False)
    try:
        for b, (p_, r_, s_) in tracks.items():
            if not c.set_bone_track_keys(unreal.Name(b), p_, r_, s_, False):
                raise RuntimeError("set_bone_track_keys failed on %s / %s" % (key, b))
    finally:
        c.close_bracket(False)
    # read back: the hand must now sit on the grip target on the changed frames
    for f in frames[:: max(1, len(frames) // 12)]:
        ps, pt = APE.get_anim_pose_at_frame(pack, f, po), APE.get_anim_pose_at_frame(mst, f, mo)
        hl_s, hr_s = xf(APE.get_bone_pose(ps, "hand_l", W)), xf(APE.get_bone_pose(ps, "hand_r", W))
        hr_t, hl_t = xf(APE.get_bone_pose(pt, "hand_r", W)), xf(APE.get_bone_pose(pt, "hand_l", W))
        grip = add(hr_t[0], qrot(qmul(hr_t[1], qinv(hr_s[1])), sub(hl_s[0], hr_s[0])))
        m1 = xf(APE.get_bone_pose(ps, "middle_01_l", W))[0]
        g_p, g_r = xf(APE.get_bone_pose(ps, gun_bone, W))
        w_hold = 1.0 - smoothstep(HOLD_IN, HOLD_OUT, cloud.dist(qrot(qinv(g_r), sub(mul(add(hl_s[0], m1), 0.5), g_p))))
        if w_hold > 0.999:
            after = max(after, length(sub(hl_t[0], grip)))
    saved = EAL.save_asset(dst_path, only_if_is_dirty=False) if SAVE else False
    return "FIXED %s: %d frames (%d..%d), hand was up to %.1f cm off the grip, now %.2f cm; saved %s" % (
        key, changed, frames[0], frames[-1], before, after, saved)


if MODE == "fix":
    names = NAMES
    if names is None:
        with open(AFFECTED, encoding="utf-8") as fh:
            names = [a["name"] for a in json.load(fh)]
    with open(GUN_SCAN, encoding="utf-8") as fh:
        clouds = {k: Cloud(v["pts"]) for k, v in json.load(fh)["pack_guns"].items()}
    master_mesh = unreal.load_asset(MASTER_MESH)
    meshes, lines = {}, []
    for key in names:
        try:
            lines.append(fix_clip(key, clouds, master_mesh, meshes))
        except Exception as e:
            lines.append("FAIL %s: %s" % (key, e))
        with open(REPORT, "a", encoding="utf-8") as fh:
            fh.write(lines[-1] + "\n")
    unreal.log("AZLGRIP done %d clips: %d fixed, %d unchanged, %d failed" % (
        len(lines), sum(l.startswith("FIXED") for l in lines), sum(l.startswith("UNCHANGED") for l in lines),
        sum(l.startswith("FAIL") for l in lines)))
