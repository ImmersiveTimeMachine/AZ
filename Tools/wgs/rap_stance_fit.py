# @Description: Fit RifleAnimsetPro start / stop / turn clips (retargeted to the MetaHuman) to the RifleMega Rifle01 / Cr stance height
"""RifleAnimsetPro (the pistol pack's sibling) was mocapped in a LOW tactical stance: its pelvis sits 7-11 cm lower
than the RifleMega Rifle01 loops the Winchester plays (measured 2026-09-28: start end 73.6 vs walk loop 84.5, stop end
76.6 vs idle 84.2; crouch +6.9 / -9.1). Used as-is, every start / stop would bob the body. The user's rule: take the
original RifleMega clips wherever they exist and only adapt what is missing - starts / stops / turn-starts are missing
in RifleMega, so these borrowed clips are FITTED to the RifleMega stance:

  pelvis height (in the root frame) is offset so the clip's FIRST frame sits at the height of the state it leaves and
  its LAST frame at the height of the state it enters (idle -> loop for starts, loop -> idle for stops, idle -> idle for
  turns), eased in between; the clip's own pelvis motion is kept on top. The ankles stay exactly where they were
  (planted feet stay planted) and the feet keep their world rotation; thigh / calf are re-solved by a twist-free
  two-bone IK (minimal swing + in-plane bend, the same solver as Tools/riflemega_retarget.py).

The upper body is not touched here: in game it is replaced by the Rifle01 / Cr upper body (the upper-body lock) while
these borrowed transitions play.

Globals: MODE 'fit' (write + save) or 'dry' (report only); NAMES (list of clip names without prefix, or None = all
listed in PLAN); REPORT path.
"""
import math
import unreal

MODE = globals().get('MODE', 'dry')
NAMES = globals().get('NAMES', None)
REPORT = globals().get('REPORT', r'C:\UnrealEngine\Games\AZ\Saved\wgs\rap_stance_fit.txt')

RAP = '/Game/AZ/Assets/RifleAnimsetPro/AZ_RTG_MH_'
MST = '/Game/AZ/Assets/Master/RifleMega/'
HERO = '/Game/AZ/Blueprints/Character/AZ_MHC_Hero/Body/SKM_MHC_Hero_BodyMesh'
REFS = {   # reference clips of the RifleMega stance (average pelvis height over the clip)
    'idle': MST + 'Rifle_Styly01_St/Rifle01_IdleSet/AZ_MST_Rifle01_St_Idle00',
    'walk': MST + 'Rifle_Styly01_St/Rifle01_LocomotionSet/AZ_MST_Rifle01_St_Walk_F_IPC',
    'run': MST + 'Rifle_Styly01_St/Rifle01_LocomotionSet/AZ_MST_Rifle01_St_Run_F_IPC',
    'sprint': MST + 'Rifle_Styly01_St/Rifle01_LocomotionSet/AZ_MST_Rifle01_St_Sprint_IPC',
    'cr_idle': MST + 'Rifle_Cr/Rifle_Cr_IdleSet/AZ_MST_Rifle_Cr_Idle00',
    'cr_walk': MST + 'Rifle_Cr/Rifle_Cr_LocomotionSet/AZ_MST_Rifle_Cr_Walk_F_IPC'}
PLAN = {}   # clip -> (height it leaves, height it enters)
for n in ('WalkFwdStart', 'WalkFwdStart90_L', 'WalkFwdStart90_R', 'WalkFwdStart180_L', 'WalkFwdStart180_R',
          'WalkBwdStart', 'StrafeLeftStart', 'StrafeRightStart'):
    PLAN['Rifle_' + n] = ('idle', 'walk')
for n in ('WalkFwdStop_LU', 'WalkFwdStop_RU', 'WalkBwdStop_LU', 'WalkBwdStop_RU', 'StrafeLeftStop_LU',
          'StrafeLeftStop_RU', 'StrafeRightStop_LU', 'StrafeRightStop_RU'):
    PLAN['Rifle_' + n] = ('walk', 'idle')
PLAN['Rifle_SprintStart'] = ('idle', 'sprint')
PLAN['Rifle_SprintStop_LU'] = ('sprint', 'idle')
PLAN['Rifle_SprintStop_RU'] = ('sprint', 'idle')
for d in ('Fwd', 'Bwd', 'Lt', 'Rt'):
    PLAN['Rifle_Crouch_Walk%sStart' % d] = ('cr_idle', 'cr_walk')
    PLAN['Rifle_Crouch_Walk%sStop_LU' % d] = ('cr_walk', 'cr_idle')
    PLAN['Rifle_Crouch_Walk%sStop_RU' % d] = ('cr_walk', 'cr_idle')
for n in ('TurnL_90', 'TurnR_90', 'TurnL_180', 'TurnR_180', 'TurnL_90Loop', 'TurnR_90Loop'):
    PLAN['Rifle_' + n] = ('idle', 'idle')
