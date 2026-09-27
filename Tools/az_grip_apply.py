"""Weapon grip pilot - write the solved grip into assets (Unreal Python).

MODE = "assets"  : from Saved/az_grip_solution.json (Tools/az_grip_solve.py):
                   * weapon mesh socket LeftHandGrip = the solved hand_l transform (weapon-model space);
                   * character attach socket (RightHand<Weapon>Socket) = the solved attach (hero body + SKM_AZ_Master +
                     Tools/hero_sockets.json);
                   * grip pose asset GRIP_POSE (1 frame, SK_AZ_Master): the reference pose with the solved finger
                     rotations (only the finger bones are used at runtime).
MODE = "curves"  : per master clip, float curves AZ_Grip_L / AZ_Grip_R = the pack hand is on the pack gun (the
                   pack palm within 10..14 cm of its gun's nearest vertex; geometry from Saved/az_gun_scan.json).
MODE = "preview" : NAMES (master clip paths) -> copies in PREVIEW_DIR with the runtime grip applied offline
                   (fingers <- grip pose by curve weight, left arm two-bone IK onto the weapon's LeftHandGrip).
"""
import json
import math
import os
from collections import defaultdict

import unreal

MODE = globals().get("MODE", "assets")
NAMES = globals().get("NAMES", None)
CHUNK = globals().get("CHUNK", None)
CHUNK_SIZE = globals().get("CHUNK_SIZE", 150)
SAVED = unreal.Paths.project_saved_dir()
SOLUTION = os.path.join(SAVED, globals().get("SOLUTION_NAME", "az_grip_solution.json"))
GRIP_POSE = globals().get("GRIP_POSE", "/Game/AZ/Blueprints/Animation/WeaponGrip/AS_Grip_Winchester")
PREVIEW_DIR = globals().get("PREVIEW_DIR", "/Game/AZ/Assets/Master/GripPreview")
MASTER_SKELETON = "/Game/AZ/Blueprints/Character/Master/SK_AZ_Master"
MASTER_MESH = "/Game/AZ/Assets/Characters/Master/SKM_AZ_Master"
HERO_MESH = "/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh"
SOCKETS_JSON = "C:/UnrealEngine/Games/AZ/Tools/hero_sockets.json"
DST_ROOT, DST_PREFIX = "/Game/AZ/Assets/Master/RifleMega/", "AZ_MST_"
PACK_ROOT = "/Game/RifleMega_MocapAnimPack/AnimationsFBX/"
_M = "/Game/RifleMega_MocapAnimPack/Demo/Models/"
MESH_BY_SKEL = {
    "Rifle_Mannequin_A_Skeleton": _M + "Character/Mesh/Rifle_Mannequin_A",
    "Rifle_Auto_Mannequin_A_Skeleton": _M + "forShootingReloading/Character_Automatic/Mesh/Rifle_Auto_Mannequin_A",
    "Rifle_DB_Mannequin_A_Skeleton": _M + "forShootingReloading/Character_DoubleBarrel/Mesh/Rifle_DB_Mannequin_A",
    "Rifle_SG_Mannequin_A_Skeleton": _M + "forShootingReloading/Character_ShotGun/Mesh/Rifle_SG_Mannequin_A",
    "Rifle_Winch_Mannequin_A_Skeleton": _M + "forShootingReloading/Character_Winchester/Mesh/Rifle_Winch_Mannequin_A"}
CURVE_L, CURVE_R = "AZ_Grip_L", "AZ_Grip_R"
HOLD_IN, HOLD_OUT = 10.0, 14.0
APE = unreal.AnimPoseExtensions
EAL = unreal.EditorAssetLibrary
AL = unreal.AnimationLibrary
W, LOC = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL
FINGER_BONES = ["%s_0%d_%s" % (f, k, s) for s in ("l", "r") for f in ("thumb", "index", "middle", "ring", "pinky") for k in (1, 2, 3)]


def log(msg):
    unreal.log("AZGRIPAPPLY " + msg)


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3.0 - 2.0 * t)


def opts(mesh):
    o = unreal.AnimPoseEvaluationOptions()
    o.set_editor_property("optional_skeletal_mesh", mesh)
    o.set_editor_property("should_retarget", False)
    o.set_editor_property("extract_root_motion", False)
    return o


