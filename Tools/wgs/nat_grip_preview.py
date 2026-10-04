# @Description: Bake the natural right-hand grasp (re-grip + anatomical fingers) into a preview clip and show it in the level
# Run in the editor (Unreal Python):
#   exec(open(r"C:/UnrealEngine/Games/AZ/Tools/wgs/nat_grip_preview.py").read())
# Input : Saved/wgs/nat_grip.json from the offline solver (scratch prototype, later Tools/wgs/grasp_bake.py):
#           "correction": hand_r re-grip in its own bone space, [tx, ty, tz, qx, qy, qz, qw]  (HandNew = D * HandClip)
#           "locals"    : right-hand finger bone local rotations of the solved grasp {bone: [qx, qy, qz, qw]}
# Output: SRC clip copied to <SRC>_NatGrip (same folder), per frame:
#           * hand_r moved by the correction, right arm two-bone IK to reach it (elbow keeps its side);
#           * az_weapon_r re-expressed under the moved hand so the WEAPON STAYS where the clip has it;
#           * right-hand fingers (and ring/pinky metacarpals) = the solved grasp, weighted by the AZ_Grip_R curve.
#         Two actors in the open level (NOT saved): AZ_GRIPVIEW_BEFORE (the source clip) and AZ_GRIPVIEW_NATURAL,
#         each with the Winchester on RightHandWinchesterSocket, placed in front of the editor camera.
import json
import math

import unreal

SRC = globals().get("SRC", "/Game/AZ/Assets/Master/GripPreview/AZ_MST_Rifle01_St_Idle00_GripPreview")
DATA = globals().get("DATA", "C:/UnrealEngine/Games/AZ/Saved/wgs/nat_grip.json")
DATA_L = globals().get("DATA_L", "C:/UnrealEngine/Games/AZ/Saved/wgs/nat_grip_left.json")   # optional: left hand on its (new) hold
MESH = "/Game/AZ/Assets/Characters/Master/SKM_AZ_Master"
WEAPON = globals().get("WEAPON", "/Game/AZ/Assets/Weapons/Winchester_Rifle/Skeletal/SK_Winchester")
SOCKET = globals().get("SOCKET", "RightHandWinchesterSocket")              # the weapon attach socket on the character
BEFORE_SOCKET = globals().get("BEFORE_SOCKET", SOCKET)                     # the BEFORE actor's attach socket (the old hold)
DST = globals().get("DST", None)                                            # baked clip path (default: <SRC>_NatGrip)
LABEL = globals().get("LABEL", "AZ_GRIPVIEW")
NO_WEAPON_BONE = [globals().get("NO_WEAPON_BONE", False)]
CURVE_R = "AZ_Grip_R"
CURVE_L = "AZ_Grip_L"
REACH = 0.998
APE = unreal.AnimPoseExtensions
EAL = unreal.EditorAssetLibrary
AL = unreal.AnimationLibrary
W, LOC = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL


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
    return _mul(a, 1.0 / n) if n > 1e-12 else (0.0, 0.0, 0.0)
def _qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)
def _qinv(a): return (-a[0], -a[1], -a[2], a[3])
def _qrot(q, v):
    x, y, z, w = q
    t = (2 * (y * v[2] - z * v[1]), 2 * (z * v[0] - x * v[2]), 2 * (x * v[1] - y * v[0]))
    return (v[0] + w * t[0] + (y * t[2] - z * t[1]), v[1] + w * t[1] + (z * t[0] - x * t[2]), v[2] + w * t[2] + (x * t[1] - y * t[0]))
def _qslerp(a, b, t):
    d = sum(x * y for x, y in zip(a, b))
    if d < 0:
        b, d = tuple(-x for x in b), -d
    if d > 0.9995:
        r = tuple(x + (y - x) * t for x, y in zip(a, b))
    else:
        th = math.acos(d)
        s = math.sin(th)
        r = tuple(math.sin((1 - t) * th) / s * x + math.sin(t * th) / s * y for x, y in zip(a, b))
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