PLAN['Rifle_Crouch_TurnL90'] = ('cr_idle', 'cr_idle')
PLAN['Rifle_Crouch_TurnR90'] = ('cr_idle', 'cr_idle')

BONES = ['root', 'pelvis', 'thigh_l', 'calf_l', 'foot_l', 'ball_l', 'thigh_r', 'calf_r', 'foot_r', 'ball_r']
WRITTEN = ['pelvis', 'thigh_l', 'calf_l', 'foot_l', 'thigh_r', 'calf_r', 'foot_r']
REACH = 0.998
APE = unreal.AnimPoseExtensions
W, LOC = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL


# ---------------------------------------------------------------- math (UE conventions, same as riflemega_retarget.py)
def v(x): return (x.x, x.y, x.z)
def q(x): return (x.x, x.y, x.z, x.w)
def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def length(a): return math.sqrt(dot(a, a))
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
def smoothstep(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3.0 - 2.0 * x)


def two_bone(a, b, c, target, fallback_pole):
    """Twist-free two-bone IK (minimal swing + in-plane bend) - copy of Tools/riflemega_retarget.py two_bone()."""
    upper, lower = length(sub(b, a)), length(sub(c, b))
    to_t = sub(target, a)
    d = max(abs(upper - lower) + 0.01, min((upper + lower) * REACH, length(to_t)))
    direction = unit(to_t)
    swing = between(unit(sub(c, a)), direction)
    b1 = qrot(swing, sub(b, a))
    perp = sub(b1, mul(direction, dot(b1, direction)))
    fb = sub(fallback_pole, mul(direction, dot(fallback_pole, direction)))
    w = max(0.0, min(1.0, (length(perp) - 0.5) / 2.5))
    w = w * w * (3.0 - 2.0 * w)
    pole = unit(add(mul(unit(perp), w), mul(unit(fb), 1.0 - w))) if length(perp) > 1e-6 else unit(fb)
    cos_a = max(-1.0, min(1.0, (upper * upper + d * d - lower * lower) / (2.0 * upper * d)))
    sin_a = math.sqrt(max(0.0, 1.0 - cos_a * cos_a))
    b_new = mul(add(mul(direction, cos_a), mul(pole, sin_a)), upper)
    r_upper = qmul(between(unit(b1), unit(b_new)), swing)
    c_new = mul(direction, d)
    r_lower = between(unit(qrot(r_upper, sub(c, b))), unit(sub(c_new, b_new)))
    return r_upper, r_lower, length(to_t) - d


def fk(local, parent):
    world = {}
    for b in BONES:
        lp, lr = local[b]
        p = parent.get(b)
        if p in world:
            pp, pr = world[p]
            world[b] = (add(pp, qrot(pr, lp)), qmul(pr, lr))
        else:
            world[b] = (lp, lr)
    return world


def parents_of(mesh):
    # The MetaHuman leg chain (constant); spawning a helper actor to query it fails during PIE and dirties the level.
    return {'root': 'None', 'pelvis': 'root', 'thigh_l': 'pelvis', 'calf_l': 'thigh_l', 'foot_l': 'calf_l',
            'ball_l': 'foot_l', 'thigh_r': 'pelvis', 'calf_r': 'thigh_r', 'foot_r': 'calf_r', 'ball_r': 'foot_r'}


def opts(mesh):
    o = unreal.AnimPoseEvaluationOptions()
    o.set_editor_property('optional_skeletal_mesh', mesh)
    return o


def frames(anim, mesh):
    o = opts(mesh)
    out = []
    for f in range(unreal.AnimationLibrary.get_num_keys(anim)):
        pose = APE.get_anim_pose_at_frame(anim, f, o)
        out.append({b: (v(APE.get_bone_pose(pose, b, LOC).translation), q(APE.get_bone_pose(pose, b, LOC).rotation))
                    for b in BONES})
    return out


def pelvis_height(fr, parent):
    """Pelvis height above the root, in the root's frame (Z)."""
    w = fk(fr, parent)
    return qrot(qinv(w['root'][1]), sub(w['pelvis'][0], w['root'][0]))[2]