def xform(v):
    return unreal.Transform(unreal.Vector(v[0], v[1], v[2]), unreal.Quat(v[3], v[4], v[5], v[6]).rotator(), unreal.Vector(1, 1, 1))


# ---------------------------------------------------------------- assets
def set_char_socket(mesh_path, name, xf):
    m = unreal.load_asset(mesh_path)
    s = m.find_socket(name)
    m.modify()
    s.modify()
    s.set_editor_property("relative_location", xf.translation)
    s.set_editor_property("relative_rotation", xf.rotation.rotator())
    ok = EAL.save_asset(mesh_path, only_if_is_dirty=False)
    l, r = s.get_editor_property("relative_location"), s.get_editor_property("relative_rotation")
    return ok, {"bone": str(s.get_editor_property("bone_name")), "loc": [l.x, l.y, l.z], "rot_pitch_yaw_roll": [r.pitch, r.yaw, r.roll],
                "scale": [1.0, 1.0, 1.0]}


def make_assets(sol):
    ref = json.load(open(os.path.join(SAVED, "az_grip_reference.json")))
    # 1. weapon mesh socket LeftHandGrip
    sm = unreal.load_asset(sol["weapon_mesh"])
    g = xform(sol["left_hand_grip"])
    sock = sm.find_socket("LeftHandGrip")
    old = None
    if sock is None:
        sock = unreal.StaticMeshSocket(sm)
        sock.set_editor_property("socket_name", "LeftHandGrip")
        sm.add_socket(sock)
    else:
        old = (sock.get_editor_property("relative_location"), sock.get_editor_property("relative_rotation"))
    sm.modify()
    sock.set_editor_property("relative_location", g.translation)
    sock.set_editor_property("relative_rotation", g.rotation.rotator())
    ok_sm = EAL.save_asset(sol["weapon_mesh"].split(".")[0], only_if_is_dirty=False)
    log("weapon socket LeftHandGrip %s -> loc %s rot %s (was %s) saved %s" % (sm.get_name(), g.translation, g.rotation.rotator(), old, ok_sm))
    # 2. attach socket on both character meshes (+ json)
    a = xform(sol["socket_new"])
    rec = None
    for mp in (HERO_MESH, MASTER_MESH):
        ok, rec = set_char_socket(mp, sol["socket"], a)
        log("attach socket %s on %s saved %s: %s" % (sol["socket"], mp.rsplit("/", 1)[-1], ok, rec))
    data = json.load(open(SOCKETS_JSON, encoding="utf-8"))
    data[sol["socket"]] = rec
    with open(SOCKETS_JSON, "w", encoding="utf-8") as fh:
        json.dump(data, fh, indent=1, sort_keys=True)
    # 3. grip pose asset: reference pose, finger rotations replaced
    folder, name = GRIP_POSE.rsplit("/", 1)
    seq = unreal.load_asset(GRIP_POSE) if EAL.does_asset_exist(GRIP_POSE) else None
    if seq is None:
        f = unreal.AnimSequenceFactory()
        f.set_editor_property("target_skeleton", unreal.load_asset(MASTER_SKELETON))
        seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.AnimSequence, f)
    c = seq.controller
    c.open_bracket("AZ weapon grip pose", False)
    try:
        c.remove_all_bone_tracks(False)
        c.set_frame_rate(unreal.FrameRate(30, 1), False)
        c.set_number_of_frames(unreal.FrameNumber(1), False)
        for b in ref["bones"]:
            v = list(ref["local"][b])
            if b in sol["pose_local"]:
                v[3:7] = sol["pose_local"][b]
            p, q = unreal.Vector(v[0], v[1], v[2]), unreal.Quat(v[3], v[4], v[5], v[6])
            c.add_bone_curve(unreal.Name(b), False)
            c.set_bone_track_keys(unreal.Name(b), [p, p], [q, q], [unreal.Vector(1, 1, 1)] * 2, False)
    finally:
        c.close_bracket(False)
    seq.set_editor_property("retarget_source_asset", unreal.load_asset(MASTER_MESH))
    ok = EAL.save_asset(GRIP_POSE, only_if_is_dirty=False)
    log("grip pose %s: %d bones (%d finger bones solved) saved %s" % (GRIP_POSE, len(ref["bones"]), len(sol["pose_local"]), ok))