def _two_bone(a, b, c, target):
    """Minimal swing + in-plane bend; the elbow keeps its own side (pole = the current elbow)."""
    upper, lower = _len(_sub(b, a)), _len(_sub(c, b))
    to_t = _sub(target, a)
    d = max(abs(upper - lower) + 0.01, min((upper + lower) * REACH, _len(to_t)))
    direction = _unit(to_t)
    swing = _between(_unit(_sub(c, a)), direction)
    b1 = _qrot(swing, _sub(b, a))
    pole = _unit(_sub(b1, _mul(direction, _dot(b1, direction))))
    cos_a = max(-1.0, min(1.0, (upper * upper + d * d - lower * lower) / (2.0 * upper * d)))
    sin_a = math.sqrt(max(0.0, 1.0 - cos_a * cos_a))
    b_new = _mul(_add(_mul(direction, cos_a), _mul(pole, sin_a)), upper)
    r_upper = _qmul(_between(_unit(b1), _unit(b_new)), swing)
    c_new = _mul(direction, d)
    r_lower = _between(_unit(_qrot(r_upper, _sub(c, b))), _unit(_sub(c_new, b_new)))
    return r_upper, r_lower


def _curve_at(clip, name, t):
    if not AL.does_curve_exist(clip, name, unreal.RawCurveTrackTypes.RCT_FLOAT):
        return 1.0
    times, values = AL.get_float_keys(clip, name)
    if not times:
        return 1.0
    for i in range(len(times) - 1):
        if times[i] <= t <= times[i + 1]:
            k = (t - times[i]) / max(1e-6, times[i + 1] - times[i])
            return values[i] + (values[i + 1] - values[i]) * k
    return values[0] if t < times[0] else values[-1]


def _xf(v):
    return unreal.Transform(unreal.Vector(v[0], v[1], v[2]), unreal.Quat(v[3], v[4], v[5], v[6]).rotator(), unreal.Vector(1, 1, 1))