def fit(anim, hero, parent, h_from, h_to):
    src = frames(anim, hero)
    n = len(src)
    pose0 = APE.get_anim_pose_at_frame(anim, 0, opts(hero))
    w0 = fk(src[0], parent)
    err = max(length(sub(w0[b][0], v(APE.get_bone_pose(pose0, b, W).translation))) for b in BONES)
    if err > 0.05:
        raise RuntimeError('FK mismatch %.3f cm on %s' % (err, anim.get_name()))
    o0 = h_from - pelvis_height(src[0], parent)
    o1 = h_to - pelvis_height(src[-1], parent)

    # The legs must still reach the planted ankles: the largest raise per frame that keeps both ankles within reach
    # (a wide, low stance cannot be lifted as far as a narrow, upright one). Widened (moving min) and softened so
    # the clamp never pops.
    def max_raise(fr):
        w = fk(fr, parent)
        best = 1e9
        for side in ('l', 'r'):
            hip, knee, ankle = w['thigh_' + side][0], w['calf_' + side][0], w['foot_' + side][0]
            reach = (length(sub(knee, hip)) + length(sub(ankle, knee))) * REACH
            d = sub(ankle, hip)
            h2 = d[0] * d[0] + d[1] * d[1]
            best = min(best, (d[2] + math.sqrt(reach * reach - h2)) if reach * reach > h2 else 0.0)
        return best
    limit = [max_raise(fr) for fr in src]
    limit = [min(limit[max(0, i - 3):i + 4]) for i in range(n)]
    limit = [sum(limit[max(0, i - 3):i + 4]) / len(limit[max(0, i - 3):i + 4]) for i in range(n)]
    tracks = {b: ([], [], []) for b in WRITTEN}
    worst_miss, worst_ankle = 0.0, 0.0
    fit.residual = (0.0, 0.0)
    for f in range(n):
        local = dict(src[f])
        world0 = fk(local, parent)
        want = o0 + (o1 - o0) * smoothstep(f / max(1, n - 1))
        off = min(want, limit[f])
        if f == 0:
            fit.residual = (want - off, fit.residual[1])
        if f == n - 1:
            fit.residual = (fit.residual[0], want - off)
        rp, rr = local['root']
        pp, pr = local['pelvis']
        local['pelvis'] = (add(pp, (0.0, 0.0, off)) if parent['pelvis'] == 'root' else add(pp, qrot(qinv(rr), (0.0, 0.0, off))), pr)
        world = fk(local, parent)
        for side in ('l', 'r'):
            ankle, foot_rot = world0['foot_' + side]
            th, ca, fo = world['thigh_' + side], world['calf_' + side], world['foot_' + side]
            toes = sub(world0['ball_' + side][0], world0['foot_' + side][0])
            ru, rl, miss = two_bone(th[0], ca[0], fo[0], ankle, unit(toes))
            worst_miss = max(worst_miss, miss)
            th_w = qmul(ru, th[1])
            ca_w = qmul(rl, qmul(ru, ca[1]))
            local['thigh_' + side] = (local['thigh_' + side][0], qmul(qinv(world['pelvis'][1]), th_w))
            local['calf_' + side] = (local['calf_' + side][0], qmul(qinv(th_w), ca_w))
            local['foot_' + side] = (local['foot_' + side][0], qmul(qinv(ca_w), foot_rot))
        check = fk(local, parent)
        for side in ('l', 'r'):
            worst_ankle = max(worst_ankle, length(sub(check['foot_' + side][0], world0['foot_' + side][0])))
        for b in WRITTEN:
            p_, r_ = local[b]
            tracks[b][0].append(unreal.Vector(*p_))
            tracks[b][1].append(unreal.Quat(*qnorm(r_)))
            tracks[b][2].append(unreal.Vector(1.0, 1.0, 1.0))
    if MODE == 'fit':
        anim.modify()
        controller = anim.controller
        controller.open_bracket('Winchester stance fit (pelvis height + leg IK)', False)
        try:
            for b in WRITTEN:
                p_, r_, s_ = tracks[b]
                if not controller.set_bone_track_keys(unreal.Name(b), p_, r_, s_, False):
                    raise RuntimeError('set_bone_track_keys failed on %s / %s' % (anim.get_name(), b))
        finally:
            controller.close_bracket(False)
    return o0, o1, worst_miss, worst_ankle


def run():
    hero = unreal.load_asset(HERO)
    parent = parents_of(hero)
    heights = {}
    for key, path in REFS.items():
        fr = frames(unreal.load_asset(path), hero)
        heights[key] = sum(pelvis_height(x, parent) for x in fr) / len(fr)
    lines = ['MODE %s  refs %s' % (MODE, ' '.join('%s %.1f' % kv for kv in heights.items()))]
    for name, (a, b) in sorted(PLAN.items()):
        if NAMES and name not in NAMES:
            continue
        path = RAP + name
        anim = unreal.load_asset(path)
        if not anim:
            lines.append('MISSING ' + path)
            continue
        try:
            o0, o1, miss, ankle = fit(anim, hero, parent, heights[a], heights[b])
            saved = unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False) if MODE == 'fit' else False
            lines.append('%-28s %-7s -> %-7s  wanted %+5.1f .. %+5.1f cm  short by %.1f / %.1f cm  IK miss %.2f  ankle %.2f  saved %s'
                         % (name, a, b, o0, o1, fit.residual[0], fit.residual[1], miss, ankle, saved))
        except Exception as e:
            lines.append('FAIL %s: %s' % (name, e))
    open(REPORT, 'w').write('\n'.join(lines) + '\n')
    print('\n'.join(lines))


run()