# ---------------------------------------------------------------- curves
class Cloud(object):
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


def master_clips():
    return sorted(p.split(".")[0] for p in EAL.list_assets(DST_ROOT, recursive=True, include_folder=False)
                  if p.rsplit("/", 1)[-1].startswith(DST_PREFIX))


def write_curve(clip, name, times, values):
    if AL.does_curve_exist(clip, name, unreal.RawCurveTrackTypes.RCT_FLOAT):
        AL.remove_curve(clip, name, False)
    AL.add_curve(clip, name, unreal.RawCurveTrackTypes.RCT_FLOAT, False)
    AL.add_float_curve_keys(clip, name, times, values)


def curves_for(path, clouds, meshes):
    sub_dir, name = path[len(DST_ROOT):].rsplit("/", 1)
    pack = unreal.load_asset(PACK_ROOT + sub_dir + "/" + name[len(DST_PREFIX):])
    mst = unreal.load_asset(path)
    skel = pack.get_editor_property("skeleton")
    sk = skel.get_name()
    if sk not in meshes:
        meshes[sk] = (unreal.load_asset(MESH_BY_SKEL[sk]),
                      [n for n in (str(x) for x in APE.get_bone_names(APE.get_reference_pose(skel))) if n.endswith("Mesh")][0])
    pm, gun = meshes[sk]
    cloud = clouds[skel.get_path_name()]
    o = opts(pm)
    n = AL.get_num_keys(mst)
    fps = AL.get_num_frames(mst) / mst.get_play_length() if mst.get_play_length() > 0 else 30.0
    times, wl, wr = [], [], []
    mo = opts(unreal.load_asset(MASTER_MESH))
    for f in range(n):
        # LEFT: the pack's left palm is on the pack gun (it holds the handguard; reloads keep it there)
        p = APE.get_anim_pose_at_frame(pack, min(f, AL.get_num_keys(pack) - 1), o)
        g = APE.get_bone_pose(p, gun, W)
        palm = (APE.get_bone_pose(p, "hand_l", W).translation + APE.get_bone_pose(p, "middle_01_l", W).translation) * 0.5
        lp = g.inverse_transform_location(palm)
        wl.append(1.0 - smoothstep(HOLD_IN, HOLD_OUT, cloud.dist((lp.x, lp.y, lp.z))))
        # RIGHT: the gun still sits in the right hand as in the hold (az_weapon_r local ~ identity). Proximity is
        # wrong here: loading shells keeps the right hand AT the gun without holding the grip. Tolerance covers
        # the lever rock of the Winchester shots (2.9 cm / 23.5 deg).
        a = APE.get_bone_pose(APE.get_anim_pose_at_frame(mst, f, mo), "az_weapon_r", LOC)
        ang = math.degrees(2.0 * math.acos(min(1.0, abs(a.rotation.w))))
        wr.append((1.0 - smoothstep(3.5, 7.0, a.translation.length())) * (1.0 - smoothstep(26.0, 40.0, ang)))
        times.append(f / fps)
    write_curve(mst, CURVE_L, times, wl)
    write_curve(mst, CURVE_R, times, wr)
    ok = EAL.save_asset(path, only_if_is_dirty=False)
    return "%s: L on %d/%d, R on %d/%d frames; saved %s" % (name, sum(w > 0.99 for w in wl), n, sum(w > 0.99 for w in wr), n, ok)


# ---------------------------------------------------------------- preview (the runtime grip, applied offline)
REACH = 0.998


def _v(x): return (x.x, x.y, x.z)
def _q(x): return (x.x, x.y, x.z, x.w)
def _add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def _sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def _mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def _dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def _cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def _len(a): return math.sqrt(_dot(a, a))
def _unit(a):
    n = _len(a)
    return _mul(a, 1.0 / n) if n > 1e-9 else (0.0, 0.0, 0.0)