def bake(src_path, data, data_l=None):
    dst = DST or (src_path + "_NatGrip" if not src_path.endswith("_GripPreview") else src_path.replace("_GripPreview", "_NatGrip"))
    if not EAL.does_asset_exist(dst):
        EAL.duplicate_asset(src_path, dst)
    clip = unreal.load_asset(dst)
    src = unreal.load_asset(src_path)                      # poses always from the untouched source (re-runs overwrite)
    mesh = unreal.load_asset(MESH)
    o = unreal.AnimPoseEvaluationOptions()
    o.set_editor_property("optional_skeletal_mesh", mesh)
    o.set_editor_property("should_retarget", False)
    o.set_editor_property("extract_root_motion", False)
    corr = _xf(data["correction"])
    finger_rot = {b: tuple(q) for b, q in data["locals"].items()}
    arm = ("upperarm_r", "lowerarm_r", "hand_r", "az_weapon_r")
    bones = list(arm) + list(finger_rot.keys())
    so = mesh.find_socket(SOCKET)
    attach = unreal.Transform(so.get_editor_property("relative_location"), so.get_editor_property("relative_rotation"), unreal.Vector(1, 1, 1))
    left_in_weapon = _xf(data_l["hand_in_weapon"]) if data_l else None
    finger_rot_l = {b: tuple(q) for b, q in data_l["locals"].items()} if data_l else {}
    if data_l:
        bones += ["upperarm_l", "lowerarm_l", "hand_l"] + list(finger_rot_l.keys())
    n = AL.get_num_keys(clip)
    fps = AL.get_num_frames(clip) / clip.get_play_length() if clip.get_play_length() > 0 else 30.0
    tracks = {b: ([], [], []) for b in bones}
    moved = 0.0
    for f in range(n):
        wr = max(0.0, min(1.0, _curve_at(src, CURVE_R, f / fps)))
        p = APE.get_anim_pose_at_frame(src, f, o)
        loc = {b: APE.get_bone_pose(p, b, LOC) for b in bones}
        rot = {b: _q(loc[b].rotation) for b in bones}
        tra = {b: _v(loc[b].translation) for b in bones}
        cl, up, lo, hd, wp = (APE.get_bone_pose(p, b, W) for b in ("clavicle_r", "upperarm_r", "lowerarm_r", "hand_r", "az_weapon_r"))
        target = unreal.MathLibrary.compose_transforms(corr, hd)       # hand_r moved in its own space
        goal = _add(_v(hd.translation), _mul(_sub(_v(target.translation), _v(hd.translation)), wr))
        hand_q = _qslerp(_q(hd.rotation), _q(target.rotation), wr)
        ru, rl = _two_bone(_v(up.translation), _v(lo.translation), _v(hd.translation), goal)
        up_w = _qmul(ru, _q(up.rotation))
        lo_w = _qmul(rl, _qmul(ru, _q(lo.rotation)))
        rot["upperarm_r"] = _qmul(_qinv(_q(cl.rotation)), up_w)
        rot["lowerarm_r"] = _qmul(_qinv(up_w), lo_w)
        rot["hand_r"] = _qmul(_qinv(lo_w), hand_q)
        # the weapon keeps its world place: its local under the NEW hand
        new_hand = unreal.Transform(unreal.Vector(*goal), unreal.Quat(*hand_q).rotator(), unreal.Vector(1, 1, 1))
        wl = unreal.MathLibrary.make_relative_transform(wp, new_hand)
        rot["az_weapon_r"], tra["az_weapon_r"] = _q(wl.rotation), _v(wl.translation)
        for b, q in finger_rot.items():
            rot[b] = _qslerp(rot[b], q, wr)
        moved = max(moved, _len(_sub(goal, _v(hd.translation))))
        if data_l:
            # left hand: IK onto its hold on the (unchanged) weapon, like the AZ Weapon Grip node does with LeftHandGrip
            wlw = max(0.0, min(1.0, _curve_at(src, CURVE_L, f / fps)))
            weapon_w = unreal.MathLibrary.compose_transforms(attach, wp)
            tgt = unreal.MathLibrary.compose_transforms(left_in_weapon, weapon_w)
            cll, upl, lol, hdl = (APE.get_bone_pose(p, b, W) for b in ("clavicle_l", "upperarm_l", "lowerarm_l", "hand_l"))
            goal_l = _add(_v(hdl.translation), _mul(_sub(_v(tgt.translation), _v(hdl.translation)), wlw))
            hq_l = _qslerp(_q(hdl.rotation), _q(tgt.rotation), wlw)
            rul, rll = _two_bone(_v(upl.translation), _v(lol.translation), _v(hdl.translation), goal_l)
            upw = _qmul(rul, _q(upl.rotation))
            low = _qmul(rll, _qmul(rul, _q(lol.rotation)))
            rot["upperarm_l"] = _qmul(_qinv(_q(cll.rotation)), upw)
            rot["lowerarm_l"] = _qmul(_qinv(upw), low)
            rot["hand_l"] = _qmul(_qinv(low), hq_l)
            for b, q in finger_rot_l.items():
                rot[b] = _qslerp(rot[b], q, wlw)
        for b in bones:
            sc = loc[b].scale3d
            tracks[b][0].append(unreal.Vector(*tra[b]))
            tracks[b][1].append(unreal.Quat(*rot[b]))
            tracks[b][2].append(unreal.Vector(sc.x, sc.y, sc.z))
    c = clip.controller
    c.open_bracket("AZ natural grip preview", False)
    try:
        for b, (p_, r_, s_) in tracks.items():
            if not c.set_bone_track_keys(unreal.Name(b), p_, r_, s_, False):
                if b == "az_weapon_r":           # clip skeleton without the weapon bone (MetaHuman-native sets):
                    NO_WEAPON_BONE[0] = True     # the preview then hangs the weapon on hand_r with the inverse correction
                    continue
                raise RuntimeError("set_bone_track_keys failed on " + b)
    finally:
        c.close_bracket(False)
    ok = EAL.save_asset(dst, only_if_is_dirty=False)
    print("[NatGrip] %s: %d frames, hand moved up to %.1f cm, saved %s" % (dst, n, moved, ok))
    return dst