def _qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)
def _qinv(a): return (-a[0], -a[1], -a[2], a[3])
def _qrot(a, p):
    r = _qmul(_qmul(a, (p[0], p[1], p[2], 0.0)), _qinv(a))
    return (r[0], r[1], r[2])
def _qslerp(a, b, t):
    d = sum(x * y for x, y in zip(a, b))
    if d < 0:
        b, d = tuple(-x for x in b), -d
    if d > 0.9995:
        r = tuple(x + (y - x) * t for x, y in zip(a, b))
    else:
        th = math.acos(d)
        s = math.sin(th)
        wa, wb = math.sin((1 - t) * th) / s, math.sin(t * th) / s
        r = tuple(wa * x + wb * y for x, y in zip(a, b))
    n = math.sqrt(sum(x * x for x in r))
    return tuple(x / n for x in r)
def _between(a, b):
    d = max(-1.0, min(1.0, _dot(a, b)))
    axis = _cross(a, b)
    n = _len(axis)
    if n < 1e-9:
        return (0.0, 0.0, 0.0, 1.0)
    h = math.acos(d) * 0.5
    s = math.sin(h) / n
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(h))


def _two_bone(a, b, c, target, fallback_pole):
    """Same solver as riflemega_retarget.py / az_master_left_grip.py (minimal swing + in-plane bend)."""
    upper, lower = _len(_sub(b, a)), _len(_sub(c, b))
    to_t = _sub(target, a)
    d = max(abs(upper - lower) + 0.01, min((upper + lower) * REACH, _len(to_t)))
    direction = _unit(to_t)
    swing = _between(_unit(_sub(c, a)), direction)
    b1 = _qrot(swing, _sub(b, a))
    perp = _sub(b1, _mul(direction, _dot(b1, direction)))
    fb = _sub(fallback_pole, _mul(direction, _dot(fallback_pole, direction)))
    w = smoothstep(0.5, 3.0, _len(perp))
    pole = _unit(_add(_mul(_unit(perp), w), _mul(_unit(fb), 1.0 - w))) if _len(perp) > 1e-6 else _unit(fb)
    cos_a = max(-1.0, min(1.0, (upper * upper + d * d - lower * lower) / (2.0 * upper * d)))
    sin_a = math.sqrt(max(0.0, 1.0 - cos_a * cos_a))
    b_new = _mul(_add(_mul(direction, cos_a), _mul(pole, sin_a)), upper)
    r_upper = _qmul(_between(_unit(b1), _unit(b_new)), swing)
    c_new = _mul(direction, d)
    r_lower = _between(_unit(_qrot(r_upper, _sub(c, b))), _unit(_sub(c_new, b_new)))
    return r_upper, r_lower


def curve_at(clip, name, t):
    if not AL.does_curve_exist(clip, name, unreal.RawCurveTrackTypes.RCT_FLOAT):
        return 0.0
    times, values = AL.get_float_keys(clip, name)
    if not times:
        return 0.0
    for i in range(len(times) - 1):
        if times[i] <= t <= times[i + 1]:
            k = (t - times[i]) / max(1e-6, times[i + 1] - times[i])
            return values[i] + (values[i + 1] - values[i]) * k
    return values[0] if t < times[0] else values[-1]


def preview(path, sol):
    name = path.rsplit("/", 1)[-1]
    dst = PREVIEW_DIR + "/" + name + "_GripPreview"
    if not EAL.does_asset_exist(dst):
        EAL.duplicate_asset(path, dst)
    clip = unreal.load_asset(dst)
    src = unreal.load_asset(path)          # poses/curves always from the untouched master clip (re-runs overwrite in place)
    mesh = unreal.load_asset(MASTER_MESH)
    o = opts(mesh)
    grip = unreal.load_asset(GRIP_POSE)
    gpose = APE.get_anim_pose_at_frame(grip, 0, o)
    grip_local = {b: _q(APE.get_bone_pose(gpose, b, LOC).rotation) for b in FINGER_BONES}
    so = mesh.find_socket(sol["socket"])
    attach = unreal.Transform(so.relative_location, so.relative_rotation, so.relative_scale)
    lg = xform(sol["left_hand_grip"])
    n = AL.get_num_keys(clip)
    fps = AL.get_num_frames(clip) / clip.get_play_length() if clip.get_play_length() > 0 else 30.0
    arm = ("upperarm_l", "lowerarm_l", "hand_l")
    tracks = {b: ([], [], []) for b in FINGER_BONES + list(arm)}
    worst = 0.0
    for f in range(n):
        t = f / fps
        wl, wr = curve_at(src, CURVE_L, t), curve_at(src, CURVE_R, t)
        p = APE.get_anim_pose_at_frame(src, f, o)
        loc = {b: APE.get_bone_pose(p, b, LOC) for b in tracks}
        rot = {b: _q(loc[b].rotation) for b in tracks}
        for b in FINGER_BONES:
            w = wl if b.endswith("_l") else wr
            if w > 1e-3:
                rot[b] = _qslerp(rot[b], grip_local[b], w)
        if wl > 1e-3:
            wcs = unreal.MathLibrary.compose_transforms(attach, APE.get_bone_pose(p, "az_weapon_r", W))
            target = unreal.MathLibrary.compose_transforms(lg, wcs)
            cl, up, lo, hd = (APE.get_bone_pose(p, b, W) for b in ("clavicle_l", "upperarm_l", "lowerarm_l", "hand_l"))
            goal = _add(_v(hd.translation), _mul(_sub(_v(target.translation), _v(hd.translation)), wl))
            ru, rl = _two_bone(_v(up.translation), _v(lo.translation), _v(hd.translation), goal, (0.0, -1.0, -1.0))
            up_w = _qmul(ru, _q(up.rotation))
            lo_w = _qmul(rl, _qmul(ru, _q(lo.rotation)))
            hand_w = _qslerp(_q(hd.rotation), _q(target.rotation), wl)
            rot["upperarm_l"] = _qmul(_qinv(_q(cl.rotation)), up_w)
            rot["lowerarm_l"] = _qmul(_qinv(up_w), lo_w)
            rot["hand_l"] = _qmul(_qinv(lo_w), hand_w)
            worst = max(worst, _len(_sub(_v(target.translation), _v(hd.translation))) * wl)
        for b in tracks:
            tl, sc = loc[b].translation, loc[b].scale3d
            tracks[b][0].append(unreal.Vector(tl.x, tl.y, tl.z))
            tracks[b][1].append(unreal.Quat(*rot[b]))
            tracks[b][2].append(unreal.Vector(sc.x, sc.y, sc.z))
    c = clip.controller
    c.open_bracket("AZ weapon grip preview", False)
    try:
        for b, (p_, r_, s_) in tracks.items():
            if not c.set_bone_track_keys(unreal.Name(b), p_, r_, s_, False):
                raise RuntimeError("set_bone_track_keys failed on " + b)
    finally:
        c.close_bracket(False)
    ok = EAL.save_asset(dst, only_if_is_dirty=False)
    return "%s: %d frames, left hand moved up to %.1f cm onto the grip; saved %s" % (dst, n, worst, ok)


# ---------------------------------------------------------------- main
if MODE == "preview":
    sol = json.load(open(SOLUTION))
    for pth in NAMES:
        try:
            log("PREVIEW " + preview(pth, sol))
        except Exception as e:
            log("PREVIEW FAIL %s: %s" % (pth, e))
elif MODE == "assets":
    make_assets(json.load(open(SOLUTION)))
elif MODE == "curves":
    with open(os.path.join(SAVED, "az_gun_scan.json"), encoding="utf-8") as fh:
        clouds = {k: Cloud(v["pts"]) for k, v in json.load(fh)["pack_guns"].items()}
    paths = NAMES or master_clips()
    if CHUNK is not None:
        paths = paths[CHUNK * CHUNK_SIZE:(CHUNK + 1) * CHUNK_SIZE]
    meshes, lines = {}, []
    for pth in paths:
        try:
            lines.append(curves_for(pth, clouds, meshes))
        except Exception as e:
            lines.append("FAIL %s: %s" % (pth, e))
    with open(os.path.join(SAVED, "az_grip_curves_report.txt"), "a", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")
    log("curves chunk %s: %d clips, %d failed" % (CHUNK, len(lines), sum(l.startswith("FAIL") for l in lines)))