def _spawn(label, anim_path, location, rotation, socket=None):
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in eas.get_all_level_actors():
        if a.get_actor_label().startswith(label):
            eas.destroy_actor(a)
    body = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, location, rotation)
    body.set_actor_label(label)
    comp = body.skeletal_mesh_component
    comp.set_skinned_asset_and_update(unreal.load_asset(MESH), True)
    comp.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    anim = unreal.load_asset(anim_path)
    comp.set_editor_property("animation_data", unreal.SingleAnimationPlayData(anim_to_play=anim, saved_looping=True, saved_playing=True, saved_position=0.0, saved_play_rate=1.0))
    comp.override_animation_data(anim, True, True, 0.0, 1.0)
    gun = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, location, rotation)
    gun.set_actor_label(label + "_Winchester")
    gun.skeletal_mesh_component.set_skinned_asset_and_update(unreal.load_asset(WEAPON), True)
    gun.attach_to_component(comp, socket or SOCKET, unreal.AttachmentRule.SNAP_TO_TARGET, unreal.AttachmentRule.SNAP_TO_TARGET,
                            unreal.AttachmentRule.KEEP_WORLD, False)
    return body


def show(before_path, after_path):
    cam_loc, cam_rot = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_level_viewport_camera_info()
    fwd = cam_rot.get_forward_vector()
    right = cam_rot.get_right_vector()
    base = cam_loc + fwd * 260.0
    base = unreal.Vector(base.x, base.y, cam_loc.z - 140.0)
    face = unreal.Rotator(roll=0.0, pitch=0.0, yaw=cam_rot.yaw)  # mesh -X (its right side, the gun hand) toward the camera; keywords: the positional order is (roll, pitch, yaw)
    _spawn(LABEL + "_BEFORE", before_path, base - right * 70.0, face, BEFORE_SOCKET)
    nat = _spawn(LABEL + "_NATURAL", after_path, base + right * 70.0, face, SOCKET)
    if NO_WEAPON_BONE[0]:
        # weapon = S * HandClip = (S * D^-1) * HandNew: hang it on hand_r with S * D^-1 so it stays where the clip has it
        eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        gun = [a for a in eas.get_all_level_actors() if a.get_actor_label() == LABEL + "_NATURAL_Winchester"][0]
        so = unreal.load_asset(MESH).find_socket(SOCKET)
        S = unreal.Transform(so.get_editor_property("relative_location"), so.get_editor_property("relative_rotation"), unreal.Vector(1, 1, 1))
        Dc = _xf(json.load(open(DATA))["correction"])
        rel = unreal.MathLibrary.compose_transforms(S, Dc.inverse())
        gun.attach_to_component(nat.skeletal_mesh_component, "hand_r", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                                unreal.AttachmentRule.KEEP_WORLD, False)
        gun.root_component.set_relative_transform(rel, False, False)
        print("[NatGrip] weapon hung on hand_r with the inverse correction (clip has no az_weapon_r)")
    print("[NatGrip] actors at", base, "(level NOT saved)")


if __name__ == "__main__" or True:
    data = json.load(open(DATA))
    import os as _os
    data_l = json.load(open(DATA_L)) if globals().get("LEFT", True) and _os.path.exists(DATA_L) else None
    if globals().get("BAKE", True):
        dst = bake(SRC, data, data_l)
    else:                                                      # actors only (the clip is already baked)
        dst = DST or (SRC.replace("_GripPreview", "_NatGrip") if SRC.endswith("_GripPreview") else SRC + "_NatGrip")
    if globals().get("SHOW", True):
        show(SRC, dst)
    import gc
    gc.collect()
